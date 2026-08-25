#pragma once

#include "utilities/Result.h"
#include "utilities/Types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::serial {

/// One named blob inside a `.tnp` file.
struct TnpEntry {
    std::string name;
    ByteBuffer data;
    /// Reserved: 0 means the bytes are stored as-is. No compression codec is
    /// implemented yet, and the field exists so adding one later does not
    /// require a new container version.
    u8 compression = 0;
};

/// The `.tnp` project container.
///
/// A small, explicitly versioned archive: a magic number, a container version, a
/// table of named entries with CRC-32 checksums, then the data. It exists so a
/// project can grow beyond a single JSON document - assets, per-section files,
/// future capture data - without changing what a `.tnp` file *is*.
///
/// Layout, all integers big-endian:
///
///     0   4   magic "TNPC"
///     4   2   container version
///     6   2   flags (reserved, zero)
///     8   4   entry count
///     12  ..  entry table, then the data blocks
///
/// Each table record is: name length (2), name, compression (1), stored length
/// (4), CRC-32 of the stored bytes (4), absolute offset of the data (4).
class TnpContainer {
public:
    static constexpr u16 kVersion = 1;

    /// Entry holding the complete project document, in `.tnpjson` form.
    static constexpr std::string_view kProjectEntry = "project.tnpjson";
    /// Entry describing the container itself.
    static constexpr std::string_view kManifestEntry = "manifest.json";

    void addEntry(std::string name, ByteBuffer data);
    void addTextEntry(std::string name, std::string_view text);

    [[nodiscard]] const ByteBuffer* find(std::string_view name) const;
    [[nodiscard]] std::optional<std::string> findText(std::string_view name) const;

    [[nodiscard]] const std::vector<TnpEntry>& entries() const { return entries_; }
    [[nodiscard]] bool empty() const { return entries_.empty(); }

    /// Encodes the container.
    [[nodiscard]] ByteBuffer serialize() const;

    /// Decodes a container, verifying the magic number, the version and every
    /// entry checksum. A file that fails any of those checks is reported rather
    /// than partially loaded.
    [[nodiscard]] static Result<TnpContainer> parse(const ByteBuffer& bytes);

    /// True when `bytes` starts with the container magic number. Used to tell a
    /// `.tnp` file from a `.tnpjson` one regardless of its extension.
    [[nodiscard]] static bool looksLikeContainer(const ByteBuffer& bytes);

private:
    std::vector<TnpEntry> entries_;
};

} // namespace tnp::serial
