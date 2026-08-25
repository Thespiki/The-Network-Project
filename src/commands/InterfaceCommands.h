#pragma once

#include "commands/Command.h"
#include "core/network/Interface.h"

namespace tnp::commands {

/// A snapshot of everything editable on an interface.
///
/// Interface editing is a form, not a stream of individual edits: the properties
/// panel captures the settings when editing starts and issues one command with
/// the result. That keeps a single field change from producing five undo steps
/// and makes the "before" state exact.
struct InterfaceSettings {
    std::string name;
    std::string displayName;
    std::string description;
    core::MacAddress mac;
    core::AdminState adminState = core::AdminState::Up;
    u32 mtu = core::kDefaultMtu;
    u64 speedMbps = 0;
    core::DuplexMode duplex = core::DuplexMode::Auto;
    bool dhcp = false;
    std::vector<core::Ipv4Prefix> ipv4;
    std::vector<core::Ipv6Prefix> ipv6;
    core::VlanConfiguration vlan;

    [[nodiscard]] static InterfaceSettings capture(const core::Interface& iface);

    /// Checks the values that an interface would otherwise refuse silently.
    /// `ConfigureInterfaceCommand` calls this first, so an invalid MTU or
    /// address is reported rather than quietly dropped.
    [[nodiscard]] Status validate() const;

    /// Applies the snapshot. Addresses that the interface rejects are skipped.
    void applyTo(core::Interface& iface) const;

    bool operator==(const InterfaceSettings&) const = default;
};

/// Replaces an interface's configuration wholesale.
class ConfigureInterfaceCommand final : public Command {
public:
    ConfigureInterfaceCommand(core::DeviceId device, core::InterfaceId iface,
                              InterfaceSettings settings);

    [[nodiscard]] std::string_view kind() const override { return "configure-interface"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;
    [[nodiscard]] std::string failureReason() const override { return failure_; }

private:
    core::DeviceId device_;
    core::InterfaceId interface_;
    InterfaceSettings newSettings_;
    InterfaceSettings oldSettings_;
    std::string interfaceName_;
    std::string failure_;
};

/// Adds one IPv4 address. Used by the console, where addresses are configured
/// one at a time.
class AddIpv4AddressCommand final : public Command {
public:
    AddIpv4AddressCommand(core::DeviceId device, core::InterfaceId iface, core::Ipv4Prefix address);

    [[nodiscard]] std::string_view kind() const override { return "add-ipv4-address"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

    [[nodiscard]] std::string failureReason() const override { return failure_; }

private:
    core::DeviceId device_;
    core::InterfaceId interface_;
    core::Ipv4Prefix address_;
    std::string failure_;
};

class RemoveIpv4AddressCommand final : public Command {
public:
    RemoveIpv4AddressCommand(core::DeviceId device, core::InterfaceId iface, core::Ipv4Prefix address);

    [[nodiscard]] std::string_view kind() const override { return "remove-ipv4-address"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::DeviceId device_;
    core::InterfaceId interface_;
    core::Ipv4Prefix address_;
};

/// Enables or disables an interface ("shutdown" / "no shutdown").
class SetInterfaceAdminStateCommand final : public Command {
public:
    SetInterfaceAdminStateCommand(core::DeviceId device, core::InterfaceId iface,
                                  core::AdminState state);

    [[nodiscard]] std::string_view kind() const override { return "set-interface-admin-state"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::DeviceId device_;
    core::InterfaceId interface_;
    core::AdminState newState_;
    core::AdminState oldState_ = core::AdminState::Up;
};

} // namespace tnp::commands
