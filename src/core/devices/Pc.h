#pragma once

#include "core/devices/Ipv4Device.h"

namespace tnp::core {

/// A workstation.
///
/// One wired interface plus a wireless interface that starts administratively
/// down, so a PC can be moved onto a wireless segment without changing its type.
class Pc final : public Ipv4Device {
public:
    static constexpr std::string_view kWiredInterfaceName = "GigabitEthernet0";
    static constexpr std::string_view kWirelessInterfaceName = "Wireless0";

    Pc(DeviceId id, std::string name);

    [[nodiscard]] DeviceType type() const override { return DeviceType::Pc; }

    [[nodiscard]] Interface& wiredInterface() { return *interfaces().front(); }
};

} // namespace tnp::core
