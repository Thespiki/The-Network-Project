#include "core/devices/Server.h"

namespace tnp::core {

Server::Server(DeviceId id, std::string name)
    : Ipv4Device(id, std::move(name)), dhcp_(*this, stack_), dns_(*this, stack_) {
    addInterface("GigabitEthernet0", InterfaceType::GigabitEthernet);

    stack_.setForwardingEnabled(false);
    bindDhcpClient();

    stack_.bindUdpPort(proto::kPortDhcpServer,
                       [this](DeviceContext& context, Interface& ingress, const proto::Ipv4Header& ip,
                              const proto::UdpHeader& udp, std::span<const u8> payload) {
                           dhcp_.handleDatagram(context, ingress, ip, udp, payload);
                       });

    stack_.bindUdpPort(proto::kPortDns,
                       [this](DeviceContext& context, Interface& ingress, const proto::Ipv4Header& ip,
                              const proto::UdpHeader& udp, std::span<const u8> payload) {
                           dns_.handleDatagram(context, ingress, ip, udp, payload);
                       });
}

void Server::onReset() {
    Ipv4Device::onReset();
    dhcp_.onReset();
}

} // namespace tnp::core
