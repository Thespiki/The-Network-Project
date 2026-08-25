#include "validation/BuiltinRules.h"

#include "core/devices/DhcpServer.h"
#include "core/devices/Ipv4Stack.h"
#include "core/devices/SwitchingEngine.h"

#include <format>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace tnp::validation {
namespace {

using namespace core;

/// Base for the common "one rule, one lambda" shape.
class SimpleRule : public ValidationRule {
public:
    SimpleRule(std::string_view id, std::string_view description)
        : id_(id), description_(description) {}

    [[nodiscard]] std::string_view id() const override { return id_; }
    [[nodiscard]] std::string_view description() const override { return description_; }

private:
    std::string_view id_;
    std::string_view description_;
};

// --- Addressing ------------------------------------------------------------

class DuplicateIpv4AddressRule final : public SimpleRule {
public:
    DuplicateIpv4AddressRule()
        : SimpleRule("duplicate-ipv4-address",
                     "The same IPv4 address configured on more than one interface") {}

    void check(ValidationContext& context) const override {
        struct Holder {
            const Device* device;
            const Interface* iface;
        };
        std::map<Ipv4Address, std::vector<Holder>> owners;

        for (const auto& device : context.network().devices()) {
            for (const auto& iface : device->interfaces()) {
                for (const Ipv4Prefix& prefix : iface->ipv4Addresses()) {
                    owners[prefix.address()].push_back(Holder{device.get(), iface.get()});
                }
            }
        }

        for (const auto& [address, holders] : owners) {
            if (holders.size() < 2) continue;

            std::string others;
            for (const Holder& holder : holders) {
                if (!others.empty()) others += ", ";
                others += std::format("{} {}", holder.device->name(), holder.iface->shortName());
            }
            for (const Holder& holder : holders) {
                context.reportInterface(
                    Severity::Error, "duplicate-ipv4-address",
                    std::format("{} is configured on more than one interface ({})",
                                address.toString(), others),
                    *holder.device, *holder.iface,
                    "Give each interface a unique address within its subnet.");
            }
        }
    }
};

class DuplicateMacAddressRule final : public SimpleRule {
public:
    DuplicateMacAddressRule()
        : SimpleRule("duplicate-mac-address", "The same MAC address on more than one interface") {}

    void check(ValidationContext& context) const override {
        std::map<MacAddress, std::vector<std::pair<const Device*, const Interface*>>> owners;

        for (const auto& device : context.network().devices()) {
            for (const auto& iface : device->interfaces()) {
                if (iface->macAddress().isZero()) continue;
                owners[iface->macAddress()].emplace_back(device.get(), iface.get());
            }
        }

        for (const auto& [mac, holders] : owners) {
            if (holders.size() < 2) continue;
            for (const auto& [device, iface] : holders) {
                context.reportInterface(
                    Severity::Error, "duplicate-mac-address",
                    std::format("MAC address {} appears on {} interfaces; switching will misbehave",
                                mac.toString(), holders.size()),
                    *device, *iface, "Assign a unique MAC address to each interface.");
            }
        }
    }
};

class LinkSubnetMismatchRule final : public SimpleRule {
public:
    LinkSubnetMismatchRule()
        : SimpleRule("link-subnet-mismatch",
                     "Directly connected interfaces addressed in different subnets") {}

    void check(ValidationContext& context) const override {
        for (const auto& link : context.network().links()) {
            const Interface* a = context.network().findInterface(link->endpointA().interface);
            const Interface* b = context.network().findInterface(link->endpointB().interface);
            if (a == nullptr || b == nullptr) continue;

            const auto addressA = a->primaryIpv4();
            const auto addressB = b->primaryIpv4();
            if (!addressA || !addressB) continue;

            if (addressA->network() == addressB->network()) continue;

            context.reportLink(
                Severity::Error, "link-subnet-mismatch",
                std::format("{} and {} are cabled together but sit in different subnets",
                            addressA->toString(), addressB->toString()),
                *link,
                "Put both ends of a link in the same subnet, or remove the addresses and let a "
                "router or switch separate them.");
        }
    }
};

class RouterInterfaceWithoutAddressRule final : public SimpleRule {
public:
    RouterInterfaceWithoutAddressRule()
        : SimpleRule("router-interface-without-address",
                     "A connected routing interface with no IPv4 address") {}

