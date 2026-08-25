#include "core/devices/AccessPoint.h"

namespace tnp::core {

AccessPoint::AccessPoint(DeviceId id, std::string name) : Device(id, std::move(name)), switching_(*this) {
    addInterface("GigabitEthernet0", InterfaceType::GigabitEthernet);
    addInterface("Wireless0", InterfaceType::Wireless);
}

void AccessPoint::onPowerOn(DeviceContext& context) {
    Device::onPowerOn(context);
    switching_.onPowerOn(context);
}

void AccessPoint::onReset() {
    Device::onReset();
    switching_.onReset();
}

void AccessPoint::onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) {
    switching_.onFrameReceived(context, ingress, frame);
}

void AccessPoint::onTimer(DeviceContext& context, TimerId timer) {
    switching_.onTimer(context, timer);
}

} // namespace tnp::core
