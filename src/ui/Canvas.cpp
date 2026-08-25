#include "ui/Canvas.h"

#include "commands/DeviceCommands.h"
#include "commands/LinkCommands.h"
#include "commands/ProjectCommands.h"
#include "core/devices/Ipv4Stack.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace tnp::ui {
namespace {

using namespace core;

/// Device box size in project units. Shared with the SVG export so a picture
/// looks like what the user was editing.
constexpr float kDeviceHalfWidth = 46.0f;
constexpr float kDeviceHalfHeight = 30.0f;

constexpr float kMinZoom = 0.15f;
constexpr float kMaxZoom = 4.0f;

/// A click within this many pixels of a cable selects it.
constexpr float kLinkPickRadius = 6.0f;

constexpr float kMinimapSize = 168.0f;
constexpr float kMinimapMargin = 14.0f;

ImVec2 add(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
ImVec2 sub(ImVec2 a, ImVec2 b) { return ImVec2(a.x - b.x, a.y - b.y); }

float distanceToSegment(Vec2 point, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const float lengthSquared = ab.lengthSquared();
    if (lengthSquared <= 0.0001f) return (point - a).length();

    float t = ((point.x - a.x) * ab.x + (point.y - a.y) * ab.y) / lengthSquared;
    t = std::clamp(t, 0.0f, 1.0f);
    return (point - (a + ab * t)).length();
}

/// Primary address of a device, for the label under its icon.
std::string primaryAddress(const Device& device) {
    for (const auto& iface : device.interfaces()) {
        if (const auto address = iface->primaryIpv4()) return address->toString();
    }
    for (const auto& iface : device.interfaces()) {
        if (iface->ipv4DhcpEnabled()) return "dhcp";
    }
    return {};
}

/// Draws text centred on `center`, at the canvas font size.
void centeredText(ImDrawList* draw, ImVec2 center, ImU32 color, const std::string& text,
                  float fontSize) {
    if (text.empty()) return;
    const ImVec2 extent = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
    draw->AddText(nullptr, fontSize, ImVec2(center.x - extent.x * 0.5f, center.y - extent.y * 0.5f),
                  color, text.c_str());
}

} // namespace

const char* canvasToolName(CanvasTool tool) {
    switch (tool) {
        case CanvasTool::Select:       return "Select";
        case CanvasTool::Connect:      return "Connect";
        case CanvasTool::Text:         return "Text";
        case CanvasTool::Rectangle:    return "Rectangle";
        case CanvasTool::Ellipse:      return "Ellipse";
        case CanvasTool::Arrow:        return "Arrow";
        case CanvasTool::NetworkLabel: return "Network label";
    }
    return "Select";
}

// ---------------------------------------------------------------------------
// Coordinates
// ---------------------------------------------------------------------------

ImVec2 Canvas::toScreen(Vec2 world) const {
    return ImVec2(origin_.x + size_.x * 0.5f + (world.x - offset_.x) * zoom_,
                  origin_.y + size_.y * 0.5f + (world.y - offset_.y) * zoom_);
}

Vec2 Canvas::toWorld(ImVec2 screen) const {
    return Vec2{(screen.x - origin_.x - size_.x * 0.5f) / zoom_ + offset_.x,
                (screen.y - origin_.y - size_.y * 0.5f) / zoom_ + offset_.y};
}

void Canvas::setZoom(float zoom) { zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom); }

void Canvas::zoomAt(ImVec2 screenPoint, float newZoom) {
    // Keep the world point under the cursor fixed, which is what makes wheel
    // zoom feel like it is magnifying rather than scrolling.
    const Vec2 before = toWorld(screenPoint);
    setZoom(newZoom);
    const Vec2 after = toWorld(screenPoint);
    offset_ += before - after;
}

Rect Canvas::deviceBounds(Vec2 position) {
    return Rect::fromCenter(position, Vec2{kDeviceHalfWidth, kDeviceHalfHeight});
}

Vec2 Canvas::snap(const UiContext& context, Vec2 world) const {
    const Layout& layout = context.application.project().layout();
    if (!layout.snapToGrid || layout.gridSize <= 0.0f) return world;

    const float grid = layout.gridSize;
    return Vec2{std::round(world.x / grid) * grid, std::round(world.y / grid) * grid};
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void Canvas::draw(UiContext& context) {
    origin_ = ImGui::GetCursorScreenPos();
    size_ = ImGui::GetContentRegionAvail();
    if (size_.x < 32.0f || size_.y < 32.0f) return;

    context.viewCenter = offset_;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin_, add(origin_, size_), theme().canvasBackground);

    // One invisible button covers the canvas so ImGui routes mouse input here
    // and the panel behind it does not also react.
    ImGui::InvisibleButton("##canvas", size_,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                               ImGuiButtonFlags_MouseButtonMiddle);

    draw->PushClipRect(origin_, add(origin_, size_), true);

    drawGrid(draw);
    handleInput(context);

    drawAnnotations(draw, context);
    drawLinks(draw, context);
    drawPackets(draw, context);
    drawDevices(draw, context);
    drawPendingConnection(draw, context);
    drawSelectionBox(draw);

    if (context.application.project().network().deviceCount() == 0) drawEmptyHint(draw, context);
    if (context.showMinimap) drawMinimap(draw, context);

    draw->PopClipRect();

    drawContextMenu(context);
}

