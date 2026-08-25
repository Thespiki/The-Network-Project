#pragma once

#include "utilities/Types.h"

#include <array>
#include <compare>
#include <cstring>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace tnp {

/// RFC 4122 version 4 identifier.
///
/// Every persisted entity in TNP (project, device, interface, link, test,
/// annotation) is keyed by a UUID rather than by an array index, so identities
/// survive serialization, undo/redo and copy/paste.
class Uuid {
public:
    using Bytes = std::array<u8, 16>;

    constexpr Uuid() = default;
    explicit constexpr Uuid(Bytes bytes) : bytes_(bytes) {}

    /// Cryptographically-irrelevant but well distributed random identifier.
    [[nodiscard]] static Uuid generate();

    /// Parses the canonical 8-4-4-4-12 hexadecimal form. Braces are tolerated.
    [[nodiscard]] static std::optional<Uuid> parse(std::string_view text);

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] bool isNil() const;

    [[nodiscard]] const Bytes& bytes() const { return bytes_; }

    auto operator<=>(const Uuid&) const = default;
    bool operator==(const Uuid&) const = default;

private:
    Bytes bytes_{};
};

} // namespace tnp

template <>
struct std::hash<tnp::Uuid> {
    std::size_t operator()(const tnp::Uuid& id) const noexcept {
        // The bytes are already uniformly distributed; fold them into size_t.
        std::uint64_t hi = 0;
        std::uint64_t lo = 0;
        std::memcpy(&hi, id.bytes().data(), 8);
        std::memcpy(&lo, id.bytes().data() + 8, 8);
        return static_cast<std::size_t>(hi ^ (lo * 0x9E3779B97F4A7C15ULL));
    }
};
