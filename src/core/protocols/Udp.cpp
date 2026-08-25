#include "core/protocols/Udp.h"

#include "core/protocols/Ipv4.h"
#include "utilities/ByteStream.h"

namespace tnp::core::proto {
namespace {

constexpr std::size_t kChecksumOffset = 6;

/// Partial one's-complement sum over the IPv4 pseudo-header.
u32 pseudoHeaderSum(Ipv4Address source, Ipv4Address destination, u16 udpLength) {
    u32 sum = 0;
    sum += (source.value() >> 16) & 0xFFFFu;
    sum += source.value() & 0xFFFFu;
    sum += (destination.value() >> 16) & 0xFFFFu;
    sum += destination.value() & 0xFFFFu;
    sum += static_cast<u32>(IpProtocol::Udp);
    sum += udpLength;
    return sum;
}

} // namespace

ByteBuffer encodeUdp(UdpHeader header, Ipv4Address source, Ipv4Address destination,
                     std::span<const u8> payload) {
    header.length = static_cast<u16>(kUdpHeaderSize + payload.size());
    header.checksum = 0;

    ByteBuffer bytes;
    bytes.reserve(header.length);
    ByteWriter writer{bytes};

    writer.u16v(header.sourcePort);
    writer.u16v(header.destinationPort);
    writer.u16v(header.length);
    writer.u16v(0); // checksum placeholder
    writer.bytes(payload);

    u16 checksum = internetChecksum(bytes, pseudoHeaderSum(source, destination, header.length));
    // RFC 768: a computed checksum of zero is transmitted as all ones, because
    // zero means "no checksum".
    if (checksum == 0) checksum = 0xFFFF;
    writer.patchU16(kChecksumOffset, checksum);

    return bytes;
}

std::optional<UdpDatagramView> decodeUdp(std::span<const u8> bytes) {
    if (bytes.size() < kUdpHeaderSize) return std::nullopt;

    ByteReader reader{bytes};
    UdpDatagramView view;

    const auto sourcePort = reader.u16v();
    const auto destinationPort = reader.u16v();
    const auto length = reader.u16v();
    const auto checksum = reader.u16v();
    if (!sourcePort || !destinationPort || !length || !checksum) return std::nullopt;
    if (*length < kUdpHeaderSize || *length > bytes.size()) return std::nullopt;

    view.header.sourcePort = *sourcePort;
    view.header.destinationPort = *destinationPort;
    view.header.length = *length;
    view.header.checksum = *checksum;
    view.payload = bytes.subspan(kUdpHeaderSize, *length - kUdpHeaderSize);
    return view;
}

bool udpChecksumValid(std::span<const u8> bytes, Ipv4Address source, Ipv4Address destination) {
    if (bytes.size() < kUdpHeaderSize) return false;
    const u16 stored = static_cast<u16>((bytes[kChecksumOffset] << 8) | bytes[kChecksumOffset + 1]);
    if (stored == 0) return true; // checksum not computed by the sender

    const u16 length = static_cast<u16>((bytes[4] << 8) | bytes[5]);
    if (length > bytes.size()) return false;
    return internetChecksum(bytes.first(length), pseudoHeaderSum(source, destination, length)) == 0;
}

} // namespace tnp::core::proto
