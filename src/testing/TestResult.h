#pragma once

#include "core/network/Ids.h"
#include "utilities/Time.h"
#include "utilities/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace tnp::testing {

enum class TestStatus : u8 {
    NotRun,
    Passed,
    Failed,  ///< the network behaved differently from what the test expects
    Error,   ///< the test itself could not run: missing device, no address
    Skipped  ///< disabled by the user
};

[[nodiscard]] std::string_view testStatusName(TestStatus status);

/// One hop of the path a test's traffic actually took.
struct TestStep {
    std::string description; ///< "PC1 -> Router1"
    bool reached = false;    ///< the traffic got this far
    SimTime time{};
};

/// What happened when one test ran.
struct TestResult {
    core::TestId testId;
    std::string testName;
    TestStatus status = TestStatus::NotRun;

    /// One-line outcome, suitable for a list.
    std::string message;

    /// Why it failed, when the engine could tell: "no route to 10.0.0.5",
    /// "denied by firewall rule 3", "TTL expired at Router2".
    std::string reason;

    /// The path the probes took, for the hop-by-hop display.
    std::vector<TestStep> steps;

    u32 probesSent = 0;
    u32 probesReceived = 0;
    Duration averageRtt = Duration::zero();

    /// Simulated time the run consumed.
    Duration duration = Duration::zero();

    [[nodiscard]] bool passed() const { return status == TestStatus::Passed; }
};

/// The outcome of running a set of tests.
struct TestRunSummary {
    std::vector<TestResult> results;

    [[nodiscard]] std::size_t passedCount() const;
    [[nodiscard]] std::size_t failedCount() const;
    [[nodiscard]] std::size_t errorCount() const;
    [[nodiscard]] std::size_t skippedCount() const;

    [[nodiscard]] bool allPassed() const;

    /// "3 passed, 1 failed, 0 errors"
    [[nodiscard]] std::string summaryLine() const;
};

} // namespace tnp::testing