void Canvas::drawGrid(ImDrawList* draw) const {
    // Below this zoom the grid becomes noise rather than guidance.
    if (zoom_ < 0.4f) return;

    const float grid = 24.0f * zoom_;
    if (grid < 6.0f) return;

    const ImVec2 bottomRight = add(origin_, size_);
    const Vec2 topLeftWorld = toWorld(origin_);

    const float startX = origin_.x - std::fmod((topLeftWorld.x * zoom_), grid);
    const float startY = origin_.y - std::fmod((topLeftWorld.y * zoom_), grid);

    int index = 0;
    for (float x = startX; x < bottomRight.x; x += grid, ++index) {
        const bool major = (index % 5) == 0;
        draw->AddLine(ImVec2(x, origin_.y), ImVec2(x, bottomRight.y),
                      major ? theme().canvasGridMajor : theme().canvasGridMinor, 1.0f);
    }
    index = 0;
    for (float y = startY; y < bottomRight.y; y += grid, ++index) {
        const bool major = (index % 5) == 0;
        draw->AddLine(ImVec2(origin_.x, y), ImVec2(bottomRight.x, y),
                      major ? theme().canvasGridMajor : theme().canvasGridMinor, 1.0f);
    }
}

void Canvas::drawEmptyHint(ImDrawList* draw, UiContext& context) const {
    (void)context;
    const ImVec2 center(origin_.x + size_.x * 0.5f, origin_.y + size_.y * 0.5f);
    centeredText(draw, center, theme().textDisabled,
                 "Drag a device from the palette, or use File > New from sample", 15.0f);
}

// ---------------------------------------------------------------------------
// Drawing: links
// ---------------------------------------------------------------------------

