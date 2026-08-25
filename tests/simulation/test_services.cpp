// DHCP and DNS over the real wire.
//
// Both are claimed to work in the README, so both are exercised end to end here
// rather than only through their in-memory interfaces.

#include "TestHelpers.h"

#include "core/devices/DhcpClient.h"
#include "core/devices/DhcpServer.h"
#include "core/devices/DnsServer.h"
#include "core/protocols/Dns.h"
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

} // namespace

TEST_CASE("A client obtains an address over DHCP", "[simulation][dhcp]") {
    Project project;
    Network& network = project.network();

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");
    Device& server = addDevice(network, DeviceType::Server, "Server1");

    connect(network, pc, "Gi0", sw, "Gi0/1");
    connect(network, sw, "Gi0/2", server, "Gi0");
    assign(server, "Gi0", "192.168.1.1/24");

    // The client has no configured address; it must ask for one.
    iface(pc, "Gi0").setIpv4DhcpEnabled(true);
    network.refreshOperationalStates();

    DhcpPool pool;
    pool.name = "users";
    pool.subnet = prefix("192.168.1.0/24");
    pool.rangeFirst = ipv4("192.168.1.100");
    pool.rangeLast = ipv4("192.168.1.120");
    pool.gateway = ipv4("192.168.1.1");
    pool.dnsServer = ipv4("192.168.1.1");

    REQUIRE(server.dhcpServer() != nullptr);
    server.dhcpServer()->setEnabled(true);
    server.dhcpServer()->addPool(pool);

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();
    simulator.runUntilIdle(seconds(30));

    // The full four-message exchange, on the wire.
    CHECK(sawEvent(simulator, TraceKind::DhcpDiscoverSent));
    CHECK(sawEvent(simulator, TraceKind::DhcpOfferSent));
    CHECK(sawEvent(simulator, TraceKind::DhcpRequestSent));
    CHECK(sawEvent(simulator, TraceKind::DhcpAckSent));
    CHECK(sawEvent(simulator, TraceKind::DhcpLeaseAssigned));

    // The client really configured itself.
    const auto address = iface(pc, "Gi0").primaryIpv4();
    REQUIRE(address.has_value());
    CHECK(pool.subnet.contains(address->address()));
    CHECK(address->prefixLength() == 24);
    CHECK(pc.ipv4Stack()->defaultGateway() == ipv4("192.168.1.1"));
    CHECK(pc.ipv4Stack()->dnsServers() == std::vector<Ipv4Address>{ipv4("192.168.1.1")});

    // And the server recorded the lease.
    const auto leases = server.dhcpServer()->leases();
    REQUIRE(leases.size() == 1);
    CHECK(leases.front().address == address->address());
    CHECK(leases.front().client == iface(pc, "Gi0").macAddress());

    SECTION("the leased address is usable") {
        PingRequest request;
        request.destination = ipv4("192.168.1.1");
        request.count = 1;

        const auto ping = simulator.ping(pc.id(), request);
        REQUIRE(ping.isOk());
        simulator.runUntilIdle(seconds(5));

        CHECK(pc.ipv4Stack()->pingStatistics(ping.value())->received == 1);
    }

    SECTION("a reset takes the leased address away again") {
        simulator.reset();
        CHECK(iface(pc, "Gi0").ipv4Addresses().empty());
        CHECK(iface(pc, "Gi0").ipv4DhcpEnabled()); // the configuration survives
    }
}

