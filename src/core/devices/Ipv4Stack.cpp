#include "core/devices/Ipv4Stack.h"

#include "core/network/Device.h"
#include "core/protocols/Arp.h"
#include "core/protocols/Icmp.h"
#include "core/protocols/Tcp.h"

#include <algorithm>
#include <format>

namespace tnp::core {
namespace {

using namespace proto;

/// How often expired ARP entries are swept out.
constexpr Duration kArpSweepInterval = seconds(30);

/// Classic ping filler: the printable ASCII run real implementations use.
ByteBuffer makeEchoPayload(std::size_t size) {
    ByteBuffer payload(size);
    for (std::size_t i = 0; i < size; ++i) {
        payload[i] = static_cast<u8>('a' + (i % 23));
    }
    return payload;
}

} // namespace

Ipv4Stack::Ipv4Stack(Device& owner) : owner_(owner) {}

std::string Ipv4Stack::ownerName() const { return owner_.name(); }

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

Status Ipv4Stack::addStaticRoute(StaticRouteEntry entry) {
    if (!entry.id.isValid()) entry.id = RouteId::generate();

    const auto duplicate = std::find_if(staticRoutes_.begin(), staticRoutes_.end(),
                                        [&](const StaticRouteEntry& existing) {
        return existing.destination.network() == entry.destination.network() &&
               existing.nextHop == entry.nextHop &&
               existing.egressInterface == entry.egressInterface;
    });
    if (duplicate != staticRoutes_.end()) {
        return Status::failure(std::format("a static route for {} via the same next hop already exists",
                                           entry.destination.toNetworkString()));
    }

    const auto resolution = resolveStaticRoute(owner_, entry);
    if (!resolution.route) return Status::failure(resolution.problem);

    staticRoutes_.push_back(std::move(entry));
    refreshRoutes();
    return Status::ok();
}

bool Ipv4Stack::removeStaticRoute(RouteId id) {
    const auto it = std::find_if(staticRoutes_.begin(), staticRoutes_.end(),
                                 [id](const StaticRouteEntry& entry) { return entry.id == id; });
    if (it == staticRoutes_.end()) return false;
    staticRoutes_.erase(it);
    refreshRoutes();
    return true;
}

void Ipv4Stack::setStaticRoutes(std::vector<StaticRouteEntry> routes) {
    staticRoutes_ = std::move(routes);
    for (auto& entry : staticRoutes_) {
        if (!entry.id.isValid()) entry.id = RouteId::generate();
    }
    refreshRoutes();
}

void Ipv4Stack::setDefaultGateway(std::optional<Ipv4Address> gateway) {
    // The default gateway is not a separate concept: it is the 0.0.0.0/0 static
    // route. Modelling it that way means one lookup path, and the routing table
    // shown by the CLI is the table actually used.
    const auto removed = std::remove_if(staticRoutes_.begin(), staticRoutes_.end(),
                                        [](const StaticRouteEntry& entry) {
        return entry.destination.prefixLength() == 0;
    });
    staticRoutes_.erase(removed, staticRoutes_.end());

    if (gateway) {
        StaticRouteEntry entry;
        entry.id = RouteId::generate();
        entry.destination = Ipv4Prefix{Ipv4Address::any(), 0};
        entry.nextHop = *gateway;
        entry.metric = 1;
        entry.description = "default gateway";
        staticRoutes_.push_back(std::move(entry));
    }
    refreshRoutes();
}

std::optional<Ipv4Address> Ipv4Stack::defaultGateway() const {
    for (const StaticRouteEntry& entry : staticRoutes_) {
        if (entry.destination.prefixLength() == 0 && entry.nextHop) return entry.nextHop;
    }
    return std::nullopt;
}

void Ipv4Stack::refreshRoutes() {
    rebuildRoutingTable(owner_, staticRoutes_, routingTable_);
}

void Ipv4Stack::bindUdpPort(u16 port, UdpHandler handler) {
    udpHandlers_[port] = std::move(handler);
}

void Ipv4Stack::unbindUdpPort(u16 port) { udpHandlers_.erase(port); }

bool Ipv4Stack::isUdpPortBound(u16 port) const { return udpHandlers_.contains(port); }

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Ipv4Stack::onPowerOn(DeviceContext& context) {
    refreshRoutes();
    scheduleArpSweep(context);
}

void Ipv4Stack::onReset() {
    arpCache_.clear();
    pendingArp_.clear();
    pings_.clear();
    timers_.clear();
    routingTable_.clear();
    nextPingId_ = 1;
    nextIpIdentification_ = 1;
}

TimerId Ipv4Stack::armTimer(DeviceContext& context, TimerPurpose purpose, Duration delay) {
    const TimerId id = context.nextTimerId();
    timers_[id] = purpose;
    context.scheduleTimer(owner_, id, delay);
    return id;
}

void Ipv4Stack::scheduleArpSweep(DeviceContext& context) {
    armTimer(context, TimerPurpose{TimerKind::ArpSweep, 0, 0}, kArpSweepInterval);
}

// ---------------------------------------------------------------------------
// Receive path
// ---------------------------------------------------------------------------

bool Ipv4Stack::acceptsDestinationMac(const Interface& ingress, MacAddress destination) const {
    return destination.isBroadcast() || destination.isMulticast() ||
           destination == ingress.macAddress();
}

bool Ipv4Stack::onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) {
    const auto ethernet = decodeEthernet(frame.bytes);
    if (!ethernet) {
        context.trace(TraceEvent{.kind = TraceKind::FrameDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = "malformed Ethernet frame discarded"});
        return true;
    }

    if (!acceptsDestinationMac(ingress, ethernet->header.destination)) {
        context.trace(TraceEvent{.kind = TraceKind::FrameDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("frame for {} is not addressed to {}",
                                                        ethernet->header.destination.toString(),
                                                        ingress.name())}
                          .with("destination-mac", ethernet->header.destination.toString())
                          .with("interface-mac", ingress.macAddress().toString()));
        return true;
    }

    switch (static_cast<EtherType>(ethernet->header.etherType)) {
        case EtherType::Arp:
            handleArp(context, ingress, ethernet->payload, frame);
            return true;
        case EtherType::Ipv4:
            handleIpv4(context, ingress, ethernet->payload, frame);
            return true;
        default:
            context.trace(TraceEvent{.kind = TraceKind::FrameDropped,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .interface = ingress.id(),
                                     .packet = frame.id,
                                     .summary = std::format("EtherType {} is not handled by this device",
                                                            etherTypeName(ethernet->header.etherType))});
            return true;
    }
}

