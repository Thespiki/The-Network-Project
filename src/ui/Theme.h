#pragma once

#include "core/network/DeviceType.h"
#include "core/network/Frame.h"
#include "validation/ValidationIssue.h"

#include <imgui.h>

namespace tnp::ui {

/// The application's colour vocabulary.
///
/// Every colour used anywhere in the interface is named here. Panels never mix
/// their own constants: that is what keeps the canvas, the log and the problems
/// list agreeing on what "error" looks like.
struct Theme {
    // Surfaces
    ImU32 canvasBackground;
    ImU32 canvasGridMinor;
    ImU32 canvasGridMajor;
    ImU32 panelBackground;

    // Text
    ImU32 text;
    ImU32 textSubtle;
    ImU32 textDisabled;

    // Devices
    ImU32 deviceFill;
    ImU32 deviceFillHovered;
    ImU32 deviceBorder;
    ImU32 deviceBorderSelected;
    ImU32 deviceShadow;

    // Links
    ImU32 link;
    ImU32 linkSelected;
    ImU32 linkDown;
    ImU32 linkPending;

    // Status
    ImU32 accent;
    ImU32 success;
    ImU32 warning;
    ImU32 error;
    ImU32 info;

    // Selection
    ImU32 selectionBox;
    ImU32 selectionBoxFill;
};

/// The single theme instance, applied at start-up.
[[nodiscard]] const Theme& theme();

/// Applies the TNP look to the current ImGui context.
void applyTheme();

/// Accent colour for a device category, used on icons and palette entries.
[[nodiscard]] ImU32 deviceAccent(core::DeviceType type);

/// Colour a packet is drawn with, by protocol.
[[nodiscard]] ImU32 packetColor(core::FrameCategory category);

/// Colour matching a validation severity.
[[nodiscard]] ImU32 severityColor(validation::Severity severity);

/// A glyph standing in for an icon, drawn from the default font so no image
/// assets are needed.
[[nodiscard]] const char* severityGlyph(validation::Severity severity);

} // namespace tnp::ui
