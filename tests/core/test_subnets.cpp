#include "core/network/Subnet.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;

namespace {
Ipv4Prefix parse(const char* text) {
    const auto value = Ipv4Prefix::parse(text);
    REQUIRE(value.has_value());
    return *value;
}
} // namespace

TEST_CASE("CIDR prefixes parse and keep the host address", "[core][subnet]") {
    const Ipv4Prefix prefix = parse("192.168.1.10/24");

    CHECK(prefix.address().toString() == "192.168.1.10");
    CHECK(prefix.prefixLength() == 24);
    CHECK(prefix.toString() == "192.168.1.10/24");
    CHECK(prefix.toNetworkString() == "192.168.1.0/24");
    CHECK(prefix.toMaskString() == "192.168.1.10 255.255.255.0");

    CHECK_FALSE(Ipv4Prefix::parse("192.168.1.10").has_value());
    CHECK_FALSE(Ipv4Prefix::parse("192.168.1.10/33").has_value());
    CHECK_FALSE(Ipv4Prefix::parse("192.168.1.10/-1").has_value());
    CHECK_FALSE(Ipv4Prefix::parse("999.1.1.1/24").has_value());
}

TEST_CASE("Subnet arithmetic", "[core][subnet]") {
    const Ipv4Prefix prefix = parse("192.168.1.10/24");

    CHECK(prefix.networkAddress().toString() == "192.168.1.0");
    CHECK(prefix.broadcastAddress().toString() == "192.168.1.255");
    CHECK(prefix.netmask().toString() == "255.255.255.0");
    CHECK(prefix.wildcardMask().toString() == "0.0.0.255");
    CHECK(prefix.firstUsableAddress().toString() == "192.168.1.1");
    CHECK(prefix.lastUsableAddress().toString() == "192.168.1.254");
    CHECK(prefix.usableHostCount() == 254);
    CHECK(prefix.totalAddressCount() == 256);
    CHECK(prefix.hasBroadcastAddress());
}

TEST_CASE("Edge prefix lengths behave correctly", "[core][subnet]") {
    SECTION("/0 covers everything") {
        const Ipv4Prefix everything = parse("0.0.0.0/0");
        CHECK(everything.netmask().toString() == "0.0.0.0");
        CHECK(everything.contains(*Ipv4Address::parse("8.8.8.8")));
        CHECK(everything.isDefaultRoute());
        CHECK(everything.totalAddressCount() == 4294967296ULL);
    }

    SECTION("/31 is a point-to-point link with two usable addresses") {
        const Ipv4Prefix link = parse("10.0.0.0/31");
        CHECK(link.isPointToPoint());
        CHECK(link.usableHostCount() == 2);
        CHECK(link.firstUsableAddress().toString() == "10.0.0.0");
        CHECK(link.lastUsableAddress().toString() == "10.0.0.1");
        CHECK_FALSE(link.hasBroadcastAddress());
        CHECK(link.isUsableHostAddress(*Ipv4Address::parse("10.0.0.0")));
    }

    SECTION("/32 is a single host route") {
        const Ipv4Prefix host = parse("10.0.0.5/32");
        CHECK(host.isHostRoute());
        CHECK(host.usableHostCount() == 1);
        CHECK(host.firstUsableAddress() == host.lastUsableAddress());
        CHECK_FALSE(host.hasBroadcastAddress());
    }

    SECTION("/30 leaves two usable addresses") {
        const Ipv4Prefix link = parse("10.0.0.0/30");
        CHECK(link.usableHostCount() == 2);
        CHECK(link.firstUsableAddress().toString() == "10.0.0.1");
        CHECK(link.lastUsableAddress().toString() == "10.0.0.2");
    }
}