void Ipv4Stack::handleArp(DeviceContext& context, Interface& ingress, std::span<const u8> payload,
                          const Frame& frame) {
    const auto arp = decodeArp(payload);
    if (!arp) {
        context.trace(TraceEvent{.kind = TraceKind::FrameDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = "malformed ARP packet discarded"});
        return;
    }

    const bool targetsUs = owner_.ownsIpv4Address(arp->targetIp);

    // RFC 826: any ARP packet refreshes an existing binding, but only a packet
    // addressed to us is allowed to create one. That keeps the cache from
    // filling with every address that ever broadcast on the segment.
    const bool refreshed = arpCache_.refresh(arp->senderIp, arp->senderMac, ingress.id(), context.now());
    if (!refreshed && targetsUs && !arp->senderIp.isUnspecified()) {
        arpCache_.insert(arp->senderIp, arp->senderMac, ingress.id(), context.now());
    }

    if (arp->operation == ArpOperation::Request) {
        context.trace(TraceEvent{.kind = TraceKind::ArpRequestReceived,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("who has {}? tell {}",
                                                        arp->targetIp.toString(), arp->senderIp.toString())}
                          .with("target-ip", arp->targetIp.toString())
                          .with("sender-ip", arp->senderIp.toString())
                          .with("sender-mac", arp->senderMac.toString()));

        if (!targetsUs) return;

        const ArpMessage reply = makeArpReply(*arp, ingress.macAddress());
        EthernetHeader header;
        header.destination = arp->senderMac;
        header.source = ingress.macAddress();
        header.etherType = static_cast<u16>(EtherType::Arp);

        auto replyFrame = context.makeFrame(
            owner_, encodeEthernet(header, encodeArp(reply)), FrameCategory::Arp,
            std::format("ARP reply {} is at {}", reply.senderIp.toString(), reply.senderMac.toString()));

        context.trace(TraceEvent{.kind = TraceKind::ArpReplySent,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = replyFrame.id,
                                 .summary = std::format("{} is at {}", reply.senderIp.toString(),
                                                        reply.senderMac.toString())}
                          .with("sender-ip", reply.senderIp.toString())
                          .with("sender-mac", reply.senderMac.toString())
                          .with("target-ip", reply.targetIp.toString()));

        context.transmit(owner_, ingress, std::move(replyFrame));
        return;
    }

    // A reply is always solicited, so it is safe to cache unconditionally.
    arpCache_.insert(arp->senderIp, arp->senderMac, ingress.id(), context.now());

    context.trace(TraceEvent{.kind = TraceKind::ArpReplyReceived,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = ingress.id(),
                             .packet = frame.id,
                             .summary = std::format("{} is at {}", arp->senderIp.toString(),
                                                    arp->senderMac.toString())}
                      .with("sender-ip", arp->senderIp.toString())
                      .with("sender-mac", arp->senderMac.toString()));

    const auto pending = pendingArp_.find(arp->senderIp);
    if (pending == pendingArp_.end()) return;

    context.trace(TraceEvent{.kind = TraceKind::ArpResolved,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = ingress.id(),
                             .summary = std::format("{} resolved to {}, releasing {} queued packet(s)",
                                                    arp->senderIp.toString(), arp->senderMac.toString(),
                                                    pending->second.queue.size())}
                      .with("address", arp->senderIp.toString())
                      .with("mac", arp->senderMac.toString())
                      .with("queued", std::to_string(pending->second.queue.size())));

    context.cancelTimer(owner_, pending->second.timer);
    timers_.erase(pending->second.timer);

    PendingArp released = std::move(pending->second);
    pendingArp_.erase(pending);

    Interface* egress = owner_.findInterface(released.egress);
    if (egress == nullptr) return;

    for (auto& queued : released.queue) {
        transmitIpv4Frame(context, *egress, arp->senderMac, queued.ipPacket, queued.category,
                          queued.summary, queued.inheritedIdentity);
    }
}

void Ipv4Stack::handleIpv4(DeviceContext& context, Interface& ingress, std::span<const u8> payload,
                           const Frame& frame) {
    const auto packet = decodeIpv4(payload);
    if (!packet) {
        context.trace(TraceEvent{.kind = TraceKind::IpPacketDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = "malformed IPv4 packet discarded"});
        return;
    }

    if (!ipv4HeaderChecksumValid(packet->headerBytes)) {
        context.trace(TraceEvent{.kind = TraceKind::IpChecksumInvalid,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = "IPv4 header checksum is wrong; packet discarded"});
        return;
    }

    const Ipv4Address destination = packet->header.destination;
    const bool forUs = isLocalDestination(destination) || destination.isLimitedBroadcast() ||
                       isBroadcastFor(ingress, destination);

    if (forUs) {
        deliverLocally(context, ingress, *packet, frame);
        return;
    }

    if (!forwarding_) {
        context.trace(TraceEvent{.kind = TraceKind::IpPacketDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("{} is not an address of {} and forwarding is disabled",
                                                        destination.toString(), ownerName())}
                          .with("destination-ip", destination.toString()));
        return;
    }

    forwardIpv4(context, ingress, *packet, frame);
}

