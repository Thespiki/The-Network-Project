#pragma once

#include "core/network/Ipv4Address.h"
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

/// A 128-bit IPv6 address.
///
/// Stored in network order (the same order it appears on the wire), because
/// unlike IPv4 there is no native integer type to hold it and byte-wise
/// comparison in network order is already lexicographic.
class Ipv6Address {
public:
    static constexpr std::size_t kSize = 16;
    using Bytes = std::array<u8, kSize>;

    constexpr Ipv6Address() = default;
    explicit constexpr Ipv6Address(Bytes bytes) : bytes_(bytes) {}

    /// Accepts full form, "::" zero compression and the IPv4-embedded form
    /// ("::ffff:192.0.2.1"). A zone index ("%eth0") is not accepted.
    [[nodiscard]] static std::optional<Ipv6Address> parse(std::string_view text);
    [[nodiscard]] static std::optional<Ipv6Address> fromBytes(std::span<const u8> bytes);

    [[nodiscard]] static Ipv6Address unspecified() { return Ipv6Address{}; }
    [[nodiscard]] static Ipv6Address loopback();

    /// Builds the fe80::/64 link-local address from a MAC-derived interface
    /// identifier. `interfaceId` supplies the low 64 bits.
    [[nodiscard]] static Ipv6Address linkLocalFromInterfaceId(u64 interfaceId);

    /// RFC 5952 canonical text: lower case, longest zero run compressed once.
    [[nodiscard]] std::string toString() const;
    /// Uncompressed 8-group form, useful in the packet inspector.
    [[nodiscard]] std::string toExpandedString() const;

    [[nodiscard]] const Bytes& bytes() const { return bytes_; }
    [[nodiscard]] u16 group(std::size_t index) const;

    [[nodiscard]] bool isUnspecified() const;
    [[nodiscard]] bool isLoopback() const;
    [[nodiscard]] bool isMulticast() const { return bytes_[0] == 0xFF; }
    [[nodiscard]] bool isLinkLocal() const { return bytes_[0] == 0xFE && (bytes_[1] & 0xC0u) == 0x80u; }
    [[nodiscard]] bool isUniqueLocal() const { return (bytes_[0] & 0xFEu) == 0xFCu; }

    /// Zeroes every bit below `prefixLength`.
    [[nodiscard]] Ipv6Address maskedTo(u8 prefixLength) const;

    auto operator<=>(const Ipv6Address&) const = default;
    bool operator==(const Ipv6Address&) const = default;

private:
    Bytes bytes_{};
};

} // namespace tnp::core

template <>
struct std::hash<tnp::core::Ipv6Address> {
    std::size_t operator()(const tnp::core::Ipv6Address& address) const noexcept {
        std::uint64_t hi = 0;
        std::uint64_t lo = 0;
        std::memcpy(&hi, address.bytes().data(), 8);
        std::memcpy(&lo, address.bytes().data() + 8, 8);
        return static_cast<std::size_t>(hi ^ (lo * 0x9E3779B97F4A7C15ULL));
    }
};
