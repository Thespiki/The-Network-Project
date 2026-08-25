#pragma once

#include "core/network/Ids.h"
#include "core/network/Subnet.h"
#include "utilities/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace tnp::core {

/// OSPFv2 configuration.
///
/// STATUS: the configuration model below is real - it is edited in the
/// properties panel, serialized with the project and reported by the CLI - but
/// **no adjacency formation, LSA flooding or SPF calculation is implemented**.
/// A device with OSPF configured therefore forwards using its connected and
/// static routes only, and the validator raises an informational issue saying
/// so. The types here exist because the routing table already supports a
/// `RouteSource::Ospf` with its own administrative distance, so adding the
/// protocol later is an additive change rather than a redesign.
///
/// See docs/ROADMAP.md for the planned implementation order.

/// Neighbour states of the OSPF finite state machine (RFC 2328 section 10.1).
enum class OspfNeighborState : u8 {
    Down, Attempt, Init, TwoWay, ExStart, Exchange, Loading, Full
};

[[nodiscard]] std::string_view ospfNeighborStateName(OspfNeighborState state);

/// Interface network types. Determines whether a DR/BDR election happens.
enum class OspfNetworkType : u8 { Broadcast, PointToPoint, NonBroadcast };

[[nodiscard]] std::string_view ospfNetworkTypeName(OspfNetworkType type);

/// One "network <prefix> area <id>" statement.
struct OspfNetworkStatement {
    Ipv4Prefix network;
    u32 areaId = 0;
};

/// Per-interface OSPF settings.
struct OspfInterfaceSettings {
    InterfaceId interface;
    u32 cost = 10;
    u16 helloIntervalSeconds = 10;
    u16 deadIntervalSeconds = 40;
    u8 priority = 1;
    bool passive = false;
    OspfNetworkType networkType = OspfNetworkType::Broadcast;
};

/// An OSPF process on one device.
struct OspfConfiguration {
    bool enabled = false;
    u32 processId = 1;

    /// Router ID. When unset, a real implementation picks the highest loopback
    /// or interface address; TNP requires it to be explicit to stay
    /// deterministic.
    Ipv4Address routerId;

    std::vector<OspfNetworkStatement> networks;
    std::vector<OspfInterfaceSettings> interfaces;

    bool redistributeConnected = false;
    bool redistributeStatic = false;

    [[nodiscard]] bool isConfigured() const { return enabled && !networks.empty(); }
};

} // namespace tnp::core
