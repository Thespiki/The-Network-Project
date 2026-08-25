# Simulation layer: the clock, the event queue, the packet history and the
# engine that implements core::DeviceContext. Depends on core; nothing above it.
tnp_add_module(tnp_simulation
    SOURCES
        SimulationEvent.h
        EventQueue.h
        EventQueue.cpp
        NetworkScheduler.h
        NetworkScheduler.cpp
        SimulationState.h
        SimulationState.cpp
        Packet.h
        Packet.cpp
        PacketDecoder.h
        PacketDecoder.cpp
        Simulator.h
        Simulator.cpp
    PUBLIC_DEPS
        tnp::tnp_core)
