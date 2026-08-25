#include "validation/ValidationIssue.h"

#include <algorithm>

namespace tnp::validation {

std::string_view severityName(Severity severity) {
    switch (severity) {
        case Severity::Info:    return "Info";
        case Severity::Warning: return "Warning";
        case Severity::Error:   return "Error";
    }
    return "Info";
}

std::size_t ValidationReport::count(Severity severity) const {
    return static_cast<std::size_t>(
        std::count_if(issues.begin(), issues.end(),
                      [severity](const ValidationIssue& issue) { return issue.severity == severity; }));
}

std::vector<ValidationIssue> ValidationReport::forObject(const core::ObjectRef& subject) const {
    std::vector<ValidationIssue> result;
    for (const ValidationIssue& issue : issues) {
        const bool matchesSubject = issue.subject.id == subject.id;
        const bool matchesParent = issue.subject.parent == subject.id && !subject.id.isNil();
        if (matchesSubject || matchesParent) result.push_back(issue);
    }
    return result;
}

void ValidationReport::sort() {
    std::stable_sort(issues.begin(), issues.end(),
                     [](const ValidationIssue& a, const ValidationIssue& b) {
                         if (a.severity != b.severity) return a.severity > b.severity;
                         if (a.subjectName != b.subjectName) return a.subjectName < b.subjectName;
                         return a.code < b.code;
                     });
}

} // namespace tnp::validation
