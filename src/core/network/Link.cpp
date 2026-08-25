#include "core/network/Link.h"

namespace tnp::core {

std::string_view linkMediumName(LinkMedium medium) {
    switch (medium) {
        case LinkMedium::Copper:   return "Copper";
        case LinkMedium::Fiber:    return "Fiber";
        case LinkMedium::Serial:   return "Serial";
        case LinkMedium::Wireless: return "Wireless";
        case LinkMedium::Virtual:  return "Virtual";
    }
    return "Copper";
}

Duration linkMediumDefaultDelay(LinkMedium medium) {
    // Order-of-magnitude values chosen so multi-hop paths produce readable,
    // distinguishable timings in the simulation timeline.
    switch (medium) {
        case LinkMedium::Copper:   return microseconds(1);
        case LinkMedium::Fiber:    return microseconds(5);
        case LinkMedium::Serial:   return microseconds(500);
        case LinkMedium::Wireless: return microseconds(50);
        case LinkMedium::Virtual:  return milliseconds(10);
    }
    return microseconds(1);
}

Link::Link(LinkId id, LinkEndpoint a, LinkEndpoint b, LinkMedium medium)
    : id_(id), a_(a), b_(b), medium_(medium), propagationDelay_(linkMediumDefaultDelay(medium)) {}

bool Link::involves(DeviceId device) const {
    return a_.device == device || b_.device == device;
}

bool Link::involves(InterfaceId interface) const {
    return a_.interface == interface || b_.interface == interface;
}

std::optional<LinkEndpoint> Link::peerOf(InterfaceId interface) const {
    if (a_.interface == interface) return b_;
    if (b_.interface == interface) return a_;
    return std::nullopt;
}

std::optional<LinkEndpoint> Link::endpointOf(InterfaceId interface) const {
    if (a_.interface == interface) return a_;
    if (b_.interface == interface) return b_;
    return std::nullopt;
}

Duration Link::transferTimeFor(std::size_t bytes) const {
    if (bandwidthMbps_ == 0) return propagationDelay_;
    // bits / (Mbit/s) gives microseconds; convert to nanoseconds.
    const u64 bits = static_cast<u64>(bytes) * 8;
    const u64 nanos = (bits * 1000) / bandwidthMbps_;
    return propagationDelay_ + nanoseconds(static_cast<i64>(nanos));
}

} // namespace tnp::core