void Ipv4Stack::forwardIpv4(DeviceContext& context, Interface& ingress,
                            const Ipv4PacketView& packet, const Frame& frame) {
    const Ipv4Address destination = packet.header.destination;

    if (packet.header.ttl <= 1) {
        context.trace(TraceEvent{.kind = TraceKind::IpTtlExpired,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("TTL reached zero at {}; packet to {} discarded",
                                                        ownerName(), destination.toString())}
                          .with("destination-ip", destination.toString())
                          .with("ttl", std::to_string(packet.header.ttl)));

        sendIcmpError(context, IcmpType::TimeExceeded,
                      static_cast<u8>(IcmpTimeExceededCode::TtlExpiredInTransit), packet);
        return;
    }

    const Route* route = routingTable_.lookup(destination);

    context.trace(TraceEvent{.kind = TraceKind::RouteLookup,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = ingress.id(),
                             .packet = frame.id,
                             .summary = route ? std::format("{} matches {}", destination.toString(),
                                                            route->toString())
                                              : std::format("{} matches no route", destination.toString())}
                      .with("destination-ip", destination.toString())
                      .with("matched", route ? route->destination.toNetworkString() : std::string{"none"}));

    if (route == nullptr) {
        context.trace(TraceEvent{.kind = TraceKind::IpNoRouteToHost,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("{} has no route to {}", ownerName(),
                                                        destination.toString())}
                          .with("destination-ip", destination.toString()));

        sendIcmpError(context, IcmpType::DestinationUnreachable,
                      static_cast<u8>(IcmpUnreachableCode::NetworkUnreachable), packet);
        return;
    }

    Interface* egress = owner_.findInterface(route->egressInterface);
    if (egress == nullptr || !egress->isOperational()) {
        context.trace(TraceEvent{.kind = TraceKind::IpPacketDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("egress interface for {} is down",
                                                        route->destination.toNetworkString())});

        sendIcmpError(context, IcmpType::DestinationUnreachable,
                      static_cast<u8>(IcmpUnreachableCode::HostUnreachable), packet);
        return;
    }

    if (forwardingFilter_ && !forwardingFilter_(context, ingress, *egress, packet.header, packet.payload)) {
        // The filter is responsible for tracing why it refused the packet.
        return;
    }

    if (packet.datagram.size() > egress->mtu()) {
        const bool mayFragment = !packet.header.dontFragment;
        context.trace(TraceEvent{.kind = TraceKind::IpFragmentationNeeded,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = egress->id(),
                                 .packet = frame.id,
                                 .summary = std::format(
                                     "{} byte packet exceeds the {} byte MTU of {}{}",
                                     packet.datagram.size(), egress->mtu(), egress->name(),
                                     mayFragment ? " and IPv4 fragmentation is not simulated" : "")}
                          .with("packet-size", std::to_string(packet.datagram.size()))
                          .with("mtu", std::to_string(egress->mtu())));

        if (!mayFragment) {
            sendIcmpError(context, IcmpType::DestinationUnreachable,
                          static_cast<u8>(IcmpUnreachableCode::FragmentationNeeded), packet);
        }
        return;
    }

    ByteBuffer forwarded(packet.datagram.begin(), packet.datagram.end());
    setIpv4Ttl(forwarded, static_cast<u8>(packet.header.ttl - 1));

    const Ipv4Address nextHop = route->nextHop.value_or(destination);

    context.trace(TraceEvent{.kind = TraceKind::IpPacketForwarded,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = egress->id(),
                             .packet = frame.id,
                             .summary = std::format("{} forwarded {} -> {} out {} (TTL {} -> {})",
                                                    ownerName(), packet.header.source.toString(),
                                                    destination.toString(), egress->name(),
                                                    packet.header.ttl, packet.header.ttl - 1)}
                      .with("source-ip", packet.header.source.toString())
                      .with("destination-ip", destination.toString())
                      .with("next-hop", nextHop.toString())
                      .with("egress-interface", egress->name())
                      .with("ttl", std::to_string(packet.header.ttl - 1)));

    FrameIdentity identity = frame.identity();
    ++identity.hopCount;
    emitOnInterface(context, *egress, nextHop, std::move(forwarded), frame.category, frame.summary,
                    identity);
}

void Ipv4Stack::deliverLocally(DeviceContext& context, Interface& ingress,
                               const Ipv4PacketView& packet, const Frame& frame) {
    context.trace(TraceEvent{.kind = TraceKind::IpPacketDelivered,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = ingress.id(),
                             .packet = frame.id,
                             .summary = std::format("{} accepted a {} packet from {}", ownerName(),
                                                    ipProtocolName(packet.header.protocol),
                                                    packet.header.source.toString())}
                      .with("source-ip", packet.header.source.toString())
                      .with("destination-ip", packet.header.destination.toString())
                      .with("protocol", ipProtocolName(packet.header.protocol)));

    switch (static_cast<IpProtocol>(packet.header.protocol)) {
        case IpProtocol::Icmp:
            handleIcmp(context, ingress, packet, frame);
            return;
        case IpProtocol::Udp:
            handleUdp(context, ingress, packet, frame);
            return;
        case IpProtocol::Tcp:
            // The header codec exists but no connection state machine runs yet,
            // so the segment is observed and discarded rather than answered.
            context.trace(TraceEvent{.kind = TraceKind::TcpSegmentReceived,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .interface = ingress.id(),
                                     .packet = frame.id,
                                     .summary = "TCP segment received; TCP is not simulated in this version"});
            return;
        default:
            sendIcmpError(context, IcmpType::DestinationUnreachable,
                          static_cast<u8>(IcmpUnreachableCode::ProtocolUnreachable), packet);
            return;
    }
}

