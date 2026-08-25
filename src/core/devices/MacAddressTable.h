#pragma once

#include "core/network/Ids.h"
#include "core/network/MacAddress.h"
#include "core/network/Vlan.h"
#include "utilities/Time.h"

#include <map>
#include <utility>
#include <vector>

namespace tnp::core {

/// Default ageing time of a learned MAC address, matching the IEEE 802.1D
/// recommended value.
inline constexpr Duration kDefaultMacAgeingTime = std::chrono::duration_cast<Duration>(std::chrono::minutes{5});

struct MacTableEntry {
    VlanId vlan = kDefaultVlan;
    MacAddress mac;
    InterfaceId port;
    SimTime learnedAt{};
    SimTime expiresAt{};
    bool isStatic = false;

    [[nodiscard]] bool isExpired(SimTime now) const { return !isStatic && now >= expiresAt; }
};

/// What a `learn()` call actually did, so the caller can trace it accurately.
enum class MacLearnResult : u8 {
    Unchanged, ///< same MAC, same port: only the timer was refreshed
    Learned,   ///< a new binding was created
    Moved      ///< the address appeared on a different port
};

/// The forwarding database of a switch.
///
/// Keyed by (VLAN, MAC): the same address may legitimately exist in several
/// VLANs, and forwarding must never leak between them.
class MacAddressTable {
public:
    MacLearnResult learn(VlanId vlan, MacAddress mac, InterfaceId port, SimTime now,
                         Duration ageingTime = kDefaultMacAgeingTime);

    void learnStatic(VlanId vlan, MacAddress mac, InterfaceId port, SimTime now);

    /// The port to send a frame for `mac` in `vlan` out of, or nullptr when the
    /// address is unknown - which is what makes the switch flood.
    [[nodiscard]] const MacTableEntry* lookup(VlanId vlan, MacAddress mac, SimTime now) const;

    bool remove(VlanId vlan, MacAddress mac);
    std::size_t removeByPort(InterfaceId port);
    std::size_t removeByVlan(VlanId vlan);
    void clear();

    /// Drops aged-out entries and returns them for tracing.
    std::vector<MacTableEntry> removeExpired(SimTime now);

    /// Entries ordered by VLAN then MAC address.
    [[nodiscard]] std::vector<MacTableEntry> entries() const;
    [[nodiscard]] std::size_t size() const { return entries_.size(); }
    [[nodiscard]] bool empty() const { return entries_.empty(); }

private:
    using Key = std::pair<VlanId, MacAddress>;
    std::map<Key, MacTableEntry> entries_;
};

} // namespace tnp::core
