#include "core/devices/Ipv4Device.h"

namespace tnp::core {

Ipv4Device::Ipv4Device(DeviceId id, std::string name)
    : Device(id, std::move(name)), stack_(*this), dhcpClient_(*this, stack_) {}

void Ipv4Device::bindDhcpClient() {
    stack_.bindUdpPort(proto::kPortDhcpClient,
                       [this](DeviceContext& context, Interface& ingress, const proto::Ipv4Header& ip,
                              const proto::UdpHeader& udp, std::span<const u8> payload) {
                           dhcpClient_.handleDatagram(context, ingress, ip, udp, payload);
                       });
}

void Ipv4Device::onPowerOn(DeviceContext& context) {
    Device::onPowerOn(context);
    stack_.onPowerOn(context);
    dhcpClient_.onPowerOn(context);
}

void Ipv4Device::onReset() {
    Device::onReset();
    dhcpClient_.onReset();
    stack_.onReset();
}

void Ipv4Device::onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) {
    stack_.onFrameReceived(context, ingress, frame);
}

void Ipv4Device::onTimer(DeviceContext& context, TimerId timer) {
    if (dhcpClient_.onTimer(context, timer)) return;
    stack_.onTimer(context, timer);
}

} // namespace tnp::core
