#include "core/devices/DhcpClient.h"

#include "core/network/Device.h"

#include <format>

namespace tnp::core {

using namespace proto;

namespace {

constexpr int kMaxDhcpAttempts = 4;
constexpr Duration kDhcpRetryInterval = seconds(4);

} // namespace

std::string_view dhcpClientStateName(DhcpClientState state) {
    switch (state) {
        case DhcpClientState::Idle:       return "idle";
        case DhcpClientState::Selecting:  return "selecting";
        case DhcpClientState::Requesting: return "requesting";
        case DhcpClientState::Bound:      return "bound";
        case DhcpClientState::Failed:     return "failed";
    }
    return "idle";
}

DhcpClient::DhcpClient(Device& owner, Ipv4Stack& stack) : owner_(owner), stack_(stack) {}

void DhcpClient::onPowerOn(DeviceContext& context) {
    for (const auto& iface : owner_.interfaces()) {
        if (!iface->ipv4DhcpEnabled() || !iface->isAdminUp()) continue;

        Session& session = sessions_[iface->id()];
        session.interface = iface->id();
        startDiscovery(context, *iface, session);
    }
}

void DhcpClient::onReset() {
    for (auto& [interfaceId, session] : sessions_) {
        if (!session.installedAddress) continue;
        if (Interface* iface = owner_.findInterface(interfaceId)) {
            iface->removeIpv4Address(*session.installedAddress);
        }
    }
    sessions_.clear();
    timers_.clear();
}

void DhcpClient::armRetry(DeviceContext& context, Session& session) {
    const TimerId timer = context.nextTimerId();
    timers_[timer] = session.interface;
    session.retryTimer = timer;
    context.scheduleTimer(owner_, timer, kDhcpRetryInterval);
}

void DhcpClient::startDiscovery(DeviceContext& context, Interface& iface, Session& session) {
    session.state = DhcpClientState::Selecting;
    session.transactionId = nextTransactionId_++;
    ++session.attempts;

    DhcpMessage discover;
    discover.operation = 1;
    discover.transactionId = session.transactionId;
    discover.clientMac = iface.macAddress();
    discover.messageType = DhcpMessageType::Discover;

    Ipv4SendOptions options;
    options.egress = iface.id();
    options.source = Ipv4Address::any(); // no address yet, by definition

    stack_.sendUdp(context, Ipv4Address::limitedBroadcast(), kPortDhcpClient, kPortDhcpServer,
                   encodeDhcp(discover), FrameCategory::Dhcp, "DHCPDISCOVER", options);

    context.trace(TraceEvent{.kind = TraceKind::DhcpDiscoverSent,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = iface.id(),
                             .summary = std::format("{} is looking for a DHCP server on {}",
                                                    owner_.name(), iface.name())}
                      .with("client-mac", iface.macAddress().toString())
                      .with("attempt", std::to_string(session.attempts)));

    armRetry(context, session);
}

void DhcpClient::sendRequest(DeviceContext& context, Interface& iface, Session& session) {
    session.state = DhcpClientState::Requesting;

    DhcpMessage request;
    request.operation = 1;
    request.transactionId = session.transactionId;
    request.clientMac = iface.macAddress();
    request.messageType = DhcpMessageType::Request;
    request.requestedAddress = session.offeredAddress;
    request.serverIdentifier = session.serverIdentifier;

    Ipv4SendOptions options;
    options.egress = iface.id();
    options.source = Ipv4Address::any();

    stack_.sendUdp(context, Ipv4Address::limitedBroadcast(), kPortDhcpClient, kPortDhcpServer,
                   encodeDhcp(request), FrameCategory::Dhcp, "DHCPREQUEST", options);

    context.trace(TraceEvent{.kind = TraceKind::DhcpRequestSent,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = iface.id(),
                             .summary = std::format("{} requests {}", owner_.name(),
                                                    session.offeredAddress.toString())}
                      .with("requested-ip", session.offeredAddress.toString())
                      .with("server-ip", session.serverIdentifier.toString()));

    armRetry(context, session);
}

void DhcpClient::applyBinding(DeviceContext& context, Interface& iface, Session& session,
                              const DhcpMessage& ack) {
    const auto prefixLength = ack.subnetMask
                                  ? Ipv4Prefix::prefixLengthForMask(*ack.subnetMask)
                                  : std::optional<u8>{24};
    if (!prefixLength) return;

    const Ipv4Prefix assigned{ack.yourAddress, *prefixLength};

    // Replace any address a previous lease installed on this interface.
    if (session.installedAddress) iface.removeIpv4Address(*session.installedAddress);
    if (!iface.addIpv4Address(assigned)) return;
    session.installedAddress = assigned;

    DhcpBinding binding;
    binding.interface = iface.id();
    binding.address = assigned;
    binding.gateway = ack.router;
    binding.dnsServer = ack.domainNameServer;
    binding.serverAddress = ack.serverIdentifier.value_or(ack.serverAddress);
    binding.acquiredAt = context.now();
    binding.expiresAt = context.now() + seconds(ack.leaseTimeSeconds.value_or(86400));

    session.binding = binding;
    session.state = DhcpClientState::Bound;

    if (ack.router) stack_.setDefaultGateway(*ack.router);
    if (ack.domainNameServer) stack_.setDnsServers({*ack.domainNameServer});
    stack_.refreshRoutes();

    context.trace(TraceEvent{.kind = TraceKind::DhcpLeaseAssigned,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = iface.id(),
                             .summary = std::format("{} configured {} on {} from {}", owner_.name(),
                                                    assigned.toString(), iface.name(),
                                                    binding.serverAddress.toString())}
                      .with("address", assigned.toString())
                      .with("gateway", ack.router ? ack.router->toString() : std::string{"none"})
                      .with("dns", ack.domainNameServer ? ack.domainNameServer->toString()
                                                        : std::string{"none"})
                      .with("server-ip", binding.serverAddress.toString()));
}

void DhcpClient::handleDatagram(DeviceContext& context, Interface& ingress, const Ipv4Header& ip,
                                const UdpHeader& udp, std::span<const u8> payload) {
    (void)ip;
    (void)udp;

    const auto message = decodeDhcp(payload);
    if (!message || message->operation != 2) return;

    const auto entry = sessions_.find(ingress.id());
    if (entry == sessions_.end()) return;

    Session& session = entry->second;
    if (message->transactionId != session.transactionId) return;
    if (message->clientMac != ingress.macAddress()) return;

    context.cancelTimer(owner_, session.retryTimer);
    timers_.erase(session.retryTimer);

    switch (message->messageType) {
        case DhcpMessageType::Offer: {
            if (session.state != DhcpClientState::Selecting) return;
            session.offeredAddress = message->yourAddress;
            session.serverIdentifier = message->serverIdentifier.value_or(message->serverAddress);

            context.trace(TraceEvent{.kind = TraceKind::DhcpOfferSent,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .interface = ingress.id(),
                                     .summary = std::format("offer of {} received from {}",
                                                            session.offeredAddress.toString(),
                                                            session.serverIdentifier.toString())}
                              .with("offered-ip", session.offeredAddress.toString())
                              .with("server-ip", session.serverIdentifier.toString()));

            sendRequest(context, ingress, session);
            return;
        }
        case DhcpMessageType::Ack:
            applyBinding(context, ingress, session, *message);
            return;
        case DhcpMessageType::Nak:
            session.state = DhcpClientState::Selecting;
            startDiscovery(context, ingress, session);
            return;
        default:
            return;
    }
}

bool DhcpClient::onTimer(DeviceContext& context, TimerId timer) {
    const auto entry = timers_.find(timer);
    if (entry == timers_.end()) return false;

    const InterfaceId interfaceId = entry->second;
    timers_.erase(entry);

    const auto sessionEntry = sessions_.find(interfaceId);
    if (sessionEntry == sessions_.end()) return true;

    Session& session = sessionEntry->second;
    if (session.state == DhcpClientState::Bound) return true;

    Interface* iface = owner_.findInterface(interfaceId);
    if (iface == nullptr) return true;

    if (session.attempts >= kMaxDhcpAttempts) {
        session.state = DhcpClientState::Failed;
        context.trace(TraceEvent{.kind = TraceKind::DhcpNoAddressAvailable,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = interfaceId,
                                 .summary = std::format("{} found no DHCP server on {} after {} attempts",
                                                        owner_.name(), iface->name(), session.attempts)}
                          .with("attempts", std::to_string(session.attempts)));
        return true;
    }

    // Retry from the top: a lost OFFER and a lost ACK are both recovered by
    // starting the exchange again, which is what a real client does.
    startDiscovery(context, *iface, session);
    return true;
}

std::vector<DhcpBinding> DhcpClient::bindings() const {
    std::vector<DhcpBinding> result;
    for (const auto& [interfaceId, session] : sessions_) {
        if (session.binding) result.push_back(*session.binding);
    }
    return result;
}

DhcpClientState DhcpClient::stateOf(InterfaceId interface) const {
    const auto entry = sessions_.find(interface);
    return entry == sessions_.end() ? DhcpClientState::Idle : entry->second.state;
}

} // namespace tnp::core
