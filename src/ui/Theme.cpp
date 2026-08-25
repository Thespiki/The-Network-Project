#include "ui/Theme.h"

namespace tnp::ui {
namespace {

/// Colours are written as 0xAABBGGRR to match ImGui's packed layout, so nothing
/// is converted per frame.
constexpr ImU32 rgba(u8 r, u8 g, u8 b, u8 a = 255) {
    return (static_cast<ImU32>(a) << 24) | (static_cast<ImU32>(b) << 16) |
           (static_cast<ImU32>(g) << 8) | static_cast<ImU32>(r);
}

Theme makeTheme() {
    Theme value{};

    value.canvasBackground = rgba(0x14, 0x17, 0x1C);
    value.canvasGridMinor = rgba(0x1E, 0x23, 0x2B);
    value.canvasGridMajor = rgba(0x2A, 0x31, 0x3C);
    value.panelBackground = rgba(0x18, 0x1C, 0x22);

    value.text = rgba(0xE4, 0xE9, 0xF0);
    value.textSubtle = rgba(0x93, 0xA1, 0xB3);
    value.textDisabled = rgba(0x5C, 0x66, 0x75);

    value.deviceFill = rgba(0x22, 0x28, 0x32);
    value.deviceFillHovered = rgba(0x2A, 0x32, 0x3E);
    value.deviceBorder = rgba(0x3A, 0x45, 0x55);
    value.deviceBorderSelected = rgba(0x4C, 0x9A, 0xE8);
    value.deviceShadow = rgba(0x00, 0x00, 0x00, 0x60);

    value.link = rgba(0x5F, 0x7D, 0x95);
    value.linkSelected = rgba(0x4C, 0x9A, 0xE8);
    value.linkDown = rgba(0x8A, 0x4B, 0x4B);
    value.linkPending = rgba(0x4C, 0x9A, 0xE8, 0xB0);

    value.accent = rgba(0x4C, 0x9A, 0xE8);
    value.success = rgba(0x4F, 0xB0, 0x7E);
    value.warning = rgba(0xD6, 0xA1, 0x3C);
    value.error = rgba(0xD9, 0x5F, 0x5F);
    value.info = rgba(0x6E, 0x8B, 0xA8);

    value.selectionBox = rgba(0x4C, 0x9A, 0xE8, 0xC0);
    value.selectionBoxFill = rgba(0x4C, 0x9A, 0xE8, 0x28);

    return value;
}

ImVec4 toVec4(ImU32 color) {
    return ImGui::ColorConvertU32ToFloat4(color);
}

} // namespace

const Theme& theme() {
    static const Theme value = makeTheme();
    return value;
}

void applyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    // Geometry: square-ish and compact, the way engineering tools look. Rounded
    // enough not to feel harsh, not so rounded that it reads as a consumer app.
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 5);
    style.CellPadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 5);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextPadding = ImVec2(16, 4);

    const Theme& colors = theme();
    ImVec4* target = style.Colors;

    target[ImGuiCol_Text] = toVec4(colors.text);
    target[ImGuiCol_TextDisabled] = toVec4(colors.textDisabled);

    target[ImGuiCol_WindowBg] = toVec4(colors.panelBackground);
    target[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    target[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.13f, 0.98f);

    target[ImGuiCol_Border] = ImVec4(0.18f, 0.21f, 0.26f, 1.00f);
    target[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    target[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.15f, 0.19f, 1.00f);
    target[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.21f, 0.27f, 1.00f);
    target[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.26f, 0.33f, 1.00f);

    target[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
    target[ImGuiCol_TitleBgActive] = ImVec4(0.11f, 0.13f, 0.17f, 1.00f);
    target[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);

    target[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);

    target[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.09f, 0.12f, 0.60f);
    target[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.28f, 0.35f, 1.00f);
    target[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.35f, 0.43f, 1.00f);
    target[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.42f, 0.51f, 1.00f);

    target[ImGuiCol_CheckMark] = toVec4(colors.accent);
    target[ImGuiCol_SliderGrab] = toVec4(colors.accent);
    target[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.68f, 0.95f, 1.00f);

    target[ImGuiCol_Button] = ImVec4(0.17f, 0.20f, 0.26f, 1.00f);
    target[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.28f, 0.36f, 1.00f);
    target[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.35f, 0.45f, 1.00f);

    target[ImGuiCol_Header] = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
    target[ImGuiCol_HeaderHovered] = ImVec4(0.23f, 0.29f, 0.37f, 1.00f);
    target[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.34f, 0.44f, 1.00f);

    target[ImGuiCol_Separator] = ImVec4(0.18f, 0.21f, 0.26f, 1.00f);
    target[ImGuiCol_SeparatorHovered] = toVec4(colors.accent);
    target[ImGuiCol_SeparatorActive] = toVec4(colors.accent);

    target[ImGuiCol_ResizeGrip] = ImVec4(0.24f, 0.28f, 0.35f, 0.60f);
    target[ImGuiCol_ResizeGripHovered] = toVec4(colors.accent);
    target[ImGuiCol_ResizeGripActive] = toVec4(colors.accent);

    target[ImGuiCol_Tab] = ImVec4(0.11f, 0.13f, 0.17f, 1.00f);
    target[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.25f, 0.32f, 1.00f);
    target[ImGuiCol_TabSelected] = ImVec4(0.17f, 0.21f, 0.27f, 1.00f);
    target[ImGuiCol_TabDimmed] = ImVec4(0.09f, 0.10f, 0.13f, 1.00f);
    target[ImGuiCol_TabDimmedSelected] = ImVec4(0.13f, 0.15f, 0.19f, 1.00f);

    target[ImGuiCol_DockingPreview] = ImVec4(0.30f, 0.60f, 0.91f, 0.55f);
    target[ImGuiCol_DockingEmptyBg] = toVec4(colors.canvasBackground);

    target[ImGuiCol_TableHeaderBg] = ImVec4(0.13f, 0.15f, 0.19f, 1.00f);
    target[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
    target[ImGuiCol_TableBorderLight] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    target[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    target[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);

    target[ImGuiCol_TextSelectedBg] = ImVec4(0.30f, 0.60f, 0.91f, 0.35f);
    target[ImGuiCol_NavCursor] = toVec4(colors.accent);
}

ImU32 deviceAccent(core::DeviceType type) {
    switch (core::deviceTypeCategory(type)) {
        case core::DeviceCategory::Computers:      return rgba(0x4C, 0x8B, 0xD8);
        case core::DeviceCategory::Networking:     return rgba(0x3F, 0xAE, 0x8F);
        case core::DeviceCategory::Security:       return rgba(0xCF, 0x6B, 0x5A);
        case core::DeviceCategory::Wireless:       return rgba(0xA1, 0x73, 0xD1);
        case core::DeviceCategory::Infrastructure: return rgba(0xC9, 0xA2, 0x4A);
    }
    return rgba(0x4C, 0x8B, 0xD8);
}

ImU32 packetColor(core::FrameCategory category) {
    switch (category) {
        case core::FrameCategory::Arp:       return rgba(0xE0, 0xB0, 0x45);
        case core::FrameCategory::Icmp:      return rgba(0x5B, 0xC8, 0x8A);
        case core::FrameCategory::IcmpError: return rgba(0xE0, 0x6C, 0x5C);
        case core::FrameCategory::Tcp:       return rgba(0x6A, 0x9F, 0xE8);
        case core::FrameCategory::Udp:       return rgba(0x8E, 0x7F, 0xE0);
        case core::FrameCategory::Dhcp:      return rgba(0xD9, 0x8C, 0xC8);
        case core::FrameCategory::Dns:       return rgba(0x5C, 0xC0, 0xD0);
        case core::FrameCategory::Ipv6:      return rgba(0x7A, 0xC8, 0xC8);
        default:                             return rgba(0x9A, 0xA6, 0xB4);
    }
}

ImU32 severityColor(validation::Severity severity) {
    switch (severity) {
        case validation::Severity::Error:   return theme().error;
        case validation::Severity::Warning: return theme().warning;
        case validation::Severity::Info:    return theme().info;
    }
    return theme().info;
}

const char* severityGlyph(validation::Severity severity) {
    switch (severity) {
        case validation::Severity::Error:   return "x";
        case validation::Severity::Warning: return "!";
        case validation::Severity::Info:    return "i";
    }
    return "i";
}

} // namespace tnp::ui
