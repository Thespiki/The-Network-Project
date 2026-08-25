#include "core/network/Ipv4Address.h"

#include "utilities/StringUtilities.h"

#include <format>

namespace tnp::core {

std::optional<Ipv4Address> Ipv4Address::parse(std::string_view text) {
    const std::string trimmed = strings::trim(text);
    if (trimmed.empty() || trimmed.size() > 15) return std::nullopt;

    u32 value = 0;
    int octetCount = 0;
    std::size_t index = 0;

    while (index <= trimmed.size()) {
        // Read one decimal group.
        std::size_t digits = 0;
        u32 octet = 0;
        while (index < trimmed.size() && trimmed[index] >= '0' && trimmed[index] <= '9') {
            octet = octet * 10 + static_cast<u32>(trimmed[index] - '0');
            if (++digits > 3 || octet > 255) return std::nullopt;
            ++index;
        }
        if (digits == 0) return std::nullopt;
        // "01" or "007" are rejected: they invite base-8 confusion.
        if (digits > 1 && trimmed[index - digits] == '0') return std::nullopt;

        value = (value << 8) | octet;
        if (++octetCount > 4) return std::nullopt;

        if (index == trimmed.size()) break;
        if (trimmed[index] != '.') return std::nullopt;
        ++index;
    }

    if (octetCount != 4) return std::nullopt;
    return Ipv4Address{value};
}

std::optional<Ipv4Address> Ipv4Address::fromBytes(std::span<const u8> bytes) {
    if (bytes.size() < 4) return std::nullopt;
    return Ipv4Address{bytes[0], bytes[1], bytes[2], bytes[3]};
}

std::string Ipv4Address::toString() const {
    return std::format("{}.{}.{}.{}", octet(0), octet(1), octet(2), octet(3));
}

std::array<u8, 4> Ipv4Address::toBytes() const {
    return {octet(0), octet(1), octet(2), octet(3)};
}

} // namespace tnp::core
