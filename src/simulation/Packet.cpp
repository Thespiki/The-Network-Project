#include "simulation/Packet.h"

#include <algorithm>
#include <utility>

namespace tnp::sim {

float PacketInFlight::progressAt(SimTime time) const {
    const Duration total = arrival - departure;
    if (total <= Duration::zero()) return 1.0f;

    const Duration elapsed = time - departure;
    if (elapsed <= Duration::zero()) return 0.0f;
    if (elapsed >= total) return 1.0f;

    return static_cast<float>(static_cast<double>(elapsed.count()) / static_cast<double>(total.count()));
}

PacketRecord& PacketRegistry::observe(const core::Frame& frame) {
    const auto existing = records_.find(frame.id);
    if (existing != records_.end()) {
        // A forwarded packet keeps its identity but carries new bytes.
        existing->second.bytes = frame.bytes;
        return existing->second;
    }

    PacketRecord record;
    record.id = frame.id;
    record.origin = frame.origin;
    record.category = frame.category;
    record.summary = frame.summary;
    record.createdAt = frame.createdAt;
    record.bytes = frame.bytes;

    records_.emplace(frame.id, std::move(record));
    order_.push_back(frame.id);

    // Trimming only ever removes from the front of the order, so the record
    // just appended always survives.
    trim();
    return records_.at(frame.id);
}

void PacketRegistry::addHop(core::PacketId packet, PacketHop hop) {
    const auto record = records_.find(packet);
    if (record == records_.end()) return;
    record->second.hops.push_back(std::move(hop));
}

const PacketRecord* PacketRegistry::find(core::PacketId packet) const {
    const auto record = records_.find(packet);
    return record == records_.end() ? nullptr : &record->second;
}

void PacketRegistry::setCapacity(std::size_t capacity) {
    capacity_ = std::max<std::size_t>(capacity, 1);
    trim();
}

void PacketRegistry::trim() {
    while (order_.size() > capacity_) {
        records_.erase(order_.front());
        order_.pop_front();
    }
}

void PacketRegistry::clear() {
    records_.clear();
    order_.clear();
}

} // namespace tnp::sim
