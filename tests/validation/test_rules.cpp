#include "TestHelpers.h"

#include "app/SampleProject.h"
#include "core/devices/DhcpServer.h"
#include "core/devices/SwitchingEngine.h"
#include "validation/NetworkValidator.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::tests;
using namespace tnp::validation;

namespace {

bool hasIssue(const ValidationReport& report, std::string_view code) {
    return std::any_of(report.issues.begin(), report.issues.end(),
                       [code](const ValidationIssue& issue) { return issue.code == code; });
}

std::size_t countIssues(const ValidationReport& report, std::string_view code) {
    return static_cast<std::size_t>(
        std::count_if(report.issues.begin(), report.issues.end(),
                      [code](const ValidationIssue& issue) { return issue.code == code; }));
}

ValidationReport validate(const Project& project) {
    return NetworkValidator{}.validate(project);
}

} // namespace

TEST_CASE("The sample project has no errors", "[validation]") {
    Project project;
    app::buildSampleProject(project);

    const ValidationReport report = validate(project);
    INFO((report.issues.empty() ? std::string{} : report.issues.front().message));
    CHECK_FALSE(report.hasErrors());
}

TEST_CASE("An empty project is reported as such", "[validation]") {
    Project project;
    const ValidationReport report = validate(project);

    CHECK(hasIssue(report, "empty-project"));
    CHECK_FALSE(report.hasErrors());
}

TEST_CASE("Duplicate addresses are errors on both interfaces", "[validation]") {
    Project project;
    Device& a = addDevice(project.network(), DeviceType::Pc, "PC1");
    Device& b = addDevice(project.network(), DeviceType::Pc, "PC2");

    assign(a, "Gi0", "192.168.1.10/24");
    assign(b, "Gi0", "192.168.1.10/24");

    const ValidationReport report = validate(project);
    CHECK(report.hasErrors());
    CHECK(countIssues(report, "duplicate-ipv4-address") == 2);

    const auto forPc1 = report.forObject(ObjectRef::device(a.id()));
    CHECK_FALSE(forPc1.empty());
}

TEST_CASE("Duplicate MAC addresses are errors", "[validation]") {
    Project project;
    Device& a = addDevice(project.network(), DeviceType::Pc, "PC1");
    Device& b = addDevice(project.network(), DeviceType::Pc, "PC2");

    iface(b, "Gi0").setMacAddress(iface(a, "Gi0").macAddress());

    const ValidationReport report = validate(project);
    CHECK(hasIssue(report, "duplicate-mac-address"));
    CHECK(report.hasErrors());
}

TEST_CASE("Cabled interfaces in different subnets are an error", "[validation]") {
    Project project;
    Network& network = project.network();

    Device& a = addDevice(network, DeviceType::Router, "R1");
    Device& b = addDevice(network, DeviceType::Router, "R2");

    connect(network, a, "Gi0/0", b, "Gi0/0");
    assign(a, "Gi0/0", "192.168.1.1/24");
    assign(b, "Gi0/0", "10.0.0.1/24");
    network.refreshOperationalStates();

    const ValidationReport report = validate(project);
    CHECK(hasIssue(report, "link-subnet-mismatch"));
    CHECK(report.hasErrors());
}

TEST_CASE("Topology problems are reported at the right severity", "[validation]") {
    Project project;
    Network& network = project.network();

    SECTION("an isolated device") {
        addDevice(network, DeviceType::Pc, "Lonely");
        CHECK(hasIssue(validate(project), "isolated-device"));
    }

    SECTION("a cabled interface that is shut down") {
        Device& a = addDevice(network, DeviceType::Pc, "PC1");
        Device& b = addDevice(network, DeviceType::Switch, "SW1");
        connect(network, a, "Gi0", b, "Gi0/1");
        iface(a, "Gi0").setAdminState(AdminState::Down);
        network.refreshOperationalStates();

        CHECK(hasIssue(validate(project), "interface-shutdown"));
    }

    SECTION("mismatched MTUs across a link") {
        Device& a = addDevice(network, DeviceType::Pc, "PC1");
        Device& b = addDevice(network, DeviceType::Switch, "SW1");
        connect(network, a, "Gi0", b, "Gi0/1");
        REQUIRE(iface(a, "Gi0").setMtu(9000).isOk());

        CHECK(hasIssue(validate(project), "mtu-mismatch"));
    }

    SECTION("a routing interface with no address") {
        Device& router = addDevice(network, DeviceType::Router, "R1");
        Device& sw = addDevice(network, DeviceType::Switch, "SW1");
        connect(network, router, "Gi0/0", sw, "Gi0/1");
        network.refreshOperationalStates();

        CHECK(hasIssue(validate(project), "router-interface-without-address"));
    }
}

TEST_CASE("A switching loop is detected", "[validation]") {
    Project project;
    Network& network = project.network();

    Device& a = addDevice(network, DeviceType::Switch, "SW1");
    Device& b = addDevice(network, DeviceType::Switch, "SW2");
    Device& c = addDevice(network, DeviceType::Switch, "SW3");

    connect(network, a, "Gi0/1", b, "Gi0/1");
    connect(network, b, "Gi0/2", c, "Gi0/1");

    CHECK_FALSE(hasIssue(validate(project), "switching-loop"));

    // Closing the triangle creates a loop no spanning tree will break.
    connect(network, c, "Gi0/2", a, "Gi0/2");
    CHECK(hasIssue(validate(project), "switching-loop"));
}

