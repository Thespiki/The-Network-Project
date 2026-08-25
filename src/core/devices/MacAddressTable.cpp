#include "core/devices/MacAddressTable.h"

namespace tnp::core {

MacLearnResult MacAddressTable::learn(VlanId vlan, MacAddress mac, InterfaceId port, SimTime now,
                                      Duration ageingTime) {
    const Key key{vlan, mac};
    const auto it = entries_.find(key);

    if (it == entries_.end()) {
        MacTableEntry entry;
        entry.vlan = vlan;
        entry.mac = mac;
        entry.port = port;
        entry.learnedAt = now;
        entry.expiresAt = now + ageingTime;
        entries_.emplace(key, entry);
        return MacLearnResult::Learned;
    }

    if (it->second.isStatic) return MacLearnResult::Unchanged;

    const bool moved = it->second.port != port;
    it->second.port = port;
    it->second.learnedAt = now;
    it->second.expiresAt = now + ageingTime;
    return moved ? MacLearnResult::Moved : MacLearnResult::Unchanged;
}

void MacAddressTable::learnStatic(VlanId vlan, MacAddress mac, InterfaceId port, SimTime now) {
    MacTableEntry& entry = entries_[Key{vlan, mac}];
    entry.vlan = vlan;
    entry.mac = mac;
    entry.port = port;
    entry.learnedAt = now;
    entry.expiresAt = now;
    entry.isStatic = true;
}

const MacTableEntry* MacAddressTable::lookup(VlanId vlan, MacAddress mac, SimTime now) const {
    const auto it = entries_.find(Key{vlan, mac});
    if (it == entries_.end()) return nullptr;
    if (it->second.isExpired(now)) return nullptr;
    return &it->second;
}

bool MacAddressTable::remove(VlanId vlan, MacAddress mac) {
    return entries_.erase(Key{vlan, mac}) > 0;
}

std::size_t MacAddressTable::removeByPort(InterfaceId port) {
    std::size_t removed = 0;
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.port == port) {
            it = entries_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

std::size_t MacAddressTable::removeByVlan(VlanId vlan) {
    std::size_t removed = 0;
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.vlan == vlan) {
            it = entries_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

void MacAddressTable::clear() { entries_.clear(); }

std::vector<MacTableEntry> MacAddressTable::removeExpired(SimTime now) {
    std::vector<MacTableEntry> removed;
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.isExpired(now)) {
            removed.push_back(it->second);
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
    return removed;
}

std::vector<MacTableEntry> MacAddressTable::entries() const {
    std::vector<MacTableEntry> result;
    result.reserve(entries_.size());
    for (const auto& [key, entry] : entries_) result.push_back(entry);
    return result;
}

} // namespace tnp::core
