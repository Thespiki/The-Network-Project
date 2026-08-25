#pragma once

#include "ui/UiContext.h"

#include <filesystem>
#include <string>
#include <vector>

namespace tnp::ui {

/// A file browser built on `std::filesystem`.
///
/// TNP does not link a platform dialog library: one more dependency, one more
/// thing to port, and a native dialog would still need this fallback on the
/// systems where it is unavailable. This one is small, works identically
/// everywhere, and knows about TNP's own extensions.
class FileDialog {
public:
    enum class Mode : u8 { Open, Save };

    void open(Mode mode, std::string title, std::vector<std::string> extensions,
              std::string suggestedName = {});

    /// Draws the dialog. Returns true on the frame the user confirms a path.
    bool draw();

    [[nodiscard]] const std::filesystem::path& result() const { return result_; }
    [[nodiscard]] bool isOpen() const { return isOpen_; }

private:
    void refreshEntries();

    struct Entry {
        std::string name;
        bool isDirectory = false;
        std::uintmax_t size = 0;
    };

    Mode mode_ = Mode::Open;
    std::string title_ = "Open";
    std::vector<std::string> extensions_;

    std::filesystem::path directory_;
    std::vector<Entry> entries_;
    std::string fileName_;
    std::string error_;

    std::filesystem::path result_;
    bool isOpen_ = false;
    bool shouldFocus_ = false;
};

/// Asks which pair of interfaces a new cable should join.
///
/// Guessing would be wrong often enough to be annoying: which port a cable goes
/// into is a real decision, and on a switch with eight identical ports only the
/// user knows which one they mean.
class InterfacePickerDialog {
public:
    /// Draws the dialog when `context.requestInterfacePicker` is set.
    void draw(UiContext& context);

private:
    core::InterfaceId sourceInterface_;
    core::InterfaceId targetInterface_;
    core::DeviceId lastSource_;
    core::DeviceId lastTarget_;
};

/// Application preferences.
class PreferencesDialog {
public:
    void draw(UiContext& context);
};

/// About TNP, including what this build does and does not simulate.
class AboutDialog {
public:
    void draw(UiContext& context);
};

/// "Save your changes?" before a destructive action.
class UnsavedChangesDialog {
public:
    /// What the user chose.
    enum class Outcome : u8 { None, Save, Discard, Cancel };

    /// Opens the prompt and remembers what to do once it is answered.
    void request(std::string reason);

    [[nodiscard]] Outcome draw(UiContext& context);
    [[nodiscard]] bool isOpen() const { return isOpen_; }

private:
    bool isOpen_ = false;
    bool shouldOpen_ = false;
    std::string reason_;
};

/// Export the topology as SVG.
class ExportDialog {
public:
    void draw(UiContext& context, FileDialog& fileDialog);

    /// Called by `MainWindow` once the file dialog produced a path.
    void completeExport(UiContext& context, const std::filesystem::path& path);

    [[nodiscard]] bool isAwaitingPath() const { return awaitingPath_; }

private:
    app::SvgExportOptions options_;
    bool awaitingPath_ = false;
};

} // namespace tnp::ui
