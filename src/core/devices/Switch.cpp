#include "core/devices/Switch.h"

namespace tnp::core {

Switch::Switch(DeviceId id, std::string name, std::size_t portCount)
    : Device(id, std::move(name)), switching_(*this) {
    // Switch ports are numbered from 1, the way the hardware is labelled.
    createInterfaces(InterfaceType::GigabitEthernet, portCount, "0/", 1);
}

void Switch::onPowerOn(DeviceContext& context) {
    Device::onPowerOn(context);
    switching_.onPowerOn(context);
}

void Switch::onReset() {
    Device::onReset();
    switching_.onReset();
}

void Switch::onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) {
    // The `deliverToHost` flag is ignored: a layer-2 switch has nothing to
    // deliver to. A layer-3 switch is the device that acts on it.
    switching_.onFrameReceived(context, ingress, frame);
}

void Switch::onTimer(DeviceContext& context, TimerId timer) {
    switching_.onTimer(context, timer);
}

} // namespace tnp::core
