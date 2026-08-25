#pragma once

#include "app/Application.h"
#include "utilities/Time.h"

#include <string>

namespace tnp::ui {

/// State that belongs to the interface rather than to the document.
///
/// Panels are stateless views over `Application` plus this struct. Nothing here
/// is saved with a project: which packet is being inspected or which panel is
/// open is a property of the session, not of the network.
struct UiContext {
    explicit UiContext(app::Application& applicationRef) : application(applicationRef) {}

    app::Application& application;

    // --- Cross-panel selection ---------------------------------------------
    /// Packet shown in the inspector.
    core::PacketId inspectedPacket;

    /// Device the console is attached to. Kept here so clicking a device on the
    /// canvas can offer to point the console at it.
    core::DeviceId consoleDevice;

    /// Interface shown in the properties panel when a device is selected.
    core::InterfaceId inspectedInterface;

    // --- Panel visibility ---------------------------------------------------
    bool showPalette = true;
    bool showProperties = true;
    bool showProblems = true;
    bool showConsole = true;
    bool showLog = true;
    bool showPackets = true;
    bool showEvents = true;
    bool showSimulation = true;
    bool showTests = true;
    bool showLearning = false;
    bool showMinimap = true;
    bool showImGuiDemo = false;

    // --- Canvas interaction --------------------------------------------------
    /// True while the user is dragging a cable from an interface.
    bool isConnecting = false;
    core::DeviceId connectFromDevice;
    core::InterfaceId connectFromInterface;

    /// Set when the canvas wants the interface picker opened for a pending link.
    bool requestInterfacePicker = false;
    core::DeviceId pickerSourceDevice;
    core::DeviceId pickerTargetDevice;

    /// Centre of the canvas view in project coordinates, refreshed by the canvas
    /// each frame. A device added by clicking the palette lands here, where the
    /// user is actually looking.
    Vec2 viewCenter;

    /// Device type the palette is currently dragging, if any.
    bool isDraggingNewDevice = false;
    core::DeviceType draggedDeviceType = core::DeviceType::Pc;

    // --- Transient status ----------------------------------------------------
    /// Message shown in the status bar, cleared after a few seconds.
    std::string statusMessage;
    bool statusIsError = false;
    double statusExpiresAt = 0.0;

    void setStatus(std::string message, bool isError = false);

    /// True while the status message should still be shown.
    [[nodiscard]] bool hasStatus() const;

    // --- Dialog requests -----------------------------------------------------
    // Panels ask for a dialog by setting a flag; `MainWindow` owns the dialogs
    // themselves, so no panel has to know they exist.
    bool requestOpenDialog = false;
    bool requestSaveAsDialog = false;
    bool requestExportDialog = false;
    bool requestPreferences = false;
    bool requestAbout = false;
    bool requestNewProjectConfirm = false;
    bool requestQuit = false;
};

} // namespace tnp::ui
