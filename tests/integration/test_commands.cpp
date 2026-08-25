#include "TestHelpers.h"

#include "commands/CommandManager.h"
#include "commands/DeviceCommands.h"
#include "commands/InterfaceCommands.h"
#include "commands/LinkCommands.h"
#include "commands/ProjectCommands.h"
#include "commands/RoutingCommands.h"

#include <catch2/catch_test_macros.hpp>

using namespace tnp;
using namespace tnp::core;
using namespace tnp::commands;
using namespace tnp::tests;

TEST_CASE("Adding a device is undoable and redoable", "[commands]") {
    Project project;
    CommandManager manager{project};

    auto command = std::make_unique<AddDeviceCommand>(DeviceType::Router, "Router1", Vec2{10, 20});
    const DeviceId id = command->deviceId();

    REQUIRE(manager.run(std::move(command)));
    CHECK(project.network().deviceCount() == 1);
    CHECK(project.layout().position(id) == Vec2{10, 20});
    CHECK(manager.canUndo());
    CHECK(manager.undoLabel() == "Add Router1");

    REQUIRE(manager.undo());
    CHECK(project.network().deviceCount() == 0);
    CHECK_FALSE(project.layout().has(id));
    CHECK(manager.canRedo());

    REQUIRE(manager.redo());
    CHECK(project.network().deviceCount() == 1);
    CHECK(project.network().findDevice(id) != nullptr); // the same identity comes back
    CHECK(project.layout().position(id) == Vec2{10, 20});
}

TEST_CASE("Undoing a delete restores the device, its configuration and its links", "[commands]") {
    Project project;
    Network& network = project.network();
    CommandManager manager{project};

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");
    Device& router = addDevice(network, DeviceType::Router, "Router1");

    connect(network, pc, "Gi0", sw, "Gi0/1");
    connect(network, sw, "Gi0/2", router, "Gi0/0");
    assign(router, "Gi0/0", "192.168.1.1/24");
    network.refreshOperationalStates();

    const DeviceId switchId = sw.id();
    project.layout().setPosition(switchId, Vec2{100, 100});

    REQUIRE(manager.run(std::make_unique<DeleteDevicesCommand>(std::vector<DeviceId>{switchId})));
    CHECK(network.deviceCount() == 2);
    CHECK(network.linkCount() == 0);

    REQUIRE(manager.undo());
    CHECK(network.deviceCount() == 3);
    CHECK(network.linkCount() == 2);
    CHECK(project.layout().position(switchId) == Vec2{100, 100});
    CHECK(iface(pc, "Gi0").isOperational());

    // The router kept its address through the whole cycle.
    CHECK(iface(router, "Gi0/0").ipv4Addresses().size() == 1);
    CHECK(router.routingTable()->lookup(ipv4("192.168.1.50")) != nullptr);
}

TEST_CASE("Moves merge into one undo step per drag", "[commands]") {
    Project project;
    CommandManager manager{project};

    auto add = std::make_unique<AddDeviceCommand>(DeviceType::Pc, "PC1", Vec2{0, 0});
    const DeviceId id = add->deviceId();
    REQUIRE(manager.run(std::move(add)));

    const std::vector<DeviceId> selection{id};
    const std::size_t before = manager.undoDepth();

    // A drag produces one command per frame.
    for (int i = 0; i < 10; ++i) {
        REQUIRE(manager.run(std::make_unique<MoveDevicesCommand>(selection, Vec2{5, 0})));
    }

    CHECK(project.layout().position(id) == Vec2{50, 0});
    CHECK(manager.undoDepth() == before + 1); // ten frames, one undo entry

    REQUIRE(manager.undo());
    CHECK(project.layout().position(id) == Vec2{0, 0});
}

TEST_CASE("Breaking the merge chain starts a new undo entry", "[commands]") {
    Project project;
    CommandManager manager{project};

    auto add = std::make_unique<AddDeviceCommand>(DeviceType::Pc, "PC1", Vec2{0, 0});
    const DeviceId id = add->deviceId();
    REQUIRE(manager.run(std::move(add)));

    const std::vector<DeviceId> selection{id};
    REQUIRE(manager.run(std::make_unique<MoveDevicesCommand>(selection, Vec2{10, 0})));

    manager.breakMergeChain(); // the user released the mouse
    REQUIRE(manager.run(std::make_unique<MoveDevicesCommand>(selection, Vec2{10, 0})));

    CHECK(project.layout().position(id) == Vec2{20, 0});

    REQUIRE(manager.undo());
    CHECK(project.layout().position(id) == Vec2{10, 0});
}

TEST_CASE("A locked device does not move", "[commands]") {
    Project project;
    CommandManager manager{project};

    auto add = std::make_unique<AddDeviceCommand>(DeviceType::Pc, "PC1", Vec2{0, 0});
    const DeviceId id = add->deviceId();
    REQUIRE(manager.run(std::move(add)));

    project.layout().setLocked(id, true);
    CHECK_FALSE(manager.run(std::make_unique<MoveDevicesCommand>(std::vector<DeviceId>{id},
                                                                Vec2{50, 50})));
    CHECK(project.layout().position(id) == Vec2{0, 0});
}

