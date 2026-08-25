#include "ui/dialogs/Dialogs.h"

#include "app/ProjectFile.h"
#include "commands/LinkCommands.h"
#include "serialization/TnpCrypto.h"
#include "ui/panels/PanelSupport.h"
#include "utilities/FileSystem.h"
#include "utilities/StringUtilities.h"

#include <algorithm>
#include <format>
#include <system_error>

namespace tnp::ui {

using namespace core;

// ---------------------------------------------------------------------------
// FileDialog
// ---------------------------------------------------------------------------

void FileDialog::open(Mode mode, std::string title, std::vector<std::string> extensions,
                      std::string suggestedName) {
    mode_ = mode;
    title_ = std::move(title);
    extensions_ = std::move(extensions);
    fileName_ = std::move(suggestedName);
    error_.clear();
    result_.clear();

    if (directory_.empty()) {
        std::error_code ec;
        directory_ = std::filesystem::current_path(ec);
        if (ec) directory_ = files::userConfigDirectory();
    }

    refreshEntries();
    isOpen_ = true;
    shouldFocus_ = true;
    ImGui::OpenPopup(title_.c_str());
}

void FileDialog::refreshEntries() {
    entries_.clear();
    error_.clear();

    std::error_code ec;
    if (!std::filesystem::exists(directory_, ec)) {
        directory_ = files::userConfigDirectory();
    }

    std::filesystem::directory_iterator iterator{directory_, ec};
    if (ec) {
        error_ = std::format("Cannot read {}: {}", directory_.string(), ec.message());
        return;
    }

    for (const auto& item : iterator) {
        std::error_code entryError;
        const bool isDirectory = item.is_directory(entryError);
        if (entryError) continue;

        const std::string name = item.path().filename().string();
        if (!name.empty() && name.front() == '.') continue; // hidden entries

        if (!isDirectory && !extensions_.empty()) {
            const std::string extension = files::extensionOf(item.path());
            if (std::find(extensions_.begin(), extensions_.end(), extension) == extensions_.end()) {
                continue;
            }
        }

        Entry entry;
        entry.name = name;
        entry.isDirectory = isDirectory;
        if (!isDirectory) entry.size = item.file_size(entryError);
        entries_.push_back(std::move(entry));
    }

    // Directories first, then names, so a listing reads the way a file manager does.
    std::sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory;
        return strings::toLower(a.name) < strings::toLower(b.name);
    });
}

