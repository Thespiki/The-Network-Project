#include "core/devices/DeviceRegistry.h"

#include "core/devices/AccessPoint.h"
#include "core/devices/Cloud.h"
#include "core/devices/Firewall.h"
#include "core/devices/Hub.h"
#include "core/devices/Layer3Switch.h"
#include "core/devices/Pc.h"
#include "core/devices/Router.h"
#include "core/devices/Server.h"
#include "core/devices/Switch.h"

#include <algorithm>

namespace tnp::core {
namespace {

DeviceTypeInfo describe(DeviceType type) {
    DeviceTypeInfo info;
    info.type = type;
    info.category = deviceTypeCategory(type);
    info.displayName = std::string{deviceTypeDisplayName(type)};
    info.description = std::string{deviceTypeDescription(type)};
    info.namePrefix = std::string{deviceTypeNamePrefix(type)};
    return info;
}

/// Registers the classes that ship with TNP.
DeviceRegistry makeBuiltinRegistry() {
    DeviceRegistry registry;

    registry.registerType(describe(DeviceType::Pc), [](DeviceId id, std::string name) {
        return std::make_unique<Pc>(id, std::move(name));
    });
    registry.registerType(describe(DeviceType::Server), [](DeviceId id, std::string name) {
        return std::make_unique<Server>(id, std::move(name));
    });
    registry.registerType(describe(DeviceType::Router), [](DeviceId id, std::string name) {
        return std::make_unique<Router>(id, std::move(name));
    });
    registry.registerType(describe(DeviceType::Switch), [](DeviceId id, std::string name) {
        return std::make_unique<Switch>(id, std::move(name));
    });
    registry.registerType(describe(DeviceType::Layer3Switch), [](DeviceId id, std::string name) {
        return std::make_unique<Layer3Switch>(id, std::move(name));
    });
    registry.registerType(describe(DeviceType::Firewall), [](DeviceId id, std::string name) {
        return std::make_unique<Firewall>(id, std::move(name));
    });
    registry.registerType(describe(DeviceType::AccessPoint), [](DeviceId id, std::string name) {
        return std::make_unique<AccessPoint>(id, std::move(name));
    });
    registry.registerType(describe(DeviceType::Hub), [](DeviceId id, std::string name) {
        return std::make_unique<Hub>(id, std::move(name));
    });
    registry.registerType(describe(DeviceType::Cloud), [](DeviceId id, std::string name) {
        return std::make_unique<Cloud>(id, std::move(name));
    });

    return registry;
}

} // namespace

void DeviceRegistry::registerType(DeviceTypeInfo info, Factory factory) {
    if (!factory) return;

    const auto existing = std::find_if(entries_.begin(), entries_.end(),
                                       [&](const Entry& entry) { return entry.info.type == info.type; });
    if (existing != entries_.end()) {
        existing->info = info;
        existing->factory = std::move(factory);
        const auto orderEntry = std::find_if(order_.begin(), order_.end(),
                                             [&](const DeviceTypeInfo& e) { return e.type == info.type; });
        if (orderEntry != order_.end()) *orderEntry = std::move(info);
        return;
    }

    order_.push_back(info);
    entries_.push_back(Entry{std::move(info), std::move(factory)});
}

std::unique_ptr<Device> DeviceRegistry::create(DeviceType type, DeviceId id, std::string name) const {
    const auto entry = std::find_if(entries_.begin(), entries_.end(),
                                    [type](const Entry& candidate) { return candidate.info.type == type; });
    if (entry == entries_.end()) return nullptr;
    return entry->factory(id, std::move(name));
}

std::unique_ptr<Device> DeviceRegistry::create(DeviceType type, std::string name) const {
    return create(type, DeviceId::generate(), std::move(name));
}

bool DeviceRegistry::isRegistered(DeviceType type) const {
    return std::any_of(entries_.begin(), entries_.end(),
                       [type](const Entry& entry) { return entry.info.type == type; });
}

const DeviceTypeInfo* DeviceRegistry::info(DeviceType type) const {
    const auto entry = std::find_if(entries_.begin(), entries_.end(),
                                    [type](const Entry& candidate) { return candidate.info.type == type; });
    return entry == entries_.end() ? nullptr : &entry->info;
}

std::vector<DeviceTypeInfo> DeviceRegistry::typesInCategory(DeviceCategory category) const {
    std::vector<DeviceTypeInfo> result;
    for (const DeviceTypeInfo& info : order_) {
        if (info.category == category) result.push_back(info);
    }
    return result;
}

const DeviceRegistry& builtinDeviceRegistry() {
    static const DeviceRegistry registry = makeBuiltinRegistry();
    return registry;
}

} // namespace tnp::core
