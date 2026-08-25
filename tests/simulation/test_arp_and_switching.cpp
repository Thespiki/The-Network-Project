#include "TestHelpers.h"

#include "core/devices/SwitchingEngine.h"
#include "simulation/Simulator.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::tests;

namespace {

/// True when the trace contains an event of `kind` at `device`.
bool sawEvent(const sim::Simulator& simulator, TraceKind kind, DeviceId device = DeviceId{}) {
    return std::any_of(simulator.traceLog().begin(), simulator.traceLog().end(),
                       [&](const TraceEvent& event) {
                           if (event.kind != kind) return false;
                           return !device.isValid() || event.device == device;
                       });
}

std::size_t countEvents(const sim::Simulator& simulator, TraceKind kind) {
    return static_cast<std::size_t>(
        std::count_if(simulator.traceLog().begin(), simulator.traceLog().end(),
                      [kind](const TraceEvent& event) { return event.kind == kind; }));
}

} // namespace

TEST_CASE("ARP resolves before the first packet is sent", "[simulation][arp]") {
    Project project;
    Network& network = project.network();

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& router = addDevice(network, DeviceType::Router, "Router1");

    connect(network, pc, "Gi0", router, "Gi0/0");
    assign(pc, "Gi0", "192.168.1.10/24");
    assign(router, "Gi0/0", "192.168.1.1/24");
    network.refreshOperationalStates();

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("192.168.1.1");
    request.count = 1;
    REQUIRE(simulator.ping(pc.id(), request).isOk());

    simulator.runUntilIdle(seconds(5));

    // The full exchange: cache miss, broadcast request, reply, resolution.
    CHECK(sawEvent(simulator, TraceKind::ArpCacheMiss, pc.id()));
    CHECK(sawEvent(simulator, TraceKind::ArpRequestSent, pc.id()));
    CHECK(sawEvent(simulator, TraceKind::ArpRequestReceived, router.id()));
    CHECK(sawEvent(simulator, TraceKind::ArpReplySent, router.id()));
    CHECK(sawEvent(simulator, TraceKind::ArpReplyReceived, pc.id()));
    CHECK(sawEvent(simulator, TraceKind::ArpResolved, pc.id()));
    CHECK(sawEvent(simulator, TraceKind::PingReplyReceived, pc.id()));

    const ArpCache* cache = pc.arpCache();
    REQUIRE(cache != nullptr);
    const ArpEntry* entry = cache->find(ipv4("192.168.1.1"), simulator.now());
    REQUIRE(entry != nullptr);
    CHECK(entry->mac == iface(router, "Gi0/0").macAddress());
}

TEST_CASE("A warm ARP cache skips resolution entirely", "[simulation][arp]") {
    Project project;
    Network& network = project.network();

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& router = addDevice(network, DeviceType::Router, "Router1");

    connect(network, pc, "Gi0", router, "Gi0/0");
    assign(pc, "Gi0", "192.168.1.10/24");
    assign(router, "Gi0/0", "192.168.1.1/24");
    network.refreshOperationalStates();

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("192.168.1.1");
    request.count = 3;
    request.interval = milliseconds(50);
    REQUIRE(simulator.ping(pc.id(), request).isOk());
    simulator.runUntilIdle(seconds(5));

    // One resolution for three probes; the rest hit the cache.
    CHECK(countEvents(simulator, TraceKind::ArpRequestSent) == 1);
    CHECK(countEvents(simulator, TraceKind::ArpCacheHit) >= 2);
    CHECK(countEvents(simulator, TraceKind::PingReplyReceived) == 3);
}

TEST_CASE("ARP for an address nobody owns times out", "[simulation][arp]") {
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
    request.destination = ipv4("192.168.1.99"); // on-link but nonexistent
    request.count = 1;
    REQUIRE(simulator.ping(pc.id(), request).isOk());

    simulator.runUntilIdle(seconds(30));

    CHECK(sawEvent(simulator, TraceKind::ArpTimedOut, pc.id()));
    CHECK(sawEvent(simulator, TraceKind::PingTimedOut, pc.id()));
    CHECK(countEvents(simulator, TraceKind::ArpRequestSent) == kMaxArpAttempts);
}

