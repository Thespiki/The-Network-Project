#include "utilities/StringUtilities.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <sstream>

namespace tnp::strings {
namespace {

constexpr std::string_view kWhitespace = " \t\r\n\f\v";

char lowerChar(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
char upperChar(char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }

} // namespace

std::string trim(std::string_view text) {
    const auto begin = text.find_first_not_of(kWhitespace);
    if (begin == std::string_view::npos) return {};
    const auto end = text.find_last_not_of(kWhitespace);
    return std::string{text.substr(begin, end - begin + 1)};
}

std::string toLower(std::string_view text) {
    std::string out{text};
    std::transform(out.begin(), out.end(), out.begin(), lowerChar);
    return out;
}

std::string toUpper(std::string_view text) {
    std::string out{text};
    std::transform(out.begin(), out.end(), out.begin(), upperChar);
    return out;
}

std::vector<std::string> split(std::string_view text, char delimiter, bool skipEmpty) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const auto pos = text.find(delimiter, start);
        std::string_view field = (pos == std::string_view::npos)
                                     ? text.substr(start)
                                     : text.substr(start, pos - start);
        if (!skipEmpty || !field.empty()) parts.emplace_back(field);
        if (pos == std::string_view::npos) break;
        start = pos + 1;
    }
    return parts;
}

std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::size_t index = 0;
    while (index < text.size()) {
        while (index < text.size() && kWhitespace.find(text[index]) != std::string_view::npos) ++index;
        if (index >= text.size()) break;
        const std::size_t start = index;
        while (index < text.size() && kWhitespace.find(text[index]) == std::string_view::npos) ++index;
        tokens.emplace_back(text.substr(start, index - start));
    }
    return tokens;
}

std::string join(const std::vector<std::string>& parts, std::string_view separator) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out.append(separator);
        out.append(parts[i]);
    }
    return out;
}

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(),
                      [](char x, char y) { return lowerChar(x) == lowerChar(y); });
}

bool isAbbreviation(std::string_view prefix, std::string_view word) {
    if (prefix.empty() || prefix.size() > word.size()) return false;
    return std::equal(prefix.begin(), prefix.end(), word.begin(),
                      [](char x, char y) { return lowerChar(x) == lowerChar(y); });
}

std::optional<i64> parseInt(std::string_view text) {
    const std::string trimmed = trim(text);
    if (trimmed.empty()) return std::nullopt;
    i64 value = 0;
    const auto* first = trimmed.data();
    const auto* last  = trimmed.data() + trimmed.size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) return std::nullopt;
    return value;
}

std::optional<u32> parseUInt(std::string_view text) {
    const auto value = parseInt(text);
    if (!value || *value < 0 || *value > 0xFFFF'FFFFLL) return std::nullopt;
    return static_cast<u32>(*value);
}

std::optional<double> parseDouble(std::string_view text) {
    const std::string trimmed = trim(text);
    if (trimmed.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        const double value = std::stod(trimmed, &consumed);
        if (consumed != trimmed.size()) return std::nullopt;
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string hexDump(const ByteBuffer& bytes, std::size_t bytesPerLine) {
    if (bytesPerLine == 0) bytesPerLine = 16;
    std::ostringstream out;
    for (std::size_t offset = 0; offset < bytes.size(); offset += bytesPerLine) {
        out << std::format("{:04X}  ", offset);

        for (std::size_t i = 0; i < bytesPerLine; ++i) {
            if (offset + i < bytes.size()) out << std::format("{:02X} ", bytes[offset + i]);
            else                           out << "   ";
            if (i + 1 == bytesPerLine / 2) out << ' ';
        }

        out << " |";
        for (std::size_t i = 0; i < bytesPerLine && offset + i < bytes.size(); ++i) {
            const u8 byte = bytes[offset + i];
            out << (byte >= 0x20 && byte < 0x7F ? static_cast<char>(byte) : '.');
        }
        out << "|\n";
    }
    return out.str();
}

std::string toHex(const ByteBuffer& bytes) {
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const u8 byte : bytes) out += std::format("{:02x}", byte);
    return out;
}

std::string sanitizeFileName(std::string_view text) {
    static constexpr std::string_view kIllegal = R"(<>:"/\|?*)";
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        const bool illegal = kIllegal.find(c) != std::string_view::npos ||
                             static_cast<unsigned char>(c) < 0x20;
        out.push_back(illegal ? '_' : c);
    }
    return out.empty() ? std::string{"untitled"} : out;
}

std::string escapeXml(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out.push_back(c);
        }
    }
    return out;
}

} // namespace tnp::strings
