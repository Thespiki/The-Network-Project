#pragma once

#include "simulation/EventQueue.h"
#include "utilities/Time.h"

#include <optional>

namespace tnp::sim {

/// The simulation clock and its pending work.
///
/// Deliberately separate from `Simulator`: this class decides *when* things
/// happen, the simulator decides *what* they do. Simulated time only moves when
/// an event is taken off this queue or when the caller explicitly advances it -
/// never because a frame was rendered.
class NetworkScheduler {
public:
    [[nodiscard]] SimTime now() const { return clock_; }

    /// Schedules `payload` `delay` after the current instant. Returns the
    /// event's sequence number, which identifies it uniquely for the run.
    u64 schedule(Duration delay, SimulationEventPayload payload);
    u64 scheduleAt(SimTime time, SimulationEventPayload payload);

    void cancelTimer(core::TimerId timer) { queue_.cancelTimer(timer); }

    /// Takes the next event if it is due at or before `target`, moving the clock
    /// to that event's time.
    [[nodiscard]] std::optional<SimulationEvent> nextDueEvent(SimTime target);

    /// Takes the next event whatever its time. Used by single-stepping.
    [[nodiscard]] std::optional<SimulationEvent> nextEvent();

    /// Moves the clock forward without processing anything. Never moves it back.
    void advanceClockTo(SimTime time);

    [[nodiscard]] bool hasPendingEvents() const { return !queue_.empty(); }
    [[nodiscard]] std::size_t pendingEventCount() const { return queue_.size(); }
    [[nodiscard]] SimTime nextEventTime() const { return queue_.nextEventTime(); }

    void reset();

private:
    EventQueue queue_;
    SimTime clock_ = simTimeZero();
};

} // namespace tnp::sim
