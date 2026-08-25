#pragma once

#include "core/devices/FirewallPolicy.h"
#include "core/devices/Ipv4Device.h"

namespace tnp::core {

/// A routing firewall.
///
/// Identical to a router in how it forwards, and different only in what it
/// refuses to forward: the policy is installed as the IPv4 stack's forwarding
/// filter, so there is exactly one forwarding implementation in TNP.
class Firewall final : public Ipv4Device {
public:
    static constexpr std::size_t kDefaultPortCount = 4;

    Firewall(DeviceId id, std::string name, std::size_t portCount = kDefaultPortCount);

    [[nodiscard]] DeviceType type() const override { return DeviceType::Firewall; }

    [[nodiscard]] FirewallPolicy* firewallPolicy() override { return &policy_; }
    [[nodiscard]] const FirewallPolicy* firewallPolicy() const override { return &policy_; }

    void onReset() override;

private:
    void installFilter();

    FirewallPolicy policy_;
};

} // namespace tnp::core
