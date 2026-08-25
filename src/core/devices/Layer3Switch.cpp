#include "core/devices/Layer3Switch.h"

#include <format>

namespace tnp::core {
namespace {

/// Adapts the simulator's context so the IP stack can "transmit" on an SVI.
///
/// An SVI has no cable of its own. When the stack sends a frame out one, this
/// wrapper hands it to the switching engine instead, which consults the
/// forwarding database for the SVI's VLAN and picks a real port - or floods.
/// Every other service passes straight through to the real context.
class VlanEgressContext final : public DeviceContext {
public:
    VlanEgressContext(DeviceContext& inner, SwitchingEngine& switching)
        : inner_(inner), switching_(switching) {}

    [[nodiscard]] SimTime now() const override { return inner_.now(); }

    [[nodiscard]] Frame makeFrame(const Device& origin, ByteBuffer bytes, FrameCategory category,
                                  std::string summary) override {
        return inner_.makeFrame(origin, std::move(bytes), category, std::move(summary));
    }

    [[nodiscard]] Frame makeForwardedFrame(FrameIdentity identity, ByteBuffer bytes,
                                           FrameCategory category, std::string summary) override {
        return inner_.makeForwardedFrame(identity, std::move(bytes), category, std::move(summary));
    }

    void transmit(Device& sender, Interface& out, Frame frame) override {
        if (out.type() == InterfaceType::Vlan) {
            switching_.emitIntoVlan(inner_, out.vlan().accessVlan, frame);
            return;
        }
        inner_.transmit(sender, out, std::move(frame));
    }

    void loopback(Device& device, Interface& iface, Frame frame) override {
        inner_.loopback(device, iface, std::move(frame));
    }

    void scheduleTimer(Device& device, TimerId timer, Duration delay) override {
        inner_.scheduleTimer(device, timer, delay);
    }

    void cancelTimer(Device& device, TimerId timer) override { inner_.cancelTimer(device, timer); }

    void trace(TraceEvent event) override { inner_.trace(std::move(event)); }

    [[nodiscard]] std::string deviceName(DeviceId device) const override {
        return inner_.deviceName(device);
    }

    [[nodiscard]] TimerId nextTimerId() override { return inner_.nextTimerId(); }

private:
    DeviceContext& inner_;
    SwitchingEngine& switching_;
};

} // namespace

Layer3Switch::Layer3Switch(DeviceId id, std::string name, std::size_t portCount)
    : Ipv4Device(id, std::move(name)), switching_(*this) {
    createInterfaces(InterfaceType::GigabitEthernet, portCount, "0/", 1);
    ensureSvi(kDefaultVlan);

    stack_.setForwardingEnabled(true);
    bindDhcpClient();
}

Interface& Layer3Switch::ensureSvi(VlanId vlan) {
    if (Interface* existing = findSvi(vlan)) return *existing;

    Interface& svi = addInterface(std::format("Vlan{}", vlan), InterfaceType::Vlan);
    svi.vlan().mode = VlanMode::Access;
    svi.vlan().accessVlan = vlan;
    // An SVI is up as soon as it is configured; it has no cable to lose.
    svi.setOperationalState(OperationalState::Up);
    return svi;
}

Interface* Layer3Switch::findSvi(VlanId vlan) {
    for (const auto& iface : interfaces()) {
        if (iface->type() == InterfaceType::Vlan && iface->vlan().accessVlan == vlan) return iface.get();
    }
    return nullptr;
}

const Interface* Layer3Switch::findSvi(VlanId vlan) const {
    return const_cast<Layer3Switch*>(this)->findSvi(vlan);
}

void Layer3Switch::onPowerOn(DeviceContext& context) {
    VlanEgressContext adapter{context, switching_};
    Ipv4Device::onPowerOn(adapter);
    switching_.onPowerOn(context);
}

void Layer3Switch::onReset() {
    Ipv4Device::onReset();
    switching_.onReset();
}

void Layer3Switch::onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame) {
    const SwitchDecision decision = switching_.onFrameReceived(context, ingress, frame);
    if (!decision.deliverToHost) return;

    Interface* svi = findSvi(decision.vlan);
    if (svi == nullptr || !svi->isAdminUp()) return;

    VlanEgressContext adapter{context, switching_};
    stack_.onFrameReceived(adapter, *svi, frame);
}

void Layer3Switch::onTimer(DeviceContext& context, TimerId timer) {
    if (switching_.onTimer(context, timer)) return;

    VlanEgressContext adapter{context, switching_};
    Ipv4Device::onTimer(adapter, timer);
}

} // namespace tnp::core
