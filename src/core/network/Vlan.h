#pragma once

#include "utilities/Types.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::core {

/// 802.1Q VLAN identifier. 0 and 4095 are reserved by the standard.
using VlanId = u16;

inline constexpr VlanId kDefaultVlan = 1;
inline constexpr VlanId kMinVlanId = 1;
inline constexpr VlanId kMaxVlanId = 4094;

[[nodiscard]] constexpr bool isValidVlanId(VlanId vlan) {
    return vlan >= kMinVlanId && vlan <= kMaxVlanId;
}

/// How a switch port treats VLAN tags.
enum class VlanMode : u8 {
    Access, ///< untagged member of exactly one VLAN
    Trunk   ///< tagged member of several VLANs, one of which may be native
};

[[nodiscard]] std::string_view vlanModeName(VlanMode mode);

/// Per-interface VLAN configuration.
///
/// Present on every interface even when the owning device does no switching:
/// keeping the model uniform means enabling VLANs on a new device type is a
/// behaviour change, not a schema change.
struct VlanConfiguration {
    VlanMode mode = VlanMode::Access;
    VlanId accessVlan = kDefaultVlan;
    VlanId nativeVlan = kDefaultVlan;

    /// Empty means "all VLANs allowed", matching switch CLI conventions.
    std::vector<VlanId> allowedVlans;

    [[nodiscard]] bool allowsVlan(VlanId vlan) const {
        if (mode == VlanMode::Access) return vlan == accessVlan;
        if (allowedVlans.empty()) return isValidVlanId(vlan);
        return std::find(allowedVlans.begin(), allowedVlans.end(), vlan) != allowedVlans.end();
    }

    /// VLAN assigned to a frame that arrives untagged on this port.
    [[nodiscard]] VlanId untaggedVlan() const {
        return mode == VlanMode::Access ? accessVlan : nativeVlan;
    }

    /// Whether a frame leaving on this port in `vlan` must carry an 802.1Q tag.
    [[nodiscard]] bool shouldTagOnEgress(VlanId vlan) const {
        return mode == VlanMode::Trunk && vlan != nativeVlan;
    }

    bool operator==(const VlanConfiguration&) const = default;
};

/// A VLAN declared on a switch, with a human readable name.
struct VlanDefinition {
    VlanId id = kDefaultVlan;
    std::string name = "default";

    bool operator==(const VlanDefinition&) const = default;
};

} // namespace tnp::core
