#pragma once

#include "core/devices/DhcpServer.h"
#include "core/devices/DnsServer.h"
#include "core/devices/Ipv4Device.h"

namespace tnp::core {

/// An end host that also offers network services.
///
/// Everything a PC has, plus a DHCP server and an authoritative DNS zone. Both
/// are off by default: a server that silently answered DHCP would be a
/// surprising thing to drop onto a canvas.
class Server final : public Ipv4Device {
public:
    Server(DeviceId id, std::string name);

    [[nodiscard]] DeviceType type() const override { return DeviceType::Server; }

    [[nodiscard]] DhcpServer* dhcpServer() override { return &dhcp_; }
    [[nodiscard]] const DhcpServer* dhcpServer() const override { return &dhcp_; }
    [[nodiscard]] DnsServer* dnsServer() override { return &dns_; }
    [[nodiscard]] const DnsServer* dnsServer() const override { return &dns_; }

    void onReset() override;

private:
    DhcpServer dhcp_;
    DnsServer dns_;
};

} // namespace tnp::core
