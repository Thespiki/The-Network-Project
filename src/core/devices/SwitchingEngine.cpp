#include "core/devices/SwitchingEngine.h"

#include "core/network/Device.h"
#include "core/protocols/Ethernet.h"

#include <algorithm>
#include <format>

namespace tnp::core {
namespace {

/// How often the forwarding database is swept for aged-out entries.
constexpr Duration kAgeingSweepInterval = seconds(15);

} // namespace

SwitchingEngine::SwitchingEngine(Device& owner) : owner_(owner) {}

void SwitchingEngine::addVlan(VlanDefinition definition) {
    if (!isValidVlanId(definition.id)) return;
    const auto it = std::find_if(vlans_.begin(), vlans_.end(),
                                 [&](const VlanDefinition& existing) { return existing.id == definition.id; });
    if (it != vlans_.end()) {
        it->name = std::move(definition.name);
        return;
    }
    vlans_.push_back(std::move(definition));
    std::sort(vlans_.begin(), vlans_.end(),
              [](const VlanDefinition& a, const VlanDefinition& b) { return a.id < b.id; });
}

bool SwitchingEngine::removeVlan(VlanId vlan) {
    if (vlan == kDefaultVlan) return false; // VLAN 1 always exists
    const auto it = std::find_if(vlans_.begin(), vlans_.end(),
                                 [vlan](const VlanDefinition& existing) { return existing.id == vlan; });
    if (it == vlans_.end()) return false;
    vlans_.erase(it);
    macTable_.removeByVlan(vlan);
    return true;
}

const VlanDefinition* SwitchingEngine::findVlan(VlanId vlan) const {
    const auto it = std::find_if(vlans_.begin(), vlans_.end(),
                                 [vlan](const VlanDefinition& existing) { return existing.id == vlan; });
    return it == vlans_.end() ? nullptr : &*it;
}

void SwitchingEngine::setVlans(std::vector<VlanDefinition> definitions) {
    vlans_ = std::move(definitions);
    if (findVlan(kDefaultVlan) == nullptr) {
        vlans_.insert(vlans_.begin(), VlanDefinition{kDefaultVlan, "default"});
    }
    std::sort(vlans_.begin(), vlans_.end(),
              [](const VlanDefinition& a, const VlanDefinition& b) { return a.id < b.id; });
}

void SwitchingEngine::onPowerOn(DeviceContext& context) { scheduleAgeing(context); }

void SwitchingEngine::onReset() {
    macTable_.clear();
    ageingTimers_.clear();
}

void SwitchingEngine::scheduleAgeing(DeviceContext& context) {
    const TimerId timer = context.nextTimerId();
    ageingTimers_.push_back(timer);
    context.scheduleTimer(owner_, timer, kAgeingSweepInterval);
}

bool SwitchingEngine::onTimer(DeviceContext& context, TimerId timer) {
    const auto it = std::find(ageingTimers_.begin(), ageingTimers_.end(), timer);
    if (it == ageingTimers_.end()) return false;
    ageingTimers_.erase(it);

    for (const MacTableEntry& entry : macTable_.removeExpired(context.now())) {
        context.trace(TraceEvent{.kind = TraceKind::MacEntryAged,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = entry.port,
                                 .summary = std::format("{} aged out of VLAN {}",
                                                        entry.mac.toString(), entry.vlan)}
                          .with("mac", entry.mac.toString())
                          .with("vlan", std::to_string(entry.vlan)));
    }

    scheduleAgeing(context);
    return true;
}

void SwitchingEngine::emitIntoVlan(DeviceContext& context, VlanId vlan, const Frame& frame) {
    const auto ethernet = proto::decodeEthernet(frame.bytes);
    if (!ethernet) return;

    const MacAddress destination = ethernet->header.destination;
    const bool flooding = destination.isBroadcast() || destination.isMulticast();

    if (!flooding) {
        if (const MacTableEntry* entry = macTable_.lookup(vlan, destination, context.now())) {
            if (Interface* egress = owner_.findInterface(entry->port)) {
                if (egress->isOperational() && egress->vlan().allowsVlan(vlan)) {
                    forwardTo(context, *egress, vlan, ethernet->header, ethernet->payload, frame);
                    return;
                }
            }
        }
    }

    std::size_t sent = 0;
    for (const auto& candidate : owner_.interfaces()) {
        if (!candidate->isConnectable()) continue;
        if (!candidate->isOperational()) continue;
        if (!candidate->vlan().allowsVlan(vlan)) continue;
        forwardTo(context, *candidate, vlan, ethernet->header, ethernet->payload, frame);
        ++sent;
    }

    context.trace(TraceEvent{.kind = TraceKind::FrameFlooded,
                             .time = context.now(),
                             .device = owner_.id(),
                             .packet = frame.id,
                             .summary = std::format("locally originated frame flooded to {} port(s) in VLAN {}",
                                                    sent, vlan)}
                      .with("ports", std::to_string(sent))
                      .with("vlan", std::to_string(vlan))
                      .with("reason", flooding ? "broadcast" : "unknown unicast"));
}

std::optional<VlanId> SwitchingEngine::classify(const Interface& ingress,
                                                const std::optional<proto::VlanTag>& tag) const {
    if (!tag) {
        // Untagged: an access port assigns its access VLAN, a trunk its native.
        return ingress.vlan().untaggedVlan();
    }
    if (!isValidVlanId(tag->vlanId)) return std::nullopt;
    if (!ingress.vlan().allowsVlan(tag->vlanId)) return std::nullopt;
    return tag->vlanId;
}

bool SwitchingEngine::isOwnAddress(MacAddress mac) const {
    for (const auto& iface : owner_.interfaces()) {
        if (iface->macAddress() == mac) return true;
    }
    return false;
}

ByteBuffer SwitchingEngine::encapsulateFor(const Interface& egress, VlanId vlan,
                                           const proto::EthernetHeader& header,
                                           std::span<const u8> payload) const {
    proto::EthernetHeader outgoing = header;
    if (egress.vlan().shouldTagOnEgress(vlan)) {
        proto::VlanTag tag;
        tag.vlanId = vlan;
        if (header.vlanTag) tag.priorityCodePoint = header.vlanTag->priorityCodePoint;
        outgoing.vlanTag = tag;
    } else {
        outgoing.vlanTag.reset();
    }
    return proto::encodeEthernet(outgoing, payload);
}

void SwitchingEngine::forwardTo(DeviceContext& context, Interface& egress, VlanId vlan,
                                const proto::EthernetHeader& header, std::span<const u8> payload,
                                const Frame& original) {
    ByteBuffer bytes = encapsulateFor(egress, vlan, header, payload);

    // A bridge does not touch the packet inside, so the identity - including the
    // hop count - is carried over unchanged.
    Frame frame = context.makeForwardedFrame(original.identity(), std::move(bytes), original.category,
                                             original.summary);
    context.transmit(owner_, egress, std::move(frame));
}

std::size_t SwitchingEngine::flood(DeviceContext& context, Interface& ingress, VlanId vlan,
                                   const proto::EthernetHeader& header, std::span<const u8> payload,
                                   const Frame& original) {
    std::size_t sent = 0;
    for (const auto& candidate : owner_.interfaces()) {
        if (candidate->id() == ingress.id()) continue;
        if (!candidate->isConnectable()) continue;
        if (!candidate->isOperational()) continue;
        if (!candidate->vlan().allowsVlan(vlan)) continue;

        forwardTo(context, *candidate, vlan, header, payload, original);
        ++sent;
    }
    return sent;
}

SwitchDecision SwitchingEngine::onFrameReceived(DeviceContext& context, Interface& ingress,
                                                const Frame& frame) {
    SwitchDecision decision;

    const auto ethernet = proto::decodeEthernet(frame.bytes);
    if (!ethernet) {
        context.trace(TraceEvent{.kind = TraceKind::FrameDropped,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = "malformed Ethernet frame discarded"});
        return decision;
    }

    const auto vlan = classify(ingress, ethernet->header.vlanTag);
    if (!vlan) {
        context.trace(TraceEvent{.kind = TraceKind::FrameFilteredByVlan,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("{} does not carry VLAN {}", ingress.name(),
                                                        ethernet->header.vlanTag->vlanId)}
                          .with("vlan", std::to_string(ethernet->header.vlanTag->vlanId))
                          .with("interface", ingress.name()));
        return decision;
    }
    decision.vlan = *vlan;

    // Learn the source. A multicast source address is illegal, so it is never
    // learned; that is what keeps a bogus frame from poisoning the table.
    if (learning_ && ethernet->header.source.isUnicast()) {
        const MacLearnResult result = macTable_.learn(*vlan, ethernet->header.source, ingress.id(),
                                                      context.now(), ageingTime_);
        if (result != MacLearnResult::Unchanged) {
            context.trace(TraceEvent{
                .kind = TraceKind::MacLearned,
                .time = context.now(),
                .device = owner_.id(),
                .interface = ingress.id(),
                .packet = frame.id,
                .summary = result == MacLearnResult::Moved
                               ? std::format("{} moved to {} in VLAN {}",
                                             ethernet->header.source.toString(), ingress.name(), *vlan)
                               : std::format("{} learned on {} in VLAN {}",
                                             ethernet->header.source.toString(), ingress.name(), *vlan)}
                    .with("mac", ethernet->header.source.toString())
                    .with("port", ingress.name())
                    .with("vlan", std::to_string(*vlan))
                    .with("moved", result == MacLearnResult::Moved ? "true" : "false"));
        }
    }

    const MacAddress destination = ethernet->header.destination;

    if (destination.isBroadcast() || destination.isMulticast()) {
        decision.deliverToHost = true; // the device's own stack must see broadcasts too

        const std::size_t sent = flood(context, ingress, *vlan, ethernet->header, ethernet->payload, frame);
        decision.bridged = sent > 0;

        context.trace(TraceEvent{.kind = TraceKind::FrameFlooded,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = ingress.id(),
                                 .packet = frame.id,
                                 .summary = std::format("broadcast flooded to {} port(s) in VLAN {}",
                                                        sent, *vlan)}
                          .with("ports", std::to_string(sent))
                          .with("vlan", std::to_string(*vlan))
                          .with("reason", "broadcast"));
        return decision;
    }

    if (isOwnAddress(destination)) {
        decision.deliverToHost = true;
        return decision;
    }

    if (const MacTableEntry* entry = macTable_.lookup(*vlan, destination, context.now())) {
        if (entry->port == ingress.id()) {
            context.trace(TraceEvent{.kind = TraceKind::FrameDropped,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .interface = ingress.id(),
                                     .packet = frame.id,
                                     .summary = std::format("{} is known on the ingress port; frame filtered",
                                                            destination.toString())});
            return decision;
        }

        Interface* egress = owner_.findInterface(entry->port);
        if (egress == nullptr || !egress->isOperational()) {
            context.trace(TraceEvent{.kind = TraceKind::FrameDropped,
                                     .time = context.now(),
                                     .device = owner_.id(),
                                     .interface = ingress.id(),
                                     .packet = frame.id,
                                     .summary = "the port for the destination address is down"});
            return decision;
        }

        context.trace(TraceEvent{.kind = TraceKind::FrameSwitched,
                                 .time = context.now(),
                                 .device = owner_.id(),
                                 .interface = egress->id(),
                                 .packet = frame.id,
                                 .summary = std::format("{} is known on {}; forwarding in VLAN {}",
                                                        destination.toString(), egress->name(), *vlan)}
                          .with("destination-mac", destination.toString())
                          .with("port", egress->name())
                          .with("vlan", std::to_string(*vlan)));

        forwardTo(context, *egress, *vlan, ethernet->header, ethernet->payload, frame);
        decision.bridged = true;
        return decision;
    }

    // Unknown unicast: flood, exactly as 802.1D requires, and let the reply
    // teach the table where the address lives.
    const std::size_t sent = flood(context, ingress, *vlan, ethernet->header, ethernet->payload, frame);
    decision.bridged = sent > 0;

    context.trace(TraceEvent{.kind = TraceKind::FrameFlooded,
                             .time = context.now(),
                             .device = owner_.id(),
                             .interface = ingress.id(),
                             .packet = frame.id,
                             .summary = std::format("{} is unknown in VLAN {}; flooded to {} port(s)",
                                                    destination.toString(), *vlan, sent)}
                      .with("destination-mac", destination.toString())
                      .with("ports", std::to_string(sent))
                      .with("vlan", std::to_string(*vlan))
                      .with("reason", "unknown unicast"));
    return decision;
}

} // namespace tnp::core
