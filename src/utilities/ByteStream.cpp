#include "utilities/ByteStream.h"

#include <array>

namespace tnp {

void ByteWriter::fixedString(const std::string& text, std::size_t fieldWidth) {
    const std::size_t copied = text.size() < fieldWidth ? text.size() : fieldWidth;
    for (std::size_t i = 0; i < copied; ++i) buffer_.push_back(static_cast<u8>(text[i]));
    buffer_.insert(buffer_.end(), fieldWidth - copied, 0u);
}

void ByteWriter::patchU16(std::size_t at, u16 value) {
    if (at + 1 >= buffer_.size()) return;
    buffer_[at]     = static_cast<u8>(value >> 8);
    buffer_[at + 1] = static_cast<u8>(value & 0xFF);
}

std::optional<u8> ByteReader::u8v() {
    if (remaining() < 1) return std::nullopt;
    return data_[position_++];
}

std::optional<u16> ByteReader::u16v() {
    if (remaining() < 2) return std::nullopt;
    const u16 value = static_cast<u16>((static_cast<u16>(data_[position_]) << 8) | data_[position_ + 1]);
    position_ += 2;
    return value;
}

std::optional<u32> ByteReader::u32v() {
    if (remaining() < 4) return std::nullopt;
    const u32 value = (static_cast<u32>(data_[position_]) << 24) |
                      (static_cast<u32>(data_[position_ + 1]) << 16) |
                      (static_cast<u32>(data_[position_ + 2]) << 8) |
                      static_cast<u32>(data_[position_ + 3]);
    position_ += 4;
    return value;
}

std::optional<std::span<const u8>> ByteReader::bytes(std::size_t count) {
    if (remaining() < count) return std::nullopt;
    const auto view = data_.subspan(position_, count);
    position_ += count;
    return view;
}

bool ByteReader::skip(std::size_t count) {
    if (remaining() < count) return false;
    position_ += count;
    return true;
}

u16 internetChecksum(std::span<const u8> data, u32 initialSum) {
    u32 sum = initialSum;
    std::size_t i = 0;
    for (; i + 1 < data.size(); i += 2) {
        sum += (static_cast<u32>(data[i]) << 8) | data[i + 1];
    }
    if (i < data.size()) sum += static_cast<u32>(data[i]) << 8; // odd trailing byte

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return static_cast<u16>(~sum & 0xFFFF);
}

namespace {

/// Table built once at first use; the container writer checksums whole entries.
const std::array<u32, 256>& crcTable() {
    static const std::array<u32, 256> table = [] {
        std::array<u32, 256> result{};
        constexpr u32 kPolynomial = 0xEDB88320u;
        for (u32 i = 0; i < 256; ++i) {
            u32 value = i;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1u) ? (kPolynomial ^ (value >> 1)) : (value >> 1);
            }
            result[i] = value;
        }
        return result;
    }();
    return table;
}

} // namespace

u32 crc32(std::span<const u8> data) {
    const auto& table = crcTable();
    u32 crc = 0xFFFFFFFFu;
    for (const u8 byte : data) {
        crc = table[(crc ^ byte) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

} // namespace tnp
