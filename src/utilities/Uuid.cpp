#include "utilities/Uuid.h"

#include <algorithm>
#include <cctype>
#include <random>

namespace tnp {
namespace {

std::mt19937_64& randomEngine() {
    // Seeded once per process from the platform entropy source. A single engine
    // keeps generation cheap; identifiers are not security sensitive.
    static thread_local std::mt19937_64 engine{std::random_device{}()};
    return engine;
}

constexpr char kHexDigits[] = "0123456789abcdef";

std::optional<u8> hexValue(char c) {
    if (c >= '0' && c <= '9') return static_cast<u8>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<u8>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<u8>(c - 'A' + 10);
    return std::nullopt;
}

} // namespace

Uuid Uuid::generate() {
    Bytes bytes{};
    const std::uint64_t a = randomEngine()();
    const std::uint64_t b = randomEngine()();
    std::memcpy(bytes.data(), &a, 8);
    std::memcpy(bytes.data() + 8, &b, 8);

    bytes[6] = static_cast<u8>((bytes[6] & 0x0F) | 0x40); // version 4
    bytes[8] = static_cast<u8>((bytes[8] & 0x3F) | 0x80); // RFC 4122 variant
    return Uuid{bytes};
}

std::optional<Uuid> Uuid::parse(std::string_view text) {
    if (!text.empty() && text.front() == '{' && text.back() == '}') {
        text = text.substr(1, text.size() - 2);
    }
    if (text.size() != 36) return std::nullopt;

    constexpr std::size_t kDashPositions[] = {8, 13, 18, 23};
    for (std::size_t pos : kDashPositions) {
        if (text[pos] != '-') return std::nullopt;
    }

    Bytes bytes{};
    std::size_t byteIndex = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '-') continue;
        const auto high = hexValue(text[i]);
        if (!high || i + 1 >= text.size()) return std::nullopt;
        const auto low = hexValue(text[i + 1]);
        if (!low) return std::nullopt;
        if (byteIndex >= bytes.size()) return std::nullopt;
        bytes[byteIndex++] = static_cast<u8>((*high << 4) | *low);
        ++i;
    }
    if (byteIndex != bytes.size()) return std::nullopt;
    return Uuid{bytes};
}

std::string Uuid::toString() const {
    std::string out;
    out.reserve(36);
    for (std::size_t i = 0; i < bytes_.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back('-');
        out.push_back(kHexDigits[bytes_[i] >> 4]);
        out.push_back(kHexDigits[bytes_[i] & 0x0F]);
    }
    return out;
}

bool Uuid::isNil() const {
    return std::all_of(bytes_.begin(), bytes_.end(), [](u8 b) { return b == 0; });
}

} // namespace tnp
