#include "core/network/Ids.h"

namespace tnp::core {

std::string_view objectKindName(ObjectKind kind) {
    switch (kind) {
        case ObjectKind::None:       return "None";
        case ObjectKind::Device:     return "Device";
        case ObjectKind::Interface:  return "Interface";
        case ObjectKind::Link:       return "Link";
        case ObjectKind::Annotation: return "Annotation";
        case ObjectKind::Test:       return "Test";
        case ObjectKind::Route:      return "Route";
        case ObjectKind::Packet:     return "Packet";
    }
    return "Unknown";
}

} // namespace tnp::core
