#pragma once

#include "core/network/Device.h"
#include "core/network/Ids.h"
#include "core/network/Link.h"
#include "utilities/Result.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tnp::core {

/// A device that was taken out of the network, with the links that went with it.
///
/// Returned rather than destroyed so the undo stack can put it back exactly as
/// it was, identifiers included.
struct RemovedDevice {
    std::unique_ptr<Device> device;
    std::vector<std::unique_ptr<Link>> links;
};

/// The topology: devices, their interfaces, and the links between interfaces.
///
/// Storage is ordered (a vector) so serialization and simulation are
/// reproducible; lookup goes through hash indices so neither the renderer nor
/// the simulator ever scans the whole network to answer "what is attached to
/// this interface". Keeping both is what lets a project with hundreds of devices
/// stay responsive.
class Network {
public:
    Network();
    ~Network();

    Network(const Network&) = delete;
    Network& operator=(const Network&) = delete;

    // --- Devices -----------------------------------------------------------
    Device& addDevice(std::unique_ptr<Device> device);

    /// Removes a device and detaches every link that touched it.
    [[nodiscard]] RemovedDevice removeDevice(DeviceId id);

    /// Puts a previously removed device (and its links) back.
    void restoreDevice(RemovedDevice removed);

    [[nodiscard]] Device* findDevice(DeviceId id);
    [[nodiscard]] const Device* findDevice(DeviceId id) const;

    /// Case-insensitive lookup by name. Names are not indexed: they change often
    /// and lookups by name only happen from the CLI and from tests.
    [[nodiscard]] Device* findDeviceByName(std::string_view name);
    [[nodiscard]] const Device* findDeviceByName(std::string_view name) const;

    [[nodiscard]] const std::vector<std::unique_ptr<Device>>& devices() const { return devices_; }
    [[nodiscard]] std::size_t deviceCount() const { return devices_.size(); }

    /// The first free name of the form "Router1", "Router2", ...
    [[nodiscard]] std::string suggestDeviceName(DeviceType type) const;
    [[nodiscard]] bool hasDeviceNamed(std::string_view name) const;

    /// The device that owns `address`, or nullptr. Used by tests and the CLI to
    /// turn a typed address into a device.
    [[nodiscard]] Device* findDeviceWithIpv4(Ipv4Address address);

    // --- Interfaces --------------------------------------------------------
    [[nodiscard]] Interface* findInterface(InterfaceId id);
    [[nodiscard]] const Interface* findInterface(InterfaceId id) const;
    [[nodiscard]] DeviceId ownerOf(InterfaceId id) const;

    // --- Links -------------------------------------------------------------
    /// Checks whether two interfaces may be joined, without changing anything.
    [[nodiscard]] Status canConnect(InterfaceId a, InterfaceId b) const;

    /// Connects two interfaces. The medium defaults to whatever suits the
    /// interface types.
    [[nodiscard]] Result<LinkId> connect(InterfaceId a, InterfaceId b);
    [[nodiscard]] Result<LinkId> connect(InterfaceId a, InterfaceId b, LinkMedium medium);

    /// Connects with a caller-supplied identifier. Used by deserialization and
    /// by undo, both of which must preserve identity.
    [[nodiscard]] Result<LinkId> connectWithId(LinkId id, InterfaceId a, InterfaceId b, LinkMedium medium);

    [[nodiscard]] std::unique_ptr<Link> disconnect(LinkId id);
    void restoreLink(std::unique_ptr<Link> link);

    [[nodiscard]] Link* findLink(LinkId id);
    [[nodiscard]] const Link* findLink(LinkId id) const;

    [[nodiscard]] Link* linkOfInterface(InterfaceId id);
    [[nodiscard]] const Link* linkOfInterface(InterfaceId id) const;

    [[nodiscard]] const std::vector<std::unique_ptr<Link>>& links() const { return links_; }
    [[nodiscard]] std::size_t linkCount() const { return links_.size(); }

    /// Identifiers of every link attached to `device`.
    [[nodiscard]] std::vector<LinkId> linksOf(DeviceId device) const;

    /// Devices one hop away from `device`.
    [[nodiscard]] std::vector<DeviceId> neighborsOf(DeviceId device) const;

    // --- Derived state -----------------------------------------------------
    /// Recomputes every interface's operational state from admin state and link
    /// health, then rebuilds the connected routes that depend on it.
    void refreshOperationalStates();

    /// Rebuilds the routing table of every device that has one.
    void refreshRouting();

    /// Rebuilds the lookup indices from the stored objects. Called after
    /// deserialization, which fills the containers directly.
    void rebuildIndices();

    void clear();

private:
    void indexDevice(Device& device);
    void unindexDevice(const Device& device);
    void indexLink(Link& link);
    void unindexLink(const Link& link);

    std::vector<std::unique_ptr<Device>> devices_;
    std::vector<std::unique_ptr<Link>> links_;

    std::unordered_map<DeviceId, Device*> deviceIndex_;
    std::unordered_map<InterfaceId, Interface*> interfaceIndex_;
    std::unordered_map<InterfaceId, DeviceId> interfaceOwner_;
    std::unordered_map<LinkId, Link*> linkIndex_;
    std::unordered_map<InterfaceId, LinkId> interfaceLink_;
    std::unordered_map<DeviceId, std::vector<LinkId>> deviceLinks_;
};

} // namespace tnp::core
