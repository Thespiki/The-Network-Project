#include "ui/MainWindow.h"

#include "app/SampleProject.h"
#include "ui/panels/PanelSupport.h"
#include "utilities/StringUtilities.h"

#include <imgui_internal.h>

#include <format>

namespace tnp::ui {

using namespace core;

namespace {

constexpr const char* kDockSpaceName = "TnpDockSpace";

/// Extensions the open dialog offers.
const std::vector<std::string> kProjectExtensions = {".tnp", ".tnpjson", ".json"};

} // namespace

MainWindow::MainWindow(app::Application& application)
    : application_(application), context_(application) {
    context_.showLearning = application_.learning().isEnabled();
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void MainWindow::draw() {
    drawDockSpace();
    handleShortcuts();
    drawPanels();
    drawDialogs();
}

void MainWindow::drawDockSpace() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("##tnp-root", nullptr, flags);
    ImGui::PopStyleVar(3);

    drawMenuBar();
    drawToolbar();

    const ImGuiID dockspaceId = ImGui::GetID(kDockSpaceName);
    const float statusHeight = ImGui::GetFrameHeightWithSpacing();

    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, -statusHeight), ImGuiDockNodeFlags_None);

    if (!layoutBuilt_) {
        layoutBuilt_ = true;
        // Only build the default arrangement when there is none saved, so a
        // layout the user arranged survives a restart.
        if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr ||
            ImGui::DockBuilderGetNode(dockspaceId)->IsEmpty()) {
            buildDefaultLayout(dockspaceId);
        }
    }

    drawStatusBar();
    ImGui::End();
}

