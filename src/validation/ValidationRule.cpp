#include "validation/ValidationRule.h"

#include <format>

namespace tnp::validation {

void ValidationContext::report(Severity severity, std::string code, std::string message,
                               core::ObjectRef subject, std::string subjectName,
                               std::string suggestion) {
    ValidationIssue issue;
    issue.severity = severity;
    issue.code = std::move(code);
    issue.message = std::move(message);
    issue.suggestion = std::move(suggestion);
    issue.subject = subject;
    issue.subjectName = std::move(subjectName);
    report_.issues.push_back(std::move(issue));
}

void ValidationContext::reportDevice(Severity severity, std::string code, std::string message,
                                     const core::Device& device, std::string suggestion) {
    report(severity, std::move(code), std::move(message), core::ObjectRef::device(device.id()),
           device.name(), std::move(suggestion));
}

void ValidationContext::reportInterface(Severity severity, std::string code, std::string message,
                                        const core::Device& device, const core::Interface& iface,
                                        std::string suggestion) {
    report(severity, std::move(code), std::move(message),
           core::ObjectRef::iface(iface.id(), device.id()),
           std::format("{} {}", device.name(), iface.shortName()), std::move(suggestion));
}

void ValidationContext::reportLink(Severity severity, std::string code, std::string message,
                                   const core::Link& link, std::string suggestion) {
    const core::Device* a = network().findDevice(link.endpointA().device);
    const core::Device* b = network().findDevice(link.endpointB().device);
    const std::string name = std::format("{} - {}", a ? a->name() : "?", b ? b->name() : "?");
    report(severity, std::move(code), std::move(message), core::ObjectRef::link(link.id()), name,
           std::move(suggestion));
}

void ValidationContext::reportProject(Severity severity, std::string code, std::string message,
                                      std::string suggestion) {
    report(severity, std::move(code), std::move(message), core::ObjectRef{}, project_.metadata().name,
           std::move(suggestion));
}

} // namespace tnp::validation
