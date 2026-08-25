#include "core/network/Subnet.h"

#include "utilities/StringUtilities.h"

#include <bit>
#include <format>

namespace tnp::core {

// --- Ipv4Prefix ------------------------------------------------------------

std::optional<Ipv4Prefix> Ipv4Prefix::parse(std::string_view text) {
    const std::string trimmed = strings::trim(text);
    const auto slash = trimmed.find('/');
    if (slash == std::string::npos) return std::nullopt;

    const auto address = Ipv4Address::parse(trimmed.substr(0, slash));
    if (!address) return std::nullopt;

    const auto length = strings::parseInt(trimmed.substr(slash + 1));
    if (!length || *length < 0 || *length > kMaxPrefixLength) return std::nullopt;

    return Ipv4Prefix{*address, static_cast<u8>(*length)};
}

std::optional<Ipv4Prefix> Ipv4Prefix::parseWithMask(std::string_view address, std::string_view mask) {
    const auto parsedAddress = Ipv4Address::parse(address);
    const auto parsedMask = Ipv4Address::parse(mask);
    if (!parsedAddress || !parsedMask) return std::nullopt;

    const auto length = prefixLengthForMask(*parsedMask);
    if (!length) return std::nullopt;

    return Ipv4Prefix{*parsedAddress, *length};
}

Ipv4Address Ipv4Prefix::maskForPrefixLength(u8 prefixLength) {
    // Shifting a 32-bit value by 32 is undefined, so /0 is handled separately.
    if (prefixLength == 0) return Ipv4Address{0u};
    if (prefixLength >= kMaxPrefixLength) return Ipv4Address{0xFFFFFFFFu};
    return Ipv4Address{static_cast<u32>(0xFFFFFFFFu << (kMaxPrefixLength - prefixLength))};
}

std::optional<u8> Ipv4Prefix::prefixLengthForMask(Ipv4Address mask) {
    const u32 value = mask.value();
    if (value == 0) return u8{0};

    // A valid mask is a run of ones followed by a run of zeros: inverting it
    // must produce a value whose bits are all contiguous from the bottom.
    const u32 inverted = ~value;
    if ((inverted & (inverted + 1)) != 0) return std::nullopt;

    return static_cast<u8>(std::popcount(value));
}

bool Ipv4Prefix::isSameSubnet(Ipv4Address a, Ipv4Address b, u8 prefixLength) {
    const Ipv4Address mask = maskForPrefixLength(prefixLength);
    return (a & mask) == (b & mask);
}

Ipv4Address Ipv4Prefix::firstUsableAddress() const {
    if (prefixLength_ >= 31) return networkAddress(); // /31 and /32 have no reserved network address
    return networkAddress().next();
}

Ipv4Address Ipv4Prefix::lastUsableAddress() const {
    if (prefixLength_ == 32) return networkAddress();
    if (prefixLength_ == 31) return broadcastAddress();
    return broadcastAddress().previous();
}

u64 Ipv4Prefix::totalAddressCount() const {
    return u64{1} << (kMaxPrefixLength - prefixLength_);
}

u64 Ipv4Prefix::usableHostCount() const {
    if (prefixLength_ == 32) return 1;
    if (prefixLength_ == 31) return 2; // RFC 3021 point-to-point link
    return totalAddressCount() - 2;
}

bool Ipv4Prefix::contains(Ipv4Address address) const {
    return (address & netmask()) == networkAddress();
}

bool Ipv4Prefix::contains(const Ipv4Prefix& other) const {
    if (other.prefixLength_ < prefixLength_) return false;
    return contains(other.networkAddress());
}

bool Ipv4Prefix::overlaps(const Ipv4Prefix& other) const {
    return contains(other) || other.contains(*this);
}

bool Ipv4Prefix::isUsableHostAddress(Ipv4Address address) const {
    if (!contains(address)) return false;
    if (!address.isAssignableToHost()) return false;
    if (prefixLength_ >= 31) return true;
    return address != networkAddress() && address != broadcastAddress();
}

std::string Ipv4Prefix::toString() const {
    return std::format("{}/{}", address_.toString(), prefixLength_);
}

std::string Ipv4Prefix::toNetworkString() const {
    return std::format("{}/{}", networkAddress().toString(), prefixLength_);
}

std::string Ipv4Prefix::toMaskString() const {
    return std::format("{} {}", address_.toString(), netmask().toString());
}

// --- Ipv6Prefix ------------------------------------------------------------

std::optional<Ipv6Prefix> Ipv6Prefix::parse(std::string_view text) {
    const std::string trimmed = strings::trim(text);
    const auto slash = trimmed.find('/');
    if (slash == std::string::npos) return std::nullopt;

    const auto address = Ipv6Address::parse(trimmed.substr(0, slash));
    if (!address) return std::nullopt;

    const auto length = strings::parseInt(trimmed.substr(slash + 1));
    if (!length || *length < 0 || *length > kMaxPrefixLength) return std::nullopt;

    return Ipv6Prefix{*address, static_cast<u8>(*length)};
}

bool Ipv6Prefix::contains(const Ipv6Address& address) const {
    return address.maskedTo(prefixLength_) == networkAddress();
}

std::string Ipv6Prefix::toString() const {
    return std::format("{}/{}", address_.toString(), prefixLength_);
}

std::string Ipv6Prefix::toNetworkString() const {
    return std::format("{}/{}", networkAddress().toString(), prefixLength_);
}

} // namespace tnp::core
