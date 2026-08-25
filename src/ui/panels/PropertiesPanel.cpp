#include "commands/DeviceCommands.h"
#include "commands/InterfaceCommands.h"
#include "commands/LinkCommands.h"
#include "commands/ProjectCommands.h"
#include "commands/RoutingCommands.h"
#include "core/devices/DhcpServer.h"
#include "core/devices/DnsServer.h"
#include "core/devices/FirewallPolicy.h"
#include "core/devices/Ipv4Stack.h"
#include "core/devices/SwitchingEngine.h"
#include "ui/Icons.h"
#include "ui/panels/PanelSupport.h"
#include "ui/panels/Panels.h"

#include <format>

namespace tnp::ui {

using namespace core;

namespace {

/// Applies an edited snapshot as one undoable change.
void applySettings(UiContext& context, const Device& device, const Interface& iface,
                   commands::InterfaceSettings settings) {
    if (!context.application.commands().run(std::make_unique<commands::ConfigureInterfaceCommand>(
            device.id(), iface.id(), std::move(settings)))) {
        const std::string& reason = context.application.commands().lastFailure();
        if (!reason.empty()) context.setStatus(reason, true);
    }
}

const char* duplexLabel(DuplexMode mode) {
    switch (mode) {
        case DuplexMode::Auto: return "Auto";
        case DuplexMode::Half: return "Half";
        case DuplexMode::Full: return "Full";
    }
    return "Auto";
}

} // namespace

// ---------------------------------------------------------------------------

void PropertiesPanel::draw(UiContext& context) {
    const app::Selection& selection = context.application.selection();
    core::Project& project = context.application.project();

    if (selection.empty()) {
        syncBuffers(ObjectRef{}, context);
        drawProjectProperties(context);
        return;
    }
    if (selection.size() > 1) {
        drawMultiSelection(context);
        return;
    }

    const ObjectRef subject = selection.primary();
    syncBuffers(subject, context);

    switch (subject.kind) {
        case ObjectKind::Device: {
            Device* device = project.network().findDevice(subject.asDeviceId());
            if (device == nullptr) {
                emptyState("This device no longer exists.");
                return;
            }
            drawDevice(context, *device);
            return;
        }
        case ObjectKind::Link: {
            Link* link = project.network().findLink(subject.asLinkId());
            if (link == nullptr) {
                emptyState("This link no longer exists.");
                return;
            }
            drawLink(context, *link);
            return;
        }
        case ObjectKind::Annotation: {
            Annotation* annotation = project.findAnnotation(AnnotationId{subject.id});
            if (annotation == nullptr) {
                emptyState("This annotation no longer exists.");
                return;
            }
            drawAnnotation(context, *annotation);
            return;
        }
        default:
            drawProjectProperties(context);
            return;
    }
}

void PropertiesPanel::syncBuffers(const ObjectRef& subject, UiContext& context) {
    if (buffersValid_ && subject == bufferOwner_) return;
    bufferOwner_ = subject;
    buffersValid_ = true;

    nameBuffer_.clear();
    descriptionBuffer_.clear();
    textBuffer_.clear();
    authorBuffer_.clear();
    gatewayBuffer_.clear();
    newAddressBuffer_.clear();
    newRouteDestination_.clear();
    newRouteNextHop_.clear();

    core::Project& project = context.application.project();

    switch (subject.kind) {
        case ObjectKind::Device:
            if (const Device* device = project.network().findDevice(subject.asDeviceId())) {
                nameBuffer_ = device->name();
                descriptionBuffer_ = device->description();
                if (const Ipv4Stack* stack = device->ipv4Stack()) {
                    if (const auto gateway = stack->defaultGateway()) {
                        gatewayBuffer_ = gateway->toString();
                    }
                }
            }
            return;

        case ObjectKind::Link:
            if (const Link* link = project.network().findLink(subject.asLinkId())) {
                nameBuffer_ = link->label();
            }
            return;

        case ObjectKind::Annotation:
            if (const Annotation* annotation = project.findAnnotation(AnnotationId{subject.id})) {
                textBuffer_ = annotation->text;
            }
            return;

        default:
            // Nothing selected: the panel shows the project itself.
            nameBuffer_ = project.metadata().name;
            authorBuffer_ = project.metadata().author;
            descriptionBuffer_ = project.metadata().description;
            return;
    }
}

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------

void PropertiesPanel::drawDevice(UiContext& context, Device& device) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float glyph = ImGui::GetTextLineHeight() * 1.8f;

