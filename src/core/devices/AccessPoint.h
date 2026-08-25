#pragma once

#include "core/devices/SwitchingEngine.h"
#include "core/network/Device.h"

namespace tnp::core {

/// A wireless access point.
///
/// Modelled as a two-port bridge: a wired uplink and a radio. Wireless stations
/// attach to the radio interface through ordinary links, so association is
/// represented topologically rather than simulated at the radio level - there is
/// no contention, no signal strength and no roaming.
class AccessPoint final : public Device {
public:
    AccessPoint(DeviceId id, std::string name);

    [[nodiscard]] DeviceType type() const override { return DeviceType::AccessPoint; }

    [[nodiscard]] SwitchingEngine* switching() override { return &switching_; }
    [[nodiscard]] const SwitchingEngine* switching() const override { return &switching_; }

    [[nodiscard]] const std::string& ssid() const { return ssid_; }
    void setSsid(std::string ssid) { ssid_ = std::move(ssid); }

    void onPowerOn(DeviceContext& context) override;
    void onReset() override;
    void onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) override;
    void onTimer(DeviceContext& context, TimerId timer) override;

private:
    SwitchingEngine switching_;
    std::string ssid_ = "TNP-WLAN";
};

} // namespace tnp::core
