#pragma once

#include "core/devices/Ipv4Stack.h"
#include "core/network/Ids.h"
#include "core/protocols/Dhcp.h"

#include <map>
#include <optional>
#include <vector>

namespace tnp::core {

class Device;

/// State of one interface's DHCP negotiation.
enum class DhcpClientState : u8 { Idle, Selecting, Requesting, Bound, Failed };

[[nodiscard]] std::string_view dhcpClientStateName(DhcpClientState state);

/// What an interface obtained from a DHCP server.
struct DhcpBinding {
    InterfaceId interface;
    Ipv4Prefix address;
    std::optional<Ipv4Address> gateway;
    std::optional<Ipv4Address> dnsServer;
    Ipv4Address serverAddress;
    SimTime acquiredAt{};
    SimTime expiresAt{};
};

/// The DHCP client of an end host.
///
/// Runs on every interface whose `ipv4DhcpEnabled()` flag is set. Addresses it
/// obtains are applied to the live interface and removed again on reset, so a
/// project file never records an address that was leased rather than configured.
class DhcpClient {
public:
    DhcpClient(Device& owner, Ipv4Stack& stack);

    DhcpClient(const DhcpClient&) = delete;
    DhcpClient& operator=(const DhcpClient&) = delete;

    /// Starts discovery on every DHCP-enabled interface.
    void onPowerOn(DeviceContext& context);

    /// Removes leased addresses and forgets all state.
    void onReset();

    /// Bound to UDP port 68 by the owning device.
    void handleDatagram(DeviceContext& context, Interface& ingress, const proto::Ipv4Header& ip,
                        const proto::UdpHeader& udp, std::span<const u8> payload);

    /// Handles a retry timer. Returns true when `timer` belonged to this client.
    bool onTimer(DeviceContext& context, TimerId timer);

    [[nodiscard]] std::vector<DhcpBinding> bindings() const;
    [[nodiscard]] DhcpClientState stateOf(InterfaceId interface) const;

private:
    struct Session {
        InterfaceId interface;
        DhcpClientState state = DhcpClientState::Idle;
        u32 transactionId = 0;
        Ipv4Address offeredAddress;
        Ipv4Address serverIdentifier;
        int attempts = 0;
        TimerId retryTimer = 0;
        std::optional<DhcpBinding> binding;
        /// The address this client installed, so reset can take it away again.
        std::optional<Ipv4Prefix> installedAddress;
    };

    void startDiscovery(DeviceContext& context, Interface& iface, Session& session);
    void sendRequest(DeviceContext& context, Interface& iface, Session& session);
    void applyBinding(DeviceContext& context, Interface& iface, Session& session,
                      const proto::DhcpMessage& ack);
    void armRetry(DeviceContext& context, Session& session);

    Device& owner_;
    Ipv4Stack& stack_;
    std::map<InterfaceId, Session> sessions_;
    std::map<TimerId, InterfaceId> timers_;
    u32 nextTransactionId_ = 0x54'4E'50'01; ///< "TNP" plus a counter, for readable captures
};

} // namespace tnp::core
