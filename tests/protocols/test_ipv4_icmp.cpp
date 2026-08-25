#include "core/protocols/Icmp.h"
#include "core/protocols/Ipv4.h"

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
} // namespace

TEST_CASE("IPv4 packets encode with a correct checksum", "[protocols][ipv4]") {
    Ipv4Header header;
    header.source = ipv4("192.168.1.10");
    header.destination = ipv4("10.0.0.5");
    header.ttl = 64;
    header.protocol = static_cast<u8>(IpProtocol::Icmp);
    header.identification = 0x1234;

    const ByteBuffer payload(40, 0xCD);
    const ByteBuffer packet = encodeIpv4(header, payload);

    CHECK(packet.size() == kIpv4MinimumHeaderSize + payload.size());

    const auto decoded = decodeIpv4(packet);
    REQUIRE(decoded.has_value());
    CHECK(decoded->header.version == 4);
    CHECK(decoded->header.headerLengthBytes() == 20);
    CHECK(decoded->header.totalLength == packet.size());
    CHECK(decoded->header.ttl == 64);
    CHECK(decoded->header.source == header.source);
    CHECK(decoded->header.destination == header.destination);
    CHECK(decoded->header.identification == 0x1234);
    CHECK(decoded->payload.size() == payload.size());
    CHECK(ipv4HeaderChecksumValid(decoded->headerBytes));
}

TEST_CASE("A corrupted IPv4 header fails its checksum", "[protocols][ipv4]") {
    Ipv4Header header;
    header.source = ipv4("192.168.1.10");
    header.destination = ipv4("10.0.0.5");

    ByteBuffer packet = encodeIpv4(header, ByteBuffer(20, 0x00));
    REQUIRE(ipv4HeaderChecksumValid(std::span<const u8>{packet}.first(20)));

    packet[12] ^= 0x01; // flip a bit in the source address
    CHECK_FALSE(ipv4HeaderChecksumValid(std::span<const u8>{packet}.first(20)));
}

TEST_CASE("Decoding trusts the length field, not the buffer size", "[protocols][ipv4]") {
    Ipv4Header header;
    header.source = ipv4("10.0.0.1");
    header.destination = ipv4("10.0.0.2");

    ByteBuffer packet = encodeIpv4(header, ByteBuffer(8, 0xEE));
    const std::size_t realSize = packet.size();

    // Simulate Ethernet padding: extra bytes after the packet that are not payload.
    packet.resize(realSize + 20, 0x00);

    const auto decoded = decodeIpv4(packet);
    REQUIRE(decoded.has_value());
    CHECK(decoded->payload.size() == 8);
    CHECK(decoded->datagram.size() == realSize);
}

TEST_CASE("Rewriting the TTL repairs the checksum", "[protocols][ipv4]") {
    Ipv4Header header;
    header.source = ipv4("10.0.0.1");
    header.destination = ipv4("10.0.0.2");
    header.ttl = 64;

    ByteBuffer packet = encodeIpv4(header, ByteBuffer(16, 0x00));
    REQUIRE(setIpv4Ttl(packet, 63));

    const auto decoded = decodeIpv4(packet);
    REQUIRE(decoded.has_value());
    CHECK(decoded->header.ttl == 63);
    CHECK(ipv4HeaderChecksumValid(decoded->headerBytes));
}

TEST_CASE("Malformed IPv4 packets are rejected", "[protocols][ipv4]") {
    CHECK_FALSE(decodeIpv4(ByteBuffer{}).has_value());
    CHECK_FALSE(decodeIpv4(ByteBuffer(10, 0x45)).has_value());

    ByteBuffer wrongVersion(20, 0x00);
    wrongVersion[0] = 0x65; // version 6, IHL 5
    CHECK_FALSE(decodeIpv4(wrongVersion).has_value());

    ByteBuffer shortHeader(20, 0x00);
    shortHeader[0] = 0x43; // IHL 3, below the legal minimum
    CHECK_FALSE(decodeIpv4(shortHeader).has_value());
}

TEST_CASE("ICMP echo messages round-trip", "[protocols][icmp]") {
    const ByteBuffer payload(32, 0x61);
    const ByteBuffer message = encodeIcmpEcho(IcmpType::EchoRequest, 0x1234, 7, payload);

    CHECK(message.size() == kIcmpHeaderSize + payload.size());
    CHECK(icmpChecksumValid(message));

    const auto decoded = decodeIcmp(message);
    REQUIRE(decoded.has_value());
    CHECK(decoded->type == static_cast<u8>(IcmpType::EchoRequest));
    CHECK(decoded->identifier == 0x1234);
    CHECK(decoded->sequence == 7);
    CHECK(decoded->payload.size() == 32);
    CHECK(decoded->isEcho());
    CHECK_FALSE(decoded->isError());
}

TEST_CASE("ICMP errors quote the packet that caused them", "[protocols][icmp]") {
    Ipv4Header header;
    header.source = ipv4("192.168.1.10");
    header.destination = ipv4("10.0.0.5");
    header.protocol = static_cast<u8>(IpProtocol::Icmp);

    const ByteBuffer original = encodeIpv4(header, encodeIcmpEcho(IcmpType::EchoRequest, 1, 1,
                                                                  ByteBuffer(56, 0x00)));

    const ByteBuffer error = encodeIcmpError(IcmpType::TimeExceeded,
                                             static_cast<u8>(IcmpTimeExceededCode::TtlExpiredInTransit),
                                             original);

    CHECK(icmpChecksumValid(error));

    const auto decoded = decodeIcmp(error);
    REQUIRE(decoded.has_value());
    CHECK(decoded->isError());
    CHECK(decoded->type == static_cast<u8>(IcmpType::TimeExceeded));

    // RFC 792: the IP header plus the first eight payload bytes.
    CHECK(decoded->payload.size() == kIpv4MinimumHeaderSize + kIcmpQuotedPayloadBytes);

    // The quote is truncated by design, so it decodes as a header rather than a
    // whole packet - and that is what lets a sender match the error to the
    // request that produced it.
    CHECK_FALSE(decodeIpv4(decoded->payload).has_value());
    const auto quoted = decodeIpv4Header(decoded->payload);
    REQUIRE(quoted.has_value());
    CHECK(quoted->header.source == header.source);
    CHECK(quoted->header.destination == header.destination);
}

TEST_CASE("A corrupted ICMP message fails its checksum", "[protocols][icmp]") {
    ByteBuffer message = encodeIcmpEcho(IcmpType::EchoRequest, 1, 1, ByteBuffer(32, 0x61));
    REQUIRE(icmpChecksumValid(message));

    message.back() ^= 0xFF;
    CHECK_FALSE(icmpChecksumValid(message));
}

TEST_CASE("ICMP names describe types and error codes", "[protocols][icmp]") {
    CHECK(icmpTypeName(0) == "Echo Reply");
    CHECK(icmpTypeName(8) == "Echo Request");
    CHECK(icmpCodeName(3, 1) == "Host unreachable");
    CHECK(icmpCodeName(11, 0) == "TTL expired in transit");
    CHECK(icmpCodeName(8, 0).empty()); // an echo request has no meaningful code
}
