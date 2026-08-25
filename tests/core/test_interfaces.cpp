#include "TestHelpers.h"

#include "core/devices/DeviceRegistry.h"
#include "core/devices/Router.h"
#include "core/devices/Switch.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::tests;

TEST_CASE("Devices are created with the interfaces their type implies", "[core][device]") {
    Network network;

    const Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    const Device& router = addDevice(network, DeviceType::Router, "Router1");
    const Device& sw = addDevice(network, DeviceType::Switch, "Switch1");

    CHECK(pc.interfaceCount() == 2); // wired plus a wireless port that starts down
    CHECK(router.interfaceCount() == Router::kDefaultEthernetPorts + Router::kDefaultSerialPorts);
    CHECK(sw.interfaceCount() == Switch::kDefaultPortCount);

    CHECK(pc.findInterfaceByName("GigabitEthernet0") != nullptr);
    CHECK(pc.findInterfaceByName("Wireless0") != nullptr);
    CHECK_FALSE(pc.findInterfaceByName("Wireless0")->isAdminUp());
}

TEST_CASE("Interfaces are found by full name, short name and abbreviation", "[core][interface]") {
    Network network;
    Device& router = addDevice(network, DeviceType::Router, "Router1");

    const Interface* full = router.findInterfaceByName("GigabitEthernet0/1");
    REQUIRE(full != nullptr);

    CHECK(router.findInterfaceByName("Gi0/1") == full);
    CHECK(router.findInterfaceByName("gi0/1") == full);
    CHECK(router.findInterfaceByName("gig0/1") == full);
    CHECK(router.findInterfaceByName("g0/1") == full);
    CHECK(router.findInterfaceByName("Se0/0/1") != nullptr);

    CHECK(router.findInterfaceByName("Gi9/9") == nullptr);
    CHECK(router.findInterfaceByName("") == nullptr);
    CHECK(full->shortName() == "Gi0/1");
}

TEST_CASE("Interfaces reject addresses that cannot sit on a host", "[core][interface]") {
    Network network;
    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Interface& port = iface(pc, "GigabitEthernet0");

    CHECK(port.addIpv4Address(prefix("192.168.1.10/24")).isOk());

    SECTION("the network address is not a host address") {
        const Status status = port.addIpv4Address(prefix("192.168.2.0/24"));
        CHECK_FALSE(status.isOk());
        CHECK(status.message().find("network") != std::string::npos);
    }
    SECTION("the broadcast address is not a host address") {
        CHECK_FALSE(port.addIpv4Address(prefix("192.168.2.255/24")).isOk());
    }
    SECTION("duplicates on the same interface are refused") {
        CHECK_FALSE(port.addIpv4Address(prefix("192.168.1.10/24")).isOk());
    }
    SECTION("multicast cannot be configured") {
        CHECK_FALSE(port.addIpv4Address(prefix("224.0.0.5/24")).isOk());
    }
    SECTION("a second address on another subnet is fine") {
        CHECK(port.addIpv4Address(prefix("10.0.0.1/8")).isOk());
        CHECK(port.ipv4Addresses().size() == 2);
        CHECK(port.primaryIpv4()->address() == ipv4("192.168.1.10"));
    }
}

TEST_CASE("MTU is validated", "[core][interface]") {
    Network network;
    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Interface& port = iface(pc, "GigabitEthernet0");

    CHECK(port.mtu() == kDefaultMtu);
    CHECK(port.setMtu(9000).isOk());
    CHECK(port.mtu() == 9000);
    CHECK_FALSE(port.setMtu(10).isOk());
    CHECK_FALSE(port.setMtu(100000).isOk());
    CHECK(port.mtu() == 9000); // a rejected value must not be applied
}

