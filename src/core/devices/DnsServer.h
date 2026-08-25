#pragma once

#include "core/devices/Ipv4Stack.h"
#include "core/network/Ids.h"
#include "core/protocols/Dns.h"

#include <optional>
#include <string>
#include <vector>

namespace tnp::core {

class Device;

/// One name-to-address mapping in the server's zone.
struct DnsRecord {
    DnsRecordId id;
    std::string name;
    Ipv4Address address;
    u32 timeToLive = 300;
};

/// A small authoritative DNS server.
///
/// It answers A queries for the names in its own table and returns NXDOMAIN for
/// everything else. There is no recursion and no upstream forwarding: a
/// simulated network has no Internet to ask.
class DnsServer {
public:
    DnsServer(Device& owner, Ipv4Stack& stack);

    DnsServer(const DnsServer&) = delete;
    DnsServer& operator=(const DnsServer&) = delete;

    [[nodiscard]] bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    [[nodiscard]] const std::vector<DnsRecord>& records() const { return records_; }
    void addRecord(DnsRecord record);
    bool removeRecord(DnsRecordId id);
    void setRecords(std::vector<DnsRecord> records);

    /// Case-insensitive lookup, used by the server itself and by the CLI.
    [[nodiscard]] std::optional<Ipv4Address> resolve(std::string_view name) const;

    /// Bound to UDP port 53 by the owning device.
    void handleDatagram(DeviceContext& context, Interface& ingress, const proto::Ipv4Header& ip,
                        const proto::UdpHeader& udp, std::span<const u8> payload);

private:
    Device& owner_;
    Ipv4Stack& stack_;
    bool enabled_ = false;
    std::vector<DnsRecord> records_;
};

} // namespace tnp::core