void Canvas::drawLinks(ImDrawList* draw, UiContext& context) const {
    const core::Project& project = context.application.project();
    const Network& network = project.network();
    const app::Selection& selection = context.application.selection();

    const bool showLabels = zoom_ > 0.7f;

    for (const auto& link : network.links()) {
        const Vec2 a = project.layout().position(link->endpointA().device);
        const Vec2 b = project.layout().position(link->endpointB().device);

        const ImVec2 screenA = toScreen(a);
        const ImVec2 screenB = toScreen(b);

        const bool selected = selection.contains(ObjectRef::link(link->id()));
        const bool hovered = hoveredLink_ == link->id();

        const Interface* interfaceA = network.findInterface(link->endpointA().interface);
        const Interface* interfaceB = network.findInterface(link->endpointB().interface);
        const bool operational = interfaceA != nullptr && interfaceB != nullptr &&
                                 interfaceA->isOperational() && interfaceB->isOperational();

        ImU32 color = theme().link;
        if (!link->isEnabled() || !operational) color = theme().linkDown;
        if (selected) color = theme().linkSelected;

        const float thickness = (selected || hovered) ? 3.0f : 2.0f;

        if (link->isEnabled()) {
            draw->AddLine(screenA, screenB, color, thickness);
        } else {
            // A disabled cable is drawn dashed: it is still documented, but it
            // carries nothing.
            const Vec2 direction = (b - a).normalized();
            const float total = (b - a).length();
            for (float t = 0.0f; t < total; t += 14.0f) {
                const Vec2 from = a + direction * t;
                const Vec2 to = a + direction * std::min(t + 7.0f, total);
                draw->AddLine(toScreen(from), toScreen(to), color, thickness);
            }
        }

        // A small square at each end marks the interface, and turns red when
        // that end is down - which is where a fault actually is.
        const Vec2 direction = (b - a).normalized();
        const Vec2 endpointA = a + direction * (kDeviceHalfWidth + 4.0f);
        const Vec2 endpointB = b - direction * (kDeviceHalfWidth + 4.0f);

        const auto endpointColor = [&](const Interface* iface) {
            if (iface == nullptr) return theme().linkDown;
            return iface->isOperational() ? theme().success : theme().linkDown;
        };

        const float dot = std::max(2.0f, 3.0f * zoom_);
        draw->AddCircleFilled(toScreen(endpointA), dot, endpointColor(interfaceA), 8);
        draw->AddCircleFilled(toScreen(endpointB), dot, endpointColor(interfaceB), 8);

        if (!showLabels) continue;

        const float fontSize = std::clamp(10.0f * zoom_, 8.0f, 13.0f);
        if (interfaceA != nullptr) {
            centeredText(draw, toScreen(a + direction * (kDeviceHalfWidth + 22.0f)),
                         theme().textSubtle, interfaceA->shortName(), fontSize);
        }
        if (interfaceB != nullptr) {
            centeredText(draw, toScreen(b - direction * (kDeviceHalfWidth + 22.0f)),
                         theme().textSubtle, interfaceB->shortName(), fontSize);
        }
        if (!link->label().empty()) {
            centeredText(draw, toScreen(lerp(a, b, 0.5f) - Vec2{0.0f, 12.0f}), theme().textSubtle,
                         link->label(), fontSize);
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing: devices
// ---------------------------------------------------------------------------

void Canvas::drawDevices(ImDrawList* draw, UiContext& context) const {
    const core::Project& project = context.application.project();
    const app::Selection& selection = context.application.selection();
    const validation::ValidationReport& report = context.application.validationReport();

    const bool showText = zoom_ > 0.45f;
    const float nameFont = std::clamp(13.0f * zoom_, 9.0f, 18.0f);
    const float typeFont = std::clamp(10.0f * zoom_, 8.0f, 14.0f);

    for (const auto& device : project.network().devices()) {
        const Vec2 position = project.layout().position(device->id());
        const Rect bounds = deviceBounds(position);

        const ImVec2 topLeft = toScreen(bounds.min);
        const ImVec2 bottomRight = toScreen(bounds.max);

        const bool selected = selection.contains(ObjectRef::device(device->id()));
        const bool hovered = hoveredDevice_ == device->id();

        draw->AddRectFilled(add(topLeft, ImVec2(2, 3)), add(bottomRight, ImVec2(2, 3)),
                            theme().deviceShadow, 7.0f * zoom_);
        draw->AddRectFilled(topLeft, bottomRight,
                            hovered ? theme().deviceFillHovered : theme().deviceFill, 7.0f * zoom_);
        draw->AddRect(topLeft, bottomRight,
                      selected ? theme().deviceBorderSelected : theme().deviceBorder, 7.0f * zoom_,
                      0, selected ? 2.5f : 1.4f);

        // Category stripe along the top edge.
        draw->AddRectFilled(ImVec2(topLeft.x + 8.0f * zoom_, topLeft.y + 5.0f * zoom_),
                            ImVec2(bottomRight.x - 8.0f * zoom_, topLeft.y + 8.5f * zoom_),
                            deviceAccent(device->type()), 2.0f);

        drawDeviceIcon(draw, device->type(),
                       ImVec2((topLeft.x + bottomRight.x) * 0.5f, topLeft.y + 24.0f * zoom_),
                       26.0f * zoom_, deviceAccent(device->type()));

        if (showText) {
            centeredText(draw, ImVec2((topLeft.x + bottomRight.x) * 0.5f, bottomRight.y - 12.0f * zoom_),
                         theme().text, device->name(), nameFont);

            const std::string address = primaryAddress(*device);
            if (!address.empty()) {
                centeredText(draw,
                             ImVec2((topLeft.x + bottomRight.x) * 0.5f, bottomRight.y + 11.0f * zoom_),
                             theme().textSubtle, address, typeFont);
            }
        }

        // The worst finding about this device becomes a badge, so a problem is
        // visible on the topology and not only in a list.
        const auto issues = report.forObject(ObjectRef::device(device->id()));
        if (!issues.empty()) {
            const auto worst = std::max_element(issues.begin(), issues.end(),
                                                [](const validation::ValidationIssue& a,
                                                   const validation::ValidationIssue& b) {
                                                    return a.severity < b.severity;
                                                });
            const ImVec2 badge(bottomRight.x - 6.0f * zoom_, topLeft.y + 6.0f * zoom_);
            draw->AddCircleFilled(badge, 6.0f * zoom_, severityColor(worst->severity), 12);
            centeredText(draw, badge, theme().canvasBackground, severityGlyph(worst->severity),
                         std::clamp(10.0f * zoom_, 7.0f, 12.0f));
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing: packets
// ---------------------------------------------------------------------------

void Canvas::drawPackets(ImDrawList* draw, UiContext& context) const {
    const sim::Simulator& simulator = context.application.simulator();
    if (!simulator.isActive()) return;

    const core::Project& project = context.application.project();
    const SimTime now = simulator.now();

    for (const sim::PacketInFlight& flight : simulator.packetsInFlight()) {
        const Vec2 from = project.layout().position(flight.fromDevice);
        const Vec2 to = project.layout().position(flight.toDevice);

        const float progress = flight.progressAt(now);
        const Vec2 position = lerp(from, to, progress);
        const ImVec2 screen = toScreen(position);

        const ImU32 color = packetColor(flight.category);
        const float radius = std::clamp(5.0f * zoom_, 3.0f, 9.0f);

        // A short trail behind the dot makes the direction of travel obvious.
        const Vec2 direction = (to - from).normalized();
        const Vec2 tail = position - direction * (10.0f);
        draw->AddLine(toScreen(tail), screen, color, std::max(1.5f, 2.0f * zoom_));

        draw->AddCircleFilled(screen, radius, color, 12);
        draw->AddCircle(screen, radius, theme().canvasBackground, 12, 1.2f);

        if (flight.packet == context.inspectedPacket) {
            draw->AddCircle(screen, radius + 4.0f * zoom_, theme().accent, 16, 2.0f);
        }
    }
}

void Canvas::drawPendingConnection(ImDrawList* draw, UiContext& context) const {
    if (!context.isConnecting) return;

    const core::Project& project = context.application.project();
    const Vec2 from = project.layout().position(context.connectFromDevice);

    draw->AddLine(toScreen(from), ImGui::GetIO().MousePos, theme().linkPending, 2.5f);
    draw->AddCircleFilled(ImGui::GetIO().MousePos, 4.0f, theme().linkPending, 10);
}

void Canvas::drawSelectionBox(ImDrawList* draw) const {
    if (isBoxSelecting_) {
        const Rect box = Rect::fromCorners(boxStart_, boxEnd_);
        draw->AddRectFilled(toScreen(box.min), toScreen(box.max), theme().selectionBoxFill, 2.0f);
        draw->AddRect(toScreen(box.min), toScreen(box.max), theme().selectionBox, 2.0f, 0, 1.5f);
    }
    if (isDrawingAnnotation_) {
        const Rect box = Rect::fromCorners(annotationStart_, annotationEnd_);
        draw->AddRect(toScreen(box.min), toScreen(box.max), theme().accent, 2.0f, 0, 1.5f);
    }
}

// ---------------------------------------------------------------------------
// Drawing: annotations
// ---------------------------------------------------------------------------

void Canvas::drawAnnotations(ImDrawList* draw, UiContext& context) const {
    const core::Project& project = context.application.project();
    const app::Selection& selection = context.application.selection();

    std::vector<const Annotation*> ordered;
    ordered.reserve(project.annotations().size());
    for (const Annotation& annotation : project.annotations()) ordered.push_back(&annotation);

    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Annotation* a, const Annotation* b) { return a->zOrder < b->zOrder; });

    for (const Annotation* annotation : ordered) {
        const Rect box = annotation->bounds();
        const ImVec2 topLeft = toScreen(box.min);
        const ImVec2 bottomRight = toScreen(box.max);
        const bool selected = selection.contains(ObjectRef::annotation(annotation->id));

        const ImU32 stroke = selected ? theme().accent : annotation->color;
        const float thickness = annotation->thickness * std::max(0.5f, zoom_);

        switch (annotation->kind) {
            case AnnotationKind::Rectangle:
                if (annotation->filled) {
                    draw->AddRectFilled(topLeft, bottomRight, annotation->fillColor, 4.0f);
                }
                draw->AddRect(topLeft, bottomRight, stroke, 4.0f, 0, thickness);
                break;

            case AnnotationKind::NetworkLabel:
                draw->AddRectFilled(topLeft, bottomRight, annotation->fillColor, 8.0f);
                draw->AddRect(topLeft, bottomRight, stroke, 8.0f, 0, thickness);
                break;

            case AnnotationKind::Ellipse: {
                const ImVec2 center((topLeft.x + bottomRight.x) * 0.5f,
                                    (topLeft.y + bottomRight.y) * 0.5f);
                const ImVec2 radius((bottomRight.x - topLeft.x) * 0.5f,
                                    (bottomRight.y - topLeft.y) * 0.5f);
                if (annotation->filled) draw->AddEllipseFilled(center, radius, annotation->fillColor);
                draw->AddEllipse(center, radius, stroke, 0.0f, 0, thickness);
                break;
            }

            case AnnotationKind::Arrow: {
                const ImVec2 start = toScreen(annotation->start);
                const ImVec2 end = toScreen(annotation->end);
                draw->AddLine(start, end, stroke, thickness);
                drawArrowHead(draw, end, sub(end, start), 12.0f * std::max(0.6f, zoom_), stroke);
                break;
            }

            case AnnotationKind::Text:
                break;
        }

        if (annotation->text.empty()) continue;

        const float fontSize = std::clamp(annotation->fontSize * zoom_, 8.0f, 48.0f);
        if (annotation->kind == AnnotationKind::Text) {
            draw->AddText(nullptr, fontSize, toScreen(annotation->start), stroke,
                          annotation->text.c_str());
        } else {
            draw->AddText(nullptr, fontSize,
                          ImVec2(topLeft.x + 8.0f, topLeft.y + 6.0f), stroke,
                          annotation->text.c_str());
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing: minimap
// ---------------------------------------------------------------------------

void Canvas::drawMinimap(ImDrawList* draw, UiContext& context) const {
    const core::Project& project = context.application.project();
    if (project.network().deviceCount() == 0) return;

    const ImVec2 mapBottomRight(origin_.x + size_.x - kMinimapMargin,
                                origin_.y + size_.y - kMinimapMargin);
    const ImVec2 mapTopLeft(mapBottomRight.x - kMinimapSize, mapBottomRight.y - kMinimapSize * 0.68f);

    draw->AddRectFilled(mapTopLeft, mapBottomRight, IM_COL32(12, 15, 19, 220), 5.0f);
    draw->AddRect(mapTopLeft, mapBottomRight, theme().deviceBorder, 5.0f, 0, 1.0f);

    Rect world = project.layout().boundingBox().expanded(120.0f);
    if (world.width() <= 1.0f || world.height() <= 1.0f) return;

    const float mapWidth = mapBottomRight.x - mapTopLeft.x - 12.0f;
    const float mapHeight = mapBottomRight.y - mapTopLeft.y - 12.0f;
    const float scale = std::min(mapWidth / world.width(), mapHeight / world.height());

    const ImVec2 mapCenter((mapTopLeft.x + mapBottomRight.x) * 0.5f,
                           (mapTopLeft.y + mapBottomRight.y) * 0.5f);
    const Vec2 worldCenter = world.center();

    const auto project2 = [&](Vec2 point) {
        return ImVec2(mapCenter.x + (point.x - worldCenter.x) * scale,
                      mapCenter.y + (point.y - worldCenter.y) * scale);
    };

    for (const auto& link : project.network().links()) {
        draw->AddLine(project2(project.layout().position(link->endpointA().device)),
                      project2(project.layout().position(link->endpointB().device)),
                      theme().link, 1.0f);
    }
    for (const auto& device : project.network().devices()) {
        draw->AddCircleFilled(project2(project.layout().position(device->id())), 2.5f,
                              deviceAccent(device->type()), 8);
    }

    // The viewport rectangle: where the canvas is looking right now.
    const Vec2 viewTopLeft = toWorld(origin_);
    const Vec2 viewBottomRight = toWorld(add(origin_, size_));
    draw->AddRect(project2(viewTopLeft), project2(viewBottomRight), theme().accent, 2.0f, 0, 1.2f);
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------

DeviceId Canvas::deviceAt(const UiContext& context, Vec2 world) const {
    const core::Project& project = context.application.project();

    // Reverse order so the device drawn last - the one on top - wins.
    const auto& devices = project.network().devices();
    for (auto it = devices.rbegin(); it != devices.rend(); ++it) {
        if (deviceBounds(project.layout().position((*it)->id())).contains(world)) return (*it)->id();
    }
    return DeviceId{};
}

LinkId Canvas::linkAt(const UiContext& context, Vec2 world) const {
    const core::Project& project = context.application.project();
    const float tolerance = kLinkPickRadius / std::max(zoom_, 0.01f);

    for (const auto& link : project.network().links()) {
        const Vec2 a = project.layout().position(link->endpointA().device);
        const Vec2 b = project.layout().position(link->endpointB().device);
        if (distanceToSegment(world, a, b) <= tolerance) return link->id();
    }
    return LinkId{};
}

AnnotationId Canvas::annotationAt(const UiContext& context, Vec2 world) const {
    const core::Project& project = context.application.project();

    const auto& annotations = project.annotations();
    for (auto it = annotations.rbegin(); it != annotations.rend(); ++it) {
        if (it->kind == AnnotationKind::Arrow) {
            if (distanceToSegment(world, it->start, it->end) <= 8.0f / std::max(zoom_, 0.01f)) {
                return it->id;
            }
            continue;
        }
        if (it->kind == AnnotationKind::Text) {
            const Rect textBox{it->start, it->start + Vec2{160.0f, it->fontSize + 4.0f}};
            if (textBox.contains(world)) return it->id;
            continue;
        }
        if (it->bounds().contains(world)) return it->id;
    }
    return AnnotationId{};
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void Canvas::handleInput(UiContext& context) {
    ImGuiIO& io = ImGui::GetIO();
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    hoveredDevice_ = DeviceId{};
    hoveredLink_ = LinkId{};

    if (hovered) {
        const Vec2 world = toWorld(io.MousePos);
        hoveredDevice_ = deviceAt(context, world);
        if (!hoveredDevice_.isValid()) hoveredLink_ = linkAt(context, world);
    }

    // Panning: middle mouse anywhere, or right-drag on empty space.
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        offset_ -= Vec2{io.MouseDelta.x / zoom_, io.MouseDelta.y / zoom_};
        isPanning_ = true;
    } else if (isPanning_ && !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        isPanning_ = false;
    }

    if (hovered && io.MouseWheel != 0.0f) {
        zoomAt(io.MousePos, zoom_ * std::pow(1.15f, io.MouseWheel));
    }

    // A device dropped from the palette lands where the cursor is.
    if (context.isDraggingNewDevice && hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        const Vec2 world = snap(context, toWorld(io.MousePos));
        context.application.addDevice(context.draggedDeviceType, world);
        context.isDraggingNewDevice = false;
        context.setStatus(std::format("Added {}",
                                      deviceTypeDisplayName(context.draggedDeviceType)));
    }

    switch (tool_) {
        case CanvasTool::Select:  handleSelectionTool(context); break;
        case CanvasTool::Connect: handleConnectTool(context); break;
        default:                  handleAnnotationTool(context); break;
    }

    if (hovered || active) handleKeyboard(context);
}

void Canvas::handleSelectionTool(UiContext& context) {
    ImGuiIO& io = ImGui::GetIO();
    app::Selection& selection = context.application.selection();
    const bool hovered = ImGui::IsItemHovered();

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const Vec2 world = toWorld(io.MousePos);

        ObjectRef clicked;
        if (const DeviceId device = deviceAt(context, world); device.isValid()) {
            clicked = ObjectRef::device(device);
        } else if (const LinkId link = linkAt(context, world); link.isValid()) {
            clicked = ObjectRef::link(link);
        } else if (const AnnotationId annotation = annotationAt(context, world); annotation.isValid()) {
            clicked = ObjectRef::annotation(annotation);
        }

        if (!clicked.isValid()) {
            if (!io.KeyShift && !io.KeyCtrl) selection.clear();
            isBoxSelecting_ = true;
            boxStart_ = world;
            boxEnd_ = world;
        } else if (io.KeyCtrl || io.KeyShift) {
            selection.toggle(clicked);
        } else if (!selection.contains(clicked)) {
            selection.select(clicked);
        }

        if (clicked.kind == ObjectKind::Device) context.inspectedInterface = InterfaceId{};
    }

    // Box selection.
    if (isBoxSelecting_) {
        boxEnd_ = toWorld(io.MousePos);

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            const Rect box = Rect::fromCorners(boxStart_, boxEnd_);
            const core::Project& project = context.application.project();

            // A click without movement is not a box select; it just deselects.
            if (box.width() > 3.0f || box.height() > 3.0f) {
                for (const auto& device : project.network().devices()) {
                    if (box.intersects(deviceBounds(project.layout().position(device->id())))) {
                        selection.add(ObjectRef::device(device->id()));
                    }
                }
                for (const Annotation& annotation : project.annotations()) {
                    if (box.intersects(annotation.bounds())) {
                        selection.add(ObjectRef::annotation(annotation.id));
                    }
                }
            }
            isBoxSelecting_ = false;
        }
        return;
    }

    // Dragging the selection.
    const bool draggingLeft = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f);
    if (draggingLeft && hoveredDevice_.isValid() && !selection.devices().empty()) {
        isDraggingDevices_ = true;
    }

    if (isDraggingDevices_ && draggingLeft) {
        const Vec2 delta{io.MouseDelta.x / zoom_, io.MouseDelta.y / zoom_};
        if (delta.x != 0.0f || delta.y != 0.0f) {
            dragAccumulated_ += delta;
            context.application.commands().run(
                std::make_unique<commands::MoveDevicesCommand>(selection.devices(), delta));
        }
    }

    if (isDraggingDevices_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) commitDrag(context);
}

void Canvas::commitDrag(UiContext& context) {
    isDraggingDevices_ = false;

    // Snapping happens once, when the gesture ends: snapping every frame would
    // make the drag feel like it is fighting the cursor.
    const Layout& layout = context.application.project().layout();
    if (layout.snapToGrid) {
        std::vector<std::pair<DeviceId, Vec2>> positions;
        for (const DeviceId device : context.application.selection().devices()) {
            positions.emplace_back(device, snap(context, layout.position(device)));
        }
        if (!positions.empty()) {
            context.application.commands().run(
                std::make_unique<commands::SetDevicePositionsCommand>(std::move(positions),
                                                                      "Snap to grid"));
        }
    }

    context.application.commands().breakMergeChain();
    dragAccumulated_ = Vec2{};
}

void Canvas::handleConnectTool(UiContext& context) {
    ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsItemHovered()) return;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const DeviceId device = deviceAt(context, toWorld(io.MousePos));

        if (!context.isConnecting) {
            if (!device.isValid()) return;
            context.isConnecting = true;
            context.connectFromDevice = device;
            context.setStatus("Now click the device to connect to");
            return;
        }

        if (!device.isValid()) {
            context.isConnecting = false;
            context.setStatus("Connection cancelled");
            return;
        }
        if (device == context.connectFromDevice) return;

        // Which pair of interfaces to use is a real decision, so it is asked
        // rather than guessed.
        context.requestInterfacePicker = true;
        context.pickerSourceDevice = context.connectFromDevice;
        context.pickerTargetDevice = device;
        context.isConnecting = false;
    }

    if (context.isConnecting && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        context.isConnecting = false;
        context.setStatus("Connection cancelled");
    }
}

void Canvas::handleAnnotationTool(UiContext& context) {
    ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsItemHovered() && !isDrawingAnnotation_) return;

    const auto kindForTool = [](CanvasTool tool) {
        switch (tool) {
            case CanvasTool::Rectangle:    return AnnotationKind::Rectangle;
            case CanvasTool::Ellipse:      return AnnotationKind::Ellipse;
            case CanvasTool::Arrow:        return AnnotationKind::Arrow;
            case CanvasTool::NetworkLabel: return AnnotationKind::NetworkLabel;
            default:                       return AnnotationKind::Text;
        }
    };

    if (tool_ == CanvasTool::Text) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            Annotation annotation;
            annotation.kind = AnnotationKind::Text;
            annotation.start = snap(context, toWorld(io.MousePos));
            annotation.end = annotation.start + Vec2{140.0f, 24.0f};
            annotation.text = "Note";
            annotation.color = theme().text;

            auto command = std::make_unique<commands::AddAnnotationCommand>(annotation);
            const AnnotationId id = command->annotationId();
            if (context.application.commands().run(std::move(command))) {
                context.application.selection().select(ObjectRef::annotation(id));
                context.setStatus("Text added; edit it in the properties panel");
            }
            tool_ = CanvasTool::Select;
        }
        return;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        isDrawingAnnotation_ = true;
        annotationStart_ = snap(context, toWorld(io.MousePos));
        annotationEnd_ = annotationStart_;
        return;
    }

    if (!isDrawingAnnotation_) return;
    annotationEnd_ = snap(context, toWorld(io.MousePos));

    if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left)) return;
    isDrawingAnnotation_ = false;

    const Rect box = Rect::fromCorners(annotationStart_, annotationEnd_);
    if (box.width() < 8.0f && box.height() < 8.0f) {
        tool_ = CanvasTool::Select;
        return;
    }

    Annotation annotation;
    annotation.kind = kindForTool(tool_);
    annotation.start = tool_ == CanvasTool::Arrow ? annotationStart_ : box.min;
    annotation.end = tool_ == CanvasTool::Arrow ? annotationEnd_ : box.max;
    annotation.color = theme().textSubtle;
    annotation.zOrder = annotation.kind == AnnotationKind::NetworkLabel ? -10 : 0;
    if (annotation.kind == AnnotationKind::NetworkLabel) {
        annotation.text = "Network";
        annotation.fillColor = IM_COL32(76, 154, 232, 26);
    }

    auto command = std::make_unique<commands::AddAnnotationCommand>(annotation);
    const AnnotationId id = command->annotationId();
    if (context.application.commands().run(std::move(command))) {
        context.application.selection().select(ObjectRef::annotation(id));
    }
    tool_ = CanvasTool::Select;
}

