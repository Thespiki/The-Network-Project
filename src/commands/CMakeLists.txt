# Undo/redo. Every editor change is a command; nothing edits the project model
# directly except through this module.
tnp_add_module(tnp_commands
    SOURCES
        Command.h
        CommandManager.h
        CommandManager.cpp
        DeviceCommands.h
        DeviceCommands.cpp
        InterfaceCommands.h
        InterfaceCommands.cpp
        LinkCommands.h
        LinkCommands.cpp
        RoutingCommands.h
        RoutingCommands.cpp
        ProjectCommands.h
        ProjectCommands.cpp
    PUBLIC_DEPS
        tnp::tnp_core)