void Ipv4Stack::handleIcmp(DeviceContext& context, Interface& ingress,
                           const Ipv4PacketView& packet, const Frame& frame) {
    const auto icmp = decodeIcmp(packet.payload);
    if (!icmp || !icmpChecksumValid(packet.payload)) {
        context.trace(TraceEvent{.kind = TraceKind::IpPacketDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = "malformed or corrupt ICMP message discarded"});
        return;
    }

    const auto type = static_cast<IcmpType>(icmp->type);

    if (type == IcmpType::EchoRequest) {
        context.trace(TraceEvent{.kind = TraceKind::IcmpEchoRequestReceived,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("echo request from {} (id {}, seq {})",
                                                        packet.header.source.toString(),
                                                        icmp->identifier, icmp->sequence)}
                          .with("source-ip", packet.header.source.toString())
                          .with("identifier", std::to_string(icmp->identifier))
                          .with("sequence", std::to_string(icmp->sequence)));

        const ByteBuffer reply = encodeIcmpEcho(IcmpType::EchoReply, icmp->identifier, icmp->sequence,
                                                icmp->payload);

        Ipv4SendOptions options;
        // Reply from the address that was pinged, so traceroute-style tools and
        // the packet inspector show a symmetric conversation.
        if (isLocalDestination(packet.header.destination)) options.source = packet.header.destination;

        const bool sent = sendIpv4(context, packet.header.source, static_cast<u8>(IpProtocol::Icmp),
                                   reply, FrameCategory::Icmp,
                                   std::format("ICMP echo reply id={} seq={}", icmp->identifier, icmp->sequence),
                                   options);

        if (sent) {
            context.trace(TraceEvent{.kind = TraceKind::IcmpEchoReplySent,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .summary = std::format("{} replied to {}", ownerName(),
                                                            packet.header.source.toString())}
                              .with("destination-ip", packet.header.source.toString())
                              .with("identifier", std::to_string(icmp->identifier))
                              .with("sequence", std::to_string(icmp->sequence)));
        }
        return;
    }

    if (type == IcmpType::EchoReply) {
        context.trace(TraceEvent{.kind = TraceKind::IcmpEchoReplyReceived,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("echo reply from {} (id {}, seq {})",
                                                        packet.header.source.toString(),
                                                        icmp->identifier, icmp->sequence)}
                          .with("source-ip", packet.header.source.toString())
                          .with("identifier", std::to_string(icmp->identifier))
                          .with("sequence", std::to_string(icmp->sequence))
                          .with("ttl", std::to_string(packet.header.ttl)));

        const auto session = pings_.find(icmp->identifier);
        if (session == pings_.end()) return;

        const auto outstanding = session->second.outstanding.find(icmp->sequence);
        if (outstanding == session->second.outstanding.end()) return;

        const Duration rtt = context.now() - outstanding->second;
        session->second.outstanding.erase(outstanding);

        PingStatistics& statistics = session->second.statistics;
        ++statistics.received;
        statistics.totalRtt += rtt;
        statistics.minimumRtt = std::min(statistics.minimumRtt, rtt);
        statistics.maximumRtt = std::max(statistics.maximumRtt, rtt);

        context.trace(TraceEvent{.kind = TraceKind::PingReplyReceived,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .packet = frame.id,
                                 .summary = std::format("reply from {}: seq={} ttl={} time={}",
                                                        packet.header.source.toString(), icmp->sequence,
                                                        packet.header.ttl, formatDuration(rtt))}
                          .with("source-ip", packet.header.source.toString())
                          .with("sequence", std::to_string(icmp->sequence))
                          .with("ttl", std::to_string(packet.header.ttl))
                          .with("rtt-ns", std::to_string(rtt.count()))
                          .with("ping-id", std::to_string(session->second.id)));

        finishPing(context, session->second);
        return;
    }

    if (icmp->isError()) {
        const TraceKind kind = (type == IcmpType::TimeExceeded)
                                   ? TraceKind::IcmpTimeExceededReceived
                                   : TraceKind::IcmpDestinationUnreachableReceived;

        context.trace(TraceEvent{.kind = kind,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("{} from {}: {}", icmpTypeName(icmp->type),
                                                        packet.header.source.toString(),
                                                        icmpCodeName(icmp->type, icmp->code))}
                          .with("reporter-ip", packet.header.source.toString())
                          .with("icmp-type", std::to_string(icmp->type))
                          .with("icmp-code", std::to_string(icmp->code))
                          .with("reason", icmpCodeName(icmp->type, icmp->code)));

        // The error quotes the packet that caused it - header plus eight bytes,
        // never the whole thing. If that packet was one of our echo requests,
        // fail its probe now instead of waiting for the timeout.
        const auto quoted = decodeIpv4Header(icmp->payload);
        if (!quoted) return;
        if (static_cast<IpProtocol>(quoted->header.protocol) != IpProtocol::Icmp) return;

        const auto quotedIcmp = decodeIcmp(quoted->payload);
        if (!quotedIcmp) return;

        const auto session = pings_.find(quotedIcmp->identifier);
        if (session == pings_.end()) return;

        const auto outstanding = session->second.outstanding.find(quotedIcmp->sequence);
        if (outstanding == session->second.outstanding.end()) return;

        session->second.outstanding.erase(outstanding);
        ++session->second.statistics.lost;

        context.trace(TraceEvent{.kind = TraceKind::PingTimedOut,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .summary = std::format("seq={} failed: {}", quotedIcmp->sequence,
                                                        icmpCodeName(icmp->type, icmp->code))}
                          .with("sequence", std::to_string(quotedIcmp->sequence))
                          .with("reason", icmpCodeName(icmp->type, icmp->code))
                          .with("ping-id", std::to_string(session->second.id)));

        finishPing(context, session->second);
    }
}

