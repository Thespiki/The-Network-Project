#include "app/ProjectFile.h"

#include "serialization/TnpContainer.h"
#include "serialization/TnpCrypto.h"
#include "utilities/FileSystem.h"
#include "utilities/Logging.h"
#include "utilities/Time.h"

#include <format>

namespace tnp::app {
namespace {

/// Small description of the container written alongside the project, so a `.tnp`
/// file can be identified without a copy of TNP.
std::string makeManifest(const core::Project& project) {
    return std::format(R"({{
  "format": "tnp-container",
  "containerVersion": {},
  "projectVersion": "{}",
  "projectName": "{}",
  "writtenAt": "{}",
  "entries": ["{}"]
}})",
                       serial::TnpContainer::kVersion, project.metadata().version.toString(),
                       project.metadata().name, currentTimestampIso8601(),
                       serial::TnpContainer::kProjectEntry);
}

} // namespace

ProjectFormat formatForPath(const std::filesystem::path& path) {
    const std::string extension = files::extensionOf(path);
    if (extension == kProjectExtension) return ProjectFormat::Container;
    if (extension == kJsonProjectExtension || extension == ".json") return ProjectFormat::Json;
    if (extension == kEncryptedProjectExtension) return ProjectFormat::Encrypted;
    return ProjectFormat::Unknown;
}

std::string_view projectFormatName(ProjectFormat format) {
    switch (format) {
        case ProjectFormat::Json:      return "TNP JSON project";
        case ProjectFormat::Container: return "TNP project";
        case ProjectFormat::Encrypted: return "Encrypted TNP project";
        case ProjectFormat::Unknown:   return "Unknown";
    }
    return "Unknown";
}

Result<serial::LoadReport> loadProject(const std::filesystem::path& path, core::Project& project) {
    auto bytes = files::readBinaryFile(path);
    if (!bytes) return Result<serial::LoadReport>::failure(bytes.error());

    const serial::ProjectSerializer serializer;

    // Content wins over the extension: a container is unmistakable.
    if (serial::TnpContainer::looksLikeContainer(bytes.value())) {
        auto container = serial::TnpContainer::parse(bytes.value());
        if (!container) return Result<serial::LoadReport>::failure(container.error());

        const auto document = container.value().findText(serial::TnpContainer::kProjectEntry);
        if (!document) {
            return Result<serial::LoadReport>::failure(
                std::format("the container has no '{}' entry", serial::TnpContainer::kProjectEntry),
                path.string());
        }
        return serializer.read(*document, project);
    }

    if (formatForPath(path) == ProjectFormat::Encrypted) {
        return Result<serial::LoadReport>::failure(std::string{serial::encryptionUnavailableReason()},
                                                   path.string());
    }

    const std::string text{bytes.value().begin(), bytes.value().end()};
    return serializer.read(text, project);
}

Status saveProject(const std::filesystem::path& path, const core::Project& project) {
    const serial::ProjectSerializer serializer;
    const ProjectFormat format = formatForPath(path);

    if (format == ProjectFormat::Encrypted) {
        return Status::failure(std::string{serial::encryptionUnavailableReason()}, path.string());
    }

    if (format == ProjectFormat::Json) {
        auto document = serializer.write(project, true);
        if (!document) return document.status();
        return files::writeTextFileAtomic(path, document.value());
    }

    // Everything else is written as a container; an unrecognised extension is
    // treated as one rather than refused, so "myproject.bak" still works.
    auto document = serializer.write(project, false);
    if (!document) return document.status();

    serial::TnpContainer container;
    container.addTextEntry(std::string{serial::TnpContainer::kManifestEntry}, makeManifest(project));
    container.addTextEntry(std::string{serial::TnpContainer::kProjectEntry}, document.value());

    return files::writeBinaryFileAtomic(path, container.serialize());
}

} // namespace tnp::app
