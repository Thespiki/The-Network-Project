# The Dear ImGui workspace.
#
# The only module that knows a window exists. Nothing below this layer includes
# an ImGui header, which is what keeps the engine testable without a display.
tnp_add_module(tnp_ui
    SOURCES
        Theme.h
        Theme.cpp
        Icons.h
        Icons.cpp
        UiContext.h
        UiContext.cpp
        Canvas.h
        Canvas.cpp
        MainWindow.h
        MainWindow.cpp
        panels/Panels.h
        panels/PanelSupport.h
        panels/DevicePalettePanel.cpp
        panels/PropertiesPanel.cpp
        panels/DiagnosticPanels.cpp
        panels/SimulationPanels.cpp
        panels/TestsPanel.cpp
        dialogs/Dialogs.h
        dialogs/Dialogs.cpp
    PUBLIC_DEPS
        tnp::tnp_app
        imgui)
