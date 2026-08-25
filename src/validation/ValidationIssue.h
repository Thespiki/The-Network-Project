#pragma once

#include "core/network/Ids.h"
#include "utilities/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace tnp::validation {

/// How serious a finding is.
enum class Severity : u8 {
    Info,    ///< worth knowing; the network still works
    Warning, ///< likely a mistake, or a limitation of what TNP simulates
    Error    ///< the network cannot work as configured
};

[[nodiscard]] std::string_view severityName(Severity severity);

/// One finding about a project.
///
/// Every issue names the object it is about, so the Problems panel can select
/// it on the canvas and open its properties. `code` is a stable identifier -
/// messages are for people, codes are for filtering, suppressing and testing.
struct ValidationIssue {
    Severity severity = Severity::Warning;
    std::string code;
    std::string message;

    /// What to do about it. Empty when there is nothing concrete to suggest.
    std::string suggestion;

    core::ObjectRef subject;
    /// Display name of the subject, resolved when the issue is created so the
    /// panel does not have to look objects up while drawing.
    std::string subjectName;
};

/// The result of a validation pass.
struct ValidationReport {
    std::vector<ValidationIssue> issues;

    [[nodiscard]] std::size_t count(Severity severity) const;
    [[nodiscard]] std::size_t errorCount() const { return count(Severity::Error); }
    [[nodiscard]] std::size_t warningCount() const { return count(Severity::Warning); }
    [[nodiscard]] std::size_t infoCount() const { return count(Severity::Info); }

    [[nodiscard]] bool hasErrors() const { return errorCount() > 0; }
    [[nodiscard]] bool isClean() const { return issues.empty(); }

    /// Findings about one object, for the properties panel's diagnostics section.
    [[nodiscard]] std::vector<ValidationIssue> forObject(const core::ObjectRef& subject) const;

    /// Sorts by severity (most serious first) then by subject name, so the panel
    /// order is stable between runs.
    void sort();
};

} // namespace tnp::validation