TEST_CASE("Two clients get different addresses", "[simulation][dhcp]") {
    Project project;
    Network& network = project.network();

    Device& pcA = addDevice(network, DeviceType::Pc, "PC1");
    Device& pcB = addDevice(network, DeviceType::Pc, "PC2");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");
    Device& server = addDevice(network, DeviceType::Server, "Server1");

    connect(network, pcA, "Gi0", sw, "Gi0/1");
    connect(network, pcB, "Gi0", sw, "Gi0/2");
    connect(network, sw, "Gi0/3", server, "Gi0");
    assign(server, "Gi0", "10.0.0.1/24");

    iface(pcA, "Gi0").setIpv4DhcpEnabled(true);
    iface(pcB, "Gi0").setIpv4DhcpEnabled(true);
    network.refreshOperationalStates();

    DhcpPool pool;
    pool.subnet = prefix("10.0.0.0/24");
    pool.rangeFirst = ipv4("10.0.0.50");
    pool.rangeLast = ipv4("10.0.0.60");
    pool.gateway = ipv4("10.0.0.1");
    server.dhcpServer()->setEnabled(true);
    server.dhcpServer()->addPool(pool);

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();
    simulator.runUntilIdle(seconds(30));

    const auto addressA = iface(pcA, "Gi0").primaryIpv4();
    const auto addressB = iface(pcB, "Gi0").primaryIpv4();

    REQUIRE(addressA.has_value());
    REQUIRE(addressB.has_value());
    CHECK(addressA->address() != addressB->address());
    CHECK(server.dhcpServer()->leases().size() == 2);
}

TEST_CASE("An exhausted pool hands out nothing", "[simulation][dhcp]") {
    Project project;
    Network& network = project.network();

    Device& pcA = addDevice(network, DeviceType::Pc, "PC1");
    Device& pcB = addDevice(network, DeviceType::Pc, "PC2");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");
    Device& server = addDevice(network, DeviceType::Server, "Server1");

    connect(network, pcA, "Gi0", sw, "Gi0/1");
    connect(network, pcB, "Gi0", sw, "Gi0/2");
    connect(network, sw, "Gi0/3", server, "Gi0");
    assign(server, "Gi0", "10.0.0.1/24");

    iface(pcA, "Gi0").setIpv4DhcpEnabled(true);
    iface(pcB, "Gi0").setIpv4DhcpEnabled(true);
    network.refreshOperationalStates();

    // Room for exactly one client.
    DhcpPool pool;
    pool.subnet = prefix("10.0.0.0/24");
    pool.rangeFirst = ipv4("10.0.0.50");
    pool.rangeLast = ipv4("10.0.0.50");
    server.dhcpServer()->setEnabled(true);
    server.dhcpServer()->addPool(pool);

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();
    simulator.runUntilIdle(seconds(60));

    CHECK(server.dhcpServer()->leases().size() == 1);
    CHECK(sawEvent(simulator, TraceKind::DhcpNoAddressAvailable));

    // One client is configured, the other is not - and neither is confused.
    const bool aHasAddress = iface(pcA, "Gi0").primaryIpv4().has_value();
    const bool bHasAddress = iface(pcB, "Gi0").primaryIpv4().has_value();
    CHECK(aHasAddress != bHasAddress);
}

TEST_CASE("A DNS server answers a real query", "[simulation][dns]") {
    Project project;
    Network& network = project.network();

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& server = addDevice(network, DeviceType::Server, "Server1");

    connect(network, pc, "Gi0", server, "Gi0");
    assign(pc, "Gi0", "10.0.0.10/24");
    assign(server, "Gi0", "10.0.0.53/24");
    network.refreshOperationalStates();

    REQUIRE(server.dnsServer() != nullptr);
    server.dnsServer()->setEnabled(true);
    server.dnsServer()->addRecord(
        DnsRecord{DnsRecordId::generate(), "www.example.local", ipv4("10.0.0.80"), 300});

    // TNP has no resolver client yet, so the query is sent directly through the
    // host's UDP path - the same path a resolver would use.
    proto::DnsMessage query;
    query.transactionId = 0x1234;
    query.questions.push_back(
        proto::DnsQuestion{"www.example.local", static_cast<u16>(proto::DnsRecordType::A), 1});

    std::optional<proto::DnsMessage> answer;
    pc.ipv4Stack()->bindUdpPort(45000, [&](DeviceContext&, Interface&, const proto::Ipv4Header&,
                                           const proto::UdpHeader&, std::span<const u8> payload) {
        answer = proto::decodeDns(payload);
    });

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();

    REQUIRE(pc.ipv4Stack()->sendUdp(simulator, ipv4("10.0.0.53"), 45000, proto::kPortDns,
                                    proto::encodeDns(query), FrameCategory::Dns, "DNS query"));

    simulator.runUntilIdle(seconds(10));

    CHECK(sawEvent(simulator, TraceKind::DnsNameResolved));
    CHECK(sawEvent(simulator, TraceKind::DnsResponseSent));

    REQUIRE(answer.has_value());
    CHECK(answer->isResponse);
    CHECK(answer->transactionId == 0x1234);
    CHECK(answer->responseCode == proto::DnsResponseCode::NoError);
    REQUIRE(answer->answers.size() == 1);
    CHECK(answer->answers.front().address == ipv4("10.0.0.80"));
}

