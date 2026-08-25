#pragma once

#include "core/network/Device.h"
#include "core/network/DeviceType.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tnp::core {

/// Everything the palette and the deserializer need to know about a device kind.
struct DeviceTypeInfo {
    DeviceType type = DeviceType::Pc;
    DeviceCategory category = DeviceCategory::Computers;
    std::string displayName;
    std::string description;
    /// Prefix used when naming new instances: "Router" becomes "Router1".
    std::string namePrefix;
};

/// Creates devices by type.
///
/// The one place that knows which concrete class implements which `DeviceType`.
/// The palette enumerates it, the "add device" command creates through it, and
/// the deserializer reconstructs through it - so adding a device kind means
/// writing the class and registering it here, with no other file to touch.
///
/// A future plugin system registers into an instance of this class; that is why
/// it is an ordinary object with a factory table rather than a switch statement
/// buried in a loader.
class DeviceRegistry {
public:
    using Factory = std::function<std::unique_ptr<Device>(DeviceId, std::string)>;

    void registerType(DeviceTypeInfo info, Factory factory);

    /// Creates a device, or nullptr when the type is not registered.
    [[nodiscard]] std::unique_ptr<Device> create(DeviceType type, DeviceId id, std::string name) const;

    /// Creates with a freshly generated identifier.
    [[nodiscard]] std::unique_ptr<Device> create(DeviceType type, std::string name) const;

    [[nodiscard]] bool isRegistered(DeviceType type) const;
    [[nodiscard]] const DeviceTypeInfo* info(DeviceType type) const;

    /// Registered kinds, in registration order.
    [[nodiscard]] const std::vector<DeviceTypeInfo>& types() const { return order_; }

    /// Registered kinds belonging to `category`.
    [[nodiscard]] std::vector<DeviceTypeInfo> typesInCategory(DeviceCategory category) const;

private:
    struct Entry {
        DeviceTypeInfo info;
        Factory factory;
    };

    std::vector<Entry> entries_;
    std::vector<DeviceTypeInfo> order_;
};

/// The registry containing the device kinds that ship with TNP.
///
/// Const, and initialised once on first use, so it is shared global *data*
/// rather than shared global mutable state.
[[nodiscard]] const DeviceRegistry& builtinDeviceRegistry();

} // namespace tnp::core
