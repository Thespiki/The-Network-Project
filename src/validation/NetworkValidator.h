#pragma once

#include "validation/ValidationRule.h"

#include <memory>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace tnp::validation {

/// Runs the validation rules over a project.
///
/// Independent of the UI and of the simulator: the headless tool, the tests and
/// the Problems panel all call the same validator and get the same findings.
class NetworkValidator {
public:
    /// Registers the built-in rules.
    NetworkValidator();

    /// Creates an empty validator, for tests that exercise one rule.
    struct Empty {};
    explicit NetworkValidator(Empty);

    void addRule(std::unique_ptr<ValidationRule> rule);

    [[nodiscard]] ValidationReport validate(const core::Project& project) const;

    [[nodiscard]] const std::vector<std::unique_ptr<ValidationRule>>& rules() const { return rules_; }

    /// Rules can be switched off individually; a disabled rule contributes
    /// nothing to the report.
    void setRuleEnabled(std::string_view id, bool enabled);
    [[nodiscard]] bool isRuleEnabled(std::string_view id) const;

private:
    std::vector<std::unique_ptr<ValidationRule>> rules_;
    std::unordered_set<std::string> disabled_;
};

} // namespace tnp::validation
