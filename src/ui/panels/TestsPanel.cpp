#include "commands/ProjectCommands.h"
#include "ui/panels/PanelSupport.h"
#include "ui/panels/Panels.h"

#include <algorithm>
#include <format>

namespace tnp::ui {

using namespace core;

namespace {

/// Resolves a test's destination for display: a device name where possible, the
/// literal address otherwise.
std::string describeDestination(const Network& network, const NetworkTest& test) {
    if (test.destinationDevice.isValid()) {
        const Device* device = network.findDevice(test.destinationDevice);
        return device != nullptr ? device->name() : std::string{"(missing device)"};
    }
    if (test.destinationAddress) return test.destinationAddress->toString();
    return "(not set)";
}

ImU32 statusColor(testing::TestStatus status) {
    switch (status) {
        case testing::TestStatus::Passed:  return theme().success;
        case testing::TestStatus::Failed:  return theme().error;
        case testing::TestStatus::Error:   return theme().warning;
        case testing::TestStatus::Skipped: return theme().textDisabled;
        case testing::TestStatus::NotRun:  return theme().textSubtle;
    }
    return theme().textSubtle;
}

} // namespace

void TestsPanel::draw(UiContext& context) {
    core::Project& project = context.application.project();
    const testing::TestRunSummary& lastRun = context.application.lastTestRun();

    if (ImGui::Button("Run all")) {
        const testing::TestRunSummary summary = context.application.runTests();
        context.setStatus(summary.summaryLine(),
                          summary.failedCount() + summary.errorCount() > 0);
    }
    helpMarker("Each test runs on a private copy of the project with cold caches, so results do "
               "not depend on what you were doing beforehand or on the order tests run in.");

    ImGui::SameLine();
    if (ImGui::Button("New test")) {
        NetworkTest test;
        test.name = std::format("Test {}", project.tests().size() + 1);
        if (!project.network().devices().empty()) {
            test.source = project.network().devices().front()->id();
        }

        auto command = std::make_unique<commands::AddTestCommand>(test);
        editing_ = command->testId();
        if (context.application.commands().run(std::move(command))) showEditor_ = true;
    }

    if (!lastRun.results.empty()) {
        ImGui::SameLine();
        subtleText(lastRun.summaryLine());
    }

    ImGui::Separator();

    if (project.tests().empty()) {
        emptyState("No tests yet. A test asserts that one device can - or cannot - reach another, "
                   "and is stored with the project.");
        return;
    }

    const float editorHeight = showEditor_ ? ImGui::GetContentRegionAvail().y * 0.45f : 0.0f;

    if (ImGui::BeginChild("test-list", ImVec2(0, -editorHeight))) {
        for (const NetworkTest& test : project.tests()) {
            ImGui::PushID(test.id.toShortString().c_str());

            const auto result = std::find_if(lastRun.results.begin(), lastRun.results.end(),
                                             [&](const testing::TestResult& entry) {
                                                 return entry.testId == test.id;
                                             });
            const testing::TestStatus status = result == lastRun.results.end()
                                                   ? testing::TestStatus::NotRun
                                                   : result->status;

            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 dot = ImGui::GetCursorScreenPos();
            draw->AddCircleFilled(ImVec2(dot.x + 6.0f, dot.y + ImGui::GetTextLineHeight() * 0.5f + 3.0f),
                                  4.5f, statusColor(status), 10);
            ImGui::Dummy(ImVec2(16.0f, 0.0f));
            ImGui::SameLine();

            const bool open = ImGui::TreeNodeEx("##test", ImGuiTreeNodeFlags_SpanAvailWidth, "%s",
                                                test.name.c_str());
            if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                editing_ = test.id;
                showEditor_ = true;
            }

            if (open) {
                const Device* source = project.network().findDevice(test.source);
                subtleText(std::format("{} -> {}, expect {}",
                                       source != nullptr ? source->name() : "(missing device)",
                                       describeDestination(project.network(), test),
                                       testExpectationName(test.expectation)));
                if (!test.description.empty()) subtleText(test.description);

                if (result != lastRun.results.end()) {
                    coloredText(statusColor(status),
                                std::format("{}: {}", testStatusName(status), result->message));

                    for (const testing::TestStep& step : result->steps) {
                        coloredText(step.reached ? theme().success : theme().error,
                                    std::format("   {} {}", step.reached ? "ok" : "--",
                                                step.description));
                    }
                    if (!result->reason.empty() && !result->passed()) {
                        subtleText(std::format("   reason: {}", result->reason));
                    }
                }

                if (ImGui::SmallButton("Edit")) {
                    editing_ = test.id;
                    showEditor_ = true;
                }
                ImGui::SameLine();
                if (smallDangerButton("Delete")) {
                    context.application.commands().run(
                        std::make_unique<commands::DeleteTestCommand>(test.id));
                    if (editing_ == test.id) showEditor_ = false;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    if (showEditor_) {
        ImGui::Separator();
        drawEditor(context);
    }
}

void TestsPanel::drawEditor(UiContext& context) {
    core::Project& project = context.application.project();

    NetworkTest* test = project.findTest(editing_);
    if (test == nullptr) {
        showEditor_ = false;
        return;
    }

    // The buffers follow the test being edited, not the frame: refreshing them
    // every frame would erase whatever has just been typed.
    if (bufferOwner_ != editing_) {
        bufferOwner_ = editing_;
        nameBuffer_ = test->name;
        descriptionBuffer_ = test->description;
        addressBuffer_ = test->destinationAddress ? test->destinationAddress->toString()
                                                  : std::string{};
    }

    NetworkTest edited = *test;
    bool changed = false;

    ImGui::TextUnformatted("Edit test");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.0f);
    if (ImGui::SmallButton("Close")) showEditor_ = false;

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##test-name", &nameBuffer_);
    if (ImGui::IsItemDeactivatedAfterEdit() && nameBuffer_ != edited.name) {
        edited.name = nameBuffer_;
        changed = true;
    }
    subtleText("Name");

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##test-description", &descriptionBuffer_);
    if (ImGui::IsItemDeactivatedAfterEdit() && descriptionBuffer_ != edited.description) {
        edited.description = descriptionBuffer_;
        changed = true;
    }
    subtleText("Description");

    const Device* source = project.network().findDevice(edited.source);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##test-source", source != nullptr ? source->name().c_str() : "Choose")) {
        for (const auto& device : project.network().devices()) {
            // Only devices that can actually send a ping.
            if (device->ipv4Stack() == nullptr) continue;
            if (ImGui::Selectable(device->name().c_str(), device->id() == edited.source)) {
                edited.source = device->id();
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    subtleText("Source");

    const std::string destinationLabel = describeDestination(project.network(), edited);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##test-destination", destinationLabel.c_str())) {
        for (const auto& device : project.network().devices()) {
            if (ImGui::Selectable(device->name().c_str(), device->id() == edited.destinationDevice)) {
                edited.destinationDevice = device->id();
                edited.destinationAddress.reset();
                addressBuffer_.clear();
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    subtleText("Destination device");

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##test-address", "or an address, e.g. 203.0.113.5", &addressBuffer_);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (addressBuffer_.empty()) {
            if (edited.destinationAddress) {
                edited.destinationAddress.reset();
                changed = true;
            }
        } else if (const auto address = Ipv4Address::parse(addressBuffer_)) {
            if (edited.destinationAddress != address) {
                edited.destinationAddress = *address;
                edited.destinationDevice = DeviceId{};
                changed = true;
            }
        } else {
            context.setStatus(std::format("'{}' is not an IPv4 address", addressBuffer_), true);
        }
    }

    const bool expectReachable = edited.expectation == TestExpectation::Reachable;
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::BeginCombo("Expect", expectReachable ? "Reachable" : "Unreachable")) {
        if (ImGui::Selectable("Reachable", expectReachable)) {
            edited.expectation = TestExpectation::Reachable;
            changed = true;
        }
        if (ImGui::Selectable("Unreachable", !expectReachable)) {
            edited.expectation = TestExpectation::Unreachable;
            changed = true;
        }
        ImGui::EndCombo();
    }
    helpMarker("'Unreachable' is how a firewall rule or an access policy is proved to work.");

    int probes = static_cast<int>(edited.probeCount);
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("Probes", &probes, 1, 1)) {
        edited.probeCount = static_cast<u32>(std::clamp(probes, 1, 50));
        changed = true;
    }

    bool enabled = edited.enabled;
    if (ImGui::Checkbox("Enabled", &enabled)) {
        edited.enabled = enabled;
        changed = true;
    }

    if (changed) {
        context.application.commands().run(std::make_unique<commands::UpdateTestCommand>(edited));
    }
}

} // namespace tnp::ui
