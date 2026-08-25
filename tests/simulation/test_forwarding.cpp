#include "TestHelpers.h"

#include "core/devices/FirewallPolicy.h"
#include "core/devices/MacAddressTable.h"
#include "simulation/Simulator.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::tests;

namespace {

bool sawEvent(const sim::Simulator& simulator, TraceKind kind) {
    return std::any_of(simulator.traceLog().begin(), simulator.traceLog().end(),
                       [kind](const TraceEvent& event) { return event.kind == kind; });
}

const TraceEvent* findEvent(const sim::Simulator& simulator, TraceKind kind) {
    const auto it = std::find_if(simulator.traceLog().begin(), simulator.traceLog().end(),
                                 [kind](const TraceEvent& event) { return event.kind == kind; });
    return it == simulator.traceLog().end() ? nullptr : &*it;
}

/// PC1 -- Switch1 -- Router1 == Router2 -- Server1, fully addressed and routed.
struct RoutedTopology {
    Project project;
    Device* pc = nullptr;
    Device* sw = nullptr;
    Device* router1 = nullptr;
    Device* router2 = nullptr;
    Device* server = nullptr;

    RoutedTopology() {
        Network& network = project.network();

        pc = &addDevice(network, DeviceType::Pc, "PC1");
        sw = &addDevice(network, DeviceType::Switch, "Switch1");
        router1 = &addDevice(network, DeviceType::Router, "Router1");
        router2 = &addDevice(network, DeviceType::Router, "Router2");
        server = &addDevice(network, DeviceType::Server, "Server1");

        connect(network, *pc, "Gi0", *sw, "Gi0/1");
        connect(network, *sw, "Gi0/2", *router1, "Gi0/0");
        connect(network, *router1, "Gi0/1", *router2, "Gi0/0");
        connect(network, *router2, "Gi0/1", *server, "Gi0");

        assign(*pc, "Gi0", "192.168.1.10/24");
        assign(*router1, "Gi0/0", "192.168.1.1/24");
        assign(*router1, "Gi0/1", "10.0.0.1/30");
        assign(*router2, "Gi0/0", "10.0.0.2/30");
        assign(*router2, "Gi0/1", "172.16.0.1/24");
        assign(*server, "Gi0", "172.16.0.20/24");

        network.refreshOperationalStates();

        pc->ipv4Stack()->setDefaultGateway(ipv4("192.168.1.1"));
        server->ipv4Stack()->setDefaultGateway(ipv4("172.16.0.1"));
        addRoute(*router1, "172.16.0.0/24", "10.0.0.2");
        addRoute(*router2, "192.168.1.0/24", "10.0.0.1");
    }
};

} // namespace