void Canvas::handleKeyboard(UiContext& context) {
    if (ImGui::GetIO().WantTextInput) return;

    ImGuiIO& io = ImGui::GetIO();
    app::Selection& selection = context.application.selection();

    if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        if (!selection.empty()) {
            context.application.deleteSelection();
            context.setStatus("Deleted");
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        context.isConnecting = false;
        isDrawingAnnotation_ = false;
        tool_ = CanvasTool::Select;
        selection.clear();
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        selection.clear();
        for (const auto& device : context.application.project().network().devices()) {
            selection.add(ObjectRef::device(device->id()));
        }
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        clipboard_ = selection.devices();
        if (!clipboard_.empty()) {
            context.setStatus(std::format("Copied {} device(s)", clipboard_.size()));
        }
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && !clipboard_.empty()) {
        auto command = std::make_unique<commands::DuplicateDevicesCommand>(clipboard_,
                                                                           Vec2{40.0f, 40.0f});
        auto* raw = command.get();
        if (context.application.commands().run(std::move(command))) {
            selection.clear();
            for (const DeviceId device : raw->createdDevices()) {
                selection.add(ObjectRef::device(device));
            }
            context.setStatus(std::format("Pasted {} device(s)", raw->createdDevices().size()));
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F)) frameSelection(context);

    // Arrow keys nudge, one grid step at a time.
    const float step = io.KeyShift ? context.application.project().layout().gridSize : 1.0f;
    Vec2 nudge;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) nudge.x -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) nudge.x += step;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) nudge.y -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) nudge.y += step;

    if ((nudge.x != 0.0f || nudge.y != 0.0f) && !selection.devices().empty()) {
        context.application.commands().run(
            std::make_unique<commands::MoveDevicesCommand>(selection.devices(), nudge));
    }
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------