    void check(ValidationContext& context) const override {
        for (const auto& device : context.network().devices()) {
            const Ipv4Stack* stack = device->ipv4Stack();
            if (stack == nullptr || !stack->forwardingEnabled()) continue;

            for (const auto& iface : device->interfaces()) {
                if (!iface->isConnected() || !iface->isAdminUp()) continue;
                if (!iface->ipv4Addresses().empty() || iface->ipv4DhcpEnabled()) continue;

                context.reportInterface(
                    Severity::Warning, "router-interface-without-address",
                    std::format("{} is cabled but has no IPv4 address, so nothing can be routed through it",
                                iface->name()),
                    *device, *iface, "Give the interface an address, or enable DHCP on it.");
            }
        }
    }
};

class HostWithoutGatewayRule final : public SimpleRule {
public:
    HostWithoutGatewayRule()
        : SimpleRule("host-without-default-gateway",
                     "An addressed end host with no default route") {}

    void check(ValidationContext& context) const override {
        for (const auto& device : context.network().devices()) {
            const Ipv4Stack* stack = device->ipv4Stack();
            if (stack == nullptr || stack->forwardingEnabled()) continue;
            if (stack->defaultGateway().has_value()) continue;

            bool addressed = false;
            bool usesDhcp = false;
            for (const auto& iface : device->interfaces()) {
                if (!iface->ipv4Addresses().empty()) addressed = true;
                if (iface->ipv4DhcpEnabled()) usesDhcp = true;
            }
            if (!addressed || usesDhcp) continue;

            context.reportDevice(
                Severity::Warning, "host-without-default-gateway",
                std::format("{} has an address but no default gateway, so it can only reach its own subnet",
                            device->name()),
                *device, "Set a default gateway in the device's IPv4 settings.");
        }
    }
};

class UnresolvableStaticRouteRule final : public SimpleRule {
public:
    UnresolvableStaticRouteRule()
        : SimpleRule("unresolvable-static-route",
                     "A static route whose next hop is not on a connected subnet") {}

    void check(ValidationContext& context) const override {
        for (const auto& device : context.network().devices()) {
            const Ipv4Stack* stack = device->ipv4Stack();
            if (stack == nullptr) continue;

            for (const StaticRouteEntry& entry : stack->staticRoutes()) {
                if (!entry.enabled) continue;

                const StaticRouteResolution resolution = resolveStaticRoute(*device, entry);
                if (resolution.route) continue;

                context.reportDevice(
                    Severity::Error, "unresolvable-static-route",
                    std::format("the route to {} cannot be installed: {}",
                                entry.destination.toNetworkString(), resolution.problem),
                    *device,
                    "Give the next hop an address on a connected subnet, or name an egress interface.");
            }
        }
    }
};

// --- Topology --------------------------------------------------------------

class IsolatedDeviceRule final : public SimpleRule {
public:
    IsolatedDeviceRule()
        : SimpleRule("isolated-device", "A device with nothing connected to it") {}

    void check(ValidationContext& context) const override {
        for (const auto& device : context.network().devices()) {
            if (!context.network().linksOf(device->id()).empty()) continue;

            context.reportDevice(
                Severity::Warning, "isolated-device",
                std::format("{} is not connected to anything", device->name()), *device,
                "Drag a cable from one of its interfaces to another device.");
        }
    }
};

class UnusedInterfaceRule final : public SimpleRule {
public:
    UnusedInterfaceRule()
        : SimpleRule("unused-interface", "A configured interface with no cable attached") {}

