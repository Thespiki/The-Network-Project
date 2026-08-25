#pragma once

#include "utilities/Result.h"
#include "utilities/Types.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace tnp::files {

/// Per-user configuration directory for TNP, created on demand.
///
///   Windows  %APPDATA%\TNP
///   macOS    ~/Library/Application Support/TNP
///   Linux    $XDG_CONFIG_HOME/tnp  (or ~/.config/tnp)
[[nodiscard]] std::filesystem::path userConfigDirectory();

/// Directory holding autosaves and crash-recovery snapshots.
[[nodiscard]] std::filesystem::path userStateDirectory();

[[nodiscard]] Result<std::string> readTextFile(const std::filesystem::path& path);
[[nodiscard]] Result<ByteBuffer>  readBinaryFile(const std::filesystem::path& path);

/// Writes via a temporary sibling file followed by an atomic rename.
///
/// This is what keeps a crash (or a full disk) during save from destroying the
/// user's existing project: the original file is only replaced once the new
/// content is completely on disk.
[[nodiscard]] Status writeTextFileAtomic(const std::filesystem::path& path, std::string_view content);
[[nodiscard]] Status writeBinaryFileAtomic(const std::filesystem::path& path, const ByteBuffer& content);

/// Lower-case extension including the dot, e.g. ".tnp".
[[nodiscard]] std::string extensionOf(const std::filesystem::path& path);

} // namespace tnp::files
