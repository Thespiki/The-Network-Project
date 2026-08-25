#include "simulation/SimulationState.h"

namespace tnp::sim {

std::string_view simulationStateName(SimulationState state) {
    switch (state) {
        case SimulationState::Stopped: return "Stopped";
        case SimulationState::Running: return "Running";
        case SimulationState::Paused:  return "Paused";
    }
    return "Stopped";
}

} // namespace tnp::sim
