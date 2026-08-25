#include "core/devices/Cloud.h"

namespace tnp::core {

Cloud::Cloud(DeviceId id, std::string name, std::size_t portCount) : Ipv4Device(id, std::move(name)) {
    createInterfaces(InterfaceType::GigabitEthernet, portCount, "0/", 0);
    stack_.setForwardingEnabled(true);
}

} // namespace tnp::core
