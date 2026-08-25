#include "app/SampleProject.h"

#include "core/devices/DeviceRegistry.h"
#include "core/devices/DnsServer.h"
#include "core/devices/Ipv4Stack.h"
#include "utilities/Logging.h"

#include <format>

namespace tnp::app {
namespace {

using namespace core;

/// Assigns an address, logging rather than throwing if the sample itself is
/// wrong - a broken sample must not take the application down with it.
void assign(Device& device, std::string_view interfaceName, std::string_view cidr) {
    Interface* iface = device.findInterfaceByName(interfaceName);
    const auto prefix = Ipv4Prefix::parse(cidr);
    if (iface == nullptr || !prefix) {
        logging::error("sample", "cannot address {} on {}", cidr, device.name());
        return;
    }
    if (const Status status = iface->addIpv4Address(*prefix); !status) {
        logging::error("sample", "{}: {}", device.name(), status.message());
    }
}

void cable(Network& network, Device& a, std::string_view aName, Device& b, std::string_view bName) {
    const Interface* first = a.findInterfaceByName(aName);
    const Interface* second = b.findInterfaceByName(bName);
    if (first == nullptr || second == nullptr) {
        logging::error("sample", "cannot connect {} {} to {} {}", a.name(), aName, b.name(), bName);
        return;
    }
    if (auto link = network.connect(first->id(), second->id()); !link) {
        logging::error("sample", "cannot connect: {}", link.message());
    }
}

void addStaticRoute(Device& device, std::string_view destination, std::string_view nextHop) {
    Ipv4Stack* stack = device.ipv4Stack();
    const auto prefix = Ipv4Prefix::parse(destination);
    const auto hop = Ipv4Address::parse(nextHop);
    if (stack == nullptr || !prefix || !hop) return;

    StaticRouteEntry entry;
    entry.destination = *prefix;
    entry.nextHop = *hop;
    entry.description = "sample topology";

    if (const Status status = stack->addStaticRoute(entry); !status) {
        logging::error("sample", "{}: {}", device.name(), status.message());
    }
}

} // namespace

void buildSampleProject(Project& project) {
    project.reset();
    project.metadata().name = "Sample Network";
    project.metadata().description =
        "Two LANs joined by a routed point-to-point link. Ping Server1 from PC1 to watch ARP "
        "resolve, the switch learn, and the routers forward.";
    project.metadata().author = "TNP";
    project.metadata().tags = {"sample", "routing", "icmp"};

    Network& network = project.network();
    const DeviceRegistry& registry = builtinDeviceRegistry();

    Device& pc = network.addDevice(registry.create(DeviceType::Pc, "PC1"));
    Device& sw = network.addDevice(registry.create(DeviceType::Switch, "Switch1"));
    Device& r1 = network.addDevice(registry.create(DeviceType::Router, "Router1"));
    Device& r2 = network.addDevice(registry.create(DeviceType::Router, "Router2"));
    Device& server = network.addDevice(registry.create(DeviceType::Server, "Server1"));

    cable(network, pc, "GigabitEthernet0", sw, "GigabitEthernet0/1");
    cable(network, sw, "GigabitEthernet0/2", r1, "GigabitEthernet0/0");
    cable(network, r1, "GigabitEthernet0/1", r2, "GigabitEthernet0/0");
    cable(network, r2, "GigabitEthernet0/1", server, "GigabitEthernet0");

    assign(pc, "GigabitEthernet0", "192.168.1.10/24");
    assign(r1, "GigabitEthernet0/0", "192.168.1.1/24");
    assign(r1, "GigabitEthernet0/1", "10.0.0.1/30");
    assign(r2, "GigabitEthernet0/0", "10.0.0.2/30");
    assign(r2, "GigabitEthernet0/1", "172.16.0.1/24");
    assign(server, "GigabitEthernet0", "172.16.0.20/24");

    // Interface state feeds connected routes, so it has to be correct before any
    // static route is resolved against it.
    network.refreshOperationalStates();

    pc.ipv4Stack()->setDefaultGateway(Ipv4Address::parse("192.168.1.1"));
    server.ipv4Stack()->setDefaultGateway(Ipv4Address::parse("172.16.0.1"));

    addStaticRoute(r1, "172.16.0.0/24", "10.0.0.2");
    addStaticRoute(r2, "192.168.1.0/24", "10.0.0.1");

    if (DnsServer* dns = server.dnsServer()) {
        dns->setEnabled(true);
        dns->addRecord(DnsRecord{DnsRecordId::generate(), "server.local",
                                 *Ipv4Address::parse("172.16.0.20"), 300});
        dns->addRecord(DnsRecord{DnsRecordId::generate(), "router.local",
                                 *Ipv4Address::parse("192.168.1.1"), 300});
    }

    Layout& layout = project.layout();
    layout.setPosition(pc.id(), Vec2{-380.0f, 0.0f});
    layout.setPosition(sw.id(), Vec2{-190.0f, 0.0f});
    layout.setPosition(r1.id(), Vec2{0.0f, 0.0f});
    layout.setPosition(r2.id(), Vec2{190.0f, 0.0f});
    layout.setPosition(server.id(), Vec2{380.0f, 0.0f});

    Annotation lan;
    lan.kind = AnnotationKind::NetworkLabel;
    lan.start = Vec2{-450.0f, -90.0f};
    lan.end = Vec2{-120.0f, 90.0f};
    lan.text = "Office LAN  192.168.1.0/24";
    project.addAnnotation(lan);

    Annotation dataCentre;
    dataCentre.kind = AnnotationKind::NetworkLabel;
    dataCentre.start = Vec2{120.0f, -90.0f};
    dataCentre.end = Vec2{450.0f, 90.0f};
    dataCentre.text = "Data centre  172.16.0.0/24";
    project.addAnnotation(dataCentre);

    NetworkTest reachServer;
    reachServer.name = "PC1 can reach Server1";
    reachServer.description = "End-to-end connectivity across both routers.";
    reachServer.source = pc.id();
    reachServer.destinationDevice = server.id();
    reachServer.expectation = TestExpectation::Reachable;
    project.addTest(reachServer);

    NetworkTest reachGateway;
    reachGateway.name = "PC1 can reach its gateway";
    reachGateway.description = "Proves the local segment and ARP work.";
    reachGateway.source = pc.id();
    reachGateway.destinationDevice = r1.id();
    reachGateway.expectation = TestExpectation::Reachable;
    project.addTest(reachGateway);

    NetworkTest unroutable;
    unroutable.name = "PC1 cannot reach an unrouted network";
    unroutable.description = "203.0.113.5 is in no routing table, so this must fail.";
    unroutable.source = pc.id();
    unroutable.destinationAddress = Ipv4Address::parse("203.0.113.5");
    unroutable.expectation = TestExpectation::Unreachable;
    unroutable.probeCount = 1;
    project.addTest(unroutable);

    project.touch();
}

} // namespace tnp::app
