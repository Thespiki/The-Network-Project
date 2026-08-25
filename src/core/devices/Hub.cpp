#include "core/devices/Hub.h"

#include <format>

namespace tnp::core {

Hub::Hub(DeviceId id, std::string name, std::size_t portCount) : Device(id, std::move(name)) {
    createInterfaces(InterfaceType::Ethernet, portCount, "0/", 1);
}

void Hub::onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) {
    std::size_t repeated = 0;

    for (const auto& port : interfaces()) {
        if (port->id() == ingress.id()) continue;
        if (!port->isOperational()) continue;

        // The bytes are repeated unchanged - a hub works at the physical layer
        // and never looks at an address.
        Frame copy = context.makeForwardedFrame(frame.identity(), frame.bytes, frame.category,
                                                frame.summary);
        context.transmit(*this, *port, std::move(copy));
        ++repeated;
    }

    context.trace(TraceEvent{.kind = TraceKind::FrameFlooded,
                             .time = context.now(),
                             .device = id(),
                             .interface = ingress.id(),
                             .packet = frame.id,
                             .summary = std::format("repeated to {} port(s)", repeated)}
                      .with("ports", std::to_string(repeated))
                      .with("reason", "hub repeats every frame"));
}

} // namespace tnp::core
