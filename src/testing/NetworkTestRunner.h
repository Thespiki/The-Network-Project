#pragma once

#include "core/project/Project.h"
#include "testing/TestResult.h"

#include <functional>
#include <memory>

namespace tnp::testing {

/// Executes the connectivity tests stored in a project.
///
/// Each run happens on a private copy of the project, obtained through the
/// serializer. That matters for two reasons: starting a simulation resets every
/// device's caches, so running tests against the live model would throw away
/// whatever the user was watching, and a test that leaves an ARP entry behind
/// would change the result of the next one. Identifiers survive serialization,
/// so a test still finds the devices it names.
class NetworkTestRunner {
public:
    explicit NetworkTestRunner(const core::Project& project);
    ~NetworkTestRunner();

    NetworkTestRunner(const NetworkTestRunner&) = delete;
    NetworkTestRunner& operator=(const NetworkTestRunner&) = delete;

    /// How long a single test may occupy the simulated clock before it is
    /// declared a failure. Generous: the timeout that matters is the one the
    /// test itself specifies.
    void setTimeBudget(Duration budget) { budget_ = budget; }

    [[nodiscard]] TestResult run(const core::NetworkTest& test);

    /// Runs every test in the project, in order. `onResult` is called as each
    /// one finishes so a UI can fill the list progressively.
    [[nodiscard]] TestRunSummary runAll(const std::function<void(const TestResult&)>& onResult = {});

    /// True when the private copy could be built. When false, `error()` says why
    /// and every run reports an error.
    [[nodiscard]] bool isReady() const { return ready_; }
    [[nodiscard]] const std::string& error() const { return error_; }

private:
    std::unique_ptr<core::Project> sandbox_;
    std::vector<core::NetworkTest> tests_;
    Duration budget_ = seconds(30);
    bool ready_ = false;
    std::string error_;
};

} // namespace tnp::testing
