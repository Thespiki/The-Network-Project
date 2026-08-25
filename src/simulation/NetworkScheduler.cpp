#include "simulation/NetworkScheduler.h"

#include <utility>

namespace tnp::sim {

u64 NetworkScheduler::schedule(Duration delay, SimulationEventPayload payload) {
    // A negative delay would let an event land in the past and break the total
    // ordering the queue guarantees.
    if (delay < Duration::zero()) delay = Duration::zero();
    return queue_.push(clock_ + delay, std::move(payload));
}

u64 NetworkScheduler::scheduleAt(SimTime time, SimulationEventPayload payload) {
    if (time < clock_) time = clock_;
    return queue_.push(time, std::move(payload));
}

std::optional<SimulationEvent> NetworkScheduler::nextDueEvent(SimTime target) {
    if (queue_.empty()) return std::nullopt;
    if (queue_.nextEventTime() > target) return std::nullopt;

    auto event = queue_.pop();
    if (event) clock_ = event->time;
    return event;
}

std::optional<SimulationEvent> NetworkScheduler::nextEvent() {
    auto event = queue_.pop();
    if (event) clock_ = event->time;
    return event;
}

void NetworkScheduler::advanceClockTo(SimTime time) {
    if (time > clock_) clock_ = time;
}

void NetworkScheduler::reset() {
    queue_.clear();
    clock_ = simTimeZero();
}

} // namespace tnp::sim