void MainWindow::buildDefaultLayout(ImGuiID dockspaceId) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

    ImGuiID centre = dockspaceId;
    const ImGuiID left = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, 0.16f, nullptr, &centre);
    const ImGuiID right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.26f, nullptr, &centre);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.30f, nullptr, &centre);
    const ImGuiID rightBottom =
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.35f, nullptr, nullptr);

    ImGui::DockBuilderDockWindow("Devices", left);
    ImGui::DockBuilderDockWindow("Network", centre);
    ImGui::DockBuilderDockWindow("Properties", right);
    ImGui::DockBuilderDockWindow("Simulation", rightBottom);
    ImGui::DockBuilderDockWindow("Problems", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Log", bottom);
    ImGui::DockBuilderDockWindow("Packets", bottom);
    ImGui::DockBuilderDockWindow("Events", bottom);
    ImGui::DockBuilderDockWindow("Tests", bottom);
    ImGui::DockBuilderDockWindow("Learning", bottom);

    ImGui::DockBuilderFinish(dockspaceId);
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------

void MainWindow::drawMenuBar() {
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New", "Ctrl+N")) requestAction(PendingAction::NewProject);
        if (ImGui::MenuItem("New from sample")) requestAction(PendingAction::NewFromSample);
        if (ImGui::MenuItem("Open...", "Ctrl+O")) requestAction(PendingAction::Open);

        if (ImGui::BeginMenu("Open recent", !application_.config().recentFiles.empty())) {
            for (const std::string& recent : application_.config().recentFiles) {
                if (ImGui::MenuItem(recent.c_str())) {
                    requestAction(PendingAction::OpenPath, recent);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear list")) application_.config().recentFiles.clear();
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Ctrl+S", false, application_.isDirty() || application_.hasPath())) {
            doSave();
        }
        if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S")) doSaveAs();

        ImGui::Separator();
        if (ImGui::MenuItem("Export topology as SVG...")) context_.requestExportDialog = true;

        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) requestAction(PendingAction::Quit);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        const std::string undoLabel = application_.commands().canUndo()
                                          ? std::format("Undo {}", application_.commands().undoLabel())
                                          : std::string{"Undo"};
        const std::string redoLabel = application_.commands().canRedo()
                                          ? std::format("Redo {}", application_.commands().redoLabel())
                                          : std::string{"Redo"};

        if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, application_.commands().canUndo())) {
            application_.commands().undo();
        }
        if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, application_.commands().canRedo())) {
            application_.commands().redo();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Select all", "Ctrl+A")) {
            application_.selection().clear();
            for (const auto& device : application_.project().network().devices()) {
                application_.selection().add(ObjectRef::device(device->id()));
            }
        }
        if (ImGui::MenuItem("Delete", "Del", false, !application_.selection().empty())) {
            application_.deleteSelection();
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Preferences...")) context_.requestPreferences = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Frame all", "F")) canvas_.frameAll(context_);
        if (ImGui::MenuItem("Zoom in", "Ctrl++")) canvas_.zoomIn();
        if (ImGui::MenuItem("Zoom out", "Ctrl+-")) canvas_.zoomOut();
        if (ImGui::MenuItem("Reset zoom", "Ctrl+0")) canvas_.resetZoom();

        ImGui::Separator();
        ImGui::MenuItem("Grid", nullptr, &application_.project().layout().gridVisible);
        ImGui::MenuItem("Snap to grid", nullptr, &application_.project().layout().snapToGrid);
        ImGui::MenuItem("Minimap", nullptr, &context_.showMinimap);

        ImGui::Separator();
        ImGui::MenuItem("Devices", nullptr, &context_.showPalette);
        ImGui::MenuItem("Properties", nullptr, &context_.showProperties);
        ImGui::MenuItem("Problems", nullptr, &context_.showProblems);
        ImGui::MenuItem("Console", nullptr, &context_.showConsole);
        ImGui::MenuItem("Log", nullptr, &context_.showLog);
        ImGui::MenuItem("Packets", nullptr, &context_.showPackets);
        ImGui::MenuItem("Events", nullptr, &context_.showEvents);
        ImGui::MenuItem("Simulation", nullptr, &context_.showSimulation);
        ImGui::MenuItem("Tests", nullptr, &context_.showTests);
        ImGui::MenuItem("Learning", nullptr, &context_.showLearning);

        ImGui::Separator();
        if (ImGui::MenuItem("Reset layout")) layoutBuilt_ = false;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Simulation")) {
        sim::Simulator& simulator = application_.simulator();
        const bool running = simulator.isRunning();

        if (ImGui::MenuItem(running ? "Pause" : "Play", "Space")) {
            if (running) simulator.pause();
            else         simulator.start();
        }
        if (ImGui::MenuItem("Step", "Right arrow")) {
            if (running) simulator.pause();
            simulator.step();
        }
        if (ImGui::MenuItem("Stop", nullptr, false, simulator.isActive())) simulator.stop();
        if (ImGui::MenuItem("Reset")) {
            simulator.reset();
            application_.learning().clear();
            context_.inspectedPacket = PacketId{};
        }

        ImGui::Separator();
        bool learning = application_.learning().isEnabled();
        if (ImGui::MenuItem("Learning mode", nullptr, &learning)) {
            application_.setLearningModeEnabled(learning);
            context_.showLearning = learning;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Validate now")) {
            application_.invalidateValidation();
            const validation::ValidationReport& report = application_.validationReport();
            context_.showProblems = true;
            context_.setStatus(report.isClean()
                                   ? std::string{"No problems found"}
                                   : std::format("{} error(s), {} warning(s)", report.errorCount(),
                                                 report.warningCount()),
                               report.hasErrors());
        }
        if (ImGui::MenuItem("Run all tests")) {
            const testing::TestRunSummary summary = application_.runTests();
            context_.showTests = true;
            context_.setStatus(summary.summaryLine(),
                               summary.failedCount() + summary.errorCount() > 0);
        }
        ImGui::Separator();
        ImGui::MenuItem("Dear ImGui demo", nullptr, &context_.showImGuiDemo);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About TNP")) context_.requestAbout = true;
        ImGui::EndMenu();
    }

    // Document title, right-aligned in the menu bar.
    const std::string title = application_.documentTitle();
    const float titleWidth = ImGui::CalcTextSize(title.c_str()).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - titleWidth - 16.0f);
    subtleText(title);

    ImGui::EndMenuBar();
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void MainWindow::drawToolbar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    ImGui::BeginChild("##toolbar", ImVec2(0, ImGui::GetFrameHeight() + 12.0f));

    const auto toolButton = [&](CanvasTool tool, const char* label, const char* tooltip) {
        const bool active = canvas_.tool() == tool;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, theme().accent);
        if (ImGui::Button(label)) canvas_.setTool(active ? CanvasTool::Select : tool);
        if (active) ImGui::PopStyleColor();
        if (ImGui::BeginItemTooltip()) {
            ImGui::TextUnformatted(tooltip);
            ImGui::EndTooltip();
        }
        ImGui::SameLine();
    };

    toolButton(CanvasTool::Select, "Select", "Select, move and box-select");
    toolButton(CanvasTool::Connect, "Connect", "Click two devices to cable them together");
    toolButton(CanvasTool::Text, "Text", "Place a text note");
    toolButton(CanvasTool::Rectangle, "Box", "Draw a rectangle");
    toolButton(CanvasTool::NetworkLabel, "Zone", "Mark a network area");
    toolButton(CanvasTool::Arrow, "Arrow", "Draw an arrow");

    ImGui::TextUnformatted("|");
    ImGui::SameLine();

    sim::Simulator& simulator = application_.simulator();
    const bool running = simulator.isRunning();

    if (running) ImGui::PushStyleColor(ImGuiCol_Button, theme().success);
    if (ImGui::Button(running ? "Pause" : "Play")) {
        if (running) simulator.pause();
        else         simulator.start();
    }
    if (running) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        if (running) simulator.pause();
        simulator.step();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        simulator.reset();
        application_.learning().clear();
        context_.inspectedPacket = PacketId{};
    }

    ImGui::SameLine();
    subtleText(std::format("{} - {}", sim::simulationStateName(simulator.state()),
                           formatSimTime(simulator.now())));

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// ---------------------------------------------------------------------------
// Status bar
// ---------------------------------------------------------------------------