void Ipv4Stack::handleUdp(DeviceContext& context, Interface& ingress,
                          const Ipv4PacketView& packet, const Frame& frame) {
    const auto datagram = decodeUdp(packet.payload);
    if (!datagram) {
        context.trace(TraceEvent{.kind = TraceKind::IpPacketDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = "malformed UDP datagram discarded"});
        return;
    }

    context.trace(TraceEvent{.kind = TraceKind::UdpDatagramReceived,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = ingress.id(),
                             .packet = frame.id,
                             .summary = std::format("UDP {}:{} -> {}:{} ({} bytes)",
                                                    packet.header.source.toString(),
                                                    datagram->header.sourcePort,
                                                    packet.header.destination.toString(),
                                                    datagram->header.destinationPort,
                                                    datagram->payload.size())}
                      .with("source-ip", packet.header.source.toString())
                      .with("source-port", std::to_string(datagram->header.sourcePort))
                      .with("destination-port", std::to_string(datagram->header.destinationPort)));

    const auto handler = udpHandlers_.find(datagram->header.destinationPort);
    if (handler != udpHandlers_.end()) {
        handler->second(context, ingress, packet.header, datagram->header, datagram->payload);
        return;
    }

    context.trace(TraceEvent{.kind = TraceKind::UdpPortUnreachable,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = ingress.id(),
                             .packet = frame.id,
                             .summary = std::format("nothing is listening on UDP port {}",
                                                    datagram->header.destinationPort)}
                      .with("port", std::to_string(datagram->header.destinationPort)));

    // RFC 1122: never answer a broadcast with an ICMP error.
    if (!packet.header.destination.isLimitedBroadcast() && isLocalDestination(packet.header.destination)) {
        sendIcmpError(context, IcmpType::DestinationUnreachable,
                      static_cast<u8>(IcmpUnreachableCode::PortUnreachable), packet);
    }
}

// ---------------------------------------------------------------------------
// Send path
// ---------------------------------------------------------------------------

bool Ipv4Stack::isLocalDestination(Ipv4Address address) const {
    return owner_.ownsIpv4Address(address);
}

bool Ipv4Stack::isBroadcastFor(const Interface& iface, Ipv4Address address) const {
    for (const Ipv4Prefix& prefix : iface.ipv4Addresses()) {
        if (prefix.hasBroadcastAddress() && prefix.broadcastAddress() == address) return true;
    }
    return false;
}

std::optional<Ipv4Address> Ipv4Stack::selectSourceAddress(const Interface& egress,
                                                          Ipv4Address destination) const {
    // Prefer an address on the same subnet as the destination, which keeps the
    // conversation symmetric on multi-homed interfaces.
    for (const Ipv4Prefix& prefix : egress.ipv4Addresses()) {
        if (prefix.contains(destination)) return prefix.address();
    }
    if (const auto primary = egress.primaryIpv4()) return primary->address();
    return std::nullopt;
}

bool Ipv4Stack::sendIpv4(DeviceContext& context, Ipv4Address destination, u8 protocol,
                         std::span<const u8> payload, FrameCategory category, std::string summary,
                         const Ipv4SendOptions& options) {
    Interface* egress = nullptr;
    Ipv4Address nextHop = destination;

    if (options.egress) {
        egress = owner_.findInterface(*options.egress);
    } else if (isLocalDestination(destination)) {
        egress = owner_.findInterfaceWithIpv4(destination);
    } else {
        const Route* route = routingTable_.lookup(destination);
        if (route == nullptr) {
            context.trace(TraceEvent{.kind = TraceKind::IpNoRouteToHost,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .summary = std::format("{} has no route to {}", ownerName(),
                                                            destination.toString())}
                              .with("destination-ip", destination.toString()));
            return false;
        }
        egress = owner_.findInterface(route->egressInterface);
        nextHop = route->nextHop.value_or(destination);

        context.trace(TraceEvent{.kind = TraceKind::RouteSelected,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = route->egressInterface,
                                 .summary = std::format("{} selected: {}", destination.toString(),
                                                        route->toString())}
                          .with("destination-ip", destination.toString())
                          .with("next-hop", nextHop.toString())
                          .with("route", route->destination.toNetworkString()));
    }

    if (egress == nullptr || !egress->isAdminUp()) {
        context.trace(TraceEvent{.kind = TraceKind::IpPacketDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .summary = std::format("no usable egress interface for {}",
                                                        destination.toString())});
        return false;
    }

    Ipv4Header header;
    header.identification = nextIpIdentification_++;
    header.ttl = options.ttl;
    header.protocol = protocol;
    header.destination = destination;

    if (options.source) {
        header.source = *options.source;
    } else if (const auto source = selectSourceAddress(*egress, destination)) {
        header.source = *source;
    } else {
        context.trace(TraceEvent{.kind = TraceKind::IpPacketDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = egress->id(),
                                 .summary = std::format("{} has no IPv4 address to send from",
                                                        egress->name())});
        return false;
    }

    ByteBuffer datagram = encodeIpv4(header, payload);

    context.trace(TraceEvent{.kind = TraceKind::IpPacketCreated,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = egress->id(),
                             .summary = std::format("{} -> {} ({}, TTL {}, {} bytes)",
                                                    header.source.toString(), destination.toString(),
                                                    ipProtocolName(protocol), header.ttl, datagram.size())}
                      .with("source-ip", header.source.toString())
                      .with("destination-ip", destination.toString())
                      .with("protocol", ipProtocolName(protocol))
                      .with("ttl", std::to_string(header.ttl))
                      .with("size", std::to_string(datagram.size())));

    if (isLocalDestination(destination)) {
        // Loopback: wrap it in an Ethernet header addressed to ourselves and
        // hand it back through the event queue.
        EthernetHeader ethernet;
        ethernet.destination = egress->macAddress();
        ethernet.source = egress->macAddress();
        ethernet.etherType = static_cast<u16>(EtherType::Ipv4);

        auto frame = context.makeFrame(owner_, encodeEthernet(ethernet, datagram), category,
                                       std::move(summary));
        context.loopback(owner_, *egress, std::move(frame));
        return true;
    }

    emitOnInterface(context, *egress, nextHop, std::move(datagram), category, std::move(summary));
    return true;
}

