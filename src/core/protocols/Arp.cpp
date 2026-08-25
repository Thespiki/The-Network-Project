#include "core/protocols/Arp.h"

#include "utilities/ByteStream.h"

namespace tnp::core::proto {
namespace {

constexpr u16 kHardwareTypeEthernet = 1;
constexpr u16 kProtocolTypeIpv4 = 0x0800;

} // namespace

std::string_view arpOperationName(ArpOperation operation) {
    switch (operation) {
        case ArpOperation::Request: return "request";
        case ArpOperation::Reply:   return "reply";
    }
    return "unknown";
}

ByteBuffer encodeArp(const ArpMessage& message) {
    ByteBuffer bytes;
    bytes.reserve(ArpMessage::kEncodedSize);
    ByteWriter writer{bytes};

    writer.u16v(message.hardwareType);
    writer.u16v(message.protocolType);
    writer.u8v(message.hardwareLength);
    writer.u8v(message.protocolLength);
    writer.u16v(static_cast<u16>(message.operation));
    writer.bytes(message.senderMac.bytes());
    writer.u32v(message.senderIp.value());
    writer.bytes(message.targetMac.bytes());
    writer.u32v(message.targetIp.value());

    return bytes;
}

std::optional<ArpMessage> decodeArp(std::span<const u8> bytes) {
    ByteReader reader{bytes};
    ArpMessage message;

    const auto hardwareType = reader.u16v();
    const auto protocolType = reader.u16v();
    const auto hardwareLength = reader.u8v();
    const auto protocolLength = reader.u8v();
    const auto operation = reader.u16v();
    if (!hardwareType || !protocolType || !hardwareLength || !protocolLength || !operation) {
        return std::nullopt;
    }

    // Only Ethernet/IPv4 ARP is simulated; anything else is not decodable here.
    if (*hardwareType != kHardwareTypeEthernet || *protocolType != kProtocolTypeIpv4) return std::nullopt;
    if (*hardwareLength != MacAddress::kSize || *protocolLength != 4) return std::nullopt;
    if (*operation != 1 && *operation != 2) return std::nullopt;

    message.hardwareType = *hardwareType;
    message.protocolType = *protocolType;
    message.hardwareLength = *hardwareLength;
    message.protocolLength = *protocolLength;
    message.operation = static_cast<ArpOperation>(*operation);

    const auto senderMac = reader.bytes(MacAddress::kSize);
    const auto senderIp = reader.u32v();
    const auto targetMac = reader.bytes(MacAddress::kSize);
    const auto targetIp = reader.u32v();
    if (!senderMac || !senderIp || !targetMac || !targetIp) return std::nullopt;

    const auto sender = MacAddress::fromBytes(*senderMac);
    const auto target = MacAddress::fromBytes(*targetMac);
    if (!sender || !target) return std::nullopt;

    message.senderMac = *sender;
    message.senderIp = Ipv4Address{*senderIp};
    message.targetMac = *target;
    message.targetIp = Ipv4Address{*targetIp};
    return message;
}

ArpMessage makeArpRequest(MacAddress senderMac, Ipv4Address senderIp, Ipv4Address targetIp) {
    ArpMessage message;
    message.operation = ArpOperation::Request;
    message.senderMac = senderMac;
    message.senderIp = senderIp;
    message.targetMac = MacAddress::zero(); // unknown - that is the point of the request
    message.targetIp = targetIp;
    return message;
}

ArpMessage makeArpReply(const ArpMessage& request, MacAddress replierMac) {
    ArpMessage reply;
    reply.operation = ArpOperation::Reply;
    reply.senderMac = replierMac;
    reply.senderIp = request.targetIp;
    reply.targetMac = request.senderMac;
    reply.targetIp = request.senderIp;
    return reply;
}

} // namespace tnp::core::proto
