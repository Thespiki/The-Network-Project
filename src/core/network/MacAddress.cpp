#include "core/network/MacAddress.h"

#include "utilities/StringUtilities.h"

#include <algorithm>
#include <format>
#include <random>

namespace tnp::core {
namespace {

std::optional<u8> hexValue(char c) {
    if (c >= '0' && c <= '9') return static_cast<u8>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<u8>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<u8>(c - 'A' + 10);
    return std::nullopt;
}

/// Collects hex digits while rejecting any other character, so "AA:BB::CC" and
/// "AA:BB:CC:DD:EE:FG" both fail rather than silently producing a wrong address.
std::optional<MacAddress::Bytes> parseDigits(std::string_view text, char separator, std::size_t groupSize) {
    MacAddress::Bytes bytes{};
    std::size_t digitIndex = 0;
    std::size_t sinceSeparator = 0;

    for (const char c : text) {
        if (c == separator) {
            if (sinceSeparator != groupSize) return std::nullopt;
            sinceSeparator = 0;
            continue;
        }
        const auto value = hexValue(c);
        if (!value) return std::nullopt;
        if (digitIndex >= MacAddress::kSize * 2) return std::nullopt;

        if (digitIndex % 2 == 0) bytes[digitIndex / 2] = static_cast<u8>(*value << 4);
        else                     bytes[digitIndex / 2] = static_cast<u8>(bytes[digitIndex / 2] | *value);
        ++digitIndex;
        ++sinceSeparator;
    }

    if (digitIndex != MacAddress::kSize * 2) return std::nullopt;
    if (sinceSeparator != groupSize) return std::nullopt;
    return bytes;
}

} // namespace

std::optional<MacAddress> MacAddress::parse(std::string_view text) {
    const std::string trimmed = strings::trim(text);
    if (trimmed.empty()) return std::nullopt;

    if (trimmed.find(':') != std::string::npos) {
        if (const auto bytes = parseDigits(trimmed, ':', 2)) return MacAddress{*bytes};
        return std::nullopt;
    }
    if (trimmed.find('-') != std::string::npos) {
        if (const auto bytes = parseDigits(trimmed, '-', 2)) return MacAddress{*bytes};
        return std::nullopt;
    }
    if (trimmed.find('.') != std::string::npos) {
        if (const auto bytes = parseDigits(trimmed, '.', 4)) return MacAddress{*bytes};
        return std::nullopt;
    }
    // Bare 12 hex digits.
    if (const auto bytes = parseDigits(trimmed, '\0', 12)) return MacAddress{*bytes};
    return std::nullopt;
}

std::optional<MacAddress> MacAddress::fromBytes(std::span<const u8> bytes) {
    if (bytes.size() < kSize) return std::nullopt;
    Bytes copy{};
    std::copy_n(bytes.begin(), kSize, copy.begin());
    return MacAddress{copy};
}

MacAddress MacAddress::broadcast() {
    return MacAddress{Bytes{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
}

MacAddress MacAddress::zero() { return MacAddress{}; }

MacAddress MacAddress::generateUnicast() {
    static thread_local std::mt19937 engine{std::random_device{}()};
    std::uniform_int_distribution<int> byteDistribution{0, 255};

    Bytes bytes{};
    for (auto& byte : bytes) byte = static_cast<u8>(byteDistribution(engine));
    bytes[0] = static_cast<u8>((bytes[0] & 0xFCu) | 0x02u); // locally administered, unicast
    return MacAddress{bytes};
}

std::string MacAddress::toString() const {
    return std::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                       bytes_[0], bytes_[1], bytes_[2], bytes_[3], bytes_[4], bytes_[5]);
}

std::string MacAddress::toCiscoString() const {
    return std::format("{:02x}{:02x}.{:02x}{:02x}.{:02x}{:02x}",
                       bytes_[0], bytes_[1], bytes_[2], bytes_[3], bytes_[4], bytes_[5]);
}

bool MacAddress::isBroadcast() const {
    return std::all_of(bytes_.begin(), bytes_.end(), [](u8 b) { return b == 0xFF; });
}

bool MacAddress::isZero() const {
    return std::all_of(bytes_.begin(), bytes_.end(), [](u8 b) { return b == 0x00; });
}

u32 MacAddress::oui() const {
    return (static_cast<u32>(bytes_[0]) << 16) | (static_cast<u32>(bytes_[1]) << 8) | bytes_[2];
}

} // namespace tnp::core
