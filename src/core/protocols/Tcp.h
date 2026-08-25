#pragma once

#include "core/network/Ipv4Address.h"
#include "utilities/Types.h"

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace tnp::core::proto {

/// TCP control bits.
struct TcpFlags {
    bool fin = false;
    bool syn = false;
    bool rst = false;
    bool psh = false;
    bool ack = false;
    bool urg = false;

    [[nodiscard]] u8 toBits() const;
    [[nodiscard]] static TcpFlags fromBits(u8 bits);

    /// "SYN, ACK"
    [[nodiscard]] std::string toString() const;

    bool operator==(const TcpFlags&) const = default;
};

/// TCP header (RFC 9293) without options.
struct TcpHeader {
    u16 sourcePort = 0;
    u16 destinationPort = 0;
    u32 sequenceNumber = 0;
    u32 acknowledgementNumber = 0;
    u8 dataOffsetWords = 5;
    TcpFlags flags;
    u16 window = 65535;
    u16 checksum = 0;
    u16 urgentPointer = 0;

    [[nodiscard]] std::size_t headerLengthBytes() const { return static_cast<std::size_t>(dataOffsetWords) * 4; }
};

inline constexpr std::size_t kTcpMinimumHeaderSize = 20;

[[nodiscard]] ByteBuffer encodeTcp(TcpHeader header,
                                   Ipv4Address source,
                                   Ipv4Address destination,
                                   std::span<const u8> payload);

struct TcpSegmentView {
    TcpHeader header;
    std::span<const u8> payload;
};

[[nodiscard]] std::optional<TcpSegmentView> decodeTcp(std::span<const u8> bytes);

[[nodiscard]] bool tcpChecksumValid(std::span<const u8> bytes, Ipv4Address source, Ipv4Address destination);

/// Connection states of RFC 9293.
///
/// The state machine itself is not driven yet; the enumeration and the header
/// codec exist so a transport implementation can be added without reshaping the
/// packet pipeline. See docs/ROADMAP for what remains.
enum class TcpState : u8 {
    Closed, Listen, SynSent, SynReceived, Established,
    FinWait1, FinWait2, CloseWait, Closing, LastAck, TimeWait
};

[[nodiscard]] std::string_view tcpStateName(TcpState state);

} // namespace tnp::core::proto
