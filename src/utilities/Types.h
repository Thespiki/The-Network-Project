#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

/// Root namespace for The Network Project.
namespace tnp {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

/// Raw wire bytes. Every simulated packet is stored as a real byte buffer so
/// the inspector, the checksums and the decoders all operate on the same data
/// an actual NIC would see.
using ByteBuffer = std::vector<u8>;

} // namespace tnp
