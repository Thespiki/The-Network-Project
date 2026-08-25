#pragma once

#include "core/devices/Ipv4Device.h"
#include "core/devices/SwitchingEngine.h"

namespace tnp::core {

/// A switch that also routes.
///
/// Frames are bridged within their VLAN by the switching engine. Frames
/// addressed to the device itself - including broadcasts, so ARP works - are
/// handed to the IPv4 stack through the switch virtual interface (SVI) of that
/// VLAN, which is how traffic crosses from one VLAN to another.
class Layer3Switch final : public Ipv4Device {
public:
    static constexpr std::size_t kDefaultPortCount = 8;

    Layer3Switch(DeviceId id, std::string name, std::size_t portCount = kDefaultPortCount);

    [[nodiscard]] DeviceType type() const override { return DeviceType::Layer3Switch; }

    [[nodiscard]] SwitchingEngine* switching() override { return &switching_; }
    [[nodiscard]] const SwitchingEngine* switching() const override { return &switching_; }

    /// Creates (or returns) the SVI for `vlan`.
    Interface& ensureSvi(VlanId vlan);

    /// The SVI serving `vlan`, or nullptr when none is configured.
    [[nodiscard]] Interface* findSvi(VlanId vlan);
    [[nodiscard]] const Interface* findSvi(VlanId vlan) const;

    void onPowerOn(DeviceContext& context) override;
    void onReset() override;
    void onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) override;
    void onTimer(DeviceContext& context, TimerId timer) override;

private:
    SwitchingEngine switching_;
};

} // namespace tnp::core