TEST_CASE("A packet crosses two routers and comes back", "[simulation][forwarding]") {
    RoutedTopology topology;
    sim::Simulator simulator{topology.project.network(), topology.project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("172.16.0.20");
    request.count = 2;
    request.interval = milliseconds(100);

    const auto ping = simulator.ping(topology.pc->id(), request);
    REQUIRE(ping.isOk());

    simulator.runUntilIdle(seconds(10));

    const PingStatistics* statistics = topology.pc->ipv4Stack()->pingStatistics(ping.value());
    REQUIRE(statistics != nullptr);
    CHECK(statistics->sent == 2);
    CHECK(statistics->received == 2);
    CHECK(statistics->lost == 0);
    CHECK(statistics->averageRtt() > Duration::zero());
    CHECK(statistics->finished);

    CHECK(sawEvent(simulator, TraceKind::IpPacketForwarded));
    CHECK(sawEvent(simulator, TraceKind::RouteLookup));
    CHECK(sawEvent(simulator, TraceKind::IcmpEchoRequestReceived));
    CHECK(sawEvent(simulator, TraceKind::IcmpEchoReplySent));
    CHECK(simulator.statistics().framesDropped == 0);
}

TEST_CASE("Routers decrement the TTL", "[simulation][forwarding]") {
    RoutedTopology topology;
    sim::Simulator simulator{topology.project.network(), topology.project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("172.16.0.20");
    request.count = 1;
    REQUIRE(simulator.ping(topology.pc->id(), request).isOk());
    simulator.runUntilIdle(seconds(10));

    const TraceEvent* reply = findEvent(simulator, TraceKind::PingReplyReceived);
    REQUIRE(reply != nullptr);

    // The reply started at 64 and crossed two routers.
    CHECK(reply->field("ttl") == "62");
}

TEST_CASE("A TTL that reaches zero produces an ICMP time exceeded", "[simulation][forwarding]") {
    RoutedTopology topology;
    sim::Simulator simulator{topology.project.network(), topology.project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("172.16.0.20");
    request.count = 1;
    request.ttl = 1; // dies at the first router, exactly as traceroute intends

    REQUIRE(simulator.ping(topology.pc->id(), request).isOk());
    simulator.runUntilIdle(seconds(10));

    CHECK(sawEvent(simulator, TraceKind::IpTtlExpired));
    CHECK(sawEvent(simulator, TraceKind::IcmpTimeExceededSent));
    CHECK(sawEvent(simulator, TraceKind::IcmpTimeExceededReceived));

    const TraceEvent* expired = findEvent(simulator, TraceKind::IpTtlExpired);
    REQUIRE(expired != nullptr);
    CHECK(expired->device == topology.router1->id());
}

TEST_CASE("An unroutable destination is rejected before any packet is built",
          "[simulation][forwarding]") {
    RoutedTopology topology;
    sim::Simulator simulator{topology.project.network(), topology.project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("203.0.113.5");
    request.count = 1;

    // PC1 has a default route, so the ping starts; Router1 is where it dies.
    REQUIRE(simulator.ping(topology.pc->id(), request).isOk());
    simulator.runUntilIdle(seconds(10));

    CHECK(sawEvent(simulator, TraceKind::IpNoRouteToHost));
    CHECK(sawEvent(simulator, TraceKind::IcmpDestinationUnreachableSent));
    CHECK(sawEvent(simulator, TraceKind::PingTimedOut));
}

TEST_CASE("A host with no route refuses the ping immediately", "[simulation][forwarding]") {
    Project project;
    Network& network = project.network();

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");
    connect(network, pc, "Gi0", sw, "Gi0/1");
    assign(pc, "Gi0", "192.168.1.10/24");
    network.refreshOperationalStates();

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("8.8.8.8");

    const auto ping = simulator.ping(pc.id(), request);
    CHECK_FALSE(ping.isOk());
    CHECK(ping.message().find("no route") != std::string::npos);
}

TEST_CASE("A device answers a ping to its own address", "[simulation][forwarding]") {
    RoutedTopology topology;
    sim::Simulator simulator{topology.project.network(), topology.project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("192.168.1.10"); // PC1's own address
    request.count = 1;

    const auto ping = simulator.ping(topology.pc->id(), request);
    REQUIRE(ping.isOk());
    simulator.runUntilIdle(seconds(5));

    const PingStatistics* statistics = topology.pc->ipv4Stack()->pingStatistics(ping.value());
    REQUIRE(statistics != nullptr);
    CHECK(statistics->received == 1);
    CHECK(simulator.statistics().framesTransmitted == 0); // loopback never touches a wire
}

TEST_CASE("A broken cable makes the far side unreachable", "[simulation][forwarding]") {
    RoutedTopology topology;
    Network& network = topology.project.network();

    const std::vector<LinkId> links = network.linksOf(topology.router1->id());
    REQUIRE_FALSE(links.empty());

    // Disable the router-to-router link, as a cable fault would.
    for (const LinkId id : links) {
        Link* link = network.findLink(id);
        if (link->involves(topology.router2->id())) link->setEnabled(false);
    }
    network.refreshOperationalStates();

    sim::Simulator simulator{network, topology.project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("172.16.0.20");
    request.count = 1;
    REQUIRE(simulator.ping(topology.pc->id(), request).isOk());
    simulator.runUntilIdle(seconds(10));

    const PingStatistics* statistics =
        topology.pc->ipv4Stack()->pingStatistics(static_cast<PingId>(1));
    REQUIRE(statistics != nullptr);
    CHECK(statistics->received == 0);
}

TEST_CASE("A firewall denies what its policy says to deny", "[simulation][firewall]") {
    Project project;
    Network& network = project.network();

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& firewall = addDevice(network, DeviceType::Firewall, "FW1");
    Device& server = addDevice(network, DeviceType::Server, "Server1");

    connect(network, pc, "Gi0", firewall, "Gi0/0");
    connect(network, firewall, "Gi0/1", server, "Gi0");
    assign(pc, "Gi0", "192.168.1.10/24");
    assign(firewall, "Gi0/0", "192.168.1.1/24");
    assign(firewall, "Gi0/1", "172.16.0.1/24");
    assign(server, "Gi0", "172.16.0.20/24");
    network.refreshOperationalStates();

    pc.ipv4Stack()->setDefaultGateway(ipv4("192.168.1.1"));
    server.ipv4Stack()->setDefaultGateway(ipv4("172.16.0.1"));

    FirewallPolicy* policy = firewall.firewallPolicy();
    REQUIRE(policy != nullptr);

    FirewallRule denyIcmp;
    denyIcmp.name = "block ping into the DMZ";
    denyIcmp.action = FirewallAction::Deny;
    denyIcmp.protocol = FirewallProtocolMatch::Icmp;
    denyIcmp.destination = prefix("172.16.0.0/24");
    policy->addRule(denyIcmp);

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("172.16.0.20");
    request.count = 1;
    const auto ping = simulator.ping(pc.id(), request);
    REQUIRE(ping.isOk());
    simulator.runUntilIdle(seconds(10));

    CHECK(sawEvent(simulator, TraceKind::FirewallDenied));
    CHECK(policy->rules().front().hitCount > 0);

    const PingStatistics* statistics = pc.ipv4Stack()->pingStatistics(ping.value());
    REQUIRE(statistics != nullptr);
    CHECK(statistics->received == 0);

    SECTION("removing the rule restores connectivity") {
        policy->clear();
        sim::Simulator second{network, project.simulationSettings()};
        second.start();

        const auto retry = second.ping(pc.id(), request);
        REQUIRE(retry.isOk());
        second.runUntilIdle(seconds(10));

        CHECK(pc.ipv4Stack()->pingStatistics(retry.value())->received == 1);
    }
}

TEST_CASE("The simulation is deterministic", "[simulation]") {
    const auto runOnce = [] {
        RoutedTopology topology;
        sim::Simulator simulator{topology.project.network(), topology.project.simulationSettings()};
        simulator.start();

        PingRequest request;
        request.destination = ipv4("172.16.0.20");
        request.count = 3;
        request.interval = milliseconds(50);
        REQUIRE(simulator.ping(topology.pc->id(), request).isOk());
        simulator.runUntilIdle(seconds(10));

        std::vector<std::pair<i64, TraceKind>> timeline;
        for (const TraceEvent& event : simulator.traceLog()) {
            timeline.emplace_back(event.time.time_since_epoch().count(), event.kind);
        }
        return timeline;
    };

    // Two runs of the same project must produce the same events at the same
    // instants; MAC addresses differ between runs but timing must not.
    CHECK(runOnce() == runOnce());
}

TEST_CASE("Stepping processes exactly one event", "[simulation]") {
    RoutedTopology topology;
    sim::Simulator simulator{topology.project.network(), topology.project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("172.16.0.20");
    request.count = 1;
    REQUIRE(simulator.ping(topology.pc->id(), request).isOk());

    simulator.pause();
    const u64 before = simulator.statistics().eventsProcessed;

    CHECK(simulator.step());
    CHECK(simulator.statistics().eventsProcessed == before + 1);

    CHECK(simulator.step());
    CHECK(simulator.statistics().eventsProcessed == before + 2);
}

TEST_CASE("Reset clears runtime state but keeps configuration", "[simulation]") {
    RoutedTopology topology;
    sim::Simulator simulator{topology.project.network(), topology.project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("172.16.0.20");
    request.count = 1;
    REQUIRE(simulator.ping(topology.pc->id(), request).isOk());
    simulator.runUntilIdle(seconds(10));

    REQUIRE_FALSE(topology.pc->arpCache()->empty());
    REQUIRE_FALSE(simulator.traceLog().empty());

    simulator.reset();

    CHECK(simulator.state() == sim::SimulationState::Stopped);
    CHECK(simulator.now() == simTimeZero());
    CHECK(simulator.traceLog().empty());
    CHECK(topology.pc->arpCache()->empty());
    CHECK(topology.sw->macTable()->empty());

    // Configuration survives.
    CHECK(topology.pc->ipv4Stack()->defaultGateway() == ipv4("192.168.1.1"));
    CHECK(iface(*topology.pc, "Gi0").ipv4Addresses().size() == 1);
}
