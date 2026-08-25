#pragma once

#include "core/project/Project.h"
#include "serialization/ProjectSerializer.h"

#include <filesystem>

namespace tnp::app {

/// On-disk shapes a project can take.
enum class ProjectFormat {
    Unknown,
    Json,      ///< .tnpjson - a readable document, used for development and export
    Container, ///< .tnp - the versioned project container
    Encrypted  ///< .tnpenc - reserved; not implemented
};

[[nodiscard]] ProjectFormat formatForPath(const std::filesystem::path& path);
[[nodiscard]] std::string_view projectFormatName(ProjectFormat format);

/// Default extension of the format TNP saves in.
inline constexpr std::string_view kProjectExtension = ".tnp";
inline constexpr std::string_view kJsonProjectExtension = ".tnpjson";
inline constexpr std::string_view kEncryptedProjectExtension = ".tnpenc";

/// Reads a project from disk.
///
/// The format is decided by the file's content where possible - the container
/// has a magic number - and by its extension otherwise, so a `.tnp` file that
/// was renamed still opens.
[[nodiscard]] Result<serial::LoadReport> loadProject(const std::filesystem::path& path,
                                                     core::Project& project);

/// Writes a project. The write is atomic: the existing file is only replaced
/// once the new content is completely on disk.
[[nodiscard]] Status saveProject(const std::filesystem::path& path, const core::Project& project);

} // namespace tnp::app