bool Ipv4Stack::sendUdp(DeviceContext& context, Ipv4Address destination, u16 sourcePort,
                        u16 destinationPort, std::span<const u8> payload, FrameCategory category,
                        std::string summary, const Ipv4SendOptions& options) {
    Ipv4Address source = options.source.value_or(Ipv4Address::any());
    if (!options.source) {
        // The UDP checksum covers the pseudo-header, so the source address must
        // be decided before the datagram is built.
        if (options.egress) {
            if (const Interface* iface = owner_.findInterface(*options.egress)) {
                if (const auto selected = selectSourceAddress(*iface, destination)) source = *selected;
            }
        } else if (const Route* route = routingTable_.lookup(destination)) {
            if (const Interface* iface = owner_.findInterface(route->egressInterface)) {
                if (const auto selected = selectSourceAddress(*iface, destination)) source = *selected;
            }
        }
    }

    UdpHeader header;
    header.sourcePort = sourcePort;
    header.destinationPort = destinationPort;

    const ByteBuffer datagram = encodeUdp(header, source, destination, payload);

    Ipv4SendOptions ipOptions = options;
    ipOptions.source = source;

    const bool sent = sendIpv4(context, destination, static_cast<u8>(IpProtocol::Udp), datagram,
                               category, summary, ipOptions);
    if (sent) {
        context.trace(TraceEvent{.kind = TraceKind::UdpDatagramSent,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .summary = std::format("UDP {}:{} -> {}:{} ({} bytes)",
                                                        source.toString(), sourcePort,
                                                        destination.toString(), destinationPort,
                                                        payload.size())}
                          .with("source-port", std::to_string(sourcePort))
                          .with("destination-port", std::to_string(destinationPort)));
    }
    return sent;
}

void Ipv4Stack::emitOnInterface(DeviceContext& context, Interface& egress, Ipv4Address nextHop,
                                ByteBuffer ipPacket, FrameCategory category, std::string summary,
                                std::optional<FrameIdentity> inherited) {
    if (nextHop.isLimitedBroadcast() || isBroadcastFor(egress, nextHop)) {
        transmitIpv4Frame(context, egress, MacAddress::broadcast(), ipPacket, category,
                          std::move(summary), inherited);
        return;
    }

    if (const ArpEntry* entry = arpCache_.find(nextHop, context.now())) {
        context.trace(TraceEvent{.kind = TraceKind::ArpCacheHit,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = egress.id(),
                                 .summary = std::format("{} is at {} (cached)", nextHop.toString(),
                                                        entry->mac.toString())}
                          .with("address", nextHop.toString())
                          .with("mac", entry->mac.toString()));

        transmitIpv4Frame(context, egress, entry->mac, ipPacket, category, std::move(summary), inherited);
        return;
    }

    context.trace(TraceEvent{.kind = TraceKind::ArpCacheMiss,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = egress.id(),
                             .summary = std::format("{} needs the MAC address of {}", ownerName(),
                                                    nextHop.toString())}
                      .with("address", nextHop.toString()));

    QueuedPacket queued;
    queued.ipPacket = std::move(ipPacket);
    queued.category = category;
    queued.summary = std::move(summary);
    queued.inheritedIdentity = inherited;

    startArpResolution(context, egress, nextHop, std::move(queued));
}

void Ipv4Stack::transmitIpv4Frame(DeviceContext& context, Interface& egress, MacAddress destinationMac,
                                  const ByteBuffer& ipPacket, FrameCategory category,
                                  std::string summary, const std::optional<FrameIdentity>& inherited) {
    EthernetHeader header;
    header.destination = destinationMac;
    header.source = egress.macAddress();
    header.etherType = static_cast<u16>(EtherType::Ipv4);

    ByteBuffer bytes = encodeEthernet(header, ipPacket);

    Frame frame = inherited
                      ? context.makeForwardedFrame(*inherited, std::move(bytes), category, summary)
                      : context.makeFrame(owner_, std::move(bytes), category, std::move(summary));

    context.transmit(owner_, egress, std::move(frame));
}

void Ipv4Stack::startArpResolution(DeviceContext& context, Interface& egress, Ipv4Address target,
                                   QueuedPacket packet) {
    PendingArp& pending = pendingArp_[target];
    const bool isNew = pending.attempts == 0;

    if (isNew) {
        pending.target = target;
        pending.egress = egress.id();
    }

    if (pending.queue.size() >= kMaxQueuedPacketsPerTarget) {
        context.trace(TraceEvent{.kind = TraceKind::IpPacketDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = egress.id(),
                                 .summary = std::format("ARP queue for {} is full; oldest packet dropped",
                                                        target.toString())});
        pending.queue.erase(pending.queue.begin());
    }
    pending.queue.push_back(std::move(packet));

    if (!isNew) return;

    sendArpRequest(context, egress, target);
    pending.attempts = 1;
    pending.timer = armTimer(context, TimerPurpose{TimerKind::ArpRetry, target.value(), 0},
                             kArpRetryInterval);
}

