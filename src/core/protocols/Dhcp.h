#pragma once

#include "core/network/Ipv4Address.h"
#include "core/network/MacAddress.h"
#include "utilities/Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::core::proto {

/// DHCP message types (RFC 2131 option 53).
enum class DhcpMessageType : u8 {
    Discover = 1,
    Offer    = 2,
    Request  = 3,
    Decline  = 4,
    Ack      = 5,
    Nak      = 6,
    Release  = 7,
    Inform   = 8
};

[[nodiscard]] std::string_view dhcpMessageTypeName(DhcpMessageType type);

/// DHCP option codes TNP encodes or understands.
enum class DhcpOption : u8 {
    SubnetMask         = 1,
    Router             = 3,
    DomainNameServer   = 6,
    RequestedIpAddress = 50,
    LeaseTime          = 51,
    MessageType        = 53,
    ServerIdentifier   = 54,
    End                = 255
};

/// A BOOTP/DHCP message.
///
/// Only the fields the simulated allocation flow needs are modelled; the rest of
/// the fixed BOOTP area is encoded as zeros so the wire format stays valid and
/// the packet inspector shows correct offsets.
struct DhcpMessage {
    u8 operation = 1;              ///< 1 = request (client), 2 = reply (server)
    u32 transactionId = 0;
    u16 secondsElapsed = 0;
    bool broadcastFlag = true;

    Ipv4Address clientAddress;     ///< ciaddr
    Ipv4Address yourAddress;       ///< yiaddr, the address being offered
    Ipv4Address serverAddress;     ///< siaddr
    Ipv4Address gatewayAddress;    ///< giaddr
    MacAddress clientMac;          ///< chaddr

    DhcpMessageType messageType = DhcpMessageType::Discover;

    std::optional<Ipv4Address> subnetMask;
    std::optional<Ipv4Address> router;
    std::optional<Ipv4Address> domainNameServer;
    std::optional<Ipv4Address> requestedAddress;
    std::optional<Ipv4Address> serverIdentifier;
    std::optional<u32> leaseTimeSeconds;
};

inline constexpr std::size_t kDhcpFixedSize = 236;
inline constexpr u32 kDhcpMagicCookie = 0x63825363;

[[nodiscard]] ByteBuffer encodeDhcp(const DhcpMessage& message);
[[nodiscard]] std::optional<DhcpMessage> decodeDhcp(std::span<const u8> bytes);

} // namespace tnp::core::proto
