#pragma once

#include "core/network/DeviceContext.h"
#include "core/network/Frame.h"
#include "core/network/Ids.h"
#include "utilities/Time.h"

#include <variant>

namespace tnp::sim {

/// A frame that has finished travelling a link and is arriving at an interface.
struct FrameArrival {
    core::LinkId link;
    core::DeviceId device;
    core::InterfaceId interface;
    core::Frame frame;
};

/// A timer a device armed through its `DeviceContext` has come due.
struct DeviceTimerExpiry {
    core::DeviceId device;
    core::TimerId timer = 0;
};

/// A frame a device sent to one of its own addresses, coming back in.
///
/// Loopback is an event like any other, rather than a direct call, so pinging
/// your own address occupies real simulated time and cannot recurse.
struct LoopbackDelivery {
    core::DeviceId device;
    core::InterfaceId interface;
    core::Frame frame;
};

using SimulationEventPayload = std::variant<FrameArrival, DeviceTimerExpiry, LoopbackDelivery>;

/// One scheduled thing to do, at one instant.
///
/// `sequence` gives a total order to events sharing a timestamp, which is what
/// makes a run reproducible: two frames that arrive at the same nanosecond are
/// always processed in the order they were scheduled.
struct SimulationEvent {
    SimTime time{};
    u64 sequence = 0;
    SimulationEventPayload payload;
};

[[nodiscard]] std::string_view simulationEventName(const SimulationEvent& event);

} // namespace tnp::sim
