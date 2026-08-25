#include "TestHelpers.h"

#include "core/routing/StaticRouting.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::tests;

TEST_CASE("Connected routes come from operational interface addresses", "[routing][static]") {
    Network network;
    Device& router = addDevice(network, DeviceType::Router, "Router1");
    Device& peer = addDevice(network, DeviceType::Router, "Router2");

    connect(network, router, "Gi0/0", peer, "Gi0/0");
    connect(network, router, "Gi0/1", peer, "Gi0/1");
    assign(router, "Gi0/0", "192.168.1.1/24");
    assign(router, "Gi0/1", "10.0.0.1/30");
    network.refreshOperationalStates();

    RoutingTable table;
    installConnectedRoutes(router, table);

    CHECK(table.size() == 2);
    CHECK(table.lookup(ipv4("192.168.1.50")) != nullptr);
    CHECK(table.lookup(ipv4("10.0.0.2")) != nullptr);
    CHECK(table.lookup(ipv4("172.16.0.1")) == nullptr);

    SECTION("a down interface contributes nothing") {
        iface(router, "Gi0/1").setAdminState(AdminState::Down);
        network.refreshOperationalStates();

        installConnectedRoutes(router, table);
        CHECK(table.size() == 1);
        CHECK(table.lookup(ipv4("10.0.0.2")) == nullptr);
    }
}

TEST_CASE("A static route resolves its egress from the next hop", "[routing][static]") {
    Network network;
    Device& router = addDevice(network, DeviceType::Router, "Router1");
    Device& peer = addDevice(network, DeviceType::Router, "Router2");

    connect(network, router, "Gi0/1", peer, "Gi0/0");
    assign(router, "Gi0/1", "10.0.0.1/30");
    network.refreshOperationalStates();

    StaticRouteEntry entry;
    entry.destination = prefix("172.16.0.0/24");
    entry.nextHop = ipv4("10.0.0.2");

    const StaticRouteResolution resolution = resolveStaticRoute(router, entry);
    REQUIRE(resolution.route.has_value());
    CHECK(resolution.route->egressInterface == iface(router, "Gi0/1").id());
    CHECK(resolution.route->source == RouteSource::Static);
}

TEST_CASE("A static route with an unreachable next hop is refused", "[routing][static]") {
    Network network;
    Device& router = addDevice(network, DeviceType::Router, "Router1");
    assign(router, "Gi0/0", "192.168.1.1/24");

    StaticRouteEntry entry;
    entry.destination = prefix("172.16.0.0/24");
    entry.nextHop = ipv4("203.0.113.1"); // on no connected subnet

    const StaticRouteResolution resolution = resolveStaticRoute(router, entry);
    CHECK_FALSE(resolution.route.has_value());
    CHECK(resolution.problem.find("not on a connected subnet") != std::string::npos);
}

TEST_CASE("A static route needs either a next hop or an interface", "[routing][static]") {
    Network network;
    Device& router = addDevice(network, DeviceType::Router, "Router1");

    StaticRouteEntry entry;
    entry.destination = prefix("172.16.0.0/24");

    const StaticRouteResolution resolution = resolveStaticRoute(router, entry);
    CHECK_FALSE(resolution.route.has_value());
    CHECK_FALSE(resolution.problem.empty());
}

TEST_CASE("The default gateway is the default static route", "[routing][static]") {
    Network network;
    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& router = addDevice(network, DeviceType::Router, "Router1");

    connect(network, pc, "Gi0", router, "Gi0/0");
    assign(pc, "Gi0", "192.168.1.10/24");
    assign(router, "Gi0/0", "192.168.1.1/24");
    network.refreshOperationalStates();

    Ipv4Stack* stack = pc.ipv4Stack();
    REQUIRE(stack != nullptr);
    CHECK_FALSE(stack->defaultGateway().has_value());

    stack->setDefaultGateway(ipv4("192.168.1.1"));

    CHECK(stack->defaultGateway() == ipv4("192.168.1.1"));
    CHECK(stack->staticRoutes().size() == 1);

    const Route* route = stack->routingTable().lookup(ipv4("8.8.8.8"));
    REQUIRE(route != nullptr);
    CHECK(route->isDefaultRoute());
    CHECK(route->nextHop == ipv4("192.168.1.1"));

    SECTION("setting it again replaces rather than duplicates") {
        stack->setDefaultGateway(ipv4("192.168.1.254"));
        CHECK(stack->staticRoutes().size() == 1);
        CHECK(stack->routingTable().lookup(ipv4("8.8.8.8"))->nextHop == ipv4("192.168.1.254"));
    }
    SECTION("clearing it removes the route") {
        stack->setDefaultGateway(std::nullopt);
        CHECK(stack->staticRoutes().empty());
        CHECK(stack->routingTable().lookup(ipv4("8.8.8.8")) == nullptr);
    }
}

TEST_CASE("Rebuilding the table keeps configuration and derives the rest", "[routing][static]") {
    Network network;
    Device& router = addDevice(network, DeviceType::Router, "Router1");
    Device& peer = addDevice(network, DeviceType::Router, "Router2");

    connect(network, router, "Gi0/1", peer, "Gi0/0");
    assign(router, "Gi0/1", "10.0.0.1/30");
    network.refreshOperationalStates();

    Ipv4Stack* stack = router.ipv4Stack();
    addRoute(router, "172.16.0.0/24", "10.0.0.2");

    CHECK(stack->staticRoutes().size() == 1);
    CHECK(stack->routingTable().size() == 2); // one connected, one static

    // The configured route survives a rebuild; the connected one is recomputed.
    stack->refreshRoutes();
    CHECK(stack->staticRoutes().size() == 1);
    CHECK(stack->routingTable().size() == 2);
}
