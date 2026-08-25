#include "core/network/InterfaceType.h"

#include "utilities/StringUtilities.h"

#include <array>
#include <optional>

namespace tnp::core {
namespace {

struct TypeInfo {
    InterfaceType type;
    std::string_view name;
    std::string_view prefix;
    u64 speedMbps;
    bool ethernetLike;
    bool connectable;
};

constexpr std::array<TypeInfo, 9> kTypes = {{
    {InterfaceType::Ethernet,           "Ethernet",           "Et",   10,     true,  true},
    {InterfaceType::FastEthernet,       "FastEthernet",       "Fa",   100,    true,  true},
    {InterfaceType::GigabitEthernet,    "GigabitEthernet",    "Gi",   1000,   true,  true},
    {InterfaceType::TenGigabitEthernet, "TenGigabitEthernet", "Te",   10000,  true,  true},
    {InterfaceType::Serial,             "Serial",             "Se",   2,      false, true},
    {InterfaceType::Wireless,           "Wireless",           "Wlan", 300,    true,  true},
    {InterfaceType::Loopback,           "Loopback",           "Lo",   0,      false, false},
    {InterfaceType::Vlan,               "Vlan",               "Vl",   0,      true,  false},
    {InterfaceType::Console,            "Console",            "Con",  0,      false, false},
}};

const TypeInfo& info(InterfaceType type) {
    for (const auto& entry : kTypes) {
        if (entry.type == type) return entry;
    }
    return kTypes[0];
}

/// Ethernet variants form one family; the rest each pair only with themselves.
int compatibilityFamily(InterfaceType type) {
    switch (type) {
        case InterfaceType::Ethernet:
        case InterfaceType::FastEthernet:
        case InterfaceType::GigabitEthernet:
        case InterfaceType::TenGigabitEthernet: return 1;
        case InterfaceType::Serial:             return 2;
        case InterfaceType::Wireless:           return 3;
        default:                                return 0; // not connectable
    }
}

} // namespace

std::string_view interfaceTypeName(InterfaceType type) { return info(type).name; }
std::string_view interfaceTypePrefix(InterfaceType type) { return info(type).prefix; }
u64 interfaceTypeDefaultSpeedMbps(InterfaceType type) { return info(type).speedMbps; }
bool interfaceTypeIsEthernetLike(InterfaceType type) { return info(type).ethernetLike; }
bool interfaceTypeIsConnectable(InterfaceType type) { return info(type).connectable; }

bool interfaceTypesAreCompatible(InterfaceType a, InterfaceType b) {
    const int familyA = compatibilityFamily(a);
    return familyA != 0 && familyA == compatibilityFamily(b);
}

std::optional<InterfaceType> parseInterfaceType(std::string_view text) {
    const std::string trimmed = strings::trim(text);
    if (trimmed.empty()) return std::nullopt;

    for (const auto& entry : kTypes) {
        if (strings::equalsIgnoreCase(trimmed, entry.name)) return entry.type;
        if (strings::equalsIgnoreCase(trimmed, entry.prefix)) return entry.type;
    }
    // No two type names share a first letter, so any prefix match is unambiguous
    // and network engineers can type "gig", "fa" or even "s".
    for (const auto& entry : kTypes) {
        if (strings::isAbbreviation(trimmed, entry.name)) return entry.type;
    }
    return std::nullopt;
}

} // namespace tnp::core
