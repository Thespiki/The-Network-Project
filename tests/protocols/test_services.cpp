#include "core/protocols/Dhcp.h"
#include "core/protocols/Dns.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::core::proto;

namespace {
Ipv4Address ipv4(const char* text) {
    const auto value = Ipv4Address::parse(text);
    REQUIRE(value.has_value());
    return *value;
}
MacAddress mac(const char* text) {
    const auto value = MacAddress::parse(text);
    REQUIRE(value.has_value());
    return *value;
}
} // namespace

TEST_CASE("DHCP messages round-trip with their options", "[protocols][dhcp]") {
    DhcpMessage offer;
    offer.operation = 2;
    offer.transactionId = 0xABCD1234;
    offer.clientMac = mac("11:22:33:44:55:66");
    offer.messageType = DhcpMessageType::Offer;
    offer.yourAddress = ipv4("192.168.1.100");
    offer.serverAddress = ipv4("192.168.1.1");
    offer.serverIdentifier = ipv4("192.168.1.1");
    offer.subnetMask = ipv4("255.255.255.0");
    offer.router = ipv4("192.168.1.1");
    offer.domainNameServer = ipv4("192.168.1.53");
    offer.leaseTimeSeconds = 86400;

    const ByteBuffer encoded = encodeDhcp(offer);
    CHECK(encoded.size() > kDhcpFixedSize);

    const auto decoded = decodeDhcp(encoded);
    REQUIRE(decoded.has_value());
    CHECK(decoded->operation == 2);
    CHECK(decoded->transactionId == 0xABCD1234);
    CHECK(decoded->clientMac == offer.clientMac);
    CHECK(decoded->messageType == DhcpMessageType::Offer);
    CHECK(decoded->yourAddress == offer.yourAddress);
    CHECK(decoded->subnetMask == offer.subnetMask);
    CHECK(decoded->router == offer.router);
    CHECK(decoded->domainNameServer == offer.domainNameServer);
    CHECK(decoded->serverIdentifier == offer.serverIdentifier);
    CHECK(decoded->leaseTimeSeconds == 86400);
}

TEST_CASE("A DHCP discover carries no address", "[protocols][dhcp]") {
    DhcpMessage discover;
    discover.operation = 1;
    discover.transactionId = 1;
    discover.clientMac = mac("AA:BB:CC:DD:EE:FF");
    discover.messageType = DhcpMessageType::Discover;

    const auto decoded = decodeDhcp(encodeDhcp(discover));
    REQUIRE(decoded.has_value());
    CHECK(decoded->messageType == DhcpMessageType::Discover);
    CHECK(decoded->yourAddress.isUnspecified());
    CHECK_FALSE(decoded->subnetMask.has_value());
    CHECK(decoded->broadcastFlag);
}

TEST_CASE("DHCP decoding rejects malformed messages", "[protocols][dhcp]") {
    CHECK_FALSE(decodeDhcp(ByteBuffer{}).has_value());
    CHECK_FALSE(decodeDhcp(ByteBuffer(100, 0x00)).has_value());

    DhcpMessage message;
    message.messageType = DhcpMessageType::Discover;
    ByteBuffer encoded = encodeDhcp(message);

    SECTION("a wrong magic cookie is not a DHCP message") {
        encoded[kDhcpFixedSize] = 0x00;
        CHECK_FALSE(decodeDhcp(encoded).has_value());
    }
    SECTION("the message type option is mandatory") {
        // Truncate to just past the cookie so no options remain.
        encoded.resize(kDhcpFixedSize + 4);
        CHECK_FALSE(decodeDhcp(encoded).has_value());
    }
}

TEST_CASE("DNS names encode as length-prefixed labels", "[protocols][dns]") {
    const ByteBuffer encoded = encodeDnsName("server.local");

    REQUIRE(encoded.size() == 1 + 6 + 1 + 5 + 1);
    CHECK(encoded[0] == 6);
    CHECK(encoded[7] == 5);
    CHECK(encoded.back() == 0);
}

TEST_CASE("DNS queries and answers round-trip", "[protocols][dns]") {
    DnsMessage query;
    query.transactionId = 0x4242;
    query.questions.push_back(DnsQuestion{"server.local", static_cast<u16>(DnsRecordType::A), 1});

    const auto decodedQuery = decodeDns(encodeDns(query));
    REQUIRE(decodedQuery.has_value());
    CHECK_FALSE(decodedQuery->isResponse);
    REQUIRE(decodedQuery->questions.size() == 1);
    CHECK(decodedQuery->questions.front().name == "server.local");

    DnsMessage response = query;
    response.isResponse = true;
    response.authoritative = true;
    DnsAnswer answer;
    answer.name = "server.local";
    answer.address = ipv4("172.16.0.20");
    response.answers.push_back(answer);

    const auto decodedResponse = decodeDns(encodeDns(response));
    REQUIRE(decodedResponse.has_value());
    CHECK(decodedResponse->isResponse);
    CHECK(decodedResponse->authoritative);
    CHECK(decodedResponse->responseCode == DnsResponseCode::NoError);
    REQUIRE(decodedResponse->answers.size() == 1);
    CHECK(decodedResponse->answers.front().address == ipv4("172.16.0.20"));
}

TEST_CASE("A DNS name error carries the code but no answer", "[protocols][dns]") {
    DnsMessage response;
    response.transactionId = 9;
    response.isResponse = true;
    response.responseCode = DnsResponseCode::NameError;
    response.questions.push_back(DnsQuestion{"missing.local", 1, 1});

    const auto decoded = decodeDns(encodeDns(response));
    REQUIRE(decoded.has_value());
    CHECK(decoded->responseCode == DnsResponseCode::NameError);
    CHECK(decoded->answers.empty());
    CHECK(dnsResponseCodeName(decoded->responseCode) == "NXDOMAIN");
}

TEST_CASE("DNS compression pointers are rejected rather than followed", "[protocols][dns]") {
    // A pointer would need whole-message context to resolve and can be made to
    // loop; the decoder refuses it instead.
    ByteBuffer message(kDnsHeaderSize, 0x00);
    message[5] = 1;    // one question
    message.push_back(0xC0);
    message.push_back(0x0C);
    message.push_back(0x00);
    message.push_back(0x01);
    message.push_back(0x00);
    message.push_back(0x01);

    CHECK_FALSE(decodeDns(message).has_value());
}
