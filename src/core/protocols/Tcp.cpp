#include "core/protocols/Tcp.h"

#include "core/protocols/Ipv4.h"
#include "utilities/ByteStream.h"

#include <vector>

namespace tnp::core::proto {
namespace {

constexpr std::size_t kChecksumOffset = 16;

constexpr u8 kFin = 0x01;
constexpr u8 kSyn = 0x02;
constexpr u8 kRst = 0x04;
constexpr u8 kPsh = 0x08;
constexpr u8 kAck = 0x10;
constexpr u8 kUrg = 0x20;

u32 pseudoHeaderSum(Ipv4Address source, Ipv4Address destination, u16 tcpLength) {
    u32 sum = 0;
    sum += (source.value() >> 16) & 0xFFFFu;
    sum += source.value() & 0xFFFFu;
    sum += (destination.value() >> 16) & 0xFFFFu;
    sum += destination.value() & 0xFFFFu;
    sum += static_cast<u32>(IpProtocol::Tcp);
    sum += tcpLength;
    return sum;
}

} // namespace

u8 TcpFlags::toBits() const {
    return static_cast<u8>((fin ? kFin : 0) | (syn ? kSyn : 0) | (rst ? kRst : 0) |
                           (psh ? kPsh : 0) | (ack ? kAck : 0) | (urg ? kUrg : 0));
}

TcpFlags TcpFlags::fromBits(u8 bits) {
    TcpFlags flags;
    flags.fin = (bits & kFin) != 0;
    flags.syn = (bits & kSyn) != 0;
    flags.rst = (bits & kRst) != 0;
    flags.psh = (bits & kPsh) != 0;
    flags.ack = (bits & kAck) != 0;
    flags.urg = (bits & kUrg) != 0;
    return flags;
}

std::string TcpFlags::toString() const {
    std::vector<std::string> names;
    if (syn) names.emplace_back("SYN");
    if (ack) names.emplace_back("ACK");
    if (fin) names.emplace_back("FIN");
    if (rst) names.emplace_back("RST");
    if (psh) names.emplace_back("PSH");
    if (urg) names.emplace_back("URG");
    if (names.empty()) return "none";

    std::string out = names.front();
    for (std::size_t i = 1; i < names.size(); ++i) out += ", " + names[i];
    return out;
}

ByteBuffer encodeTcp(TcpHeader header, Ipv4Address source, Ipv4Address destination,
                     std::span<const u8> payload) {
    header.dataOffsetWords = 5; // no options
    header.checksum = 0;

    ByteBuffer bytes;
    bytes.reserve(kTcpMinimumHeaderSize + payload.size());
    ByteWriter writer{bytes};

    writer.u16v(header.sourcePort);
    writer.u16v(header.destinationPort);
    writer.u32v(header.sequenceNumber);
    writer.u32v(header.acknowledgementNumber);
    writer.u8v(static_cast<u8>(header.dataOffsetWords << 4));
    writer.u8v(header.flags.toBits());
    writer.u16v(header.window);
    writer.u16v(0); // checksum placeholder
    writer.u16v(header.urgentPointer);
    writer.bytes(payload);

    const u16 length = static_cast<u16>(bytes.size());
    writer.patchU16(kChecksumOffset, internetChecksum(bytes, pseudoHeaderSum(source, destination, length)));
    return bytes;
}

std::optional<TcpSegmentView> decodeTcp(std::span<const u8> bytes) {
    if (bytes.size() < kTcpMinimumHeaderSize) return std::nullopt;

    ByteReader reader{bytes};
    TcpSegmentView view;

    const auto sourcePort = reader.u16v();
    const auto destinationPort = reader.u16v();
    const auto sequenceNumber = reader.u32v();
    const auto acknowledgementNumber = reader.u32v();
    const auto offsetAndReserved = reader.u8v();
    const auto flagBits = reader.u8v();
    const auto window = reader.u16v();
    const auto checksum = reader.u16v();
    const auto urgentPointer = reader.u16v();
    if (!sourcePort || !destinationPort || !sequenceNumber || !acknowledgementNumber ||
        !offsetAndReserved || !flagBits || !window || !checksum || !urgentPointer) {
        return std::nullopt;
    }

    view.header.sourcePort = *sourcePort;
    view.header.destinationPort = *destinationPort;
    view.header.sequenceNumber = *sequenceNumber;
    view.header.acknowledgementNumber = *acknowledgementNumber;
    view.header.dataOffsetWords = static_cast<u8>(*offsetAndReserved >> 4);
    view.header.flags = TcpFlags::fromBits(*flagBits);
    view.header.window = *window;
    view.header.checksum = *checksum;
    view.header.urgentPointer = *urgentPointer;

    const std::size_t headerLength = view.header.headerLengthBytes();
    if (headerLength < kTcpMinimumHeaderSize || headerLength > bytes.size()) return std::nullopt;

    view.payload = bytes.subspan(headerLength);
    return view;
}

bool tcpChecksumValid(std::span<const u8> bytes, Ipv4Address source, Ipv4Address destination) {
    if (bytes.size() < kTcpMinimumHeaderSize) return false;
    return internetChecksum(bytes, pseudoHeaderSum(source, destination, static_cast<u16>(bytes.size()))) == 0;
}

std::string_view tcpStateName(TcpState state) {
    switch (state) {
        case TcpState::Closed:      return "CLOSED";
        case TcpState::Listen:      return "LISTEN";
        case TcpState::SynSent:     return "SYN-SENT";
        case TcpState::SynReceived: return "SYN-RECEIVED";
        case TcpState::Established: return "ESTABLISHED";
        case TcpState::FinWait1:    return "FIN-WAIT-1";
        case TcpState::FinWait2:    return "FIN-WAIT-2";
        case TcpState::CloseWait:   return "CLOSE-WAIT";
        case TcpState::Closing:     return "CLOSING";
        case TcpState::LastAck:     return "LAST-ACK";
        case TcpState::TimeWait:    return "TIME-WAIT";
    }
    return "CLOSED";
}

} // namespace tnp::core::proto
