#pragma once

#include "core/devices/DeviceRegistry.h"
#include "core/project/Project.h"
#include "utilities/Result.h"

#include <string>
#include <string_view>
#include <vector>

namespace tnp::serial {

/// What happened while reading a project file.
struct LoadReport {
    /// Recoverable problems: unknown fields, malformed addresses that were
    /// skipped, references that could not be resolved.
    std::vector<std::string> warnings;

    core::ProjectVersion fileVersion;

    /// The file was written by a newer build. It was read anyway, on the
    /// understanding that unknown fields were dropped.
    bool writtenByNewerBuild = false;

    [[nodiscard]] bool isClean() const { return warnings.empty() && !writtenByNewerBuild; }
};

/// Converts a `Project` to and from the `.tnpjson` text format.
///
/// The whole model round-trips: identifiers, addressing, routing, VLANs,
/// policies, services, canvas placement, annotations, tests and simulation
/// options. Reading is deliberately forgiving - a malformed field costs that
/// field and a warning, not the project - because the format is meant to be
/// readable and hand-editable during development.
class ProjectSerializer {
public:
    explicit ProjectSerializer(const core::DeviceRegistry& registry = core::builtinDeviceRegistry());

    /// Serializes to JSON text. `pretty` produces the indented form that goes in
    /// a `.tnpjson` file; the compact form is used inside a `.tnp` container.
    [[nodiscard]] Result<std::string> write(const core::Project& project, bool pretty = true) const;

    /// Replaces the contents of `project` with what `text` describes.
    ///
    /// On failure `project` is left untouched: parsing builds a new model and
    /// only swaps it in once it is complete.
    [[nodiscard]] Result<LoadReport> read(std::string_view text, core::Project& project) const;

private:
    const core::DeviceRegistry& registry_;
};

} // namespace tnp::serial
