#pragma once

#include "commands/Command.h"
#include "core/routing/StaticRouting.h"

namespace tnp::commands {

class AddStaticRouteCommand final : public Command {
public:
    AddStaticRouteCommand(core::DeviceId device, core::StaticRouteEntry route);

    [[nodiscard]] std::string_view kind() const override { return "add-static-route"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

    [[nodiscard]] std::string failureReason() const override { return failure_; }

private:
    core::DeviceId device_;
    core::StaticRouteEntry route_;
    std::string failure_;
};

class RemoveStaticRouteCommand final : public Command {
public:
    RemoveStaticRouteCommand(core::DeviceId device, core::RouteId route);

    [[nodiscard]] std::string_view kind() const override { return "remove-static-route"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::DeviceId device_;
    core::RouteId routeId_;
    core::StaticRouteEntry removed_;
    std::size_t index_ = 0;
};

class SetDefaultGatewayCommand final : public Command {
public:
    SetDefaultGatewayCommand(core::DeviceId device, std::optional<core::Ipv4Address> gateway);

    [[nodiscard]] std::string_view kind() const override { return "set-default-gateway"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::DeviceId device_;
    std::optional<core::Ipv4Address> newGateway_;
    /// The whole static route list is kept: setting a gateway rewrites the
    /// default route, and restoring the list is the only exact reversal.
    std::vector<core::StaticRouteEntry> previousRoutes_;
};

class SetDnsServersCommand final : public Command {
public:
    SetDnsServersCommand(core::DeviceId device, std::vector<core::Ipv4Address> servers);

    [[nodiscard]] std::string_view kind() const override { return "set-dns-servers"; }
    [[nodiscard]] std::string label() const override { return "Set DNS servers"; }

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::DeviceId device_;
    std::vector<core::Ipv4Address> newServers_;
    std::vector<core::Ipv4Address> oldServers_;
};

} // namespace tnp::commands
