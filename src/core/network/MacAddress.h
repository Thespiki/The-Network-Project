#pragma once

#include "utilities/Types.h"

#include <array>
#include <compare>
#include <cstring>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace tnp::core {

/// IEEE 802 48-bit hardware address.
class MacAddress {
public:
    static constexpr std::size_t kSize = 6;
    using Bytes = std::array<u8, kSize>;

    constexpr MacAddress() = default;
    explicit constexpr MacAddress(Bytes bytes) : bytes_(bytes) {}

    /// Accepts the three notations engineers actually type:
    ///   AA:BB:CC:DD:EE:FF   AA-BB-CC-DD-EE-FF   aabb.ccdd.eeff
    [[nodiscard]] static std::optional<MacAddress> parse(std::string_view text);

    /// Reads six bytes off the wire.
    [[nodiscard]] static std::optional<MacAddress> fromBytes(std::span<const u8> bytes);

    /// FF:FF:FF:FF:FF:FF
    [[nodiscard]] static MacAddress broadcast();
    /// 00:00:00:00:00:00 - "unset", and the sender hardware field of an ARP probe.
    [[nodiscard]] static MacAddress zero();

    /// Random locally-administered unicast address (bit 1 of the first octet set,
    /// bit 0 clear). TNP never mints addresses inside a real vendor's OUI space.
    [[nodiscard]] static MacAddress generateUnicast();

    [[nodiscard]] std::string toString() const;            ///< AA:BB:CC:DD:EE:FF
    [[nodiscard]] std::string toCiscoString() const;       ///< aabb.ccdd.eeff

    [[nodiscard]] const Bytes& bytes() const { return bytes_; }
    [[nodiscard]] u8 octet(std::size_t index) const { return bytes_.at(index); }

    [[nodiscard]] bool isBroadcast() const;
    /// True for broadcast too: broadcast is the all-ones multicast address.
    [[nodiscard]] bool isMulticast() const { return (bytes_[0] & 0x01u) != 0; }
    [[nodiscard]] bool isUnicast() const { return !isMulticast(); }
    [[nodiscard]] bool isLocallyAdministered() const { return (bytes_[0] & 0x02u) != 0; }
    [[nodiscard]] bool isZero() const;

    /// The 24-bit Organizationally Unique Identifier.
    [[nodiscard]] u32 oui() const;

    auto operator<=>(const MacAddress&) const = default;
    bool operator==(const MacAddress&) const = default;

private:
    Bytes bytes_{};
};

} // namespace tnp::core

template <>
struct std::hash<tnp::core::MacAddress> {
    std::size_t operator()(const tnp::core::MacAddress& mac) const noexcept {
        std::uint64_t packed = 0;
        std::memcpy(&packed, mac.bytes().data(), tnp::core::MacAddress::kSize);
        return static_cast<std::size_t>(packed * 0x9E3779B97F4A7C15ULL);
    }
};
