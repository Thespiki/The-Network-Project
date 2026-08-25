#include "core/project/Layout.h"

#include <limits>

namespace tnp::core {

void Layout::setPosition(DeviceId device, Vec2 position) {
    placements_[device].position = position;
}

Vec2 Layout::position(DeviceId device) const {
    const auto entry = placements_.find(device);
    return entry == placements_.end() ? Vec2{} : entry->second.position;
}

bool Layout::has(DeviceId device) const { return placements_.contains(device); }

void Layout::setLocked(DeviceId device, bool locked) { placements_[device].locked = locked; }

bool Layout::isLocked(DeviceId device) const {
    const auto entry = placements_.find(device);
    return entry != placements_.end() && entry->second.locked;
}

void Layout::remove(DeviceId device) { placements_.erase(device); }

void Layout::clear() { placements_.clear(); }

Rect Layout::boundingBox() const {
    if (placements_.empty()) return Rect{};

    constexpr float kInfinity = std::numeric_limits<float>::max();
    Rect bounds{{kInfinity, kInfinity}, {-kInfinity, -kInfinity}};
    for (const auto& [device, placement] : placements_) bounds.expandToInclude(placement.position);
    return bounds;
}

} // namespace tnp::core
