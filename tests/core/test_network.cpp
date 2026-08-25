#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::tests;

TEST_CASE("Links attach to interfaces, not to devices", "[core][network]") {
    Network network;
    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");

    const LinkId link = connect(network, pc, "Gi0", sw, "Gi0/1");
    const Link* created = network.findLink(link);
    REQUIRE(created != nullptr);

    CHECK(created->involves(pc.id()));
    CHECK(created->involves(iface(pc, "Gi0").id()));
    CHECK(iface(pc, "Gi0").isConnected());
    CHECK(network.linkOfInterface(iface(pc, "Gi0").id()) == created);

    const auto peer = created->peerOf(iface(pc, "Gi0").id());
    REQUIRE(peer.has_value());
    CHECK(peer->device == sw.id());
    CHECK(peer->interface == iface(sw, "Gi0/1").id());

    // The other seven switch ports are untouched.
    CHECK_FALSE(iface(sw, "Gi0/2").isConnected());
}

TEST_CASE("Invalid connections are refused with a reason", "[core][network]") {
    Network network;
    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& pc2 = addDevice(network, DeviceType::Pc, "PC2");
    Device& router = addDevice(network, DeviceType::Router, "Router1");

    SECTION("an interface cannot connect to itself") {
        const InterfaceId port = iface(pc, "Gi0").id();
        CHECK_FALSE(network.canConnect(port, port).isOk());
    }
    SECTION("incompatible media are refused") {
        CHECK_FALSE(network.canConnect(iface(pc, "Gi0").id(), iface(router, "Se0/0/0").id()).isOk());
    }
    SECTION("an interface takes only one cable") {
        connect(network, pc, "Gi0", router, "Gi0/0");
        CHECK_FALSE(network.canConnect(iface(pc, "Gi0").id(), iface(pc2, "Gi0").id()).isOk());
    }
    SECTION("wireless only pairs with wireless") {
        CHECK(network.canConnect(iface(pc, "Wireless0").id(), iface(pc2, "Wireless0").id()).isOk());
        CHECK_FALSE(network.canConnect(iface(pc, "Wireless0").id(), iface(pc2, "Gi0").id()).isOk());
    }
}

TEST_CASE("Operational state follows admin state and link health", "[core][network]") {
    Network network;
    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");

    CHECK_FALSE(iface(pc, "Gi0").isOperational()); // nothing attached yet

    const LinkId link = connect(network, pc, "Gi0", sw, "Gi0/1");
    CHECK(iface(pc, "Gi0").isOperational());
    CHECK(iface(sw, "Gi0/1").isOperational());

    SECTION("shutting one end down takes both down") {
        iface(pc, "Gi0").setAdminState(AdminState::Down);
        network.refreshOperationalStates();
        CHECK_FALSE(iface(pc, "Gi0").isOperational());
        CHECK_FALSE(iface(sw, "Gi0/1").isOperational());
    }
    SECTION("disabling the link takes both down") {
        network.findLink(link)->setEnabled(false);
        network.refreshOperationalStates();
        CHECK_FALSE(iface(pc, "Gi0").isOperational());
        CHECK_FALSE(iface(sw, "Gi0/1").isOperational());
    }
    SECTION("removing the cable takes both down") {
        auto removed = network.disconnect(link);
        CHECK(removed != nullptr);
        CHECK_FALSE(iface(pc, "Gi0").isOperational());
        CHECK_FALSE(iface(pc, "Gi0").isConnected());
    }
}

TEST_CASE("Removing a device takes its links with it and can be undone", "[core][network]") {
    Network network;
    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");
    Device& router = addDevice(network, DeviceType::Router, "Router1");

    connect(network, pc, "Gi0", sw, "Gi0/1");
    connect(network, sw, "Gi0/2", router, "Gi0/0");
    CHECK(network.linkCount() == 2);

    const DeviceId switchId = sw.id();
    RemovedDevice removed = network.removeDevice(switchId);

    REQUIRE(removed.device != nullptr);
    CHECK(removed.links.size() == 2);
    CHECK(network.deviceCount() == 2);
    CHECK(network.linkCount() == 0);
    CHECK(network.findDevice(switchId) == nullptr);
    CHECK_FALSE(iface(pc, "Gi0").isConnected());

    network.restoreDevice(std::move(removed));

    CHECK(network.deviceCount() == 3);
    CHECK(network.linkCount() == 2);
    CHECK(network.findDevice(switchId) != nullptr);
    CHECK(iface(pc, "Gi0").isConnected());
    CHECK(iface(pc, "Gi0").isOperational());
}

TEST_CASE("Indices answer lookups without scanning", "[core][network]") {
    Network network;
    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");
    connect(network, pc, "Gi0", sw, "Gi0/1");

    CHECK(network.findDevice(pc.id()) == &pc);
    CHECK(network.findDeviceByName("pc1") == &pc); // names are case-insensitive
    CHECK(network.findInterface(iface(pc, "Gi0").id()) == &iface(pc, "Gi0"));
    CHECK(network.ownerOf(iface(pc, "Gi0").id()) == pc.id());
    CHECK(network.linksOf(pc.id()).size() == 1);
    CHECK(network.neighborsOf(pc.id()) == std::vector<DeviceId>{sw.id()});

    // Indices survive a rebuild, which is what deserialization relies on.
    network.rebuildIndices();
    CHECK(network.findDevice(pc.id()) == &pc);
    CHECK(network.linksOf(sw.id()).size() == 1);
}

TEST_CASE("Suggested names do not collide", "[core][network]") {
    Network network;

    CHECK(network.suggestDeviceName(DeviceType::Router) == "Router1");
    addDevice(network, DeviceType::Router, "Router1");
    CHECK(network.suggestDeviceName(DeviceType::Router) == "Router2");
    addDevice(network, DeviceType::Router, "Router2");
    CHECK(network.suggestDeviceName(DeviceType::Router) == "Router3");
    CHECK(network.suggestDeviceName(DeviceType::Pc) == "PC1");
}

TEST_CASE("Connected routes appear and disappear with interface state", "[core][network][routing]") {
    Network network;
    Device& router = addDevice(network, DeviceType::Router, "Router1");
    Device& pc = addDevice(network, DeviceType::Pc, "PC1");

    connect(network, router, "Gi0/0", pc, "Gi0");
    assign(router, "Gi0/0", "192.168.1.1/24");
    network.refreshOperationalStates();

    const RoutingTable* table = router.routingTable();
    REQUIRE(table != nullptr);
    REQUIRE(table->lookup(ipv4("192.168.1.50")) != nullptr);
    CHECK(table->lookup(ipv4("192.168.1.50"))->source == RouteSource::Connected);

    iface(router, "Gi0/0").setAdminState(AdminState::Down);
    network.refreshOperationalStates();
    CHECK(table->lookup(ipv4("192.168.1.50")) == nullptr);
}