void MainWindow::drawStatusBar() {
    const validation::ValidationReport& report = application_.validationReport();

    ImGui::Separator();

    if (context_.hasStatus()) {
        coloredText(context_.statusIsError ? theme().error : theme().success, context_.statusMessage);
    } else {
        const core::Project& project = application_.project();
        subtleText(std::format("{} device(s), {} link(s)   |   zoom {:.0f}%   |   {}",
                               project.network().deviceCount(), project.network().linkCount(),
                               canvas_.zoom() * 100.0f, canvasToolName(canvas_.tool())));
    }

    // Problem counts sit on the right, always visible, and open the panel.
    const std::string problems = report.isClean()
                                     ? std::string{"No problems"}
                                     : std::format("{} error(s), {} warning(s)", report.errorCount(),
                                                   report.warningCount());
    const float width = ImGui::CalcTextSize(problems.c_str()).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - width - 24.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, report.hasErrors()      ? theme().error
                                         : report.warningCount() ? theme().warning
                                                                 : theme().textSubtle);
    if (ImGui::SmallButton(problems.c_str())) context_.showProblems = true;
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Panels
// ---------------------------------------------------------------------------

void MainWindow::drawPanels() {
    if (ImGui::Begin("Network")) canvas_.draw(context_);
    ImGui::End();

    const auto panel = [](const char* name, bool* open, auto&& body) {
        if (!*open) return;
        if (ImGui::Begin(name, open)) body();
        ImGui::End();
    };

    panel("Devices", &context_.showPalette, [&] { palette_.draw(context_); });
    panel("Properties", &context_.showProperties, [&] { properties_.draw(context_); });
    panel("Problems", &context_.showProblems, [&] { problems_.draw(context_); });
    panel("Console", &context_.showConsole, [&] { console_.draw(context_); });
    panel("Log", &context_.showLog, [&] { log_.draw(context_); });
    panel("Packets", &context_.showPackets, [&] { packets_.draw(context_); });
    panel("Events", &context_.showEvents, [&] { events_.draw(context_); });
    panel("Simulation", &context_.showSimulation, [&] { simulation_.draw(context_); });
    panel("Tests", &context_.showTests, [&] { tests_.draw(context_); });
    panel("Learning", &context_.showLearning, [&] { learning_.draw(context_); });

    if (context_.showImGuiDemo) ImGui::ShowDemoWindow(&context_.showImGuiDemo);
}

// ---------------------------------------------------------------------------
// Dialogs
// ---------------------------------------------------------------------------