    drawDeviceGlyph(draw, device.type(), ImVec2(cursor.x, cursor.y), glyph,
                    deviceAccent(device.type()));
    ImGui::Dummy(ImVec2(glyph + 8.0f, glyph));
    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::TextUnformatted(device.name().c_str());
    subtleText(std::string{device.typeDisplayName()});
    ImGui::EndGroup();

    ImGui::Spacing();

    if (ImGui::Button("Open console")) {
        context.consoleDevice = device.id();
        context.application.shell().attachTo(device.id());
        context.showConsole = true;
    }

    ImGui::Spacing();

    if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##name", &nameBuffer_, ImGuiInputTextFlags_EnterReturnsTrue) ||
            ImGui::IsItemDeactivatedAfterEdit()) {
            if (!nameBuffer_.empty() && nameBuffer_ != device.name()) {
                context.application.commands().run(
                    std::make_unique<commands::RenameDeviceCommand>(device.id(), nameBuffer_));
            } else {
                nameBuffer_ = device.name();
            }
        }
        subtleText("Name");

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextMultiline("##description", &descriptionBuffer_, ImVec2(-1.0f, 52.0f))) {
            // Committed on focus loss so typing does not fill the undo stack.
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && descriptionBuffer_ != device.description()) {
            context.application.commands().run(
                std::make_unique<commands::SetDeviceDescriptionCommand>(device.id(),
                                                                        descriptionBuffer_));
        }
        subtleText("Description");
    }

    drawInterfaceList(context, device);
    drawRouting(context, device);
    drawSwitching(context, device);
    drawFirewall(context, device);
    drawServices(context, device);
    drawDiagnostics(context, ObjectRef::device(device.id()));
}