void Ipv4Stack::sendArpRequest(DeviceContext& context, Interface& egress, Ipv4Address target) {
    const auto source = selectSourceAddress(egress, target);

    const ArpMessage request = makeArpRequest(egress.macAddress(),
                                              source.value_or(Ipv4Address::any()), target);

    EthernetHeader header;
    header.destination = MacAddress::broadcast();
    header.source = egress.macAddress();
    header.etherType = static_cast<u16>(EtherType::Arp);

    auto frame = context.makeFrame(owner_, encodeEthernet(header, encodeArp(request)),
                                   FrameCategory::Arp,
                                   std::format("ARP request who has {}", target.toString()));

    context.trace(TraceEvent{.kind = TraceKind::ArpRequestSent,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = egress.id(),
                             .packet = frame.id,
                             .summary = std::format("who has {}? tell {}", target.toString(),
                                                    request.senderIp.toString())}
                      .with("target-ip", target.toString())
                      .with("sender-ip", request.senderIp.toString())
                      .with("sender-mac", request.senderMac.toString()));

    context.transmit(owner_, egress, std::move(frame));
}

void Ipv4Stack::sendIcmpError(DeviceContext& context, IcmpType type, u8 code,
                              const Ipv4PacketView& original) {
    // RFC 1122 section 3.2.2: never generate an error for an ICMP error, for a
    // broadcast or multicast destination, or for a non-initial fragment.
    if (original.header.destination.isLimitedBroadcast() ||
        original.header.destination.isMulticast() ||
        original.header.source.isUnspecified() ||
        original.header.fragmentOffset != 0) {
        return;
    }

    if (static_cast<IpProtocol>(original.header.protocol) == IpProtocol::Icmp) {
        const auto icmp = decodeIcmp(original.payload);
        if (icmp && icmp->isError()) return;
    }

    const ByteBuffer message = encodeIcmpError(type, code, original.datagram);
    const std::string reason = icmpCodeName(static_cast<u8>(type), code);

    const bool sent = sendIpv4(context, original.header.source, static_cast<u8>(IpProtocol::Icmp),
                               message, FrameCategory::IcmpError,
                               std::format("ICMP {}: {}", icmpTypeName(static_cast<u8>(type)), reason));
    if (!sent) return;

    const TraceKind kind = (type == IcmpType::TimeExceeded) ? TraceKind::IcmpTimeExceededSent
                                                            : TraceKind::IcmpDestinationUnreachableSent;

    context.trace(TraceEvent{.kind = kind,
                             .time = context.now(),
                             .device = owner_.id(),
                             .summary = std::format("{} reported \"{}\" to {}", ownerName(), reason,
                                                    original.header.source.toString())}
                      .with("destination-ip", original.header.source.toString())
                      .with("reason", reason)
                      .with("icmp-type", std::to_string(static_cast<u8>(type)))
                      .with("icmp-code", std::to_string(code)));
}

// ---------------------------------------------------------------------------
// Timers
// ---------------------------------------------------------------------------

