#pragma once

#include "core/devices/DhcpClient.h"
#include "core/devices/Ipv4Stack.h"
#include "core/network/Device.h"

namespace tnp::core {

/// Shared base for every device that runs an IPv4 stack.
///
/// The one level of inheritance in the device hierarchy, and it exists to share
/// *implementation*: the receive, timer and reset plumbing is identical for a
/// PC, a router, a firewall and a cloud, and duplicating it five times is how
/// forwarding behaviour drifts apart. Everything that differs between those
/// devices - interface layout, whether forwarding is on, extra services - is
/// composed in by the subclass.
class Ipv4Device : public Device {
public:
    Ipv4Device(DeviceId id, std::string name);

    [[nodiscard]] Ipv4Stack* ipv4Stack() override { return &stack_; }
    [[nodiscard]] const Ipv4Stack* ipv4Stack() const override { return &stack_; }

    [[nodiscard]] DhcpClient& dhcpClient() { return dhcpClient_; }
    [[nodiscard]] const DhcpClient& dhcpClient() const { return dhcpClient_; }

    void onPowerOn(DeviceContext& context) override;
    void onReset() override;
    void onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) override;
    void onTimer(DeviceContext& context, TimerId timer) override;

protected:
    /// Binds the DHCP client to UDP port 68. Subclasses that offer DHCP service
    /// bind port 67 in addition.
    void bindDhcpClient();

    Ipv4Stack stack_;
    DhcpClient dhcpClient_;
};

} // namespace tnp::core