void PropertiesPanel::drawInterfaceList(UiContext& context, Device& device) {
    if (!ImGui::CollapsingHeader("Interfaces", ImGuiTreeNodeFlags_DefaultOpen)) return;

    for (const auto& iface : device.interfaces()) {
        ImGui::PushID(iface->id().toShortString().c_str());

        const bool operational = iface->isOperational();
        const ImU32 statusColor = !iface->isAdminUp() ? theme().textDisabled
                                  : operational       ? theme().success
                                                      : theme().warning;

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 dot = ImGui::GetCursorScreenPos();
        draw->AddCircleFilled(ImVec2(dot.x + 6.0f, dot.y + ImGui::GetTextLineHeight() * 0.5f + 3.0f),
                              4.0f, statusColor, 10);
        ImGui::Dummy(ImVec2(16.0f, 0.0f));
        ImGui::SameLine();

        const auto address = iface->primaryIpv4();
        const std::string summary =
            address ? address->toString()
                    : (iface->ipv4DhcpEnabled() ? std::string{"dhcp"} : std::string{"unassigned"});

        const bool open = ImGui::TreeNodeEx(
            "##iface", ImGuiTreeNodeFlags_SpanAvailWidth,
            "%s   %s", iface->shortName().c_str(), summary.c_str());

        if (ImGui::IsItemClicked()) context.inspectedInterface = iface->id();

        if (open) {
            drawInterface(context, device, *iface);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void PropertiesPanel::drawInterface(UiContext& context, Device& device, Interface& iface) {
    ImGui::Indent(6.0f);

    if (beginFieldTable("iface-info")) {
        fieldRow("Full name", iface.name());
        fieldRow("Type", std::string{interfaceTypeName(iface.type())});
        fieldRow("MAC", iface.macAddress().toString());
        fieldRow("Status", std::format("{} / {}", adminStateName(iface.adminState()),
                                       operationalStateName(iface.operationalState())));

        const Link* link = context.application.project().network().linkOfInterface(iface.id());
        if (link != nullptr) {
            const auto peer = link->peerOf(iface.id());
            const Device* other = peer ? context.application.project().network().findDevice(peer->device)
                                       : nullptr;
            const Interface* otherInterface =
                peer ? context.application.project().network().findInterface(peer->interface) : nullptr;
            fieldRow("Connected to",
                     other && otherInterface
                         ? std::format("{} {}", other->name(), otherInterface->shortName())
                         : std::string{"unknown"});
        } else {
            fieldRow("Connected to", "nothing");
        }

        const InterfaceCounters& counters = iface.counters();
        fieldRow("Frames", std::format("{} in / {} out", counters.framesReceived, counters.framesSent));
        fieldRow("Bytes", std::format("{} in / {} out", counters.bytesReceived, counters.bytesSent));
        ImGui::EndTable();
    }

    ImGui::Spacing();

    // --- Administrative state ---------------------------------------------
    bool adminUp = iface.isAdminUp();
    if (ImGui::Checkbox("Enabled", &adminUp)) {
        context.application.commands().run(std::make_unique<commands::SetInterfaceAdminStateCommand>(
            device.id(), iface.id(), adminUp ? AdminState::Up : AdminState::Down));
    }
    helpMarker("The equivalent of 'no shutdown'. A disabled interface passes no traffic and "
               "contributes no connected route.");

    // --- Addressing --------------------------------------------------------
    ImGui::SeparatorText("IPv4");

    bool dhcp = iface.ipv4DhcpEnabled();
    if (ImGui::Checkbox("Obtain an address automatically (DHCP)", &dhcp)) {
        commands::InterfaceSettings settings = commands::InterfaceSettings::capture(iface);
        settings.dhcp = dhcp;
        if (dhcp) settings.ipv4.clear();
        applySettings(context, device, iface, std::move(settings));
    }

    if (!iface.ipv4DhcpEnabled()) {
        for (const Ipv4Prefix& prefix : iface.ipv4Addresses()) {
            ImGui::PushID(prefix.toString().c_str());
            ImGui::TextUnformatted(prefix.toString().c_str());
            ImGui::SameLine();
            subtleText(std::format("({})", prefix.toNetworkString()));
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
            if (smallDangerButton("x")) {
                context.application.commands().run(
                    std::make_unique<commands::RemoveIpv4AddressCommand>(device.id(), iface.id(),
                                                                         prefix));
            }
            ImGui::PopID();
        }

        ImGui::SetNextItemWidth(-70.0f);
        const bool submitted = ImGui::InputTextWithHint("##new-address", "192.168.1.10/24",
                                                        &newAddressBuffer_,
                                                        ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Add") || submitted) && !newAddressBuffer_.empty()) {
            const auto prefix = Ipv4Prefix::parse(newAddressBuffer_);
            if (!prefix) {
                context.setStatus("Type an address in CIDR notation, such as 192.168.1.10/24", true);
            } else if (context.application.commands().run(
                           std::make_unique<commands::AddIpv4AddressCommand>(device.id(), iface.id(),
                                                                             *prefix))) {
                newAddressBuffer_.clear();
            } else {
                context.setStatus(context.application.commands().lastFailure(), true);
            }
        }
    } else {
        for (const Ipv4Prefix& prefix : iface.ipv4Addresses()) {
            coloredText(theme().textSubtle, std::format("{} (leased)", prefix.toString()));
        }
    }

    // --- Physical ----------------------------------------------------------
    ImGui::SeparatorText("Physical");

    int mtu = static_cast<int>(iface.mtu());
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("MTU", &mtu, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
        commands::InterfaceSettings settings = commands::InterfaceSettings::capture(iface);
        settings.mtu = static_cast<u32>(std::max(0, mtu));
        applySettings(context, device, iface, std::move(settings));
    }

    int speed = static_cast<int>(iface.speedMbps());
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("Mbit/s", &speed, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
        commands::InterfaceSettings settings = commands::InterfaceSettings::capture(iface);
        settings.speedMbps = static_cast<u64>(std::max(0, speed));
        applySettings(context, device, iface, std::move(settings));
    }

    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::BeginCombo("Duplex", duplexLabel(iface.duplex()))) {
        for (const DuplexMode mode : {DuplexMode::Auto, DuplexMode::Half, DuplexMode::Full}) {
            if (ImGui::Selectable(duplexLabel(mode), iface.duplex() == mode)) {
                commands::InterfaceSettings settings = commands::InterfaceSettings::capture(iface);
                settings.duplex = mode;
                applySettings(context, device, iface, std::move(settings));
            }
        }
        ImGui::EndCombo();
    }

    // --- VLAN --------------------------------------------------------------
    if (iface.isConnectable() || iface.type() == InterfaceType::Vlan) {
        ImGui::SeparatorText("VLAN");

        const bool trunk = iface.vlan().mode == VlanMode::Trunk;
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::BeginCombo("Mode", trunk ? "Trunk" : "Access")) {
            for (const VlanMode mode : {VlanMode::Access, VlanMode::Trunk}) {
                if (ImGui::Selectable(mode == VlanMode::Trunk ? "Trunk" : "Access",
                                      iface.vlan().mode == mode)) {
                    commands::InterfaceSettings settings = commands::InterfaceSettings::capture(iface);
                    settings.vlan.mode = mode;
                    applySettings(context, device, iface, std::move(settings));
                }
            }
            ImGui::EndCombo();
        }

        int vlanId = static_cast<int>(trunk ? iface.vlan().nativeVlan : iface.vlan().accessVlan);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt(trunk ? "Native VLAN" : "Access VLAN", &vlanId, 0, 0,
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
            commands::InterfaceSettings settings = commands::InterfaceSettings::capture(iface);
            const auto value = static_cast<VlanId>(std::clamp(vlanId, 1, 4094));
            if (trunk) settings.vlan.nativeVlan = value;
            else       settings.vlan.accessVlan = value;
            applySettings(context, device, iface, std::move(settings));
        }
    }

    drawDiagnostics(context, ObjectRef::iface(iface.id(), device.id()));
    ImGui::Unindent(6.0f);
    ImGui::Spacing();
}

// ---------------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------------

void PropertiesPanel::drawRouting(UiContext& context, Device& device) {
    Ipv4Stack* stack = device.ipv4Stack();
    if (stack == nullptr) return;
    if (!ImGui::CollapsingHeader("Routing")) return;

    if (beginFieldTable("routing-info")) {
        fieldRow("Forwarding", stack->forwardingEnabled() ? "enabled" : "disabled");
        fieldRow("Default gateway", stack->defaultGateway() ? stack->defaultGateway()->toString()
                                                            : std::string{"none"});
        fieldRow("Installed routes", std::to_string(stack->routingTable().size()));
        ImGui::EndTable();
    }

    // --- Default gateway ---------------------------------------------------
    ImGui::SeparatorText("Default gateway");

    ImGui::SetNextItemWidth(-70.0f);
    const bool submitted = ImGui::InputTextWithHint("##gateway", "192.168.1.1", &gatewayBuffer_,
                                                    ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("Set") || submitted) {
        const std::string& gateway = gatewayBuffer_;
        if (gateway.empty()) {
            context.application.commands().run(
                std::make_unique<commands::SetDefaultGatewayCommand>(device.id(), std::nullopt));
        } else if (const auto address = Ipv4Address::parse(gateway)) {
            context.application.commands().run(
                std::make_unique<commands::SetDefaultGatewayCommand>(device.id(), *address));
        } else {
            context.setStatus(std::format("'{}' is not an IPv4 address", gateway), true);
        }
    }

    // --- Static routes -----------------------------------------------------
    ImGui::SeparatorText("Static routes");

    for (const StaticRouteEntry& entry : stack->staticRoutes()) {
        ImGui::PushID(entry.id.toShortString().c_str());
        ImGui::TextUnformatted(std::format("{} via {}", entry.destination.toNetworkString(),
                                           entry.nextHop ? entry.nextHop->toString()
                                                         : std::string{"interface"})
                                   .c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
        if (smallDangerButton("x")) {
            context.application.commands().run(
                std::make_unique<commands::RemoveStaticRouteCommand>(device.id(), entry.id));
        }
        ImGui::PopID();
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
    ImGui::InputTextWithHint("##route-destination", "10.0.0.0/8", &newRouteDestination_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
    ImGui::InputTextWithHint("##route-nexthop", "next hop", &newRouteNextHop_);
    ImGui::SameLine();

    if (ImGui::Button("Add##route")) {
        const auto destination = Ipv4Prefix::parse(newRouteDestination_);
        const auto nextHop = Ipv4Address::parse(newRouteNextHop_);

        if (!destination || !nextHop) {
            context.setStatus("A route needs a destination prefix and a next-hop address", true);
        } else {
            StaticRouteEntry entry;
            entry.destination = *destination;
            entry.nextHop = *nextHop;

            if (context.application.commands().run(
                    std::make_unique<commands::AddStaticRouteCommand>(device.id(), entry))) {
                newRouteDestination_.clear();
                newRouteNextHop_.clear();
            } else {
                context.setStatus(context.application.commands().lastFailure(), true);
            }
        }
    }

    // --- The live table ----------------------------------------------------
    ImGui::SeparatorText("Forwarding table");

    if (stack->routingTable().empty()) {
        subtleText("No routes are installed.");
    } else if (ImGui::BeginTable("routes", 4,
                                 ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Destination");
        ImGui::TableSetupColumn("Next hop");
        ImGui::TableSetupColumn("Interface");
        ImGui::TableSetupColumn("Source");
        ImGui::TableHeadersRow();

        for (const Route& route : stack->routingTable().routes()) {
            const Interface* egress = device.findInterface(route.egressInterface);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(route.destination.toNetworkString().c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(route.nextHop ? route.nextHop->toString().c_str() : "connected");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(egress ? egress->shortName().c_str() : "?");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(std::string{routeSourceName(route.source)}.c_str());
        }
        ImGui::EndTable();
    }

    // --- OSPF ---------------------------------------------------------------
    if (stack->ospf().enabled) {
        ImGui::SeparatorText("OSPF");
        ImGui::PushStyleColor(ImGuiCol_Text, theme().warning);
        ImGui::TextWrapped("OSPF is configured (process %u). This build stores the configuration "
                           "and reports it, but does not form adjacencies or compute routes from it.",
                           stack->ospf().processId);
        ImGui::PopStyleColor();
    }
}

// ---------------------------------------------------------------------------
// Switching, firewall and services
// ---------------------------------------------------------------------------

void PropertiesPanel::drawSwitching(UiContext& context, Device& device) {
    SwitchingEngine* switching = device.switching();
    if (switching == nullptr) return;
    if (!ImGui::CollapsingHeader("Switching")) return;

    bool learning = switching->learningEnabled();
    if (ImGui::Checkbox("Learn source addresses", &learning)) {
        switching->setLearningEnabled(learning);
        context.application.project().touch();
    }
    helpMarker("With learning off, every frame is flooded - which is what a hub does.");

    ImGui::SeparatorText("VLANs");
    for (const VlanDefinition& vlan : switching->vlans()) {
        ImGui::TextUnformatted(std::format("{:<6} {}", vlan.id, vlan.name).c_str());
    }

    ImGui::SeparatorText("Forwarding database");
    const MacAddressTable& table = switching->macTable();

    if (table.empty()) {
        subtleText("Empty. Start the simulation and send traffic to populate it.");
    } else if (ImGui::BeginTable("mac-table", 3,
                                 ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("VLAN");
        ImGui::TableSetupColumn("MAC address");
        ImGui::TableSetupColumn("Port");
        ImGui::TableHeadersRow();

        for (const MacTableEntry& entry : table.entries()) {
            const Interface* port = device.findInterface(entry.port);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%u", entry.vlan);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.mac.toString().c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(port ? port->shortName().c_str() : "?");
        }
        ImGui::EndTable();
    }
}

void PropertiesPanel::drawFirewall(UiContext& context, Device& device) {
    FirewallPolicy* policy = device.firewallPolicy();
    if (policy == nullptr) return;
    if (!ImGui::CollapsingHeader("Firewall policy")) return;

    const bool permitByDefault = policy->defaultAction() == FirewallAction::Permit;
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::BeginCombo("Default action", permitByDefault ? "Permit" : "Deny")) {
        if (ImGui::Selectable("Permit", permitByDefault)) {
            policy->setDefaultAction(FirewallAction::Permit);
            context.application.project().touch();
        }
        if (ImGui::Selectable("Deny", !permitByDefault)) {
            policy->setDefaultAction(FirewallAction::Deny);
            context.application.project().touch();
        }
        ImGui::EndCombo();
    }
    helpMarker("Rules are evaluated in order and the first match decides. This action applies "
               "when nothing matched.");

    ImGui::SeparatorText("Rules");
    if (policy->rules().empty()) {
        subtleText("No rules. Everything follows the default action.");
        return;
    }

    if (!ImGui::BeginTable("firewall-rules", 3,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("Rule");
    ImGui::TableSetupColumn("Hits", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 26.0f);
    ImGui::TableHeadersRow();

    FirewallRuleId toRemove;
    for (const FirewallRule& rule : policy->rules()) {
        ImGui::PushID(rule.id.toShortString().c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        coloredText(rule.action == FirewallAction::Permit ? theme().success : theme().error,
                    rule.name.empty() ? rule.toString() : rule.name);
        if (!rule.name.empty() && ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted(rule.toString().c_str());
            ImGui::EndTooltip();
        }
        ImGui::TableNextColumn();
        ImGui::Text("%llu", static_cast<unsigned long long>(rule.hitCount));
        ImGui::TableNextColumn();
        if (smallDangerButton("x")) toRemove = rule.id;
        ImGui::PopID();
    }
    ImGui::EndTable();

    if (toRemove.isValid()) {
        policy->removeRule(toRemove);
        context.application.project().touch();
    }
}

void PropertiesPanel::drawServices(UiContext& context, Device& device) {
    DhcpServer* dhcp = device.dhcpServer();
    DnsServer* dns = device.dnsServer();
    if (dhcp == nullptr && dns == nullptr) return;
    if (!ImGui::CollapsingHeader("Services")) return;

    if (dhcp != nullptr) {
        ImGui::SeparatorText("DHCP server");

        bool enabled = dhcp->isEnabled();
        if (ImGui::Checkbox("Enabled##dhcp", &enabled)) {
            dhcp->setEnabled(enabled);
            context.application.project().touch();
            context.application.invalidateValidation();
        }

        for (const DhcpPool& pool : dhcp->pools()) {
            ImGui::BulletText("%s: %s from %s to %s", pool.name.c_str(),
                              pool.subnet.toNetworkString().c_str(), pool.rangeFirst.toString().c_str(),
                              pool.rangeLast.toString().c_str());
        }
        if (dhcp->pools().empty()) subtleText("No pool is configured.");

        const auto leases = dhcp->leases();
        if (!leases.empty()) {
            subtleText(std::format("{} lease(s) issued", leases.size()));
            for (const DhcpLease& lease : leases) {
                ImGui::BulletText("%s -> %s", lease.address.toString().c_str(),
                                  lease.client.toString().c_str());
            }
        }
    }

    if (dns != nullptr) {
        ImGui::SeparatorText("DNS server");

        bool enabled = dns->isEnabled();
        if (ImGui::Checkbox("Enabled##dns", &enabled)) {
            dns->setEnabled(enabled);
            context.application.project().touch();
        }

        DnsRecordId toRemove;
        for (const DnsRecord& record : dns->records()) {
            ImGui::PushID(record.id.toShortString().c_str());
            ImGui::TextUnformatted(std::format("{} -> {}", record.name, record.address.toString()).c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
            if (smallDangerButton("x")) toRemove = record.id;
            ImGui::PopID();
        }
        if (toRemove.isValid()) {
            dns->removeRecord(toRemove);
            context.application.project().touch();
        }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        ImGui::InputTextWithHint("##record-name", "host.local", &newRecordName_);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
        ImGui::InputTextWithHint("##record-address", "10.0.0.5", &newRecordAddress_);
        ImGui::SameLine();
        if (ImGui::Button("Add##record")) {
            const auto address = Ipv4Address::parse(newRecordAddress_);
            if (newRecordName_.empty() || !address) {
                context.setStatus("A record needs a name and an IPv4 address", true);
            } else {
                dns->addRecord(DnsRecord{DnsRecordId::generate(), newRecordName_, *address, 300});
                context.application.project().touch();
                newRecordName_.clear();
                newRecordAddress_.clear();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Diagnostics, links, annotations
// ---------------------------------------------------------------------------

void PropertiesPanel::drawDiagnostics(UiContext& context, const ObjectRef& subject) {
    const auto issues = context.application.validationReport().forObject(subject);
    if (issues.empty()) return;

    if (!ImGui::CollapsingHeader("Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) return;

    for (const validation::ValidationIssue& issue : issues) {
        coloredText(severityColor(issue.severity),
                    std::format("{} {}", severityGlyph(issue.severity), issue.message));
        if (!issue.suggestion.empty()) {
            ImGui::Indent(14.0f);
            subtleText(issue.suggestion);
            ImGui::Unindent(14.0f);
        }
    }
}

void PropertiesPanel::drawLink(UiContext& context, Link& link) {
    const Network& network = context.application.project().network();

    const Device* deviceA = network.findDevice(link.endpointA().device);
    const Device* deviceB = network.findDevice(link.endpointB().device);
    const Interface* interfaceA = network.findInterface(link.endpointA().interface);
    const Interface* interfaceB = network.findInterface(link.endpointB().interface);

    ImGui::TextUnformatted("Link");
    subtleText(std::format("{} {} - {} {}", deviceA ? deviceA->name() : "?",
                           interfaceA ? interfaceA->shortName() : "?", deviceB ? deviceB->name() : "?",
                           interfaceB ? interfaceB->shortName() : "?"));
    ImGui::Spacing();

    if (beginFieldTable("link-info")) {
        fieldRow("Medium", std::string{linkMediumName(link.medium())});
        fieldRow("Bandwidth", std::format("{} Mbit/s", link.bandwidthMbps()));
        fieldRow("Propagation delay", formatDuration(link.propagationDelay()));
        ImGui::EndTable();
    }

    ImGui::Spacing();

    bool enabled = link.isEnabled();
    if (ImGui::Checkbox("Enabled", &enabled)) {
        context.application.commands().run(
            std::make_unique<commands::SetLinkEnabledCommand>(link.id(), enabled));
    }
    helpMarker("Disabling a link is how a cable fault is simulated: the link stays in the project "
               "but carries nothing.");

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##label", "Label", &nameBuffer_,
                                 ImGuiInputTextFlags_EnterReturnsTrue) ||
        ImGui::IsItemDeactivatedAfterEdit()) {
        if (nameBuffer_ != link.label()) {
            context.application.commands().run(std::make_unique<commands::ConfigureLinkCommand>(
                link.id(), nameBuffer_, link.propagationDelay(), link.bandwidthMbps()));
        }
    }
    subtleText("Label shown on the canvas");

    int delayMicroseconds = static_cast<int>(
        std::chrono::duration_cast<std::chrono::microseconds>(link.propagationDelay()).count());
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::InputInt("Delay (us)", &delayMicroseconds, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue)) {
        context.application.commands().run(std::make_unique<commands::ConfigureLinkCommand>(
            link.id(), link.label(), microseconds(std::max(0, delayMicroseconds)),
            link.bandwidthMbps()));
    }

    if (ImGui::Button("Disconnect")) {
        context.application.commands().run(
            std::make_unique<commands::DisconnectLinksCommand>(std::vector<LinkId>{link.id()}));
        context.application.selection().clear();
    }

    drawDiagnostics(context, ObjectRef::link(link.id()));
}

void PropertiesPanel::drawAnnotation(UiContext& context, Annotation& annotation) {
    ImGui::TextUnformatted(std::string{annotationKindName(annotation.kind)}.c_str());
    ImGui::Spacing();

    Annotation edited = annotation;
    bool changed = false;

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextMultiline("##text", &textBuffer_, ImVec2(-1.0f, 60.0f))) {
        // Committed on focus loss.
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && textBuffer_ != annotation.text) {
        edited.text = textBuffer_;
        changed = true;
    }
    subtleText("Text");

    ImVec4 color = ImGui::ColorConvertU32ToFloat4(annotation.color);
    if (ImGui::ColorEdit4("Colour", &color.x, ImGuiColorEditFlags_NoInputs)) {
        edited.color = ImGui::ColorConvertFloat4ToU32(color);
        changed = true;
    }

    if (annotation.kind != AnnotationKind::Text && annotation.kind != AnnotationKind::Arrow) {
        bool filled = annotation.filled;
        if (ImGui::Checkbox("Filled", &filled)) {
            edited.filled = filled;
            changed = true;
        }
        ImVec4 fill = ImGui::ColorConvertU32ToFloat4(annotation.fillColor);
        if (ImGui::ColorEdit4("Fill", &fill.x, ImGuiColorEditFlags_NoInputs)) {
            edited.fillColor = ImGui::ColorConvertFloat4ToU32(fill);
            changed = true;
        }
    }

    float fontSize = annotation.fontSize;
    if (ImGui::SliderFloat("Text size", &fontSize, 8.0f, 48.0f, "%.0f")) {
        edited.fontSize = fontSize;
        changed = true;
    }

    int zOrder = annotation.zOrder;
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("Draw order", &zOrder, 1, 1)) {
        edited.zOrder = zOrder;
        changed = true;
    }
    helpMarker("Lower values are drawn first. A network label usually belongs behind everything.");

    if (changed) {
        context.application.commands().run(
            std::make_unique<commands::UpdateAnnotationCommand>(edited));
    }

    ImGui::Spacing();
    if (ImGui::Button("Delete")) {
        context.application.commands().run(std::make_unique<commands::DeleteAnnotationsCommand>(
            std::vector<AnnotationId>{annotation.id}));
        context.application.selection().clear();
    }
}

void PropertiesPanel::drawMultiSelection(UiContext& context) {
    const app::Selection& selection = context.application.selection();

    ImGui::TextUnformatted(std::format("{} objects selected", selection.size()).c_str());
    subtleText(std::format("{} device(s), {} link(s), {} annotation(s)", selection.devices().size(),
                           selection.links().size(), selection.annotations().size()));

    ImGui::Spacing();
    if (ImGui::Button("Delete selection")) context.application.deleteSelection();
}

void PropertiesPanel::drawProjectProperties(UiContext& context) {
    core::Project& project = context.application.project();

    ImGui::TextUnformatted("Project");
    subtleText("Nothing is selected, so this shows the project itself.");
    ImGui::Spacing();

    ProjectMetadata edited = project.metadata();
    bool changed = false;

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##project-name", &nameBuffer_);
    if (ImGui::IsItemDeactivatedAfterEdit() && nameBuffer_ != edited.name) {
        edited.name = nameBuffer_;
        changed = true;
    }
    subtleText("Name");

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##project-author", &authorBuffer_);
    if (ImGui::IsItemDeactivatedAfterEdit() && authorBuffer_ != edited.author) {
        edited.author = authorBuffer_;
        changed = true;
    }
    subtleText("Author");

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextMultiline("##project-description", &descriptionBuffer_, ImVec2(-1.0f, 70.0f));
    if (ImGui::IsItemDeactivatedAfterEdit() && descriptionBuffer_ != edited.description) {
        edited.description = descriptionBuffer_;
        changed = true;
    }
    subtleText("Description");

    if (changed) {
        context.application.commands().run(
            std::make_unique<commands::SetProjectMetadataCommand>(edited));
    }

    ImGui::Spacing();
    if (beginFieldTable("project-info")) {
        fieldRow("Devices", std::to_string(project.network().deviceCount()));
        fieldRow("Links", std::to_string(project.network().linkCount()));
        fieldRow("Annotations", std::to_string(project.annotations().size()));
        fieldRow("Tests", std::to_string(project.tests().size()));
        fieldRow("Format version", project.metadata().version.toString());
        fieldRow("Created", project.metadata().createdAt);
        fieldRow("Modified", project.metadata().modifiedAt);
        ImGui::EndTable();
    }
}

} // namespace tnp::ui
