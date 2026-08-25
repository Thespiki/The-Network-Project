#pragma once

#include "utilities/Types.h"

#include <optional>
#include <string>
#include <string_view>

namespace tnp::core {

/// The device kinds TNP can simulate.
///
/// New kinds are added here and registered in `DeviceRegistry`; nothing else in
/// the application switches on this enumeration to decide behaviour.
enum class DeviceType : u8 {
    Pc,
    Server,
    Router,
    Switch,
    Layer3Switch,
    Firewall,
    AccessPoint,
    Hub,
    Cloud
};

/// Palette grouping.
enum class DeviceCategory : u8 { Computers, Networking, Security, Wireless, Infrastructure };

[[nodiscard]] std::string_view deviceTypeName(DeviceType type);        ///< "Layer3Switch"
[[nodiscard]] std::string_view deviceTypeDisplayName(DeviceType type); ///< "Layer 3 Switch"
[[nodiscard]] std::string_view deviceTypeDescription(DeviceType type);
[[nodiscard]] DeviceCategory deviceTypeCategory(DeviceType type);
[[nodiscard]] std::string_view deviceCategoryName(DeviceCategory category);

/// Prefix used when auto-naming a new instance: "Router1", "PC3".
[[nodiscard]] std::string_view deviceTypeNamePrefix(DeviceType type);

[[nodiscard]] std::optional<DeviceType> parseDeviceType(std::string_view text);

/// True for devices that forward IPv4 between their interfaces.
[[nodiscard]] bool deviceTypeRoutes(DeviceType type);
/// True for devices that bridge Ethernet frames between ports.
[[nodiscard]] bool deviceTypeSwitches(DeviceType type);

} // namespace tnp::core
