#pragma once

#include "core/network/DeviceContext.h"
#include "core/network/DeviceType.h"
#include "core/network/Frame.h"
#include "core/network/Ids.h"
#include "core/network/Interface.h"
#include "utilities/Types.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::core {

class Ipv4Stack;
class SwitchingEngine;
class RoutingTable;
class ArpCache;
class MacAddressTable;
class FirewallPolicy;
class DhcpServer;
class DnsServer;

/// Base class for everything that can appear on the topology canvas.
///
/// A device owns its interfaces and its protocol state. It knows nothing about
/// the topology: to send a frame it hands it to a `DeviceContext`, which is the
/// only thing that understands links, queues and time.
///
/// Behaviour is composed rather than inherited: `Router`, `Pc` and `Firewall`
/// all hold an `Ipv4Stack`, `Switch` and `AccessPoint` hold a `SwitchingEngine`,
/// and `Layer3Switch` holds both. That is why the hierarchy is one level deep.
class Device {
public:
    Device(DeviceId id, std::string name);
    virtual ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // --- Identity ----------------------------------------------------------
    [[nodiscard]] DeviceId id() const { return id_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const std::string& description() const { return description_; }
    void setDescription(std::string value) { description_ = std::move(value); }

    [[nodiscard]] virtual DeviceType type() const = 0;
    [[nodiscard]] std::string_view typeName() const { return deviceTypeName(type()); }
    [[nodiscard]] std::string_view typeDisplayName() const { return deviceTypeDisplayName(type()); }

    // --- Interfaces --------------------------------------------------------
    [[nodiscard]] const std::vector<std::unique_ptr<Interface>>& interfaces() const { return interfaces_; }
    [[nodiscard]] std::size_t interfaceCount() const { return interfaces_.size(); }

    Interface& addInterface(std::string name, InterfaceType type);
    /// Overload used by deserialization, which must preserve identifiers.
    Interface& addInterface(InterfaceId id, std::string name, InterfaceType type);

    bool removeInterface(InterfaceId id);

    /// Drops every interface.
    ///
    /// Exists for deserialization, which recreates a device through the registry
    /// (giving it the default interfaces for its type) and must then replace
    /// them with exactly the ones the file describes, identifiers included.
    void clearInterfaces();

    [[nodiscard]] Interface* findInterface(InterfaceId id);
    [[nodiscard]] const Interface* findInterface(InterfaceId id) const;

    /// Matches the full name ("GigabitEthernet0/1"), the short name ("Gi0/1")
    /// and unambiguous abbreviations, the way a network CLI does.
    [[nodiscard]] Interface* findInterfaceByName(std::string_view name);
    [[nodiscard]] const Interface* findInterfaceByName(std::string_view name) const;

    /// First interface that owns `address`, or nullptr.
    [[nodiscard]] Interface* findInterfaceWithIpv4(Ipv4Address address);
    [[nodiscard]] const Interface* findInterfaceWithIpv4(Ipv4Address address) const;

    /// True when `address` is configured on any of this device's interfaces.
    [[nodiscard]] bool ownsIpv4Address(Ipv4Address address) const;

    // --- Simulation hooks --------------------------------------------------
    /// Called once when the simulation starts. Devices arm periodic timers here.
    virtual void onPowerOn(DeviceContext& context);

    /// Clears volatile state (caches, tables, counters) when the simulation is
    /// reset. Configuration must survive.
    virtual void onReset();

    /// A frame arrived on `ingress`. This is the single entry point for all
    /// device behaviour.
    virtual void onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) = 0;

    /// A timer previously armed through `DeviceContext::scheduleTimer` fired.
    virtual void onTimer(DeviceContext& context, TimerId timer);

    // --- Capability queries ------------------------------------------------
    // The CLI, the properties panel and the validators ask a device what it can
    // do instead of downcasting to a concrete type.
    [[nodiscard]] virtual Ipv4Stack* ipv4Stack() { return nullptr; }
    [[nodiscard]] virtual const Ipv4Stack* ipv4Stack() const { return nullptr; }
    [[nodiscard]] virtual SwitchingEngine* switching() { return nullptr; }
    [[nodiscard]] virtual const SwitchingEngine* switching() const { return nullptr; }
    [[nodiscard]] virtual FirewallPolicy* firewallPolicy() { return nullptr; }
    [[nodiscard]] virtual const FirewallPolicy* firewallPolicy() const { return nullptr; }
    [[nodiscard]] virtual DhcpServer* dhcpServer() { return nullptr; }
    [[nodiscard]] virtual const DhcpServer* dhcpServer() const { return nullptr; }
    [[nodiscard]] virtual DnsServer* dnsServer() { return nullptr; }
    [[nodiscard]] virtual const DnsServer* dnsServer() const { return nullptr; }

    /// Convenience accessors that go through `ipv4Stack()`.
    [[nodiscard]] RoutingTable* routingTable();
    [[nodiscard]] const RoutingTable* routingTable() const;
    [[nodiscard]] ArpCache* arpCache();
    [[nodiscard]] const ArpCache* arpCache() const;
    [[nodiscard]] MacAddressTable* macTable();
    [[nodiscard]] const MacAddressTable* macTable() const;

protected:
    /// Adds `count` interfaces named "<type><slotPrefix><n>", numbering from
    /// `startIndex`. Switch ports conventionally start at 1, router ports at 0.
    void createInterfaces(InterfaceType type, std::size_t count,
                          std::string_view slotPrefix = "0/", std::size_t startIndex = 0);

private:
    DeviceId id_;
    std::string name_;
    std::string description_;
    std::vector<std::unique_ptr<Interface>> interfaces_;
};

} // namespace tnp::core
