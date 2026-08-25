#pragma once

#include "core/network/Ipv4Address.h"
#include "core/network/Ipv6Address.h"
#include "utilities/Types.h"

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace tnp::core {

/// An IPv4 address together with a prefix length: "192.168.1.10/24".
///
/// The address is kept exactly as supplied. That distinction matters: an
/// interface is configured with a *host* address inside a prefix, while a route
/// destination is a *network* prefix. `network()` normalises when needed.
class Ipv4Prefix {
public:
    static constexpr u8 kMaxPrefixLength = 32;

    constexpr Ipv4Prefix() = default;
    constexpr Ipv4Prefix(Ipv4Address address, u8 prefixLength)
        : address_(address), prefixLength_(prefixLength > kMaxPrefixLength ? kMaxPrefixLength : prefixLength) {}

    /// Parses CIDR notation. The prefix length must be present and in 0..32.
    [[nodiscard]] static std::optional<Ipv4Prefix> parse(std::string_view text);

    /// Parses "192.168.1.10 255.255.255.0"; the mask must be contiguous.
    [[nodiscard]] static std::optional<Ipv4Prefix> parseWithMask(std::string_view address, std::string_view mask);

    /// 255.255.255.0 for /24. Prefix length 0 yields 0.0.0.0.
    [[nodiscard]] static Ipv4Address maskForPrefixLength(u8 prefixLength);

    /// Inverse of `maskForPrefixLength`; fails on non-contiguous masks such as
    /// 255.0.255.0, which is the classic typo this guards against.
    [[nodiscard]] static std::optional<u8> prefixLengthForMask(Ipv4Address mask);

    /// True when both addresses sit in the same prefix-length-sized network.
    [[nodiscard]] static bool isSameSubnet(Ipv4Address a, Ipv4Address b, u8 prefixLength);

    [[nodiscard]] constexpr Ipv4Address address() const { return address_; }
    [[nodiscard]] constexpr u8 prefixLength() const { return prefixLength_; }

    [[nodiscard]] Ipv4Address netmask() const { return maskForPrefixLength(prefixLength_); }
    [[nodiscard]] Ipv4Address wildcardMask() const { return ~netmask(); }

    [[nodiscard]] Ipv4Address networkAddress() const { return address_ & netmask(); }

    /// Highest address in the range. For /31 and /32 this is not a broadcast
    /// address in the protocol sense; see `hasBroadcastAddress()`.
    [[nodiscard]] Ipv4Address broadcastAddress() const { return networkAddress() | wildcardMask(); }
    [[nodiscard]] bool hasBroadcastAddress() const { return prefixLength_ <= 30; }

    [[nodiscard]] Ipv4Address firstUsableAddress() const;
    [[nodiscard]] Ipv4Address lastUsableAddress() const;

    /// Addresses assignable to hosts. /31 yields 2 (RFC 3021), /32 yields 1.
    [[nodiscard]] u64 usableHostCount() const;
    [[nodiscard]] u64 totalAddressCount() const;

    [[nodiscard]] bool contains(Ipv4Address address) const;
    [[nodiscard]] bool contains(const Ipv4Prefix& other) const;
    [[nodiscard]] bool overlaps(const Ipv4Prefix& other) const;

    /// True when `address` may be configured on an interface in this prefix:
    /// inside the range, and neither the network nor the broadcast address.
    [[nodiscard]] bool isUsableHostAddress(Ipv4Address address) const;

    /// Same prefix length, address replaced by the network address.
    [[nodiscard]] Ipv4Prefix network() const { return Ipv4Prefix{networkAddress(), prefixLength_}; }

    [[nodiscard]] bool isHostRoute() const { return prefixLength_ == 32; }
    [[nodiscard]] bool isPointToPoint() const { return prefixLength_ == 31; }
    [[nodiscard]] bool isDefaultRoute() const { return prefixLength_ == 0 && address_.isUnspecified(); }

    [[nodiscard]] std::string toString() const;          ///< "192.168.1.10/24"
    [[nodiscard]] std::string toNetworkString() const;   ///< "192.168.1.0/24"
    [[nodiscard]] std::string toMaskString() const;      ///< "192.168.1.10 255.255.255.0"

    auto operator<=>(const Ipv4Prefix&) const = default;
    bool operator==(const Ipv4Prefix&) const = default;

private:
    Ipv4Address address_{};
    u8 prefixLength_ = 0;
};

/// An IPv6 address with a prefix length: "2001:db8::1/64".
class Ipv6Prefix {
public:
    static constexpr u8 kMaxPrefixLength = 128;

    constexpr Ipv6Prefix() = default;
    Ipv6Prefix(Ipv6Address address, u8 prefixLength)
        : address_(address), prefixLength_(prefixLength > kMaxPrefixLength ? kMaxPrefixLength : prefixLength) {}

    [[nodiscard]] static std::optional<Ipv6Prefix> parse(std::string_view text);

    [[nodiscard]] const Ipv6Address& address() const { return address_; }
    [[nodiscard]] u8 prefixLength() const { return prefixLength_; }

    [[nodiscard]] Ipv6Address networkAddress() const { return address_.maskedTo(prefixLength_); }
    [[nodiscard]] Ipv6Prefix network() const { return Ipv6Prefix{networkAddress(), prefixLength_}; }

    [[nodiscard]] bool contains(const Ipv6Address& address) const;

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] std::string toNetworkString() const;

    auto operator<=>(const Ipv6Prefix&) const = default;
    bool operator==(const Ipv6Prefix&) const = default;

private:
    Ipv6Address address_{};
    u8 prefixLength_ = 0;
};

} // namespace tnp::core
