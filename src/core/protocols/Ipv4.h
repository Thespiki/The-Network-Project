#pragma once

#include "core/network/Ipv4Address.h"
#include "utilities/Types.h"

#include <optional>
#include <span>
#include <string>

namespace tnp::core::proto {

/// IANA protocol numbers TNP simulates.
enum class IpProtocol : u8 {
    Icmp = 1,
    Tcp  = 6,
    Udp  = 17
};

[[nodiscard]] std::string ipProtocolName(u8 protocol);

inline constexpr u8 kDefaultTtl = 64;

/// IPv4 header (RFC 791) without options.
///
/// TNP always emits a 20-byte header (IHL 5). The decoder accepts and skips
/// options so a hand-crafted packet with options is still readable.
struct Ipv4Header {
    u8 version = 4;
    u8 headerLengthWords = 5;   ///< IHL, in 32-bit words
    u8 dscp = 0;
    u8 ecn = 0;
    u16 totalLength = 0;        ///< filled in by the encoder
    u16 identification = 0;
    bool dontFragment = false;
    bool moreFragments = false;
    u16 fragmentOffset = 0;     ///< in 8-byte units
    u8 ttl = kDefaultTtl;
    u8 protocol = static_cast<u8>(IpProtocol::Icmp);
    u16 checksum = 0;           ///< filled in by the encoder
    Ipv4Address source;
    Ipv4Address destination;

    [[nodiscard]] std::size_t headerLengthBytes() const { return static_cast<std::size_t>(headerLengthWords) * 4; }
};

inline constexpr std::size_t kIpv4MinimumHeaderSize = 20;

/// Serialises the header and payload, computing `totalLength` and the header
/// checksum from the real bytes.
[[nodiscard]] ByteBuffer encodeIpv4(Ipv4Header header, std::span<const u8> payload);

struct Ipv4PacketView {
    Ipv4Header header;
    /// Exactly `totalLength - headerLength` bytes: Ethernet padding is excluded.
    std::span<const u8> payload;
    /// The header bytes as received, for checksum verification and the inspector.
    std::span<const u8> headerBytes;
    /// Header and payload together, trimmed to `totalLength`. This is what a
    /// router re-transmits and what an ICMP error quotes.
    std::span<const u8> datagram;
};

[[nodiscard]] std::optional<Ipv4PacketView> decodeIpv4(std::span<const u8> bytes);

/// Decodes the header of a packet that is deliberately incomplete.
///
/// An ICMP error quotes only the IP header plus eight payload bytes, so a strict
/// decode - which insists the buffer really holds `totalLength` bytes - refuses
/// exactly the data the error is required to carry. `payload` is whatever was
/// actually quoted, and `datagram` covers only the bytes present.
[[nodiscard]] std::optional<Ipv4PacketView> decodeIpv4Header(std::span<const u8> bytes);

/// True when the header's checksum field matches the header contents.
[[nodiscard]] bool ipv4HeaderChecksumValid(std::span<const u8> headerBytes);

/// Rewrites the TTL of an encoded packet in place and repairs the checksum.
/// Returns false when the buffer is not a decodable IPv4 packet.
bool setIpv4Ttl(ByteBuffer& packet, u8 ttl);

} // namespace tnp::core::proto
