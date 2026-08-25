#include "core/routing/RoutingTable.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;

namespace {

Ipv4Prefix prefix(const char* text) {
    const auto value = Ipv4Prefix::parse(text);
    REQUIRE(value.has_value());
    return *value;
}

Ipv4Address ipv4(const char* text) {
    const auto value = Ipv4Address::parse(text);
    REQUIRE(value.has_value());
    return *value;
}

Route makeRoute(const char* destination, const char* nextHop, RouteSource source = RouteSource::Static,
                u32 metric = 1) {
    Route route;
    route.id = RouteId::generate();
    route.destination = prefix(destination);
    route.nextHop = ipv4(nextHop);
    route.egressInterface = InterfaceId::generate();
    route.source = source;
    route.metric = metric;
    return route;
}

} // namespace

TEST_CASE("Lookup returns the longest matching prefix", "[routing]") {
    RoutingTable table;

    REQUIRE(table.add(makeRoute("0.0.0.0/0", "192.168.1.1")).isOk());
    REQUIRE(table.add(makeRoute("10.0.0.0/8", "10.1.0.1")).isOk());
    REQUIRE(table.add(makeRoute("10.20.0.0/16", "10.20.0.1")).isOk());
    REQUIRE(table.add(makeRoute("10.20.30.0/24", "10.20.30.1")).isOk());

    CHECK(table.lookup(ipv4("10.20.30.5"))->destination.prefixLength() == 24);
    CHECK(table.lookup(ipv4("10.20.99.5"))->destination.prefixLength() == 16);
    CHECK(table.lookup(ipv4("10.99.0.5"))->destination.prefixLength() == 8);
    CHECK(table.lookup(ipv4("8.8.8.8"))->destination.prefixLength() == 0);
    CHECK(table.lookup(ipv4("8.8.8.8"))->isDefaultRoute());
}

TEST_CASE("An empty table matches nothing", "[routing]") {
    RoutingTable table;
    CHECK(table.empty());
    CHECK(table.lookup(ipv4("10.0.0.1")) == nullptr);
    CHECK(table.defaultRoute() == nullptr);
}

TEST_CASE("Administrative distance breaks ties between equal prefixes", "[routing]") {
    RoutingTable table;

    REQUIRE(table.add(makeRoute("10.0.0.0/8", "10.1.0.1", RouteSource::Ospf)).isOk());
    REQUIRE(table.add(makeRoute("10.0.0.0/8", "10.2.0.1", RouteSource::Static)).isOk());

    const Route* best = table.lookup(ipv4("10.5.5.5"));
    REQUIRE(best != nullptr);
    CHECK(best->source == RouteSource::Static); // distance 1 beats 110
    CHECK(best->nextHop == ipv4("10.2.0.1"));
}

TEST_CASE("Metric breaks ties within one source", "[routing]") {
    RoutingTable table;

    REQUIRE(table.add(makeRoute("10.0.0.0/8", "10.1.0.1", RouteSource::Static, 20)).isOk());
    REQUIRE(table.add(makeRoute("10.0.0.0/8", "10.2.0.1", RouteSource::Static, 5)).isOk());

    CHECK(table.lookup(ipv4("10.5.5.5"))->nextHop == ipv4("10.2.0.1"));
}

TEST_CASE("Disabled routes are skipped", "[routing]") {
    RoutingTable table;

    Route disabled = makeRoute("10.0.0.0/8", "10.1.0.1");
    disabled.enabled = false;
    table.addUnchecked(disabled);
    REQUIRE(table.add(makeRoute("0.0.0.0/0", "192.168.1.1")).isOk());

    const Route* best = table.lookup(ipv4("10.5.5.5"));
    REQUIRE(best != nullptr);
    CHECK(best->isDefaultRoute());
}

TEST_CASE("Duplicate routes from the same source are refused", "[routing]") {
    RoutingTable table;

    const Route route = makeRoute("10.0.0.0/8", "10.1.0.1");
    REQUIRE(table.add(route).isOk());

    Route duplicate = route;
    duplicate.id = RouteId::generate();
    duplicate.egressInterface = route.egressInterface;
    CHECK_FALSE(table.add(duplicate).isOk());
    CHECK(table.size() == 1);
}

TEST_CASE("Routes are removable by identifier, destination, source and interface", "[routing]") {
    RoutingTable table;

    const Route connectedRoute = makeRoute("192.168.1.0/24", "0.0.0.0", RouteSource::Connected);
    const Route staticRoute = makeRoute("10.0.0.0/8", "10.1.0.1");
    table.addUnchecked(connectedRoute);
    table.addUnchecked(staticRoute);
    REQUIRE(table.size() == 2);

    CHECK(table.remove(staticRoute.id));
    CHECK(table.size() == 1);
    CHECK_FALSE(table.remove(staticRoute.id));

    CHECK(table.removeBySource(RouteSource::Connected) == 1);
    CHECK(table.empty());

    const Route byInterface = makeRoute("172.16.0.0/16", "172.16.0.1");
    table.addUnchecked(byInterface);
    CHECK(table.removeByInterface(byInterface.egressInterface) == 1);
    CHECK(table.empty());
}

TEST_CASE("All matching routes are reported best first", "[routing]") {
    RoutingTable table;

    REQUIRE(table.add(makeRoute("0.0.0.0/0", "192.168.1.1")).isOk());
    REQUIRE(table.add(makeRoute("10.0.0.0/8", "10.1.0.1")).isOk());
    REQUIRE(table.add(makeRoute("10.20.0.0/16", "10.20.0.1")).isOk());

    const auto matches = table.matches(ipv4("10.20.5.5"));
    REQUIRE(matches.size() == 3);
    CHECK(matches[0]->destination.prefixLength() == 16);
    CHECK(matches[1]->destination.prefixLength() == 8);
    CHECK(matches[2]->destination.prefixLength() == 0);
}

TEST_CASE("Route descriptions read like a routing table", "[routing]") {
    Route connectedRoute;
    connectedRoute.destination = prefix("192.168.1.0/24");
    CHECK(connectedRoute.toString() == "192.168.1.0/24 is directly connected");
    CHECK(connectedRoute.isDirectlyConnected());

    CHECK(makeRoute("10.0.0.0/8", "192.168.1.1").toString() == "10.0.0.0/8 via 192.168.1.1");
    CHECK(routeSourceDistance(RouteSource::Connected) < routeSourceDistance(RouteSource::Static));
    CHECK(routeSourceDistance(RouteSource::Static) < routeSourceDistance(RouteSource::Ospf));
}
