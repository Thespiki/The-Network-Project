#pragma once

#include "core/network/MacAddress.h"
#include "core/network/Vlan.h"
#include "utilities/Types.h"

#include <optional>
#include <span>
#include <string>

namespace tnp::core::proto {

/// EtherType values TNP understands. Others are carried transparently.
enum class EtherType : u16 {
    Ipv4 = 0x0800,
    Arp  = 0x0806,
    Ipv6 = 0x86DD,
    Vlan = 0x8100  ///< 802.1Q tag protocol identifier
};

[[nodiscard]] std::string etherTypeName(u16 etherType);

/// 802.1Q tag contents.
struct VlanTag {
    u8 priorityCodePoint = 0;
    bool dropEligible = false;
    VlanId vlanId = kDefaultVlan;

    bool operator==(const VlanTag&) const = default;
};

/// Ethernet II header, optionally carrying a single 802.1Q tag.
///
/// The frame check sequence is not modelled: real NICs strip it before the
/// operating system sees the frame, and simulating bit errors is out of scope.
struct EthernetHeader {
    MacAddress destination;
    MacAddress source;
    std::optional<VlanTag> vlanTag;
    u16 etherType = static_cast<u16>(EtherType::Ipv4);

    /// 14 bytes, or 18 with a VLAN tag.
    [[nodiscard]] std::size_t encodedSize() const { return vlanTag ? 18u : 14u; }
};

/// Smallest legal Ethernet frame on the wire, excluding the FCS. Short payloads
/// are padded to reach it, exactly as a real NIC does; this is why an ARP frame
/// is 60 bytes rather than 42.
inline constexpr std::size_t kMinimumFrameSize = 60;
inline constexpr std::size_t kMinimumPayloadSize = 46;

/// Serialises the header followed by `payload`, padding to the minimum frame
/// size unless `pad` is false.
[[nodiscard]] ByteBuffer encodeEthernet(const EthernetHeader& header,
                                        std::span<const u8> payload,
                                        bool pad = true);

/// A decoded frame. `payload` points into the original buffer and may include
/// padding; upper layers must use their own length fields.
struct EthernetFrameView {
    EthernetHeader header;
    std::span<const u8> payload;
};

[[nodiscard]] std::optional<EthernetFrameView> decodeEthernet(std::span<const u8> bytes);

} // namespace tnp::core::proto
