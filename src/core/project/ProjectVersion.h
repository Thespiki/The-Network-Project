#pragma once

#include "utilities/Types.h"

#include <format>
#include <string>

namespace tnp::core {

/// Version of the on-disk project schema.
///
/// A file with a newer *major* version is refused, because its structure may
/// have changed in ways this build cannot interpret. A newer *minor* version is
/// accepted with a warning: minor bumps only add optional fields, so an older
/// build reads what it understands and ignores the rest.
struct ProjectVersion {
    u32 major = 1;
    u32 minor = 0;

    [[nodiscard]] std::string toString() const { return std::format("{}.{}", major, minor); }

    auto operator<=>(const ProjectVersion&) const = default;
    bool operator==(const ProjectVersion&) const = default;
};

inline constexpr ProjectVersion kCurrentProjectVersion{1, 0};

/// Whether this build can read a project written with `version`.
[[nodiscard]] constexpr bool isProjectVersionReadable(const ProjectVersion& version) {
    return version.major == kCurrentProjectVersion.major;
}

/// Whether the file was written by a newer build than this one.
[[nodiscard]] constexpr bool isProjectVersionNewer(const ProjectVersion& version) {
    return version > kCurrentProjectVersion;
}

} // namespace tnp::core
