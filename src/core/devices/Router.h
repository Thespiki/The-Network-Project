#pragma once

#include "core/devices/DhcpServer.h"
#include "core/devices/Ipv4Device.h"

namespace tnp::core {

/// An IPv4 router.
///
/// Forwards between its interfaces using longest-prefix match, answers ARP for
/// its own addresses, generates the ICMP errors that make traceroute and
/// unreachable-detection work, and can hand out addresses over DHCP.
class Router final : public Ipv4Device {
public:
    static constexpr std::size_t kDefaultEthernetPorts = 4;
    static constexpr std::size_t kDefaultSerialPorts = 2;

    Router(DeviceId id, std::string name,
           std::size_t ethernetPorts = kDefaultEthernetPorts,
           std::size_t serialPorts = kDefaultSerialPorts);

    [[nodiscard]] DeviceType type() const override { return DeviceType::Router; }

    [[nodiscard]] DhcpServer* dhcpServer() override { return &dhcp_; }
    [[nodiscard]] const DhcpServer* dhcpServer() const override { return &dhcp_; }

    void onReset() override;

private:
    DhcpServer dhcp_;
};

} // namespace tnp::core
