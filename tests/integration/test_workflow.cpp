// The workflow TNP exists to support, end to end:
//
//   create -> place -> cable -> address -> route -> validate -> simulate ->
//   ping -> inspect -> test -> save -> reload -> continue
//
// If this file passes, the product works. Everything else is detail.

#include "TestHelpers.h"

#include "app/Application.h"
#include "app/SampleProject.h"
#include "commands/DeviceCommands.h"
#include "commands/InterfaceCommands.h"
#include "commands/LinkCommands.h"
#include "commands/RoutingCommands.h"
#include "simulation/PacketDecoder.h"
#include "utilities/FileSystem.h"
#include "utilities/Uuid.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::tests;

namespace {

class ScratchDirectory {
public:
    ScratchDirectory() {
        path_ = std::filesystem::temp_directory_path() /
                ("tnp-workflow-" + Uuid::generate().toString().substr(0, 8));
        std::filesystem::create_directories(path_);
    }
    ~ScratchDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;

    [[nodiscard]] std::filesystem::path file(const std::string& name) const { return path_ / name; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("The complete workflow, from empty canvas to reloaded project", "[integration][workflow]") {
    const ScratchDirectory scratch;
    const auto projectPath = scratch.file("campus.tnp");

    DeviceId pcId;
    DeviceId serverId;

    // --- Build ------------------------------------------------------------
    {
        app::Application application;
        application.initialize();

        // Drag devices onto the canvas.
        pcId = application.addDevice(DeviceType::Pc, Vec2{-300, 0});
        const DeviceId switchId = application.addDevice(DeviceType::Switch, Vec2{-150, 0});
        const DeviceId routerId = application.addDevice(DeviceType::Router, Vec2{0, 0});
        serverId = application.addDevice(DeviceType::Server, Vec2{200, 0});

        REQUIRE(pcId.isValid());
        REQUIRE(application.project().network().deviceCount() == 4);
        CHECK(application.selection().primary().asDeviceId() == serverId);

        Network& network = application.project().network();
        Device& pc = *network.findDevice(pcId);
        Device& sw = *network.findDevice(switchId);
        Device& router = *network.findDevice(routerId);
        Device& server = *network.findDevice(serverId);

        // Connect interfaces.
        REQUIRE(application.connectInterfaces(iface(pc, "Gi0").id(), iface(sw, "Gi0/1").id()).isOk());
        REQUIRE(application.connectInterfaces(iface(sw, "Gi0/2").id(),
                                              iface(router, "Gi0/0").id())
                    .isOk());
        REQUIRE(application.connectInterfaces(iface(router, "Gi0/1").id(),
                                              iface(server, "Gi0").id())
                    .isOk());
        CHECK(network.linkCount() == 3);

        // Configure addressing through commands, as the properties panel does.
        const auto configure = [&](Device& device, std::string_view portName, std::string_view cidr) {
            Interface& port = iface(device, portName);
            commands::InterfaceSettings settings = commands::InterfaceSettings::capture(port);
            settings.ipv4.push_back(prefix(cidr));
            REQUIRE(application.commands().run(std::make_unique<commands::ConfigureInterfaceCommand>(
                device.id(), port.id(), settings)));
        };

        configure(pc, "Gi0", "192.168.1.10/24");
        configure(router, "Gi0/0", "192.168.1.1/24");
        configure(router, "Gi0/1", "172.16.0.1/24");
        configure(server, "Gi0", "172.16.0.20/24");

        REQUIRE(application.commands().run(
            std::make_unique<commands::SetDefaultGatewayCommand>(pcId, ipv4("192.168.1.1"))));
        REQUIRE(application.commands().run(
            std::make_unique<commands::SetDefaultGatewayCommand>(serverId, ipv4("172.16.0.1"))));

        // --- Validate -----------------------------------------------------
        const validation::ValidationReport& report = application.validationReport();
        INFO((report.issues.empty() ? std::string{} : report.issues.front().message));
        CHECK_FALSE(report.hasErrors());

        // --- Simulate and ping --------------------------------------------
        sim::Simulator& simulator = application.simulator();
        simulator.start();

        PingRequest request;
        request.destination = ipv4("172.16.0.20");
        request.count = 3;
        request.interval = milliseconds(100);

        const auto ping = simulator.ping(pcId, request);
        REQUIRE(ping.isOk());

        simulator.runUntilIdle(seconds(10));

        const PingStatistics* statistics = pc.ipv4Stack()->pingStatistics(ping.value());
        REQUIRE(statistics != nullptr);
        CHECK(statistics->sent == 3);
        CHECK(statistics->received == 3);

        // --- Inspect ------------------------------------------------------
        const sim::PacketRecord* echoReply = nullptr;
        for (auto id = simulator.packets().order().rbegin();
             id != simulator.packets().order().rend(); ++id) {
            const sim::PacketRecord* record = simulator.packets().find(*id);
            if (record != nullptr && record->category == FrameCategory::Icmp &&
                record->summary.find("reply") != std::string::npos) {
                echoReply = record;
                break;
            }
        }
        REQUIRE(echoReply != nullptr);

        const sim::DecodedPacket decoded = sim::decodePacket(echoReply->bytes);
        REQUIRE(decoded.layers.size() == 3);
        CHECK(decoded.layers[1].name == "IPv4");
        CHECK(decoded.layers[2].name == "ICMP");
        CHECK_FALSE(echoReply->hops.empty());

        // --- Automated tests ----------------------------------------------
        NetworkTest reachability;
        reachability.name = "PC reaches the server";
        reachability.source = pcId;
        reachability.destinationDevice = serverId;
        application.project().addTest(reachability);

        const testing::TestRunSummary summary = application.runTests();
        REQUIRE(summary.results.size() == 1);
        INFO(summary.results.front().message);
        CHECK(summary.results.front().passed());
        CHECK_FALSE(summary.results.front().steps.empty());

        // --- Save ---------------------------------------------------------
        application.project().metadata().name = "Campus";
        REQUIRE(application.saveAs(projectPath).isOk());
        CHECK_FALSE(application.isDirty());
        CHECK(application.documentTitle().find("campus.tnp") != std::string::npos);

        application.shutdown();
    }

    // --- Reload and continue ---------------------------------------------
    {
        app::Application application;
        application.initialize();

        REQUIRE(application.open(projectPath).isOk());
        CHECK(application.loadWarnings().empty());
        CHECK_FALSE(application.isDirty());

        Network& network = application.project().network();
        CHECK(network.deviceCount() == 4);
        CHECK(network.linkCount() == 3);
        CHECK(application.project().metadata().name == "Campus");
        CHECK(application.project().tests().size() == 1);

        // Identities survived the round trip.
        Device* pc = network.findDevice(pcId);
        REQUIRE(pc != nullptr);
        CHECK(pc->ipv4Stack()->defaultGateway() == ipv4("192.168.1.1"));

        // The reloaded project still works.
        CHECK_FALSE(application.validationReport().hasErrors());

        const testing::TestRunSummary summary = application.runTests();
        REQUIRE(summary.results.size() == 1);
        CHECK(summary.results.front().passed());

        // And it is still editable, with a fresh history.
        CHECK_FALSE(application.commands().canUndo());
        const DeviceId extra = application.addDevice(DeviceType::Pc, Vec2{-300, 150});
        CHECK(extra.isValid());
        CHECK(application.isDirty());
        CHECK(application.commands().canUndo());

        REQUIRE(application.commands().undo());
        CHECK(network.deviceCount() == 4);

        application.shutdown();
    }
}

TEST_CASE("Tests catch a network that stops working", "[integration][workflow]") {
    app::Application application;
    app::buildSampleProject(application.project());

    // The sample ships with three assertions, and they all hold.
    testing::TestRunSummary summary = application.runTests();
    REQUIRE(summary.results.size() == 3);
    INFO(summary.summaryLine());
    CHECK(summary.allPassed());

    // Break the network the way a person would: shut an interface down.
    Device* router = application.project().network().findDeviceByName("Router1");
    REQUIRE(router != nullptr);
    router->findInterfaceByName("Gi0/1")->setAdminState(AdminState::Down);
    application.project().network().refreshOperationalStates();

    summary = application.runTests();
    CHECK_FALSE(summary.allPassed());
    CHECK(summary.failedCount() == 1); // the end-to-end test

    const auto broken = std::find_if(summary.results.begin(), summary.results.end(),
                                     [](const testing::TestResult& result) {
                                         return result.testName == "PC1 can reach Server1";
                                     });
    REQUIRE(broken != summary.results.end());
    CHECK_FALSE(broken->passed());
    CHECK_FALSE(broken->reason.empty()); // the engine explains why

    // The local test still passes: the break is where the tests say it is.
    const auto local = std::find_if(summary.results.begin(), summary.results.end(),
                                    [](const testing::TestResult& result) {
                                        return result.testName == "PC1 can reach its gateway";
                                    });
    REQUIRE(local != summary.results.end());
    CHECK(local->passed());
}

TEST_CASE("Running tests does not disturb the live simulation", "[integration][workflow]") {
    app::Application application;
    app::buildSampleProject(application.project());

    Device* pc = application.project().network().findDeviceByName("PC1");
    REQUIRE(pc != nullptr);

    // Warm the ARP cache with a live run.
    sim::Simulator& simulator = application.simulator();
    simulator.start();

    PingRequest request;
    request.destination = ipv4("192.168.1.1");
    request.count = 1;
    REQUIRE(simulator.ping(pc->id(), request).isOk());
    simulator.runUntilIdle(seconds(5));

    const std::size_t cachedBefore = pc->arpCache()->size();
    REQUIRE(cachedBefore > 0);

    // Running the suite happens on a private copy of the project.
    (void)application.runTests();

    CHECK(pc->arpCache()->size() == cachedBefore);
    CHECK(simulator.state() == sim::SimulationState::Running);
}

TEST_CASE("The topology exports to SVG", "[integration][export]") {
    app::Application application;
    app::buildSampleProject(application.project());

    auto svg = application.exportSvg();
    REQUIRE(svg.isOk());

    const std::string& document = svg.value();
    CHECK(document.find("<svg") == 0);
    CHECK(document.find("</svg>") != std::string::npos);
    CHECK(document.find("PC1") != std::string::npos);
    CHECK(document.find("Server1") != std::string::npos);
    CHECK(document.find("192.168.1.10/24") != std::string::npos);
    CHECK(document.find("Office LAN") != std::string::npos); // the annotation
    CHECK(document.find("<line") != std::string::npos);      // the cables

    SECTION("an empty project has nothing to export") {
        app::Application empty;
        CHECK_FALSE(empty.exportSvg().isOk());
    }
}

TEST_CASE("Learning mode narrates from structured events", "[integration][learning]") {
    app::Application application;
    app::buildSampleProject(application.project());
    application.setLearningModeEnabled(true);

    Device* pc = application.project().network().findDeviceByName("PC1");
    REQUIRE(pc != nullptr);

    sim::Simulator& simulator = application.simulator();
    simulator.start();

    PingRequest request;
    request.destination = ipv4("172.16.0.20");
    request.count = 1;
    REQUIRE(simulator.ping(pc->id(), request).isOk());
    simulator.runUntilIdle(seconds(10));

    const auto& steps = application.learning().steps();
    REQUIRE_FALSE(steps.empty());

    // The narration follows the real exchange, in order.
    const auto findStep = [&](std::string_view needle) {
        return std::find_if(steps.begin(), steps.end(), [needle](const app::LearningStep& step) {
            return step.headline.find(needle) != std::string::npos;
        });
    };

    CHECK(findStep("does not know the MAC address") != steps.end());
    CHECK(findStep("broadcasts an ARP request") != steps.end());
    CHECK(findStep("forwards the packet") != steps.end());
    CHECK(findStep("Reply received") != steps.end());

    for (const app::LearningStep& step : steps) {
        CHECK_FALSE(step.headline.empty());
        CHECK_FALSE(step.explanation.empty());
    }

    SECTION("turning it off stops the narration") {
        application.setLearningModeEnabled(false);
        CHECK(application.learning().steps().empty());
    }
}

TEST_CASE("Autosave produces a recoverable session", "[integration][recovery]") {
    const ScratchDirectory scratch;

    app::Application application;
    application.initialize();
    application.config().autosaveEnabled = true;
    application.config().autosaveIntervalSeconds = 15; // the minimum the config allows

    app::buildSampleProject(application.project());
    application.addDevice(DeviceType::Pc, Vec2{0, 200}); // make the document dirty
    REQUIRE(application.isDirty());

    // Advance past the autosave interval.
    application.update(seconds(20));

    const auto autosave = files::userStateDirectory() / "recovery.tnpjson";
    const auto marker = files::userStateDirectory() / "recovery.json";
    CHECK(std::filesystem::exists(autosave));
    CHECK(std::filesystem::exists(marker));

    // A clean shutdown removes the recovery files: there is nothing to recover.
    application.shutdown();
    CHECK_FALSE(std::filesystem::exists(autosave));
    CHECK_FALSE(std::filesystem::exists(marker));
}
