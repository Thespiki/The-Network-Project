#include "TestHelpers.h"

#include "app/SampleProject.h"
#include "core/devices/DhcpServer.h"
#include "core/devices/DnsServer.h"
#include "core/devices/FirewallPolicy.h"
#include "core/devices/SwitchingEngine.h"
#include "serialization/ProjectSerializer.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::tests;

namespace {

/// Serializes, deserializes and returns the reconstructed project.
void roundTrip(const Project& source, Project& destination) {
    const serial::ProjectSerializer serializer;

    auto document = serializer.write(source, true);
    REQUIRE(document.isOk());

    auto report = serializer.read(document.value(), destination);
    REQUIRE(report.isOk());
    INFO((report.value().warnings.empty() ? std::string{} : report.value().warnings.front()));
    CHECK(report.value().warnings.empty());
}

/// Compares the parts of two projects that must survive a save and a load.
void expectEquivalent(const Project& a, const Project& b) {
    CHECK(a.metadata().id == b.metadata().id);
    CHECK(a.metadata().name == b.metadata().name);
    CHECK(a.metadata().description == b.metadata().description);
    CHECK(a.metadata().tags == b.metadata().tags);

    REQUIRE(a.network().deviceCount() == b.network().deviceCount());
    REQUIRE(a.network().linkCount() == b.network().linkCount());
    REQUIRE(a.annotations().size() == b.annotations().size());
    REQUIRE(a.tests().size() == b.tests().size());

    for (const auto& original : a.network().devices()) {
        const Device* copy = b.network().findDevice(original->id());
        REQUIRE(copy != nullptr);

        CHECK(copy->name() == original->name());
        CHECK(copy->type() == original->type());
        CHECK(copy->description() == original->description());
        REQUIRE(copy->interfaceCount() == original->interfaceCount());

        for (const auto& originalInterface : original->interfaces()) {
            const Interface* copiedInterface = copy->findInterface(originalInterface->id());
            REQUIRE(copiedInterface != nullptr);

            CHECK(copiedInterface->name() == originalInterface->name());
            CHECK(copiedInterface->type() == originalInterface->type());
            CHECK(copiedInterface->macAddress() == originalInterface->macAddress());
            CHECK(copiedInterface->mtu() == originalInterface->mtu());
            CHECK(copiedInterface->adminState() == originalInterface->adminState());
            CHECK(copiedInterface->ipv4Addresses() == originalInterface->ipv4Addresses());
            CHECK(copiedInterface->ipv6Addresses() == originalInterface->ipv6Addresses());
            CHECK(copiedInterface->vlan() == originalInterface->vlan());
            CHECK(copiedInterface->description() == originalInterface->description());
        }

        if (const Ipv4Stack* originalStack = original->ipv4Stack()) {
            const Ipv4Stack* copiedStack = copy->ipv4Stack();
            REQUIRE(copiedStack != nullptr);
            CHECK(copiedStack->forwardingEnabled() == originalStack->forwardingEnabled());
            CHECK(copiedStack->defaultGateway() == originalStack->defaultGateway());
            CHECK(copiedStack->dnsServers() == originalStack->dnsServers());
            REQUIRE(copiedStack->staticRoutes().size() == originalStack->staticRoutes().size());

            for (std::size_t i = 0; i < originalStack->staticRoutes().size(); ++i) {
                const StaticRouteEntry& from = originalStack->staticRoutes()[i];
                const StaticRouteEntry& to = copiedStack->staticRoutes()[i];
                CHECK(to.id == from.id);
                CHECK(to.destination.network() == from.destination.network());
                CHECK(to.nextHop == from.nextHop);
                CHECK(to.metric == from.metric);
            }
        }
    }

    for (const auto& link : a.network().links()) {
        const Link* copy = b.network().findLink(link->id());
        REQUIRE(copy != nullptr);
        CHECK(copy->endpointA() == link->endpointA());
        CHECK(copy->endpointB() == link->endpointB());
        CHECK(copy->medium() == link->medium());
        CHECK(copy->propagationDelay() == link->propagationDelay());
        CHECK(copy->bandwidthMbps() == link->bandwidthMbps());
        CHECK(copy->isEnabled() == link->isEnabled());
        CHECK(copy->label() == link->label());
    }

    for (const auto& [device, placement] : a.layout().placements()) {
        CHECK(b.layout().position(device) == placement.position);
        CHECK(b.layout().isLocked(device) == placement.locked);
    }
    CHECK(b.layout().viewZoom == a.layout().viewZoom);
    CHECK(b.layout().gridSize == a.layout().gridSize);
}

} // namespace

