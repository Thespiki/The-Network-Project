#include "app/LearningNarrator.h"

#include "utilities/StringUtilities.h"

#include <format>

namespace tnp::app {

using namespace core;

LearningNarrator::LearningNarrator(sim::Simulator& simulator) : simulator_(simulator) {
    token_ = simulator_.addTraceObserver([this](const TraceEvent& event) { onTrace(event); });
}

LearningNarrator::~LearningNarrator() { simulator_.removeTraceObserver(token_); }

void LearningNarrator::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) steps_.clear();
}

void LearningNarrator::onTrace(const TraceEvent& event) {
    if (!enabled_) return;

    auto step = explain(event);
    if (!step) return;

    steps_.push_back(std::move(*step));
    if (steps_.size() > limit_) {
        steps_.erase(steps_.begin(), steps_.begin() + static_cast<std::ptrdiff_t>(steps_.size() - limit_));
    }
}

std::optional<LearningStep> LearningNarrator::explain(const TraceEvent& event) const {
    const std::string device = simulator_.deviceName(event.device);
    if (device.empty()) return std::nullopt;

    LearningStep step;
    step.time = event.time;
    step.category = event.category();
    step.device = event.device;

    // Each case reads only the fields the engine attached, so a change of wording
    // never requires touching a protocol implementation.
    switch (event.kind) {
        case TraceKind::ArpCacheMiss:
            step.headline = std::format("{} does not know the MAC address of {}", device,
                                        event.field("address"));
            step.explanation =
                "An Ethernet frame needs a destination MAC address, and IPv4 only supplies an IP "
                "address. Before it can send anything, the host must find out which hardware "
                "address answers for that IP.";
            break;

        case TraceKind::ArpRequestSent:
            step.headline = std::format("{} broadcasts an ARP request for {}", device,
                                        event.field("target-ip"));
            step.explanation = std::format(
                "The request goes to the broadcast address FF:FF:FF:FF:FF:FF, so every device on "
                "the segment sees it. It says: whoever owns {}, tell {} at {}. The packet waiting "
                "to be sent is held in a queue until the answer arrives.",
                event.field("target-ip"), event.field("sender-ip"), event.field("sender-mac"));
            break;

        case TraceKind::ArpRequestReceived:
            step.headline = std::format("{} receives the ARP request", device);
            step.explanation = std::format(
                "Every device on the segment received the broadcast, but only the one that owns {} "
                "will answer. The others discard it.",
                event.field("target-ip"));
            break;

        case TraceKind::ArpReplySent:
            step.headline = std::format("{} answers: {} is at {}", device, event.field("sender-ip"),
                                        event.field("sender-mac"));
            step.explanation =
                "The reply is a unicast frame sent straight back to the asker, not a broadcast: "
                "the answering device already learned the asker's hardware address from the request.";
            break;

        case TraceKind::ArpResolved:
            step.headline = std::format("{} can now send its queued traffic", device);
            step.explanation = std::format(
                "The mapping {} to {} goes into the ARP cache, and the {} packet(s) that were "
                "waiting are sent immediately. The next packet to the same destination will not "
                "need ARP at all.",
                event.field("address"), event.field("mac"), event.field("queued"));
            break;

        case TraceKind::ArpCacheHit:
            step.headline = std::format("{} already knows {}", device, event.field("address"));
            step.explanation =
                "The mapping is still in the ARP cache, so the frame goes out immediately. Cached "
                "entries expire after a few minutes so the network can react to hardware changes.";
            break;

        case TraceKind::ArpTimedOut:
            step.headline = std::format("Nothing answered ARP for {}", event.field("address"));
            step.explanation = std::format(
                "After {} attempts the host gave up and dropped {} queued packet(s). Either nothing "
                "on this segment owns that address, or the device that does is unreachable.",
                event.field("attempts"), event.field("dropped"));
            break;

        case TraceKind::MacLearned:
            step.headline = std::format("{} learns where {} lives", device, event.field("mac"));
            step.explanation = std::format(
                "A switch learns by watching source addresses: this frame arrived on {}, so {} must "
                "be reachable through that port in VLAN {}. From now on traffic for that address is "
                "sent to that one port instead of every port.",
                event.field("port"), event.field("mac"), event.field("vlan"));
            break;

        case TraceKind::FrameFlooded:
            step.headline = std::format("{} floods the frame to {} port(s)", device,
                                        event.field("ports"));
            step.explanation =
                event.field("reason") == "broadcast"
                    ? "Broadcast frames are meant for everyone on the segment, so the switch sends a "
                      "copy out of every port in the VLAN except the one it came from."
                    : std::format("The destination address is not in the forwarding database, so the "
                                  "switch does the only safe thing: it sends the frame everywhere in "
                                  "VLAN {} except back where it came from. The reply will teach it "
                                  "where that address really is.",
                                  event.field("vlan"));
            break;

        case TraceKind::RouteSelected:
        case TraceKind::RouteLookup:
            step.headline = std::format("{} looks up a route to {}", device,
                                        event.field("destination-ip"));
            step.explanation = std::format(
                "The routing table is searched for the entry with the longest matching prefix - the "
                "most specific route wins. The match here is {}.",
                event.field("matched").empty() ? event.field("route") : event.field("matched"));
            break;

        case TraceKind::IpNoRouteToHost:
            step.headline = std::format("{} has no route to {}", device,
                                        event.field("destination-ip"));
            step.explanation =
                "No entry in the routing table covers that destination, and there is no default "
                "route to fall back on, so the packet cannot be forwarded. A router reports this "
                "back to the sender with an ICMP destination-unreachable message.";
            break;

        case TraceKind::IpPacketForwarded:
            step.headline = std::format("{} forwards the packet out {}", device,
                                        event.field("egress-interface"));
            step.explanation = std::format(
                "The router rewrites the Ethernet header for the next hop ({}) and decrements the "
                "TTL to {}. The IP addresses do not change - only the layer-2 envelope does, at "
                "every hop.",
                event.field("next-hop"), event.field("ttl"));
            break;

        case TraceKind::IpTtlExpired:
            step.headline = "The packet ran out of time to live";
            step.explanation = std::format(
                "Every router decrements the TTL by one. When it reaches zero the packet is "
                "discarded at {} and an ICMP time-exceeded message goes back to the sender. This is "
                "what stops a routing loop from circulating forever, and it is how traceroute works.",
                device);
            break;

        case TraceKind::IcmpEchoRequestSent:
            step.headline = std::format("{} sends an ICMP echo request to {}", device,
                                        event.field("destination-ip"));
            step.explanation = std::format(
                "This is what ping actually sends. The identifier ({}) and sequence number ({}) let "
                "the sender match the reply to the right request.",
                event.field("identifier"), event.field("sequence"));
            break;

        case TraceKind::IcmpEchoReplySent:
            step.headline = std::format("{} sends the echo reply back", device);
            step.explanation =
                "The destination copies the request's identifier, sequence number and payload into a "
                "reply and routes it back the same way any other packet would travel.";
            break;

        case TraceKind::PingReplyReceived: {
            const Duration rtt = nanoseconds(strings::parseInt(event.field("rtt-ns")).value_or(0));
            step.headline = std::format("Reply received in {}", formatDuration(rtt));
            step.explanation = std::format(
                "The round trip took {} and the reply arrived with TTL {}. A TTL lower than the "
                "sender's starting value tells you how many routers the packet crossed.",
                formatDuration(rtt), event.field("ttl"));
            break;
        }

        case TraceKind::PingTimedOut:
            step.headline = "No reply arrived in time";
            step.explanation = std::format(
                "Probe {} was given up on: {}. A lost reply is indistinguishable from a lost request, "
                "so ping cannot tell you which direction failed.",
                event.field("sequence"), event.field("reason"));
            break;

        case TraceKind::FirewallDenied:
            step.headline = std::format("{} blocks the packet", device);
            step.explanation = std::format(
                "The traffic from {} to {} matched the rule \"{}\", which denies it. The packet is "
                "dropped and nothing is forwarded.",
                event.field("source-ip"), event.field("destination-ip"), event.field("rule"));
            break;

        case TraceKind::DhcpDiscoverSent:
            step.headline = std::format("{} is looking for a DHCP server", device);
            step.explanation =
                "The client has no address yet, so it sends from 0.0.0.0 to the broadcast address "
                "255.255.255.255. Any DHCP server on the segment can answer.";
            break;

        case TraceKind::DhcpLeaseAssigned:
            step.headline = std::format("{} is configured with {}", device, event.field("address"));
            step.explanation = std::format(
                "The four-step exchange (discover, offer, request, acknowledge) is complete. The "
                "client also received a gateway ({}) and a DNS server ({}).",
                event.field("gateway"), event.field("dns"));
            break;

        case TraceKind::FrameFilteredByVlan:
            step.headline = std::format("{} drops the frame at {}", device, event.field("interface"));
            step.explanation = std::format(
                "The frame is tagged for VLAN {}, which this port does not carry. VLANs keep traffic "
                "separated even though it shares the same physical switch.",
                event.field("vlan"));
            break;

        default:
            return std::nullopt;
    }

    return step;
}

} // namespace tnp::app
