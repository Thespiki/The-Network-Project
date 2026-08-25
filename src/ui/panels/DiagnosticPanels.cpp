// Problems, log, events and learning: the four views onto what the engine and
// the validator have to say.

#include "ui/panels/PanelSupport.h"
#include "ui/panels/Panels.h"

#include "utilities/StringUtilities.h"

#include <format>

namespace tnp::ui {

using namespace core;

// ---------------------------------------------------------------------------
// Problems
// ---------------------------------------------------------------------------

void ProblemsPanel::draw(UiContext& context) {
    const validation::ValidationReport& report = context.application.validationReport();

    ImGui::Checkbox(std::format("Errors ({})", report.errorCount()).c_str(), &showErrors_);
    ImGui::SameLine();
    ImGui::Checkbox(std::format("Warnings ({})", report.warningCount()).c_str(), &showWarnings_);
    ImGui::SameLine();
    ImGui::Checkbox(std::format("Notes ({})", report.infoCount()).c_str(), &showInfo_);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##problem-filter", "Filter", &filter_);

    ImGui::Separator();

    if (report.isClean()) {
        emptyState("No problems found. Validation runs automatically after every change.");
        return;
    }

    const std::string needle = strings::toLower(filter_);
    bool anyShown = false;

    if (!ImGui::BeginTable("problems", 3,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 22.0f);
    ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn("Problem");
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    for (const validation::ValidationIssue& issue : report.issues) {
        const bool visible = (issue.severity == validation::Severity::Error && showErrors_) ||
                             (issue.severity == validation::Severity::Warning && showWarnings_) ||
                             (issue.severity == validation::Severity::Info && showInfo_);
        if (!visible) continue;

        if (!needle.empty()) {
            const std::string haystack = strings::toLower(issue.message + " " + issue.subjectName +
                                                          " " + issue.code);
            if (haystack.find(needle) == std::string::npos) continue;
        }
        anyShown = true;

        ImGui::PushID(&issue);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        coloredText(severityColor(issue.severity), severityGlyph(issue.severity));

        ImGui::TableNextColumn();
        // Clicking a row selects the object, which is the point of the panel.
        if (ImGui::Selectable(issue.subjectName.c_str(), false, ImGuiSelectableFlags_SpanAllColumns) &&
            issue.subject.isValid()) {
            context.application.selection().select(issue.subject);
        }

        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", issue.message.c_str());
        if (!issue.suggestion.empty()) subtleText(issue.suggestion);
        subtleText(issue.code);

        ImGui::PopID();
    }

    ImGui::EndTable();

    if (!anyShown) subtleText("Nothing matches the current filters.");
}

// ---------------------------------------------------------------------------
// Log
// ---------------------------------------------------------------------------

void LogPanel::draw(UiContext& context) {
    const auto& buffer = context.application.logBuffer();
    if (!buffer) {
        emptyState("Logging is not initialised.");
        return;
    }

    static const char* kLevels[] = {"TRACE", "DEBUG", "INFO", "WARNING", "ERROR"};

    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("##level", &minimumLevel_, kLevels, IM_ARRAYSIZE(kLevels))) {
        logging::Logger::instance().setMinimumLevel(static_cast<logging::Level>(minimumLevel_));
        context.application.config().logLevel = kLevels[minimumLevel_];
    }
    ImGui::SameLine();
    ImGui::Checkbox("Follow", &autoScroll_);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) buffer->clear();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##log-filter", "Filter", &filter_);

    ImGui::Separator();

    const std::string needle = strings::toLower(filter_);
    const auto records = buffer->snapshot();

