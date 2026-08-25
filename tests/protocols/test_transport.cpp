#include "core/protocols/Tcp.h"
#include "core/protocols/Udp.h"

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

TEST_CASE("UDP datagrams round-trip with a pseudo-header checksum", "[protocols][udp]") {
    const Ipv4Address source = ipv4("192.168.1.10");
    const Ipv4Address destination = ipv4("192.168.1.20");

    UdpHeader header;
    header.sourcePort = 5000;
    header.destinationPort = 53;

    const ByteBuffer payload(64, 0x5A);
    const ByteBuffer datagram = encodeUdp(header, source, destination, payload);

    CHECK(datagram.size() == kUdpHeaderSize + payload.size());
    CHECK(udpChecksumValid(datagram, source, destination));

    const auto decoded = decodeUdp(datagram);
    REQUIRE(decoded.has_value());
    CHECK(decoded->header.sourcePort == 5000);
    CHECK(decoded->header.destinationPort == 53);
    CHECK(decoded->header.length == datagram.size());
    CHECK(decoded->payload.size() == payload.size());
}

TEST_CASE("A UDP checksum computed against the wrong addresses fails", "[protocols][udp]") {
    UdpHeader header;
    header.sourcePort = 1;
    header.destinationPort = 2;

    const ByteBuffer datagram = encodeUdp(header, ipv4("10.0.0.1"), ipv4("10.0.0.2"),
                                          ByteBuffer(8, 0x00));

    CHECK(udpChecksumValid(datagram, ipv4("10.0.0.1"), ipv4("10.0.0.2")));
    CHECK_FALSE(udpChecksumValid(datagram, ipv4("10.0.0.1"), ipv4("10.0.0.3")));
}

TEST_CASE("Truncated UDP datagrams are rejected", "[protocols][udp]") {
    CHECK_FALSE(decodeUdp(ByteBuffer{}).has_value());
    CHECK_FALSE(decodeUdp(ByteBuffer(4, 0x00)).has_value());

    // A length field larger than the buffer must not be trusted.
    ByteBuffer datagram(8, 0x00);
    datagram[4] = 0xFF;
    datagram[5] = 0xFF;
    CHECK_FALSE(decodeUdp(datagram).has_value());
}

TEST_CASE("TCP flags convert both ways", "[protocols][tcp]") {
    TcpFlags flags;
    flags.syn = true;
    flags.ack = true;

    CHECK(flags.toBits() == 0x12);
    CHECK(flags.toString() == "SYN, ACK");
    CHECK(TcpFlags::fromBits(0x12) == flags);
    CHECK(TcpFlags::fromBits(0x02).syn);
    CHECK(TcpFlags::fromBits(0x04).rst);
    CHECK(TcpFlags{}.toString() == "none");
}

TEST_CASE("TCP segments round-trip", "[protocols][tcp]") {
    const Ipv4Address source = ipv4("192.168.1.10");
    const Ipv4Address destination = ipv4("192.168.1.20");

    TcpHeader header;
    header.sourcePort = 44000;
    header.destinationPort = 80;
    header.sequenceNumber = 0xDEADBEEF;
    header.acknowledgementNumber = 0x12345678;
    header.flags.syn = true;
    header.window = 64240;

    const ByteBuffer segment = encodeTcp(header, source, destination, ByteBuffer(16, 0x77));

    CHECK(segment.size() == kTcpMinimumHeaderSize + 16);
    CHECK(tcpChecksumValid(segment, source, destination));

    const auto decoded = decodeTcp(segment);
    REQUIRE(decoded.has_value());
    CHECK(decoded->header.sourcePort == 44000);
    CHECK(decoded->header.destinationPort == 80);
    CHECK(decoded->header.sequenceNumber == 0xDEADBEEF);
    CHECK(decoded->header.acknowledgementNumber == 0x12345678);
    CHECK(decoded->header.flags.syn);
    CHECK_FALSE(decoded->header.flags.ack);
    CHECK(decoded->header.window == 64240);
    CHECK(decoded->payload.size() == 16);
}

TEST_CASE("TCP state names are stable", "[protocols][tcp]") {
    CHECK(tcpStateName(TcpState::Closed) == "CLOSED");
    CHECK(tcpStateName(TcpState::Established) == "ESTABLISHED");
    CHECK(tcpStateName(TcpState::TimeWait) == "TIME-WAIT");
}
