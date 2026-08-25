#include "core/network/DeviceType.h"

#include "utilities/StringUtilities.h"

#include <array>

namespace tnp::core {
namespace {

struct TypeInfo {
    DeviceType type;
    std::string_view name;
    std::string_view displayName;
    std::string_view prefix;
    DeviceCategory category;
    bool routes;
    bool switches;
    std::string_view description;
};

constexpr std::array<TypeInfo, 9> kTypes = {{
    {DeviceType::Pc, "Pc", "PC", "PC", DeviceCategory::Computers, false, false,
     "End host with an IPv4 stack, ARP cache, routing table and ping."},
    {DeviceType::Server, "Server", "Server", "Server", DeviceCategory::Computers, false, false,
     "End host that additionally offers DHCP, DNS and generic UDP services."},
    {DeviceType::Router, "Router", "Router", "Router", DeviceCategory::Networking, true, false,
     "Forwards IPv4 between interfaces using a longest-prefix-match routing table."},
    {DeviceType::Switch, "Switch", "Switch", "Switch", DeviceCategory::Networking, false, true,
     "Layer-2 bridge with MAC learning, flooding and per-VLAN forwarding."},
    {DeviceType::Layer3Switch, "Layer3Switch", "Layer 3 Switch", "L3SW", DeviceCategory::Networking, true, true,
     "Switches within a VLAN and routes between VLAN interfaces."},
    {DeviceType::Firewall, "Firewall", "Firewall", "FW", DeviceCategory::Security, true, false,
     "Routes IPv4 subject to an ordered permit/deny policy."},
    {DeviceType::AccessPoint, "AccessPoint", "Access Point", "AP", DeviceCategory::Wireless, false, true,
     "Bridges a wireless segment onto the wired network."},
    {DeviceType::Hub, "Hub", "Hub", "Hub", DeviceCategory::Infrastructure, false, false,
     "Repeats every received frame to all other ports; one collision domain."},
    {DeviceType::Cloud, "Cloud", "Cloud", "Cloud", DeviceCategory::Infrastructure, true, false,
     "Stands in for an external network or the Internet."},
}};

const TypeInfo& info(DeviceType type) {
    for (const auto& entry : kTypes) {
        if (entry.type == type) return entry;
    }
    return kTypes[0];
}

} // namespace

std::string_view deviceTypeName(DeviceType type) { return info(type).name; }
std::string_view deviceTypeDisplayName(DeviceType type) { return info(type).displayName; }
std::string_view deviceTypeDescription(DeviceType type) { return info(type).description; }
DeviceCategory deviceTypeCategory(DeviceType type) { return info(type).category; }
std::string_view deviceTypeNamePrefix(DeviceType type) { return info(type).prefix; }
bool deviceTypeRoutes(DeviceType type) { return info(type).routes; }
bool deviceTypeSwitches(DeviceType type) { return info(type).switches; }

std::string_view deviceCategoryName(DeviceCategory category) {
    switch (category) {
        case DeviceCategory::Computers:      return "Computers";
        case DeviceCategory::Networking:     return "Networking";
        case DeviceCategory::Security:       return "Security";
        case DeviceCategory::Wireless:       return "Wireless";
        case DeviceCategory::Infrastructure: return "Infrastructure";
    }
    return "Networking";
}

std::optional<DeviceType> parseDeviceType(std::string_view text) {
    const std::string trimmed = strings::trim(text);
    for (const auto& entry : kTypes) {
        if (strings::equalsIgnoreCase(trimmed, entry.name)) return entry.type;
        if (strings::equalsIgnoreCase(trimmed, entry.displayName)) return entry.type;
    }
    return std::nullopt;
}

} // namespace tnp::core
