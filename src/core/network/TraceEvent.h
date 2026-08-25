#pragma once

#include "core/network/Ids.h"
#include "utilities/Time.h"
#include "utilities/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace tnp::core {

/// Every observable thing the simulated network does.
///
/// This enumeration is the single channel through which the engine reports what
/// happened. The log panel, the packet timeline, the automated tests and the
/// learning-mode narrator all consume the same events; none of them reaches into
/// device internals, and the engine never formats an explanation for a specific
/// consumer.
enum class TraceKind : u16 {
    // Device and interface lifecycle
    DevicePoweredOn,
    InterfaceStateChanged,

    // Layer 2
    FrameTransmitted,
    FrameReceived,
    FrameDropped,
    FrameFlooded,
    FrameSwitched,
    FrameFilteredByVlan,
    MacLearned,
    MacEntryAged,

    // ARP
    ArpCacheHit,
    ArpCacheMiss,
    ArpRequestSent,
    ArpRequestReceived,
    ArpReplySent,
    ArpReplyReceived,
    ArpResolved,
    ArpTimedOut,
    ArpEntryExpired,

    // IPv4
    IpPacketCreated,
    IpPacketDelivered,
    IpPacketForwarded,
    IpPacketDropped,
    IpTtlExpired,
    IpNoRouteToHost,
    IpChecksumInvalid,
    IpFragmentationNeeded,

    // Routing
    RouteLookup,
    RouteSelected,
    RouteAdded,
    RouteRemoved,

    // ICMP
    IcmpEchoRequestSent,
    IcmpEchoRequestReceived,
    IcmpEchoReplySent,
    IcmpEchoReplyReceived,
    IcmpDestinationUnreachableSent,
    IcmpDestinationUnreachableReceived,
    IcmpTimeExceededSent,
    IcmpTimeExceededReceived,

    // Transport
    UdpDatagramSent,
    UdpDatagramReceived,
    UdpPortUnreachable,
    TcpSegmentSent,
    TcpSegmentReceived,
    TcpStateChanged,

    // Services
    DhcpDiscoverSent,
    DhcpOfferSent,
    DhcpRequestSent,
    DhcpAckSent,
    DhcpNakSent,
    DhcpLeaseAssigned,
    DhcpNoAddressAvailable,
    DnsQuerySent,
    DnsResponseSent,
    DnsNameResolved,
    DnsNameNotFound,

    // Policy
    FirewallPermitted,
    FirewallDenied,

    // Application level results
    PingStarted,
    PingReplyReceived,
    PingTimedOut,
    PingFinished
};

[[nodiscard]] std::string_view traceKindName(TraceKind kind);

/// Coarse grouping used for log filtering and for the learning-mode chapters.
enum class TraceCategory : u8 { Device, Layer2, Arp, Ipv4, Routing, Icmp, Transport, Service, Policy, Application };

[[nodiscard]] TraceCategory traceKindCategory(TraceKind kind);
[[nodiscard]] std::string_view traceCategoryName(TraceCategory category);

/// A structured attribute attached to a trace event. Keys are stable
/// identifiers ("source-ip", "ttl"), which is what lets the learning system
/// build sentences without the engine knowing any human language.
struct TraceField {
    std::string key;
    std::string value;
};

/// One thing that happened, at one instant, at one device.
struct TraceEvent {
    TraceKind kind = TraceKind::DevicePoweredOn;
    SimTime time{};

    DeviceId device;
    InterfaceId interface;
    PacketId packet;

    /// Engine-authored one-line description. Factual, never explanatory:
    /// interpretation is the learning system's job.
    std::string summary;

    std::vector<TraceField> fields;

    /// Monotonic ordering key assigned by the simulator.
    u64 sequence = 0;

    [[nodiscard]] TraceCategory category() const { return traceKindCategory(kind); }

    /// Value of a structured field, or an empty string when absent.
    [[nodiscard]] std::string field(std::string_view key) const;
    [[nodiscard]] bool hasField(std::string_view key) const;

    TraceEvent& with(std::string key, std::string value) {
        fields.push_back(TraceField{std::move(key), std::move(value)});
        return *this;
    }
};

} // namespace tnp::core
