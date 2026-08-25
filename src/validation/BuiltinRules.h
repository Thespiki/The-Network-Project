#pragma once

#include "validation/ValidationRule.h"

#include <memory>
#include <vector>

namespace tnp::validation {

/// The checks TNP ships with.
///
/// Grouped in one factory rather than registered from static initialisers, so
/// the set is explicit, ordered and testable.
[[nodiscard]] std::vector<std::unique_ptr<ValidationRule>> makeBuiltinRules();

} // namespace tnp::validation
