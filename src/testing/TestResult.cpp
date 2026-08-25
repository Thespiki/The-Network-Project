#include "testing/TestResult.h"

#include <algorithm>
#include <format>

namespace tnp::testing {
namespace {

std::size_t countWith(const std::vector<TestResult>& results, TestStatus status) {
    return static_cast<std::size_t>(
        std::count_if(results.begin(), results.end(),
                      [status](const TestResult& result) { return result.status == status; }));
}

} // namespace

std::string_view testStatusName(TestStatus status) {
    switch (status) {
        case TestStatus::NotRun:  return "not run";
        case TestStatus::Passed:  return "passed";
        case TestStatus::Failed:  return "failed";
        case TestStatus::Error:   return "error";
        case TestStatus::Skipped: return "skipped";
    }
    return "not run";
}

std::size_t TestRunSummary::passedCount() const { return countWith(results, TestStatus::Passed); }
std::size_t TestRunSummary::failedCount() const { return countWith(results, TestStatus::Failed); }
std::size_t TestRunSummary::errorCount() const { return countWith(results, TestStatus::Error); }
std::size_t TestRunSummary::skippedCount() const { return countWith(results, TestStatus::Skipped); }

bool TestRunSummary::allPassed() const {
    return failedCount() == 0 && errorCount() == 0 && passedCount() > 0;
}

std::string TestRunSummary::summaryLine() const {
    std::string line = std::format("{} passed, {} failed, {} error(s)", passedCount(), failedCount(),
                                   errorCount());
    if (const std::size_t skipped = skippedCount(); skipped > 0) {
        line += std::format(", {} skipped", skipped);
    }
    return line;
}

} // namespace tnp::testing
