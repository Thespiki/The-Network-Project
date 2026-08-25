#pragma once

#include "ui/UiContext.h"

#include <imgui.h>

#include <optional>
#include <vector>

namespace tnp::ui {

/// Which coordinate an alignment operates on.
enum class Axis : u8 { Horizontal, Vertical };

/// Where aligned objects end up.
enum class AlignMode : u8 { Min, Center, Max };

/// What a click on the canvas does.
enum class CanvasTool : u8 {
    Select,       ///< select, drag and box-select
    Connect,      ///< drag a cable between two devices
    Text,         ///< place a text annotation
    Rectangle,    ///< draw a rectangle annotation
    Ellipse,
    Arrow,
    NetworkLabel
};

[[nodiscard]] const char* canvasToolName(CanvasTool tool);

/// The topology editor.
///
/// Draws the network with `ImDrawList` rather than with widgets: a topology is a
/// custom scene, and a retained widget per device would neither scale nor allow
/// the drawing this needs. Everything it changes goes through commands, so the
/// canvas has no undo logic of its own.
///
/// The canvas holds no model state. Device positions live in `core::Layout`,
/// selection in `app::Selection`, packets in the simulator; the only thing owned
/// here is the view (pan, zoom) and whatever gesture is in progress.
class Canvas {
public:
    void draw(UiContext& context);

    [[nodiscard]] CanvasTool tool() const { return tool_; }
    void setTool(CanvasTool tool) { tool_ = tool; }

    /// Frames the whole topology, or the selection when there is one.
    void frameAll(UiContext& context);
    void frameSelection(UiContext& context);

    void zoomIn() { setZoom(zoom_ * 1.25f); }
    void zoomOut() { setZoom(zoom_ / 1.25f); }
    void resetZoom() { setZoom(1.0f); }
    [[nodiscard]] float zoom() const { return zoom_; }

    /// Alignment and distribution, applied to the current selection.
    void alignSelection(UiContext& context, Axis axis, AlignMode mode);
    void distributeSelection(UiContext& context, Axis axis);

private:
    // --- Coordinates --------------------------------------------------------
    [[nodiscard]] ImVec2 toScreen(Vec2 world) const;
    [[nodiscard]] Vec2 toWorld(ImVec2 screen) const;
    void setZoom(float zoom);
    void zoomAt(ImVec2 screenPoint, float newZoom);

    // --- Drawing ------------------------------------------------------------
    void drawGrid(ImDrawList* draw) const;
    void drawAnnotations(ImDrawList* draw, UiContext& context) const;
    void drawLinks(ImDrawList* draw, UiContext& context) const;
    void drawDevices(ImDrawList* draw, UiContext& context) const;
    void drawPackets(ImDrawList* draw, UiContext& context) const;
    void drawPendingConnection(ImDrawList* draw, UiContext& context) const;
    void drawSelectionBox(ImDrawList* draw) const;
    void drawMinimap(ImDrawList* draw, UiContext& context) const;
    void drawEmptyHint(ImDrawList* draw, UiContext& context) const;

    // --- Interaction --------------------------------------------------------
    void handleInput(UiContext& context);
    void handleSelectionTool(UiContext& context);
    void handleConnectTool(UiContext& context);
    void handleAnnotationTool(UiContext& context);
    void handleKeyboard(UiContext& context);
    void drawContextMenu(UiContext& context);

    [[nodiscard]] core::DeviceId deviceAt(const UiContext& context, Vec2 world) const;
    [[nodiscard]] core::LinkId linkAt(const UiContext& context, Vec2 world) const;
    [[nodiscard]] core::AnnotationId annotationAt(const UiContext& context, Vec2 world) const;

    /// The world-space rectangle a device occupies.
    [[nodiscard]] static Rect deviceBounds(Vec2 position);

    /// Snaps to the grid when snapping is on.
    [[nodiscard]] Vec2 snap(const UiContext& context, Vec2 world) const;

    void commitDrag(UiContext& context);

    // --- View ---------------------------------------------------------------
    Vec2 offset_;
    float zoom_ = 1.0f;

    ImVec2 origin_{};   ///< top-left of the canvas in screen space
    ImVec2 size_{};     ///< canvas size in pixels

    CanvasTool tool_ = CanvasTool::Select;

    // --- Gesture state ------------------------------------------------------
    bool isPanning_ = false;
    bool isDraggingDevices_ = false;
    Vec2 dragAccumulated_;

    bool isBoxSelecting_ = false;
    Vec2 boxStart_;
    Vec2 boxEnd_;

    bool isDrawingAnnotation_ = false;
    Vec2 annotationStart_;
    Vec2 annotationEnd_;

    core::DeviceId hoveredDevice_;
    core::LinkId hoveredLink_;

    /// Devices copied with Ctrl+C, pasted with Ctrl+V.
    std::vector<core::DeviceId> clipboard_;
};

} // namespace tnp::ui
