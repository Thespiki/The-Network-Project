#pragma once

#include "core/project/Project.h"

namespace tnp::app {

/// Builds the example topology TNP ships with.
///
///     PC1 --- Switch1 --- Router1 === Router2 --- Server1
///        192.168.1.0/24      10.0.0.0/30      172.16.0.0/24
///
/// Used by the "sample project" menu entry, by the headless tool's `demo`
/// command, and by the integration tests - so the thing users see first is the
/// same thing the test suite proves works.
void buildSampleProject(core::Project& project);

} // namespace tnp::app
