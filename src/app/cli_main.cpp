// Headless entry point.
//
// Everything below drives the same `Application` the graphical build uses, which
// is what makes the engine testable in CI without a display and gives users a
// way to validate and test projects from a script.

#include "app/Application.h"
#include "app/SampleProject.h"
#include "core/devices/Ipv4Stack.h"
#include "utilities/FileSystem.h"
#include "utilities/Logging.h"

#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace tnp;

constexpr std::string_view kUsage = R"(TNP - The Network Project (headless tool)

Usage:
  tnpcli info      <project>              summarise a project
  tnpcli validate  <project>              run the validation rules
  tnpcli test      <project>              run the project's connectivity tests
  tnpcli ping      <project> <from> <to>  simulate a ping between two devices
  tnpcli export    <project> <out.svg>    export the topology as SVG
  tnpcli convert   <in> <out>             convert between .tnp and .tnpjson
  tnpcli demo      <out>                  write the sample project

<project> may be a .tnp container or a .tnpjson document.
)";

int fail(std::string_view message) {
    std::cerr << "error: " << message << "\n";
    return 1;
}

/// Loads a project, printing any warnings the file produced.
bool load(app::Application& application, const std::string& path) {
    if (const Status status = application.open(path); !status) {
        std::cerr << "error: " << status.message() << "\n";
        return false;
    }
    for (const std::string& warning : application.loadWarnings()) {
        std::cerr << "warning: " << warning << "\n";
    }
    return true;
}

int commandInfo(app::Application& application) {
    const core::Project& project = application.project();
    const core::Network& network = project.network();

    std::cout << std::format("{}\n", project.metadata().name);
    if (!project.metadata().description.empty()) {
        std::cout << std::format("  {}\n", project.metadata().description);
    }
    std::cout << std::format("  format version {}\n", project.metadata().version.toString());
    std::cout << std::format("  {} device(s), {} link(s), {} annotation(s), {} test(s)\n",
                             network.deviceCount(), network.linkCount(),
                             project.annotations().size(), project.tests().size());
    std::cout << "\nDevices:\n";

    for (const auto& device : network.devices()) {
        std::string addresses;
        for (const auto& iface : device->interfaces()) {
            for (const core::Ipv4Prefix& prefix : iface->ipv4Addresses()) {
                if (!addresses.empty()) addresses += ", ";
                addresses += prefix.toString();
            }
        }
        std::cout << std::format("  {:<16} {:<16} {}\n", device->name(), device->typeDisplayName(),
                                 addresses.empty() ? "(no addresses)" : addresses);
    }
    return 0;
}

int commandValidate(app::Application& application) {
    const validation::ValidationReport& report = application.validationReport();

    if (report.isClean()) {
        std::cout << "No problems found.\n";
        return 0;
    }

    for (const validation::ValidationIssue& issue : report.issues) {
        std::cout << std::format("{:<8} {:<28} {}: {}\n", validation::severityName(issue.severity),
                                 issue.code, issue.subjectName, issue.message);
        if (!issue.suggestion.empty()) std::cout << std::format("         -> {}\n", issue.suggestion);
    }

    std::cout << std::format("\n{} error(s), {} warning(s), {} note(s)\n", report.errorCount(),
                             report.warningCount(), report.infoCount());
    return report.hasErrors() ? 2 : 0;
}

int commandTest(app::Application& application) {
    const testing::TestRunSummary summary = application.runTests();
    if (summary.results.empty()) {
        std::cout << "This project contains no tests.\n";
        return 0;
    }

    for (const testing::TestResult& result : summary.results) {
        const char* mark = result.passed() ? "PASS" : "FAIL";
        if (result.status == testing::TestStatus::Skipped) mark = "SKIP";
        if (result.status == testing::TestStatus::Error) mark = "ERR ";

        std::cout << std::format("[{}] {}\n       {}\n", mark, result.testName, result.message);
        for (const testing::TestStep& step : result.steps) {
            std::cout << std::format("       {} {}\n", step.reached ? "ok  " : "fail", step.description);
        }
        if (!result.reason.empty() && !result.passed()) {
            std::cout << std::format("       reason: {}\n", result.reason);
        }
    }

    std::cout << std::format("\n{}\n", summary.summaryLine());
    return summary.failedCount() + summary.errorCount() > 0 ? 2 : 0;
}

