#include "testing/NetworkTestRunner.h"

#include "core/devices/Ipv4Stack.h"
#include "serialization/ProjectSerializer.h"
#include "simulation/Simulator.h"
#include "utilities/Logging.h"

#include <algorithm>
#include <format>

namespace tnp::testing {
namespace {

using namespace core;

/// The address a test should aim at.
///
/// A test that names a device is resolved at run time, so re-addressing the
/// network does not silently break the test.
std::optional<Ipv4Address> resolveDestination(const Network& network, const NetworkTest& test,
                                              std::string& problem) {
    if (test.destinationAddress) return *test.destinationAddress;

    const Device* device = network.findDevice(test.destinationDevice);
    if (device == nullptr) {
        problem = "the destination device is not in the project";
        return std::nullopt;
    }

    for (const auto& iface : device->interfaces()) {
        if (const auto address = iface->primaryIpv4()) return address->address();
    }

    problem = std::format("{} has no IPv4 address to aim at", device->name());
    return std::nullopt;
}

/// Failure kinds worth quoting back to the user, most specific first.
bool isExplanatory(TraceKind kind) {
    switch (kind) {
        case TraceKind::IpNoRouteToHost:
        case TraceKind::ArpTimedOut:
        case TraceKind::FirewallDenied:
        case TraceKind::IpTtlExpired:
        case TraceKind::IcmpDestinationUnreachableReceived:
        case TraceKind::IcmpTimeExceededReceived:
        case TraceKind::IpFragmentationNeeded:
        case TraceKind::FrameFilteredByVlan:
            return true;
        default:
            return false;
    }
}

/// Builds the hop list from the path a probe actually took.
std::vector<TestStep> buildSteps(const sim::Simulator& simulator, const Network& network,
                                 DeviceId source, Ipv4Address destination) {
    std::vector<TestStep> steps;

    // The first echo request the source produced is the probe whose journey is
    // worth showing; later probes follow a warm ARP cache and skip the interesting part.
    const sim::PacketRecord* probe = nullptr;
    for (const PacketId id : simulator.packets().order()) {
        const sim::PacketRecord* record = simulator.packets().find(id);
        if (record == nullptr) continue;
        if (record->origin != source) continue;
        if (record->category != FrameCategory::Icmp) continue;
        if (record->summary.find("echo request") == std::string::npos) continue;
        probe = record;
        break;
    }
    if (probe == nullptr) return steps;

    // Collapse the per-interface hops into a device-to-device path.
    std::vector<std::pair<DeviceId, SimTime>> path;
    for (const sim::PacketHop& hop : probe->hops) {
        if (hop.action != "received" && hop.action != "created") continue;
        if (!path.empty() && path.back().first == hop.device) continue;
        path.emplace_back(hop.device, hop.time);
    }

    const auto nameOf = [&](DeviceId id) {
        const Device* device = network.findDevice(id);
        return device == nullptr ? std::string{"?"} : device->name();
    };

    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        TestStep step;
        step.description = std::format("{} -> {}", nameOf(path[i].first), nameOf(path[i + 1].first));
        step.reached = true;
        step.time = path[i + 1].second;
        steps.push_back(std::move(step));
    }

    // Where did it stop? If the last device on the path is not the one holding
    // the destination address, the probe never arrived.
    const Device* target = const_cast<Network&>(network).findDeviceWithIpv4(destination);
    if (target != nullptr && !path.empty() && path.back().first != target->id()) {
        TestStep step;
        step.description = std::format("{} -> {}", nameOf(path.back().first), target->name());
        step.reached = false;
        steps.push_back(std::move(step));
    }
    return steps;
}

} // namespace

NetworkTestRunner::NetworkTestRunner(const core::Project& project) {
    // Copy through the serializer: it is the one operation that already knows
    // how to reproduce a project exactly, identifiers included.
    const serial::ProjectSerializer serializer;

    auto document = serializer.write(project, false);
    if (!document) {
        error_ = std::format("could not snapshot the project: {}", document.message());
        return;
    }

    sandbox_ = std::make_unique<core::Project>();
    auto loaded = serializer.read(document.value(), *sandbox_);
    if (!loaded) {
        error_ = std::format("could not rebuild the project snapshot: {}", loaded.message());
        sandbox_.reset();
        return;
    }

    tests_ = project.tests();
    ready_ = true;
}

