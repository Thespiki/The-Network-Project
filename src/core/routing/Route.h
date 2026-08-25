#pragma once

#include "core/network/Ids.h"
#include "core/network/Subnet.h"
#include "utilities/Types.h"

#include <optional>
#include <string>
#include <string_view>

namespace tnp::core {

/// Where a route came from. Determines its administrative distance, and which
/// routes are rebuilt automatically when the topology changes.
enum class RouteSource : u8 {
    Connected, ///< derived from an interface address; never edited by hand
    Static,    ///< configured by the user
    Dhcp,      ///< default route learned from a DHCP lease
    Ospf,      ///< reserved for the dynamic routing protocol
    Rip
};

[[nodiscard]] std::string_view routeSourceName(RouteSource source);

/// Administrative distance, the tie-breaker between routes of equal prefix
/// length from different sources. Lower wins, following common practice.
[[nodiscard]] u8 routeSourceDistance(RouteSource source);

/// One entry in a routing table.
///
/// `nextHop` is empty for directly connected networks, where the destination is
/// reachable on the link itself and ARP resolves the final address directly.
struct Route {
    RouteId id;
    Ipv4Prefix destination;
    std::optional<Ipv4Address> nextHop;
    InterfaceId egressInterface;
    u32 metric = 0;
    RouteSource source = RouteSource::Static;
    bool enabled = true;

    [[nodiscard]] bool isDirectlyConnected() const { return !nextHop.has_value(); }
    [[nodiscard]] bool isDefaultRoute() const { return destination.prefixLength() == 0; }
    [[nodiscard]] u8 administrativeDistance() const { return routeSourceDistance(source); }

    /// "10.0.0.0/8 via 192.168.1.1" or "192.168.1.0/24 is directly connected".
    [[nodiscard]] std::string toString() const;
};

} // namespace tnp::core