TEST_CASE("A DNS server returns NXDOMAIN for a name it does not hold", "[simulation][dns]") {
    Project project;
    Network& network = project.network();

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& server = addDevice(network, DeviceType::Server, "Server1");

    connect(network, pc, "Gi0", server, "Gi0");
    assign(pc, "Gi0", "10.0.0.10/24");
    assign(server, "Gi0", "10.0.0.53/24");
    network.refreshOperationalStates();

    server.dnsServer()->setEnabled(true);

    proto::DnsMessage query;
    query.transactionId = 7;
    query.questions.push_back(proto::DnsQuestion{"nowhere.local", 1, 1});

    std::optional<proto::DnsMessage> answer;
    pc.ipv4Stack()->bindUdpPort(45000, [&](DeviceContext&, Interface&, const proto::Ipv4Header&,
                                           const proto::UdpHeader&, std::span<const u8> payload) {
        answer = proto::decodeDns(payload);
    });

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();
    REQUIRE(pc.ipv4Stack()->sendUdp(simulator, ipv4("10.0.0.53"), 45000, proto::kPortDns,
                                    proto::encodeDns(query), FrameCategory::Dns, "DNS query"));
    simulator.runUntilIdle(seconds(10));

    CHECK(sawEvent(simulator, TraceKind::DnsNameNotFound));
    REQUIRE(answer.has_value());
    CHECK(answer->responseCode == proto::DnsResponseCode::NameError);
    CHECK(answer->answers.empty());
}

TEST_CASE("Nothing listening on a UDP port produces an ICMP port unreachable",
          "[simulation][udp]") {
    Project project;
    Network& network = project.network();

    Device& pcA = addDevice(network, DeviceType::Pc, "PC1");
    Device& pcB = addDevice(network, DeviceType::Pc, "PC2");

    connect(network, pcA, "Gi0", pcB, "Gi0");
    assign(pcA, "Gi0", "10.0.0.1/24");
    assign(pcB, "Gi0", "10.0.0.2/24");
    network.refreshOperationalStates();

    sim::Simulator simulator{network, project.simulationSettings()};
    simulator.start();

    const ByteBuffer payload(16, 0x42);
    REQUIRE(pcA.ipv4Stack()->sendUdp(simulator, ipv4("10.0.0.2"), 5000, 9999, payload,
                                     FrameCategory::Udp, "test datagram"));
    simulator.runUntilIdle(seconds(5));

    CHECK(sawEvent(simulator, TraceKind::UdpDatagramSent));
    CHECK(sawEvent(simulator, TraceKind::UdpDatagramReceived));
    CHECK(sawEvent(simulator, TraceKind::UdpPortUnreachable));
    CHECK(sawEvent(simulator, TraceKind::IcmpDestinationUnreachableSent));
    CHECK(sawEvent(simulator, TraceKind::IcmpDestinationUnreachableReceived));
}
