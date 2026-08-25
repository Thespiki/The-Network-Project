#pragma once

#include "core/network/Device.h"

namespace tnp::core {

/// An Ethernet repeater.
///
/// Every frame that arrives is repeated out of every other port, with no MAC
/// table and no filtering. Deliberately not built on `SwitchingEngine`: a hub is
/// the absence of bridging, and sharing code with a switch would only obscure
/// what makes the two behave differently in a simulation.
class Hub final : public Device {
public:
    static constexpr std::size_t kDefaultPortCount = 4;

    Hub(DeviceId id, std::string name, std::size_t portCount = kDefaultPortCount);

    [[nodiscard]] DeviceType type() const override { return DeviceType::Hub; }

    void onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) override;
};

} // namespace tnp::core
