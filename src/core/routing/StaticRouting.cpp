#include "core/routing/StaticRouting.h"

#include "core/network/Device.h"

#include <format>

namespace tnp::core {

void installConnectedRoutes(const Device& device, RoutingTable& table) {
    table.removeBySource(RouteSource::Connected);

    for (const auto& iface : device.interfaces()) {
        // A down interface does not contribute a route, which is exactly why
        // shutting a port down makes its subnet unreachable.
        if (!iface->isAdminUp() || !iface->isOperational()) continue;

        for (const Ipv4Prefix& prefix : iface->ipv4Addresses()) {
            Route route;
            route.id = RouteId::generate();
            route.destination = prefix.network();
            route.nextHop = std::nullopt;
            route.egressInterface = iface->id();
            route.metric = 0;
            route.source = RouteSource::Connected;
            table.addUnchecked(std::move(route));
        }
    }
}

StaticRouteResolution resolveStaticRoute(const Device& device, const StaticRouteEntry& entry) {
    StaticRouteResolution resolution;

    Route route;
    route.id = entry.id.isValid() ? entry.id : RouteId::generate();
    route.destination = entry.destination.network();
    route.nextHop = entry.nextHop;
    route.metric = entry.metric;
    route.source = RouteSource::Static;
    route.enabled = entry.enabled;

    if (entry.egressInterface.isValid()) {
        const Interface* iface = device.findInterface(entry.egressInterface);
        if (iface == nullptr) {
            resolution.problem = "the configured egress interface no longer exists";
            return resolution;
        }
        route.egressInterface = entry.egressInterface;
        resolution.route = std::move(route);
        return resolution;
    }

    if (!entry.nextHop) {
        resolution.problem = "a static route needs either a next hop or an egress interface";
        return resolution;
    }

    // Find the interface whose connected subnet contains the next hop.
    for (const auto& iface : device.interfaces()) {
        for (const Ipv4Prefix& prefix : iface->ipv4Addresses()) {
            if (!prefix.contains(*entry.nextHop)) continue;
            route.egressInterface = iface->id();
            resolution.route = std::move(route);
            return resolution;
        }
    }

    resolution.problem = std::format("next hop {} is not on a connected subnet", entry.nextHop->toString());
    return resolution;
}

void rebuildRoutingTable(const Device& device,
                         const std::vector<StaticRouteEntry>& staticRoutes,
                         RoutingTable& table) {
    table.removeBySource(RouteSource::Connected);
    table.removeBySource(RouteSource::Static);

    installConnectedRoutes(device, table);

    for (const StaticRouteEntry& entry : staticRoutes) {
        if (!entry.enabled) continue;
        auto resolution = resolveStaticRoute(device, entry);
        if (resolution.route) table.addUnchecked(std::move(*resolution.route));
    }
}

} // namespace tnp::core
