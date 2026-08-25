#include "serialization/ProjectSerializer.h"

#include "core/devices/DhcpServer.h"
#include "core/devices/DnsServer.h"
#include "core/devices/FirewallPolicy.h"
#include "core/devices/Ipv4Stack.h"
#include "core/devices/SwitchingEngine.h"
#include "serialization/JsonSerializer.h"
#include "utilities/Logging.h"
#include "utilities/StringUtilities.h"

#include <format>

namespace tnp::serial {
namespace {

using namespace core;

constexpr std::string_view kFormatName = "project";

// --- Enum text -------------------------------------------------------------
// Written as names rather than numbers: a `.tnpjson` file is meant to be read
// and hand-edited, and a renumbered enum must never silently change a project.

std::string_view adminStateText(AdminState state) { return state == AdminState::Up ? "up" : "down"; }
AdminState parseAdminState(std::string_view text) {
    return strings::equalsIgnoreCase(text, "down") ? AdminState::Down : AdminState::Up;
}

std::string_view duplexText(DuplexMode mode) { return duplexModeName(mode); }
DuplexMode parseDuplex(std::string_view text) {
    if (strings::equalsIgnoreCase(text, "half")) return DuplexMode::Half;
    if (strings::equalsIgnoreCase(text, "full")) return DuplexMode::Full;
    return DuplexMode::Auto;
}

std::string_view vlanModeText(VlanMode mode) { return vlanModeName(mode); }
VlanMode parseVlanMode(std::string_view text) {
    return strings::equalsIgnoreCase(text, "trunk") ? VlanMode::Trunk : VlanMode::Access;
}

std::string_view linkMediumText(LinkMedium medium) { return linkMediumName(medium); }
LinkMedium parseLinkMedium(std::string_view text) {
    if (strings::equalsIgnoreCase(text, "fiber")) return LinkMedium::Fiber;
    if (strings::equalsIgnoreCase(text, "serial")) return LinkMedium::Serial;
    if (strings::equalsIgnoreCase(text, "wireless")) return LinkMedium::Wireless;
    if (strings::equalsIgnoreCase(text, "virtual")) return LinkMedium::Virtual;
    return LinkMedium::Copper;
}

FirewallAction parseFirewallAction(std::string_view text) {
    return strings::equalsIgnoreCase(text, "permit") ? FirewallAction::Permit : FirewallAction::Deny;
}

FirewallProtocolMatch parseFirewallProtocol(std::string_view text) {
    if (strings::equalsIgnoreCase(text, "icmp")) return FirewallProtocolMatch::Icmp;
    if (strings::equalsIgnoreCase(text, "tcp")) return FirewallProtocolMatch::Tcp;
    if (strings::equalsIgnoreCase(text, "udp")) return FirewallProtocolMatch::Udp;
    return FirewallProtocolMatch::Any;
}

AnnotationKind parseAnnotationKind(std::string_view text) {
    if (strings::equalsIgnoreCase(text, "Rectangle")) return AnnotationKind::Rectangle;
    if (strings::equalsIgnoreCase(text, "Ellipse")) return AnnotationKind::Ellipse;
    if (strings::equalsIgnoreCase(text, "Arrow")) return AnnotationKind::Arrow;
    if (strings::equalsIgnoreCase(text, "NetworkLabel")) return AnnotationKind::NetworkLabel;
    return AnnotationKind::Text;
}

std::string_view annotationKindText(AnnotationKind kind) {
    switch (kind) {
        case AnnotationKind::Text:         return "Text";
        case AnnotationKind::Rectangle:    return "Rectangle";
        case AnnotationKind::Ellipse:      return "Ellipse";
        case AnnotationKind::Arrow:        return "Arrow";
        case AnnotationKind::NetworkLabel: return "NetworkLabel";
    }
    return "Text";
}

TestExpectation parseExpectation(std::string_view text) {
    return strings::equalsIgnoreCase(text, "unreachable") ? TestExpectation::Unreachable
                                                          : TestExpectation::Reachable;
}

OspfNetworkType parseOspfNetworkType(std::string_view text) {
    if (strings::equalsIgnoreCase(text, "point-to-point")) return OspfNetworkType::PointToPoint;
    if (strings::equalsIgnoreCase(text, "non-broadcast")) return OspfNetworkType::NonBroadcast;
    return OspfNetworkType::Broadcast;
}

// --- Writing ---------------------------------------------------------------

Json writeInterface(const Interface& iface) {
    Json node;
    node["id"] = iface.id().toString();
    node["name"] = iface.name();
    node["type"] = interfaceTypeName(iface.type());
    node["mac"] = iface.macAddress().toString();
    node["adminState"] = adminStateText(iface.adminState());
    node["mtu"] = iface.mtu();
    node["speedMbps"] = iface.speedMbps();
    node["duplex"] = duplexText(iface.duplex());
    node["dhcp"] = iface.ipv4DhcpEnabled();

    if (!iface.displayName().empty() && iface.displayName() != iface.name()) {
        node["displayName"] = iface.displayName();
    }
    if (!iface.description().empty()) node["description"] = iface.description();

    // An interface configured for DHCP has no *configured* address: whatever it
    // holds during a run was leased, and writing that would turn a lease into a
    // permanent setting the next time the project is opened.
    if (!iface.ipv4DhcpEnabled()) {
        Json addresses = Json::array();
        for (const Ipv4Prefix& prefix : iface.ipv4Addresses()) addresses.push_back(prefix.toString());
        if (!addresses.empty()) node["ipv4"] = std::move(addresses);
    }

    Json v6 = Json::array();
    for (const Ipv6Prefix& prefix : iface.ipv6Addresses()) v6.push_back(prefix.toString());
    if (!v6.empty()) node["ipv6"] = std::move(v6);

    const VlanConfiguration& vlan = iface.vlan();
    Json vlanNode;
    vlanNode["mode"] = vlanModeText(vlan.mode);
    vlanNode["accessVlan"] = vlan.accessVlan;
    vlanNode["nativeVlan"] = vlan.nativeVlan;
    if (!vlan.allowedVlans.empty()) vlanNode["allowed"] = vlan.allowedVlans;
    node["vlan"] = std::move(vlanNode);

    return node;
}

Json writeOspf(const OspfConfiguration& ospf) {
    Json node;
    node["enabled"] = ospf.enabled;
    node["processId"] = ospf.processId;
    node["routerId"] = ospf.routerId.toString();
    node["redistributeConnected"] = ospf.redistributeConnected;
    node["redistributeStatic"] = ospf.redistributeStatic;

    Json networks = Json::array();
    for (const OspfNetworkStatement& statement : ospf.networks) {
        networks.push_back(Json{{"network", statement.network.toNetworkString()},
                                {"area", statement.areaId}});
    }
    node["networks"] = std::move(networks);

    Json interfaces = Json::array();
    for (const OspfInterfaceSettings& settings : ospf.interfaces) {
        interfaces.push_back(Json{{"interface", settings.interface.toString()},
                                  {"cost", settings.cost},
                                  {"helloInterval", settings.helloIntervalSeconds},
                                  {"deadInterval", settings.deadIntervalSeconds},
                                  {"priority", settings.priority},
                                  {"passive", settings.passive},
                                  {"networkType", ospfNetworkTypeName(settings.networkType)}});
    }
    node["interfaces"] = std::move(interfaces);
    return node;
}

Json writeIpv4Stack(const Ipv4Stack& stack) {
    Json node;
    node["forwarding"] = stack.forwardingEnabled();
    if (!stack.domainName().empty()) node["domainName"] = stack.domainName();

    Json servers = Json::array();
    for (const Ipv4Address& address : stack.dnsServers()) servers.push_back(address.toString());
    if (!servers.empty()) node["dnsServers"] = std::move(servers);

    Json routes = Json::array();
    for (const StaticRouteEntry& entry : stack.staticRoutes()) {
        Json route;
        route["id"] = entry.id.toString();
        route["destination"] = entry.destination.toNetworkString();
        if (entry.nextHop) route["nextHop"] = entry.nextHop->toString();
        if (entry.egressInterface.isValid()) route["interface"] = entry.egressInterface.toString();
        route["metric"] = entry.metric;
        route["enabled"] = entry.enabled;
        if (!entry.description.empty()) route["description"] = entry.description;
        routes.push_back(std::move(route));
    }
    node["staticRoutes"] = std::move(routes);

    if (stack.ospf().enabled || !stack.ospf().networks.empty()) node["ospf"] = writeOspf(stack.ospf());
    return node;
}

Json writeSwitching(const SwitchingEngine& switching) {
    Json node;
    node["learning"] = switching.learningEnabled();
    node["ageingSeconds"] =
        std::chrono::duration_cast<std::chrono::seconds>(switching.ageingTime()).count();

    Json vlans = Json::array();
    for (const VlanDefinition& vlan : switching.vlans()) {
        vlans.push_back(Json{{"id", vlan.id}, {"name", vlan.name}});
    }
    node["vlans"] = std::move(vlans);
    return node;
}

Json writeFirewall(const FirewallPolicy& policy) {
    Json node;
    node["defaultAction"] = firewallActionName(policy.defaultAction());

    Json rules = Json::array();
    for (const FirewallRule& rule : policy.rules()) {
        Json entry;
        entry["id"] = rule.id.toString();
        entry["name"] = rule.name;
        entry["action"] = firewallActionName(rule.action);
        entry["protocol"] = firewallProtocolName(rule.protocol);
        if (rule.source) entry["source"] = rule.source->toNetworkString();
        if (rule.destination) entry["destination"] = rule.destination->toNetworkString();
        if (rule.destinationPortFirst) entry["portFirst"] = *rule.destinationPortFirst;
        if (rule.destinationPortLast) entry["portLast"] = *rule.destinationPortLast;
        entry["enabled"] = rule.enabled;
        if (!rule.description.empty()) entry["description"] = rule.description;
        rules.push_back(std::move(entry));
    }
    node["rules"] = std::move(rules);
    return node;
}

Json writeDhcpServer(const DhcpServer& server) {
    Json node;
    node["enabled"] = server.isEnabled();

    Json pools = Json::array();
    for (const DhcpPool& pool : server.pools()) {
        Json entry;
        entry["id"] = pool.id.toString();
        entry["name"] = pool.name;
        entry["subnet"] = pool.subnet.toNetworkString();
        entry["rangeFirst"] = pool.rangeFirst.toString();
        entry["rangeLast"] = pool.rangeLast.toString();
        if (pool.gateway) entry["gateway"] = pool.gateway->toString();
        if (pool.dnsServer) entry["dns"] = pool.dnsServer->toString();
        if (!pool.domainName.empty()) entry["domainName"] = pool.domainName;
        entry["leaseSeconds"] =
            std::chrono::duration_cast<std::chrono::seconds>(pool.leaseTime).count();

        Json exclusions = Json::array();
        for (const Ipv4Address& address : pool.exclusions) exclusions.push_back(address.toString());
        if (!exclusions.empty()) entry["exclusions"] = std::move(exclusions);

        pools.push_back(std::move(entry));
    }
    node["pools"] = std::move(pools);
    return node;
}

Json writeDnsServer(const DnsServer& server) {
    Json node;
    node["enabled"] = server.isEnabled();

    Json records = Json::array();
    for (const DnsRecord& record : server.records()) {
        records.push_back(Json{{"id", record.id.toString()},
                               {"name", record.name},
                               {"address", record.address.toString()},
                               {"ttl", record.timeToLive}});
    }
    node["records"] = std::move(records);
    return node;
}

Json writeDevice(const Device& device) {
    Json node;
    node["id"] = device.id().toString();
    node["type"] = deviceTypeName(device.type());
    node["name"] = device.name();
    if (!device.description().empty()) node["description"] = device.description();

    Json interfaces = Json::array();
    for (const auto& iface : device.interfaces()) interfaces.push_back(writeInterface(*iface));
    node["interfaces"] = std::move(interfaces);

    if (const Ipv4Stack* stack = device.ipv4Stack()) node["ipv4"] = writeIpv4Stack(*stack);
    if (const SwitchingEngine* switching = device.switching()) node["switching"] = writeSwitching(*switching);
    if (const FirewallPolicy* policy = device.firewallPolicy()) node["firewall"] = writeFirewall(*policy);
    if (const DhcpServer* dhcp = device.dhcpServer()) node["dhcpServer"] = writeDhcpServer(*dhcp);
    if (const DnsServer* dns = device.dnsServer()) node["dnsServer"] = writeDnsServer(*dns);

    return node;
}

Json writeLink(const Link& link) {
    Json node;
    node["id"] = link.id().toString();
    node["a"] = Json{{"device", link.endpointA().device.toString()},
                     {"interface", link.endpointA().interface.toString()}};
    node["b"] = Json{{"device", link.endpointB().device.toString()},
                     {"interface", link.endpointB().interface.toString()}};
    node["medium"] = linkMediumText(link.medium());
    node["propagationDelayNs"] = writeDuration(link.propagationDelay());
    node["bandwidthMbps"] = link.bandwidthMbps();
    node["enabled"] = link.isEnabled();
    if (!link.label().empty()) node["label"] = link.label();
    return node;
}

Json writeLayout(const Layout& layout) {
    Json devices = Json::array();
    for (const auto& [device, placement] : layout.placements()) {
        devices.push_back(Json{{"device", device.toString()},
                               {"x", placement.position.x},
                               {"y", placement.position.y},
                               {"locked", placement.locked}});
    }
    // Sorted so the file is stable across runs: an unordered map's iteration
    // order would otherwise produce a spurious diff on every save.
    std::sort(devices.begin(), devices.end(), [](const Json& a, const Json& b) {
        return a.at("device").get<std::string>() < b.at("device").get<std::string>();
    });

    Json node;
    node["devices"] = std::move(devices);
    node["view"] = Json{{"x", layout.viewOffset.x}, {"y", layout.viewOffset.y}, {"zoom", layout.viewZoom}};
    node["grid"] = Json{{"visible", layout.gridVisible},
                        {"snap", layout.snapToGrid},
                        {"size", layout.gridSize}};
    return node;
}

Json writeAnnotation(const Annotation& annotation) {
    Json node;
    node["id"] = annotation.id.toString();
    node["kind"] = annotationKindText(annotation.kind);
    node["start"] = writeVec2(annotation.start);
    node["end"] = writeVec2(annotation.end);
    node["text"] = annotation.text;
    node["color"] = annotation.color;
    node["fillColor"] = annotation.fillColor;
    node["fontSize"] = annotation.fontSize;
    node["thickness"] = annotation.thickness;
    node["filled"] = annotation.filled;
    node["zOrder"] = annotation.zOrder;
    return node;
}

Json writeTest(const NetworkTest& test) {
    Json node;
    node["id"] = test.id.toString();
    node["name"] = test.name;
    if (!test.description.empty()) node["description"] = test.description;
    node["source"] = test.source.toString();
    if (test.destinationDevice.isValid()) node["destinationDevice"] = test.destinationDevice.toString();
    if (test.destinationAddress) node["destinationAddress"] = test.destinationAddress->toString();
    node["protocol"] = testProtocolName(test.protocol);
    node["expectation"] = testExpectationName(test.expectation);
    node["probeCount"] = test.probeCount;
    node["timeoutNs"] = writeDuration(test.timeout);
    node["payloadSize"] = test.payloadSize;
    node["enabled"] = test.enabled;
    return node;
}

Json writeSimulationSettings(const SimulationSettings& settings) {
    Json node;
    node["speedMultiplier"] = settings.speedMultiplier;
    node["maximumStepPerFrameNs"] = writeDuration(settings.maximumStepPerFrame);
    node["traceHistoryLimit"] = settings.traceHistoryLimit;
    node["packetHistoryLimit"] = settings.packetHistoryLimit;
    node["learningMode"] = settings.learningModeEnabled;
    node["autoStartOnTraffic"] = settings.autoStartOnTraffic;
    return node;
}

Json writeMetadata(const ProjectMetadata& metadata) {
    Json node;
    node["id"] = metadata.id.toString();
    node["name"] = metadata.name;
    node["description"] = metadata.description;
    node["author"] = metadata.author;
    node["tags"] = metadata.tags;
    node["createdAt"] = metadata.createdAt;
    node["modifiedAt"] = metadata.modifiedAt;
    node["writtenBy"] = metadata.writtenBy;
    return node;
}

// --- Reading ---------------------------------------------------------------

void readInterface(Device& device, const Json& node, ParseReport& report, std::string_view path) {
    const auto id = readId<InterfaceTag>(node, "id", report, path);
    const std::string name = readString(node, "name", report, path, "Interface");
    const auto type = parseInterfaceType(readString(node, "type", report, path, "GigabitEthernet"));

    if (!type) {
        report.warning(std::string{path}, std::format("unknown interface type on '{}'; "
                                                      "GigabitEthernet was used", name));
    }

    Interface& iface = device.addInterface(id, name, type.value_or(InterfaceType::GigabitEthernet));

    iface.setDisplayName(readString(node, "displayName", report, path));
    iface.setDescription(readString(node, "description", report, path));
    iface.setAdminState(parseAdminState(readString(node, "adminState", report, path, "up")));
    iface.setDuplex(parseDuplex(readString(node, "duplex", report, path, "auto")));
    iface.setSpeedMbps(static_cast<u64>(readInt(node, "speedMbps", report, path,
                                                static_cast<i64>(iface.speedMbps()))));
    iface.setIpv4DhcpEnabled(readBool(node, "dhcp", report, path, false));

    if (const auto mac = readMac(node, "mac", report, path)) iface.setMacAddress(*mac);

    const u32 mtu = readUInt(node, "mtu", report, path, kDefaultMtu);
    if (const Status status = iface.setMtu(mtu); !status) {
        report.warning(std::string{path} + ".mtu", status.message());
    }

    for (const Json& entry : readArray(node, "ipv4", report, path)) {
        if (!entry.is_string()) continue;
        const auto prefix = Ipv4Prefix::parse(entry.get<std::string>());
        if (!prefix) {
            report.warning(std::string{path} + ".ipv4",
                           std::format("'{}' is not a CIDR prefix", entry.get<std::string>()));
            continue;
        }
        if (const Status status = iface.addIpv4Address(*prefix); !status) {
            report.warning(std::string{path} + ".ipv4", status.message());
        }
    }

    for (const Json& entry : readArray(node, "ipv6", report, path)) {
        if (!entry.is_string()) continue;
        const auto prefix = Ipv6Prefix::parse(entry.get<std::string>());
        if (!prefix) {
            report.warning(std::string{path} + ".ipv6",
                           std::format("'{}' is not an IPv6 prefix", entry.get<std::string>()));
            continue;
        }
        if (const Status status = iface.addIpv6Address(*prefix); !status) {
            report.warning(std::string{path} + ".ipv6", status.message());
        }
    }

    const Json& vlanNode = readObject(node, "vlan", report, path);
    VlanConfiguration& vlan = iface.vlan();
    vlan.mode = parseVlanMode(readString(vlanNode, "mode", report, path, "access"));
    vlan.accessVlan = static_cast<VlanId>(readUInt(vlanNode, "accessVlan", report, path, kDefaultVlan));
    vlan.nativeVlan = static_cast<VlanId>(readUInt(vlanNode, "nativeVlan", report, path, kDefaultVlan));
    vlan.allowedVlans.clear();
    for (const Json& entry : readArray(vlanNode, "allowed", report, path)) {
        if (entry.is_number_unsigned() || entry.is_number_integer()) {
            vlan.allowedVlans.push_back(static_cast<VlanId>(entry.get<i64>()));
        }
    }
}

void readOspf(OspfConfiguration& ospf, const Json& node, ParseReport& report, std::string_view path) {
    ospf.enabled = readBool(node, "enabled", report, path, false);
    ospf.processId = readUInt(node, "processId", report, path, 1);
    ospf.routerId = readIpv4(node, "routerId", report, path).value_or(Ipv4Address{});
    ospf.redistributeConnected = readBool(node, "redistributeConnected", report, path, false);
    ospf.redistributeStatic = readBool(node, "redistributeStatic", report, path, false);

    for (const Json& entry : readArray(node, "networks", report, path)) {
        const auto prefix = readIpv4Prefix(entry, "network", report, path);
        if (!prefix) continue;
        ospf.networks.push_back(OspfNetworkStatement{*prefix, readUInt(entry, "area", report, path, 0)});
    }

    for (const Json& entry : readArray(node, "interfaces", report, path)) {
        OspfInterfaceSettings settings;
        settings.interface = readOptionalId<InterfaceTag>(entry, "interface");
        settings.cost = readUInt(entry, "cost", report, path, 10);
        settings.helloIntervalSeconds = static_cast<u16>(readUInt(entry, "helloInterval", report, path, 10));
        settings.deadIntervalSeconds = static_cast<u16>(readUInt(entry, "deadInterval", report, path, 40));
        settings.priority = static_cast<u8>(readUInt(entry, "priority", report, path, 1));
        settings.passive = readBool(entry, "passive", report, path, false);
        settings.networkType = parseOspfNetworkType(readString(entry, "networkType", report, path,
                                                               "broadcast"));
        ospf.interfaces.push_back(settings);
    }
}

void readIpv4Stack(Ipv4Stack& stack, const Json& node, ParseReport& report, std::string_view path) {
    stack.setForwardingEnabled(readBool(node, "forwarding", report, path, stack.forwardingEnabled()));
    stack.setDomainName(readString(node, "domainName", report, path));

    std::vector<Ipv4Address> servers;
    for (const Json& entry : readArray(node, "dnsServers", report, path)) {
        if (!entry.is_string()) continue;
        if (const auto address = Ipv4Address::parse(entry.get<std::string>())) servers.push_back(*address);
    }
    stack.setDnsServers(std::move(servers));

    std::vector<StaticRouteEntry> routes;
    for (const Json& entry : readArray(node, "staticRoutes", report, path)) {
        StaticRouteEntry route;
        route.id = readId<RouteTag>(entry, "id", report, path);

        const auto destination = readIpv4Prefix(entry, "destination", report, path);
        if (!destination) {
            report.warning(std::string{path} + ".staticRoutes", "a route has no valid destination and was dropped");
            continue;
        }
        route.destination = *destination;
        route.nextHop = readIpv4(entry, "nextHop", report, path);
        route.egressInterface = readOptionalId<InterfaceTag>(entry, "interface");
        route.metric = readUInt(entry, "metric", report, path, 1);
        route.enabled = readBool(entry, "enabled", report, path, true);
        route.description = readString(entry, "description", report, path);
        routes.push_back(std::move(route));
    }
    stack.setStaticRoutes(std::move(routes));

    if (node.contains("ospf")) {
        readOspf(stack.ospf(), readObject(node, "ospf", report, path), report, path);
    }
}

void readSwitching(SwitchingEngine& switching, const Json& node, ParseReport& report,
                   std::string_view path) {
    switching.setLearningEnabled(readBool(node, "learning", report, path, true));

    const i64 ageing = readInt(node, "ageingSeconds", report, path, 300);
    if (ageing > 0) switching.setAgeingTime(seconds(ageing));

    std::vector<VlanDefinition> vlans;
    for (const Json& entry : readArray(node, "vlans", report, path)) {
        VlanDefinition vlan;
        vlan.id = static_cast<VlanId>(readUInt(entry, "id", report, path, kDefaultVlan));
        vlan.name = readString(entry, "name", report, path, "vlan");
        if (!isValidVlanId(vlan.id)) {
            report.warning(std::string{path} + ".vlans", std::format("VLAN {} is out of range", vlan.id));
            continue;
        }
        vlans.push_back(std::move(vlan));
    }
    if (!vlans.empty()) switching.setVlans(std::move(vlans));
}

void readFirewall(FirewallPolicy& policy, const Json& node, ParseReport& report,
                  std::string_view path) {
    policy.setDefaultAction(parseFirewallAction(readString(node, "defaultAction", report, path, "permit")));

    std::vector<FirewallRule> rules;
    for (const Json& entry : readArray(node, "rules", report, path)) {
        FirewallRule rule;
        rule.id = readId<FirewallRuleTag>(entry, "id", report, path);
        rule.name = readString(entry, "name", report, path);
        rule.action = parseFirewallAction(readString(entry, "action", report, path, "deny"));
        rule.protocol = parseFirewallProtocol(readString(entry, "protocol", report, path, "ip"));
        rule.source = readIpv4Prefix(entry, "source", report, path);
        rule.destination = readIpv4Prefix(entry, "destination", report, path);
        if (entry.contains("portFirst")) {
            rule.destinationPortFirst = static_cast<u16>(readUInt(entry, "portFirst", report, path, 0));
        }
        if (entry.contains("portLast")) {
            rule.destinationPortLast = static_cast<u16>(readUInt(entry, "portLast", report, path, 0));
        }
        rule.enabled = readBool(entry, "enabled", report, path, true);
        rule.description = readString(entry, "description", report, path);
        rules.push_back(std::move(rule));
    }
    policy.setRules(std::move(rules));
}

void readDhcpServer(DhcpServer& server, const Json& node, ParseReport& report, std::string_view path) {
    server.setEnabled(readBool(node, "enabled", report, path, false));

    std::vector<DhcpPool> pools;
    for (const Json& entry : readArray(node, "pools", report, path)) {
        DhcpPool pool;
        pool.id = readId<DhcpPoolTag>(entry, "id", report, path);
        pool.name = readString(entry, "name", report, path, "pool");

        const auto subnet = readIpv4Prefix(entry, "subnet", report, path);
        const auto first = readIpv4(entry, "rangeFirst", report, path);
        const auto last = readIpv4(entry, "rangeLast", report, path);
        if (!subnet || !first || !last) {
            report.warning(std::string{path} + ".dhcpServer.pools",
                           std::format("pool '{}' is incomplete and was dropped", pool.name));
            continue;
        }
        pool.subnet = *subnet;
        pool.rangeFirst = *first;
        pool.rangeLast = *last;
        pool.gateway = readIpv4(entry, "gateway", report, path);
        pool.dnsServer = readIpv4(entry, "dns", report, path);
        pool.domainName = readString(entry, "domainName", report, path);

        const i64 lease = readInt(entry, "leaseSeconds", report, path, 86400);
        if (lease > 0) pool.leaseTime = seconds(lease);

        for (const Json& exclusion : readArray(entry, "exclusions", report, path)) {
            if (!exclusion.is_string()) continue;
            if (const auto address = Ipv4Address::parse(exclusion.get<std::string>())) {
                pool.exclusions.push_back(*address);
            }
        }
        pools.push_back(std::move(pool));
    }
    server.setPools(std::move(pools));
}

void readDnsServer(DnsServer& server, const Json& node, ParseReport& report, std::string_view path) {
    server.setEnabled(readBool(node, "enabled", report, path, false));

    std::vector<DnsRecord> records;
    for (const Json& entry : readArray(node, "records", report, path)) {
        DnsRecord record;
        record.id = readId<DnsRecordTag>(entry, "id", report, path);
        record.name = readString(entry, "name", report, path);

        const auto address = readIpv4(entry, "address", report, path);
        if (record.name.empty() || !address) {
            report.warning(std::string{path} + ".dnsServer.records", "a record is incomplete and was dropped");
            continue;
        }
        record.address = *address;
        record.timeToLive = readUInt(entry, "ttl", report, path, 300);
        records.push_back(std::move(record));
    }
    server.setRecords(std::move(records));
}

} // namespace

// ---------------------------------------------------------------------------

ProjectSerializer::ProjectSerializer(const DeviceRegistry& registry) : registry_(registry) {}

Result<std::string> ProjectSerializer::write(const Project& project, bool pretty) const {
    try {
        Json root;
        root["tnp"] = Json{{"format", kFormatName},
                           {"version", kCurrentProjectVersion.toString()}};
        root["metadata"] = writeMetadata(project.metadata());
        root["simulation"] = writeSimulationSettings(project.simulationSettings());

        Json devices = Json::array();
        for (const auto& device : project.network().devices()) devices.push_back(writeDevice(*device));

        Json links = Json::array();
        for (const auto& link : project.network().links()) links.push_back(writeLink(*link));

        root["network"] = Json{{"devices", std::move(devices)}, {"links", std::move(links)}};
        root["layout"] = writeLayout(project.layout());

        Json annotations = Json::array();
        for (const Annotation& annotation : project.annotations()) {
            annotations.push_back(writeAnnotation(annotation));
        }
        root["annotations"] = std::move(annotations);

        Json tests = Json::array();
        for (const NetworkTest& test : project.tests()) tests.push_back(writeTest(test));
        root["tests"] = std::move(tests);

        return pretty ? root.dump(2) : root.dump();
    } catch (const std::exception& error) {
        return Result<std::string>::failure(error.what(), "while serializing the project");
    }
}

Result<LoadReport> ProjectSerializer::read(std::string_view text, Project& project) const {
    Json root;
    try {
        root = Json::parse(text);
    } catch (const Json::parse_error& error) {
        return Result<LoadReport>::failure(error.what(), "the file is not valid JSON");
    }

    if (!root.is_object()) {
        return Result<LoadReport>::failure("the document root must be an object");
    }

    ParseReport report;
    LoadReport result;

    // --- Header, checked before anything is modified -----------------------
    const Json& header = readObject(root, "tnp", report, "tnp");
    const std::string format = readString(header, "format", report, "tnp", std::string{kFormatName});
    if (format != kFormatName) {
        return Result<LoadReport>::failure(
            std::format("this file declares format '{}', not a TNP project", format));
    }

    const std::string versionText = readString(header, "version", report, "tnp", "1.0");
    const auto parts = strings::split(versionText, '.');
    ProjectVersion version;
    if (parts.size() == 2) {
        version.major = strings::parseUInt(parts[0]).value_or(1);
        version.minor = strings::parseUInt(parts[1]).value_or(0);
    }
    if (!isProjectVersionReadable(version)) {
        return Result<LoadReport>::failure(
            std::format("project format {} cannot be read by this build, which supports {}.x",
                        version.toString(), kCurrentProjectVersion.major));
    }
    result.fileVersion = version;
    result.writtenByNewerBuild = isProjectVersionNewer(version);

    // --- From here the project is replaced ---------------------------------
    project.reset();
    Network& network = project.network();

    ProjectMetadata& metadata = project.metadata();
    const Json& metadataNode = readObject(root, "metadata", report, "metadata");
    metadata.id = readId<ProjectTag>(metadataNode, "id", report, "metadata");
    metadata.name = readString(metadataNode, "name", report, "metadata", "Untitled Project");
    metadata.description = readString(metadataNode, "description", report, "metadata");
    metadata.author = readString(metadataNode, "author", report, "metadata");
    metadata.createdAt = readString(metadataNode, "createdAt", report, "metadata");
    metadata.modifiedAt = readString(metadataNode, "modifiedAt", report, "metadata");
    metadata.writtenBy = readString(metadataNode, "writtenBy", report, "metadata");
    metadata.version = version;
    metadata.tags.clear();
    for (const Json& tag : readArray(metadataNode, "tags", report, "metadata")) {
        if (tag.is_string()) metadata.tags.push_back(tag.get<std::string>());
    }

    SimulationSettings& simulation = project.simulationSettings();
    const Json& simulationNode = readObject(root, "simulation", report, "simulation");
    simulation.speedMultiplier = readDouble(simulationNode, "speedMultiplier", report, "simulation", 1.0);
    simulation.maximumStepPerFrame = readDuration(simulationNode, "maximumStepPerFrameNs", report,
                                                  "simulation", milliseconds(100));
    simulation.traceHistoryLimit = readUInt(simulationNode, "traceHistoryLimit", report, "simulation", 20000);
    simulation.packetHistoryLimit = readUInt(simulationNode, "packetHistoryLimit", report, "simulation", 2000);
    simulation.learningModeEnabled = readBool(simulationNode, "learningMode", report, "simulation", false);
    simulation.autoStartOnTraffic = readBool(simulationNode, "autoStartOnTraffic", report, "simulation", true);

    // --- Devices -----------------------------------------------------------
    const Json& networkNode = readObject(root, "network", report, "network");
    for (const Json& node : readArray(networkNode, "devices", report, "network")) {
        const std::string typeText = readString(node, "type", report, "network.devices");
        const auto type = parseDeviceType(typeText);
        if (!type || !registry_.isRegistered(*type)) {
            report.error("network.devices", std::format("unknown device type '{}'; the device was skipped",
                                                        typeText));
            continue;
        }

        const auto id = readId<DeviceTag>(node, "id", report, "network.devices");
        const std::string name = readString(node, "name", report, "network.devices", "Device");

        auto device = registry_.create(*type, id, name);
        if (!device) continue;
        device->setDescription(readString(node, "description", report, "network.devices"));

        // The registry gave the device its default interfaces; the file decides
        // what it actually has.
        device->clearInterfaces();
        const std::string devicePath = std::format("network.devices[{}]", name);
        for (const Json& ifaceNode : readArray(node, "interfaces", report, devicePath)) {
            readInterface(*device, ifaceNode, report, devicePath);
        }

        if (Ipv4Stack* stack = device->ipv4Stack()) {
            readIpv4Stack(*stack, readObject(node, "ipv4", report, devicePath), report, devicePath);
        }
        if (SwitchingEngine* switching = device->switching()) {
            readSwitching(*switching, readObject(node, "switching", report, devicePath), report, devicePath);
        }
        if (FirewallPolicy* policy = device->firewallPolicy()) {
            readFirewall(*policy, readObject(node, "firewall", report, devicePath), report, devicePath);
        }
        if (DhcpServer* dhcp = device->dhcpServer()) {
            readDhcpServer(*dhcp, readObject(node, "dhcpServer", report, devicePath), report, devicePath);
        }
        if (DnsServer* dns = device->dnsServer()) {
            readDnsServer(*dns, readObject(node, "dnsServer", report, devicePath), report, devicePath);
        }

        network.addDevice(std::move(device));
    }

    // --- Links -------------------------------------------------------------
    for (const Json& node : readArray(networkNode, "links", report, "network")) {
        const auto id = readId<LinkTag>(node, "id", report, "network.links");
        const Json& endpointA = readObject(node, "a", report, "network.links");
        const Json& endpointB = readObject(node, "b", report, "network.links");

        const auto interfaceA = readOptionalId<InterfaceTag>(endpointA, "interface");
        const auto interfaceB = readOptionalId<InterfaceTag>(endpointB, "interface");
        const LinkMedium medium = parseLinkMedium(readString(node, "medium", report, "network.links",
                                                             "copper"));

        auto created = network.connectWithId(id, interfaceA, interfaceB, medium);
        if (!created) {
            report.warning("network.links",
                           std::format("a link was dropped: {}", created.message()));
            continue;
        }

        Link* link = network.findLink(created.value());
        if (link == nullptr) continue;
        link->setPropagationDelay(readDuration(node, "propagationDelayNs", report, "network.links",
                                               linkMediumDefaultDelay(medium)));
        link->setBandwidthMbps(static_cast<u64>(readInt(node, "bandwidthMbps", report, "network.links",
                                                        static_cast<i64>(link->bandwidthMbps()))));
        link->setEnabled(readBool(node, "enabled", report, "network.links", true));
        link->setLabel(readString(node, "label", report, "network.links"));
    }

    // --- Layout ------------------------------------------------------------
    Layout& layout = project.layout();
    const Json& layoutNode = readObject(root, "layout", report, "layout");
    for (const Json& node : readArray(layoutNode, "devices", report, "layout")) {
        const auto device = readOptionalId<DeviceTag>(node, "device");
        if (!device.isValid()) continue;
        layout.setPosition(device, Vec2{static_cast<float>(readDouble(node, "x", report, "layout")),
                                        static_cast<float>(readDouble(node, "y", report, "layout"))});
        layout.setLocked(device, readBool(node, "locked", report, "layout", false));
    }

    const Json& viewNode = readObject(layoutNode, "view", report, "layout");
    layout.viewOffset = Vec2{static_cast<float>(readDouble(viewNode, "x", report, "layout.view")),
                             static_cast<float>(readDouble(viewNode, "y", report, "layout.view"))};
    layout.viewZoom = static_cast<float>(readDouble(viewNode, "zoom", report, "layout.view", 1.0));
    if (layout.viewZoom <= 0.0f) layout.viewZoom = 1.0f;

    const Json& gridNode = readObject(layoutNode, "grid", report, "layout");
    layout.gridVisible = readBool(gridNode, "visible", report, "layout.grid", true);
    layout.snapToGrid = readBool(gridNode, "snap", report, "layout.grid", false);
    layout.gridSize = static_cast<float>(readDouble(gridNode, "size", report, "layout.grid", 24.0));
    if (layout.gridSize <= 0.0f) layout.gridSize = 24.0f;

    // --- Annotations -------------------------------------------------------
    std::vector<Annotation> annotations;
    for (const Json& node : readArray(root, "annotations", report, "annotations")) {
        Annotation annotation;
        annotation.id = readId<AnnotationTag>(node, "id", report, "annotations");
        annotation.kind = parseAnnotationKind(readString(node, "kind", report, "annotations", "Text"));
        annotation.start = readVec2(node, "start", report, "annotations");
        annotation.end = readVec2(node, "end", report, "annotations");
        annotation.text = readString(node, "text", report, "annotations");
        annotation.color = readUInt(node, "color", report, "annotations", annotation.color);
        annotation.fillColor = readUInt(node, "fillColor", report, "annotations", annotation.fillColor);
        annotation.fontSize = static_cast<float>(readDouble(node, "fontSize", report, "annotations", 16.0));
        annotation.thickness = static_cast<float>(readDouble(node, "thickness", report, "annotations", 2.0));
        annotation.filled = readBool(node, "filled", report, "annotations", false);
        annotation.zOrder = static_cast<i32>(readInt(node, "zOrder", report, "annotations", 0));
        annotations.push_back(std::move(annotation));
    }
    project.setAnnotations(std::move(annotations));

    // --- Tests -------------------------------------------------------------
    std::vector<NetworkTest> tests;
    for (const Json& node : readArray(root, "tests", report, "tests")) {
        NetworkTest test;
        test.id = readId<TestTag>(node, "id", report, "tests");
        test.name = readString(node, "name", report, "tests", "Test");
        test.description = readString(node, "description", report, "tests");
        test.source = readOptionalId<DeviceTag>(node, "source");
        test.destinationDevice = readOptionalId<DeviceTag>(node, "destinationDevice");
        test.destinationAddress = readIpv4(node, "destinationAddress", report, "tests");
        test.expectation = parseExpectation(readString(node, "expectation", report, "tests", "reachable"));
        test.probeCount = readUInt(node, "probeCount", report, "tests", 3);
        test.timeout = readDuration(node, "timeoutNs", report, "tests", seconds(2));
        test.payloadSize = static_cast<std::size_t>(readInt(node, "payloadSize", report, "tests", 32));
        test.enabled = readBool(node, "enabled", report, "tests", true);
        tests.push_back(std::move(test));
    }
    project.setTests(std::move(tests));

    // --- Finish ------------------------------------------------------------
    network.rebuildIndices();
    network.refreshOperationalStates();
    project.pruneDanglingReferences();

    result.warnings = report.warnings();
    for (const std::string& issue : report.errors()) result.warnings.push_back(issue);

    if (!result.warnings.empty()) {
        logging::warning("serialization", "project loaded with {} issue(s)", result.warnings.size());
    }
    return result;
}

} // namespace tnp::serial