    void check(ValidationContext& context) const override {
        for (const auto& device : context.network().devices()) {
            for (const auto& iface : device->interfaces()) {
                if (!iface->isConnectable() || iface->isConnected()) continue;
                // Only worth mentioning when the interface has been configured:
                // an untouched spare port is not a finding.
                if (iface->ipv4Addresses().empty() && !iface->ipv4DhcpEnabled()) continue;

                context.reportInterface(
                    Severity::Info, "unused-interface",
                    std::format("{} is configured but nothing is connected to it", iface->name()),
                    *device, *iface);
            }
        }
    }
};

class InterfaceShutdownRule final : public SimpleRule {
public:
    InterfaceShutdownRule()
        : SimpleRule("interface-shutdown", "A cabled interface that is administratively down") {}

    void check(ValidationContext& context) const override {
        for (const auto& device : context.network().devices()) {
            for (const auto& iface : device->interfaces()) {
                if (!iface->isConnected() || iface->isAdminUp()) continue;

                context.reportInterface(
                    Severity::Warning, "interface-shutdown",
                    std::format("{} has a cable but is administratively down", iface->name()),
                    *device, *iface, "Enable the interface in its properties.");
            }
        }
    }
};

class MtuMismatchRule final : public SimpleRule {
public:
    MtuMismatchRule()
        : SimpleRule("mtu-mismatch", "Interfaces on the same link with different MTUs") {}

    void check(ValidationContext& context) const override {
        for (const auto& link : context.network().links()) {
            const Interface* a = context.network().findInterface(link->endpointA().interface);
            const Interface* b = context.network().findInterface(link->endpointB().interface);
            if (a == nullptr || b == nullptr || a->mtu() == b->mtu()) continue;

            context.reportLink(
                Severity::Warning, "mtu-mismatch",
                std::format("MTU {} on one end and {} on the other; large packets will be dropped",
                            a->mtu(), b->mtu()),
                *link, "Set the same MTU on both interfaces.");
        }
    }
};

class SwitchingLoopRule final : public SimpleRule {
public:
    SwitchingLoopRule()
        : SimpleRule("switching-loop",
                     "A physical loop between bridging devices, which TNP does not break") {}

    void check(ValidationContext& context) const override {
        // Union-find over the bridging devices only: a cycle among devices that
        // flood is a broadcast storm, and TNP simulates no spanning tree.
        std::unordered_map<Uuid, Uuid> parent;

        const auto bridges = [](const Device& device) {
            return device.switching() != nullptr || device.type() == DeviceType::Hub;
        };

        for (const auto& device : context.network().devices()) {
            if (bridges(*device)) parent[device->id().uuid()] = device->id().uuid();
        }

        // Iterative find with path compression.
        const std::function<Uuid(Uuid)> find = [&](Uuid node) {
            while (parent[node] != node) {
                parent[node] = parent[parent[node]];
                node = parent[node];
            }
            return node;
        };

        for (const auto& link : context.network().links()) {
            const Device* a = context.network().findDevice(link->endpointA().device);
            const Device* b = context.network().findDevice(link->endpointB().device);
            if (a == nullptr || b == nullptr) continue;
            if (!bridges(*a) || !bridges(*b)) continue;

            const Uuid rootA = find(a->id().uuid());
            const Uuid rootB = find(b->id().uuid());

            if (rootA == rootB) {
                context.reportLink(
                    Severity::Warning, "switching-loop",
                    std::format("this link closes a loop between {} and {}; broadcast frames will "
                                "circulate forever because no spanning tree protocol is simulated",
                                a->name(), b->name()),
                    *link,
                    "Remove one link of the loop, or disable it to keep it as a documented standby path.");
                continue;
            }
            parent[rootA] = rootB;
        }
    }
};

// --- VLANs -----------------------------------------------------------------

class UndefinedVlanRule final : public SimpleRule {
public:
    UndefinedVlanRule()
        : SimpleRule("undefined-vlan", "A port assigned to a VLAN the switch does not know") {}

