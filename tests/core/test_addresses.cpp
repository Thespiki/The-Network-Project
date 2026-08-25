#include "core/network/Ipv4Address.h"
#include "core/network/Ipv6Address.h"
#include "core/network/MacAddress.h"
#include "utilities/Uuid.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;

TEST_CASE("MAC addresses parse every notation engineers use", "[core][mac]") {
    const auto colons = MacAddress::parse("AA:BB:CC:DD:EE:FF");
    const auto dashes = MacAddress::parse("aa-bb-cc-dd-ee-ff");
    const auto cisco = MacAddress::parse("aabb.ccdd.eeff");
    const auto bare = MacAddress::parse("AABBCCDDEEFF");

    REQUIRE(colons.has_value());
    REQUIRE(dashes.has_value());
    REQUIRE(cisco.has_value());
    REQUIRE(bare.has_value());

    CHECK(*colons == *dashes);
    CHECK(*colons == *cisco);
    CHECK(*colons == *bare);
    CHECK(colons->toString() == "AA:BB:CC:DD:EE:FF");
    CHECK(colons->toCiscoString() == "aabb.ccdd.eeff");
}

TEST_CASE("MAC address parsing rejects malformed input", "[core][mac]") {
    CHECK_FALSE(MacAddress::parse("AA:BB:CC:DD:EE").has_value());
    CHECK_FALSE(MacAddress::parse("AA:BB:CC:DD:EE:FF:00").has_value());
    CHECK_FALSE(MacAddress::parse("AA:BB:CC:DD:EE:GG").has_value());
    CHECK_FALSE(MacAddress::parse("AA:BB::DD:EE:FF").has_value());
    CHECK_FALSE(MacAddress::parse("").has_value());
    CHECK_FALSE(MacAddress::parse("not a mac").has_value());
}

TEST_CASE("MAC address classification", "[core][mac]") {
    CHECK(MacAddress::broadcast().isBroadcast());
    CHECK(MacAddress::broadcast().isMulticast()); // broadcast is the all-ones multicast address
    CHECK(MacAddress::zero().isZero());
    CHECK(MacAddress::parse("01:00:5E:00:00:01")->isMulticast());
    CHECK(MacAddress::parse("02:00:00:00:00:01")->isUnicast());
    CHECK(MacAddress::parse("02:00:00:00:00:01")->isLocallyAdministered());
    CHECK(MacAddress::parse("AA:BB:CC:DD:EE:FF")->oui() == 0xAABBCC);
}

TEST_CASE("Generated MAC addresses are locally administered unicast", "[core][mac]") {
    for (int i = 0; i < 32; ++i) {
        const MacAddress generated = MacAddress::generateUnicast();
        CHECK(generated.isUnicast());
        CHECK(generated.isLocallyAdministered());
        CHECK_FALSE(generated.isZero());
    }
}

TEST_CASE("IPv4 addresses parse and round-trip", "[core][ipv4]") {
    const auto address = Ipv4Address::parse("192.168.1.10");
    REQUIRE(address.has_value());

    CHECK(address->toString() == "192.168.1.10");
    CHECK(address->octet(0) == 192);
    CHECK(address->octet(3) == 10);
    CHECK(address->value() == 0xC0A8010A);
    CHECK(*address == Ipv4Address(192, 168, 1, 10));
}

TEST_CASE("IPv4 parsing is strict", "[core][ipv4]") {
    CHECK_FALSE(Ipv4Address::parse("192.168.1").has_value());
    CHECK_FALSE(Ipv4Address::parse("192.168.1.1.1").has_value());
    CHECK_FALSE(Ipv4Address::parse("192.168.1.256").has_value());
    CHECK_FALSE(Ipv4Address::parse("192.168.1.-1").has_value());
    CHECK_FALSE(Ipv4Address::parse("192.168.01.1").has_value()); // leading zeros invite octal confusion
    CHECK_FALSE(Ipv4Address::parse("192.168.1.a").has_value());
    CHECK_FALSE(Ipv4Address::parse("").has_value());
    CHECK_FALSE(Ipv4Address::parse("192.168..1").has_value());

    CHECK(Ipv4Address::parse("0.0.0.0").has_value());
    CHECK(Ipv4Address::parse("255.255.255.255").has_value());
}

