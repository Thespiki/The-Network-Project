#pragma once

#include "core/devices/SwitchingEngine.h"
#include "core/network/Device.h"

namespace tnp::core {

/// A layer-2 Ethernet switch.
///
/// Learns source addresses, forwards known unicast to a single port, floods
/// unknown unicast and broadcast, and keeps VLANs apart. It has no IP stack:
/// management addressing belongs to a layer-3 switch.
class Switch final : public Device {
public:
    static constexpr std::size_t kDefaultPortCount = 8;

    Switch(DeviceId id, std::string name, std::size_t portCount = kDefaultPortCount);

    [[nodiscard]] DeviceType type() const override { return DeviceType::Switch; }

    [[nodiscard]] SwitchingEngine* switching() override { return &switching_; }
    [[nodiscard]] const SwitchingEngine* switching() const override { return &switching_; }

    void onPowerOn(DeviceContext& context) override;
    void onReset() override;
    void onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) override;
    void onTimer(DeviceContext& context, TimerId timer) override;

private:
    SwitchingEngine switching_;
};

} // namespace tnp::core
