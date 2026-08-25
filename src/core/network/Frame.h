#pragma once

#include "core/network/Ids.h"
#include "utilities/Time.h"
#include "utilities/Types.h"

#include <string>
#include <string_view>

namespace tnp::core {

/// Protocol classification of a frame, derived from its bytes when it is built.
///
/// This is a networking fact, not a rendering hint: the canvas happens to map it
/// to a colour, the log panel to a filter, and the test runner to a match rule.
enum class FrameCategory : u8 {
    Unknown,
    Arp,
    Icmp,
    IcmpError,
    Tcp,
    Udp,
    Dhcp,
    Dns,
    Ipv6,
    Other
};

[[nodiscard]] std::string_view frameCategoryName(FrameCategory category);

/// The parts of a frame that identify the *packet* rather than its current
/// encapsulation. A router that rewrites the TTL and builds a new Ethernet
/// header keeps the identity, so the inspector shows one packet crossing the
/// topology instead of a new packet per hop.
struct FrameIdentity {
    PacketId id;
    DeviceId origin;
    SimTime createdAt{};
    u32 hopCount = 0;
};

/// A complete Ethernet frame in flight, plus the bookkeeping the simulator and
/// the inspector need.
///
/// `bytes` really is the wire format: headers written by the protocol encoders,
/// checksums computed for real, decoded again by the inspector. Nothing in TNP
/// carries a "pretend" packet with pre-baked display fields.
struct Frame {
    PacketId id;
    ByteBuffer bytes;

    SimTime createdAt{};
    DeviceId origin;

    /// Number of layer-3 devices that have forwarded this frame's payload.
    u32 hopCount = 0;

    FrameCategory category = FrameCategory::Unknown;

    /// Short engine-authored description such as "ICMP echo request 1/4".
    /// Derived from the encoded bytes at creation time; purely informational.
    std::string summary;

    [[nodiscard]] std::size_t size() const { return bytes.size(); }
    [[nodiscard]] bool empty() const { return bytes.empty(); }

    [[nodiscard]] FrameIdentity identity() const { return FrameIdentity{id, origin, createdAt, hopCount}; }
};

} // namespace tnp::core
