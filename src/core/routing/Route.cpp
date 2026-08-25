#include "core/routing/Route.h"

#include <format>

namespace tnp::core {

std::string_view routeSourceName(RouteSource source) {
    switch (source) {
        case RouteSource::Connected: return "connected";
        case RouteSource::Static:    return "static";
        case RouteSource::Dhcp:      return "dhcp";
        case RouteSource::Ospf:      return "ospf";
        case RouteSource::Rip:       return "rip";
    }
    return "static";
}

u8 routeSourceDistance(RouteSource source) {
    switch (source) {
        case RouteSource::Connected: return 0;
        case RouteSource::Static:    return 1;
        case RouteSource::Dhcp:      return 5;
        case RouteSource::Ospf:      return 110;
        case RouteSource::Rip:       return 120;
    }
    return 1;
}

std::string Route::toString() const {
    if (isDirectlyConnected()) {
        return std::format("{} is directly connected", destination.toNetworkString());
    }
    return std::format("{} via {}", destination.toNetworkString(), nextHop->toString());
}

} // namespace tnp::core
