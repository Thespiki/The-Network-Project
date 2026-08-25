#pragma once

#include "utilities/Types.h"

#include <cstring>
#include <optional>
#include <span>
#include <string>

namespace tnp {

/// Appends fields to a byte buffer in network byte order (big endian).
///
/// Protocol encoders write real wire bytes through this type; nothing in TNP
/// invents a "pretend" packet representation.
class ByteWriter {
public:
    explicit ByteWriter(ByteBuffer& buffer) : buffer_(buffer) {}

    void u8v(u8 value) { buffer_.push_back(value); }

    void u16v(u16 value) {
        buffer_.push_back(static_cast<u8>(value >> 8));
        buffer_.push_back(static_cast<u8>(value & 0xFF));
    }

    void u32v(u32 value) {
        buffer_.push_back(static_cast<u8>(value >> 24));
        buffer_.push_back(static_cast<u8>((value >> 16) & 0xFF));
        buffer_.push_back(static_cast<u8>((value >> 8) & 0xFF));
        buffer_.push_back(static_cast<u8>(value & 0xFF));
    }

    void bytes(std::span<const u8> data) { buffer_.insert(buffer_.end(), data.begin(), data.end()); }
    void bytes(const ByteBuffer& data)   { buffer_.insert(buffer_.end(), data.begin(), data.end()); }

    void fill(u8 value, std::size_t count) { buffer_.insert(buffer_.end(), count, value); }

    /// Writes `text` and pads with NUL bytes up to `fieldWidth` (DHCP fields).
    void fixedString(const std::string& text, std::size_t fieldWidth);

    [[nodiscard]] std::size_t offset() const { return buffer_.size(); }

    /// Overwrites a previously written 16-bit field, e.g. a checksum computed
    /// after the rest of the header is known.
    void patchU16(std::size_t at, u16 value);

private:
    ByteBuffer& buffer_;
};

/// Bounds-checked sequential reader over wire bytes.
///
/// Every accessor returns `std::optional`; a truncated or malformed packet makes
/// decoding fail cleanly instead of reading past the buffer.
class ByteReader {
public:
    explicit ByteReader(std::span<const u8> data) : data_(data) {}
    explicit ByteReader(const ByteBuffer& data) : data_(data) {}

    [[nodiscard]] std::optional<u8>  u8v();
    [[nodiscard]] std::optional<u16> u16v();
    [[nodiscard]] std::optional<u32> u32v();

    [[nodiscard]] std::optional<std::span<const u8>> bytes(std::size_t count);

    [[nodiscard]] bool skip(std::size_t count);

    [[nodiscard]] std::size_t position() const { return position_; }
    [[nodiscard]] std::size_t remaining() const { return data_.size() - position_; }
    [[nodiscard]] bool exhausted() const { return position_ >= data_.size(); }

    /// Everything not consumed yet, without advancing.
    [[nodiscard]] std::span<const u8> rest() const { return data_.subspan(position_); }

private:
    std::span<const u8> data_;
    std::size_t position_ = 0;
};

/// The standard one's-complement Internet checksum (RFC 1071), used by IPv4,
/// ICMP, UDP and TCP.
[[nodiscard]] u16 internetChecksum(std::span<const u8> data, u32 initialSum = 0);

/// CRC-32 (IEEE 802.3 polynomial), used for `.tnp` container entry integrity.
[[nodiscard]] u32 crc32(std::span<const u8> data);

} // namespace tnp
