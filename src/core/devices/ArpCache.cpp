#include "core/devices/ArpCache.h"

namespace tnp::core {

void ArpCache::insert(Ipv4Address address, MacAddress mac, InterfaceId interface, SimTime now,
                      Duration lifetime) {
    ArpEntry& entry = entries_[address];
    entry.address = address;
    entry.mac = mac;
    entry.interface = interface;
    entry.learnedAt = now;
    entry.expiresAt = now + lifetime;
    entry.isStatic = false;
}

void ArpCache::insertStatic(Ipv4Address address, MacAddress mac, InterfaceId interface, SimTime now) {
    ArpEntry& entry = entries_[address];
    entry.address = address;
    entry.mac = mac;
    entry.interface = interface;
    entry.learnedAt = now;
    entry.expiresAt = now;
    entry.isStatic = true;
}

bool ArpCache::refresh(Ipv4Address address, MacAddress mac, InterfaceId interface, SimTime now,
                       Duration lifetime) {
    const auto it = entries_.find(address);
    if (it == entries_.end()) return false;
    if (it->second.isStatic) return true; // a static binding wins over the wire

    it->second.mac = mac;
    it->second.interface = interface;
    it->second.learnedAt = now;
    it->second.expiresAt = now + lifetime;
    return true;
}

const ArpEntry* ArpCache::find(Ipv4Address address, SimTime now) const {
    const auto it = entries_.find(address);
    if (it == entries_.end()) return nullptr;
    if (it->second.isExpired(now)) return nullptr;
    return &it->second;
}

bool ArpCache::remove(Ipv4Address address) { return entries_.erase(address) > 0; }

void ArpCache::clear() { entries_.clear(); }

std::vector<Ipv4Address> ArpCache::removeExpired(SimTime now) {
    std::vector<Ipv4Address> removed;
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.isExpired(now)) {
            removed.push_back(it->first);
            it = entries_.erase(it);
        } else {
            ++it;
        }
    }
    return removed;
}

std::vector<ArpEntry> ArpCache::entries() const {
    std::vector<ArpEntry> result;
    result.reserve(entries_.size());
    for (const auto& [address, entry] : entries_) result.push_back(entry);
    return result;
}

} // namespace tnp::core
