#pragma once

#include "utilities/Types.h"

#include <array>
#include <compare>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace tnp::core {

/// A 32-bit IPv4 address.
///
/// Stored as a host-order integer so masking, comparison and subnet arithmetic
/// are single instructions. Conversion to and from the wire is explicit, which
/// keeps byte-order mistakes out of the protocol encoders.
class Ipv4Address {
public:
    constexpr Ipv4Address() = default;
    explicit constexpr Ipv4Address(u32 value) : value_(value) {}
    constexpr Ipv4Address(u8 a, u8 b, u8 c, u8 d)
        : value_((static_cast<u32>(a) << 24) | (static_cast<u32>(b) << 16) |
                 (static_cast<u32>(c) << 8) | static_cast<u32>(d)) {}

    /// Strict dotted-quad parsing: exactly four decimal octets in 0..255, no
    /// leading zeros beyond a single "0", no octal or hexadecimal forms.
    [[nodiscard]] static std::optional<Ipv4Address> parse(std::string_view text);

    /// Four bytes in network order.
    [[nodiscard]] static std::optional<Ipv4Address> fromBytes(std::span<const u8> bytes);

    [[nodiscard]] static constexpr Ipv4Address any()             { return Ipv4Address{0u}; }
    [[nodiscard]] static constexpr Ipv4Address loopback()        { return Ipv4Address{127, 0, 0, 1}; }
    [[nodiscard]] static constexpr Ipv4Address limitedBroadcast(){ return Ipv4Address{0xFFFFFFFFu}; }

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] std::array<u8, 4> toBytes() const;

    [[nodiscard]] constexpr u32 value() const { return value_; }
    [[nodiscard]] constexpr u8 octet(std::size_t index) const {
        return static_cast<u8>((value_ >> (24 - 8 * index)) & 0xFFu);
    }

    [[nodiscard]] constexpr bool isUnspecified() const { return value_ == 0; }
    [[nodiscard]] constexpr bool isLoopback() const { return octet(0) == 127; }
    [[nodiscard]] constexpr bool isMulticast() const { return octet(0) >= 224 && octet(0) <= 239; }
    [[nodiscard]] constexpr bool isLimitedBroadcast() const { return value_ == 0xFFFFFFFFu; }
    [[nodiscard]] constexpr bool isLinkLocal() const { return octet(0) == 169 && octet(1) == 254; }
    [[nodiscard]] constexpr bool isReserved() const { return octet(0) >= 240; }

    /// RFC 1918 private space.
    [[nodiscard]] constexpr bool isPrivate() const {
        return octet(0) == 10 ||
               (octet(0) == 172 && octet(1) >= 16 && octet(1) <= 31) ||
               (octet(0) == 192 && octet(1) == 168);
    }

    /// Usable as a host address on a link: not 0.0.0.0, not a multicast group,
    /// not the limited broadcast and not in the reserved class E range.
    [[nodiscard]] constexpr bool isAssignableToHost() const {
        return !isUnspecified() && !isMulticast() && !isLimitedBroadcast() && !isReserved();
    }

    [[nodiscard]] constexpr Ipv4Address operator&(Ipv4Address other) const { return Ipv4Address{value_ & other.value_}; }
    [[nodiscard]] constexpr Ipv4Address operator|(Ipv4Address other) const { return Ipv4Address{value_ | other.value_}; }
    [[nodiscard]] constexpr Ipv4Address operator~() const { return Ipv4Address{~value_}; }

    [[nodiscard]] constexpr Ipv4Address next() const { return Ipv4Address{value_ + 1}; }
    [[nodiscard]] constexpr Ipv4Address previous() const { return Ipv4Address{value_ - 1}; }

    auto operator<=>(const Ipv4Address&) const = default;
    bool operator==(const Ipv4Address&) const = default;

private:
    u32 value_ = 0;
};

} // namespace tnp::core

template <>
struct std::hash<tnp::core::Ipv4Address> {
    std::size_t operator()(const tnp::core::Ipv4Address& address) const noexcept {
        return std::hash<tnp::u32>{}(address.value());
    }
};
