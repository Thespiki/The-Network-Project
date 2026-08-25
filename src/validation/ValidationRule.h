#pragma once

#include "core/project/Project.h"
#include "validation/ValidationIssue.h"

#include <string>
#include <string_view>

namespace tnp::validation {

/// What a rule may look at, and how it reports.
///
/// Rules receive the project read-only. Validation never repairs anything: a
/// tool that quietly rewrote a user's configuration while checking it would be
/// far more dangerous than one that only reports.
class ValidationContext {
public:
    ValidationContext(const core::Project& project, ValidationReport& report)
        : project_(project), report_(report) {}

    [[nodiscard]] const core::Project& project() const { return project_; }
    [[nodiscard]] const core::Network& network() const { return project_.network(); }

    void report(Severity severity, std::string code, std::string message,
                core::ObjectRef subject, std::string subjectName, std::string suggestion = {});

    /// Convenience overloads that resolve the display name for you.
    void reportDevice(Severity severity, std::string code, std::string message,
                      const core::Device& device, std::string suggestion = {});
    void reportInterface(Severity severity, std::string code, std::string message,
                         const core::Device& device, const core::Interface& iface,
                         std::string suggestion = {});
    void reportLink(Severity severity, std::string code, std::string message,
                    const core::Link& link, std::string suggestion = {});
    void reportProject(Severity severity, std::string code, std::string message,
                       std::string suggestion = {});

private:
    const core::Project& project_;
    ValidationReport& report_;
};

/// A single check.
///
/// Rules are separate objects rather than branches in one function so they can
/// be enabled individually, tested individually, and added by future code
/// without touching the validator.
class ValidationRule {
public:
    virtual ~ValidationRule() = default;

    /// Stable identifier, also used as the issue code prefix.
    [[nodiscard]] virtual std::string_view id() const = 0;

    /// One line describing what the rule looks for, shown in preferences.
    [[nodiscard]] virtual std::string_view description() const = 0;

    virtual void check(ValidationContext& context) const = 0;
};

} // namespace tnp::validation
