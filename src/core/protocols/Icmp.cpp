#include "core/protocols/Icmp.h"

#include "core/protocols/Ipv4.h"
#include "utilities/ByteStream.h"

#include <algorithm>
#include <format>

namespace tnp::core::proto {
namespace {

constexpr std::size_t kChecksumOffset = 2;

ByteBuffer encodeWithChecksum(u8 type, u8 code, u16 restHigh, u16 restLow, std::span<const u8> payload) {
    ByteBuffer bytes;
    bytes.reserve(kIcmpHeaderSize + payload.size());
    ByteWriter writer{bytes};

    writer.u8v(type);
    writer.u8v(code);
    writer.u16v(0); // checksum placeholder
    writer.u16v(restHigh);
    writer.u16v(restLow);
    writer.bytes(payload);

    writer.patchU16(kChecksumOffset, internetChecksum(bytes));
    return bytes;
}

} // namespace

std::string icmpTypeName(u8 type) {
    switch (static_cast<IcmpType>(type)) {
        case IcmpType::EchoReply:              return "Echo Reply";
        case IcmpType::DestinationUnreachable: return "Destination Unreachable";
        case IcmpType::EchoRequest:            return "Echo Request";
        case IcmpType::TimeExceeded:           return "Time Exceeded";
    }
    return std::format("Type {}", type);
}

std::string icmpCodeName(u8 type, u8 code) {
    if (static_cast<IcmpType>(type) == IcmpType::DestinationUnreachable) {
        switch (static_cast<IcmpUnreachableCode>(code)) {
            case IcmpUnreachableCode::NetworkUnreachable:  return "Network unreachable";
            case IcmpUnreachableCode::HostUnreachable:     return "Host unreachable";
            case IcmpUnreachableCode::ProtocolUnreachable: return "Protocol unreachable";
            case IcmpUnreachableCode::PortUnreachable:     return "Port unreachable";
            case IcmpUnreachableCode::FragmentationNeeded: return "Fragmentation needed";
        }
    }
    if (static_cast<IcmpType>(type) == IcmpType::TimeExceeded) {
        switch (static_cast<IcmpTimeExceededCode>(code)) {
            case IcmpTimeExceededCode::TtlExpiredInTransit:            return "TTL expired in transit";
            case IcmpTimeExceededCode::FragmentReassemblyTimeExceeded: return "Fragment reassembly time exceeded";
        }
    }
    if (code == 0) return {};
    return std::format("Code {}", code);
}

ByteBuffer encodeIcmpEcho(IcmpType type, u16 identifier, u16 sequence, std::span<const u8> payload) {
    return encodeWithChecksum(static_cast<u8>(type), 0, identifier, sequence, payload);
}

ByteBuffer encodeIcmpError(IcmpType type, u8 code, std::span<const u8> originalDatagram) {
    // Quote the IP header plus the first eight payload bytes, which is what the
    // receiver needs to match the error to the flow that caused it.
    std::size_t quoteLength = originalDatagram.size();
    if (const auto packet = decodeIpv4(originalDatagram)) {
        quoteLength = std::min(originalDatagram.size(),
                               packet->header.headerLengthBytes() + kIcmpQuotedPayloadBytes);
    }
    return encodeWithChecksum(static_cast<u8>(type), code, 0, 0, originalDatagram.first(quoteLength));
}

std::optional<IcmpMessage> decodeIcmp(std::span<const u8> bytes) {
    if (bytes.size() < kIcmpHeaderSize) return std::nullopt;

    ByteReader reader{bytes};
    IcmpMessage message;

    const auto type = reader.u8v();
    const auto code = reader.u8v();
    const auto checksum = reader.u16v();
    const auto restHigh = reader.u16v();
    const auto restLow = reader.u16v();
    if (!type || !code || !checksum || !restHigh || !restLow) return std::nullopt;

    message.type = *type;
    message.code = *code;
    message.checksum = *checksum;
    message.identifier = *restHigh;
    message.sequence = *restLow;
    message.payload = reader.rest();
    return message;
}

bool icmpChecksumValid(std::span<const u8> bytes) {
    if (bytes.size() < kIcmpHeaderSize) return false;
    return internetChecksum(bytes) == 0;
}

} // namespace tnp::core::proto
