#pragma once

#include "ui/Canvas.h"
#include "ui/UiContext.h"
#include "ui/dialogs/Dialogs.h"
#include "ui/panels/Panels.h"

#include <filesystem>

namespace tnp::ui {

/// The workspace: menus, toolbar, dock layout, panels and status bar.
///
/// Owns the panels and the dialogs but none of the model. Everything it does to
/// a project goes through `Application`, which is why the same operations are
/// available from the headless tool and from the tests.
class MainWindow {
public:
    explicit MainWindow(app::Application& application);

    /// Draws one frame.
    void draw();

    /// True once the user has asked to quit and any unsaved work is resolved.
    [[nodiscard]] bool wantsToClose() const { return wantsToClose_; }

    /// Asks to close, giving the user a chance to save first.
    void requestClose();

    [[nodiscard]] UiContext& context() { return context_; }

private:
    /// Something the user asked for that may have to wait for a "save first?"
    /// answer.
    enum class PendingAction : u8 { None, NewProject, NewFromSample, Open, OpenPath, Quit };

    void drawMenuBar();
    void drawToolbar();
    void drawStatusBar();
    void drawDockSpace();
    void drawPanels();
    void drawDialogs();

    void handleShortcuts();
    void buildDefaultLayout(ImGuiID dockspaceId);

    /// Runs `action`, first asking about unsaved changes when there are any.
    void requestAction(PendingAction action, std::filesystem::path path = {});
    void performPendingAction();

    void doNewProject();
    void doNewFromSample();
    void doOpen(const std::filesystem::path& path);
    void doSave();
    void doSaveAs();

    app::Application& application_;
    UiContext context_;

    Canvas canvas_;

    DevicePalettePanel palette_;
    PropertiesPanel properties_;
    ProblemsPanel problems_;
    ConsolePanel console_;
    LogPanel log_;
    PacketPanel packets_;
    EventsPanel events_;
    SimulationPanel simulation_;
    TestsPanel tests_;
    LearningPanel learning_;

    FileDialog fileDialog_;
    InterfacePickerDialog interfacePicker_;
    PreferencesDialog preferences_;
    AboutDialog about_;
    UnsavedChangesDialog unsavedChanges_;
    ExportDialog exportDialog_;

    /// What the file dialog is currently being used for.
    enum class FileDialogPurpose : u8 { None, Open, SaveAs, Export };
    FileDialogPurpose fileDialogPurpose_ = FileDialogPurpose::None;

    PendingAction pending_ = PendingAction::None;
    std::filesystem::path pendingPath_;

    bool wantsToClose_ = false;
    bool layoutBuilt_ = false;
    bool recoveryPromptShown_ = false;
};

} // namespace tnp::ui
