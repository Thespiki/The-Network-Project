#include "serialization/TnpCrypto.h"

namespace tnp::serial {
namespace {

constexpr std::string_view kReason =
    "Encrypted projects (.tnpenc) are not implemented in this build. TNP will not ship a "
    "hand-rolled cipher; the feature needs a vetted authenticated-encryption library and a "
    "memory-hard key derivation function, and it is deliberately held back until the .tnp "
    "container format is stable.";

} // namespace

bool isEncryptionAvailable() { return false; }

std::string_view encryptionUnavailableReason() { return kReason; }

Result<ByteBuffer> encryptContainer(const ByteBuffer&, std::string_view) {
    return Result<ByteBuffer>::failure(std::string{kReason});
}

Result<ByteBuffer> decryptContainer(const ByteBuffer&, std::string_view) {
    return Result<ByteBuffer>::failure(std::string{kReason});
}

} // namespace tnp::serial
