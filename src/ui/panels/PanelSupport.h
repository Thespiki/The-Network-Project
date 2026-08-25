#pragma once

// Small drawing helpers shared by the panels.
//
// They exist so panels stay about *what* they show rather than about ImGui
// bookkeeping, and so labels, spacing and colours stay consistent between them.

#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <string>
#include <string_view>

namespace tnp::ui {

/// A dimmed caption line.
inline void subtleText(const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme().textSubtle);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
}

/// A coloured status word, e.g. "up" in green.
inline void coloredText(ImU32 color, const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
}

/// A label/value row inside a two-column table.
inline void fieldRow(std::string_view label, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::PushStyleColor(ImGuiCol_Text, theme().textSubtle);
    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::PopStyleColor();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(value.c_str());
}

/// Begins a borderless two-column table for label/value rows.
inline bool beginFieldTable(const char* id) {
    return ImGui::BeginTable(id, 2,
                             ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX |
                                 ImGuiTableFlags_NoBordersInBody);
}

/// A help marker: "(?)" that explains something on hover.
inline void helpMarker(const char* description) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (!ImGui::BeginItemTooltip()) return;
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
    ImGui::TextUnformatted(description);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

/// A centred message for an empty panel, so a blank area never looks broken.
inline void emptyState(const std::string& message) {
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 size = ImGui::CalcTextSize(message.c_str());
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (available.x - size.x) * 0.5f,
                               ImGui::GetCursorPosY() + available.y * 0.4f));
    ImGui::PushStyleColor(ImGuiCol_Text, theme().textDisabled);
    ImGui::TextWrapped("%s", message.c_str());
    ImGui::PopStyleColor();
}

/// A small right-aligned button, for row actions such as "remove".
inline bool smallDangerButton(const char* label) {
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(90, 40, 40, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(130, 55, 55, 255));
    const bool pressed = ImGui::SmallButton(label);
    ImGui::PopStyleColor(2);
    return pressed;
}

} // namespace tnp::ui
