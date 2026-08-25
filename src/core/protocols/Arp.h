#pragma once

#include "core/network/Ipv4Address.h"
#include "core/network/MacAddress.h"
#include "utilities/Types.h"

#include <optional>
#include <span>
#include <string_view>

namespace tnp::core::proto {

enum class ArpOperation : u16 { Request = 1, Reply = 2 };

[[nodiscard]] std::string_view arpOperationName(ArpOperation operation);

/// ARP packet for the Ethernet/IPv4 pairing (RFC 826).
///
/// The hardware and protocol type fields are kept in the struct rather than
/// hard-coded so the decoder can reject packets for address families TNP does
/// not simulate instead of silently misreading them.
struct ArpMessage {
    u16 hardwareType = 1;                                 ///< Ethernet
    u16 protocolType = static_cast<u16>(0x0800);          ///< IPv4
    u8 hardwareLength = MacAddress::kSize;
    u8 protocolLength = 4;
    ArpOperation operation = ArpOperation::Request;

    MacAddress senderMac;
    Ipv4Address senderIp;
    MacAddress targetMac;   ///< zero in a request
    Ipv4Address targetIp;

    static constexpr std::size_t kEncodedSize = 28;
};

[[nodiscard]] ByteBuffer encodeArp(const ArpMessage& message);
[[nodiscard]] std::optional<ArpMessage> decodeArp(std::span<const u8> bytes);

/// Builds the broadcast "who has `targetIp`, tell `senderIp`" request.
[[nodiscard]] ArpMessage makeArpRequest(MacAddress senderMac, Ipv4Address senderIp, Ipv4Address targetIp);

/// Builds the unicast reply to `request`.
[[nodiscard]] ArpMessage makeArpReply(const ArpMessage& request, MacAddress replierMac);

} // namespace tnp::core::proto
