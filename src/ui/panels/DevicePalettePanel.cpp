#include "core/devices/DeviceRegistry.h"
#include "ui/Icons.h"
#include "ui/panels/PanelSupport.h"
#include "ui/panels/Panels.h"

#include "app/SampleProject.h"
#include "utilities/StringUtilities.h"

#include <format>

namespace tnp::ui {

using namespace core;

void DevicePalettePanel::draw(UiContext& context) {
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##palette-filter", "Filter devices", &filter_);

    ImGui::Spacing();

    const DeviceRegistry& registry = context.application.deviceRegistry();
    const std::string needle = strings::toLower(filter_);

    constexpr DeviceCategory kCategories[] = {
        DeviceCategory::Computers, DeviceCategory::Networking, DeviceCategory::Security,
        DeviceCategory::Wireless, DeviceCategory::Infrastructure};

    ImDrawList* draw = ImGui::GetWindowDrawList();
    bool anyShown = false;

    for (const DeviceCategory category : kCategories) {
        auto types = registry.typesInCategory(category);

        if (!needle.empty()) {
            std::erase_if(types, [&](const DeviceTypeInfo& info) {
                return strings::toLower(info.displayName).find(needle) == std::string::npos &&
                       strings::toLower(info.description).find(needle) == std::string::npos;
            });
        }
        if (types.empty()) continue;
        anyShown = true;

        if (!ImGui::CollapsingHeader(std::string{deviceCategoryName(category)}.c_str(),
                                     ImGuiTreeNodeFlags_DefaultOpen)) {
            continue;
        }

        for (const DeviceTypeInfo& info : types) {
            ImGui::PushID(static_cast<int>(info.type));

            const float rowHeight = ImGui::GetTextLineHeight() * 2.1f;
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();

            // The whole row is the target, so a drag can start anywhere on it.
            const bool pressed = ImGui::Selectable("##entry", false, 0, ImVec2(0.0f, rowHeight));

            drawDeviceGlyph(draw, info.type, ImVec2(rowStart.x + 4.0f, rowStart.y + 3.0f),
                            rowHeight - 6.0f, deviceAccent(info.type));

            const float textX = rowStart.x + rowHeight + 4.0f;
            draw->AddText(ImVec2(textX, rowStart.y + 3.0f), theme().text, info.displayName.c_str());
            draw->AddText(nullptr, ImGui::GetFontSize() * 0.85f,
                          ImVec2(textX, rowStart.y + 3.0f + ImGui::GetTextLineHeight()),
                          theme().textSubtle, info.namePrefix.c_str());

            if (ImGui::BeginItemTooltip()) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
                ImGui::TextUnformatted(info.description.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            // Click to place at the centre of the view; drag to place precisely.
            if (pressed) {
                context.application.addDevice(info.type, context.viewCenter);
                context.setStatus(std::format("Added {}", info.displayName));
            }
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
                context.isDraggingNewDevice = true;
                context.draggedDeviceType = info.type;
            }

            ImGui::PopID();
        }
        ImGui::Spacing();
    }

    if (!anyShown) emptyState("No device matches that filter.");

    if (context.isDraggingNewDevice) {
        ImGui::SetTooltip("Drop on the canvas to place a %s",
                          std::string{deviceTypeDisplayName(context.draggedDeviceType)}.c_str());
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            // Released outside the canvas: the drag simply ends.
            context.isDraggingNewDevice = false;
        }
    }
}

} // namespace tnp::ui
