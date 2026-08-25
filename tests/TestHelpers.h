#pragma once

// Shared helpers for the test suite.
//
// Building a topology by hand is verbose enough that repeating it in every test
// obscures what is actually being asserted, so the common shapes live here.

#include "core/devices/DeviceRegistry.h"
#include "core/devices/Ipv4Stack.h"
#include "core/network/Network.h"
#include "core/project/Project.h"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

namespace tnp::tests {

/// Adds a device of `type` named `name`.
inline core::Device& addDevice(core::Network& network, core::DeviceType type, std::string name) {
    return network.addDevice(core::builtinDeviceRegistry().create(type, std::move(name)));
}

/// Looks up an interface, failing the test if it is missing.
inline core::Interface& iface(core::Device& device, std::string_view name) {
    core::Interface* found = device.findInterfaceByName(name);
    REQUIRE(found != nullptr);
    return *found;
}

/// Cables two interfaces, failing the test if the connection is refused.
inline core::LinkId connect(core::Network& network, core::Device& a, std::string_view aName,
                            core::Device& b, std::string_view bName) {
    auto link = network.connect(iface(a, aName).id(), iface(b, bName).id());
    REQUIRE(link.isOk());
    return link.value();
}

/// Assigns an address in CIDR notation, failing the test if it is rejected.
inline void assign(core::Device& device, std::string_view interfaceName, std::string_view cidr) {
    const auto prefix = core::Ipv4Prefix::parse(cidr);
    REQUIRE(prefix.has_value());
    REQUIRE(iface(device, interfaceName).addIpv4Address(*prefix).isOk());
}

inline core::Ipv4Address ipv4(std::string_view text) {
    const auto address = core::Ipv4Address::parse(text);
    REQUIRE(address.has_value());
    return *address;
}

inline core::Ipv4Prefix prefix(std::string_view text) {
    const auto value = core::Ipv4Prefix::parse(text);
    REQUIRE(value.has_value());
    return *value;
}

inline core::MacAddress mac(std::string_view text) {
    const auto value = core::MacAddress::parse(text);
    REQUIRE(value.has_value());
    return *value;
}

/// Adds a static route, failing the test if it cannot be installed.
inline void addRoute(core::Device& device, std::string_view destination, std::string_view nextHop) {
    core::StaticRouteEntry entry;
    entry.destination = prefix(destination);
    entry.nextHop = ipv4(nextHop);
    REQUIRE(device.ipv4Stack() != nullptr);
    REQUIRE(device.ipv4Stack()->addStaticRoute(entry).isOk());
}

} // namespace tnp::tests