    void check(ValidationContext& context) const override {
        for (const auto& device : context.network().devices()) {
            const SwitchingEngine* switching = device->switching();
            if (switching == nullptr) continue;

            for (const auto& iface : device->interfaces()) {
                const VlanConfiguration& vlan = iface->vlan();

                std::set<VlanId> used;
                if (vlan.mode == VlanMode::Access) {
                    used.insert(vlan.accessVlan);
                } else {
                    used.insert(vlan.nativeVlan);
                    used.insert(vlan.allowedVlans.begin(), vlan.allowedVlans.end());
                }

                for (const VlanId id : used) {
                    if (switching->findVlan(id) != nullptr) continue;
                    context.reportInterface(
                        Severity::Warning, "undefined-vlan",
                        std::format("{} references VLAN {}, which is not defined on {}",
                                    iface->name(), id, device->name()),
                        *device, *iface, std::format("Add VLAN {} to the switch's VLAN database.", id));
                }
            }
        }
    }
};

// --- Services --------------------------------------------------------------

class DhcpPoolRule final : public SimpleRule {
public:
    DhcpPoolRule() : SimpleRule("dhcp-pool-invalid", "A DHCP pool that cannot allocate addresses") {}

    void check(ValidationContext& context) const override {
        for (const auto& device : context.network().devices()) {
            const DhcpServer* server = device->dhcpServer();
            if (server == nullptr || !server->isEnabled()) continue;

            if (server->pools().empty()) {
                context.reportDevice(
                    Severity::Warning, "dhcp-pool-invalid",
                    std::format("DHCP is enabled on {} but no address pool is configured", device->name()),
                    *device, "Add a pool covering the subnet the clients are on.");
                continue;
            }

            for (const DhcpPool& pool : server->pools()) {
                if (!pool.isValid()) {
                    context.reportDevice(
                        Severity::Error, "dhcp-pool-invalid",
                        std::format("pool '{}' is not usable: the range {} - {} does not fit inside {}",
                                    pool.name, pool.rangeFirst.toString(), pool.rangeLast.toString(),
                                    pool.subnet.toNetworkString()),
                        *device, "Correct the subnet or the address range.");
                    continue;
                }

                const bool serves = device->findInterfaceWithIpv4(pool.subnet.address()) != nullptr ||
                                    [&] {
                                        for (const auto& iface : device->interfaces()) {
                                            for (const Ipv4Prefix& prefix : iface->ipv4Addresses()) {
                                                if (pool.subnet.contains(prefix.address())) return true;
                                            }
                                        }
                                        return false;
                                    }();

                if (!serves) {
                    context.reportDevice(
                        Severity::Warning, "dhcp-pool-invalid",
                        std::format("pool '{}' covers {} but {} has no interface on that subnet",
                                    pool.name, pool.subnet.toNetworkString(), device->name()),
                        *device, "Give the device an address inside the pool's subnet.");
                }
            }
        }
    }
};

// --- Honest disclosure -----------------------------------------------------

class UnsimulatedFeatureRule final : public SimpleRule {
public:
    UnsimulatedFeatureRule()
        : SimpleRule("feature-not-simulated",
                     "Configuration that this build stores but does not simulate") {}

    void check(ValidationContext& context) const override {
        for (const auto& device : context.network().devices()) {
            const Ipv4Stack* stack = device->ipv4Stack();
            if (stack == nullptr || !stack->ospf().isConfigured()) continue;

            context.reportDevice(
                Severity::Info, "feature-not-simulated",
                std::format("{} has OSPF configured; this build saves the configuration but does not "
                            "form adjacencies or compute routes from it",
                            device->name()),
                *device, "Use static routes for now. The configuration will be honoured once OSPF lands.");
        }
    }
};

// --- Project level ---------------------------------------------------------

class DuplicateDeviceNameRule final : public SimpleRule {
public:
    DuplicateDeviceNameRule()
        : SimpleRule("duplicate-device-name", "Two devices sharing a name") {}

