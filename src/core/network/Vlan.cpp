#include "core/network/Vlan.h"

namespace tnp::core {

std::string_view vlanModeName(VlanMode mode) {
    switch (mode) {
        case VlanMode::Access: return "access";
        case VlanMode::Trunk:  return "trunk";
    }
    return "access";
}

} // namespace tnp::core
