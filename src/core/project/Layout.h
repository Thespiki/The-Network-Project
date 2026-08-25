#pragma once

#include "core/network/Ids.h"
#include "utilities/Geometry.h"

#include <unordered_map>

namespace tnp::core {

/// Where a device sits on the canvas.
struct DevicePlacement {
    Vec2 position;
    /// A locked device cannot be dragged, which keeps a carefully arranged
    /// backbone from being nudged while wiring access switches.
    bool locked = false;
};

/// Canvas placement, kept strictly apart from the network model.
///
/// A `Device` has no coordinates. Moving an icon must not touch the object the
/// simulator reads, and a topology remains meaningful when it is exported,
/// diffed or generated without any layout at all.
class Layout {
public:
    void setPosition(DeviceId device, Vec2 position);
    [[nodiscard]] Vec2 position(DeviceId device) const;
    [[nodiscard]] bool has(DeviceId device) const;

    void setLocked(DeviceId device, bool locked);
    [[nodiscard]] bool isLocked(DeviceId device) const;

    void remove(DeviceId device);
    void clear();

    [[nodiscard]] const std::unordered_map<DeviceId, DevicePlacement>& placements() const {
        return placements_;
    }

    /// Bounding box of every placed device. Empty when nothing is placed.
    [[nodiscard]] Rect boundingBox() const;

    // --- Canvas view state -------------------------------------------------
    // Saved with the project so reopening it restores the same view.
    Vec2 viewOffset;
    float viewZoom = 1.0f;

    bool gridVisible = true;
    bool snapToGrid = false;
    float gridSize = 24.0f;

private:
    std::unordered_map<DeviceId, DevicePlacement> placements_;
};

} // namespace tnp::core
