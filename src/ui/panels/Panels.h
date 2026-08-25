#pragma once

#include "ui/UiContext.h"

#include <string>
#include <vector>

namespace tnp::ui {

class Canvas;

/// The device palette.
///
/// Enumerates the device registry rather than a hard-coded list, so a device
/// kind added to the registry appears here without touching the UI.
class DevicePalettePanel {
public:
    void draw(UiContext& context);

private:
    std::string filter_;
};

/// Properties of whatever is selected.
///
/// Edits go through commands, never straight into the model, so everything done
/// here is undoable and the console and the canvas see the same changes.
class PropertiesPanel {
public:
    void draw(UiContext& context);

private:
    void drawDevice(UiContext& context, core::Device& device);
    void drawInterfaceList(UiContext& context, core::Device& device);
    void drawInterface(UiContext& context, core::Device& device, core::Interface& iface);
    void drawRouting(UiContext& context, core::Device& device);
    void drawSwitching(UiContext& context, core::Device& device);
    void drawFirewall(UiContext& context, core::Device& device);
    void drawServices(UiContext& context, core::Device& device);
    void drawDiagnostics(UiContext& context, const core::ObjectRef& subject);
    void drawLink(UiContext& context, core::Link& link);
    void drawAnnotation(UiContext& context, core::Annotation& annotation);
    void drawMultiSelection(UiContext& context);
    void drawProjectProperties(UiContext& context);

    /// Refreshes the edit buffers when the selection changes.
    void syncBuffers(const core::ObjectRef& subject, UiContext& context);

    /// What the buffers below currently hold. They are refreshed only when this
    /// changes; refreshing them every frame would discard the keystrokes that
    /// have not been committed yet.
    core::ObjectRef bufferOwner_;
    bool buffersValid_ = false;

    std::string nameBuffer_;
    std::string descriptionBuffer_;
    std::string textBuffer_;
    std::string authorBuffer_;
    std::string gatewayBuffer_;

    // "Add address" and "add route" inline forms.
    std::string newAddressBuffer_;
    std::string newRouteDestination_;
    std::string newRouteNextHop_;
    std::string newRecordName_;
    std::string newRecordAddress_;
};

/// Validation findings, grouped by severity.
class ProblemsPanel {
public:
    void draw(UiContext& context);

private:
    bool showErrors_ = true;
    bool showWarnings_ = true;
    bool showInfo_ = true;
    std::string filter_;
};

/// The integrated device console.
class ConsolePanel {
public:
    void draw(UiContext& context);

private:
    void appendLine(cli::ShellLine line);

    std::vector<cli::ShellLine> lines_;
    std::string input_;
    bool scrollToBottom_ = true;
    int historyCursor_ = -1;
};

/// The application log.
class LogPanel {
public:
    void draw(UiContext& context);

private:
    int minimumLevel_ = static_cast<int>(logging::Level::Debug);
    std::string filter_;
    bool autoScroll_ = true;
};

/// Captured packets, with a full protocol breakdown of the selected one.
class PacketPanel {
public:
    void draw(UiContext& context);

private:
    void drawList(UiContext& context);
    void drawInspector(UiContext& context);

    std::string filter_;
    bool autoScroll_ = true;
};

/// The engine's structured event stream.
class EventsPanel {
public:
    void draw(UiContext& context);

private:
    std::string filter_;
    bool autoScroll_ = true;
    bool categoryEnabled_[10] = {true, true, true, true, true, true, true, true, true, true};
};

/// Playback controls and engine statistics.
class SimulationPanel {
public:
    void draw(UiContext& context);
};

/// The project's connectivity tests.
class TestsPanel {
public:
    void draw(UiContext& context);

private:
    void drawEditor(UiContext& context);

    core::TestId editing_;
    core::TestId bufferOwner_;
    std::string nameBuffer_;
    std::string descriptionBuffer_;
    std::string addressBuffer_;
    bool showEditor_ = false;
};

/// Learning mode: the engine's events, explained.
class LearningPanel {
public:
    void draw(UiContext& context);

private:
    bool autoScroll_ = true;
};

} // namespace tnp::ui
