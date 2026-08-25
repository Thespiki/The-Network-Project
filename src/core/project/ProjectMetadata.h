#pragma once

#include "core/network/Ids.h"
#include "core/project/ProjectVersion.h"

#include <string>
#include <vector>

namespace tnp::core {

/// Descriptive information about a project. Never affects simulation.
struct ProjectMetadata {
    ProjectId id = ProjectId::generate();
    std::string name = "Untitled Project";
    std::string description;
    std::string author;
    std::vector<std::string> tags;

    /// ISO-8601 UTC timestamps.
    std::string createdAt;
    std::string modifiedAt;

    /// Version of TNP that last wrote the file, for diagnostics.
    std::string writtenBy;

    ProjectVersion version = kCurrentProjectVersion;
};

} // namespace tnp::core
