#include "core/protocols/Arp.h"
#include "core/protocols/Ethernet.h"
#include "utilities/ByteStream.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::core::proto;

namespace {
MacAddress mac(const char* text) {
    const auto value = MacAddress::parse(text);
    REQUIRE(value.has_value());
    return *value;
}
Ipv4Address ipv4(const char* text) {
    const auto value = Ipv4Address::parse(text);
    REQUIRE(value.has_value());
    return *value;
}
} // namespace

TEST_CASE("Ethernet frames encode and decode", "[protocols][ethernet]") {
    EthernetHeader header;
    header.destination = mac("AA:BB:CC:DD:EE:FF");
    header.source = mac("11:22:33:44:55:66");
    header.etherType = static_cast<u16>(EtherType::Ipv4);

    const ByteBuffer payload(100, 0xAB);
    const ByteBuffer frame = encodeEthernet(header, payload);

    CHECK(frame.size() == 14 + payload.size());

    const auto decoded = decodeEthernet(frame);
    REQUIRE(decoded.has_value());
    CHECK(decoded->header.destination == header.destination);
    CHECK(decoded->header.source == header.source);
    CHECK(decoded->header.etherType == header.etherType);
    CHECK_FALSE(decoded->header.vlanTag.has_value());
    CHECK(decoded->payload.size() == payload.size());
}

TEST_CASE("Short frames are padded to the Ethernet minimum", "[protocols][ethernet]") {
    EthernetHeader header;
    header.destination = MacAddress::broadcast();
    header.source = mac("11:22:33:44:55:66");
    header.etherType = static_cast<u16>(EtherType::Arp);

    // A 28-byte ARP payload plus a 14-byte header is 42 bytes, below the 60-byte
    // minimum a real NIC puts on the wire.
    const ByteBuffer frame = encodeEthernet(header, ByteBuffer(28, 0x00));
    CHECK(frame.size() == kMinimumFrameSize);

    const ByteBuffer unpadded = encodeEthernet(header, ByteBuffer(28, 0x00), false);
    CHECK(unpadded.size() == 42);
}

TEST_CASE("802.1Q tags survive a round trip", "[protocols][ethernet][vlan]") {
    EthernetHeader header;
    header.destination = mac("AA:BB:CC:DD:EE:FF");
    header.source = mac("11:22:33:44:55:66");
    header.etherType = static_cast<u16>(EtherType::Ipv4);
    header.vlanTag = VlanTag{5, false, 100};

    CHECK(header.encodedSize() == 18);

    const ByteBuffer frame = encodeEthernet(header, ByteBuffer(60, 0x11));
    const auto decoded = decodeEthernet(frame);

    REQUIRE(decoded.has_value());
    REQUIRE(decoded->header.vlanTag.has_value());
    CHECK(decoded->header.vlanTag->vlanId == 100);
    CHECK(decoded->header.vlanTag->priorityCodePoint == 5);
    CHECK(decoded->header.etherType == static_cast<u16>(EtherType::Ipv4));
}

TEST_CASE("Truncated Ethernet frames fail to decode", "[protocols][ethernet]") {
    CHECK_FALSE(decodeEthernet(ByteBuffer{}).has_value());
    CHECK_FALSE(decodeEthernet(ByteBuffer(10, 0x00)).has_value());
    CHECK_FALSE(decodeEthernet(ByteBuffer(13, 0x00)).has_value());
    CHECK(decodeEthernet(ByteBuffer(14, 0x00)).has_value());
}

TEST_CASE("ARP requests and replies round-trip", "[protocols][arp]") {
    const ArpMessage request = makeArpRequest(mac("11:22:33:44:55:66"), ipv4("192.168.1.10"),
                                              ipv4("192.168.1.1"));

    CHECK(request.operation == ArpOperation::Request);
    CHECK(request.targetMac.isZero()); // that is exactly what is being asked

    const ByteBuffer encoded = encodeArp(request);
    CHECK(encoded.size() == ArpMessage::kEncodedSize);

    const auto decoded = decodeArp(encoded);
    REQUIRE(decoded.has_value());
    CHECK(decoded->senderMac == request.senderMac);
    CHECK(decoded->senderIp == request.senderIp);
    CHECK(decoded->targetIp == request.targetIp);

    const ArpMessage reply = makeArpReply(*decoded, mac("AA:BB:CC:DD:EE:FF"));
    CHECK(reply.operation == ArpOperation::Reply);
    CHECK(reply.senderIp == ipv4("192.168.1.1"));
    CHECK(reply.senderMac == mac("AA:BB:CC:DD:EE:FF"));
    CHECK(reply.targetIp == ipv4("192.168.1.10"));
    CHECK(reply.targetMac == mac("11:22:33:44:55:66"));
}

TEST_CASE("ARP decoding rejects packets it cannot represent", "[protocols][arp]") {
    ByteBuffer encoded = encodeArp(makeArpRequest(mac("11:22:33:44:55:66"), ipv4("10.0.0.1"),
                                                  ipv4("10.0.0.2")));

    CHECK(decodeArp(encoded).has_value());

    SECTION("truncated") {
        encoded.resize(20);
        CHECK_FALSE(decodeArp(encoded).has_value());
    }
    SECTION("a hardware type other than Ethernet") {
        encoded[1] = 6;
        CHECK_FALSE(decodeArp(encoded).has_value());
    }
    SECTION("a protocol type other than IPv4") {
        encoded[3] = 0x86;
        CHECK_FALSE(decodeArp(encoded).has_value());
    }
    SECTION("an unknown operation") {
        encoded[7] = 9;
        CHECK_FALSE(decodeArp(encoded).has_value());
    }
}

TEST_CASE("The Internet checksum matches RFC 1071", "[protocols][checksum]") {
    // The worked example from RFC 1071 section 3.
    const ByteBuffer data = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    CHECK(internetChecksum(data) == 0x220d);

    // Summing a buffer that already contains its own checksum yields zero.
    ByteBuffer checked = data;
    const u16 checksum = internetChecksum(checked);
    checked.push_back(static_cast<u8>(checksum >> 8));
    checked.push_back(static_cast<u8>(checksum & 0xFF));
    CHECK(internetChecksum(checked) == 0);
}

TEST_CASE("CRC-32 matches the known IEEE 802.3 value", "[protocols][checksum]") {
    const std::string text = "123456789";
    const ByteBuffer bytes(text.begin(), text.end());
    CHECK(crc32(bytes) == 0xCBF43926u);
    CHECK(crc32(ByteBuffer{}) == 0u);
}
