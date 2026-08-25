#include "core/network/Device.h"

#include "core/devices/Ipv4Stack.h"
#include "core/devices/SwitchingEngine.h"
#include "utilities/StringUtilities.h"

#include <algorithm>
#include <format>

namespace tnp::core {

Device::Device(DeviceId id, std::string name) : id_(id), name_(std::move(name)) {}

Device::~Device() = default;

Interface& Device::addInterface(std::string name, InterfaceType type) {
    return addInterface(InterfaceId::generate(), std::move(name), type);
}

Interface& Device::addInterface(InterfaceId id, std::string name, InterfaceType type) {
    interfaces_.push_back(std::make_unique<Interface>(id, id_, std::move(name), type));
    return *interfaces_.back();
}

bool Device::removeInterface(InterfaceId id) {
    const auto it = std::find_if(interfaces_.begin(), interfaces_.end(),
                                 [id](const auto& iface) { return iface->id() == id; });
    if (it == interfaces_.end()) return false;
    interfaces_.erase(it);
    return true;
}

void Device::clearInterfaces() { interfaces_.clear(); }

Interface* Device::findInterface(InterfaceId id) {
    const auto it = std::find_if(interfaces_.begin(), interfaces_.end(),
                                 [id](const auto& iface) { return iface->id() == id; });
    return it == interfaces_.end() ? nullptr : it->get();
}

const Interface* Device::findInterface(InterfaceId id) const {
    return const_cast<Device*>(this)->findInterface(id);
}

Interface* Device::findInterfaceByName(std::string_view name) {
    const std::string wanted = strings::trim(name);
    if (wanted.empty()) return nullptr;

    for (const auto& iface : interfaces_) {
        if (strings::equalsIgnoreCase(iface->name(), wanted)) return iface.get();
    }
    for (const auto& iface : interfaces_) {
        if (strings::equalsIgnoreCase(iface->shortName(), wanted)) return iface.get();
    }

    // Abbreviated form: "gi0/1" for "GigabitEthernet0/1". Split the alphabetic
    // prefix from the numeric suffix and match each part independently.
    const auto digitStart = wanted.find_first_of("0123456789");
    if (digitStart == std::string::npos || digitStart == 0) return nullptr;

    const std::string typePart = wanted.substr(0, digitStart);
    const std::string portPart = wanted.substr(digitStart);

    Interface* match = nullptr;
    for (const auto& iface : interfaces_) {
        const std::string fullName = iface->name();
        const auto ifaceDigits = fullName.find_first_of("0123456789");
        if (ifaceDigits == std::string::npos) continue;
        if (fullName.substr(ifaceDigits) != portPart) continue;
        if (!strings::isAbbreviation(typePart, fullName.substr(0, ifaceDigits))) continue;
        if (match != nullptr) return nullptr; // ambiguous
        match = iface.get();
    }
    return match;
}

const Interface* Device::findInterfaceByName(std::string_view name) const {
    return const_cast<Device*>(this)->findInterfaceByName(name);
}

Interface* Device::findInterfaceWithIpv4(Ipv4Address address) {
    for (const auto& iface : interfaces_) {
        if (iface->hasIpv4Address(address)) return iface.get();
    }
    return nullptr;
}

const Interface* Device::findInterfaceWithIpv4(Ipv4Address address) const {
    return const_cast<Device*>(this)->findInterfaceWithIpv4(address);
}

bool Device::ownsIpv4Address(Ipv4Address address) const {
    return findInterfaceWithIpv4(address) != nullptr;
}

void Device::onPowerOn(DeviceContext& context) {
    context.trace(TraceEvent{.kind = TraceKind::DevicePoweredOn,
                             .time = context.now(),
                             .device = id_,
                             .summary = std::format("{} powered on", name_)});
}

void Device::onReset() {
    for (auto& iface : interfaces_) iface->counters().reset();
}

void Device::onTimer(DeviceContext&, TimerId) {}

void Device::createInterfaces(InterfaceType type, std::size_t count, std::string_view slotPrefix,
                              std::size_t startIndex) {
    for (std::size_t i = 0; i < count; ++i) {
        addInterface(std::format("{}{}{}", interfaceTypeName(type), slotPrefix, startIndex + i), type);
    }
}

RoutingTable* Device::routingTable() {
    auto* stack = ipv4Stack();
    return stack ? &stack->routingTable() : nullptr;
}

const RoutingTable* Device::routingTable() const {
    const auto* stack = ipv4Stack();
    return stack ? &stack->routingTable() : nullptr;
}

ArpCache* Device::arpCache() {
    auto* stack = ipv4Stack();
    return stack ? &stack->arpCache() : nullptr;
}

const ArpCache* Device::arpCache() const {
    const auto* stack = ipv4Stack();
    return stack ? &stack->arpCache() : nullptr;
}

MacAddressTable* Device::macTable() {
    auto* engine = switching();
    return engine ? &engine->macTable() : nullptr;
}

const MacAddressTable* Device::macTable() const {
    const auto* engine = switching();
    return engine ? &engine->macTable() : nullptr;
}

} // namespace tnp::core