TEST_CASE("Connecting and disconnecting round-trip through undo", "[commands]") {
    Project project;
    Network& network = project.network();
    CommandManager manager{project};

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "Switch1");

    auto connectCommand = std::make_unique<ConnectInterfacesCommand>(iface(pc, "Gi0").id(),
                                                                     iface(sw, "Gi0/1").id());
    REQUIRE(manager.run(std::move(connectCommand)));
    REQUIRE(network.linkCount() == 1);

    const LinkId linkId = network.links().front()->id();

    REQUIRE(manager.undo());
    CHECK(network.linkCount() == 0);
    CHECK_FALSE(iface(pc, "Gi0").isConnected());

    REQUIRE(manager.redo());
    CHECK(network.linkCount() == 1);
    CHECK(network.links().front()->id() == linkId); // the link keeps its identity

    REQUIRE(manager.run(std::make_unique<DisconnectLinksCommand>(std::vector<LinkId>{linkId})));
    CHECK(network.linkCount() == 0);

    REQUIRE(manager.undo());
    CHECK(network.linkCount() == 1);
    CHECK(iface(pc, "Gi0").isOperational());
}

TEST_CASE("A refused connection changes nothing", "[commands]") {
    Project project;
    Network& network = project.network();
    CommandManager manager{project};

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& router = addDevice(network, DeviceType::Router, "R1");

    CHECK_FALSE(manager.run(std::make_unique<ConnectInterfacesCommand>(
        iface(pc, "Gi0").id(), iface(router, "Se0/0/0").id())));

    CHECK(network.linkCount() == 0);
    CHECK_FALSE(manager.canUndo()); // a no-op must not enter the history
    CHECK_FALSE(manager.lastFailure().empty());
}

TEST_CASE("Interface configuration is one undoable edit", "[commands]") {
    Project project;
    Network& network = project.network();
    CommandManager manager{project};

    Device& router = addDevice(network, DeviceType::Router, "R1");
    Interface& port = iface(router, "Gi0/0");

    InterfaceSettings settings = InterfaceSettings::capture(port);
    settings.description = "uplink";
    settings.mtu = 9000;
    settings.ipv4.push_back(prefix("10.0.0.1/24"));
    settings.vlan.accessVlan = 20;

    REQUIRE(manager.run(std::make_unique<ConfigureInterfaceCommand>(router.id(), port.id(),
                                                                    settings)));
    CHECK(port.description() == "uplink");
    CHECK(port.mtu() == 9000);
    CHECK(port.ipv4Addresses().size() == 1);
    CHECK(port.vlan().accessVlan == 20);

    REQUIRE(manager.undo());
    CHECK(port.description().empty());
    CHECK(port.mtu() == kDefaultMtu);
    CHECK(port.ipv4Addresses().empty());
    CHECK(port.vlan().accessVlan == kDefaultVlan);
}

TEST_CASE("Addresses can be added and removed one at a time", "[commands]") {
    Project project;
    Network& network = project.network();
    CommandManager manager{project};

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Interface& port = iface(pc, "Gi0");

    REQUIRE(manager.run(std::make_unique<AddIpv4AddressCommand>(pc.id(), port.id(),
                                                                prefix("192.168.1.10/24"))));
    CHECK(port.ipv4Addresses().size() == 1);

    CHECK_FALSE(manager.run(std::make_unique<AddIpv4AddressCommand>(pc.id(), port.id(),
                                                                    prefix("192.168.2.0/24"))));
    CHECK_FALSE(manager.lastFailure().empty());
    CHECK(port.ipv4Addresses().size() == 1);

    REQUIRE(manager.run(std::make_unique<RemoveIpv4AddressCommand>(pc.id(), port.id(),
                                                                   prefix("192.168.1.10/24"))));
    CHECK(port.ipv4Addresses().empty());

    REQUIRE(manager.undo());
    CHECK(port.ipv4Addresses().size() == 1);
}

TEST_CASE("Routing changes are undoable", "[commands]") {
    Project project;
    Network& network = project.network();
    CommandManager manager{project};

    Device& router = addDevice(network, DeviceType::Router, "R1");
    Device& peer = addDevice(network, DeviceType::Router, "R2");
    connect(network, router, "Gi0/1", peer, "Gi0/0");
    assign(router, "Gi0/1", "10.0.0.1/30");
    network.refreshOperationalStates();

    StaticRouteEntry entry;
    entry.destination = prefix("172.16.0.0/24");
    entry.nextHop = ipv4("10.0.0.2");

    REQUIRE(manager.run(std::make_unique<AddStaticRouteCommand>(router.id(), entry)));
    CHECK(router.ipv4Stack()->staticRoutes().size() == 1);
    CHECK(router.routingTable()->lookup(ipv4("172.16.0.5")) != nullptr);

    REQUIRE(manager.undo());
    CHECK(router.ipv4Stack()->staticRoutes().empty());
    CHECK(router.routingTable()->lookup(ipv4("172.16.0.5")) == nullptr);

    REQUIRE(manager.run(std::make_unique<SetDefaultGatewayCommand>(router.id(), ipv4("10.0.0.2"))));
    CHECK(router.ipv4Stack()->defaultGateway() == ipv4("10.0.0.2"));

    REQUIRE(manager.undo());
    CHECK_FALSE(router.ipv4Stack()->defaultGateway().has_value());
}

