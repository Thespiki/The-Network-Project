#include "commands/RoutingCommands.h"

#include "core/devices/Ipv4Stack.h"

#include <algorithm>
#include <format>

namespace tnp::commands {

using namespace core;

namespace {

Ipv4Stack* stackOf(Project& project, DeviceId device) {
    Device* owner = project.network().findDevice(device);
    return owner == nullptr ? nullptr : owner->ipv4Stack();
}

} // namespace

// --- AddStaticRouteCommand -------------------------------------------------

AddStaticRouteCommand::AddStaticRouteCommand(DeviceId device, StaticRouteEntry route)
    : device_(device), route_(std::move(route)) {
    if (!route_.id.isValid()) route_.id = RouteId::generate();
}

std::string AddStaticRouteCommand::label() const {
    return std::format("Add route to {}", route_.destination.toNetworkString());
}

bool AddStaticRouteCommand::execute(Project& project) {
    Ipv4Stack* stack = stackOf(project, device_);
    if (stack == nullptr) {
        failure_ = "this device does not route";
        return false;
    }

    const Status status = stack->addStaticRoute(route_);
    if (!status) {
        failure_ = status.message();
        return false;
    }
    return true;
}

void AddStaticRouteCommand::undo(Project& project) {
    if (Ipv4Stack* stack = stackOf(project, device_)) stack->removeStaticRoute(route_.id);
}

// --- RemoveStaticRouteCommand ----------------------------------------------

RemoveStaticRouteCommand::RemoveStaticRouteCommand(DeviceId device, RouteId route)
    : device_(device), routeId_(route) {}

std::string RemoveStaticRouteCommand::label() const {
    return removed_.destination.prefixLength() == 0 && !removed_.nextHop
               ? "Remove route"
               : std::format("Remove route to {}", removed_.destination.toNetworkString());
}

bool RemoveStaticRouteCommand::execute(Project& project) {
    Ipv4Stack* stack = stackOf(project, device_);
    if (stack == nullptr) return false;

    const auto& routes = stack->staticRoutes();
    const auto it = std::find_if(routes.begin(), routes.end(),
                                 [this](const StaticRouteEntry& entry) { return entry.id == routeId_; });
    if (it == routes.end()) return false;

    removed_ = *it;
    index_ = static_cast<std::size_t>(std::distance(routes.begin(), it));
    return stack->removeStaticRoute(routeId_);
}

void RemoveStaticRouteCommand::undo(Project& project) {
    Ipv4Stack* stack = stackOf(project, device_);
    if (stack == nullptr) return;

    // Reinsert at the original index: route order is visible in the CLI and in
    // the properties panel, so restoring it matters.
    std::vector<StaticRouteEntry> routes = stack->staticRoutes();
    routes.insert(routes.begin() + static_cast<std::ptrdiff_t>(std::min(index_, routes.size())),
                  removed_);
    stack->setStaticRoutes(std::move(routes));
}

// --- SetDefaultGatewayCommand ----------------------------------------------

SetDefaultGatewayCommand::SetDefaultGatewayCommand(DeviceId device,
                                                   std::optional<Ipv4Address> gateway)
    : device_(device), newGateway_(gateway) {}

std::string SetDefaultGatewayCommand::label() const {
    return newGateway_ ? std::format("Set default gateway to {}", newGateway_->toString())
                       : "Clear default gateway";
}

bool SetDefaultGatewayCommand::execute(Project& project) {
    Ipv4Stack* stack = stackOf(project, device_);
    if (stack == nullptr) return false;
    if (stack->defaultGateway() == newGateway_) return false;

    previousRoutes_ = stack->staticRoutes();
    stack->setDefaultGateway(newGateway_);
    return true;
}

void SetDefaultGatewayCommand::undo(Project& project) {
    if (Ipv4Stack* stack = stackOf(project, device_)) stack->setStaticRoutes(previousRoutes_);
}

// --- SetDnsServersCommand --------------------------------------------------

SetDnsServersCommand::SetDnsServersCommand(DeviceId device, std::vector<Ipv4Address> servers)
    : device_(device), newServers_(std::move(servers)) {}

bool SetDnsServersCommand::execute(Project& project) {
    Ipv4Stack* stack = stackOf(project, device_);
    if (stack == nullptr || stack->dnsServers() == newServers_) return false;

    oldServers_ = stack->dnsServers();
    stack->setDnsServers(newServers_);
    return true;
}

void SetDnsServersCommand::undo(Project& project) {
    if (Ipv4Stack* stack = stackOf(project, device_)) stack->setDnsServers(oldServers_);
}

} // namespace tnp::commands
