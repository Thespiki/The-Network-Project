#pragma once

#include "utilities/Types.h"

#include <optional>
#include <span>
#include <string>

namespace tnp::core::proto {

enum class IcmpType : u8 {
    EchoReply              = 0,
    DestinationUnreachable = 3,
    EchoRequest            = 8,
    TimeExceeded           = 11
};

enum class IcmpUnreachableCode : u8 {
    NetworkUnreachable  = 0,
    HostUnreachable     = 1,
    ProtocolUnreachable = 2,
    PortUnreachable     = 3,
    FragmentationNeeded = 4
};

enum class IcmpTimeExceededCode : u8 {
    TtlExpiredInTransit            = 0,
    FragmentReassemblyTimeExceeded = 1
};

[[nodiscard]] std::string icmpTypeName(u8 type);
[[nodiscard]] std::string icmpCodeName(u8 type, u8 code);

/// A decoded ICMP message.
///
/// The identifier/sequence pair is only meaningful for echo messages; for error
/// messages `payload` holds the quoted original datagram (IP header plus the
/// first eight bytes of its payload, per RFC 792).
struct IcmpMessage {
    u8 type = static_cast<u8>(IcmpType::EchoRequest);
    u8 code = 0;
    u16 checksum = 0;
    u16 identifier = 0;
    u16 sequence = 0;
    std::span<const u8> payload;

    [[nodiscard]] bool isEcho() const {
        return type == static_cast<u8>(IcmpType::EchoRequest) || type == static_cast<u8>(IcmpType::EchoReply);
    }
    [[nodiscard]] bool isError() const {
        return type == static_cast<u8>(IcmpType::DestinationUnreachable) ||
               type == static_cast<u8>(IcmpType::TimeExceeded);
    }
};

inline constexpr std::size_t kIcmpHeaderSize = 8;

/// Bytes of the original datagram quoted inside an ICMP error: the IP header
/// plus eight payload bytes, which is enough to identify the flow.
inline constexpr std::size_t kIcmpQuotedPayloadBytes = 8;

[[nodiscard]] ByteBuffer encodeIcmpEcho(IcmpType type, u16 identifier, u16 sequence, std::span<const u8> payload);

/// Builds an ICMP error quoting `originalDatagram` (a complete IPv4 packet).
[[nodiscard]] ByteBuffer encodeIcmpError(IcmpType type, u8 code, std::span<const u8> originalDatagram);

[[nodiscard]] std::optional<IcmpMessage> decodeIcmp(std::span<const u8> bytes);

/// True when the message's checksum covers its bytes correctly.
[[nodiscard]] bool icmpChecksumValid(std::span<const u8> bytes);

} // namespace tnp::core::proto
