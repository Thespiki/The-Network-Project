#include "core/devices/Router.h"

namespace tnp::core {

Router::Router(DeviceId id, std::string name, std::size_t ethernetPorts, std::size_t serialPorts)
    : Ipv4Device(id, std::move(name)), dhcp_(*this, stack_) {
    createInterfaces(InterfaceType::GigabitEthernet, ethernetPorts, "0/", 0);
    createInterfaces(InterfaceType::Serial, serialPorts, "0/0/", 0);

    // The one setting that makes a router a router.
    stack_.setForwardingEnabled(true);

    stack_.bindUdpPort(proto::kPortDhcpServer,
                       [this](DeviceContext& context, Interface& ingress, const proto::Ipv4Header& ip,
                              const proto::UdpHeader& udp, std::span<const u8> payload) {
                           dhcp_.handleDatagram(context, ingress, ip, udp, payload);
                       });

    bindDhcpClient();
}

void Router::onReset() {
    Ipv4Device::onReset();
    dhcp_.onReset();
}

} // namespace tnp::core
