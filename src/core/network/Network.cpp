#include "core/network/Network.h"

#include "core/devices/Ipv4Stack.h"
#include "utilities/StringUtilities.h"

#include <algorithm>
#include <format>

namespace tnp::core {
namespace {

/// The medium that suits a pair of interface types.
LinkMedium defaultMediumFor(InterfaceType a, InterfaceType b) {
    if (a == InterfaceType::Serial || b == InterfaceType::Serial) return LinkMedium::Serial;
    if (a == InterfaceType::Wireless || b == InterfaceType::Wireless) return LinkMedium::Wireless;
    if (a == InterfaceType::TenGigabitEthernet && b == InterfaceType::TenGigabitEthernet) {
        return LinkMedium::Fiber;
    }
    return LinkMedium::Copper;
}

} // namespace

Network::Network() = default;
Network::~Network() = default;

// ---------------------------------------------------------------------------
// Indexing
// ---------------------------------------------------------------------------

void Network::indexDevice(Device& device) {
    deviceIndex_[device.id()] = &device;
    for (const auto& iface : device.interfaces()) {
        interfaceIndex_[iface->id()] = iface.get();
        interfaceOwner_[iface->id()] = device.id();
    }
}

void Network::unindexDevice(const Device& device) {
    deviceIndex_.erase(device.id());
    deviceLinks_.erase(device.id());
    for (const auto& iface : device.interfaces()) {
        interfaceIndex_.erase(iface->id());
        interfaceOwner_.erase(iface->id());
        interfaceLink_.erase(iface->id());
    }
}

void Network::indexLink(Link& link) {
    linkIndex_[link.id()] = &link;
    interfaceLink_[link.endpointA().interface] = link.id();
    interfaceLink_[link.endpointB().interface] = link.id();
    deviceLinks_[link.endpointA().device].push_back(link.id());
    if (link.endpointB().device != link.endpointA().device) {
        deviceLinks_[link.endpointB().device].push_back(link.id());
    }
}

void Network::unindexLink(const Link& link) {
    linkIndex_.erase(link.id());
    interfaceLink_.erase(link.endpointA().interface);
    interfaceLink_.erase(link.endpointB().interface);

    for (const DeviceId device : {link.endpointA().device, link.endpointB().device}) {
        const auto entry = deviceLinks_.find(device);
        if (entry == deviceLinks_.end()) continue;
        auto& list = entry->second;
        list.erase(std::remove(list.begin(), list.end(), link.id()), list.end());
        if (list.empty()) deviceLinks_.erase(entry);
    }
}

void Network::rebuildIndices() {
    deviceIndex_.clear();
    interfaceIndex_.clear();
    interfaceOwner_.clear();
    linkIndex_.clear();
    interfaceLink_.clear();
    deviceLinks_.clear();

    for (const auto& device : devices_) indexDevice(*device);
    for (const auto& link : links_) indexLink(*link);
}

// ---------------------------------------------------------------------------
// Devices
// ---------------------------------------------------------------------------

Device& Network::addDevice(std::unique_ptr<Device> device) {
    Device& reference = *device;
    devices_.push_back(std::move(device));
    indexDevice(reference);
    return reference;
}

RemovedDevice Network::removeDevice(DeviceId id) {
    RemovedDevice removed;

    const auto it = std::find_if(devices_.begin(), devices_.end(),
                                 [id](const auto& device) { return device->id() == id; });
    if (it == devices_.end()) return removed;

    // Take the links first: a link whose device is gone is not a link.
    for (const LinkId linkId : linksOf(id)) {
        if (auto link = disconnect(linkId)) removed.links.push_back(std::move(link));
    }

    unindexDevice(**it);
    removed.device = std::move(*it);
    devices_.erase(it);
    return removed;
}

void Network::restoreDevice(RemovedDevice removed) {
    if (!removed.device) return;

    addDevice(std::move(removed.device));
    for (auto& link : removed.links) restoreLink(std::move(link));
    refreshOperationalStates();
}

Device* Network::findDevice(DeviceId id) {
    const auto entry = deviceIndex_.find(id);
    return entry == deviceIndex_.end() ? nullptr : entry->second;
}

const Device* Network::findDevice(DeviceId id) const {
    return const_cast<Network*>(this)->findDevice(id);
}

Device* Network::findDeviceByName(std::string_view name) {
    const auto it = std::find_if(devices_.begin(), devices_.end(), [name](const auto& device) {
        return strings::equalsIgnoreCase(device->name(), name);
    });
    return it == devices_.end() ? nullptr : it->get();
}

const Device* Network::findDeviceByName(std::string_view name) const {
    return const_cast<Network*>(this)->findDeviceByName(name);
}

bool Network::hasDeviceNamed(std::string_view name) const {
    return findDeviceByName(name) != nullptr;
}

std::string Network::suggestDeviceName(DeviceType type) const {
    const std::string_view prefix = deviceTypeNamePrefix(type);
    for (std::size_t index = 1; index <= devices_.size() + 1; ++index) {
        std::string candidate = std::format("{}{}", prefix, index);
        if (!hasDeviceNamed(candidate)) return candidate;
    }
    return std::format("{}{}", prefix, devices_.size() + 1);
}

Device* Network::findDeviceWithIpv4(Ipv4Address address) {
    for (const auto& device : devices_) {
        if (device->ownsIpv4Address(address)) return device.get();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Interfaces
// ---------------------------------------------------------------------------

Interface* Network::findInterface(InterfaceId id) {
    const auto entry = interfaceIndex_.find(id);
    return entry == interfaceIndex_.end() ? nullptr : entry->second;
}

const Interface* Network::findInterface(InterfaceId id) const {
    return const_cast<Network*>(this)->findInterface(id);
}

DeviceId Network::ownerOf(InterfaceId id) const {
    const auto entry = interfaceOwner_.find(id);
    return entry == interfaceOwner_.end() ? DeviceId{} : entry->second;
}

// ---------------------------------------------------------------------------
// Links
// ---------------------------------------------------------------------------

Status Network::canConnect(InterfaceId a, InterfaceId b) const {
    if (a == b) return Status::failure("an interface cannot be connected to itself");

    const Interface* first = findInterface(a);
    const Interface* second = findInterface(b);
    if (first == nullptr || second == nullptr) return Status::failure("interface not found");

    if (!first->isConnectable()) {
        return Status::failure(std::format("{} interfaces cannot take a cable",
                                           interfaceTypeName(first->type())));
    }
    if (!second->isConnectable()) {
        return Status::failure(std::format("{} interfaces cannot take a cable",
                                           interfaceTypeName(second->type())));
    }
    if (first->isConnected()) {
        return Status::failure(std::format("{} is already connected", first->name()));
    }
    if (second->isConnected()) {
        return Status::failure(std::format("{} is already connected", second->name()));
    }
    if (!interfaceTypesAreCompatible(first->type(), second->type())) {
        return Status::failure(std::format("{} cannot be connected to {}",
                                           interfaceTypeName(first->type()),
                                           interfaceTypeName(second->type())));
    }
    return Status::ok();
}

Result<LinkId> Network::connect(InterfaceId a, InterfaceId b) {
    const Interface* first = findInterface(a);
    const Interface* second = findInterface(b);
    if (first == nullptr || second == nullptr) {
        return Result<LinkId>::failure("interface not found");
    }
    return connect(a, b, defaultMediumFor(first->type(), second->type()));
}

Result<LinkId> Network::connect(InterfaceId a, InterfaceId b, LinkMedium medium) {
    return connectWithId(LinkId::generate(), a, b, medium);
}

Result<LinkId> Network::connectWithId(LinkId id, InterfaceId a, InterfaceId b, LinkMedium medium) {
    if (const Status check = canConnect(a, b); !check) {
        return Result<LinkId>::failure(check.error());
    }
    if (linkIndex_.contains(id)) {
        return Result<LinkId>::failure("a link with that identifier already exists");
    }

    Interface* first = findInterface(a);
    Interface* second = findInterface(b);

    LinkEndpoint endpointA{ownerOf(a), a};
    LinkEndpoint endpointB{ownerOf(b), b};

    auto link = std::make_unique<Link>(id, endpointA, endpointB, medium);
    // The wire runs at the rate of the slower side, as auto-negotiation would.
    link->setBandwidthMbps(std::min(first->speedMbps(), second->speedMbps()));

    Link& reference = *link;
    links_.push_back(std::move(link));
    indexLink(reference);

    first->attachLink(id);
    second->attachLink(id);

    refreshOperationalStates();
    return id;
}

std::unique_ptr<Link> Network::disconnect(LinkId id) {
    const auto it = std::find_if(links_.begin(), links_.end(),
                                 [id](const auto& link) { return link->id() == id; });
    if (it == links_.end()) return nullptr;

    if (Interface* a = findInterface((*it)->endpointA().interface)) a->detachLink();
    if (Interface* b = findInterface((*it)->endpointB().interface)) b->detachLink();

    unindexLink(**it);
    auto link = std::move(*it);
    links_.erase(it);

    refreshOperationalStates();
    return link;
}

void Network::restoreLink(std::unique_ptr<Link> link) {
    if (!link) return;

    Interface* a = findInterface(link->endpointA().interface);
    Interface* b = findInterface(link->endpointB().interface);
    if (a == nullptr || b == nullptr) return; // the far device is gone; drop the link

    a->attachLink(link->id());
    b->attachLink(link->id());

    Link& reference = *link;
    links_.push_back(std::move(link));
    indexLink(reference);

    refreshOperationalStates();
}

Link* Network::findLink(LinkId id) {
    const auto entry = linkIndex_.find(id);
    return entry == linkIndex_.end() ? nullptr : entry->second;
}

const Link* Network::findLink(LinkId id) const {
    return const_cast<Network*>(this)->findLink(id);
}

Link* Network::linkOfInterface(InterfaceId id) {
    const auto entry = interfaceLink_.find(id);
    return entry == interfaceLink_.end() ? nullptr : findLink(entry->second);
}

const Link* Network::linkOfInterface(InterfaceId id) const {
    return const_cast<Network*>(this)->linkOfInterface(id);
}

std::vector<LinkId> Network::linksOf(DeviceId device) const {
    const auto entry = deviceLinks_.find(device);
    return entry == deviceLinks_.end() ? std::vector<LinkId>{} : entry->second;
}

std::vector<DeviceId> Network::neighborsOf(DeviceId device) const {
    std::vector<DeviceId> neighbors;
    for (const LinkId linkId : linksOf(device)) {
        const Link* link = findLink(linkId);
        if (link == nullptr) continue;
        const DeviceId other = link->endpointA().device == device ? link->endpointB().device
                                                                  : link->endpointA().device;
        if (other == device) continue;
        if (std::find(neighbors.begin(), neighbors.end(), other) == neighbors.end()) {
            neighbors.push_back(other);
        }
    }
    return neighbors;
}

// ---------------------------------------------------------------------------
// Derived state
// ---------------------------------------------------------------------------

void Network::refreshOperationalStates() {
    for (const auto& device : devices_) {
        for (const auto& iface : device->interfaces()) {
            if (!iface->isAdminUp()) {
                iface->setOperationalState(OperationalState::Down);
                continue;
            }
            if (!iface->isConnectable()) {
                // Loopbacks and SVIs have no cable to lose.
                iface->setOperationalState(OperationalState::Up);
                continue;
            }

            const Link* link = linkOfInterface(iface->id());
            if (link == nullptr || !link->isEnabled()) {
                iface->setOperationalState(OperationalState::Down);
                continue;
            }

            const auto peer = link->peerOf(iface->id());
            const Interface* other = peer ? findInterface(peer->interface) : nullptr;
            const bool peerUp = other != nullptr && other->isAdminUp();
            iface->setOperationalState(peerUp ? OperationalState::Up : OperationalState::Down);
        }
    }

    // Connected routes follow interface state, so they have to be rebuilt here
    // rather than being left to go stale.
    refreshRouting();
}

void Network::refreshRouting() {
    for (const auto& device : devices_) {
        if (Ipv4Stack* stack = device->ipv4Stack()) stack->refreshRoutes();
    }
}

void Network::clear() {
    links_.clear();
    devices_.clear();
    deviceIndex_.clear();
    interfaceIndex_.clear();
    interfaceOwner_.clear();
    linkIndex_.clear();
    interfaceLink_.clear();
    deviceLinks_.clear();
}

} // namespace tnp::core
