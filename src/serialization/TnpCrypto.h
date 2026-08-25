#pragma once

#include "utilities/Result.h"
#include "utilities/Types.h"

#include <string_view>

namespace tnp::serial {

/// Encrypted project containers (`.tnpenc`).
///
/// STATUS: **not implemented**. The functions below exist to fix the boundary -
/// encryption wraps a finished `.tnp` container and knows nothing about the
/// project model - and they return a clear failure rather than a weak
/// substitute.
///
/// The reason for the delay is deliberate. Doing this properly needs a vetted
/// authenticated-encryption implementation (an AEAD such as AES-256-GCM or
/// XChaCha20-Poly1305) and a memory-hard password KDF (Argon2id or scrypt).
/// TNP has no cryptographic dependency today, and inventing one - or shipping
/// something that merely looks encrypted - would be worse than shipping nothing:
/// a user would believe a project is protected when it is not.
///
/// The `.tnp` format also has to be stable first. Encrypting a container whose
/// layout is still moving would produce files no future build could open.
///
/// See docs/ROADMAP.md for the planned design.

/// Whether this build can read and write `.tnpenc` files. Always false for now;
/// callers should use it to hide or disable the feature rather than presenting
/// an option that fails.
[[nodiscard]] bool isEncryptionAvailable();

/// Would wrap a serialized `.tnp` container in an encrypted envelope.
[[nodiscard]] Result<ByteBuffer> encryptContainer(const ByteBuffer& container,
                                                  std::string_view passphrase);

/// Would unwrap a `.tnpenc` envelope back into a `.tnp` container.
[[nodiscard]] Result<ByteBuffer> decryptContainer(const ByteBuffer& envelope,
                                                  std::string_view passphrase);

/// Human-readable explanation of why encryption is unavailable, for the UI.
[[nodiscard]] std::string_view encryptionUnavailableReason();

} // namespace tnp::serial