void MainWindow::drawDialogs() {
    // Offer crash recovery once, on the first frame it is available.
    if (application_.hasRecoverableSession() && !recoveryPromptShown_) {
        recoveryPromptShown_ = true;
        ImGui::OpenPopup("Recover unsaved work");
    }

    if (ImGui::BeginPopupModal("Recover unsaved work", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("TNP did not close normally last time, and an autosave from that "
                           "session is available.");
        if (!application_.recoveredFrom().empty()) {
            subtleText(std::format("It was working on {}", application_.recoveredFrom()));
        }
        ImGui::Spacing();

        if (ImGui::Button("Recover", ImVec2(110, 0))) {
            if (const Status status = application_.recoverSession(); status) {
                context_.setStatus("Recovered the previous session");
            } else {
                context_.setStatus(status.message(), true);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(110, 0))) {
            application_.discardRecovery();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    interfacePicker_.draw(context_);
    preferences_.draw(context_);
    about_.draw(context_);
    exportDialog_.draw(context_, fileDialog_);

    // Unsaved changes, and the action waiting on the answer.
    switch (unsavedChanges_.draw(context_)) {
        case UnsavedChangesDialog::Outcome::Save:
            doSave();
            // Saving may open a "save as" dialog; only continue once it worked.
            if (!application_.isDirty()) performPendingAction();
            break;
        case UnsavedChangesDialog::Outcome::Discard:
            performPendingAction();
            break;
        case UnsavedChangesDialog::Outcome::Cancel:
            pending_ = PendingAction::None;
            pendingPath_.clear();
            break;
        case UnsavedChangesDialog::Outcome::None:
            break;
    }

    if (context_.requestOpenDialog) {
        context_.requestOpenDialog = false;
        fileDialogPurpose_ = FileDialogPurpose::Open;
        fileDialog_.open(FileDialog::Mode::Open, "Open project", kProjectExtensions);
    }
    if (context_.requestSaveAsDialog) {
        context_.requestSaveAsDialog = false;
        fileDialogPurpose_ = FileDialogPurpose::SaveAs;

        const std::string suggested =
            application_.hasPath()
                ? application_.path().filename().string()
                : strings::sanitizeFileName(application_.project().metadata().name) + ".tnp";
        fileDialog_.open(FileDialog::Mode::Save, "Save project as", kProjectExtensions, suggested);
    }
    if (exportDialog_.isAwaitingPath() && !fileDialog_.isOpen()) {
        fileDialogPurpose_ = FileDialogPurpose::Export;
    }

    if (fileDialog_.draw()) {
        const std::filesystem::path chosen = fileDialog_.result();
        switch (fileDialogPurpose_) {
            case FileDialogPurpose::Open:
                doOpen(chosen);
                break;
            case FileDialogPurpose::SaveAs:
                if (const Status status = application_.saveAs(chosen); status) {
                    context_.setStatus(std::format("Saved {}", chosen.filename().string()));
                    performPendingAction();
                } else {
                    context_.setStatus(status.message(), true);
                }
                break;
            case FileDialogPurpose::Export:
                exportDialog_.completeExport(context_, chosen);
                break;
            case FileDialogPurpose::None:
                break;
        }
        fileDialogPurpose_ = FileDialogPurpose::None;
    }

    if (context_.requestQuit) {
        context_.requestQuit = false;
        requestAction(PendingAction::Quit);
    }
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void MainWindow::requestAction(PendingAction action, std::filesystem::path path) {
    pending_ = action;
    pendingPath_ = std::move(path);

    if (!application_.isDirty()) {
        performPendingAction();
        return;
    }
    unsavedChanges_.request("Save before continuing?");
}

void MainWindow::performPendingAction() {
    const PendingAction action = pending_;
    const std::filesystem::path path = pendingPath_;

    pending_ = PendingAction::None;
    pendingPath_.clear();

    switch (action) {
        case PendingAction::NewProject:    doNewProject(); break;
        case PendingAction::NewFromSample: doNewFromSample(); break;
        case PendingAction::Open:          context_.requestOpenDialog = true; break;
        case PendingAction::OpenPath:      doOpen(path); break;
        case PendingAction::Quit:          wantsToClose_ = true; break;
        case PendingAction::None:          break;
    }
}

void MainWindow::doNewProject() {
    application_.newProject();
    canvas_.frameAll(context_);
    context_.inspectedPacket = PacketId{};
    context_.setStatus("New project");
}

void MainWindow::doNewFromSample() {
    application_.newProject();
    app::buildSampleProject(application_.project());
    application_.project().network().refreshOperationalStates();
    application_.invalidateValidation();
    application_.commands().clear();
    canvas_.frameAll(context_);
    context_.setStatus("Loaded the sample network - press Play, then ping Server1 from PC1");
}

void MainWindow::doOpen(const std::filesystem::path& path) {
    if (const Status status = application_.open(path); !status) {
        context_.setStatus(status.message(), true);
        return;
    }

    canvas_.frameAll(context_);
    context_.inspectedPacket = PacketId{};
    context_.setStatus(std::format("Opened {}", path.filename().string()));

    if (!application_.loadWarnings().empty()) {
        context_.showLog = true;
        context_.setStatus(std::format("Opened {} with {} warning(s); see the log",
                                       path.filename().string(),
                                       application_.loadWarnings().size()),
                           true);
    }
}

void MainWindow::doSave() {
    if (!application_.hasPath()) {
        doSaveAs();
        return;
    }
    if (const Status status = application_.save(); status) {
        context_.setStatus(std::format("Saved {}", application_.path().filename().string()));
    } else {
        context_.setStatus(status.message(), true);
    }
}

void MainWindow::doSaveAs() { context_.requestSaveAsDialog = true; }

void MainWindow::requestClose() { requestAction(PendingAction::Quit); }

// ---------------------------------------------------------------------------
// Shortcuts
// ---------------------------------------------------------------------------

void MainWindow::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;

    const bool ctrl = io.KeyCtrl || io.KeySuper; // Cmd on macOS

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) requestAction(PendingAction::NewProject);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) requestAction(PendingAction::Open);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (io.KeyShift) doSaveAs();
        else             doSave();
    }

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (io.KeyShift) application_.commands().redo();
        else             application_.commands().undo();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) application_.commands().redo();

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Equal)) canvas_.zoomIn();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Minus)) canvas_.zoomOut();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_0)) canvas_.resetZoom();

    if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        sim::Simulator& simulator = application_.simulator();
        if (simulator.isRunning()) simulator.pause();
        else                       simulator.start();
    }
}

} // namespace tnp::ui
