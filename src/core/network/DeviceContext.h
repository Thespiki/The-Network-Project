#pragma once

#include "core/network/Frame.h"
#include "core/network/Ids.h"
#include "core/network/TraceEvent.h"
#include "utilities/Time.h"
#include "utilities/Types.h"

#include <string>

namespace tnp::core {

class Device;
class Interface;

/// Opaque timer key. Devices allocate their own values and map them back to
/// whatever the timer was for (an ARP retry, a ping timeout, a cache sweep).
using TimerId = u64;

/// The services a simulated device needs from its environment.
///
/// This interface is the seam that keeps the dependency arrow pointing the right
/// way: device behaviour lives in `core`, while the event queue, the clock and
/// the topology walk live in `simulation`, which implements this interface. A
/// device therefore has no idea what a link, a scheduler or a UI is - it only
/// knows how to ask for a frame to go out of one of its own interfaces.
class DeviceContext {
public:
    virtual ~DeviceContext() = default;

    /// Current simulation time.
    [[nodiscard]] virtual SimTime now() const = 0;

    /// Wraps encoded wire bytes into a tracked frame with a fresh identity.
    [[nodiscard]] virtual Frame makeFrame(const Device& origin,
                                          ByteBuffer bytes,
                                          FrameCategory category,
                                          std::string summary) = 0;

    /// Re-encapsulates a packet that is passing through this device.
    ///
    /// The identity is taken verbatim, so the caller decides whether the hop
    /// count advances: a router increments it, a bridge does not.
    [[nodiscard]] virtual Frame makeForwardedFrame(FrameIdentity identity,
                                                   ByteBuffer bytes,
                                                   FrameCategory category,
                                                   std::string summary) = 0;

    /// Hands a frame to the wire attached to `out`. If nothing is attached, or
    /// the interface is down, the implementation records the drop.
    virtual void transmit(Device& sender, Interface& out, Frame frame) = 0;

    /// Delivers a frame straight back into `device` on `iface`, without a wire.
    ///
    /// This is how traffic addressed to the device itself is handled. Routing it
    /// through the event queue rather than calling the receive path directly
    /// keeps the loopback case on the same timeline as everything else and
    /// avoids unbounded recursion when a host pings its own address.
    virtual void loopback(Device& device, Interface& iface, Frame frame) = 0;

    /// Requests `device.onTimer(context, timer)` after `delay`.
    virtual void scheduleTimer(Device& device, TimerId timer, Duration delay) = 0;

    /// Cancels a pending timer. Cancelling an unknown timer is not an error.
    virtual void cancelTimer(Device& device, TimerId timer) = 0;

    /// Records something that happened. Every observable engine behaviour goes
    /// through here.
    virtual void trace(TraceEvent event) = 0;

    /// Human readable device name, for trace summaries.
    [[nodiscard]] virtual std::string deviceName(DeviceId device) const = 0;

    /// Allocates a monotonically increasing timer key. Convenience so devices do
    /// not each need their own counter.
    [[nodiscard]] virtual TimerId nextTimerId() = 0;
};

} // namespace tnp::core
