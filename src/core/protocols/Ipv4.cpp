#include "core/protocols/Ipv4.h"

#include "utilities/ByteStream.h"

#include <format>

namespace tnp::core::proto {
namespace {

constexpr std::size_t kChecksumOffset = 10;
constexpr std::size_t kTtlOffset = 8;

} // namespace

std::string ipProtocolName(u8 protocol) {
    switch (static_cast<IpProtocol>(protocol)) {
        case IpProtocol::Icmp: return "ICMP";
        case IpProtocol::Tcp:  return "TCP";
        case IpProtocol::Udp:  return "UDP";
    }
    return std::format("Protocol {}", protocol);
}

ByteBuffer encodeIpv4(Ipv4Header header, std::span<const u8> payload) {
    header.headerLengthWords = 5; // TNP never emits options
    header.totalLength = static_cast<u16>(kIpv4MinimumHeaderSize + payload.size());
    header.checksum = 0;

    ByteBuffer bytes;
    bytes.reserve(header.totalLength);
    ByteWriter writer{bytes};

    writer.u8v(static_cast<u8>((header.version << 4) | (header.headerLengthWords & 0x0Fu)));
    writer.u8v(static_cast<u8>((header.dscp << 2) | (header.ecn & 0x03u)));
    writer.u16v(header.totalLength);
    writer.u16v(header.identification);

    const u16 flagsAndOffset = static_cast<u16>((header.dontFragment ? 0x4000u : 0u) |
                                                (header.moreFragments ? 0x2000u : 0u) |
                                                (header.fragmentOffset & 0x1FFFu));
    writer.u16v(flagsAndOffset);
    writer.u8v(header.ttl);
    writer.u8v(header.protocol);

    const std::size_t checksumAt = writer.offset();
    writer.u16v(0); // placeholder

    writer.u32v(header.source.value());
    writer.u32v(header.destination.value());

    const u16 checksum = internetChecksum(std::span<const u8>{bytes}.first(kIpv4MinimumHeaderSize));
    writer.patchU16(checksumAt, checksum);

    writer.bytes(payload);
    return bytes;
}

namespace {

/// Shared body of the two decoders. `requireCompletePayload` is what separates a
/// packet that must be whole from a fragment quoted inside an ICMP error.
std::optional<Ipv4PacketView> decodeIpv4Impl(std::span<const u8> bytes, bool requireCompletePayload) {
    if (bytes.size() < kIpv4MinimumHeaderSize) return std::nullopt;

    ByteReader reader{bytes};
    Ipv4PacketView view;

    const auto versionAndLength = reader.u8v();
    if (!versionAndLength) return std::nullopt;
    view.header.version = static_cast<u8>(*versionAndLength >> 4);
    view.header.headerLengthWords = static_cast<u8>(*versionAndLength & 0x0Fu);
    if (view.header.version != 4 || view.header.headerLengthWords < 5) return std::nullopt;

    const std::size_t headerLength = view.header.headerLengthBytes();
    if (bytes.size() < headerLength) return std::nullopt;

    const auto serviceField = reader.u8v();
    const auto totalLength = reader.u16v();
    const auto identification = reader.u16v();
    const auto flagsAndOffset = reader.u16v();
    const auto ttl = reader.u8v();
    const auto protocol = reader.u8v();
    const auto checksum = reader.u16v();
    const auto source = reader.u32v();
    const auto destination = reader.u32v();
    if (!serviceField || !totalLength || !identification || !flagsAndOffset || !ttl ||
        !protocol || !checksum || !source || !destination) {
        return std::nullopt;
    }

    view.header.dscp = static_cast<u8>(*serviceField >> 2);
    view.header.ecn = static_cast<u8>(*serviceField & 0x03u);
    view.header.totalLength = *totalLength;
    view.header.identification = *identification;
    view.header.dontFragment = (*flagsAndOffset & 0x4000u) != 0;
    view.header.moreFragments = (*flagsAndOffset & 0x2000u) != 0;
    view.header.fragmentOffset = static_cast<u16>(*flagsAndOffset & 0x1FFFu);
    view.header.ttl = *ttl;
    view.header.protocol = *protocol;
    view.header.checksum = *checksum;
    view.header.source = Ipv4Address{*source};
    view.header.destination = Ipv4Address{*destination};

    if (*totalLength < headerLength) return std::nullopt;

    // Trust the length field, not the buffer size: an Ethernet frame carrying a
    // short packet is padded, and that padding is not IP payload.
    const std::size_t available = bytes.size() - headerLength;
    std::size_t payloadLength = *totalLength - headerLength;

    if (payloadLength > available) {
        if (requireCompletePayload) return std::nullopt;
        payloadLength = available;
    }

    view.headerBytes = bytes.first(headerLength);
    view.payload = bytes.subspan(headerLength, payloadLength);
    view.datagram = bytes.first(headerLength + payloadLength);
    return view;
}

} // namespace

std::optional<Ipv4PacketView> decodeIpv4(std::span<const u8> bytes) {
    return decodeIpv4Impl(bytes, true);
}

std::optional<Ipv4PacketView> decodeIpv4Header(std::span<const u8> bytes) {
    return decodeIpv4Impl(bytes, false);
}

bool ipv4HeaderChecksumValid(std::span<const u8> headerBytes) {
    if (headerBytes.size() < kIpv4MinimumHeaderSize) return false;
    // Summing a correct header including its checksum field yields zero.
    return internetChecksum(headerBytes) == 0;
}

bool setIpv4Ttl(ByteBuffer& packet, u8 ttl) {
    if (packet.size() < kIpv4MinimumHeaderSize) return false;
    const std::size_t headerLength = static_cast<std::size_t>(packet[0] & 0x0Fu) * 4;
    if (headerLength < kIpv4MinimumHeaderSize || packet.size() < headerLength) return false;

    packet[kTtlOffset] = ttl;
    packet[kChecksumOffset] = 0;
    packet[kChecksumOffset + 1] = 0;

    const u16 checksum = internetChecksum(std::span<const u8>{packet}.first(headerLength));
    packet[kChecksumOffset] = static_cast<u8>(checksum >> 8);
    packet[kChecksumOffset + 1] = static_cast<u8>(checksum & 0xFFu);
    return true;
}

} // namespace tnp::core::proto
