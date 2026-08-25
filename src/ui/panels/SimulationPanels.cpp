// The panels that drive and observe a run: playback controls, the packet
// capture with its protocol breakdown, the console, and the test suite.

#include "core/devices/Ipv4Stack.h"
#include "simulation/PacketDecoder.h"
#include "ui/panels/PanelSupport.h"
#include "ui/panels/Panels.h"

#include "utilities/StringUtilities.h"

#include <format>

namespace tnp::ui {

using namespace core;

// ---------------------------------------------------------------------------
// Simulation controls
// ---------------------------------------------------------------------------

void SimulationPanel::draw(UiContext& context) {
    sim::Simulator& simulator = context.application.simulator();
    core::Project& project = context.application.project();

    const sim::SimulationState state = simulator.state();

    const ImU32 stateColor = state == sim::SimulationState::Running   ? theme().success
                             : state == sim::SimulationState::Paused  ? theme().warning
                                                                      : theme().textSubtle;
    coloredText(stateColor, std::string{sim::simulationStateName(state)});
    ImGui::SameLine();
    subtleText(std::format("at {}", formatSimTime(simulator.now())));

    ImGui::Spacing();

    const float buttonWidth = (ImGui::GetContentRegionAvail().x - 24.0f) / 4.0f;

    if (state == sim::SimulationState::Running) {
        if (ImGui::Button("Pause", ImVec2(buttonWidth, 0))) simulator.pause();
    } else if (ImGui::Button("Play", ImVec2(buttonWidth, 0))) {
        simulator.start();
    }

    ImGui::SameLine();
    if (ImGui::Button("Step", ImVec2(buttonWidth, 0))) {
        if (state == sim::SimulationState::Running) simulator.pause();
        simulator.step();
    }
    helpMarker("Processes exactly one event. Stepping is how an exchange is read one message "
               "at a time.");

    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(buttonWidth, 0))) simulator.stop();

    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(buttonWidth, 0))) {
        simulator.reset();
        context.application.learning().clear();
        context.inspectedPacket = PacketId{};
    }
    helpMarker("Clears caches, packet history and the event log. Configuration is untouched.");

    ImGui::Spacing();

    auto speed = static_cast<float>(project.simulationSettings().speedMultiplier);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##speed", &speed, 0.01f, 4.0f, "Speed x%.2f",
                           ImGuiSliderFlags_Logarithmic)) {
        project.simulationSettings().speedMultiplier = speed;
        simulator.applySettings(project.simulationSettings());
    }
    subtleText("Simulated seconds per real second");

    ImGui::Spacing();
    ImGui::SeparatorText("Statistics");

    const sim::SimulationStatistics& statistics = simulator.statistics();
    if (beginFieldTable("simulation-stats")) {
        fieldRow("Events processed", std::to_string(statistics.eventsProcessed));
        fieldRow("Events pending", std::to_string(simulator.pendingEventCount()));
        fieldRow("Packets created", std::to_string(statistics.packetsCreated));
        fieldRow("Frames sent", std::to_string(statistics.framesTransmitted));
        fieldRow("Frames delivered", std::to_string(statistics.framesDelivered));
        fieldRow("Frames dropped", std::to_string(statistics.framesDropped));
        fieldRow("In flight", std::to_string(simulator.packetsInFlight().size()));
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Send a ping");

    // A quick way to generate traffic without opening a console.
    const auto devices = context.application.selection().devices();
    if (devices.empty()) {
        subtleText("Select a device on the canvas to ping from it.");
        return;
    }

    Device* source = project.network().findDevice(devices.front());
    if (source == nullptr || source->ipv4Stack() == nullptr) {
        subtleText("The selected device has no IPv4 stack.");
        return;
    }

    static std::string target;
    static int count = 4;

    ImGui::TextUnformatted(std::format("From {}", source->name()).c_str());
    ImGui::SetNextItemWidth(-1.0f);
    const bool submitted = ImGui::InputTextWithHint("##ping-target", "Address or device name",
                                                    &target, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SetNextItemWidth(110.0f);
    ImGui::InputInt("Probes", &count);
    count = std::clamp(count, 1, 100);

    ImGui::SameLine();
    if (ImGui::Button("Ping") || submitted) {
        std::optional<Ipv4Address> destination = Ipv4Address::parse(target);
        if (!destination) {
            if (const Device* named = project.network().findDeviceByName(target)) {
                for (const auto& iface : named->interfaces()) {
                    if (const auto address = iface->primaryIpv4()) {
                        destination = address->address();
                        break;
                    }
                }
            }
        }

        if (!destination) {
            context.setStatus(std::format("'{}' is neither an address nor a device", target), true);
        } else {
            PingRequest request;
            request.destination = *destination;
            request.count = static_cast<u32>(count);

            const auto ping = simulator.ping(source->id(), request);
            if (!ping) context.setStatus(ping.message(), true);
            else       context.setStatus(std::format("Pinging {}", destination->toString()));
        }
    }
}

// ---------------------------------------------------------------------------
// Packets
// ---------------------------------------------------------------------------

void PacketPanel::draw(UiContext& context) {
    ImGui::Checkbox("Follow", &autoScroll_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##packet-filter", "Filter", &filter_);

    ImGui::Separator();

    const float listHeight = ImGui::GetContentRegionAvail().y * 0.45f;
    if (ImGui::BeginChild("packet-list", ImVec2(0, listHeight))) drawList(context);
    ImGui::EndChild();

    ImGui::Separator();

    if (ImGui::BeginChild("packet-inspector")) drawInspector(context);
    ImGui::EndChild();
}

void PacketPanel::drawList(UiContext& context) {
    const sim::Simulator& simulator = context.application.simulator();
    const sim::PacketRegistry& packets = simulator.packets();

    if (packets.order().empty()) {
        emptyState("No packets captured yet.");
        return;
    }

    const std::string needle = strings::toLower(filter_);

    if (!ImGui::BeginTable("packets", 4,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("From", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Protocol", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Summary");
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    for (const PacketId id : packets.order()) {
        const sim::PacketRecord* record = packets.find(id);
        if (record == nullptr) continue;

        if (!needle.empty() && strings::toLower(record->summary).find(needle) == std::string::npos) {
            continue;
        }

        ImGui::PushID(id.toShortString().c_str());
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        subtleText(formatSimTime(record->createdAt));

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(simulator.deviceName(record->origin).c_str());

        ImGui::TableNextColumn();
        coloredText(packetColor(record->category), std::string{frameCategoryName(record->category)});

        ImGui::TableNextColumn();
        if (ImGui::Selectable(record->summary.c_str(), context.inspectedPacket == id,
                              ImGuiSelectableFlags_SpanAllColumns)) {
            context.inspectedPacket = id;
        }

        ImGui::PopID();
    }

    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndTable();
}

void PacketPanel::drawInspector(UiContext& context) {
    const sim::Simulator& simulator = context.application.simulator();
    const sim::PacketRecord* record = simulator.packets().find(context.inspectedPacket);

    if (record == nullptr) {
        emptyState("Select a packet to take it apart.");
        return;
    }

    ImGui::TextUnformatted(std::format("Packet {}", record->id.toShortString()).c_str());
    ImGui::SameLine();
    subtleText(std::format("{} bytes on the wire", record->size()));

    ImGui::Spacing();

    // --- Path --------------------------------------------------------------
    if (ImGui::CollapsingHeader("Path", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const sim::PacketHop& hop : record->hops) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme().textSubtle);
            ImGui::TextUnformatted(formatSimTime(hop.time).c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextUnformatted(std::format("{} {}", simulator.deviceName(hop.device), hop.action).c_str());
            if (!hop.detail.empty()) {
                ImGui::SameLine();
                subtleText(std::format("- {}", hop.detail));
            }
        }
    }

    // --- Protocol layers ---------------------------------------------------
    const sim::DecodedPacket decoded = sim::decodePacket(record->bytes);

    for (const sim::DecodedLayer& layer : decoded.layers) {
        if (!ImGui::CollapsingHeader(layer.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) continue;

        subtleText(layer.summary);

        if (!ImGui::BeginTable(layer.name.c_str(), 3,
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            continue;
        }
        ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Notes");

        for (const sim::DecodedField& field : layer.fields) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            subtleText(field.name);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(field.value.c_str());
            ImGui::TableNextColumn();
            if (field.detail == "INCORRECT") coloredText(theme().error, field.detail);
            else if (!field.detail.empty())  subtleText(field.detail);
        }
        ImGui::EndTable();
    }

    if (!decoded.problem.empty()) {
        coloredText(theme().warning, decoded.problem);
    }

    // --- Raw bytes ---------------------------------------------------------
    if (ImGui::CollapsingHeader("Bytes")) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme().textSubtle);
        ImGui::TextUnformatted(strings::hexDump(record->bytes).c_str());
        ImGui::PopStyleColor();
    }
}

// ---------------------------------------------------------------------------
// Console
// ---------------------------------------------------------------------------

void ConsolePanel::appendLine(cli::ShellLine line) {
    lines_.push_back(std::move(line));

    // Bounded: a long session must not grow without limit.
    constexpr std::size_t kMaxLines = 4000;
    if (lines_.size() > kMaxLines) {
        lines_.erase(lines_.begin(), lines_.begin() + static_cast<std::ptrdiff_t>(kMaxLines / 4));
    }
    scrollToBottom_ = true;
}

void ConsolePanel::draw(UiContext& context) {
    cli::DeviceShell& shell = context.application.shell();
    core::Project& project = context.application.project();

    // Device picker.
    const Device* attached = project.network().findDevice(shell.attachedDevice());
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("##console-device", attached ? attached->name().c_str() : "No device")) {
        for (const auto& device : project.network().devices()) {
            if (ImGui::Selectable(device->name().c_str(), device->id() == shell.attachedDevice())) {
                shell.attachTo(device->id());
                context.consoleDevice = device->id();
                lines_.clear();
                appendLine(cli::ShellLine{std::format("Connected to {}. Type '?' for help.",
                                                      device->name()),
                                          false, false});
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) lines_.clear();

    ImGui::Separator();

    for (cli::ShellLine& line : shell.drainEvents()) appendLine(std::move(line));

    const float inputHeight = ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("console-scroll", ImVec2(0, -inputHeight), ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const cli::ShellLine& line : lines_) {
            const ImU32 color = line.isError  ? theme().error
                                : line.isEvent ? theme().success
                                               : theme().text;
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(line.text.c_str());
            ImGui::PopStyleColor();
        }
        if (scrollToBottom_) {
            ImGui::SetScrollHereY(1.0f);
            scrollToBottom_ = false;
        }
    }
    ImGui::EndChild();

    // Prompt.
    ImGui::TextUnformatted(shell.prompt().c_str());
    ImGui::SameLine();

    ImGui::SetNextItemWidth(-1.0f);
    const bool submitted = ImGui::InputText("##console-input", &input_,
                                            ImGuiInputTextFlags_EnterReturnsTrue);

    if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
        ImGui::SetKeyboardFocusHere(-1);
    }

    if (submitted && !input_.empty()) {
        appendLine(cli::ShellLine{shell.prompt() + " " + input_, false, false});

        const cli::ShellResponse response = shell.execute(input_);
        for (const cli::ShellLine& line : response.lines) appendLine(line);

        input_.clear();
        historyCursor_ = -1;
        ImGui::SetKeyboardFocusHere(-1);
    }

    // Command history with the up and down arrows.
    if (ImGui::IsItemFocused() && !shell.history().empty()) {
        const int size = static_cast<int>(shell.history().size());
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            historyCursor_ = historyCursor_ < 0 ? size - 1 : std::max(0, historyCursor_ - 1);
            input_ = shell.history()[static_cast<std::size_t>(historyCursor_)];
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && historyCursor_ >= 0) {
            ++historyCursor_;
            if (historyCursor_ >= size) {
                historyCursor_ = -1;
                input_.clear();
            } else {
                input_ = shell.history()[static_cast<std::size_t>(historyCursor_)];
            }
        }
    }
}

} // namespace tnp::ui
