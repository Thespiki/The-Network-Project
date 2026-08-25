#include "simulation/PacketDecoder.h"

#include "core/protocols/Arp.h"
#include "core/protocols/Dhcp.h"
#include "core/protocols/Dns.h"
#include "core/protocols/Ethernet.h"
#include "core/protocols/Icmp.h"
#include "core/protocols/Ipv4.h"
#include "core/protocols/Tcp.h"
#include "core/protocols/Udp.h"

#include <format>
#include <utility>

namespace tnp::sim {
namespace {

using namespace core;
using namespace core::proto;

DecodedField field(std::string name, std::string value, std::size_t offset, std::size_t length,
                   std::string detail = {}) {
    return DecodedField{std::move(name), std::move(value), std::move(detail), offset, length};
}

void decodeEthernetLayer(DecodedPacket& packet, const EthernetFrameView& view, std::size_t base) {
    DecodedLayer layer;
    layer.name = "Ethernet II";
    layer.offset = base;
    layer.length = view.header.encodedSize();
    layer.summary = std::format("{} -> {}, {}", view.header.source.toString(),
                                view.header.destination.toString(),
                                etherTypeName(view.header.etherType));

    layer.fields.push_back(field("Destination", view.header.destination.toString(), base, 6,
                                 view.header.destination.isBroadcast() ? "broadcast"
                                 : view.header.destination.isMulticast() ? "multicast"
                                                                         : "unicast"));
    layer.fields.push_back(field("Source", view.header.source.toString(), base + 6, 6));

    std::size_t cursor = base + 12;
    if (view.header.vlanTag) {
        layer.fields.push_back(field("802.1Q VLAN", std::to_string(view.header.vlanTag->vlanId),
                                     cursor, 4,
                                     std::format("priority {}", view.header.vlanTag->priorityCodePoint)));
        cursor += 4;
    }
    layer.fields.push_back(field("EtherType", std::format("0x{:04X}", view.header.etherType), cursor, 2,
                                 etherTypeName(view.header.etherType)));

    packet.layers.push_back(std::move(layer));
}

void decodeArpLayer(DecodedPacket& packet, std::span<const u8> payload, std::size_t base) {
    const auto arp = decodeArp(payload);
    if (!arp) {
        packet.problem = "ARP packet is malformed";
        return;
    }

    DecodedLayer layer;
    layer.name = "ARP";
    layer.offset = base;
    layer.length = ArpMessage::kEncodedSize;

    if (arp->operation == ArpOperation::Request) {
        layer.summary = std::format("Who has {}? Tell {}", arp->targetIp.toString(),
                                    arp->senderIp.toString());
    } else {
        layer.summary = std::format("{} is at {}", arp->senderIp.toString(), arp->senderMac.toString());
    }

    layer.fields.push_back(field("Hardware type", std::to_string(arp->hardwareType), base, 2, "Ethernet"));
    layer.fields.push_back(field("Protocol type", std::format("0x{:04X}", arp->protocolType), base + 2, 2, "IPv4"));
    layer.fields.push_back(field("Operation", std::to_string(static_cast<u16>(arp->operation)), base + 6, 2,
                                 std::string{arpOperationName(arp->operation)}));
    layer.fields.push_back(field("Sender MAC", arp->senderMac.toString(), base + 8, 6));
    layer.fields.push_back(field("Sender IP", arp->senderIp.toString(), base + 14, 4));
    layer.fields.push_back(field("Target MAC", arp->targetMac.toString(), base + 18, 6,
                                 arp->targetMac.isZero() ? "unknown - this is what is being asked" : ""));
    layer.fields.push_back(field("Target IP", arp->targetIp.toString(), base + 24, 4));

    packet.summary = layer.summary;
    packet.layers.push_back(std::move(layer));
}

void decodeIcmpLayer(DecodedPacket& packet, std::span<const u8> payload, std::size_t base) {
    const auto icmp = decodeIcmp(payload);
    if (!icmp) {
        packet.problem = "ICMP message is truncated";
        return;
    }

    DecodedLayer layer;
    layer.name = "ICMP";
    layer.offset = base;
    layer.length = kIcmpHeaderSize + icmp->payload.size();

    layer.fields.push_back(field("Type", std::to_string(icmp->type), base, 1, icmpTypeName(icmp->type)));
    layer.fields.push_back(field("Code", std::to_string(icmp->code), base + 1, 1,
                                 icmpCodeName(icmp->type, icmp->code)));
    layer.fields.push_back(field("Checksum", std::format("0x{:04X}", icmp->checksum), base + 2, 2,
                                 icmpChecksumValid(payload) ? "correct" : "INCORRECT"));

    if (icmp->isEcho()) {
        layer.fields.push_back(field("Identifier", std::to_string(icmp->identifier), base + 4, 2));
        layer.fields.push_back(field("Sequence", std::to_string(icmp->sequence), base + 6, 2));
        layer.fields.push_back(field("Data", std::format("{} bytes", icmp->payload.size()),
                                     base + kIcmpHeaderSize, icmp->payload.size()));
        layer.summary = std::format("{} id={} seq={}", icmpTypeName(icmp->type), icmp->identifier,
                                    icmp->sequence);
    } else {
        layer.fields.push_back(field("Quoted datagram", std::format("{} bytes", icmp->payload.size()),
                                     base + kIcmpHeaderSize, icmp->payload.size(),
                                     "the packet that triggered this error"));
        layer.summary = std::format("{}: {}", icmpTypeName(icmp->type),
                                    icmpCodeName(icmp->type, icmp->code));
    }

    packet.summary = layer.summary;
    packet.layers.push_back(std::move(layer));
}

void decodeDhcpLayer(DecodedPacket& packet, std::span<const u8> payload, std::size_t base) {
    const auto message = decodeDhcp(payload);
    if (!message) return;

    DecodedLayer layer;
    layer.name = "DHCP";
    layer.offset = base;
    layer.length = payload.size();
    layer.summary = std::string{dhcpMessageTypeName(message->messageType)};

    layer.fields.push_back(field("Message type", std::string{dhcpMessageTypeName(message->messageType)},
                                 base, 1));
    layer.fields.push_back(field("Transaction ID", std::format("0x{:08X}", message->transactionId),
                                 base + 4, 4));
    layer.fields.push_back(field("Client MAC", message->clientMac.toString(), base + 28, 6));
    layer.fields.push_back(field("Your address", message->yourAddress.toString(), base + 16, 4));
    if (message->subnetMask) {
        layer.fields.push_back(field("Subnet mask", message->subnetMask->toString(), base, 4));
    }
    if (message->router) {
        layer.fields.push_back(field("Router", message->router->toString(), base, 4));
    }
    if (message->domainNameServer) {
        layer.fields.push_back(field("DNS server", message->domainNameServer->toString(), base, 4));
    }
    if (message->leaseTimeSeconds) {
        layer.fields.push_back(field("Lease time", std::format("{} s", *message->leaseTimeSeconds), base, 4));
    }

    packet.summary = layer.summary;
    packet.layers.push_back(std::move(layer));
}

void decodeDnsLayer(DecodedPacket& packet, std::span<const u8> payload, std::size_t base) {
    const auto message = decodeDns(payload);
    if (!message) return;

    DecodedLayer layer;
    layer.name = "DNS";
    layer.offset = base;
    layer.length = payload.size();

    const std::string queried = message->questions.empty() ? std::string{"?"} : message->questions.front().name;
    layer.summary = message->isResponse ? std::format("Response for {} ({})", queried,
                                                      dnsResponseCodeName(message->responseCode))
                                        : std::format("Query for {}", queried);

    layer.fields.push_back(field("Transaction ID", std::format("0x{:04X}", message->transactionId), base, 2));
    layer.fields.push_back(field("Type", message->isResponse ? "response" : "query", base + 2, 2));
    layer.fields.push_back(field("Response code", std::string{dnsResponseCodeName(message->responseCode)},
                                 base + 2, 2));
    for (const auto& question : message->questions) {
        layer.fields.push_back(field("Question", std::format("{} {}", question.name,
                                                             dnsRecordTypeName(question.type)),
                                     base + kDnsHeaderSize, 0));
    }
    for (const auto& answer : message->answers) {
        layer.fields.push_back(field("Answer", std::format("{} {} {}", answer.name,
                                                           dnsRecordTypeName(answer.type),
                                                           answer.address.toString()),
                                     base + kDnsHeaderSize, 0));
    }

    packet.summary = layer.summary;
    packet.layers.push_back(std::move(layer));
}

void decodeUdpLayer(DecodedPacket& packet, std::span<const u8> payload, std::size_t base,
                    Ipv4Address source, Ipv4Address destination) {
    const auto datagram = decodeUdp(payload);
    if (!datagram) {
        packet.problem = "UDP datagram is truncated";
        return;
    }

    DecodedLayer layer;
    layer.name = "UDP";
    layer.offset = base;
    layer.length = datagram->header.length;
    layer.summary = std::format("{} -> {}", datagram->header.sourcePort, datagram->header.destinationPort);

    layer.fields.push_back(field("Source port", std::to_string(datagram->header.sourcePort), base, 2));
    layer.fields.push_back(field("Destination port", std::to_string(datagram->header.destinationPort),
                                 base + 2, 2));
    layer.fields.push_back(field("Length", std::to_string(datagram->header.length), base + 4, 2));
    layer.fields.push_back(field("Checksum", std::format("0x{:04X}", datagram->header.checksum), base + 6, 2,
                                 udpChecksumValid(payload, source, destination) ? "correct" : "INCORRECT"));

    packet.summary = layer.summary;
    const std::size_t payloadBase = base + kUdpHeaderSize;
    packet.layers.push_back(std::move(layer));

    const u16 sourcePort = datagram->header.sourcePort;
    const u16 destinationPort = datagram->header.destinationPort;

    if (destinationPort == kPortDhcpServer || destinationPort == kPortDhcpClient ||
        sourcePort == kPortDhcpServer || sourcePort == kPortDhcpClient) {
        decodeDhcpLayer(packet, datagram->payload, payloadBase);
    } else if (destinationPort == kPortDns || sourcePort == kPortDns) {
        decodeDnsLayer(packet, datagram->payload, payloadBase);
    }
}

void decodeTcpLayer(DecodedPacket& packet, std::span<const u8> payload, std::size_t base,
                    Ipv4Address source, Ipv4Address destination) {
    const auto segment = decodeTcp(payload);
    if (!segment) {
        packet.problem = "TCP segment is truncated";
        return;
    }

    DecodedLayer layer;
    layer.name = "TCP";
    layer.offset = base;
    layer.length = payload.size();
    layer.summary = std::format("{} -> {} [{}] seq={}", segment->header.sourcePort,
                                segment->header.destinationPort, segment->header.flags.toString(),
                                segment->header.sequenceNumber);

    layer.fields.push_back(field("Source port", std::to_string(segment->header.sourcePort), base, 2));
    layer.fields.push_back(field("Destination port", std::to_string(segment->header.destinationPort),
                                 base + 2, 2));
    layer.fields.push_back(field("Sequence", std::to_string(segment->header.sequenceNumber), base + 4, 4));
    layer.fields.push_back(field("Acknowledgement", std::to_string(segment->header.acknowledgementNumber),
                                 base + 8, 4));
    layer.fields.push_back(field("Flags", segment->header.flags.toString(), base + 13, 1));
    layer.fields.push_back(field("Window", std::to_string(segment->header.window), base + 14, 2));
    layer.fields.push_back(field("Checksum", std::format("0x{:04X}", segment->header.checksum), base + 16, 2,
                                 tcpChecksumValid(payload, source, destination) ? "correct" : "INCORRECT"));

    packet.summary = layer.summary;
    packet.layers.push_back(std::move(layer));
}

void decodeIpv4Layer(DecodedPacket& packet, std::span<const u8> payload, std::size_t base) {
    const auto ip = decodeIpv4(payload);
    if (!ip) {
        packet.problem = "IPv4 packet is malformed";
        return;
    }

    DecodedLayer layer;
    layer.name = "IPv4";
    layer.offset = base;
    layer.length = ip->header.headerLengthBytes();
    layer.summary = std::format("{} -> {}, {}", ip->header.source.toString(),
                                ip->header.destination.toString(),
                                ipProtocolName(ip->header.protocol));

    layer.fields.push_back(field("Version", std::to_string(ip->header.version), base, 1));
    layer.fields.push_back(field("Header length", std::format("{} bytes", ip->header.headerLengthBytes()),
                                 base, 1));
    layer.fields.push_back(field("Total length", std::to_string(ip->header.totalLength), base + 2, 2));
    layer.fields.push_back(field("Identification", std::format("0x{:04X}", ip->header.identification),
                                 base + 4, 2));
    layer.fields.push_back(field("Flags", ip->header.dontFragment ? "Don't fragment" : "none", base + 6, 2));
    layer.fields.push_back(field("TTL", std::to_string(ip->header.ttl), base + 8, 1,
                                 ip->header.ttl <= 1 ? "expires at the next router" : ""));
    layer.fields.push_back(field("Protocol", std::to_string(ip->header.protocol), base + 9, 1,
                                 ipProtocolName(ip->header.protocol)));
    layer.fields.push_back(field("Checksum", std::format("0x{:04X}", ip->header.checksum), base + 10, 2,
                                 ipv4HeaderChecksumValid(ip->headerBytes) ? "correct" : "INCORRECT"));
    layer.fields.push_back(field("Source", ip->header.source.toString(), base + 12, 4));
    layer.fields.push_back(field("Destination", ip->header.destination.toString(), base + 16, 4));

    packet.summary = layer.summary;
    packet.layers.push_back(std::move(layer));

    const std::size_t payloadBase = base + ip->header.headerLengthBytes();
    switch (static_cast<IpProtocol>(ip->header.protocol)) {
        case IpProtocol::Icmp: decodeIcmpLayer(packet, ip->payload, payloadBase); break;
        case IpProtocol::Udp:  decodeUdpLayer(packet, ip->payload, payloadBase, ip->header.source,
                                              ip->header.destination); break;
        case IpProtocol::Tcp:  decodeTcpLayer(packet, ip->payload, payloadBase, ip->header.source,
                                              ip->header.destination); break;
        default: break;
    }
}

} // namespace

DecodedPacket decodePacket(const ByteBuffer& bytes) {
    DecodedPacket packet;
    packet.totalLength = bytes.size();

    const auto ethernet = decodeEthernet(bytes);
    if (!ethernet) {
        packet.problem = "not a decodable Ethernet frame";
        return packet;
    }

    decodeEthernetLayer(packet, *ethernet, 0);
    packet.summary = packet.layers.front().summary;

    const std::size_t payloadBase = ethernet->header.encodedSize();
    switch (static_cast<EtherType>(ethernet->header.etherType)) {
        case EtherType::Arp:  decodeArpLayer(packet, ethernet->payload, payloadBase); break;
        case EtherType::Ipv4: decodeIpv4Layer(packet, ethernet->payload, payloadBase); break;
        case EtherType::Ipv6:
            packet.problem = "IPv6 payloads are not decoded in this version";
            break;
        default:
            break;
    }
    return packet;
}

std::string describeFrame(const ByteBuffer& bytes) {
    const DecodedPacket packet = decodePacket(bytes);
    return packet.summary.empty() ? std::string{"Ethernet frame"} : packet.summary;
}

FrameCategory classifyFrame(const ByteBuffer& bytes) {
    const auto ethernet = decodeEthernet(bytes);
    if (!ethernet) return FrameCategory::Unknown;

    switch (static_cast<EtherType>(ethernet->header.etherType)) {
        case EtherType::Arp:  return FrameCategory::Arp;
        case EtherType::Ipv6: return FrameCategory::Ipv6;
        case EtherType::Ipv4: break;
        default:              return FrameCategory::Other;
    }

    const auto ip = decodeIpv4(ethernet->payload);
    if (!ip) return FrameCategory::Other;

    switch (static_cast<IpProtocol>(ip->header.protocol)) {
        case IpProtocol::Icmp: {
            const auto icmp = decodeIcmp(ip->payload);
            if (icmp && icmp->isError()) return FrameCategory::IcmpError;
            return FrameCategory::Icmp;
        }
        case IpProtocol::Tcp: return FrameCategory::Tcp;
        case IpProtocol::Udp: {
            const auto datagram = decodeUdp(ip->payload);
            if (!datagram) return FrameCategory::Udp;
            const u16 source = datagram->header.sourcePort;
            const u16 destination = datagram->header.destinationPort;
            if (source == kPortDhcpServer || destination == kPortDhcpServer ||
                source == kPortDhcpClient || destination == kPortDhcpClient) {
                return FrameCategory::Dhcp;
            }
            if (source == kPortDns || destination == kPortDns) return FrameCategory::Dns;
            return FrameCategory::Udp;
        }
        default: return FrameCategory::Other;
    }
}

} // namespace tnp::sim
