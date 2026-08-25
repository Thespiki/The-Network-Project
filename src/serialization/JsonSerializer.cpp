#include "serialization/JsonSerializer.h"

#include <format>

namespace tnp::serial {
namespace {

const Json& emptyArray() {
    static const Json value = Json::array();
    return value;
}

const Json& emptyObject() {
    static const Json value = Json::object();
    return value;
}

std::string join(std::string_view path, std::string_view key) {
    return std::format("{}.{}", path, key);
}

/// Reports a type mismatch. A key that is simply absent is not reported: most
/// fields are optional and defaulting them is the intended behaviour.
void noteWrongType(ParseReport& report, std::string_view path, std::string_view key,
                   std::string_view expected) {
    report.warning(join(path, key), std::format("expected {}; the default was used", expected));
}

} // namespace

void ParseReport::error(std::string path, std::string message) {
    errors_.push_back(std::format("{}: {}", path, message));
}

void ParseReport::warning(std::string path, std::string message) {
    warnings_.push_back(std::format("{}: {}", path, message));
}

std::string ParseReport::summary() const {
    std::string text;
    for (const std::string& issue : errors_) text += "error: " + issue + "\n";
    for (const std::string& issue : warnings_) text += "warning: " + issue + "\n";
    return text;
}

std::string readString(const Json& node, std::string_view key, ParseReport& report,
                       std::string_view path, std::string fallback) {
    if (!node.is_object() || !node.contains(key)) return fallback;
    const auto& value = node.at(std::string{key});
    if (value.is_null()) return fallback;
    if (!value.is_string()) {
        noteWrongType(report, path, key, "a string");
        return fallback;
    }
    return value.get<std::string>();
}

i64 readInt(const Json& node, std::string_view key, ParseReport& report, std::string_view path,
            i64 fallback) {
    if (!node.is_object() || !node.contains(key)) return fallback;
    const auto& value = node.at(std::string{key});
    if (value.is_null()) return fallback;
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        noteWrongType(report, path, key, "an integer");
        return fallback;
    }
    return value.get<i64>();
}

u32 readUInt(const Json& node, std::string_view key, ParseReport& report, std::string_view path,
             u32 fallback) {
    const i64 value = readInt(node, key, report, path, static_cast<i64>(fallback));
    if (value < 0 || value > 0xFFFF'FFFFLL) {
        report.warning(join(path, key), "value out of range; the default was used");
        return fallback;
    }
    return static_cast<u32>(value);
}

double readDouble(const Json& node, std::string_view key, ParseReport& report, std::string_view path,
                  double fallback) {
    if (!node.is_object() || !node.contains(key)) return fallback;
    const auto& value = node.at(std::string{key});
    if (value.is_null()) return fallback;
    if (!value.is_number()) {
        noteWrongType(report, path, key, "a number");
        return fallback;
    }
    return value.get<double>();
}

bool readBool(const Json& node, std::string_view key, ParseReport& report, std::string_view path,
              bool fallback) {
    if (!node.is_object() || !node.contains(key)) return fallback;
    const auto& value = node.at(std::string{key});
    if (value.is_null()) return fallback;
    if (!value.is_boolean()) {
        noteWrongType(report, path, key, "a boolean");
        return fallback;
    }
    return value.get<bool>();
}

const Json& readArray(const Json& node, std::string_view key, ParseReport& report,
                      std::string_view path) {
    if (!node.is_object() || !node.contains(key)) return emptyArray();
    const auto& value = node.at(std::string{key});
    if (value.is_null()) return emptyArray();
    if (!value.is_array()) {
        noteWrongType(report, path, key, "an array");
        return emptyArray();
    }
    return value;
}

const Json& readObject(const Json& node, std::string_view key, ParseReport& report,
                       std::string_view path) {
    if (!node.is_object() || !node.contains(key)) return emptyObject();
    const auto& value = node.at(std::string{key});
    if (value.is_null()) return emptyObject();
    if (!value.is_object()) {
        noteWrongType(report, path, key, "an object");
        return emptyObject();
    }
    return value;
}

Json writeUuid(const Uuid& value) { return value.toString(); }

std::optional<core::MacAddress> readMac(const Json& node, std::string_view key, ParseReport& report,
                                        std::string_view path) {
    const std::string text = readString(node, key, report, path);
    if (text.empty()) return std::nullopt;

    if (const auto mac = core::MacAddress::parse(text)) return mac;
    report.warning(join(path, key), std::format("'{}' is not a MAC address", text));
    return std::nullopt;
}

std::optional<core::Ipv4Address> readIpv4(const Json& node, std::string_view key, ParseReport& report,
                                          std::string_view path) {
    const std::string text = readString(node, key, report, path);
    if (text.empty()) return std::nullopt;

    if (const auto address = core::Ipv4Address::parse(text)) return address;
    report.warning(join(path, key), std::format("'{}' is not an IPv4 address", text));
    return std::nullopt;
}

std::optional<core::Ipv4Prefix> readIpv4Prefix(const Json& node, std::string_view key,
                                               ParseReport& report, std::string_view path) {
    const std::string text = readString(node, key, report, path);
    if (text.empty()) return std::nullopt;

    if (const auto prefix = core::Ipv4Prefix::parse(text)) return prefix;
    report.warning(join(path, key), std::format("'{}' is not an IPv4 prefix in CIDR notation", text));
    return std::nullopt;
}

Vec2 readVec2(const Json& node, std::string_view key, ParseReport& report, std::string_view path) {
    const Json& value = readObject(node, key, report, path);
    const std::string childPath = join(path, key);
    return Vec2{static_cast<float>(readDouble(value, "x", report, childPath)),
                static_cast<float>(readDouble(value, "y", report, childPath))};
}

Json writeVec2(const Vec2& value) {
    return Json{{"x", value.x}, {"y", value.y}};
}

Duration readDuration(const Json& node, std::string_view key, ParseReport& report,
                      std::string_view path, Duration fallback) {
    const i64 nanos = readInt(node, key, report, path, fallback.count());
    if (nanos < 0) {
        report.warning(join(path, key), "a duration cannot be negative; the default was used");
        return fallback;
    }
    return Duration{nanos};
}

Json writeDuration(Duration value) { return value.count(); }

} // namespace tnp::serial