TEST_CASE("A switch learns source addresses and stops flooding", "[simulation][switching]") {
    Project project;
    Network& network = project.network();

    Device& pcA = addDevice(network, DeviceType::Pc, "PC1");
    Device& pcB = addDevice(network, DeviceType::Pc, "PC2");
    Device& pcC = addDevice(network, DeviceType::Pc, "PC3");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");

    connect(network, pcA, "Gi0", sw, "Gi0/1");
    connect(network, pcB, "Gi0", sw, "Gi0/2");
    connect(network, pcC, "Gi0", sw, "Gi0/3");
    assign(pcA, "Gi0", "192.168.1.10/24");
    assign(pcB, "Gi0", "192.168.1.20/24");
    assign(pcC, "Gi0", "192.168.1.30/24");
    network.refreshOperationalStates();

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("192.168.1.20");
    request.count = 2;
    request.interval = milliseconds(50);
    REQUIRE(simulator.ping(pcA.id(), request).isOk());
    simulator.runUntilIdle(seconds(5));

    const MacAddressTable* table = sw.macTable();
    REQUIRE(table != nullptr);

    const MacTableEntry* entryA = table->lookup(kDefaultVlan, iface(pcA, "Gi0").macAddress(),
                                                simulator.now());
    const MacTableEntry* entryB = table->lookup(kDefaultVlan, iface(pcB, "Gi0").macAddress(),
                                                simulator.now());
    REQUIRE(entryA != nullptr);
    REQUIRE(entryB != nullptr);
    CHECK(entryA->port == iface(sw, "Gi0/1").id());
    CHECK(entryB->port == iface(sw, "Gi0/2").id());

    // PC3 was never a destination, so it only ever saw the initial ARP broadcast.
    CHECK(iface(pcC, "Gi0").counters().framesReceived == 1);
    CHECK(sawEvent(simulator, TraceKind::MacLearned, sw.id()));
    CHECK(sawEvent(simulator, TraceKind::FrameSwitched, sw.id()));
}

TEST_CASE("A hub repeats to every other port", "[simulation][hub]") {
    Project project;
    Network& network = project.network();

    Device& pcA = addDevice(network, DeviceType::Pc, "PC1");
    Device& pcB = addDevice(network, DeviceType::Pc, "PC2");
    Device& pcC = addDevice(network, DeviceType::Pc, "PC3");
    Device& hub = addDevice(network, DeviceType::Hub, "Hub1");

    connect(network, pcA, "Gi0", hub, "Et0/1");
    connect(network, pcB, "Gi0", hub, "Et0/2");
    connect(network, pcC, "Gi0", hub, "Et0/3");
    assign(pcA, "Gi0", "192.168.1.10/24");
    assign(pcB, "Gi0", "192.168.1.20/24");
    assign(pcC, "Gi0", "192.168.1.30/24");
    network.refreshOperationalStates();

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("192.168.1.20");
    request.count = 1;
    REQUIRE(simulator.ping(pcA.id(), request).isOk());
    simulator.runUntilIdle(seconds(5));

    // A hub never learns, so PC3 sees every frame of the exchange - which is the
    // difference a switch exists to make.
    CHECK(iface(pcC, "Gi0").counters().framesReceived > 1);
    CHECK(sawEvent(simulator, TraceKind::PingReplyReceived, pcA.id()));
}

TEST_CASE("VLANs keep traffic apart on one switch", "[simulation][vlan]") {
    Project project;
    Network& network = project.network();

    Device& pcA = addDevice(network, DeviceType::Pc, "PC1");
    Device& pcB = addDevice(network, DeviceType::Pc, "PC2");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");

    connect(network, pcA, "Gi0", sw, "Gi0/1");
    connect(network, pcB, "Gi0", sw, "Gi0/2");
    assign(pcA, "Gi0", "192.168.1.10/24");
    assign(pcB, "Gi0", "192.168.1.20/24");

    // Same subnet, different VLANs: the switch must not bridge between them.
    sw.switching()->addVlan(VlanDefinition{10, "users"});
    sw.switching()->addVlan(VlanDefinition{20, "servers"});
    iface(sw, "Gi0/1").vlan().accessVlan = 10;
    iface(sw, "Gi0/2").vlan().accessVlan = 20;
    network.refreshOperationalStates();

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();

    PingRequest request;
    request.destination = ipv4("192.168.1.20");
    request.count = 1;
    REQUIRE(simulator.ping(pcA.id(), request).isOk());
    simulator.runUntilIdle(seconds(15));

    CHECK(iface(pcB, "Gi0").counters().framesReceived == 0);
    CHECK_FALSE(sawEvent(simulator, TraceKind::PingReplyReceived, pcA.id()));
    CHECK(sawEvent(simulator, TraceKind::ArpTimedOut, pcA.id()));
}
