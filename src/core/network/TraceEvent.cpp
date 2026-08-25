#include "core/network/TraceEvent.h"

#include <algorithm>

namespace tnp::core {

std::string_view traceKindName(TraceKind kind) {
    switch (kind) {
        case TraceKind::DevicePoweredOn:                   return "DevicePoweredOn";
        case TraceKind::InterfaceStateChanged:             return "InterfaceStateChanged";
        case TraceKind::FrameTransmitted:                  return "FrameTransmitted";
        case TraceKind::FrameReceived:                     return "FrameReceived";
        case TraceKind::FrameDropped:                      return "FrameDropped";
        case TraceKind::FrameFlooded:                      return "FrameFlooded";
        case TraceKind::FrameSwitched:                     return "FrameSwitched";
        case TraceKind::FrameFilteredByVlan:               return "FrameFilteredByVlan";
        case TraceKind::MacLearned:                        return "MacLearned";
        case TraceKind::MacEntryAged:                      return "MacEntryAged";
        case TraceKind::ArpCacheHit:                       return "ArpCacheHit";
        case TraceKind::ArpCacheMiss:                      return "ArpCacheMiss";
        case TraceKind::ArpRequestSent:                    return "ArpRequestSent";
        case TraceKind::ArpRequestReceived:                return "ArpRequestReceived";
        case TraceKind::ArpReplySent:                      return "ArpReplySent";
        case TraceKind::ArpReplyReceived:                  return "ArpReplyReceived";
        case TraceKind::ArpResolved:                       return "ArpResolved";
        case TraceKind::ArpTimedOut:                       return "ArpTimedOut";
        case TraceKind::ArpEntryExpired:                   return "ArpEntryExpired";
        case TraceKind::IpPacketCreated:                   return "IpPacketCreated";
        case TraceKind::IpPacketDelivered:                 return "IpPacketDelivered";
        case TraceKind::IpPacketForwarded:                 return "IpPacketForwarded";
        case TraceKind::IpPacketDropped:                   return "IpPacketDropped";
        case TraceKind::IpTtlExpired:                      return "IpTtlExpired";
        case TraceKind::IpNoRouteToHost:                   return "IpNoRouteToHost";
        case TraceKind::IpChecksumInvalid:                 return "IpChecksumInvalid";
        case TraceKind::IpFragmentationNeeded:             return "IpFragmentationNeeded";
        case TraceKind::RouteLookup:                       return "RouteLookup";
        case TraceKind::RouteSelected:                     return "RouteSelected";
        case TraceKind::RouteAdded:                        return "RouteAdded";
        case TraceKind::RouteRemoved:                      return "RouteRemoved";
        case TraceKind::IcmpEchoRequestSent:               return "IcmpEchoRequestSent";
        case TraceKind::IcmpEchoRequestReceived:           return "IcmpEchoRequestReceived";
        case TraceKind::IcmpEchoReplySent:                 return "IcmpEchoReplySent";
        case TraceKind::IcmpEchoReplyReceived:             return "IcmpEchoReplyReceived";
        case TraceKind::IcmpDestinationUnreachableSent:    return "IcmpDestinationUnreachableSent";
        case TraceKind::IcmpDestinationUnreachableReceived:return "IcmpDestinationUnreachableReceived";
        case TraceKind::IcmpTimeExceededSent:              return "IcmpTimeExceededSent";
        case TraceKind::IcmpTimeExceededReceived:          return "IcmpTimeExceededReceived";
        case TraceKind::UdpDatagramSent:                   return "UdpDatagramSent";
        case TraceKind::UdpDatagramReceived:               return "UdpDatagramReceived";
        case TraceKind::UdpPortUnreachable:                return "UdpPortUnreachable";
        case TraceKind::TcpSegmentSent:                    return "TcpSegmentSent";
        case TraceKind::TcpSegmentReceived:                return "TcpSegmentReceived";
        case TraceKind::TcpStateChanged:                   return "TcpStateChanged";
        case TraceKind::DhcpDiscoverSent:                  return "DhcpDiscoverSent";
        case TraceKind::DhcpOfferSent:                     return "DhcpOfferSent";
        case TraceKind::DhcpRequestSent:                   return "DhcpRequestSent";
        case TraceKind::DhcpAckSent:                       return "DhcpAckSent";
        case TraceKind::DhcpNakSent:                       return "DhcpNakSent";
        case TraceKind::DhcpLeaseAssigned:                 return "DhcpLeaseAssigned";
        case TraceKind::DhcpNoAddressAvailable:            return "DhcpNoAddressAvailable";
        case TraceKind::DnsQuerySent:                      return "DnsQuerySent";
        case TraceKind::DnsResponseSent:                   return "DnsResponseSent";
        case TraceKind::DnsNameResolved:                   return "DnsNameResolved";
        case TraceKind::DnsNameNotFound:                   return "DnsNameNotFound";
        case TraceKind::FirewallPermitted:                 return "FirewallPermitted";
        case TraceKind::FirewallDenied:                    return "FirewallDenied";
        case TraceKind::PingStarted:                       return "PingStarted";
        case TraceKind::PingReplyReceived:                 return "PingReplyReceived";
        case TraceKind::PingTimedOut:                      return "PingTimedOut";
        case TraceKind::PingFinished:                      return "PingFinished";
    }
    return "Unknown";
}

TraceCategory traceKindCategory(TraceKind kind) {
    switch (kind) {
        case TraceKind::DevicePoweredOn:
        case TraceKind::InterfaceStateChanged:
            return TraceCategory::Device;

        case TraceKind::FrameTransmitted:
        case TraceKind::FrameReceived:
        case TraceKind::FrameDropped:
        case TraceKind::FrameFlooded:
        case TraceKind::FrameSwitched:
        case TraceKind::FrameFilteredByVlan:
        case TraceKind::MacLearned:
        case TraceKind::MacEntryAged:
            return TraceCategory::Layer2;

        case TraceKind::ArpCacheHit:
        case TraceKind::ArpCacheMiss:
        case TraceKind::ArpRequestSent:
        case TraceKind::ArpRequestReceived:
        case TraceKind::ArpReplySent:
        case TraceKind::ArpReplyReceived:
        case TraceKind::ArpResolved:
        case TraceKind::ArpTimedOut:
        case TraceKind::ArpEntryExpired:
            return TraceCategory::Arp;

        case TraceKind::IpPacketCreated:
        case TraceKind::IpPacketDelivered:
        case TraceKind::IpPacketForwarded:
        case TraceKind::IpPacketDropped:
        case TraceKind::IpTtlExpired:
        case TraceKind::IpNoRouteToHost:
        case TraceKind::IpChecksumInvalid:
        case TraceKind::IpFragmentationNeeded:
            return TraceCategory::Ipv4;

        case TraceKind::RouteLookup:
        case TraceKind::RouteSelected:
        case TraceKind::RouteAdded:
        case TraceKind::RouteRemoved:
            return TraceCategory::Routing;

        case TraceKind::IcmpEchoRequestSent:
        case TraceKind::IcmpEchoRequestReceived:
        case TraceKind::IcmpEchoReplySent:
        case TraceKind::IcmpEchoReplyReceived:
        case TraceKind::IcmpDestinationUnreachableSent:
        case TraceKind::IcmpDestinationUnreachableReceived:
        case TraceKind::IcmpTimeExceededSent:
        case TraceKind::IcmpTimeExceededReceived:
            return TraceCategory::Icmp;

        case TraceKind::UdpDatagramSent:
        case TraceKind::UdpDatagramReceived:
        case TraceKind::UdpPortUnreachable:
        case TraceKind::TcpSegmentSent:
        case TraceKind::TcpSegmentReceived:
        case TraceKind::TcpStateChanged:
            return TraceCategory::Transport;

        case TraceKind::DhcpDiscoverSent:
        case TraceKind::DhcpOfferSent:
        case TraceKind::DhcpRequestSent:
        case TraceKind::DhcpAckSent:
        case TraceKind::DhcpNakSent:
        case TraceKind::DhcpLeaseAssigned:
        case TraceKind::DhcpNoAddressAvailable:
        case TraceKind::DnsQuerySent:
        case TraceKind::DnsResponseSent:
        case TraceKind::DnsNameResolved:
        case TraceKind::DnsNameNotFound:
            return TraceCategory::Service;

        case TraceKind::FirewallPermitted:
        case TraceKind::FirewallDenied:
            return TraceCategory::Policy;

        case TraceKind::PingStarted:
        case TraceKind::PingReplyReceived:
        case TraceKind::PingTimedOut:
        case TraceKind::PingFinished:
            return TraceCategory::Application;
    }
    return TraceCategory::Device;
}

std::string_view traceCategoryName(TraceCategory category) {
    switch (category) {
        case TraceCategory::Device:      return "Device";
        case TraceCategory::Layer2:      return "Layer 2";
        case TraceCategory::Arp:         return "ARP";
        case TraceCategory::Ipv4:        return "IPv4";
        case TraceCategory::Routing:     return "Routing";
        case TraceCategory::Icmp:        return "ICMP";
        case TraceCategory::Transport:   return "Transport";
        case TraceCategory::Service:     return "Services";
        case TraceCategory::Policy:      return "Policy";
        case TraceCategory::Application: return "Application";
    }
    return "Device";
}

std::string TraceEvent::field(std::string_view key) const {
    const auto it = std::find_if(fields.begin(), fields.end(),
                                 [key](const TraceField& f) { return f.key == key; });
    return it == fields.end() ? std::string{} : it->value;
}

bool TraceEvent::hasField(std::string_view key) const {
    return std::any_of(fields.begin(), fields.end(),
                       [key](const TraceField& f) { return f.key == key; });
}

} // namespace tnp::core