TEST_CASE("Containment and overlap", "[core][subnet]") {
    const Ipv4Prefix lan = parse("192.168.1.0/24");

    CHECK(lan.contains(*Ipv4Address::parse("192.168.1.1")));
    CHECK(lan.contains(*Ipv4Address::parse("192.168.1.255")));
    CHECK_FALSE(lan.contains(*Ipv4Address::parse("192.168.2.1")));

    CHECK(parse("10.0.0.0/8").contains(parse("10.1.0.0/16")));
    CHECK_FALSE(parse("10.1.0.0/16").contains(parse("10.0.0.0/8")));
    CHECK(parse("10.0.0.0/8").overlaps(parse("10.1.0.0/16")));
    CHECK(parse("10.1.0.0/16").overlaps(parse("10.0.0.0/8")));
    CHECK_FALSE(parse("10.0.0.0/8").overlaps(parse("172.16.0.0/12")));
}

TEST_CASE("Host addresses exclude the network and broadcast addresses", "[core][subnet]") {
    const Ipv4Prefix lan = parse("192.168.1.0/24");

    CHECK_FALSE(lan.isUsableHostAddress(*Ipv4Address::parse("192.168.1.0")));
    CHECK_FALSE(lan.isUsableHostAddress(*Ipv4Address::parse("192.168.1.255")));
    CHECK_FALSE(lan.isUsableHostAddress(*Ipv4Address::parse("192.168.2.1")));
    CHECK(lan.isUsableHostAddress(*Ipv4Address::parse("192.168.1.1")));
    CHECK(lan.isUsableHostAddress(*Ipv4Address::parse("192.168.1.254")));
}

TEST_CASE("Masks convert to prefix lengths and reject non-contiguous values", "[core][subnet]") {
    CHECK(Ipv4Prefix::maskForPrefixLength(24).toString() == "255.255.255.0");
    CHECK(Ipv4Prefix::maskForPrefixLength(0).toString() == "0.0.0.0");
    CHECK(Ipv4Prefix::maskForPrefixLength(32).toString() == "255.255.255.255");
    CHECK(Ipv4Prefix::maskForPrefixLength(30).toString() == "255.255.255.252");

    CHECK(Ipv4Prefix::prefixLengthForMask(*Ipv4Address::parse("255.255.255.0")) == 24);
    CHECK(Ipv4Prefix::prefixLengthForMask(*Ipv4Address::parse("0.0.0.0")) == 0);
    CHECK(Ipv4Prefix::prefixLengthForMask(*Ipv4Address::parse("255.255.255.255")) == 32);

    // The classic typo: a mask with a hole in it.
    CHECK_FALSE(Ipv4Prefix::prefixLengthForMask(*Ipv4Address::parse("255.0.255.0")).has_value());
    CHECK_FALSE(Ipv4Prefix::prefixLengthForMask(*Ipv4Address::parse("255.255.255.1")).has_value());
}

TEST_CASE("isSameSubnet answers the question hosts actually ask", "[core][subnet]") {
    const Ipv4Address a = *Ipv4Address::parse("192.168.1.10");
    const Ipv4Address b = *Ipv4Address::parse("192.168.1.200");
    const Ipv4Address c = *Ipv4Address::parse("192.168.2.10");

    CHECK(Ipv4Prefix::isSameSubnet(a, b, 24));
    CHECK_FALSE(Ipv4Prefix::isSameSubnet(a, c, 24));
    CHECK(Ipv4Prefix::isSameSubnet(a, c, 16));
    CHECK_FALSE(Ipv4Prefix::isSameSubnet(a, b, 25));
}

TEST_CASE("IPv6 prefixes", "[core][subnet]") {
    const auto prefix = Ipv6Prefix::parse("2001:db8::1/64");
    REQUIRE(prefix.has_value());

    CHECK(prefix->prefixLength() == 64);
    CHECK(prefix->toNetworkString() == "2001:db8::/64");
    CHECK(prefix->contains(*Ipv6Address::parse("2001:db8::ffff")));
    CHECK_FALSE(prefix->contains(*Ipv6Address::parse("2001:db9::1")));
}
