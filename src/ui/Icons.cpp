#include "ui/Icons.h"

#include <cmath>

namespace tnp::ui {
namespace {

constexpr float kPi = 3.14159265358979323846f;

ImVec2 add(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
ImVec2 scale(ImVec2 v, float s) { return ImVec2(v.x * s, v.y * s); }

/// Maps a coordinate in the unit square (-1..1) to screen space.
ImVec2 point(ImVec2 center, float size, float x, float y) {
    return ImVec2(center.x + x * size * 0.5f, center.y + y * size * 0.5f);
}

void drawMonitor(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    draw->AddRect(point(center, size, -0.85f, -0.7f), point(center, size, 0.85f, 0.35f), color,
                  2.0f, 0, 1.6f);
    draw->AddLine(point(center, size, -0.3f, 0.35f), point(center, size, -0.3f, 0.7f), color, 1.6f);
    draw->AddLine(point(center, size, 0.3f, 0.35f), point(center, size, 0.3f, 0.7f), color, 1.6f);
    draw->AddLine(point(center, size, -0.6f, 0.7f), point(center, size, 0.6f, 0.7f), color, 1.6f);
}

void drawServer(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    for (int i = 0; i < 3; ++i) {
        const float top = -0.8f + static_cast<float>(i) * 0.55f;
        draw->AddRect(point(center, size, -0.7f, top), point(center, size, 0.7f, top + 0.4f), color,
                      2.0f, 0, 1.6f);
        draw->AddCircleFilled(point(center, size, -0.5f, top + 0.2f), size * 0.045f, color, 8);
    }
}

void drawRouter(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    // A puck with four arrows: the classic "this thing chooses a direction".
    draw->AddEllipse(center, ImVec2(size * 0.75f, size * 0.42f), color, 0.0f, 0, 1.6f);

    const float reach = 0.5f;
    const ImVec2 directions[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (const ImVec2& direction : directions) {
        const ImVec2 tip = point(center, size, direction.x * reach, direction.y * reach * 0.7f);
        draw->AddLine(center, tip, color, 1.5f);
        drawArrowHead(draw, tip, direction, size * 0.16f, color);
    }
}

void drawSwitch(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    draw->AddRect(point(center, size, -0.85f, -0.45f), point(center, size, 0.85f, 0.45f), color,
                  3.0f, 0, 1.6f);
    // Two pairs of crossing arrows, which is what distinguishes a bridge from a
    // router in every network diagram ever drawn.
    draw->AddLine(point(center, size, -0.5f, -0.18f), point(center, size, 0.5f, -0.18f), color, 1.4f);
    drawArrowHead(draw, point(center, size, 0.5f, -0.18f), ImVec2(1, 0), size * 0.14f, color);
    draw->AddLine(point(center, size, 0.5f, 0.18f), point(center, size, -0.5f, 0.18f), color, 1.4f);
    drawArrowHead(draw, point(center, size, -0.5f, 0.18f), ImVec2(-1, 0), size * 0.14f, color);
}

void drawLayer3Switch(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    drawSwitch(draw, center, size, color);
    draw->AddEllipse(point(center, size, 0.0f, -0.62f), ImVec2(size * 0.24f, size * 0.14f), color,
                     0.0f, 0, 1.4f);
}

void drawFirewall(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    // A brick wall: staggered courses read as a barrier at any size.
    const float left = -0.85f;
    const float right = 0.85f;
    draw->AddRect(point(center, size, left, -0.6f), point(center, size, right, 0.6f), color, 2.0f,
                  0, 1.6f);

    for (int row = 1; row < 3; ++row) {
        const float y = -0.6f + static_cast<float>(row) * 0.4f;
        draw->AddLine(point(center, size, left, y), point(center, size, right, y), color, 1.2f);
    }
    for (int row = 0; row < 3; ++row) {
        const float top = -0.6f + static_cast<float>(row) * 0.4f;
        const float offset = (row % 2 == 0) ? 0.0f : 0.28f;
        for (float x = left + 0.28f + offset; x < right - 0.05f; x += 0.56f) {
            draw->AddLine(point(center, size, x, top), point(center, size, x, top + 0.4f), color, 1.2f);
        }
    }
}

void drawAccessPoint(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    draw->AddRectFilled(point(center, size, -0.55f, 0.3f), point(center, size, 0.55f, 0.62f), color,
                        3.0f);
    // Radiating arcs.
    for (int i = 1; i <= 3; ++i) {
        const float radius = size * 0.16f * static_cast<float>(i);
        draw->PathArcTo(point(center, size, 0.0f, 0.3f), radius, kPi * 1.15f, kPi * 1.85f, 16);
        draw->PathStroke(color, 0, 1.5f);
    }
}

void drawHub(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    draw->AddRect(point(center, size, -0.85f, -0.4f), point(center, size, 0.85f, 0.4f), color, 3.0f,
                  0, 1.6f);
    // A hub has no idea where anything is: dots, no arrows.
    for (int i = -2; i <= 2; ++i) {
        draw->AddCircleFilled(point(center, size, static_cast<float>(i) * 0.3f, 0.0f), size * 0.05f,
                              color, 10);
    }
}

void drawCloud(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    draw->AddCircle(point(center, size, -0.4f, 0.1f), size * 0.24f, color, 16, 1.6f);
    draw->AddCircle(point(center, size, 0.0f, -0.15f), size * 0.30f, color, 16, 1.6f);
    draw->AddCircle(point(center, size, 0.42f, 0.1f), size * 0.24f, color, 16, 1.6f);
    draw->AddLine(point(center, size, -0.45f, 0.32f), point(center, size, 0.45f, 0.32f), color, 1.6f);
}

} // namespace

void drawArrowHead(ImDrawList* draw, ImVec2 tip, ImVec2 direction, float size, ImU32 color) {
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.0001f) return;

    const ImVec2 unit(direction.x / length, direction.y / length);
    const ImVec2 normal(-unit.y, unit.x);

    const ImVec2 back = add(tip, scale(unit, -size));
    draw->AddTriangleFilled(tip, add(back, scale(normal, size * 0.45f)),
                            add(back, scale(normal, -size * 0.45f)), color);
}

void drawDeviceIcon(ImDrawList* draw, core::DeviceType type, ImVec2 center, float size,
                    ImU32 color) {
    switch (type) {
        case core::DeviceType::Pc:           drawMonitor(draw, center, size, color); break;
        case core::DeviceType::Server:       drawServer(draw, center, size, color); break;
        case core::DeviceType::Router:       drawRouter(draw, center, size, color); break;
        case core::DeviceType::Switch:       drawSwitch(draw, center, size, color); break;
        case core::DeviceType::Layer3Switch: drawLayer3Switch(draw, center, size, color); break;
        case core::DeviceType::Firewall:     drawFirewall(draw, center, size, color); break;
        case core::DeviceType::AccessPoint:  drawAccessPoint(draw, center, size, color); break;
        case core::DeviceType::Hub:          drawHub(draw, center, size, color); break;
        case core::DeviceType::Cloud:        drawCloud(draw, center, size, color); break;
    }
}

void drawDeviceGlyph(ImDrawList* draw, core::DeviceType type, ImVec2 topLeft, float size,
                     ImU32 color) {
    drawDeviceIcon(draw, type, ImVec2(topLeft.x + size * 0.5f, topLeft.y + size * 0.5f), size, color);
}

} // namespace tnp::ui
