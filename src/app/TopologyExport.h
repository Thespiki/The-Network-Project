#pragma once

#include "core/project/Project.h"
#include "utilities/Result.h"

namespace tnp::app {

/// Options for exporting a topology picture.
struct SvgExportOptions {
    bool includeAnnotations = true;
    bool includeInterfaceLabels = true;
    bool includeAddresses = true;
    bool darkBackground = true;
    float margin = 80.0f;
};

/// Renders the topology to a standalone SVG document.
///
/// Written from the project model rather than from the canvas, so an export does
/// not depend on the window being open, on the current zoom, or on the UI at all.
/// The headless tool exports the same picture the application does.
[[nodiscard]] Result<std::string> exportTopologySvg(const core::Project& project,
                                                    const SvgExportOptions& options = {});

} // namespace tnp::app
