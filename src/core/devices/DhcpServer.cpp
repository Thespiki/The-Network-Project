#include "core/devices/DhcpServer.h"

#include "core/network/Device.h"

#include <algorithm>
#include <format>

namespace tnp::core {

using namespace proto;

bool DhcpPool::isValid() const {
    if (subnet.prefixLength() == 0 || subnet.prefixLength() > 30) return false;
    if (!subnet.contains(rangeFirst) || !subnet.contains(rangeLast)) return false;
    if (rangeFirst > rangeLast) return false;
    return subnet.isUsableHostAddress(rangeFirst) && subnet.isUsableHostAddress(rangeLast);
}

bool DhcpPool::covers(Ipv4Address address) const {
    return address >= rangeFirst && address <= rangeLast;
}

DhcpServer::DhcpServer(Device& owner, Ipv4Stack& stack) : owner_(owner), stack_(stack) {}

void DhcpServer::addPool(DhcpPool pool) {
    if (!pool.id.isValid()) pool.id = DhcpPoolId::generate();
    pools_.push_back(std::move(pool));
}

bool DhcpServer::removePool(DhcpPoolId id) {
    const auto it = std::find_if(pools_.begin(), pools_.end(),
                                 [id](const DhcpPool& pool) { return pool.id == id; });
    if (it == pools_.end()) return false;
    pools_.erase(it);
    return true;
}

void DhcpServer::setPools(std::vector<DhcpPool> pools) {
    pools_ = std::move(pools);
    for (auto& pool : pools_) {
        if (!pool.id.isValid()) pool.id = DhcpPoolId::generate();
    }
}

std::vector<DhcpLease> DhcpServer::leases() const {
    std::vector<DhcpLease> result;
    result.reserve(leases_.size());
    for (const auto& [client, lease] : leases_) result.push_back(lease);
    return result;
}

void DhcpServer::onReset() { leases_.clear(); }

const DhcpPool* DhcpServer::selectPool(const Interface& ingress) const {
    // The pool that matches the subnet the request arrived on. Relay agents are
    // not simulated, so the client is always on a directly attached subnet.
    for (const DhcpPool& pool : pools_) {
        for (const Ipv4Prefix& prefix : ingress.ipv4Addresses()) {
            if (pool.subnet.network() == prefix.network()) return &pool;
        }
    }
    return nullptr;
}

std::optional<Ipv4Address> DhcpServer::serverAddressIn(const Interface& ingress,
                                                       const DhcpPool& pool) const {
    for (const Ipv4Prefix& prefix : ingress.ipv4Addresses()) {
        if (pool.subnet.contains(prefix.address())) return prefix.address();
    }
    return std::nullopt;
}

bool DhcpServer::isAddressAvailable(Ipv4Address address, MacAddress client, SimTime now) const {
    if (owner_.ownsIpv4Address(address)) return false;
    for (const auto& [mac, lease] : leases_) {
        if (lease.address != address) continue;
        if (mac == client) return true;          // the client's own current lease
        if (lease.isExpired(now)) return true;   // reclaimable
        return false;
    }
    return true;
}

std::optional<Ipv4Address> DhcpServer::allocate(const DhcpPool& pool, MacAddress client, SimTime now) {
    // Honour an existing binding first: a client that comes back should get the
    // same address, which is what makes a simulated network reproducible.
    const auto existing = leases_.find(client);
    if (existing != leases_.end() && pool.covers(existing->second.address)) {
        return existing->second.address;
    }

    for (u32 value = pool.rangeFirst.value(); value <= pool.rangeLast.value(); ++value) {
        const Ipv4Address candidate{value};
        if (!pool.subnet.isUsableHostAddress(candidate)) continue;
        if (std::find(pool.exclusions.begin(), pool.exclusions.end(), candidate) != pool.exclusions.end()) {
            continue;
        }
        if (pool.gateway && *pool.gateway == candidate) continue;
        if (!isAddressAvailable(candidate, client, now)) continue;
        return candidate;
    }
    return std::nullopt;
}

void DhcpServer::sendReply(DeviceContext& context, Interface& egress, const DhcpMessage& reply,
                           Ipv4Address serverAddress, TraceKind kind, const std::string& description) {
    Ipv4SendOptions options;
    options.egress = egress.id();
    options.source = serverAddress;

    // Replies go to the limited broadcast address: the client has no address
    // yet, so it cannot be reached by unicast.
    stack_.sendUdp(context, Ipv4Address::limitedBroadcast(), kPortDhcpServer, kPortDhcpClient,
                   encodeDhcp(reply), FrameCategory::Dhcp, description, options);

    context.trace(TraceEvent{.kind = kind,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = egress.id(),
                             .summary = description}
                      .with("client-mac", reply.clientMac.toString())
                      .with("offered-ip", reply.yourAddress.toString())
                      .with("server-ip", serverAddress.toString()));
}

void DhcpServer::handleDatagram(DeviceContext& context, Interface& ingress, const Ipv4Header& ip,
                                const UdpHeader& udp, std::span<const u8> payload) {
    (void)ip;
    (void)udp;
    if (!enabled_) return;

    const auto request = decodeDhcp(payload);
    if (!request) return;
    if (request->operation != 1) return; // only client requests are served

    const DhcpPool* pool = selectPool(ingress);
    if (pool == nullptr || !pool->isValid()) {
        context.trace(TraceEvent{.kind = TraceKind::DhcpNoAddressAvailable,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .summary = std::format("no valid DHCP pool serves {}", ingress.name())});
        return;
    }

    const auto serverAddress = serverAddressIn(ingress, *pool);
    if (!serverAddress) return;

    DhcpMessage reply;
    reply.operation = 2;
    reply.transactionId = request->transactionId;
    reply.clientMac = request->clientMac;
    reply.serverAddress = *serverAddress;
    reply.serverIdentifier = *serverAddress;
    reply.subnetMask = pool->subnet.netmask();
    reply.router = pool->gateway;
    reply.domainNameServer = pool->dnsServer;
    reply.leaseTimeSeconds = static_cast<u32>(
        std::chrono::duration_cast<std::chrono::seconds>(pool->leaseTime).count());

    if (request->messageType == DhcpMessageType::Discover) {
        const auto offered = allocate(*pool, request->clientMac, context.now());
        if (!offered) {
            context.trace(TraceEvent{.kind = TraceKind::DhcpNoAddressAvailable,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .interface = ingress.id(),
                                     .summary = std::format("pool {} is exhausted", pool->name)}
                              .with("pool", pool->name));
            return;
        }

        reply.messageType = DhcpMessageType::Offer;
        reply.yourAddress = *offered;
        sendReply(context, ingress, reply, *serverAddress, TraceKind::DhcpOfferSent,
                  std::format("DHCPOFFER {} to {}", offered->toString(), request->clientMac.toString()));
        return;
    }

    if (request->messageType == DhcpMessageType::Request) {
        const Ipv4Address wanted = request->requestedAddress.value_or(request->clientAddress);

        // A REQUEST naming another server's identifier is not ours to answer.
        if (request->serverIdentifier && *request->serverIdentifier != *serverAddress) return;

        const bool acceptable = pool->covers(wanted) &&
                                isAddressAvailable(wanted, request->clientMac, context.now());

        if (!acceptable) {
            reply.messageType = DhcpMessageType::Nak;
            reply.yourAddress = Ipv4Address::any();
            sendReply(context, ingress, reply, *serverAddress, TraceKind::DhcpNakSent,
                      std::format("DHCPNAK to {}: {} is not available",
                                  request->clientMac.toString(), wanted.toString()));
            return;
        }

        DhcpLease lease;
        lease.address = wanted;
        lease.client = request->clientMac;
        lease.pool = pool->id;
        lease.issuedAt = context.now();
        lease.expiresAt = context.now() + pool->leaseTime;
        leases_[request->clientMac] = lease;

        reply.messageType = DhcpMessageType::Ack;
        reply.yourAddress = wanted;
        sendReply(context, ingress, reply, *serverAddress, TraceKind::DhcpAckSent,
                  std::format("DHCPACK {} to {}", wanted.toString(), request->clientMac.toString()));

        context.trace(TraceEvent{.kind = TraceKind::DhcpLeaseAssigned,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .summary = std::format("{} leased to {} for {}", wanted.toString(),
                                                        request->clientMac.toString(),
                                                        formatDuration(pool->leaseTime))}
                          .with("address", wanted.toString())
                          .with("client-mac", request->clientMac.toString())
                          .with("pool", pool->name));
        return;
    }

    if (request->messageType == DhcpMessageType::Release) {
        leases_.erase(request->clientMac);
    }
}

} // namespace tnp::core
