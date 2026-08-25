#include "TestHelpers.h"

#include "app/Application.h"
#include "app/SampleProject.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::tests;

namespace {

/// Runs a command whose output the test does not examine.
void run(cli::DeviceShell& shell, std::string_view command) {
    (void)shell.execute(command);
}

/// All output of a command as one string, for substring assertions.
std::string textOf(const cli::ShellResponse& response) {
    std::string text;
    for (const cli::ShellLine& line : response.lines) text += line.text + "\n";
    return text;
}

bool contains(const cli::ShellResponse& response, std::string_view needle) {
    return textOf(response).find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("The console reads the real device model", "[cli]") {
    app::Application application;
    app::buildSampleProject(application.project());

    Device* router = application.project().network().findDeviceByName("Router1");
    REQUIRE(router != nullptr);

    cli::DeviceShell& shell = application.shell();
    shell.attachTo(router->id());
    CHECK(shell.prompt() == "Router1#");

    SECTION("show ip route prints the table the forwarding path uses") {
        const auto response = shell.execute("show ip route");
        CHECK_FALSE(response.isError);
        CHECK(contains(response, "192.168.1.0/24"));
        CHECK(contains(response, "10.0.0.0/30"));
        CHECK(contains(response, "172.16.0.0/24")); // the static route
        CHECK(contains(response, "C ")); // connected
        CHECK(contains(response, "S ")); // static

        // Removing the route removes it from the output; nothing is canned.
        router->ipv4Stack()->setStaticRoutes({});
        CHECK_FALSE(contains(shell.execute("show ip route"), "172.16.0.0/24"));
    }

    SECTION("show ip interface brief reflects interface state") {
        const auto response = shell.execute("show ip int brief");
        CHECK(contains(response, "GigabitEthernet0/0"));
        CHECK(contains(response, "192.168.1.1"));
        CHECK(contains(response, "unassigned")); // the unused ports

        router->findInterfaceByName("Gi0/0")->setAdminState(AdminState::Down);
        CHECK(contains(shell.execute("show ip interface brief"), "admin down"));
    }

    SECTION("abbreviations work the way a network CLI does") {
        CHECK_FALSE(shell.execute("sh ip ro").isError);
        CHECK_FALSE(shell.execute("sho ver").isError);
        CHECK_FALSE(shell.execute("show int").isError);
    }

    SECTION("unknown commands are reported, not silently ignored") {
        const auto response = shell.execute("show nonsense");
        CHECK(response.isError);
    }
}

TEST_CASE("Configuration through the console goes through the undo stack", "[cli]") {
    app::Application application;
    Network& network = application.project().network();

    Device& router = addDevice(network, DeviceType::Router, "R1");
    cli::DeviceShell& shell = application.shell();
    shell.attachTo(router.id());

    REQUIRE_FALSE(shell.execute("configure terminal").isError);
    CHECK(shell.prompt() == "R1(config)#");

    REQUIRE_FALSE(shell.execute("interface GigabitEthernet0/0").isError);
    CHECK(shell.prompt() == "R1(config-if)#");

    REQUIRE_FALSE(shell.execute("ip address 192.168.1.1 255.255.255.0").isError);
    CHECK(iface(router, "Gi0/0").ipv4Addresses().size() == 1);
    CHECK(iface(router, "Gi0/0").primaryIpv4()->toString() == "192.168.1.1/24");

    REQUIRE_FALSE(shell.execute("description uplink to the core").isError);
    CHECK(iface(router, "Gi0/0").description() == "uplink to the core");

    REQUIRE_FALSE(shell.execute("shutdown").isError);
    CHECK_FALSE(iface(router, "Gi0/0").isAdminUp());

    REQUIRE_FALSE(shell.execute("no shutdown").isError);
    CHECK(iface(router, "Gi0/0").isAdminUp());

    run(shell, "exit");
    CHECK(shell.prompt() == "R1(config)#");

    REQUIRE_FALSE(shell.execute("hostname CoreRouter").isError);
    CHECK(router.name() == "CoreRouter");

    // Every one of those was a command, so the whole session is reversible.
    while (application.commands().canUndo()) application.commands().undo();

    CHECK(router.name() == "R1");
    CHECK(iface(router, "Gi0/0").ipv4Addresses().empty());
    CHECK(iface(router, "Gi0/0").description().empty());
}

TEST_CASE("CIDR notation is accepted alongside a mask", "[cli]") {
    app::Application application;
    Device& router = addDevice(application.project().network(), DeviceType::Router, "R1");

    cli::DeviceShell& shell = application.shell();
    shell.attachTo(router.id());
    run(shell, "configure terminal");
    run(shell, "interface Gi0/0");

    REQUIRE_FALSE(shell.execute("ip address 10.0.0.1/30").isError);
    CHECK(iface(router, "Gi0/0").primaryIpv4()->toString() == "10.0.0.1/30");
}

TEST_CASE("Invalid configuration is rejected with a reason", "[cli]") {
    app::Application application;
    Device& router = addDevice(application.project().network(), DeviceType::Router, "R1");

    cli::DeviceShell& shell = application.shell();
    shell.attachTo(router.id());
    run(shell, "configure terminal");
    run(shell, "interface Gi0/0");

    SECTION("the network address is not a host address") {
        const auto response = shell.execute("ip address 192.168.1.0 255.255.255.0");
        CHECK(response.isError);
        CHECK(iface(router, "Gi0/0").ipv4Addresses().empty());
    }
    SECTION("a nonsensical MTU") {
        CHECK(shell.execute("mtu 3").isError);
        CHECK(iface(router, "Gi0/0").mtu() == kDefaultMtu);
    }
    SECTION("an interface that does not exist") {
        run(shell, "exit");
        CHECK(shell.execute("interface Gi9/9").isError);
    }
}

TEST_CASE("Static routes can be configured and removed from the console", "[cli]") {
    app::Application application;
    Network& network = application.project().network();

    Device& router = addDevice(network, DeviceType::Router, "R1");
    Device& peer = addDevice(network, DeviceType::Router, "R2");
    connect(network, router, "Gi0/1", peer, "Gi0/0");
    assign(router, "Gi0/1", "10.0.0.1/30");
    network.refreshOperationalStates();

    cli::DeviceShell& shell = application.shell();
    shell.attachTo(router.id());
    run(shell, "configure terminal");

    REQUIRE_FALSE(shell.execute("ip route 172.16.0.0 255.255.255.0 10.0.0.2").isError);
    CHECK(router.ipv4Stack()->staticRoutes().size() == 1);
    CHECK(router.routingTable()->lookup(ipv4("172.16.0.5")) != nullptr);

    REQUIRE_FALSE(shell.execute("no ip route 172.16.0.0 255.255.255.0").isError);
    CHECK(router.ipv4Stack()->staticRoutes().empty());

    SECTION("a next hop that is not on a connected subnet is refused") {
        const auto response = shell.execute("ip route 10.9.0.0 255.255.0.0 203.0.113.1");
        CHECK(response.isError);
        CHECK(router.ipv4Stack()->staticRoutes().empty());
    }
}

TEST_CASE("Ping from the console drives the real simulator", "[cli]") {
    app::Application application;
    app::buildSampleProject(application.project());

    Device* pc = application.project().network().findDeviceByName("PC1");
    REQUIRE(pc != nullptr);

    cli::DeviceShell& shell = application.shell();
    shell.attachTo(pc->id());

    const auto response = shell.execute("ping 172.16.0.20 count 2");
    CHECK_FALSE(response.isError);
    CHECK(contains(response, "Pinging 172.16.0.20"));

    application.simulator().runUntilIdle(seconds(10));

    // Results arrive asynchronously, exactly as they do in a real console.
    const auto events = shell.drainEvents();
    CHECK_FALSE(events.empty());

    const bool sawReply = std::any_of(events.begin(), events.end(), [](const cli::ShellLine& line) {
        return line.text.find("reply from 172.16.0.20") != std::string::npos;
    });
    CHECK(sawReply);
}

TEST_CASE("Ping accepts a device name as an editor convenience", "[cli]") {
    app::Application application;
    app::buildSampleProject(application.project());

    Device* pc = application.project().network().findDeviceByName("PC1");
    cli::DeviceShell& shell = application.shell();
    shell.attachTo(pc->id());

    CHECK_FALSE(shell.execute("ping Server1 count 1").isError);

    // A hostname is not accepted: TNP has no DNS client, and pretending would be
    // worse than saying so.
    const auto response = shell.execute("ping server.local");
    CHECK(response.isError);
}

TEST_CASE("show arp and show mac address-table reflect simulation state", "[cli]") {
    app::Application application;
    app::buildSampleProject(application.project());

    Device* pc = application.project().network().findDeviceByName("PC1");
    Device* sw = application.project().network().findDeviceByName("Switch1");
    REQUIRE(pc != nullptr);
    REQUIRE(sw != nullptr);

    cli::DeviceShell& shell = application.shell();
    shell.attachTo(pc->id());

    CHECK(contains(shell.execute("show arp"), "empty"));

    run(shell, "ping 172.16.0.20 count 1");
    application.simulator().runUntilIdle(seconds(10));

    CHECK(contains(shell.execute("show arp"), "192.168.1.1"));

    shell.attachTo(sw->id());
    const auto macTable = shell.execute("show mac address-table");
    CHECK(contains(macTable, "Gi0/1"));
    CHECK(contains(macTable, "dynamic"));

    // A switch has no ARP cache, and says so rather than printing nothing.
    CHECK(shell.execute("show arp").isError);

    run(shell, "clear mac address-table");
    CHECK(contains(shell.execute("show mac address-table"), "empty"));
}

TEST_CASE("show running-config is generated from the model", "[cli]") {
    app::Application application;
    app::buildSampleProject(application.project());

    Device* router = application.project().network().findDeviceByName("Router1");
    cli::DeviceShell& shell = application.shell();
    shell.attachTo(router->id());

    const auto response = shell.execute("show running-config");
    const std::string text = textOf(response);

    CHECK(text.find("hostname Router1") != std::string::npos);
    CHECK(text.find("interface GigabitEthernet0/0") != std::string::npos);
    CHECK(text.find("ip address 192.168.1.1 255.255.255.0") != std::string::npos);
    CHECK(text.find("ip route 172.16.0.0 255.255.255.0 10.0.0.2") != std::string::npos);
    CHECK(text.find("end") != std::string::npos);
}

TEST_CASE("Completion offers commands for the current mode", "[cli]") {
    app::Application application;
    Device& router = addDevice(application.project().network(), DeviceType::Router, "R1");

    cli::DeviceShell& shell = application.shell();
    shell.attachTo(router.id());

    CHECK_FALSE(shell.completions("sh").empty());
    CHECK(shell.completions("zzz").empty());

    run(shell, "configure terminal");
    const auto configCompletions = shell.completions("int");
    CHECK_FALSE(configCompletions.empty());

    // Interface names are completed too, since they are what follows.
    const bool offersInterface = std::any_of(configCompletions.begin(), configCompletions.end(),
                                             [](const std::string& candidate) {
                                                 return candidate.find("GigabitEthernet0/0") !=
                                                        std::string::npos;
                                             });
    CHECK(offersInterface);
}

TEST_CASE("A console with no device attached refuses commands politely", "[cli]") {
    app::Application application;
    cli::DeviceShell& shell = application.shell();

    CHECK(shell.prompt() == "(no device)>");
    const auto response = shell.execute("show ip route");
    CHECK(response.isError);
    CHECK(contains(response, "No device is selected"));
}