TEST_CASE("Interface type compatibility governs cabling", "[core][interface]") {
    CHECK(interfaceTypesAreCompatible(InterfaceType::GigabitEthernet, InterfaceType::FastEthernet));
    CHECK(interfaceTypesAreCompatible(InterfaceType::Serial, InterfaceType::Serial));
    CHECK(interfaceTypesAreCompatible(InterfaceType::Wireless, InterfaceType::Wireless));

    CHECK_FALSE(interfaceTypesAreCompatible(InterfaceType::Serial, InterfaceType::GigabitEthernet));
    CHECK_FALSE(interfaceTypesAreCompatible(InterfaceType::Wireless, InterfaceType::GigabitEthernet));
    CHECK_FALSE(interfaceTypesAreCompatible(InterfaceType::Loopback, InterfaceType::Loopback));
    CHECK_FALSE(interfaceTypeIsConnectable(InterfaceType::Loopback));
    CHECK_FALSE(interfaceTypeIsConnectable(InterfaceType::Vlan));
}

TEST_CASE("VLAN configuration decides membership and tagging", "[core][vlan]") {
    VlanConfiguration access;
    access.mode = VlanMode::Access;
    access.accessVlan = 10;

    CHECK(access.allowsVlan(10));
    CHECK_FALSE(access.allowsVlan(20));
    CHECK(access.untaggedVlan() == 10);
    CHECK_FALSE(access.shouldTagOnEgress(10));

    VlanConfiguration trunk;
    trunk.mode = VlanMode::Trunk;
    trunk.nativeVlan = 1;
    trunk.allowedVlans = {1, 10, 20};

    CHECK(trunk.allowsVlan(10));
    CHECK_FALSE(trunk.allowsVlan(30));
    CHECK(trunk.untaggedVlan() == 1);
    CHECK(trunk.shouldTagOnEgress(10));
    CHECK_FALSE(trunk.shouldTagOnEgress(1)); // the native VLAN travels untagged

    VlanConfiguration openTrunk;
    openTrunk.mode = VlanMode::Trunk;
    CHECK(openTrunk.allowsVlan(999)); // an empty allow list means all VLANs
    CHECK_FALSE(openTrunk.allowsVlan(0));
    CHECK_FALSE(openTrunk.allowsVlan(4095));
}

TEST_CASE("Device capability queries replace downcasting", "[core][device]") {
    Network network;

    CHECK(addDevice(network, DeviceType::Router, "R").ipv4Stack() != nullptr);
    CHECK(addDevice(network, DeviceType::Router, "R2").switching() == nullptr);
    CHECK(addDevice(network, DeviceType::Switch, "S").switching() != nullptr);
    CHECK(addDevice(network, DeviceType::Switch, "S2").ipv4Stack() == nullptr);
    CHECK(addDevice(network, DeviceType::Layer3Switch, "L3").ipv4Stack() != nullptr);
    CHECK(addDevice(network, DeviceType::Layer3Switch, "L3b").switching() != nullptr);
    CHECK(addDevice(network, DeviceType::Firewall, "FW").firewallPolicy() != nullptr);
    CHECK(addDevice(network, DeviceType::Server, "SRV").dnsServer() != nullptr);
    CHECK(addDevice(network, DeviceType::Server, "SRV2").dhcpServer() != nullptr);
    CHECK(addDevice(network, DeviceType::Hub, "H").switching() == nullptr);
    CHECK(addDevice(network, DeviceType::Pc, "PC").ipv4Stack()->forwardingEnabled() == false);
    CHECK(addDevice(network, DeviceType::Router, "R3").ipv4Stack()->forwardingEnabled() == true);
}

TEST_CASE("The device registry creates every registered type", "[core][registry]") {
    const DeviceRegistry& registry = builtinDeviceRegistry();

    CHECK(registry.types().size() == 9);
    for (const DeviceTypeInfo& info : registry.types()) {
        auto device = registry.create(info.type, "Test");
        REQUIRE(device != nullptr);
        CHECK(device->type() == info.type);
        CHECK(device->name() == "Test");
    }

    CHECK(registry.typesInCategory(DeviceCategory::Computers).size() == 2);
    CHECK(registry.info(DeviceType::Router) != nullptr);
    CHECK(registry.info(DeviceType::Router)->namePrefix == "Router");
}
