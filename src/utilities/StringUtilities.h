#pragma once

#include "utilities/Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::strings {

[[nodiscard]] std::string trim(std::string_view text);
[[nodiscard]] std::string toLower(std::string_view text);
[[nodiscard]] std::string toUpper(std::string_view text);

/// Splits on a single character. Empty fields are kept unless `skipEmpty`.
[[nodiscard]] std::vector<std::string> split(std::string_view text, char delimiter, bool skipEmpty = false);

/// Splits on runs of whitespace, dropping empty fields. Used by the device CLI.
[[nodiscard]] std::vector<std::string> tokenize(std::string_view text);

[[nodiscard]] std::string join(const std::vector<std::string>& parts, std::string_view separator);

[[nodiscard]] bool startsWith(std::string_view text, std::string_view prefix);
[[nodiscard]] bool endsWith(std::string_view text, std::string_view suffix);
[[nodiscard]] bool equalsIgnoreCase(std::string_view a, std::string_view b);

/// True when `prefix` is a non-empty prefix of `word`, ignoring case. Enables
/// the abbreviated commands network engineers expect ("sh ip ro").
[[nodiscard]] bool isAbbreviation(std::string_view prefix, std::string_view word);

[[nodiscard]] std::optional<i64> parseInt(std::string_view text);
[[nodiscard]] std::optional<u32> parseUInt(std::string_view text);
[[nodiscard]] std::optional<double> parseDouble(std::string_view text);

/// Hexadecimal dump in the classic "offset  bytes  ascii" layout, used by the
/// packet inspector's raw view.
[[nodiscard]] std::string hexDump(const ByteBuffer& bytes, std::size_t bytesPerLine = 16);

/// Lowercase hex without separators.
[[nodiscard]] std::string toHex(const ByteBuffer& bytes);

/// Replaces characters that are illegal in file names with '_'.
[[nodiscard]] std::string sanitizeFileName(std::string_view text);

/// Escapes '&', '<', '>', '"' for SVG/XML text nodes.
[[nodiscard]] std::string escapeXml(std::string_view text);

} // namespace tnp::strings
