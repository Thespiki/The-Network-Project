#pragma once

#include "core/devices/Ipv4Device.h"

namespace tnp::core {

/// A stand-in for a network TNP does not simulate in detail.
///
/// It routes like a router, so a project can model "the rest of the world"
/// behind one icon: give it an address on the link that faces the topology and
/// static routes for whatever is supposed to live beyond it. Nothing behind a
/// cloud is simulated - it answers only for addresses it owns.
class Cloud final : public Ipv4Device {
public:
    static constexpr std::size_t kDefaultPortCount = 2;

    Cloud(DeviceId id, std::string name, std::size_t portCount = kDefaultPortCount);

    [[nodiscard]] DeviceType type() const override { return DeviceType::Cloud; }

    [[nodiscard]] const std::string& providerName() const { return providerName_; }
    void setProviderName(std::string name) { providerName_ = std::move(name); }

private:
    std::string providerName_ = "Internet";
};

} // namespace tnp::core
