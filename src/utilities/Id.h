#pragma once

#include "utilities/Uuid.h"

#include <compare>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace tnp {

/// Phantom-typed wrapper around `Uuid`.
///
/// `Id<DeviceTag>` and `Id<LinkTag>` are distinct types, so the compiler rejects
/// passing a link identifier where a device identifier is expected. This has
/// caught more bugs than it costs: identifiers are passed through every layer of
/// the application.
template <typename Tag>
class Id {
public:
    constexpr Id() = default;
    explicit constexpr Id(Uuid value) : value_(value) {}

    [[nodiscard]] static Id generate() { return Id{Uuid::generate()}; }

    [[nodiscard]] static std::optional<Id> parse(std::string_view text) {
        if (const auto uuid = Uuid::parse(text)) return Id{*uuid};
        return std::nullopt;
    }

    [[nodiscard]] const Uuid& uuid() const { return value_; }
    [[nodiscard]] std::string toString() const { return value_.toString(); }

    /// Short form used in logs and labels, e.g. "a3f19c42".
    [[nodiscard]] std::string toShortString() const { return value_.toString().substr(0, 8); }

    [[nodiscard]] bool isValid() const { return !value_.isNil(); }
    explicit operator bool() const { return isValid(); }

    auto operator<=>(const Id&) const = default;
    bool operator==(const Id&) const = default;

private:
    Uuid value_{};
};

} // namespace tnp

template <typename Tag>
struct std::hash<tnp::Id<Tag>> {
    std::size_t operator()(const tnp::Id<Tag>& id) const noexcept {
        return std::hash<tnp::Uuid>{}(id.uuid());
    }
};
