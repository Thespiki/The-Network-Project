#pragma once

/// Convenience aggregate header for the addressing types, plus a small variant
/// used where either address family is acceptable (DNS answers, CLI arguments).

#include "core/network/Ipv4Address.h"
#include "core/network/Ipv6Address.h"
#include "core/network/MacAddress.h"
#include "core/network/Subnet.h"

#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace tnp::core {

/// Either an IPv4 or an IPv6 address.
class IpAddress {
public:
    IpAddress() = default;
    IpAddress(Ipv4Address address) : value_(address) {}  // NOLINT(google-explicit-constructor)
    IpAddress(Ipv6Address address) : value_(address) {}  // NOLINT(google-explicit-constructor)

    /// Tries IPv4 first, then IPv6.
    [[nodiscard]] static std::optional<IpAddress> parse(std::string_view text);

    [[nodiscard]] bool isV4() const { return std::holds_alternative<Ipv4Address>(value_); }
    [[nodiscard]] bool isV6() const { return std::holds_alternative<Ipv6Address>(value_); }

    [[nodiscard]] std::optional<Ipv4Address> v4() const {
        if (const auto* address = std::get_if<Ipv4Address>(&value_)) return *address;
        return std::nullopt;
    }
    [[nodiscard]] std::optional<Ipv6Address> v6() const {
        if (const auto* address = std::get_if<Ipv6Address>(&value_)) return *address;
        return std::nullopt;
    }

    [[nodiscard]] std::string toString() const;

    bool operator==(const IpAddress&) const = default;

private:
    std::variant<Ipv4Address, Ipv6Address> value_{Ipv4Address{}};
};

} // namespace tnp::core