void Ipv4Stack::onTimer(DeviceContext& context, TimerId timer) {
    const auto entry = timers_.find(timer);
    if (entry == timers_.end()) return;

    const TimerPurpose purpose = entry->second;
    timers_.erase(entry);

    switch (purpose.kind) {
        case TimerKind::ArpSweep: {
            for (const Ipv4Address address : arpCache_.removeExpired(context.now())) {
                context.trace(TraceEvent{.kind = TraceKind::ArpEntryExpired,
                                         .time = context.now(),
                                         .device = owner_.id(),
                                         .summary = std::format("ARP entry for {} expired",
                                                                address.toString())}
                                  .with("address", address.toString()));
            }
            scheduleArpSweep(context);
            return;
        }

        case TimerKind::ArpRetry: {
            const Ipv4Address target{purpose.primary};
            const auto pending = pendingArp_.find(target);
            if (pending == pendingArp_.end()) return;

            Interface* egress = owner_.findInterface(pending->second.egress);

            if (pending->second.attempts < kMaxArpAttempts && egress != nullptr) {
                sendArpRequest(context, *egress, target);
                ++pending->second.attempts;
                pending->second.timer = armTimer(context, purpose, kArpRetryInterval);
                return;
            }

            context.trace(TraceEvent{.kind = TraceKind::ArpTimedOut,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .summary = std::format("{} did not answer ARP after {} attempts; "
                                                            "{} queued packet(s) dropped",
                                                            target.toString(), kMaxArpAttempts,
                                                            pending->second.queue.size())}
                              .with("address", target.toString())
                              .with("attempts", std::to_string(kMaxArpAttempts))
                              .with("dropped", std::to_string(pending->second.queue.size())));

            pendingArp_.erase(pending);
            return;
        }

        case TimerKind::PingSend: {
            const auto session = pings_.find(static_cast<u16>(purpose.primary));
            if (session == pings_.end()) return;
            sendProbe(context, session->second);
            return;
        }

        case TimerKind::PingTimeout: {
            const auto session = pings_.find(static_cast<u16>(purpose.primary));
            if (session == pings_.end()) return;

            const auto sequence = static_cast<u16>(purpose.secondary);
            const auto outstanding = session->second.outstanding.find(sequence);
            if (outstanding == session->second.outstanding.end()) return; // already answered

            session->second.outstanding.erase(outstanding);
            ++session->second.statistics.lost;

            context.trace(TraceEvent{.kind = TraceKind::PingTimedOut,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .summary = std::format("request timed out (seq {})", sequence)}
                              .with("sequence", std::to_string(sequence))
                              .with("reason", "no reply within the timeout")
                              .with("ping-id", std::to_string(session->second.id)));

            finishPing(context, session->second);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Ping
// ---------------------------------------------------------------------------

Result<PingId> Ipv4Stack::startPing(DeviceContext& context, const PingRequest& request) {
    if (!request.destination.isAssignableToHost() && !request.destination.isLimitedBroadcast()) {
        return Result<PingId>::failure(std::format("{} is not a pingable address",
                                                   request.destination.toString()));
    }
    if (request.count == 0) {
        return Result<PingId>::failure("the probe count must be at least 1");
    }
    if (!isLocalDestination(request.destination) && routingTable_.lookup(request.destination) == nullptr) {
        return Result<PingId>::failure(std::format("{} has no route to {}", ownerName(),
                                                   request.destination.toString()));
    }

    PingSession session;
    session.id = nextPingId_++;
    if (nextPingId_ == 0) nextPingId_ = 1; // identifier 0 is reserved as "none"
    session.request = request;

    const auto inserted = pings_.emplace(session.id, std::move(session));

    context.trace(TraceEvent{.kind = TraceKind::PingStarted,
                             .time = context.now(),
                             .device = owner_.id(),
                             .summary = std::format("{} is pinging {} with {} bytes of data, {} time(s)",
                                                    ownerName(), request.destination.toString(),
                                                    request.payloadSize, request.count)}
                      .with("destination-ip", request.destination.toString())
                      .with("count", std::to_string(request.count))
                      .with("payload-size", std::to_string(request.payloadSize))
                      .with("ping-id", std::to_string(inserted.first->second.id)));

    sendProbe(context, inserted.first->second);
    return inserted.first->second.id;
}

void Ipv4Stack::sendProbe(DeviceContext& context, PingSession& session) {
    const u16 sequence = session.nextSequence++;

    const ByteBuffer payload = makeEchoPayload(session.request.payloadSize);
    const ByteBuffer message = encodeIcmpEcho(IcmpType::EchoRequest, session.id, sequence, payload);

    // Recorded before sending: a loopback ping can be answered from inside the
    // very next event, and the reply must find its outstanding entry.
    session.outstanding[sequence] = context.now();
    ++session.statistics.sent;

    Ipv4SendOptions options;
    options.ttl = session.request.ttl;

    const bool sent = sendIpv4(context, session.request.destination, static_cast<u8>(IpProtocol::Icmp),
                               message, FrameCategory::Icmp,
                               std::format("ICMP echo request id={} seq={}", session.id, sequence),
                               options);

    if (sent) {
        context.trace(TraceEvent{.kind = TraceKind::IcmpEchoRequestSent,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .summary = std::format("echo request to {} (id {}, seq {})",
                                                        session.request.destination.toString(),
                                                        session.id, sequence)}
                          .with("destination-ip", session.request.destination.toString())
                          .with("identifier", std::to_string(session.id))
                          .with("sequence", std::to_string(sequence)));

        armTimer(context, TimerPurpose{TimerKind::PingTimeout, session.id, sequence},
                 session.request.timeout);
    } else {
        session.outstanding.erase(sequence);
        ++session.statistics.lost;
    }

    if (session.statistics.sent < session.request.count) {
        session.sendTimer = armTimer(context, TimerPurpose{TimerKind::PingSend, session.id, 0},
                                     session.request.interval);
    } else {
        finishPing(context, session);
    }
}

void Ipv4Stack::finishPing(DeviceContext& context, PingSession& session) {
    if (session.statistics.finished) return;
    if (session.statistics.sent < session.request.count) return;
    if (!session.outstanding.empty()) return;

    session.statistics.finished = true;

    const PingStatistics& statistics = session.statistics;
    const u32 lossPercent = statistics.sent == 0
                                ? 0
                                : (statistics.lost * 100) / statistics.sent;

    context.trace(TraceEvent{.kind = TraceKind::PingFinished,
                             .time = context.now(),
                             .device = owner_.id(),
                             .summary = std::format("{} sent, {} received, {}% loss{}",
                                                    statistics.sent, statistics.received, lossPercent,
                                                    statistics.received > 0
                                                        ? std::format(", average {}",
                                                                      formatDuration(statistics.averageRtt()))
                                                        : std::string{})}
                      .with("ping-id", std::to_string(session.id))
                      .with("destination-ip", session.request.destination.toString())
                      .with("sent", std::to_string(statistics.sent))
                      .with("received", std::to_string(statistics.received))
                      .with("lost", std::to_string(statistics.lost))
                      .with("loss-percent", std::to_string(lossPercent))
                      .with("average-rtt-ns", std::to_string(statistics.averageRtt().count())));
}

const PingStatistics* Ipv4Stack::pingStatistics(PingId id) const {
    const auto session = pings_.find(id);
    return session == pings_.end() ? nullptr : &session->second.statistics;
}

bool Ipv4Stack::hasActivePing() const {
    return std::any_of(pings_.begin(), pings_.end(), [](const auto& entry) {
        return !entry.second.statistics.finished;
    });
}

void Ipv4Stack::cancelPing(DeviceContext& context, PingId id) {
    const auto session = pings_.find(id);
    if (session == pings_.end()) return;

    context.cancelTimer(owner_, session->second.sendTimer);
    session->second.statistics.finished = true;
    session->second.outstanding.clear();
}

} // namespace tnp::core
