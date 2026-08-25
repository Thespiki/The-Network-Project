#include "core/protocols/Dhcp.h"

#include "utilities/ByteStream.h"

namespace tnp::core::proto {
namespace {

void writeAddressOption(ByteWriter& writer, DhcpOption option, const std::optional<Ipv4Address>& address) {
    if (!address) return;
    writer.u8v(static_cast<u8>(option));
    writer.u8v(4);
    writer.u32v(address->value());
}

} // namespace

std::string_view dhcpMessageTypeName(DhcpMessageType type) {
    switch (type) {
        case DhcpMessageType::Discover: return "DHCPDISCOVER";
        case DhcpMessageType::Offer:    return "DHCPOFFER";
        case DhcpMessageType::Request:  return "DHCPREQUEST";
        case DhcpMessageType::Decline:  return "DHCPDECLINE";
        case DhcpMessageType::Ack:      return "DHCPACK";
        case DhcpMessageType::Nak:      return "DHCPNAK";
        case DhcpMessageType::Release:  return "DHCPRELEASE";
        case DhcpMessageType::Inform:   return "DHCPINFORM";
    }
    return "DHCP";
}

ByteBuffer encodeDhcp(const DhcpMessage& message) {
    ByteBuffer bytes;
    bytes.reserve(kDhcpFixedSize + 64);
    ByteWriter writer{bytes};

    writer.u8v(message.operation);
    writer.u8v(1);  // htype: Ethernet
    writer.u8v(MacAddress::kSize);
    writer.u8v(0);  // hops
    writer.u32v(message.transactionId);
    writer.u16v(message.secondsElapsed);
    writer.u16v(message.broadcastFlag ? 0x8000u : 0u);

    writer.u32v(message.clientAddress.value());
    writer.u32v(message.yourAddress.value());
    writer.u32v(message.serverAddress.value());
    writer.u32v(message.gatewayAddress.value());

    writer.bytes(message.clientMac.bytes());
    writer.fill(0u, 16 - MacAddress::kSize); // chaddr padding
    writer.fill(0u, 64);                     // sname
    writer.fill(0u, 128);                    // file

    writer.u32v(kDhcpMagicCookie);

    writer.u8v(static_cast<u8>(DhcpOption::MessageType));
    writer.u8v(1);
    writer.u8v(static_cast<u8>(message.messageType));

    writeAddressOption(writer, DhcpOption::SubnetMask, message.subnetMask);
    writeAddressOption(writer, DhcpOption::Router, message.router);
    writeAddressOption(writer, DhcpOption::DomainNameServer, message.domainNameServer);
    writeAddressOption(writer, DhcpOption::RequestedIpAddress, message.requestedAddress);
    writeAddressOption(writer, DhcpOption::ServerIdentifier, message.serverIdentifier);

    if (message.leaseTimeSeconds) {
        writer.u8v(static_cast<u8>(DhcpOption::LeaseTime));
        writer.u8v(4);
        writer.u32v(*message.leaseTimeSeconds);
    }

    writer.u8v(static_cast<u8>(DhcpOption::End));
    return bytes;
}

std::optional<DhcpMessage> decodeDhcp(std::span<const u8> bytes) {
    if (bytes.size() < kDhcpFixedSize + 4) return std::nullopt;

    ByteReader reader{bytes};
    DhcpMessage message;

    const auto operation = reader.u8v();
    const auto hardwareType = reader.u8v();
    const auto hardwareLength = reader.u8v();
    if (!operation || !hardwareType || !hardwareLength) return std::nullopt;
    if (!reader.skip(1)) return std::nullopt; // hops

    const auto transactionId = reader.u32v();
    const auto secondsElapsed = reader.u16v();
    const auto flags = reader.u16v();
    if (!transactionId || !secondsElapsed || !flags) return std::nullopt;

    const auto clientAddress = reader.u32v();
    const auto yourAddress = reader.u32v();
    const auto serverAddress = reader.u32v();
    const auto gatewayAddress = reader.u32v();
    if (!clientAddress || !yourAddress || !serverAddress || !gatewayAddress) return std::nullopt;

    const auto clientHardware = reader.bytes(16);
    if (!clientHardware) return std::nullopt;
    const auto clientMac = MacAddress::fromBytes(*clientHardware);
    if (!clientMac) return std::nullopt;

    if (!reader.skip(64 + 128)) return std::nullopt; // sname + file

    const auto cookie = reader.u32v();
    if (!cookie || *cookie != kDhcpMagicCookie) return std::nullopt;

    message.operation = *operation;
    message.transactionId = *transactionId;
    message.secondsElapsed = *secondsElapsed;
    message.broadcastFlag = (*flags & 0x8000u) != 0;
    message.clientAddress = Ipv4Address{*clientAddress};
    message.yourAddress = Ipv4Address{*yourAddress};
    message.serverAddress = Ipv4Address{*serverAddress};
    message.gatewayAddress = Ipv4Address{*gatewayAddress};
    message.clientMac = *clientMac;

    bool sawMessageType = false;
    while (!reader.exhausted()) {
        const auto code = reader.u8v();
        if (!code) break;
        if (*code == static_cast<u8>(DhcpOption::End)) break;
        if (*code == 0) continue; // pad

        const auto length = reader.u8v();
        if (!length) return std::nullopt;
        const auto value = reader.bytes(*length);
        if (!value) return std::nullopt;

        const auto asAddress = [&]() -> std::optional<Ipv4Address> {
            if (value->size() != 4) return std::nullopt;
            return Ipv4Address::fromBytes(*value);
        };

        switch (static_cast<DhcpOption>(*code)) {
            case DhcpOption::MessageType:
                if (value->size() != 1) return std::nullopt;
                message.messageType = static_cast<DhcpMessageType>((*value)[0]);
                sawMessageType = true;
                break;
            case DhcpOption::SubnetMask:         message.subnetMask = asAddress(); break;
            case DhcpOption::Router:             message.router = asAddress(); break;
            case DhcpOption::DomainNameServer:   message.domainNameServer = asAddress(); break;
            case DhcpOption::RequestedIpAddress: message.requestedAddress = asAddress(); break;
            case DhcpOption::ServerIdentifier:   message.serverIdentifier = asAddress(); break;
            case DhcpOption::LeaseTime:
                if (value->size() == 4) {
                    message.leaseTimeSeconds = (static_cast<u32>((*value)[0]) << 24) |
                                               (static_cast<u32>((*value)[1]) << 16) |
                                               (static_cast<u32>((*value)[2]) << 8) |
                                               static_cast<u32>((*value)[3]);
                }
                break;
            default:
                break; // unknown options are ignored, as a real client would
        }
    }

    if (!sawMessageType) return std::nullopt;
    return message;
}

} // namespace tnp::core::proto
