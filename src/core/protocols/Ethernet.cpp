#include "core/protocols/Ethernet.h"

#include "utilities/ByteStream.h"

#include <format>

namespace tnp::core::proto {

std::string etherTypeName(u16 etherType) {
    switch (static_cast<EtherType>(etherType)) {
        case EtherType::Ipv4: return "IPv4";
        case EtherType::Arp:  return "ARP";
        case EtherType::Ipv6: return "IPv6";
        case EtherType::Vlan: return "802.1Q";
    }
    return std::format("0x{:04X}", etherType);
}

ByteBuffer encodeEthernet(const EthernetHeader& header, std::span<const u8> payload, bool pad) {
    ByteBuffer bytes;
    bytes.reserve(header.encodedSize() + payload.size());
    ByteWriter writer{bytes};

    writer.bytes(header.destination.bytes());
    writer.bytes(header.source.bytes());

    if (header.vlanTag) {
        writer.u16v(static_cast<u16>(EtherType::Vlan));
        const u16 control = static_cast<u16>((header.vlanTag->priorityCodePoint & 0x07u) << 13) |
                            static_cast<u16>(header.vlanTag->dropEligible ? 0x1000u : 0u) |
                            static_cast<u16>(header.vlanTag->vlanId & 0x0FFFu);
        writer.u16v(control);
    }

    writer.u16v(header.etherType);
    writer.bytes(payload);

    if (pad && bytes.size() < kMinimumFrameSize) {
        writer.fill(0u, kMinimumFrameSize - bytes.size());
    }
    return bytes;
}

std::optional<EthernetFrameView> decodeEthernet(std::span<const u8> bytes) {
    ByteReader reader{bytes};

    const auto destination = reader.bytes(MacAddress::kSize);
    const auto source = reader.bytes(MacAddress::kSize);
    if (!destination || !source) return std::nullopt;

    EthernetFrameView view;
    const auto destinationMac = MacAddress::fromBytes(*destination);
    const auto sourceMac = MacAddress::fromBytes(*source);
    if (!destinationMac || !sourceMac) return std::nullopt;

    view.header.destination = *destinationMac;
    view.header.source = *sourceMac;

    auto etherType = reader.u16v();
    if (!etherType) return std::nullopt;

    if (*etherType == static_cast<u16>(EtherType::Vlan)) {
        const auto control = reader.u16v();
        if (!control) return std::nullopt;

        VlanTag tag;
        tag.priorityCodePoint = static_cast<u8>((*control >> 13) & 0x07u);
        tag.dropEligible = (*control & 0x1000u) != 0;
        tag.vlanId = static_cast<VlanId>(*control & 0x0FFFu);
        view.header.vlanTag = tag;

        etherType = reader.u16v();
        if (!etherType) return std::nullopt;
    }

    view.header.etherType = *etherType;
    view.payload = reader.rest();
    return view;
}

} // namespace tnp::core::proto
