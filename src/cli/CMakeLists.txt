# The integrated device console. Every command reads or writes the real model;
# configuration goes through the undo stack, and ping drives the real simulator.
tnp_add_module(tnp_cli
    SOURCES
        DeviceShell.h
        DeviceShell.cpp
    PUBLIC_DEPS
        tnp::tnp_simulation
        tnp::tnp_commands)
