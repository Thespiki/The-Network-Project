#pragma once

#include "utilities/Time.h"
#include "utilities/Types.h"

namespace tnp::core {

/// Simulation options that belong to the project rather than to the machine it
/// runs on, so a shared project reproduces the same run.
struct SimulationSettings {
    /// Simulated seconds per wall-clock second while the engine is running.
    double speedMultiplier = 1.0;

    /// Upper bound on how far one frame may advance simulated time. Without it,
    /// a stalled window would try to catch up in a single burst and appear to
    /// hang.
    Duration maximumStepPerFrame = milliseconds(100);

    /// How many trace events are kept for the log and timeline panels.
    u32 traceHistoryLimit = 20000;

    /// How many packets are retained for inspection after they are delivered.
    u32 packetHistoryLimit = 2000;

    /// When on, the engine's structured events are also narrated in plain
    /// language by the learning system.
    bool learningModeEnabled = false;

    /// Start the simulation automatically when the user issues a command that
    /// needs it, such as `ping` from the device console.
    bool autoStartOnTraffic = true;
};

} // namespace tnp::core