TEST_CASE("The sample project survives a round trip", "[serialization][roundtrip]") {
    Project original;
    app::buildSampleProject(original);

    Project restored;
    roundTrip(original, restored);
    expectEquivalent(original, restored);

    // Derived state is rebuilt, not stored, so it must be correct after loading.
    const Device* router = restored.network().findDeviceByName("Router1");
    REQUIRE(router != nullptr);
    CHECK(router->routingTable()->lookup(ipv4("172.16.0.20")) != nullptr);
}

TEST_CASE("Every configurable field survives a round trip", "[serialization][roundtrip]") {
    Project original;
    Network& network = original.network();

    Device& router = addDevice(network, DeviceType::Router, "Edge");
    Device& sw = addDevice(network, DeviceType::Switch, "Access");
    Device& firewall = addDevice(network, DeviceType::Firewall, "Perimeter");
    Device& server = addDevice(network, DeviceType::Server, "Services");

    connect(network, router, "Gi0/0", sw, "Gi0/1");
    connect(network, sw, "Gi0/2", firewall, "Gi0/0");
    connect(network, firewall, "Gi0/1", server, "Gi0");

    assign(router, "Gi0/0", "10.1.0.1/24");
    assign(firewall, "Gi0/0", "10.1.0.2/24");
    assign(firewall, "Gi0/1", "10.2.0.1/24");
    assign(server, "Gi0", "10.2.0.10/24");
    network.refreshOperationalStates();

    original.metadata().name = "Full coverage";
    original.metadata().description = "Every field the schema knows about.";
    original.metadata().author = "Test";
    original.metadata().tags = {"alpha", "beta"};

    // Interface detail
    Interface& port = iface(router, "Gi0/0");
    port.setDescription("uplink to the access switch");
    port.setDisplayName("Uplink");
    REQUIRE(port.setMtu(9000).isOk());
    port.setDuplex(DuplexMode::Full);
    port.setSpeedMbps(10000);
    REQUIRE(port.addIpv6Address(*Ipv6Prefix::parse("2001:db8::1/64")).isOk());

    // VLANs
    sw.switching()->addVlan(VlanDefinition{10, "users"});
    sw.switching()->addVlan(VlanDefinition{20, "voice"});
    sw.switching()->setAgeingTime(seconds(600));
    Interface& trunk = iface(sw, "Gi0/2");
    trunk.vlan().mode = VlanMode::Trunk;
    trunk.vlan().nativeVlan = 1;
    trunk.vlan().allowedVlans = {1, 10, 20};

    // Routing
    addRoute(router, "10.2.0.0/24", "10.1.0.2");
    router.ipv4Stack()->setDnsServers({ipv4("10.2.0.10")});
    router.ipv4Stack()->setDomainName("example.local");
    server.ipv4Stack()->setDefaultGateway(ipv4("10.2.0.1"));

    // OSPF configuration (stored but not simulated)
    OspfConfiguration& ospf = router.ipv4Stack()->ospf();
    ospf.enabled = true;
    ospf.processId = 42;
    ospf.routerId = ipv4("1.1.1.1");
    ospf.networks.push_back(OspfNetworkStatement{prefix("10.1.0.0/24"), 0});

    // Firewall
    FirewallRule rule;
    rule.name = "allow web";
    rule.action = FirewallAction::Permit;
    rule.protocol = FirewallProtocolMatch::Tcp;
    rule.source = prefix("10.1.0.0/24");
    rule.destination = prefix("10.2.0.0/24");
    rule.destinationPortFirst = 80;
    rule.destinationPortLast = 443;
    firewall.firewallPolicy()->addRule(rule);
    firewall.firewallPolicy()->setDefaultAction(FirewallAction::Deny);

    // Services
    DhcpPool pool;
    pool.name = "users";
    pool.subnet = prefix("10.2.0.0/24");
    pool.rangeFirst = ipv4("10.2.0.100");
    pool.rangeLast = ipv4("10.2.0.200");
    pool.gateway = ipv4("10.2.0.1");
    pool.dnsServer = ipv4("10.2.0.10");
    pool.exclusions = {ipv4("10.2.0.150")};
    server.dhcpServer()->setEnabled(true);
    server.dhcpServer()->addPool(pool);

    server.dnsServer()->setEnabled(true);
    server.dnsServer()->addRecord(DnsRecord{DnsRecordId::generate(), "www.example.local",
                                            ipv4("10.2.0.10"), 600});

    // Layout, annotations and tests
    original.layout().setPosition(router.id(), Vec2{-100.0f, 50.0f});
    original.layout().setLocked(router.id(), true);
    original.layout().viewZoom = 1.75f;
    original.layout().gridSize = 32.0f;
    original.layout().snapToGrid = true;

    Annotation note;
    note.kind = AnnotationKind::Rectangle;
    note.start = Vec2{-200.0f, -100.0f};
    note.end = Vec2{200.0f, 100.0f};
    note.text = "Core";
    note.thickness = 3.0f;
    note.filled = true;
    note.zOrder = 5;
    original.addAnnotation(note);

    NetworkTest test;
    test.name = "edge to services";
    test.source = router.id();
    test.destinationDevice = server.id();
    test.probeCount = 7;
    test.timeout = seconds(5);
    test.payloadSize = 128;
    original.addTest(test);

    original.simulationSettings().speedMultiplier = 0.25;
    original.simulationSettings().traceHistoryLimit = 12345;

    Project restored;
    roundTrip(original, restored);
    expectEquivalent(original, restored);

    // Spot-check the parts the generic comparison does not cover.
    const Device* restoredSwitch = restored.network().findDeviceByName("Access");
    REQUIRE(restoredSwitch != nullptr);
    CHECK(restoredSwitch->switching()->vlans().size() == 3); // VLAN 1 plus the two added
    CHECK(restoredSwitch->switching()->ageingTime() == seconds(600));

    const Device* restoredFirewall = restored.network().findDeviceByName("Perimeter");
    REQUIRE(restoredFirewall != nullptr);
    const FirewallPolicy* policy = restoredFirewall->firewallPolicy();
    REQUIRE(policy->rules().size() == 1);
    CHECK(policy->defaultAction() == FirewallAction::Deny);
    CHECK(policy->rules().front().name == "allow web");
    CHECK(policy->rules().front().destinationPortFirst == 80);
    CHECK(policy->rules().front().destinationPortLast == 443);

    const Device* restoredServer = restored.network().findDeviceByName("Services");
    REQUIRE(restoredServer != nullptr);
    REQUIRE(restoredServer->dhcpServer()->pools().size() == 1);
    CHECK(restoredServer->dhcpServer()->isEnabled());
    CHECK(restoredServer->dhcpServer()->pools().front().exclusions.size() == 1);
    CHECK(restoredServer->dnsServer()->resolve("www.example.local") == ipv4("10.2.0.10"));

    const Device* restoredRouter = restored.network().findDeviceByName("Edge");
    CHECK(restoredRouter->ipv4Stack()->ospf().enabled);
    CHECK(restoredRouter->ipv4Stack()->ospf().processId == 42);
    CHECK(restoredRouter->ipv4Stack()->ospf().networks.size() == 1);
    CHECK(restoredRouter->ipv4Stack()->domainName() == "example.local");

    CHECK(restored.tests().front().probeCount == 7);
    CHECK(restored.tests().front().timeout == seconds(5));
    CHECK(restored.annotations().front().filled);
    CHECK(restored.annotations().front().zOrder == 5);
    CHECK(restored.simulationSettings().speedMultiplier == 0.25);
    CHECK(restored.simulationSettings().traceHistoryLimit == 12345);
}

