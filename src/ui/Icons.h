#pragma once

#include "core/network/DeviceType.h"

#include <imgui.h>

namespace tnp::ui {

/// Device icons, drawn as vectors rather than loaded as images.
///
/// Two reasons: they stay sharp at any zoom, and TNP ships with no binary
/// assets to keep in sync with the code. Each icon is drawn inside the unit
/// square centred on `center` and scaled by `size`.
void drawDeviceIcon(ImDrawList* drawList, core::DeviceType type, ImVec2 center, float size,
                    ImU32 color);

/// The same icons at a fixed small size, for the palette and list rows.
void drawDeviceGlyph(ImDrawList* drawList, core::DeviceType type, ImVec2 topLeft, float size,
                     ImU32 color);

/// A small arrow head at `tip`, pointing along `direction`. Used by link
/// direction hints and annotation arrows.
void drawArrowHead(ImDrawList* drawList, ImVec2 tip, ImVec2 direction, float size, ImU32 color);

} // namespace tnp::ui
