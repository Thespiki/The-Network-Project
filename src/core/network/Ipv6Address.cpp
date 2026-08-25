#include "core/network/Ipv6Address.h"

#include "utilities/StringUtilities.h"

#include <algorithm>
#include <format>

namespace tnp::core {
namespace {

constexpr std::size_t kGroupCount = 8;

std::optional<u16> parseGroup(std::string_view text) {
    if (text.empty() || text.size() > 4) return std::nullopt;
    u16 value = 0;
    for (const char c : text) {
        u16 digit = 0;
        if (c >= '0' && c <= '9')      digit = static_cast<u16>(c - '0');
        else if (c >= 'a' && c <= 'f') digit = static_cast<u16>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = static_cast<u16>(c - 'A' + 10);
        else return std::nullopt;
        value = static_cast<u16>((value << 4) | digit);
    }
    return value;
}

/// Splits "a:b:c" into groups, expanding a trailing dotted-quad into two groups.
/// Returns nullopt when any group is malformed.
std::optional<std::vector<u16>> parseGroupList(std::string_view text) {
    std::vector<u16> groups;
    if (text.empty()) return groups;

    for (const auto& part : strings::split(text, ':')) {
        if (part.find('.') != std::string::npos) {
            // Only legal as the final element.
            const auto embedded = Ipv4Address::parse(part);
            if (!embedded) return std::nullopt;
            groups.push_back(static_cast<u16>((embedded->octet(0) << 8) | embedded->octet(1)));
            groups.push_back(static_cast<u16>((embedded->octet(2) << 8) | embedded->octet(3)));
            continue;
        }
        const auto group = parseGroup(part);
        if (!group) return std::nullopt;
        groups.push_back(*group);
    }
    return groups;
}

} // namespace

std::optional<Ipv6Address> Ipv6Address::parse(std::string_view text) {
    const std::string trimmed = strings::trim(text);
    if (trimmed.empty() || trimmed.size() > 45) return std::nullopt;
    if (trimmed.find('%') != std::string::npos) return std::nullopt; // zone indices unsupported

    const auto compression = trimmed.find("::");
    std::vector<u16> groups;

    if (compression == std::string::npos) {
        const auto parsed = parseGroupList(trimmed);
        if (!parsed || parsed->size() != kGroupCount) return std::nullopt;
        groups = *parsed;
    } else {
        // Only one "::" is legal.
        if (trimmed.find("::", compression + 1) != std::string::npos) return std::nullopt;

        const auto head = parseGroupList(trimmed.substr(0, compression));
        const auto tail = parseGroupList(trimmed.substr(compression + 2));
        if (!head || !tail) return std::nullopt;
        if (head->size() + tail->size() >= kGroupCount) return std::nullopt;

        groups = *head;
        groups.resize(kGroupCount - tail->size(), 0);
        groups.insert(groups.end(), tail->begin(), tail->end());
    }

    Bytes bytes{};
    for (std::size_t i = 0; i < kGroupCount; ++i) {
        bytes[i * 2]     = static_cast<u8>(groups[i] >> 8);
        bytes[i * 2 + 1] = static_cast<u8>(groups[i] & 0xFFu);
    }
    return Ipv6Address{bytes};
}

std::optional<Ipv6Address> Ipv6Address::fromBytes(std::span<const u8> bytes) {
    if (bytes.size() < kSize) return std::nullopt;
    Bytes copy{};
    std::copy_n(bytes.begin(), kSize, copy.begin());
    return Ipv6Address{copy};
}

Ipv6Address Ipv6Address::loopback() {
    Bytes bytes{};
    bytes[15] = 1;
    return Ipv6Address{bytes};
}

Ipv6Address Ipv6Address::linkLocalFromInterfaceId(u64 interfaceId) {
    Bytes bytes{};
    bytes[0] = 0xFE;
    bytes[1] = 0x80;
    for (std::size_t i = 0; i < 8; ++i) {
        bytes[15 - i] = static_cast<u8>((interfaceId >> (8 * i)) & 0xFFu);
    }
    return Ipv6Address{bytes};
}

u16 Ipv6Address::group(std::size_t index) const {
    if (index >= kGroupCount) return 0;
    return static_cast<u16>((static_cast<u16>(bytes_[index * 2]) << 8) | bytes_[index * 2 + 1]);
}

std::string Ipv6Address::toString() const {
    // Find the longest run of zero groups; RFC 5952 only compresses runs of two
    // or more, and only the first such longest run.
    std::size_t bestStart = kGroupCount;
    std::size_t bestLength = 0;
    std::size_t runStart = 0;
    std::size_t runLength = 0;

    for (std::size_t i = 0; i < kGroupCount; ++i) {
        if (group(i) == 0) {
            if (runLength == 0) runStart = i;
            ++runLength;
            if (runLength > bestLength) {
                bestLength = runLength;
                bestStart = runStart;
            }
        } else {
            runLength = 0;
        }
    }
    if (bestLength < 2) bestStart = kGroupCount;

    std::string out;
    for (std::size_t i = 0; i < kGroupCount;) {
        if (i == bestStart) {
            // The compressed run supplies both of its surrounding colons.
            out += "::";
            i += bestLength;
            continue;
        }
        if (!out.empty() && !strings::endsWith(out, "::")) out += ':';
        out += std::format("{:x}", group(i));
        ++i;
    }
    return out.empty() ? "::" : out;
}

std::string Ipv6Address::toExpandedString() const {
    std::string out;
    for (std::size_t i = 0; i < kGroupCount; ++i) {
        if (i != 0) out += ':';
        out += std::format("{:04x}", group(i));
    }
    return out;
}

bool Ipv6Address::isUnspecified() const {
    return std::all_of(bytes_.begin(), bytes_.end(), [](u8 b) { return b == 0; });
}

bool Ipv6Address::isLoopback() const { return *this == loopback(); }

Ipv6Address Ipv6Address::maskedTo(u8 prefixLength) const {
    if (prefixLength >= 128) return *this;
    Bytes masked{};
    const std::size_t fullBytes = prefixLength / 8;
    const std::size_t spareBits = prefixLength % 8;

    for (std::size_t i = 0; i < fullBytes; ++i) masked[i] = bytes_[i];
    if (spareBits != 0 && fullBytes < kSize) {
        const u8 mask = static_cast<u8>(0xFFu << (8 - spareBits));
        masked[fullBytes] = static_cast<u8>(bytes_[fullBytes] & mask);
    }
    return Ipv6Address{masked};
}

} // namespace tnp::core