TEST_CASE("A round trip is stable: writing twice gives the same bytes",
          "[serialization][roundtrip]") {
    Project original;
    app::buildSampleProject(original);

    const serial::ProjectSerializer serializer;
    auto first = serializer.write(original, true);
    REQUIRE(first.isOk());

    Project restored;
    REQUIRE(serializer.read(first.value(), restored).isOk());

    auto second = serializer.write(restored, true);
    REQUIRE(second.isOk());

    CHECK(first.value() == second.value());
}

TEST_CASE("A DHCP interface stores no address", "[serialization][roundtrip]") {
    Project original;
    Device& pc = addDevice(original.network(), DeviceType::Pc, "PC1");

    Interface& port = iface(pc, "Gi0");
    port.setIpv4DhcpEnabled(true);
    // Simulate a lease having been installed during a run.
    REQUIRE(port.addIpv4Address(prefix("192.168.1.50/24")).isOk());

    Project restored;
    roundTrip(original, restored);

    const Device* copy = restored.network().findDeviceByName("PC1");
    REQUIRE(copy != nullptr);
    const Interface* copiedPort = copy->findInterfaceByName("Gi0");
    REQUIRE(copiedPort != nullptr);

    CHECK(copiedPort->ipv4DhcpEnabled());
    CHECK(copiedPort->ipv4Addresses().empty()); // a lease is not configuration
}

TEST_CASE("An empty project round-trips", "[serialization][roundtrip]") {
    Project original;
    original.metadata().name = "Nothing here";

    Project restored;
    roundTrip(original, restored);

    CHECK(restored.network().deviceCount() == 0);
    CHECK(restored.metadata().name == "Nothing here");
}
