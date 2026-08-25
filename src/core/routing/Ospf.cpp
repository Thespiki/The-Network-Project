#include "core/routing/Ospf.h"

namespace tnp::core {

std::string_view ospfNeighborStateName(OspfNeighborState state) {
    switch (state) {
        case OspfNeighborState::Down:     return "DOWN";
        case OspfNeighborState::Attempt:  return "ATTEMPT";
        case OspfNeighborState::Init:     return "INIT";
        case OspfNeighborState::TwoWay:   return "2WAY";
        case OspfNeighborState::ExStart:  return "EXSTART";
        case OspfNeighborState::Exchange: return "EXCHANGE";
        case OspfNeighborState::Loading:  return "LOADING";
        case OspfNeighborState::Full:     return "FULL";
    }
    return "DOWN";
}

std::string_view ospfNetworkTypeName(OspfNetworkType type) {
    switch (type) {
        case OspfNetworkType::Broadcast:    return "broadcast";
        case OspfNetworkType::PointToPoint: return "point-to-point";
        case OspfNetworkType::NonBroadcast: return "non-broadcast";
    }
    return "broadcast";
}

} // namespace tnp::core