TEST_CASE("An addressed host with no default gateway is a warning", "[validation]") {
    Project project;
    Network& network = project.network();

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "SW1");
    connect(network, pc, "Gi0", sw, "Gi0/1");
    assign(pc, "Gi0", "192.168.1.10/24");
    network.refreshOperationalStates();

    CHECK(hasIssue(validate(project), "host-without-default-gateway"));

    pc.ipv4Stack()->setDefaultGateway(ipv4("192.168.1.1"));
    CHECK_FALSE(hasIssue(validate(project), "host-without-default-gateway"));
}

TEST_CASE("A port in an undefined VLAN is a warning", "[validation]") {
    Project project;
    Device& sw = addDevice(project.network(), DeviceType::Switch, "SW1");

    iface(sw, "Gi0/1").vlan().accessVlan = 99;
    CHECK(hasIssue(validate(project), "undefined-vlan"));

    sw.switching()->addVlan(VlanDefinition{99, "guests"});
    CHECK_FALSE(hasIssue(validate(project), "undefined-vlan"));
}

TEST_CASE("An unusable DHCP pool is reported", "[validation]") {
    Project project;
    Device& server = addDevice(project.network(), DeviceType::Server, "Server1");
    assign(server, "Gi0", "192.168.1.10/24");

    server.dhcpServer()->setEnabled(true);
    CHECK(hasIssue(validate(project), "dhcp-pool-invalid")); // enabled with no pool

    SECTION("a range outside its own subnet") {
        DhcpPool pool;
        pool.name = "broken";
        pool.subnet = prefix("192.168.1.0/24");
        pool.rangeFirst = ipv4("10.0.0.1");
        pool.rangeLast = ipv4("10.0.0.50");
        server.dhcpServer()->addPool(pool);

        const ValidationReport report = validate(project);
        CHECK(hasIssue(report, "dhcp-pool-invalid"));
        CHECK(report.hasErrors());
    }

    SECTION("a valid pool on a served subnet is fine") {
        DhcpPool pool;
        pool.name = "users";
        pool.subnet = prefix("192.168.1.0/24");
        pool.rangeFirst = ipv4("192.168.1.100");
        pool.rangeLast = ipv4("192.168.1.200");
        server.dhcpServer()->addPool(pool);

        CHECK_FALSE(hasIssue(validate(project), "dhcp-pool-invalid"));
    }
}

TEST_CASE("Configured but unsimulated features are disclosed", "[validation]") {
    Project project;
    Device& router = addDevice(project.network(), DeviceType::Router, "R1");

    OspfConfiguration& ospf = router.ipv4Stack()->ospf();
    ospf.enabled = true;
    ospf.networks.push_back(OspfNetworkStatement{prefix("10.0.0.0/8"), 0});

    const ValidationReport report = validate(project);
    CHECK(hasIssue(report, "feature-not-simulated"));

    const auto issue = std::find_if(report.issues.begin(), report.issues.end(),
                                    [](const ValidationIssue& entry) {
                                        return entry.code == "feature-not-simulated";
                                    });
    REQUIRE(issue != report.issues.end());
    CHECK(issue->severity == Severity::Info); // honest, not alarming
}

TEST_CASE("Broken test references are errors", "[validation]") {
    Project project;
    Device& pc = addDevice(project.network(), DeviceType::Pc, "PC1");

    NetworkTest test;
    test.name = "goes nowhere";
    test.source = pc.id();
    project.addTest(test); // no destination at all

    CHECK(hasIssue(validate(project), "test-reference-broken"));
}

TEST_CASE("Duplicate device names are errors", "[validation]") {
    Project project;
    addDevice(project.network(), DeviceType::Pc, "Same");
    addDevice(project.network(), DeviceType::Router, "Same");

    CHECK(hasIssue(validate(project), "duplicate-device-name"));
}

TEST_CASE("Rules can be disabled individually", "[validation]") {
    Project project;
    addDevice(project.network(), DeviceType::Pc, "Lonely");

    NetworkValidator validator;
    CHECK(hasIssue(validator.validate(project), "isolated-device"));

    validator.setRuleEnabled("isolated-device", false);
    CHECK_FALSE(validator.isRuleEnabled("isolated-device"));
    CHECK_FALSE(hasIssue(validator.validate(project), "isolated-device"));

    validator.setRuleEnabled("isolated-device", true);
    CHECK(hasIssue(validator.validate(project), "isolated-device"));
}

TEST_CASE("Reports are ordered with the most serious first", "[validation]") {
    Project project;
    Device& a = addDevice(project.network(), DeviceType::Pc, "PC1");
    Device& b = addDevice(project.network(), DeviceType::Pc, "PC2");
    assign(a, "Gi0", "192.168.1.10/24");
    assign(b, "Gi0", "192.168.1.10/24");

    const ValidationReport report = validate(project);
    REQUIRE(report.issues.size() >= 2);

    for (std::size_t i = 1; i < report.issues.size(); ++i) {
        CHECK(report.issues[i - 1].severity >= report.issues[i].severity);
    }
    CHECK(report.errorCount() + report.warningCount() + report.infoCount() == report.issues.size());
}

TEST_CASE("Every built-in rule has an identifier and a description", "[validation]") {
    const NetworkValidator validator;
    CHECK(validator.rules().size() >= 15);

    for (const auto& rule : validator.rules()) {
        CHECK_FALSE(rule->id().empty());
        CHECK_FALSE(rule->description().empty());
    }
}
