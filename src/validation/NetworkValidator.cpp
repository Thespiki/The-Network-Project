#include "validation/NetworkValidator.h"

#include "utilities/Logging.h"
#include "validation/BuiltinRules.h"

namespace tnp::validation {

NetworkValidator::NetworkValidator() : rules_(makeBuiltinRules()) {}

NetworkValidator::NetworkValidator(Empty) {}

void NetworkValidator::addRule(std::unique_ptr<ValidationRule> rule) {
    if (rule) rules_.push_back(std::move(rule));
}

void NetworkValidator::setRuleEnabled(std::string_view id, bool enabled) {
    if (enabled) disabled_.erase(std::string{id});
    else         disabled_.insert(std::string{id});
}

bool NetworkValidator::isRuleEnabled(std::string_view id) const {
    return !disabled_.contains(std::string{id});
}

ValidationReport NetworkValidator::validate(const core::Project& project) const {
    ValidationReport report;
    ValidationContext context{project, report};

    for (const auto& rule : rules_) {
        if (!isRuleEnabled(rule->id())) continue;
        rule->check(context);
    }

    report.sort();

    if (report.hasErrors()) {
        logging::warning("validation", "{} error(s) and {} warning(s) found", report.errorCount(),
                     report.warningCount());
    }
    return report;
}

} // namespace tnp::validation