    if (!ImGui::BeginChild("log-scroll", ImVec2(0, 0), ImGuiChildFlags_None,
                           ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }

    for (const logging::Record& record : records) {
        if (static_cast<int>(record.level) < minimumLevel_) continue;
        if (!needle.empty() &&
            strings::toLower(record.message).find(needle) == std::string::npos &&
            strings::toLower(record.category).find(needle) == std::string::npos) {
            continue;
        }

        const ImU32 color = record.level == logging::Level::Error     ? theme().error
                            : record.level == logging::Level::Warning ? theme().warning
                            : record.level == logging::Level::Info    ? theme().text
                                                                      : theme().textSubtle;

        ImGui::PushStyleColor(ImGuiCol_Text, theme().textDisabled);
        ImGui::TextUnformatted(record.wallClock.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();

        if (record.simTime) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme().info);
            ImGui::TextUnformatted(formatSimTime(*record.simTime).c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(std::format("[{}] {}", record.category, record.message).c_str());
        ImGui::PopStyleColor();
    }

    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void EventsPanel::draw(UiContext& context) {
    const sim::Simulator& simulator = context.application.simulator();

    static constexpr TraceCategory kCategories[] = {
        TraceCategory::Device, TraceCategory::Layer2,    TraceCategory::Arp,
        TraceCategory::Ipv4,   TraceCategory::Routing,   TraceCategory::Icmp,
        TraceCategory::Transport, TraceCategory::Service, TraceCategory::Policy,
        TraceCategory::Application};

    // Category toggles: an ARP exchange and a routing decision are different
    // stories, and being able to watch one at a time is the point.
    for (std::size_t i = 0; i < std::size(kCategories); ++i) {
        if (i != 0 && i % 5 != 0) ImGui::SameLine();
        ImGui::Checkbox(std::string{traceCategoryName(kCategories[i])}.c_str(), &categoryEnabled_[i]);
    }

    ImGui::Checkbox("Follow", &autoScroll_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##event-filter", "Filter", &filter_);

    ImGui::Separator();

    if (simulator.traceLog().empty()) {
        emptyState("No events yet. Start the simulation and send some traffic.");
        return;
    }

    const std::string needle = strings::toLower(filter_);

    if (!ImGui::BeginTable("events", 4,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Device", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthFixed, 170.0f);
    ImGui::TableSetupColumn("Detail");
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    for (const TraceEvent& event : simulator.traceLog()) {
        const auto categoryIndex = static_cast<std::size_t>(event.category());
        if (categoryIndex < std::size(kCategories) && !categoryEnabled_[categoryIndex]) continue;

        if (!needle.empty() && strings::toLower(event.summary).find(needle) == std::string::npos &&
            strings::toLower(std::string{traceKindName(event.kind)}).find(needle) ==
                std::string::npos) {
            continue;
        }

        ImGui::PushID(static_cast<int>(event.sequence));
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        subtleText(formatSimTime(event.time));

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(simulator.deviceName(event.device).c_str());

        ImGui::TableNextColumn();
        // Selecting an event selects its packet, so the inspector follows along.
        if (ImGui::Selectable(std::string{traceKindName(event.kind)}.c_str(),
                              event.packet.isValid() && event.packet == context.inspectedPacket,
                              ImGuiSelectableFlags_SpanAllColumns)) {
            if (event.packet.isValid()) {
                context.inspectedPacket = event.packet;
                context.showPackets = true;
            }
            if (event.device.isValid()) {
                context.application.selection().select(ObjectRef::device(event.device));
            }
        }

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(event.summary.c_str());

        ImGui::PopID();
    }

    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndTable();
}

// ---------------------------------------------------------------------------
// Learning mode
// ---------------------------------------------------------------------------

void LearningPanel::draw(UiContext& context) {
    app::LearningNarrator& narrator = context.application.learning();

    bool enabled = narrator.isEnabled();
    if (ImGui::Checkbox("Explain what the network is doing", &enabled)) {
        context.application.setLearningModeEnabled(enabled);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Follow", &autoScroll_);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) narrator.clear();

    ImGui::Separator();

    if (!enabled) {
        emptyState("Turn this on, then run something - a ping, a DHCP request - and every step "
                   "will be explained here as it happens.");
        return;
    }
    if (narrator.steps().empty()) {
        emptyState("Waiting for the network to do something.");
        return;
    }

    if (!ImGui::BeginChild("learning-scroll")) {
        ImGui::EndChild();
        return;
    }

    for (const app::LearningStep& step : narrator.steps()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme().accent);
        ImGui::TextWrapped("%s", step.headline.c_str());
        ImGui::PopStyleColor();

        ImGui::Indent(10.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, theme().text);
        ImGui::TextWrapped("%s", step.explanation.c_str());
        ImGui::PopStyleColor();
        subtleText(std::format("{} - {}", formatSimTime(step.time),
                               traceCategoryName(step.category)));
        ImGui::Unindent(10.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

} // namespace tnp::ui
