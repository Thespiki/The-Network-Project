#pragma once

#include "core/routing/RoutingTable.h"

#include <string>
#include <vector>

namespace tnp::core {

class Device;

/// A static route as the user configured it.
///
/// Deliberately distinct from `Route`: this is *configuration* and is what gets
/// serialized, while `RoutingTable` holds the *runtime* table, which also
/// contains connected routes derived from interface addresses. Keeping them
/// apart stops stale connected routes from being written into project files.
struct StaticRouteEntry {
    RouteId id;
    Ipv4Prefix destination;

    /// Address of the next router. Empty for an interface-only ("directly
    /// attached") static route.
    std::optional<Ipv4Address> nextHop;

    /// Optional explicit egress interface. When unset it is resolved from the
    /// next hop at the moment the table is rebuilt.
    InterfaceId egressInterface;

    u32 metric = 1;
    bool enabled = true;
    std::string description;
};

/// Result of turning one configured static route into a runtime route.
struct StaticRouteResolution {
    std::optional<Route> route;
    /// Why the route could not be installed, when `route` is empty.
    std::string problem;
};

/// Rebuilds the connected routes of `device` into `table`.
///
/// One /N route per usable IPv4 address on every operational interface. Called
/// whenever addressing or interface state changes, so the table always reflects
/// reality rather than a snapshot taken when the project was saved.
void installConnectedRoutes(const Device& device, RoutingTable& table);

/// Resolves a configured static route against the device's interfaces.
///
/// Fails when the next hop is not on any connected subnet and no explicit egress
/// interface was given - the same condition a real router reports as
/// "%Invalid next hop address".
[[nodiscard]] StaticRouteResolution resolveStaticRoute(const Device& device, const StaticRouteEntry& entry);

/// Clears static and connected routes from `table` and reinstalls them.
void rebuildRoutingTable(const Device& device,
                         const std::vector<StaticRouteEntry>& staticRoutes,
                         RoutingTable& table);

} // namespace tnp::core
