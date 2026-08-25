#pragma once

#include "core/devices/MacAddressTable.h"
#include "core/network/DeviceContext.h"
#include "core/network/Frame.h"
#include "core/network/Interface.h"
#include "core/network/Vlan.h"
#include "core/protocols/Ethernet.h"

#include <map>
#include <optional>
#include <vector>

namespace tnp::core {

class Device;

/// What the bridge did with a frame.
struct SwitchDecision {
    /// The frame was sent out at least one port.
    bool bridged = false;
    /// The frame is also for this device itself: it was addressed to one of its
    /// own MAC addresses, or it was a broadcast. A layer-3 switch hands these to
    /// its IP stack; a plain layer-2 switch ignores the flag.
    bool deliverToHost = false;
    /// VLAN the frame was classified into on ingress.
    VlanId vlan = kDefaultVlan;
};

/// The forwarding logic of a bridge: MAC learning, unicast forwarding, unknown
/// unicast and broadcast flooding, and 802.1Q tagging.
///
/// Composed into `Switch`, `Layer3Switch` and `AccessPoint`. A `Hub` does not
/// use it - repeating every frame to every port is precisely the behaviour a
/// bridge exists to avoid, so it is implemented separately.
class SwitchingEngine {
public:
    explicit SwitchingEngine(Device& owner);

    SwitchingEngine(const SwitchingEngine&) = delete;
    SwitchingEngine& operator=(const SwitchingEngine&) = delete;

    [[nodiscard]] MacAddressTable& macTable() { return macTable_; }
    [[nodiscard]] const MacAddressTable& macTable() const { return macTable_; }

    // --- VLAN database -----------------------------------------------------
    [[nodiscard]] const std::vector<VlanDefinition>& vlans() const { return vlans_; }
    void addVlan(VlanDefinition definition);
    bool removeVlan(VlanId vlan);
    [[nodiscard]] const VlanDefinition* findVlan(VlanId vlan) const;
    void setVlans(std::vector<VlanDefinition> definitions);

    // --- Settings ----------------------------------------------------------
    [[nodiscard]] bool learningEnabled() const { return learning_; }
    void setLearningEnabled(bool enabled) { learning_ = enabled; }

    [[nodiscard]] Duration ageingTime() const { return ageingTime_; }
    void setAgeingTime(Duration ageing) { ageingTime_ = ageing; }

    // --- Simulation --------------------------------------------------------
    void onPowerOn(DeviceContext& context);
    void onReset();

    /// Classifies, learns from and forwards a frame.
    SwitchDecision onFrameReceived(DeviceContext& context, Interface& ingress, const Frame& frame);

    /// Handles the periodic ageing timer. Returns true when `timer` belonged to
    /// this engine.
    bool onTimer(DeviceContext& context, TimerId timer);

    /// Sends a frame this device originated into `vlan`.
    ///
    /// Used by a layer-3 switch whose IP stack transmits on a VLAN interface:
    /// the frame has no physical egress port of its own, so the forwarding
    /// database picks one, or it is flooded when the destination is unknown.
    void emitIntoVlan(DeviceContext& context, VlanId vlan, const Frame& frame);

private:
    /// VLAN a frame belongs to, or nullopt when the port rejects it.
    [[nodiscard]] std::optional<VlanId> classify(const Interface& ingress,
                                                 const std::optional<proto::VlanTag>& tag) const;

    /// True when `mac` is one of this device's own interface addresses.
    [[nodiscard]] bool isOwnAddress(MacAddress mac) const;

    /// Rebuilds the frame for `egress`, adding or removing the 802.1Q tag as the
    /// port requires.
    [[nodiscard]] ByteBuffer encapsulateFor(const Interface& egress, VlanId vlan,
                                            const proto::EthernetHeader& header,
                                            std::span<const u8> payload) const;

    void forwardTo(DeviceContext& context, Interface& egress, VlanId vlan,
                   const proto::EthernetHeader& header, std::span<const u8> payload,
                   const Frame& original);

    std::size_t flood(DeviceContext& context, Interface& ingress, VlanId vlan,
                      const proto::EthernetHeader& header, std::span<const u8> payload,
                      const Frame& original);

    void scheduleAgeing(DeviceContext& context);

    Device& owner_;
    MacAddressTable macTable_;
    std::vector<VlanDefinition> vlans_{VlanDefinition{kDefaultVlan, "default"}};
    bool learning_ = true;
    Duration ageingTime_ = kDefaultMacAgeingTime;
    std::vector<TimerId> ageingTimers_;
};

} // namespace tnp::core