TEST_CASE("IPv4 classification", "[core][ipv4]") {
    CHECK(Ipv4Address::parse("127.0.0.1")->isLoopback());
    CHECK(Ipv4Address::parse("10.1.2.3")->isPrivate());
    CHECK(Ipv4Address::parse("172.16.0.1")->isPrivate());
    CHECK(Ipv4Address::parse("172.32.0.1")->isPrivate() == false);
    CHECK(Ipv4Address::parse("192.168.5.5")->isPrivate());
    CHECK(Ipv4Address::parse("8.8.8.8")->isPrivate() == false);
    CHECK(Ipv4Address::parse("224.0.0.1")->isMulticast());
    CHECK(Ipv4Address::parse("169.254.1.1")->isLinkLocal());
    CHECK(Ipv4Address::any().isUnspecified());

    CHECK_FALSE(Ipv4Address::any().isAssignableToHost());
    CHECK_FALSE(Ipv4Address::limitedBroadcast().isAssignableToHost());
    CHECK_FALSE(Ipv4Address::parse("224.0.0.5")->isAssignableToHost());
    CHECK(Ipv4Address::parse("192.168.1.10")->isAssignableToHost());
}

TEST_CASE("IPv6 addresses parse in every legal form", "[core][ipv6]") {
    const auto full = Ipv6Address::parse("2001:0db8:0000:0000:0000:0000:0000:0001");
    const auto compressed = Ipv6Address::parse("2001:db8::1");
    REQUIRE(full.has_value());
    REQUIRE(compressed.has_value());
    CHECK(*full == *compressed);
    CHECK(compressed->toString() == "2001:db8::1");
    CHECK(full->toExpandedString() == "2001:0db8:0000:0000:0000:0000:0000:0001");

    CHECK(Ipv6Address::parse("::")->isUnspecified());
    CHECK(Ipv6Address::parse("::1")->isLoopback());
    CHECK(Ipv6Address::parse("fe80::1")->isLinkLocal());
    CHECK(Ipv6Address::parse("ff02::1")->isMulticast());
    CHECK(Ipv6Address::parse("fd00::1")->isUniqueLocal());
    CHECK(Ipv6Address::parse("::ffff:192.0.2.1").has_value());
}

TEST_CASE("IPv6 parsing rejects malformed input", "[core][ipv6]") {
    CHECK_FALSE(Ipv6Address::parse("2001:db8::1::2").has_value()); // two compressions
    CHECK_FALSE(Ipv6Address::parse("2001:db8:1:2:3:4:5").has_value()); // too few groups
    CHECK_FALSE(Ipv6Address::parse("2001:db8:1:2:3:4:5:6:7").has_value());
    CHECK_FALSE(Ipv6Address::parse("2001:zzzz::1").has_value());
    CHECK_FALSE(Ipv6Address::parse("fe80::1%eth0").has_value()); // zone indices unsupported
}

TEST_CASE("IPv6 text form follows RFC 5952", "[core][ipv6]") {
    // The longest zero run is compressed, and only once.
    CHECK(Ipv6Address::parse("2001:db8:0:0:1:0:0:1")->toString() == "2001:db8::1:0:0:1");
    CHECK(Ipv6Address::parse("0:0:0:0:0:0:0:0")->toString() == "::");
    CHECK(Ipv6Address::parse("2001:db8:0:1:1:1:1:1")->toString() == "2001:db8:0:1:1:1:1:1");
}

TEST_CASE("UUIDs round-trip and are distinct", "[core][uuid]") {
    const Uuid generated = Uuid::generate();
    const auto parsed = Uuid::parse(generated.toString());

    REQUIRE(parsed.has_value());
    CHECK(*parsed == generated);
    CHECK(generated.toString().size() == 36);
    CHECK_FALSE(generated.isNil());
    CHECK(Uuid{}.isNil());

    CHECK(Uuid::generate() != Uuid::generate());

    CHECK_FALSE(Uuid::parse("not-a-uuid").has_value());
    CHECK_FALSE(Uuid::parse("").has_value());
    CHECK(Uuid::parse("{" + generated.toString() + "}").has_value());
}