NetworkTestRunner::~NetworkTestRunner() = default;

TestResult NetworkTestRunner::run(const NetworkTest& test) {
    TestResult result;
    result.testId = test.id;
    result.testName = test.name;

    if (!ready_) {
        result.status = TestStatus::Error;
        result.message = error_;
        return result;
    }
    if (!test.enabled) {
        result.status = TestStatus::Skipped;
        result.message = "disabled";
        return result;
    }

    Network& network = sandbox_->network();

    Device* source = network.findDevice(test.source);
    if (source == nullptr) {
        result.status = TestStatus::Error;
        result.message = "the source device is not in the project";
        return result;
    }
    if (source->ipv4Stack() == nullptr) {
        result.status = TestStatus::Error;
        result.message = std::format("{} has no IPv4 stack and cannot send a ping", source->name());
        return result;
    }

    std::string problem;
    const auto destination = resolveDestination(network, test, problem);
    if (!destination) {
        result.status = TestStatus::Error;
        result.message = problem;
        return result;
    }

    // A fresh simulator per test: every run starts from cold caches, so results
    // do not depend on the order tests happen to run in.
    core::SimulationSettings settings = sandbox_->simulationSettings();
    settings.speedMultiplier = 1.0;
    sim::Simulator simulator{network, settings};

    // Remember the most recent explanatory failure as the run proceeds.
    std::string lastFailure;
    simulator.addTraceObserver([&lastFailure](const TraceEvent& event) {
        if (isExplanatory(event.kind)) lastFailure = event.summary;
    });

    simulator.start();

    PingRequest request;
    request.destination = *destination;
    request.count = std::max<u32>(test.probeCount, 1);
    request.timeout = test.timeout;
    request.payloadSize = test.payloadSize;
    // Probes back to back: a test should not spend simulated seconds waiting
    // between them.
    request.interval = milliseconds(10);

    const auto ping = simulator.ping(source->id(), request);

    if (!ping) {
        // A rejected ping is still an answer when the test expects failure.
        result.probesSent = 0;
        result.reason = ping.message();
        if (test.expectation == TestExpectation::Unreachable) {
            result.status = TestStatus::Passed;
            result.message = std::format("{} is unreachable, as expected ({})",
                                         destination->toString(), ping.message());
        } else {
            result.status = TestStatus::Failed;
            result.message = std::format("could not reach {}: {}", destination->toString(),
                                         ping.message());
        }
        return result;
    }

    simulator.runUntilIdle(budget_);

    const PingStatistics* statistics = source->ipv4Stack()->pingStatistics(ping.value());
    if (statistics == nullptr) {
        result.status = TestStatus::Error;
        result.message = "the ping produced no statistics";
        return result;
    }

    result.probesSent = statistics->sent;
    result.probesReceived = statistics->received;
    result.averageRtt = statistics->averageRtt();
    result.duration = simulator.now().time_since_epoch();
    result.steps = buildSteps(simulator, network, source->id(), *destination);
    result.reason = lastFailure;

    const bool reachable = statistics->received > 0;
    const bool expectedReachable = test.expectation == TestExpectation::Reachable;

    if (reachable == expectedReachable) {
        result.status = TestStatus::Passed;
        result.message = reachable
                             ? std::format("{} replied to {}/{} probes, average {}",
                                           destination->toString(), statistics->received,
                                           statistics->sent, formatDuration(statistics->averageRtt()))
                             : std::format("{} is unreachable, as expected", destination->toString());
    } else {
        result.status = TestStatus::Failed;
        result.message = expectedReachable
                             ? std::format("{} did not reply to any of {} probes",
                                           destination->toString(), statistics->sent)
                             : std::format("{} replied to {}/{} probes but was expected to be unreachable",
                                           destination->toString(), statistics->received,
                                           statistics->sent);
    }

    simulator.stop();
    return result;
}

TestRunSummary NetworkTestRunner::runAll(const std::function<void(const TestResult&)>& onResult) {
    TestRunSummary summary;
    summary.results.reserve(tests_.size());

    for (const NetworkTest& test : tests_) {
        TestResult result = run(test);
        if (onResult) onResult(result);
        summary.results.push_back(std::move(result));
    }

    logging::info("tests", "{}", summary.summaryLine());
    return summary;
}

} // namespace tnp::testing
