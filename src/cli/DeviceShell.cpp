#include "cli/DeviceShell.h"

#include "commands/DeviceCommands.h"
#include "commands/InterfaceCommands.h"
#include "commands/RoutingCommands.h"
#include "core/devices/DhcpServer.h"
#include "core/devices/Ipv4Stack.h"
#include "core/devices/SwitchingEngine.h"
#include "utilities/StringUtilities.h"

#include <algorithm>
#include <format>

namespace tnp::cli {
namespace {

using namespace core;

constexpr std::size_t kMaxHistory = 200;

/// Matches a word against a command name, accepting unambiguous abbreviations
/// the way a network CLI does ("sh ip ro").
bool is(std::string_view word, std::string_view command) {
    return strings::isAbbreviation(word, command);
}

std::string word(const std::vector<std::string>& words, std::size_t index) {
    return index < words.size() ? words[index] : std::string{};
}

} // namespace

DeviceShell::DeviceShell(Project& project, sim::Simulator& simulator,
                         commands::CommandManager& commands)
    : project_(project), simulator_(simulator), commands_(commands) {
    traceToken_ = simulator_.addTraceObserver([this](const TraceEvent& event) { onTrace(event); });
}

DeviceShell::~DeviceShell() { simulator_.removeTraceObserver(traceToken_); }

Device* DeviceShell::device() const { return project_.network().findDevice(device_); }

bool DeviceShell::isAttached() const { return device() != nullptr; }

void DeviceShell::attachTo(DeviceId device) {
    device_ = device;
    mode_ = ShellMode::Exec;
    configInterface_ = InterfaceId{};
}

std::string DeviceShell::prompt() const {
    const Device* target = device();
    if (target == nullptr) return "(no device)>";

    switch (mode_) {
        case ShellMode::Exec:      return target->name() + "#";
        case ShellMode::Configure: return target->name() + "(config)#";
        case ShellMode::ConfigInterface: return target->name() + "(config-if)#";
    }
    return target->name() + "#";
}

// ---------------------------------------------------------------------------
// Asynchronous output
// ---------------------------------------------------------------------------

void DeviceShell::onTrace(const TraceEvent& event) {
    if (event.device != device_) return;

    switch (event.kind) {
        case TraceKind::PingReplyReceived:
        case TraceKind::PingTimedOut:
        case TraceKind::PingFinished:
        case TraceKind::IcmpDestinationUnreachableReceived:
        case TraceKind::IcmpTimeExceededReceived:
        case TraceKind::DhcpLeaseAssigned:
            pending_.push_back(ShellLine{event.summary, false, true});
            return;
        case TraceKind::ArpTimedOut:
        case TraceKind::IpNoRouteToHost:
            pending_.push_back(ShellLine{event.summary, true, true});
            return;
        default:
            return;
    }
}

std::vector<ShellLine> DeviceShell::drainEvents() {
    std::vector<ShellLine> lines;
    lines.swap(pending_);
    return lines;
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

ShellResponse DeviceShell::execute(std::string_view line) {
    ShellResponse response;

    const std::string trimmed = strings::trim(line);
    if (!trimmed.empty()) {
        history_.push_back(trimmed);
        while (history_.size() > kMaxHistory) history_.pop_front();
    }
    if (trimmed.empty()) return response;

    if (device() == nullptr) {
        response.addError("No device is selected. Choose one on the canvas first.");
        return response;
    }

    const Words words = strings::tokenize(trimmed);
    if (words.empty()) return response;

    switch (mode_) {
        case ShellMode::Exec:            return executeExec(words);
        case ShellMode::Configure:       return executeConfigure(words);
        case ShellMode::ConfigInterface: return executeConfigInterface(words);
    }
    return response;
}

ShellResponse DeviceShell::executeExec(const Words& words) {
    ShellResponse response;
    const std::string& command = words[0];

    if (command == "?" || is(command, "help")) {
        response.add("Available commands:");
        response.add("  show ...                 inspect this device");
        response.add("  ping <address|device>    send ICMP echo requests");
        response.add("  clear arp                empty the ARP cache");
        response.add("  clear mac address-table  empty the forwarding database");
        response.add("  configure terminal       enter configuration mode");
        response.add("  exit                     leave the current mode");
        response.add("Type 'show ?' for the list of show commands.");
        return response;
    }
    if (is(command, "show")) return showCommand(words);
    if (is(command, "ping")) return pingCommand(words);
    if (is(command, "clear")) return clearCommand(words);

    if (is(command, "configure")) {
        mode_ = ShellMode::Configure;
        response.add("Enter configuration commands, one per line. End with 'exit'.");
        return response;
    }
    if (is(command, "exit") || is(command, "end")) {
        response.add("Already at the top level.");
        return response;
    }

    response.addError(std::format("Unknown command: {}", command));
    return response;
}

ShellResponse DeviceShell::executeConfigure(const Words& words) {
    ShellResponse response;
    Device* target = device();
    const std::string& command = words[0];

    if (command == "?" || is(command, "help")) {
        response.add("Configuration commands:");
        response.add("  hostname <name>");
        response.add("  interface <name>");
        response.add("  ip route <prefix> <mask|/length> <next-hop>");
        response.add("  no ip route <prefix> <mask|/length>");
        response.add("  ip default-gateway <address>");
        response.add("  vlan <id> [name <name>]");
        response.add("  exit");
        return response;
    }

    if (is(command, "exit") || is(command, "end")) {
        mode_ = ShellMode::Exec;
        return response;
    }

    if (is(command, "hostname")) {
        const std::string name = word(words, 1);
        if (name.empty()) {
            response.addError("Usage: hostname <name>");
            return response;
        }
        if (!commands_.run(std::make_unique<commands::RenameDeviceCommand>(device_, name))) {
            response.addError("The name is unchanged or invalid.");
        }
        return response;
    }

    if (is(command, "interface")) {
        const std::string name = word(words, 1);
        Interface* iface = target->findInterfaceByName(name);
        if (iface == nullptr) {
            response.addError(std::format("No such interface: {}", name));
            return response;
        }
        configInterface_ = iface->id();
        mode_ = ShellMode::ConfigInterface;
        return response;
    }

    const bool negated = is(command, "no");
    const std::size_t base = negated ? 1 : 0;

    if (is(word(words, base), "ip") && is(word(words, base + 1), "route")) {
        const auto destination = [&]() -> std::optional<Ipv4Prefix> {
            const std::string prefixText = word(words, base + 2);
            const std::string maskText = word(words, base + 3);
            if (prefixText.find('/') != std::string::npos) return Ipv4Prefix::parse(prefixText);
            return Ipv4Prefix::parseWithMask(prefixText, maskText);
        }();

        if (!destination) {
            response.addError("Usage: ip route <prefix> <mask|/length> <next-hop>");
            return response;
        }

        if (negated) {
            const Ipv4Stack* stack = target->ipv4Stack();
            if (stack == nullptr) {
                response.addError("This device does not route.");
                return response;
            }
            const auto& routes = stack->staticRoutes();
            const auto it = std::find_if(routes.begin(), routes.end(),
                                         [&](const StaticRouteEntry& entry) {
                                             return entry.destination.network() == destination->network();
                                         });
            if (it == routes.end()) {
                response.addError("No such route.");
                return response;
            }
            commands_.run(std::make_unique<commands::RemoveStaticRouteCommand>(device_, it->id));
            return response;
        }

        // The next hop is the last word, whichever notation the prefix used.
        const auto nextHop = Ipv4Address::parse(words.back());
        if (!nextHop) {
            response.addError("Usage: ip route <prefix> <mask|/length> <next-hop>");
            return response;
        }

        StaticRouteEntry entry;
        entry.destination = *destination;
        entry.nextHop = *nextHop;

        if (!commands_.run(std::make_unique<commands::AddStaticRouteCommand>(device_, entry))) {
            const std::string& reason = commands_.lastFailure();
            response.addError(reason.empty() ? "The route could not be added." : reason);
        }
        return response;
    }

    if (is(word(words, base), "ip") && is(word(words, base + 1), "default-gateway")) {
        if (negated) {
            commands_.run(std::make_unique<commands::SetDefaultGatewayCommand>(device_, std::nullopt));
            return response;
        }
        const auto gateway = Ipv4Address::parse(word(words, base + 2));
        if (!gateway) {
            response.addError("Usage: ip default-gateway <address>");
            return response;
        }
        commands_.run(std::make_unique<commands::SetDefaultGatewayCommand>(device_, *gateway));
        return response;
    }

    if (is(command, "vlan")) {
        SwitchingEngine* switching = target->switching();
        if (switching == nullptr) {
            response.addError("This device does not switch.");
            return response;
        }
        const auto id = strings::parseUInt(word(words, 1));
        if (!id || !isValidVlanId(static_cast<VlanId>(*id))) {
            response.addError(std::format("VLAN id must be between {} and {}.", kMinVlanId, kMaxVlanId));
            return response;
        }
        std::string name = std::format("VLAN{:04}", *id);
        if (words.size() >= 4 && is(words[2], "name")) name = words[3];

        // VLAN database edits are not undoable yet; see docs/ROADMAP.md.
        switching->addVlan(VlanDefinition{static_cast<VlanId>(*id), name});
        project_.touch();
        return response;
    }

    response.addError(std::format("Unknown configuration command: {}", command));
    return response;
}

ShellResponse DeviceShell::executeConfigInterface(const Words& words) {
    ShellResponse response;
    Device* target = device();
    Interface* iface = target->findInterface(configInterface_);
    if (iface == nullptr) {
        mode_ = ShellMode::Configure;
        response.addError("The interface no longer exists.");
        return response;
    }

    const std::string& command = words[0];

    if (command == "?" || is(command, "help")) {
        response.add("Interface commands:");
        response.add("  ip address <address> <mask>");
        response.add("  ip address <address>/<length>");
        response.add("  ip address dhcp");
        response.add("  no ip address");
        response.add("  description <text>");
        response.add("  mtu <bytes>");
        response.add("  switchport mode access|trunk");
        response.add("  switchport access vlan <id>");
        response.add("  shutdown | no shutdown");
        response.add("  exit");
        return response;
    }

    if (is(command, "exit") || is(command, "end")) {
        mode_ = ShellMode::Configure;
        configInterface_ = InterfaceId{};
        return response;
    }

    const bool negated = is(command, "no");
    const std::size_t base = negated ? 1 : 0;
    const std::string subject = word(words, base);

    if (is(subject, "shutdown")) {
        const AdminState state = negated ? AdminState::Up : AdminState::Down;
        if (!commands_.run(std::make_unique<commands::SetInterfaceAdminStateCommand>(
                device_, iface->id(), state))) {
            response.add("Already in that state.");
        }
        return response;
    }

    if (is(subject, "description")) {
        commands::InterfaceSettings settings = commands::InterfaceSettings::capture(*iface);
        settings.description.clear();
        if (!negated) {
            for (std::size_t i = base + 1; i < words.size(); ++i) {
                if (!settings.description.empty()) settings.description += ' ';
                settings.description += words[i];
            }
        }
        commands_.run(std::make_unique<commands::ConfigureInterfaceCommand>(device_, iface->id(),
                                                                           std::move(settings)));
        return response;
    }

    if (is(subject, "mtu")) {
        const auto value = strings::parseUInt(word(words, base + 1));
        if (!value) {
            response.addError("Usage: mtu <bytes>");
            return response;
        }
        commands::InterfaceSettings settings = commands::InterfaceSettings::capture(*iface);
        settings.mtu = *value;
        if (!commands_.run(std::make_unique<commands::ConfigureInterfaceCommand>(device_, iface->id(),
                                                                                std::move(settings)))) {
            const std::string& reason = commands_.lastFailure();
            response.addError(reason.empty() ? "The MTU is unchanged." : reason + ".");
        }
        return response;
    }

    if (is(subject, "ip") && is(word(words, base + 1), "address")) {
        commands::InterfaceSettings settings = commands::InterfaceSettings::capture(*iface);

        if (negated) {
            settings.ipv4.clear();
            settings.dhcp = false;
            commands_.run(std::make_unique<commands::ConfigureInterfaceCommand>(device_, iface->id(),
                                                                               std::move(settings)));
            return response;
        }

        const std::string addressText = word(words, base + 2);
        if (is(addressText, "dhcp")) {
            settings.dhcp = true;
            settings.ipv4.clear();
            commands_.run(std::make_unique<commands::ConfigureInterfaceCommand>(device_, iface->id(),
                                                                               std::move(settings)));
            response.add("The address will be requested when the simulation starts.");
            return response;
        }

        const auto prefix = addressText.find('/') != std::string::npos
                                ? Ipv4Prefix::parse(addressText)
                                : Ipv4Prefix::parseWithMask(addressText, word(words, base + 3));
        if (!prefix) {
            response.addError("Usage: ip address <address> <mask>  or  ip address <address>/<length>");
            return response;
        }

        if (!commands_.run(std::make_unique<commands::AddIpv4AddressCommand>(device_, iface->id(),
                                                                             *prefix))) {
            const std::string& reason = commands_.lastFailure();
            response.addError(reason.empty() ? "The address could not be added." : reason);
        }
        return response;
    }

    if (is(subject, "switchport")) {
        commands::InterfaceSettings settings = commands::InterfaceSettings::capture(*iface);

        if (is(word(words, base + 1), "mode")) {
            const std::string modeText = word(words, base + 2);
            if (is(modeText, "access"))      settings.vlan.mode = VlanMode::Access;
            else if (is(modeText, "trunk"))  settings.vlan.mode = VlanMode::Trunk;
            else {
                response.addError("Usage: switchport mode access|trunk");
                return response;
            }
        } else if (is(word(words, base + 1), "access") && is(word(words, base + 2), "vlan")) {
            const auto id = strings::parseUInt(word(words, base + 3));
            if (!id || !isValidVlanId(static_cast<VlanId>(*id))) {
                response.addError("Usage: switchport access vlan <id>");
                return response;
            }
            settings.vlan.mode = VlanMode::Access;
            settings.vlan.accessVlan = static_cast<VlanId>(*id);
        } else {
            response.addError("Usage: switchport mode access|trunk  or  switchport access vlan <id>");
            return response;
        }

        commands_.run(std::make_unique<commands::ConfigureInterfaceCommand>(device_, iface->id(),
                                                                           std::move(settings)));
        return response;
    }

    response.addError(std::format("Unknown interface command: {}", command));
    return response;
}

// ---------------------------------------------------------------------------
// show
// ---------------------------------------------------------------------------

ShellResponse DeviceShell::showCommand(const Words& words) {
    ShellResponse response;
    const std::string subject = word(words, 1);

    if (subject.empty() || subject == "?") {
        response.add("show interfaces [name]      interface state and counters");
        response.add("show ip interface brief     one line per interface");
        response.add("show ip route               the forwarding table");
        response.add("show arp                    the ARP cache");
        response.add("show mac address-table      the switching database");
        response.add("show vlan                   configured VLANs");
        response.add("show ip dhcp binding        DHCP leases handed out");
        response.add("show running-config         this device's configuration");
        response.add("show version                device summary");
        return response;
    }

    if (is(subject, "interfaces")) return showInterfaces(words);
    if (is(subject, "version")) return showVersion();
    if (is(subject, "arp")) return showArp();
    if (is(subject, "vlan")) return showVlan();
    if (is(subject, "running-config")) return showRunningConfig();

    if (is(subject, "mac")) return showMacTable();

    if (is(subject, "ip")) {
        const std::string what = word(words, 2);
        if (is(what, "route")) return showIpRoute();
        if (is(what, "arp")) return showArp();
        if (is(what, "interface")) return showIpInterfaceBrief();
        if (is(what, "dhcp")) return showDhcpBindings();
        response.addError("Try: show ip route | show ip interface brief | show ip arp | show ip dhcp binding");
        return response;
    }

    response.addError(std::format("Unknown show command: {}", subject));
    return response;
}

ShellResponse DeviceShell::showVersion() {
    ShellResponse response;
    const Device* target = device();

    response.add(std::format("{} - {}", target->name(), target->typeDisplayName()));
    response.add(std::format("Description: {}",
                             target->description().empty() ? "(none)" : target->description()));
    response.add(std::format("Interfaces:  {}", target->interfaceCount()));
    response.add(std::format("Identifier:  {}", target->id().toString()));

    if (const Ipv4Stack* stack = target->ipv4Stack()) {
        response.add(std::format("IPv4 forwarding: {}", stack->forwardingEnabled() ? "enabled" : "disabled"));
        response.add(std::format("Routing table:   {} entries", stack->routingTable().size()));
    }
    if (const SwitchingEngine* switching = target->switching()) {
        response.add(std::format("MAC table:       {} entries", switching->macTable().size()));
    }
    response.add(std::format("Simulation:      {} at {}",
                             sim::simulationStateName(simulator_.state()),
                             formatSimTime(simulator_.now())));
    return response;
}

ShellResponse DeviceShell::showInterfaces(const Words& words) {
    ShellResponse response;
    const Device* target = device();

    const std::string filter = word(words, 2);
    bool matched = false;

    for (const auto& iface : target->interfaces()) {
        if (!filter.empty() && target->findInterfaceByName(filter) != iface.get()) continue;
        matched = true;

        response.add(iface->statusSummary());
        response.add(std::format("  Hardware address {}, MTU {} bytes, {} Mbit/s, {} duplex",
                                 iface->macAddress().toString(), iface->mtu(), iface->speedMbps(),
                                 duplexModeName(iface->duplex())));
        if (!iface->description().empty()) {
            response.add(std::format("  Description: {}", iface->description()));
        }

        for (const Ipv4Prefix& prefix : iface->ipv4Addresses()) {
            response.add(std::format("  Internet address {} ({})", prefix.toString(),
                                     prefix.toNetworkString()));
        }
        if (iface->ipv4DhcpEnabled()) response.add("  Address acquired by DHCP");

        for (const Ipv6Prefix& prefix : iface->ipv6Addresses()) {
            response.add(std::format("  IPv6 address {}", prefix.toString()));
        }

        if (iface->vlan().mode == VlanMode::Access) {
            response.add(std::format("  Switchport: access, VLAN {}", iface->vlan().accessVlan));
        } else {
            response.add(std::format("  Switchport: trunk, native VLAN {}", iface->vlan().nativeVlan));
        }

        const InterfaceCounters& counters = iface->counters();
        response.add(std::format("  {} frames in ({} bytes), {} frames out ({} bytes), {} dropped",
                                 counters.framesReceived, counters.bytesReceived, counters.framesSent,
                                 counters.bytesSent, counters.framesDropped));
        response.add("");
    }

    if (!matched) response.addError(std::format("No such interface: {}", filter));
    return response;
}

ShellResponse DeviceShell::showIpInterfaceBrief() {
    ShellResponse response;
    const Device* target = device();

    response.add(std::format("{:<24} {:<18} {:<10} {}", "Interface", "IP-Address", "Status", "Protocol"));
    for (const auto& iface : target->interfaces()) {
        const auto address = iface->primaryIpv4();
        response.add(std::format("{:<24} {:<18} {:<10} {}", iface->name(),
                                 address ? address->address().toString()
                                         : (iface->ipv4DhcpEnabled() ? "dhcp" : "unassigned"),
                                 iface->isAdminUp() ? "up" : "admin down",
                                 operationalStateName(iface->operationalState())));
    }
    return response;
}

ShellResponse DeviceShell::showIpRoute() {
    ShellResponse response;
    const Device* target = device();

    const RoutingTable* table = target->routingTable();
    if (table == nullptr) {
        response.addError("This device has no routing table.");
        return response;
    }

    response.add("Codes: C - connected, S - static, D - DHCP, O - OSPF, R - RIP");
    response.add("");

    if (table->empty()) {
        response.add("No routes are installed.");
        return response;
    }

    response.add(std::format("{:<3} {:<20} {:<18} {:<24} {}", "", "Destination", "Next hop",
                             "Interface", "Distance/Metric"));

    for (const Route& route : table->routes()) {
        const char code = [&] {
            switch (route.source) {
                case RouteSource::Connected: return 'C';
                case RouteSource::Static:    return 'S';
                case RouteSource::Dhcp:      return 'D';
                case RouteSource::Ospf:      return 'O';
                case RouteSource::Rip:       return 'R';
            }
            return '?';
        }();

        const Interface* egress = target->findInterface(route.egressInterface);
        response.add(std::format("{:<3} {:<20} {:<18} {:<24} [{}/{}]", code,
                                 route.destination.toNetworkString(),
                                 route.nextHop ? route.nextHop->toString() : "directly connected",
                                 egress ? egress->shortName() : "?",
                                 route.administrativeDistance(), route.metric));
    }
    return response;
}

ShellResponse DeviceShell::showArp() {
    ShellResponse response;
    const Device* target = device();

    const ArpCache* cache = target->arpCache();
    if (cache == nullptr) {
        response.addError("This device has no ARP cache.");
        return response;
    }
    if (cache->empty()) {
        response.add("The ARP cache is empty.");
        return response;
    }

    response.add(std::format("{:<18} {:<20} {:<24} {}", "Address", "Hardware address", "Interface", "Age"));
    for (const ArpEntry& entry : cache->entries()) {
        const Interface* iface = target->findInterface(entry.interface);
        response.add(std::format("{:<18} {:<20} {:<24} {}", entry.address.toString(),
                                 entry.mac.toString(), iface ? iface->shortName() : "?",
                                 entry.isStatic ? std::string{"static"}
                                                : formatDuration(entry.age(simulator_.now()))));
    }
    return response;
}

ShellResponse DeviceShell::showMacTable() {
    ShellResponse response;
    const Device* target = device();

    const MacAddressTable* table = target->macTable();
    if (table == nullptr) {
        response.addError("This device does not switch.");
        return response;
    }
    if (table->empty()) {
        response.add("The forwarding database is empty.");
        return response;
    }

    response.add(std::format("{:<6} {:<20} {:<10} {}", "VLAN", "MAC address", "Type", "Port"));
    for (const MacTableEntry& entry : table->entries()) {
        const Interface* iface = target->findInterface(entry.port);
        response.add(std::format("{:<6} {:<20} {:<10} {}", entry.vlan, entry.mac.toString(),
                                 entry.isStatic ? "static" : "dynamic",
                                 iface ? iface->shortName() : "?"));
    }
    return response;
}

ShellResponse DeviceShell::showVlan() {
    ShellResponse response;
    const Device* target = device();

    const SwitchingEngine* switching = target->switching();
    if (switching == nullptr) {
        response.addError("This device does not switch.");
        return response;
    }

    response.add(std::format("{:<6} {:<20} {}", "VLAN", "Name", "Ports"));
    for (const VlanDefinition& vlan : switching->vlans()) {
        std::vector<std::string> ports;
        for (const auto& iface : target->interfaces()) {
            if (!iface->isConnectable()) continue;
            if (iface->vlan().allowsVlan(vlan.id)) ports.push_back(iface->shortName());
        }
        response.add(std::format("{:<6} {:<20} {}", vlan.id, vlan.name,
                                 ports.empty() ? std::string{"-"} : strings::join(ports, ", ")));
    }
    return response;
}

ShellResponse DeviceShell::showDhcpBindings() {
    ShellResponse response;
    const Device* target = device();

    const DhcpServer* server = target->dhcpServer();
    if (server == nullptr) {
        response.addError("This device has no DHCP server.");
        return response;
    }

    response.add(std::format("DHCP service is {}.", server->isEnabled() ? "enabled" : "disabled"));
    for (const DhcpPool& pool : server->pools()) {
        response.add(std::format("Pool '{}': {} from {} to {}", pool.name,
                                 pool.subnet.toNetworkString(), pool.rangeFirst.toString(),
                                 pool.rangeLast.toString()));
    }

    const auto leases = server->leases();
    if (leases.empty()) {
        response.add("No addresses are currently leased.");
        return response;
    }

    response.add("");
    response.add(std::format("{:<18} {:<20} {}", "Address", "Client", "Expires"));
    for (const DhcpLease& lease : leases) {
        response.add(std::format("{:<18} {:<20} {}", lease.address.toString(),
                                 lease.client.toString(), formatSimTime(lease.expiresAt)));
    }
    return response;
}

ShellResponse DeviceShell::showRunningConfig() {
    ShellResponse response;
    for (const std::string& line : runningConfiguration()) response.add(line);
    return response;
}

std::vector<std::string> DeviceShell::runningConfiguration() const {
    std::vector<std::string> lines;
    const Device* target = device();
    if (target == nullptr) return lines;

    lines.push_back("!");
    lines.push_back(std::format("hostname {}", target->name()));
    if (!target->description().empty()) {
        lines.push_back(std::format("! {}", target->description()));
    }
    lines.push_back("!");

    if (const SwitchingEngine* switching = target->switching()) {
        for (const VlanDefinition& vlan : switching->vlans()) {
            lines.push_back(std::format("vlan {}", vlan.id));
            lines.push_back(std::format(" name {}", vlan.name));
        }
        lines.push_back("!");
    }

    for (const auto& iface : target->interfaces()) {
        lines.push_back(std::format("interface {}", iface->name()));
        if (!iface->description().empty()) {
            lines.push_back(std::format(" description {}", iface->description()));
        }
        if (iface->ipv4DhcpEnabled()) {
            lines.push_back(" ip address dhcp");
        }
        for (const Ipv4Prefix& prefix : iface->ipv4Addresses()) {
            lines.push_back(std::format(" ip address {} {}", prefix.address().toString(),
                                        prefix.netmask().toString()));
        }
        for (const Ipv6Prefix& prefix : iface->ipv6Addresses()) {
            lines.push_back(std::format(" ipv6 address {}", prefix.toString()));
        }
        if (iface->isConnectable()) {
            if (iface->vlan().mode == VlanMode::Trunk) {
                lines.push_back(" switchport mode trunk");
                lines.push_back(std::format(" switchport trunk native vlan {}", iface->vlan().nativeVlan));
            } else if (iface->vlan().accessVlan != kDefaultVlan) {
                lines.push_back(" switchport mode access");
                lines.push_back(std::format(" switchport access vlan {}", iface->vlan().accessVlan));
            }
        }
        if (iface->mtu() != kDefaultMtu) lines.push_back(std::format(" mtu {}", iface->mtu()));
        if (!iface->isAdminUp()) lines.push_back(" shutdown");
        lines.push_back("!");
    }

    if (const Ipv4Stack* stack = target->ipv4Stack()) {
        for (const StaticRouteEntry& entry : stack->staticRoutes()) {
            if (!entry.nextHop) continue;
            if (entry.destination.prefixLength() == 0 && !stack->forwardingEnabled()) {
                lines.push_back(std::format("ip default-gateway {}", entry.nextHop->toString()));
                continue;
            }
            lines.push_back(std::format("ip route {} {} {}", entry.destination.networkAddress().toString(),
                                        entry.destination.netmask().toString(),
                                        entry.nextHop->toString()));
        }
        for (const Ipv4Address& server : stack->dnsServers()) {
            lines.push_back(std::format("ip name-server {}", server.toString()));
        }
        lines.push_back("!");
    }

    lines.push_back("end");
    return lines;
}

// ---------------------------------------------------------------------------
// ping and clear
// ---------------------------------------------------------------------------

std::optional<Ipv4Address> DeviceShell::resolveTarget(std::string_view text, std::string& problem) const {
    if (const auto address = Ipv4Address::parse(text)) return address;

    // A device name is an editor convenience, not name resolution: TNP has no
    // DNS client yet, so `ping server.local` is deliberately not accepted.
    if (const Device* named = project_.network().findDeviceByName(text)) {
        for (const auto& iface : named->interfaces()) {
            if (const auto address = iface->primaryIpv4()) return address->address();
        }
        problem = std::format("{} has no IPv4 address", named->name());
        return std::nullopt;
    }

    problem = std::format("'{}' is neither an IPv4 address nor a device in this project", text);
    return std::nullopt;
}

ShellResponse DeviceShell::pingCommand(const Words& words) {
    ShellResponse response;

    const std::string targetText = word(words, 1);
    if (targetText.empty()) {
        response.addError("Usage: ping <address|device> [count <n>]");
        return response;
    }

    std::string problem;
    const auto destination = resolveTarget(targetText, problem);
    if (!destination) {
        response.addError(problem);
        return response;
    }

    PingRequest request;
    request.destination = *destination;
    for (std::size_t i = 2; i + 1 < words.size(); ++i) {
        if (is(words[i], "count")) {
            if (const auto count = strings::parseUInt(words[i + 1])) request.count = *count;
        } else if (is(words[i], "size")) {
            if (const auto size = strings::parseUInt(words[i + 1])) request.payloadSize = *size;
        }
    }
    request.count = std::clamp<u32>(request.count, 1, 100);
    request.payloadSize = std::clamp<std::size_t>(request.payloadSize, 0, 1400);

    const auto ping = simulator_.ping(device_, request);
    if (!ping) {
        response.addError(ping.message());
        return response;
    }

    response.add(std::format("Pinging {} with {} bytes of data, {} time(s):", destination->toString(),
                             request.payloadSize, request.count));
    if (!simulator_.isRunning()) {
        response.add("The simulation is paused; press play or step to let the packets move.");
    }
    return response;
}

ShellResponse DeviceShell::clearCommand(const Words& words) {
    ShellResponse response;
    Device* target = device();
    const std::string subject = word(words, 1);

    if (is(subject, "arp")) {
        ArpCache* cache = target->arpCache();
        if (cache == nullptr) {
            response.addError("This device has no ARP cache.");
            return response;
        }
        cache->clear();
        response.add("ARP cache cleared.");
        return response;
    }

    if (is(subject, "mac")) {
        MacAddressTable* table = target->macTable();
        if (table == nullptr) {
            response.addError("This device does not switch.");
            return response;
        }
        table->clear();
        response.add("Forwarding database cleared.");
        return response;
    }

    response.addError("Usage: clear arp | clear mac address-table");
    return response;
}

// ---------------------------------------------------------------------------
// Completion
// ---------------------------------------------------------------------------

std::vector<std::string> DeviceShell::completions(std::string_view prefix) const {
    static const std::vector<std::string> execWords = {
        "show", "ping", "clear", "configure terminal", "exit", "help",
        "show interfaces", "show ip route", "show ip interface brief", "show arp",
        "show mac address-table", "show vlan", "show ip dhcp binding", "show running-config",
        "show version"};
    static const std::vector<std::string> configWords = {
        "hostname", "interface", "ip route", "ip default-gateway", "vlan", "exit"};
    static const std::vector<std::string> interfaceWords = {
        "ip address", "no ip address", "ip address dhcp", "description", "mtu",
        "switchport mode access", "switchport mode trunk", "switchport access vlan",
        "shutdown", "no shutdown", "exit"};

    const std::vector<std::string>& source = mode_ == ShellMode::Exec        ? execWords
                                             : mode_ == ShellMode::Configure ? configWords
                                                                             : interfaceWords;

    std::vector<std::string> matches;
    for (const std::string& candidate : source) {
        if (strings::isAbbreviation(prefix, candidate)) matches.push_back(candidate);
    }

    // In interface mode the interface names are worth completing too.
    if (mode_ == ShellMode::Configure && isAttached()) {
        for (const auto& iface : device()->interfaces()) {
            const std::string candidate = "interface " + iface->name();
            if (strings::isAbbreviation(prefix, candidate)) matches.push_back(candidate);
        }
    }
    return matches;
}

} // namespace tnp::cli