TEST_CASE("Annotations and tests are undoable", "[commands]") {
    Project project;
    CommandManager manager{project};

    Annotation annotation;
    annotation.kind = AnnotationKind::Text;
    annotation.text = "Core";
    auto addAnnotation = std::make_unique<AddAnnotationCommand>(annotation);
    const AnnotationId annotationId = addAnnotation->annotationId();

    REQUIRE(manager.run(std::move(addAnnotation)));
    CHECK(project.annotations().size() == 1);

    REQUIRE(manager.undo());
    CHECK(project.annotations().empty());
    REQUIRE(manager.redo());
    CHECK(project.findAnnotation(annotationId) != nullptr);

    Device& pc = addDevice(project.network(), DeviceType::Pc, "PC1");
    NetworkTest test;
    test.name = "reach the gateway";
    test.source = pc.id();
    test.destinationAddress = ipv4("192.168.1.1");

    REQUIRE(manager.run(std::make_unique<AddTestCommand>(test)));
    CHECK(project.tests().size() == 1);
    REQUIRE(manager.undo());
    CHECK(project.tests().empty());
}

TEST_CASE("The dirty flag tracks distance from the last save", "[commands]") {
    Project project;
    CommandManager manager{project};

    CHECK_FALSE(manager.isDirty());

    REQUIRE(manager.run(std::make_unique<AddDeviceCommand>(DeviceType::Pc, "PC1", Vec2{})));
    CHECK(manager.isDirty());

    manager.markSaved();
    CHECK_FALSE(manager.isDirty());

    REQUIRE(manager.run(std::make_unique<AddDeviceCommand>(DeviceType::Pc, "PC2", Vec2{})));
    CHECK(manager.isDirty());

    // Undoing back to the saved point makes the document clean again.
    REQUIRE(manager.undo());
    CHECK_FALSE(manager.isDirty());

    REQUIRE(manager.redo());
    CHECK(manager.isDirty());
}

TEST_CASE("A new change discards the redo branch", "[commands]") {
    Project project;
    CommandManager manager{project};

    REQUIRE(manager.run(std::make_unique<AddDeviceCommand>(DeviceType::Pc, "PC1", Vec2{})));
    REQUIRE(manager.undo());
    CHECK(manager.canRedo());

    REQUIRE(manager.run(std::make_unique<AddDeviceCommand>(DeviceType::Router, "R1", Vec2{})));
    CHECK_FALSE(manager.canRedo());
    CHECK(project.network().deviceCount() == 1);
    CHECK(project.network().findDeviceByName("R1") != nullptr);
}

TEST_CASE("History is bounded", "[commands]") {
    Project project;
    CommandManager manager{project};
    manager.setHistoryLimit(5);

    for (int i = 0; i < 20; ++i) {
        manager.breakMergeChain();
        REQUIRE(manager.run(std::make_unique<AddDeviceCommand>(
            DeviceType::Pc, "PC" + std::to_string(i), Vec2{})));
    }

    CHECK(manager.undoDepth() == 5);
    CHECK(project.network().deviceCount() == 20);
}

TEST_CASE("Duplicating devices copies hardware but not addresses or cables", "[commands]") {
    Project project;
    Network& network = project.network();
    CommandManager manager{project};

    Device& pc = addDevice(network, DeviceType::Pc, "PC1");
    Device& sw = addDevice(network, DeviceType::Switch, "SW1");
    connect(network, pc, "Gi0", sw, "Gi0/1");
    assign(pc, "Gi0", "192.168.1.10/24");
    iface(pc, "Gi0").setDescription("desk 1");
    network.refreshOperationalStates();

    auto duplicate = std::make_unique<DuplicateDevicesCommand>(std::vector<DeviceId>{pc.id()},
                                                                Vec2{40, 40});
    auto* raw = duplicate.get();
    REQUIRE(manager.run(std::move(duplicate)));

    REQUIRE(raw->createdDevices().size() == 1);
    const Device* copy = network.findDevice(raw->createdDevices().front());
    REQUIRE(copy != nullptr);

    CHECK(copy->name() == "PC2");
    CHECK(copy->id() != pc.id());
    CHECK(copy->interfaceCount() == pc.interfaceCount());
    CHECK(copy->findInterfaceByName("Gi0")->description() == "desk 1");

    // No duplicate address and no surprise cable.
    CHECK(copy->findInterfaceByName("Gi0")->ipv4Addresses().empty());
    CHECK(network.linksOf(copy->id()).empty());
    CHECK(copy->findInterfaceByName("Gi0")->macAddress() != iface(pc, "Gi0").macAddress());

    REQUIRE(manager.undo());
    CHECK(network.deviceCount() == 2);
}
