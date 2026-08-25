#pragma once

#include "core/network/Ids.h"
#include "core/network/Ipv4Address.h"
#include "core/network/MacAddress.h"
#include "utilities/Time.h"

#include <map>
#include <optional>
#include <vector>

namespace tnp::core {

/// Default lifetime of a dynamically learned ARP entry.
inline constexpr Duration kDefaultArpEntryLifetime = std::chrono::duration_cast<Duration>(std::chrono::minutes{4});

/// One IPv4-to-MAC binding.
struct ArpEntry {
    Ipv4Address address;
    MacAddress mac;
    InterfaceId interface;
    SimTime learnedAt{};
    SimTime expiresAt{};
    bool isStatic = false;

    [[nodiscard]] bool isExpired(SimTime now) const { return !isStatic && now >= expiresAt; }
    [[nodiscard]] Duration age(SimTime now) const { return now - learnedAt; }
};

/// A device's ARP cache.
///
/// Backed by an ordered map so iteration - and therefore the trace events
/// produced while ageing entries out - is deterministic. Caches hold tens of
/// entries, so the logarithmic lookup is irrelevant next to reproducibility.
class ArpCache {
public:
    /// Inserts or refreshes a dynamic entry.
    void insert(Ipv4Address address, MacAddress mac, InterfaceId interface, SimTime now,
                Duration lifetime = kDefaultArpEntryLifetime);

    /// Inserts an entry that never ages out.
    void insertStatic(Ipv4Address address, MacAddress mac, InterfaceId interface, SimTime now);

    /// Refreshes an existing entry only. Returns false when the address is not
    /// cached. This implements the RFC 826 rule that any ARP packet updates an
    /// existing binding, while only packets addressed to us create one.
    bool refresh(Ipv4Address address, MacAddress mac, InterfaceId interface, SimTime now,
                 Duration lifetime = kDefaultArpEntryLifetime);

    /// Live entry for `address`, or nullptr when absent or expired.
    [[nodiscard]] const ArpEntry* find(Ipv4Address address, SimTime now) const;

    [[nodiscard]] bool contains(Ipv4Address address, SimTime now) const { return find(address, now) != nullptr; }

    bool remove(Ipv4Address address);
    void clear();

    /// Drops expired entries and returns the addresses that were removed, so the
    /// caller can emit one trace event per expiry.
    std::vector<Ipv4Address> removeExpired(SimTime now);

    /// Entries ordered by address.
    [[nodiscard]] std::vector<ArpEntry> entries() const;
    [[nodiscard]] std::size_t size() const { return entries_.size(); }
    [[nodiscard]] bool empty() const { return entries_.empty(); }

private:
    std::map<Ipv4Address, ArpEntry> entries_;
};

} // namespace tnp::core