void Canvas::drawContextMenu(UiContext& context) {
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !isPanning_) {
        const Vec2 world = toWorld(ImGui::GetIO().MousePos);
        app::Selection& selection = context.application.selection();

        if (const DeviceId device = deviceAt(context, world); device.isValid()) {
            if (!selection.contains(ObjectRef::device(device))) {
                selection.select(ObjectRef::device(device));
            }
        } else if (const LinkId link = linkAt(context, world); link.isValid()) {
            selection.select(ObjectRef::link(link));
        }
        ImGui::OpenPopup("canvas-context");
    }

    if (!ImGui::BeginPopup("canvas-context")) return;

    app::Selection& selection = context.application.selection();
    core::Project& project = context.application.project();

    const auto devices = selection.devices();
    const auto links = selection.links();

    if (devices.size() == 1) {
        if (const Device* device = project.network().findDevice(devices.front())) {
            ImGui::SeparatorText(device->name().c_str());

            if (ImGui::MenuItem("Open console")) {
                context.consoleDevice = device->id();
                context.application.shell().attachTo(device->id());
                context.showConsole = true;
            }
            if (ImGui::MenuItem("Lock position", nullptr, project.layout().isLocked(device->id()))) {
                project.layout().setLocked(device->id(), !project.layout().isLocked(device->id()));
            }
        }
    } else if (!devices.empty()) {
        ImGui::SeparatorText(std::format("{} devices", devices.size()).c_str());
    }

    if (!links.empty()) {
        Link* link = project.network().findLink(links.front());
        if (link != nullptr && links.size() == 1) {
            ImGui::SeparatorText("Link");
            const bool enabled = link->isEnabled();
            if (ImGui::MenuItem(enabled ? "Disable (simulate a cable fault)" : "Enable")) {
                context.application.commands().run(
                    std::make_unique<commands::SetLinkEnabledCommand>(link->id(), !enabled));
            }
        }
    }

    if (!selection.empty()) {
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, !devices.empty())) clipboard_ = devices;
        if (ImGui::MenuItem("Delete", "Del")) context.application.deleteSelection();
    }

    if (devices.size() >= 2) {
        ImGui::Separator();
        if (ImGui::BeginMenu("Align")) {
            if (ImGui::MenuItem("Left")) alignSelection(context, Axis::Horizontal, AlignMode::Min);
            if (ImGui::MenuItem("Horizontal centre")) alignSelection(context, Axis::Horizontal, AlignMode::Center);
            if (ImGui::MenuItem("Right")) alignSelection(context, Axis::Horizontal, AlignMode::Max);
            ImGui::Separator();
            if (ImGui::MenuItem("Top")) alignSelection(context, Axis::Vertical, AlignMode::Min);
            if (ImGui::MenuItem("Vertical centre")) alignSelection(context, Axis::Vertical, AlignMode::Center);
            if (ImGui::MenuItem("Bottom")) alignSelection(context, Axis::Vertical, AlignMode::Max);
            ImGui::EndMenu();
        }
        if (devices.size() >= 3 && ImGui::BeginMenu("Distribute")) {
            if (ImGui::MenuItem("Horizontally")) distributeSelection(context, Axis::Horizontal);
            if (ImGui::MenuItem("Vertically")) distributeSelection(context, Axis::Vertical);
            ImGui::EndMenu();
        }
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Frame all", "F")) frameAll(context);
    if (ImGui::MenuItem("Reset zoom")) resetZoom();
    ImGui::MenuItem("Show minimap", nullptr, &context.showMinimap);
    ImGui::MenuItem("Show grid", nullptr, &project.layout().gridVisible);
    ImGui::MenuItem("Snap to grid", nullptr, &project.layout().snapToGrid);

    ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// View helpers
// ---------------------------------------------------------------------------

void Canvas::frameAll(UiContext& context) {
    const core::Project& project = context.application.project();
    if (project.network().deviceCount() == 0) {
        offset_ = Vec2{};
        setZoom(1.0f);
        return;
    }

    const Rect bounds = project.layout().boundingBox().expanded(140.0f);
    offset_ = bounds.center();

    if (size_.x > 1.0f && size_.y > 1.0f && bounds.width() > 1.0f && bounds.height() > 1.0f) {
        setZoom(std::min(size_.x / bounds.width(), size_.y / bounds.height()));
    }
}

void Canvas::frameSelection(UiContext& context) {
    const auto devices = context.application.selection().devices();
    if (devices.empty()) {
        frameAll(context);
        return;
    }

    const Layout& layout = context.application.project().layout();
    Rect bounds = deviceBounds(layout.position(devices.front()));
    for (const DeviceId device : devices) {
        const Rect box = deviceBounds(layout.position(device));
        bounds.expandToInclude(box.min);
        bounds.expandToInclude(box.max);
    }
    bounds = bounds.expanded(140.0f);

    offset_ = bounds.center();
    if (size_.x > 1.0f && size_.y > 1.0f) {
        setZoom(std::min(size_.x / bounds.width(), size_.y / bounds.height()));
    }
}

void Canvas::alignSelection(UiContext& context, Axis axis, AlignMode mode) {
    const auto devices = context.application.selection().devices();
    if (devices.size() < 2) return;

    const Layout& layout = context.application.project().layout();
    const auto valueOf = [&](DeviceId device) {
        const Vec2 position = layout.position(device);
        return axis == Axis::Horizontal ? position.x : position.y;
    };

    float lowest = valueOf(devices.front());
    float highest = lowest;
    for (const DeviceId device : devices) {
        lowest = std::min(lowest, valueOf(device));
        highest = std::max(highest, valueOf(device));
    }

    // Centring uses the midpoint of the extremes rather than the mean, so a
    // cluster of devices on one side does not drag the line towards itself.
    const float target = mode == AlignMode::Min     ? lowest
                         : mode == AlignMode::Max   ? highest
                                                    : (lowest + highest) * 0.5f;

    std::vector<std::pair<DeviceId, Vec2>> positions;
    positions.reserve(devices.size());
    for (const DeviceId device : devices) {
        Vec2 position = layout.position(device);
        if (axis == Axis::Horizontal) position.x = target;
        else                          position.y = target;
        positions.emplace_back(device, position);
    }

    context.application.commands().run(
        std::make_unique<commands::SetDevicePositionsCommand>(std::move(positions), "Align devices"));
}

void Canvas::distributeSelection(UiContext& context, Axis axis) {
    auto devices = context.application.selection().devices();
    if (devices.size() < 3) return;

    const Layout& layout = context.application.project().layout();
    const auto valueOf = [&](DeviceId device) {
        const Vec2 position = layout.position(device);
        return axis == Axis::Horizontal ? position.x : position.y;
    };

    std::sort(devices.begin(), devices.end(),
              [&](DeviceId a, DeviceId b) { return valueOf(a) < valueOf(b); });

    const float first = valueOf(devices.front());
    const float span = valueOf(devices.back()) - first;
    const float step = span / static_cast<float>(devices.size() - 1);

    std::vector<std::pair<DeviceId, Vec2>> positions;
    positions.reserve(devices.size());
    for (std::size_t i = 0; i < devices.size(); ++i) {
        Vec2 position = layout.position(devices[i]);
        const float value = first + step * static_cast<float>(i);
        if (axis == Axis::Horizontal) position.x = value;
        else                          position.y = value;
        positions.emplace_back(devices[i], position);
    }

    context.application.commands().run(
        std::make_unique<commands::SetDevicePositionsCommand>(std::move(positions),
                                                              "Distribute devices"));
}

} // namespace tnp::ui
