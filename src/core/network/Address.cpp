#include "core/network/Address.h"

namespace tnp::core {

std::optional<IpAddress> IpAddress::parse(std::string_view text) {
    if (const auto v4Address = Ipv4Address::parse(text)) return IpAddress{*v4Address};
    if (const auto v6Address = Ipv6Address::parse(text)) return IpAddress{*v6Address};
    return std::nullopt;
}

std::string IpAddress::toString() const {
    if (const auto* address = std::get_if<Ipv4Address>(&value_)) return address->toString();
    return std::get<Ipv6Address>(value_).toString();
}

} // namespace tnp::core