bool FileDialog::draw() {
    if (!isOpen_) return false;

    bool confirmed = false;
    ImGui::SetNextWindowSize(ImVec2(680, 460), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal(title_.c_str(), &isOpen_, ImGuiWindowFlags_NoSavedSettings)) {
        return false;
    }

    // Breadcrumb: current directory plus a way up.
    if (ImGui::Button("Up") && directory_.has_parent_path()) {
        directory_ = directory_.parent_path();
        refreshEntries();
    }
    ImGui::SameLine();
    if (ImGui::Button("Home")) {
        directory_ = files::userConfigDirectory().parent_path();
        refreshEntries();
    }
    ImGui::SameLine();
    subtleText(directory_.string());

    ImGui::Separator();

    const float listHeight = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2.4f;
    if (ImGui::BeginChild("entries", ImVec2(0, listHeight), ImGuiChildFlags_Borders)) {
        if (!error_.empty()) coloredText(theme().error, error_);

        for (const Entry& entry : entries_) {
            const std::string label = entry.isDirectory ? "[ " + entry.name + " ]" : entry.name;

            if (ImGui::Selectable(label.c_str(), fileName_ == entry.name)) {
                if (entry.isDirectory) {
                    directory_ /= entry.name;
                    refreshEntries();
                    break;
                }
                fileName_ = entry.name;
            }
            if (!entry.isDirectory && ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                fileName_ = entry.name;
                confirmed = true;
            }
        }
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth(-160.0f);
    if (shouldFocus_) {
        ImGui::SetKeyboardFocusHere();
        shouldFocus_ = false;
    }
    if (ImGui::InputTextWithHint("##filename", "File name", &fileName_,
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
        confirmed = true;
    }

    ImGui::SameLine();
    if (ImGui::Button(mode_ == Mode::Open ? "Open" : "Save", ImVec2(70, 0))) confirmed = true;
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(70, 0))) {
        isOpen_ = false;
        ImGui::CloseCurrentPopup();
    }

    if (confirmed && !fileName_.empty()) {
        result_ = directory_ / fileName_;

        if (mode_ == Mode::Open && !std::filesystem::exists(result_)) {
            error_ = std::format("{} does not exist", result_.filename().string());
            confirmed = false;
        } else {
            isOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
    } else {
        confirmed = false;
    }

    ImGui::EndPopup();
    return confirmed;
}

// ---------------------------------------------------------------------------
// InterfacePickerDialog
// ---------------------------------------------------------------------------

void InterfacePickerDialog::draw(UiContext& context) {
    if (context.requestInterfacePicker) {
        ImGui::OpenPopup("Connect devices");
        context.requestInterfacePicker = false;

        // Reset the choice when the pair changes, and preselect the first free
        // port on each side so the common case is one click.
        if (context.pickerSourceDevice != lastSource_ || context.pickerTargetDevice != lastTarget_) {
            sourceInterface_ = InterfaceId{};
            targetInterface_ = InterfaceId{};
            lastSource_ = context.pickerSourceDevice;
            lastTarget_ = context.pickerTargetDevice;
        }
    }

    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Connect devices", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    Network& network = context.application.project().network();
    Device* source = network.findDevice(context.pickerSourceDevice);
    Device* target = network.findDevice(context.pickerTargetDevice);

    if (source == nullptr || target == nullptr) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    const auto drawInterfaceList = [&](Device& device, InterfaceId& selected, const char* id) {
        ImGui::TextUnformatted(device.name().c_str());
        if (!ImGui::BeginListBox(id, ImVec2(230, 180))) return;

        for (const auto& iface : device.interfaces()) {
            if (!iface->isConnectable()) continue;

            const bool used = iface->isConnected();
            const std::string label = used ? std::format("{} (in use)", iface->shortName())
                                           : iface->shortName();

            // Preselect the first free port.
            if (!selected.isValid() && !used) selected = iface->id();

            ImGui::BeginDisabled(used);
            if (ImGui::Selectable(label.c_str(), selected == iface->id())) selected = iface->id();
            ImGui::EndDisabled();
        }
        ImGui::EndListBox();
    };

    drawInterfaceList(*source, sourceInterface_, "##source");
    ImGui::SameLine();
    drawInterfaceList(*target, targetInterface_, "##target");

    const Status compatible = network.canConnect(sourceInterface_, targetInterface_);
    if (!compatible) coloredText(theme().warning, compatible.message());

    ImGui::Separator();

    ImGui::BeginDisabled(!compatible);
    if (ImGui::Button("Connect", ImVec2(100, 0))) {
        const Status status = context.application.connectInterfaces(sourceInterface_, targetInterface_);
        if (status) context.setStatus(std::format("Connected {} to {}", source->name(), target->name()));
        else        context.setStatus(status.message(), true);

        sourceInterface_ = InterfaceId{};
        targetInterface_ = InterfaceId{};
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// PreferencesDialog
// ---------------------------------------------------------------------------

void PreferencesDialog::draw(UiContext& context) {
    if (context.requestPreferences) {
        ImGui::OpenPopup("Preferences");
        context.requestPreferences = false;
    }

    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Preferences", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    app::ApplicationConfig& config = context.application.config();

    ImGui::SeparatorText("Editing");
    ImGui::Checkbox("Show grid", &context.application.project().layout().gridVisible);
    ImGui::Checkbox("Snap to grid", &context.application.project().layout().snapToGrid);
    ImGui::Checkbox("Show minimap", &context.showMinimap);

    ImGui::SeparatorText("Autosave");
    ImGui::Checkbox("Autosave", &config.autosaveEnabled);
    helpMarker("Writes a recovery copy at the interval below. It is deleted when you save or "
               "close normally, so it only survives a crash.");

    ImGui::BeginDisabled(!config.autosaveEnabled);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderInt("Interval (s)", &config.autosaveIntervalSeconds, 15, 900);
    ImGui::EndDisabled();

    ImGui::SeparatorText("Learning mode");
    bool learning = context.application.learning().isEnabled();
    if (ImGui::Checkbox("Explain events in plain language", &learning)) {
        context.application.setLearningModeEnabled(learning);
    }

    ImGui::SeparatorText("Simulation");
    core::SimulationSettings& settings = context.application.project().simulationSettings();

    int traceLimit = static_cast<int>(settings.traceHistoryLimit);
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputInt("Event history", &traceLimit, 1000, 5000)) {
        settings.traceHistoryLimit = static_cast<u32>(std::clamp(traceLimit, 100, 500000));
        context.application.simulator().applySettings(settings);
    }

    int packetLimit = static_cast<int>(settings.packetHistoryLimit);
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputInt("Packet history", &packetLimit, 100, 1000)) {
        settings.packetHistoryLimit = static_cast<u32>(std::clamp(packetLimit, 50, 100000));
        context.application.simulator().applySettings(settings);
    }

    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(100, 0))) {
        if (const Status status = config.save(); !status) {
            context.setStatus(status.message(), true);
        }
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// AboutDialog
// ---------------------------------------------------------------------------

void AboutDialog::draw(UiContext& context) {
    if (context.requestAbout) {
        ImGui::OpenPopup("About TNP");
        context.requestAbout = false;
    }

    ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("About TNP", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    ImGui::TextUnformatted("The Network Project");
    subtleText(std::format("Version {}", TNP_VERSION_STRING));

    ImGui::Spacing();
    ImGui::TextWrapped("Design, configure, simulate and analyse computer networks. Packets carry "
                       "real bytes: headers are encoded, checksums are computed and verified, and "
                       "the inspector decodes what is actually on the wire.");

    ImGui::SeparatorText("What this build simulates");
    ImGui::BulletText("Ethernet with 802.1Q VLANs, MAC learning and flooding");
    ImGui::BulletText("ARP, including caching, retries and timeouts");
    ImGui::BulletText("IPv4 forwarding with longest-prefix match and TTL");
    ImGui::BulletText("ICMP echo, destination unreachable and time exceeded");
    ImGui::BulletText("DHCP allocation and an authoritative DNS zone");
    ImGui::BulletText("Ordered firewall policy on a routing firewall");

    ImGui::SeparatorText("What it does not, yet");
    ImGui::BulletText("TCP: the header codec exists, the state machine does not");
    ImGui::BulletText("OSPF: configuration is stored and reported, no adjacencies form");
    ImGui::BulletText("Spanning tree: a physical loop is reported, not broken");
    ImGui::BulletText("IPv4 fragmentation, NAT, IPv6 forwarding");
    ImGui::BulletText(serial::isEncryptionAvailable() ? "Encrypted projects"
                                                      : "Encrypted projects (.tnpenc)");

    ImGui::SeparatorText("Built with");
    subtleText("Dear ImGui, GLFW, nlohmann/json, Catch2");

    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(100, 0))) ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// UnsavedChangesDialog
// ---------------------------------------------------------------------------

void UnsavedChangesDialog::request(std::string reason) {
    reason_ = std::move(reason);
    shouldOpen_ = true;
}

UnsavedChangesDialog::Outcome UnsavedChangesDialog::draw(UiContext& context) {
    if (shouldOpen_) {
        ImGui::OpenPopup("Unsaved changes");
        shouldOpen_ = false;
        isOpen_ = true;
    }

    if (!ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return Outcome::None;
    }

    ImGui::TextWrapped("%s has unsaved changes.",
                       context.application.hasPath()
                           ? context.application.path().filename().string().c_str()
                           : "This project");
    if (!reason_.empty()) subtleText(reason_);

    ImGui::Spacing();
    Outcome outcome = Outcome::None;

    if (ImGui::Button("Save", ImVec2(100, 0))) outcome = Outcome::Save;
    ImGui::SameLine();
    if (ImGui::Button("Discard", ImVec2(100, 0))) outcome = Outcome::Discard;
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) outcome = Outcome::Cancel;

    if (outcome != Outcome::None) {
        isOpen_ = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    return outcome;
}

// ---------------------------------------------------------------------------
// ExportDialog
// ---------------------------------------------------------------------------

void ExportDialog::draw(UiContext& context, FileDialog& fileDialog) {
    if (context.requestExportDialog) {
        ImGui::OpenPopup("Export topology");
        context.requestExportDialog = false;
    }

    ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Export topology", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextWrapped("The topology is written as SVG: a vector picture that stays sharp at any "
                       "size and can be edited in any drawing tool.");
    ImGui::Spacing();

    ImGui::Checkbox("Include annotations", &options_.includeAnnotations);
    ImGui::Checkbox("Include interface labels", &options_.includeInterfaceLabels);
    ImGui::Checkbox("Include addresses", &options_.includeAddresses);
    ImGui::Checkbox("Dark background", &options_.darkBackground);

    ImGui::Separator();
    if (ImGui::Button("Choose file...", ImVec2(130, 0))) {
        const std::string suggested =
            strings::sanitizeFileName(context.application.project().metadata().name) + ".svg";
        fileDialog.open(FileDialog::Mode::Save, "Export topology as SVG", {".svg"}, suggested);
        awaitingPath_ = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void ExportDialog::completeExport(UiContext& context, const std::filesystem::path& path) {
    awaitingPath_ = false;

    auto document = context.application.exportSvg(options_);
    if (!document) {
        context.setStatus(document.message(), true);
        return;
    }

    if (const Status status = files::writeTextFileAtomic(path, document.value()); !status) {
        context.setStatus(status.message(), true);
        return;
    }
    context.setStatus(std::format("Exported {}", path.filename().string()));
}

} // namespace tnp::ui
