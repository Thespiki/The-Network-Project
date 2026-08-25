#pragma once

// Internal helpers for the serialization module.
//
// This header pulls in nlohmann/json and is deliberately *not* part of the
// module's public interface: `ProjectSerializer` exposes strings and domain
// objects only, so no other layer of TNP acquires a dependency on the JSON
// library or on its compile time.

#include "core/network/Ids.h"
#include "core/network/MacAddress.h"
#include "core/network/Subnet.h"
#include "utilities/Geometry.h"
#include "utilities/Time.h"
#include "utilities/Types.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::serial {

using Json = nlohmann::json;

/// Collects problems found while reading a document.
///
/// Reading never throws and never stops at the first mistake: a project with one
/// bad address should still open, with the problem reported, rather than
/// refusing to load at all.
class ParseReport {
public:
    void error(std::string path, std::string message);
    void warning(std::string path, std::string message);

    [[nodiscard]] bool hasErrors() const { return !errors_.empty(); }
    [[nodiscard]] const std::vector<std::string>& errors() const { return errors_; }
    [[nodiscard]] const std::vector<std::string>& warnings() const { return warnings_; }

    /// All problems as one multi-line string, for the log and error dialogs.
    [[nodiscard]] std::string summary() const;

private:
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
};

// --- Safe scalar accessors -------------------------------------------------
// Each returns the fallback when the key is missing or has the wrong type, and
// notes a warning in the report when the type is wrong (a missing optional key
// is not a problem).

[[nodiscard]] std::string readString(const Json& node, std::string_view key, ParseReport& report,
                                     std::string_view path, std::string fallback = {});
[[nodiscard]] i64 readInt(const Json& node, std::string_view key, ParseReport& report,
                          std::string_view path, i64 fallback = 0);
[[nodiscard]] u32 readUInt(const Json& node, std::string_view key, ParseReport& report,
                           std::string_view path, u32 fallback = 0);
[[nodiscard]] double readDouble(const Json& node, std::string_view key, ParseReport& report,
                                std::string_view path, double fallback = 0.0);
[[nodiscard]] bool readBool(const Json& node, std::string_view key, ParseReport& report,
                            std::string_view path, bool fallback = false);

/// Returns the child array, or an empty array when absent or of the wrong type.
[[nodiscard]] const Json& readArray(const Json& node, std::string_view key, ParseReport& report,
                                    std::string_view path);
[[nodiscard]] const Json& readObject(const Json& node, std::string_view key, ParseReport& report,
                                     std::string_view path);

// --- Domain types ----------------------------------------------------------

[[nodiscard]] Json writeUuid(const Uuid& value);

/// Reads an identifier. A missing or malformed value yields a fresh one and a
/// warning, so a hand-edited file still opens with usable identities.
template <typename Tag>
[[nodiscard]] Id<Tag> readId(const Json& node, std::string_view key, ParseReport& report,
                             std::string_view path) {
    const std::string text = readString(node, key, report, path);
    if (text.empty()) {
        report.warning(std::string{path} + "." + std::string{key}, "missing identifier; a new one was generated");
        return Id<Tag>::generate();
    }
    if (const auto parsed = Id<Tag>::parse(text)) return *parsed;

    report.warning(std::string{path} + "." + std::string{key},
                   "'" + text + "' is not a valid identifier; a new one was generated");
    return Id<Tag>::generate();
}

/// Reads an optional identifier: absence is legitimate, not a problem.
template <typename Tag>
[[nodiscard]] Id<Tag> readOptionalId(const Json& node, std::string_view key) {
    if (!node.is_object() || !node.contains(key)) return Id<Tag>{};
    const auto& value = node.at(std::string{key});
    if (!value.is_string()) return Id<Tag>{};
    return Id<Tag>::parse(value.get<std::string>()).value_or(Id<Tag>{});
}

[[nodiscard]] std::optional<core::MacAddress> readMac(const Json& node, std::string_view key,
                                                      ParseReport& report, std::string_view path);
[[nodiscard]] std::optional<core::Ipv4Address> readIpv4(const Json& node, std::string_view key,
                                                        ParseReport& report, std::string_view path);
[[nodiscard]] std::optional<core::Ipv4Prefix> readIpv4Prefix(const Json& node, std::string_view key,
                                                             ParseReport& report, std::string_view path);

[[nodiscard]] Vec2 readVec2(const Json& node, std::string_view key, ParseReport& report,
                            std::string_view path);
[[nodiscard]] Json writeVec2(const Vec2& value);

[[nodiscard]] Duration readDuration(const Json& node, std::string_view key, ParseReport& report,
                                    std::string_view path, Duration fallback);
[[nodiscard]] Json writeDuration(Duration value);

} // namespace tnp::serial
