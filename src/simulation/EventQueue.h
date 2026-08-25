#pragma once

#include "simulation/SimulationEvent.h"

#include <optional>
#include <queue>
#include <unordered_set>
#include <vector>

namespace tnp::sim {

/// The simulation's schedule: a time-ordered queue of pending events.
///
/// Ordering is by (time, insertion sequence), so the queue is a total order and
/// two runs of the same project produce identical results. Cancellation is done
/// with a tombstone set rather than by searching the heap: timers are cancelled
/// far more often than they are examined, and a heap has no cheap erase.
class EventQueue {
public:
    /// Schedules `payload` for `time`. Returns the sequence number assigned.
    u64 push(SimTime time, SimulationEventPayload payload);

    /// Removes and returns the earliest live event, skipping cancelled timers.
    [[nodiscard]] std::optional<SimulationEvent> pop();

    /// Time of the earliest live event. Only valid when `empty()` is false.
    [[nodiscard]] SimTime nextEventTime() const;

    [[nodiscard]] bool empty() const;
    [[nodiscard]] std::size_t size() const { return events_.size(); }

    /// Marks a device timer so it is skipped when it surfaces.
    void cancelTimer(core::TimerId timer);

    void clear();

private:
    struct Compare {
        bool operator()(const SimulationEvent& a, const SimulationEvent& b) const {
            if (a.time != b.time) return a.time > b.time; // earliest first
            return a.sequence > b.sequence;
        }
    };

    /// True when the event at the top is a cancelled timer.
    [[nodiscard]] bool topIsCancelled() const;

    /// Discards cancelled timers sitting at the front of the queue.
    /// Logically const: it only removes events that are already dead, which is
    /// why `empty()` and `nextEventTime()` may call it.
    void dropCancelledFromTop() const;

    mutable std::priority_queue<SimulationEvent, std::vector<SimulationEvent>, Compare> events_;
    mutable std::unordered_set<core::TimerId> cancelled_;
    u64 nextSequence_ = 0;
};

} // namespace tnp::sim
