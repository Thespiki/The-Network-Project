#pragma once

#include "core/devices/ArpCache.h"
#include "core/network/DeviceContext.h"
#include "core/network/Frame.h"
#include "core/network/Interface.h"
#include "core/protocols/Ethernet.h"
#include "core/protocols/Icmp.h"
#include "core/protocols/Ipv4.h"
#include "core/protocols/Udp.h"
#include "core/routing/Ospf.h"
#include "core/routing/RoutingTable.h"
#include "core/routing/StaticRouting.h"
#include "utilities/Result.h"

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tnp::core {

class Device;

/// Parameters of a ping run.
struct PingRequest {
    Ipv4Address destination;
    u32 count = 4;
    Duration interval = seconds(1);
    Duration timeout = seconds(2);
    std::size_t payloadSize = 32;
    u8 ttl = proto::kDefaultTtl;
};

/// Live statistics of a ping run, readable while it is in progress.
struct PingStatistics {
    u32 sent = 0;
    u32 received = 0;
    u32 lost = 0;
    Duration minimumRtt = Duration::max();
    Duration maximumRtt = Duration::zero();
    Duration totalRtt = Duration::zero();
    bool finished = false;

    [[nodiscard]] Duration averageRtt() const {
        return received == 0 ? Duration::zero() : totalRtt / received;
    }
};

/// Identifies one ping run on a device. The value is also used as the ICMP echo
/// identifier, which is how replies are matched back to their run.
using PingId = u16;

/// Optional overrides for an outgoing packet.
struct Ipv4SendOptions {
    u8 ttl = proto::kDefaultTtl;

    /// Source address to stamp on the packet. Without it the address of the
    /// egress interface is used. A DHCP client needs this: it must send from
    /// 0.0.0.0 before it owns an address.
    std::optional<Ipv4Address> source;

    /// Sends out a specific interface instead of consulting the routing table.
    /// Used for link-local traffic such as DHCP discovery.
    std::optional<InterfaceId> egress;
};

/// Maximum number of packets held while waiting for one ARP resolution. Beyond
/// this the oldest are dropped, as a real host's queue does.
inline constexpr std::size_t kMaxQueuedPacketsPerTarget = 4;

/// How many ARP requests are sent before a resolution is abandoned.
inline constexpr int kMaxArpAttempts = 3;
inline constexpr Duration kArpRetryInterval = seconds(1);

/// The IPv4 protocol machinery of a device: ARP, forwarding, ICMP and a UDP
/// dispatch point.
///
/// Composed into `Pc`, `Server`, `Router`, `Layer3Switch`, `Firewall` and
/// `Cloud` rather than inherited, which is why routing behaviour is identical
/// everywhere it appears and why a firewall is "a router plus a filter" instead
/// of a parallel implementation.
///
/// The stack never touches the topology. It asks its `DeviceContext` to put a
/// frame on an interface and to wake it up later; everything else - links,
/// queueing, time - belongs to the simulator.
class Ipv4Stack {
public:
    /// Handles a datagram delivered to a bound UDP port.
    using UdpHandler = std::function<void(DeviceContext&, Interface&, const proto::Ipv4Header&,
                                          const proto::UdpHeader&, std::span<const u8>)>;

    /// Consulted before a packet is forwarded between interfaces. Returning
    /// false drops the packet; the caller reports why.
    using ForwardingFilter = std::function<bool(DeviceContext&, Interface& ingress, Interface& egress,
                                                const proto::Ipv4Header&, std::span<const u8>)>;

    explicit Ipv4Stack(Device& owner);

    Ipv4Stack(const Ipv4Stack&) = delete;
    Ipv4Stack& operator=(const Ipv4Stack&) = delete;

    // --- Configuration -----------------------------------------------------
    [[nodiscard]] bool forwardingEnabled() const { return forwarding_; }
    void setForwardingEnabled(bool enabled) { forwarding_ = enabled; }

    [[nodiscard]] const std::vector<StaticRouteEntry>& staticRoutes() const { return staticRoutes_; }
    [[nodiscard]] Status addStaticRoute(StaticRouteEntry entry);
    bool removeStaticRoute(RouteId id);
    void setStaticRoutes(std::vector<StaticRouteEntry> routes);

    /// Convenience wrapper that installs or clears the 0.0.0.0/0 static route.
    void setDefaultGateway(std::optional<Ipv4Address> gateway);
    [[nodiscard]] std::optional<Ipv4Address> defaultGateway() const;

    [[nodiscard]] const std::vector<Ipv4Address>& dnsServers() const { return dnsServers_; }
    void setDnsServers(std::vector<Ipv4Address> servers) { dnsServers_ = std::move(servers); }

    [[nodiscard]] const std::string& domainName() const { return domainName_; }
    void setDomainName(std::string name) { domainName_ = std::move(name); }

    [[nodiscard]] RoutingTable& routingTable() { return routingTable_; }
    [[nodiscard]] const RoutingTable& routingTable() const { return routingTable_; }

    [[nodiscard]] ArpCache& arpCache() { return arpCache_; }
    [[nodiscard]] const ArpCache& arpCache() const { return arpCache_; }

    [[nodiscard]] OspfConfiguration& ospf() { return ospf_; }
    [[nodiscard]] const OspfConfiguration& ospf() const { return ospf_; }

    /// Rebuilds connected and static routes from the current interface state.
    /// Must be called after addressing or link state changes.
    void refreshRoutes();

    // --- Extension points --------------------------------------------------
    void bindUdpPort(u16 port, UdpHandler handler);
    void unbindUdpPort(u16 port);
    [[nodiscard]] bool isUdpPortBound(u16 port) const;

    void setForwardingFilter(ForwardingFilter filter) { forwardingFilter_ = std::move(filter); }

    // --- Simulation lifecycle ----------------------------------------------
    void onPowerOn(DeviceContext& context);
    void onReset();

    /// Handles a frame. Returns true when the stack consumed it.
    bool onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame);

    void onTimer(DeviceContext& context, TimerId timer);

    // --- Application interface ---------------------------------------------
    /// Starts a ping run. Fails immediately when no route to the destination
    /// exists, so the CLI can report that without waiting for a timeout.
    [[nodiscard]] Result<PingId> startPing(DeviceContext& context, const PingRequest& request);

    [[nodiscard]] const PingStatistics* pingStatistics(PingId id) const;
    [[nodiscard]] bool hasActivePing() const;
    void cancelPing(DeviceContext& context, PingId id);

    /// Sends a UDP datagram.
    bool sendUdp(DeviceContext& context, Ipv4Address destination, u16 sourcePort, u16 destinationPort,
                 std::span<const u8> payload, FrameCategory category, std::string summary,
                 const Ipv4SendOptions& options = {});

    /// Sends an arbitrary IPv4 payload. Returns false when the packet could not
    /// be routed; queueing behind ARP counts as success.
    bool sendIpv4(DeviceContext& context, Ipv4Address destination, u8 protocol,
                  std::span<const u8> payload, FrameCategory category, std::string summary,
                  const Ipv4SendOptions& options = {});

private:
    // --- Timer bookkeeping -------------------------------------------------
    enum class TimerKind : u8 { ArpRetry, ArpSweep, PingSend, PingTimeout };

    struct TimerPurpose {
        TimerKind kind = TimerKind::ArpSweep;
        u32 primary = 0;   ///< target address value, or ping identifier
        u32 secondary = 0; ///< sequence number for ping timeouts
    };

    /// A packet waiting for its next hop's MAC address.
    struct QueuedPacket {
        ByteBuffer ipPacket;
        FrameCategory category = FrameCategory::Unknown;
        std::string summary;
        /// Set when the packet is passing through rather than originating here,
        /// so its identity survives the wait.
        std::optional<FrameIdentity> inheritedIdentity;
    };

    struct PendingArp {
        Ipv4Address target;
        InterfaceId egress;
        std::vector<QueuedPacket> queue;
        int attempts = 0;
        TimerId timer = 0;
    };

    struct PingSession {
        PingId id = 0;
        PingRequest request;
        PingStatistics statistics;
        u16 nextSequence = 1;
        std::map<u16, SimTime> outstanding;
        TimerId sendTimer = 0;
    };

    // --- Receive path ------------------------------------------------------
    bool acceptsDestinationMac(const Interface& ingress, MacAddress destination) const;
    void handleArp(DeviceContext& context, Interface& ingress, std::span<const u8> payload, const Frame& frame);
    void handleIpv4(DeviceContext& context, Interface& ingress, std::span<const u8> payload, const Frame& frame);
    void forwardIpv4(DeviceContext& context, Interface& ingress, const proto::Ipv4PacketView& packet,
                     const Frame& frame);
    void deliverLocally(DeviceContext& context, Interface& ingress, const proto::Ipv4PacketView& packet,
                        const Frame& frame);
    void handleIcmp(DeviceContext& context, Interface& ingress, const proto::Ipv4PacketView& packet,
                    const Frame& frame);
    void handleUdp(DeviceContext& context, Interface& ingress, const proto::Ipv4PacketView& packet,
                   const Frame& frame);

    // --- Send path ---------------------------------------------------------
    void emitOnInterface(DeviceContext& context, Interface& egress, Ipv4Address nextHop,
                         ByteBuffer ipPacket, FrameCategory category, std::string summary,
                         std::optional<FrameIdentity> inherited = std::nullopt);
    void transmitIpv4Frame(DeviceContext& context, Interface& egress, MacAddress destinationMac,
                           const ByteBuffer& ipPacket, FrameCategory category, std::string summary,
                           const std::optional<FrameIdentity>& inherited);
    void startArpResolution(DeviceContext& context, Interface& egress, Ipv4Address target,
                            QueuedPacket packet);
    void sendProbe(DeviceContext& context, PingSession& session);
    void sendArpRequest(DeviceContext& context, Interface& egress, Ipv4Address target);
    void sendIcmpError(DeviceContext& context, proto::IcmpType type, u8 code,
                       const proto::Ipv4PacketView& original);

    // --- Helpers -----------------------------------------------------------
    [[nodiscard]] bool isLocalDestination(Ipv4Address address) const;
    [[nodiscard]] bool isBroadcastFor(const Interface& iface, Ipv4Address address) const;
    [[nodiscard]] std::optional<Ipv4Address> selectSourceAddress(const Interface& egress,
                                                                 Ipv4Address destination) const;
    TimerId armTimer(DeviceContext& context, TimerPurpose purpose, Duration delay);
    void scheduleArpSweep(DeviceContext& context);
    void finishPing(DeviceContext& context, PingSession& session);
    [[nodiscard]] std::string ownerName() const;

    Device& owner_;

    bool forwarding_ = false;
    std::vector<StaticRouteEntry> staticRoutes_;
    std::vector<Ipv4Address> dnsServers_;
    std::string domainName_;
    OspfConfiguration ospf_;

    RoutingTable routingTable_;
    ArpCache arpCache_;

    std::map<Ipv4Address, PendingArp> pendingArp_;
    std::map<u16, PingSession> pings_;
    std::map<u16, UdpHandler> udpHandlers_;
    ForwardingFilter forwardingFilter_;

    std::map<TimerId, TimerPurpose> timers_;

    u16 nextPingId_ = 1;
    u16 nextIpIdentification_ = 1;
};

} // namespace tnp::core
