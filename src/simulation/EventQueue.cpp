#include "simulation/EventQueue.h"

namespace tnp::sim {

std::string_view simulationEventName(const SimulationEvent& event) {
    if (std::holds_alternative<FrameArrival>(event.payload)) return "FrameArrival";
    if (std::holds_alternative<DeviceTimerExpiry>(event.payload)) return "DeviceTimer";
    return "Loopback";
}

u64 EventQueue::push(SimTime time, SimulationEventPayload payload) {
    SimulationEvent event;
    event.time = time;
    event.sequence = nextSequence_++;
    event.payload = std::move(payload);

    const u64 sequence = event.sequence;
    events_.push(std::move(event));
    return sequence;
}

bool EventQueue::topIsCancelled() const {
    if (events_.empty()) return false;
    const auto* timer = std::get_if<DeviceTimerExpiry>(&events_.top().payload);
    return timer != nullptr && cancelled_.contains(timer->timer);
}

void EventQueue::dropCancelledFromTop() const {
    while (topIsCancelled()) {
        const auto* timer = std::get_if<DeviceTimerExpiry>(&events_.top().payload);
        cancelled_.erase(timer->timer);
        events_.pop();
    }
}

std::optional<SimulationEvent> EventQueue::pop() {
    dropCancelledFromTop();
    if (events_.empty()) return std::nullopt;

    SimulationEvent event = events_.top();
    events_.pop();
    return event;
}

SimTime EventQueue::nextEventTime() const {
    dropCancelledFromTop();
    return events_.empty() ? SimTime{Duration::max()} : events_.top().time;
}

bool EventQueue::empty() const {
    dropCancelledFromTop();
    return events_.empty();
}

void EventQueue::cancelTimer(core::TimerId timer) {
    if (timer == 0) return;
    cancelled_.insert(timer);
}

void EventQueue::clear() {
    while (!events_.empty()) events_.pop();
    cancelled_.clear();
    nextSequence_ = 0;
}

} // namespace tnp::sim
