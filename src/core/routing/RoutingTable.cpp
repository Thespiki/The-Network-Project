#include "core/routing/RoutingTable.h"

#include <algorithm>
#include <format>

namespace tnp::core {

Status RoutingTable::add(Route route) {
    const auto duplicate = std::find_if(routes_.begin(), routes_.end(), [&](const Route& existing) {
        return existing.destination.network() == route.destination.network() &&
               existing.nextHop == route.nextHop &&
               existing.egressInterface == route.egressInterface &&
               existing.source == route.source;
    });
    if (duplicate != routes_.end()) {
        return Status::failure(std::format("a {} route for {} already exists",
                                           routeSourceName(route.source),
                                           route.destination.toNetworkString()));
    }
    addUnchecked(std::move(route));
    return Status::ok();
}

void RoutingTable::addUnchecked(Route route) {
    if (!route.id.isValid()) route.id = RouteId::generate();
    route.destination = route.destination.network();
    routes_.push_back(std::move(route));
    sort();
}

bool RoutingTable::remove(RouteId id) {
    const auto it = std::find_if(routes_.begin(), routes_.end(),
                                 [id](const Route& route) { return route.id == id; });
    if (it == routes_.end()) return false;
    routes_.erase(it);
    return true;
}

std::size_t RoutingTable::removeByDestination(const Ipv4Prefix& destination) {
    const auto network = destination.network();
    const auto removed = std::remove_if(routes_.begin(), routes_.end(), [&](const Route& route) {
        return route.destination == network;
    });
    const auto count = static_cast<std::size_t>(std::distance(removed, routes_.end()));
    routes_.erase(removed, routes_.end());
    return count;
}

std::size_t RoutingTable::removeBySource(RouteSource source) {
    const auto removed = std::remove_if(routes_.begin(), routes_.end(),
                                        [source](const Route& route) { return route.source == source; });
    const auto count = static_cast<std::size_t>(std::distance(removed, routes_.end()));
    routes_.erase(removed, routes_.end());
    return count;
}

std::size_t RoutingTable::removeByInterface(InterfaceId interface) {
    const auto removed = std::remove_if(routes_.begin(), routes_.end(), [interface](const Route& route) {
        return route.egressInterface == interface;
    });
    const auto count = static_cast<std::size_t>(std::distance(removed, routes_.end()));
    routes_.erase(removed, routes_.end());
    return count;
}

void RoutingTable::clear() { routes_.clear(); }

const Route* RoutingTable::find(RouteId id) const {
    const auto it = std::find_if(routes_.begin(), routes_.end(),
                                 [id](const Route& route) { return route.id == id; });
    return it == routes_.end() ? nullptr : &*it;
}

const Route* RoutingTable::lookup(Ipv4Address destination) const {
    for (const Route& route : routes_) {
        if (!route.enabled) continue;
        if (route.destination.contains(destination)) return &route;
    }
    return nullptr;
}

std::vector<const Route*> RoutingTable::matches(Ipv4Address destination) const {
    std::vector<const Route*> result;
    for (const Route& route : routes_) {
        if (!route.enabled) continue;
        if (route.destination.contains(destination)) result.push_back(&route);
    }
    return result;
}

const Route* RoutingTable::defaultRoute() const {
    for (const Route& route : routes_) {
        if (route.enabled && route.isDefaultRoute()) return &route;
    }
    return nullptr;
}

void RoutingTable::sort() {
    std::stable_sort(routes_.begin(), routes_.end(), [](const Route& a, const Route& b) {
        if (a.destination.prefixLength() != b.destination.prefixLength()) {
            return a.destination.prefixLength() > b.destination.prefixLength();
        }
        if (a.administrativeDistance() != b.administrativeDistance()) {
            return a.administrativeDistance() < b.administrativeDistance();
        }
        return a.metric < b.metric;
    });
}

} // namespace tnp::core
