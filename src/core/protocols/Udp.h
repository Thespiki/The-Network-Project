#pragma once

#include "core/network/Ipv4Address.h"
#include "utilities/Types.h"

#include <optional>
#include <span>

namespace tnp::core::proto {

/// UDP header (RFC 768).
struct UdpHeader {
    u16 sourcePort = 0;
    u16 destinationPort = 0;
    u16 length = 0;    ///< header + payload, filled in by the encoder
    u16 checksum = 0;  ///< filled in by the encoder
};

inline constexpr std::size_t kUdpHeaderSize = 8;

/// Serialises a datagram. The checksum covers the IPv4 pseudo-header, so the
/// endpoint addresses must be supplied.
[[nodiscard]] ByteBuffer encodeUdp(UdpHeader header,
                                   Ipv4Address source,
                                   Ipv4Address destination,
                                   std::span<const u8> payload);

struct UdpDatagramView {
    UdpHeader header;
    std::span<const u8> payload;
};

[[nodiscard]] std::optional<UdpDatagramView> decodeUdp(std::span<const u8> bytes);

/// Verifies the pseudo-header checksum. A zero checksum means "not computed"
/// and is reported as valid, matching RFC 768.
[[nodiscard]] bool udpChecksumValid(std::span<const u8> bytes, Ipv4Address source, Ipv4Address destination);

/// Well-known ports used by the simulated services.
inline constexpr u16 kPortDhcpServer = 67;
inline constexpr u16 kPortDhcpClient = 68;
inline constexpr u16 kPortDns = 53;

} // namespace tnp::core::proto
