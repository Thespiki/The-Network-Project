#pragma once

#include "utilities/Types.h"

#include <string_view>

namespace tnp::sim {

/// Run state of the engine.
enum class SimulationState : u8 {
    Stopped, ///< no run in progress; device caches are empty
    Running, ///< time advances with the wall clock
    Paused   ///< a run exists and can be stepped, but time does not advance
};

[[nodiscard]] std::string_view simulationStateName(SimulationState state);

/// Counters for the current run.
struct SimulationStatistics {
    u64 eventsProcessed = 0;
    u64 framesTransmitted = 0;
    u64 framesDelivered = 0;
    u64 framesDropped = 0;
    u64 packetsCreated = 0;
    u64 traceEvents = 0;

    void reset() { *this = SimulationStatistics{}; }
};

} // namespace tnp::sim