int commandPing(app::Application& application, const std::string& fromName, const std::string& toName) {
    core::Network& network = application.project().network();

    core::Device* source = network.findDeviceByName(fromName);
    if (source == nullptr) return fail(std::format("no device called '{}'", fromName));
    if (source->ipv4Stack() == nullptr) {
        return fail(std::format("{} has no IPv4 stack", source->name()));
    }

    // The destination may be a device name or a literal address.
    std::optional<core::Ipv4Address> destination = core::Ipv4Address::parse(toName);
    if (!destination) {
        const core::Device* target = network.findDeviceByName(toName);
        if (target == nullptr) return fail(std::format("no device or address called '{}'", toName));
        for (const auto& iface : target->interfaces()) {
            if (const auto address = iface->primaryIpv4()) {
                destination = address->address();
                break;
            }
        }
        if (!destination) return fail(std::format("{} has no IPv4 address", target->name()));
    }

    sim::Simulator& simulator = application.simulator();
    simulator.start();

    core::PingRequest request;
    request.destination = *destination;
    request.count = 4;
    request.interval = milliseconds(200);

    const auto ping = simulator.ping(source->id(), request);
    if (!ping) return fail(ping.message());

    std::cout << std::format("Pinging {} from {} with {} bytes of data:\n", destination->toString(),
                             source->name(), request.payloadSize);

    simulator.runUntilIdle(seconds(30));

    for (const core::TraceEvent& event : simulator.traceLog()) {
        if (event.kind == core::TraceKind::PingReplyReceived ||
            event.kind == core::TraceKind::PingTimedOut) {
            std::cout << std::format("  {}\n", event.summary);
        }
    }

    const core::PingStatistics* statistics = source->ipv4Stack()->pingStatistics(ping.value());
    if (statistics == nullptr) return fail("the ping produced no statistics");

    std::cout << std::format("\n{} sent, {} received, {} lost", statistics->sent, statistics->received,
                             statistics->lost);
    if (statistics->received > 0) {
        std::cout << std::format(", average {}", formatDuration(statistics->averageRtt()));
    }
    std::cout << "\n";

    return statistics->received > 0 ? 0 : 2;
}

int commandExport(app::Application& application, const std::string& output) {
    auto svg = application.exportSvg();
    if (!svg) return fail(svg.message());

    if (const Status status = files::writeTextFileAtomic(output, svg.value()); !status) {
        return fail(status.message());
    }
    std::cout << std::format("Wrote {}\n", output);
    return 0;
}

int commandConvert(app::Application& application, const std::string& output) {
    if (const Status status = application.saveAs(output); !status) return fail(status.message());
    std::cout << std::format("Wrote {}\n", output);
    return 0;
}

int commandDemo(app::Application& application, const std::string& output) {
    app::buildSampleProject(application.project());
    if (const Status status = application.saveAs(output); !status) return fail(status.message());

    std::cout << std::format("Wrote the sample project to {}\n", output);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.empty() || arguments[0] == "-h" || arguments[0] == "--help") {
        std::cout << kUsage;
        return arguments.empty() ? 1 : 0;
    }

    logging::Logger::instance().addSink(std::make_shared<logging::ConsoleSink>(false));
    logging::Logger::instance().setMinimumLevel(logging::Level::Warning);

    app::Application application;
    application.initialize();

    const std::string& command = arguments[0];
    const auto argument = [&](std::size_t index) -> std::string {
        return index < arguments.size() ? arguments[index] : std::string{};
    };

    int exitCode = 0;

    if (command == "demo") {
        if (argument(1).empty()) exitCode = fail("usage: tnpcli demo <out>");
        else                     exitCode = commandDemo(application, argument(1));
    } else if (argument(1).empty()) {
        exitCode = fail(std::format("'{}' needs a project file", command));
    } else if (!load(application, argument(1))) {
        exitCode = 1;
    } else if (command == "info") {
        exitCode = commandInfo(application);
    } else if (command == "validate") {
        exitCode = commandValidate(application);
    } else if (command == "test") {
        exitCode = commandTest(application);
    } else if (command == "ping") {
        if (argument(2).empty() || argument(3).empty()) {
            exitCode = fail("usage: tnpcli ping <project> <from> <to>");
        } else {
            exitCode = commandPing(application, argument(2), argument(3));
        }
    } else if (command == "export") {
        if (argument(2).empty()) exitCode = fail("usage: tnpcli export <project> <out.svg>");
        else                     exitCode = commandExport(application, argument(2));
    } else if (command == "convert") {
        if (argument(2).empty()) exitCode = fail("usage: tnpcli convert <in> <out>");
        else                     exitCode = commandConvert(application, argument(2));
    } else {
        exitCode = fail(std::format("unknown command '{}'", command));
        std::cerr << kUsage;
    }

    application.shutdown();
    return exitCode;
}
