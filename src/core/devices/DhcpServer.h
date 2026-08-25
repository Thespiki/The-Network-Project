#pragma once

#include "core/devices/Ipv4Stack.h"
#include "core/network/Ids.h"
#include "core/network/MacAddress.h"
#include "core/network/Subnet.h"
#include "core/protocols/Dhcp.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tnp::core {

class Device;

/// A range of addresses the server may hand out, plus the options it sends
/// with them.
struct DhcpPool {
    DhcpPoolId id;
    std::string name = "pool";
    Ipv4Prefix subnet;
    Ipv4Address rangeFirst;
    Ipv4Address rangeLast;

    std::optional<Ipv4Address> gateway;
    std::optional<Ipv4Address> dnsServer;
    std::string domainName;

    Duration leaseTime = std::chrono::duration_cast<Duration>(std::chrono::hours{24});

    /// Addresses inside the range that must never be allocated.
    std::vector<Ipv4Address> exclusions;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool covers(Ipv4Address address) const;
};

/// One address currently allocated to a client.
struct DhcpLease {
    Ipv4Address address;
    MacAddress client;
    DhcpPoolId pool;
    SimTime issuedAt{};
    SimTime expiresAt{};

    [[nodiscard]] bool isExpired(SimTime now) const { return now >= expiresAt; }
};

/// The DHCP service of a server or router.
///
/// Speaks the real four-message exchange over the real wire format: a client's
/// DISCOVER is decoded from bytes, and the OFFER, REQUEST and ACK that follow
/// are visible in the packet inspector like any other traffic.
class DhcpServer {
public:
    DhcpServer(Device& owner, Ipv4Stack& stack);

    DhcpServer(const DhcpServer&) = delete;
    DhcpServer& operator=(const DhcpServer&) = delete;

    [[nodiscard]] bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    [[nodiscard]] const std::vector<DhcpPool>& pools() const { return pools_; }
    void addPool(DhcpPool pool);
    bool removePool(DhcpPoolId id);
    void setPools(std::vector<DhcpPool> pools);

    /// Currently allocated leases, ordered by client MAC address.
    [[nodiscard]] std::vector<DhcpLease> leases() const;

    void onReset();

    /// Bound to UDP port 67 by the owning device.
    void handleDatagram(DeviceContext& context, Interface& ingress, const proto::Ipv4Header& ip,
                        const proto::UdpHeader& udp, std::span<const u8> payload);

private:
    /// The pool serving the subnet the request arrived on.
    [[nodiscard]] const DhcpPool* selectPool(const Interface& ingress) const;

    /// The server's own address inside `pool`, used as the server identifier.
    [[nodiscard]] std::optional<Ipv4Address> serverAddressIn(const Interface& ingress,
                                                             const DhcpPool& pool) const;

    [[nodiscard]] std::optional<Ipv4Address> allocate(const DhcpPool& pool, MacAddress client, SimTime now);
    [[nodiscard]] bool isAddressAvailable(Ipv4Address address, MacAddress client, SimTime now) const;

    void sendReply(DeviceContext& context, Interface& egress, const proto::DhcpMessage& reply,
                   Ipv4Address serverAddress, TraceKind kind, const std::string& description);

    Device& owner_;
    Ipv4Stack& stack_;
    bool enabled_ = false;
    std::vector<DhcpPool> pools_;
    std::map<MacAddress, DhcpLease> leases_;
};

} // namespace tnp::core