    void check(ValidationContext& context) const override {
        std::unordered_map<std::string, int> counts;
        for (const auto& device : context.network().devices()) ++counts[device->name()];

        for (const auto& device : context.network().devices()) {
            if (counts[device->name()] < 2) continue;
            context.reportDevice(
                Severity::Error, "duplicate-device-name",
                std::format("more than one device is called '{}'; commands and tests that name a "
                            "device become ambiguous",
                            device->name()),
                *device, "Rename one of them.");
        }
    }
};

class TestReferenceRule final : public SimpleRule {
public:
    TestReferenceRule()
        : SimpleRule("test-reference-broken", "A stored test that points at a missing device") {}

    void check(ValidationContext& context) const override {
        for (const NetworkTest& test : context.project().tests()) {
            const Device* source = context.network().findDevice(test.source);
            if (source == nullptr) {
                context.report(Severity::Error, "test-reference-broken",
                               std::format("test '{}' has no source device", test.name),
                               core::ObjectRef::test(test.id), test.name,
                               "Choose a source device, or delete the test.");
                continue;
            }
            if (!test.hasDestination()) {
                context.report(Severity::Error, "test-reference-broken",
                               std::format("test '{}' has no destination", test.name),
                               core::ObjectRef::test(test.id), test.name,
                               "Choose a destination device or type an address.");
                continue;
            }
            if (test.destinationDevice.isValid() &&
                context.network().findDevice(test.destinationDevice) == nullptr) {
                context.report(Severity::Error, "test-reference-broken",
                               std::format("test '{}' points at a device that no longer exists", test.name),
                               core::ObjectRef::test(test.id), test.name,
                               "Pick a new destination, or delete the test.");
            }
            if (source->ipv4Stack() == nullptr) {
                context.report(Severity::Error, "test-reference-broken",
                               std::format("test '{}' starts from {}, which has no IPv4 stack and "
                                           "cannot send a ping",
                                           test.name, source->name()),
                               core::ObjectRef::test(test.id), test.name,
                               "Start the test from a PC, server, router or firewall.");
            }
        }
    }
};

class EmptyProjectRule final : public SimpleRule {
public:
    EmptyProjectRule() : SimpleRule("empty-project", "A project with no devices") {}

    void check(ValidationContext& context) const override {
        if (context.network().deviceCount() != 0) return;
        context.reportProject(Severity::Info, "empty-project", "the project has no devices yet",
                              "Drag a device from the palette onto the canvas.");
    }
};

} // namespace

std::vector<std::unique_ptr<ValidationRule>> makeBuiltinRules() {
    std::vector<std::unique_ptr<ValidationRule>> rules;

    rules.push_back(std::make_unique<EmptyProjectRule>());
    rules.push_back(std::make_unique<DuplicateDeviceNameRule>());
    rules.push_back(std::make_unique<DuplicateIpv4AddressRule>());
    rules.push_back(std::make_unique<DuplicateMacAddressRule>());
    rules.push_back(std::make_unique<LinkSubnetMismatchRule>());
    rules.push_back(std::make_unique<RouterInterfaceWithoutAddressRule>());
    rules.push_back(std::make_unique<HostWithoutGatewayRule>());
    rules.push_back(std::make_unique<UnresolvableStaticRouteRule>());
    rules.push_back(std::make_unique<IsolatedDeviceRule>());
    rules.push_back(std::make_unique<UnusedInterfaceRule>());
    rules.push_back(std::make_unique<InterfaceShutdownRule>());
    rules.push_back(std::make_unique<MtuMismatchRule>());
    rules.push_back(std::make_unique<SwitchingLoopRule>());
    rules.push_back(std::make_unique<UndefinedVlanRule>());
    rules.push_back(std::make_unique<DhcpPoolRule>());
    rules.push_back(std::make_unique<TestReferenceRule>());
    rules.push_back(std::make_unique<UnsimulatedFeatureRule>());

    return rules;
}

} // namespace tnp::validation
