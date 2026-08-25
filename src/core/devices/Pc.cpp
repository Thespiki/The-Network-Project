#include "core/devices/Pc.h"

namespace tnp::core {

Pc::Pc(DeviceId id, std::string name) : Ipv4Device(id, std::move(name)) {
    addInterface(std::string{kWiredInterfaceName}, InterfaceType::GigabitEthernet);

    Interface& wireless = addInterface(std::string{kWirelessInterfaceName}, InterfaceType::Wireless);
    wireless.setAdminState(AdminState::Down);

    // A host does not forward: traffic that is not addressed to it is dropped.
    stack_.setForwardingEnabled(false);
    bindDhcpClient();
}

} // namespace tnp::core
