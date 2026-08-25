#pragma once

#include "utilities/Types.h"

#include <optional>
#include <string>
#include <string_view>

namespace tnp::core {

/// Physical (or virtual) nature of a network interface.
enum class InterfaceType : u8 {
    Ethernet,           ///< 10 Mbit/s legacy Ethernet
    FastEthernet,       ///< 100 Mbit/s
    GigabitEthernet,    ///< 1 Gbit/s
    TenGigabitEthernet, ///< 10 Gbit/s
    Serial,             ///< point-to-point WAN link
    Wireless,           ///< 802.11 station or radio
    Loopback,           ///< always up, never attached to a link
    Vlan,               ///< switch virtual interface on a layer-3 switch
    Console             ///< management port, carries no user traffic
};

[[nodiscard]] std::string_view interfaceTypeName(InterfaceType type);

/// Cisco-style short prefix: "Gi", "Fa", "Se", "Lo"...
[[nodiscard]] std::string_view interfaceTypePrefix(InterfaceType type);

/// Nominal line rate in Mbit/s. Zero for types with no defined rate.
[[nodiscard]] u64 interfaceTypeDefaultSpeedMbps(InterfaceType type);

/// True for types that carry Ethernet frames and therefore have a MAC address.
[[nodiscard]] bool interfaceTypeIsEthernetLike(InterfaceType type);

/// True for interfaces that can be attached to a `Link`.
[[nodiscard]] bool interfaceTypeIsConnectable(InterfaceType type);

/// Whether two interface types may be joined by a link.
///
/// Ethernet variants interoperate (they negotiate down to the slower rate),
/// serial only pairs with serial, wireless only with wireless, and loopback or
/// console ports never take a link at all.
[[nodiscard]] bool interfaceTypesAreCompatible(InterfaceType a, InterfaceType b);

/// Parses a full name ("GigabitEthernet") or a prefix ("Gi", "gig").
[[nodiscard]] std::optional<InterfaceType> parseInterfaceType(std::string_view text);

} // namespace tnp::core
