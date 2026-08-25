#include "commands/DeviceCommands.h"

#include <algorithm>
#include <format>

namespace tnp::commands {

using namespace core;

// --- AddDeviceCommand ------------------------------------------------------

AddDeviceCommand::AddDeviceCommand(DeviceType type, std::string name, Vec2 position,
                                   const DeviceRegistry& registry)
    : type_(type), name_(std::move(name)), position_(position), registry_(registry),
      id_(DeviceId::generate()) {}

std::string AddDeviceCommand::label() const { return std::format("Add {}", name_); }

bool AddDeviceCommand::execute(Project& project) {
    if (removed_.device) {
        // Redo: put the original object back rather than building a new one, so
        // any configuration made before the undo survives.
        project.network().restoreDevice(std::move(removed_));
        removed_ = RemovedDevice{};
    } else {
        auto device = registry_.create(type_, id_, name_);
        if (!device) return false;
        project.network().addDevice(std::move(device));
    }

    project.layout().setPosition(id_, position_);
    project.network().refreshOperationalStates();
    return true;
}

void AddDeviceCommand::undo(Project& project) {
    position_ = project.layout().position(id_);
    removed_ = project.network().removeDevice(id_);
    project.layout().remove(id_);
    project.network().refreshOperationalStates();
}

// --- DeleteDevicesCommand --------------------------------------------------

DeleteDevicesCommand::DeleteDevicesCommand(std::vector<DeviceId> devices)
    : devices_(std::move(devices)) {}

std::string DeleteDevicesCommand::label() const {
    return label_.empty() ? std::format("Delete {} device(s)", devices_.size()) : label_;
}

bool DeleteDevicesCommand::execute(Project& project) {
    removed_.clear();

    for (const DeviceId id : devices_) {
        const Device* device = project.network().findDevice(id);
        if (device == nullptr) continue;

        if (devices_.size() == 1) label_ = std::format("Delete {}", device->name());

        Removed entry;
        entry.hadPlacement = project.layout().has(id);
        if (entry.hadPlacement) {
            entry.placement.position = project.layout().position(id);
            entry.placement.locked = project.layout().isLocked(id);
        }
        entry.device = project.network().removeDevice(id);
        project.layout().remove(id);

        if (entry.device.device) removed_.push_back(std::move(entry));
    }

    if (removed_.empty()) return false;
    project.network().refreshOperationalStates();
    return true;
}

void DeleteDevicesCommand::undo(Project& project) {
    // Restored in reverse so links between two deleted devices find both ends.
    for (auto entry = removed_.rbegin(); entry != removed_.rend(); ++entry) {
        const DeviceId id = entry->device.device ? entry->device.device->id() : DeviceId{};
        project.network().restoreDevice(std::move(entry->device));
        if (entry->hadPlacement && id.isValid()) {
            project.layout().setPosition(id, entry->placement.position);
            project.layout().setLocked(id, entry->placement.locked);
        }
    }
    removed_.clear();
    project.network().refreshOperationalStates();
}

// --- MoveDevicesCommand ----------------------------------------------------

MoveDevicesCommand::MoveDevicesCommand(std::vector<DeviceId> devices, Vec2 delta)
    : devices_(std::move(devices)), delta_(delta) {}

std::string MoveDevicesCommand::label() const {
    return devices_.size() == 1 ? "Move device" : std::format("Move {} devices", devices_.size());
}

bool MoveDevicesCommand::execute(Project& project) {
    if (devices_.empty()) return false;
    if (delta_.x == 0.0f && delta_.y == 0.0f) return false;

    bool moved = false;
    for (const DeviceId id : devices_) {
        if (project.layout().isLocked(id)) continue;
        project.layout().setPosition(id, project.layout().position(id) + delta_);
        moved = true;
    }
    return moved;
}

void MoveDevicesCommand::undo(Project& project) {
    for (const DeviceId id : devices_) {
        if (project.layout().isLocked(id)) continue;
        project.layout().setPosition(id, project.layout().position(id) - delta_);
    }
}

bool MoveDevicesCommand::mergeWith(const Command& next) {
    const auto* other = dynamic_cast<const MoveDevicesCommand*>(&next);
    if (other == nullptr) return false;
    if (other->devices_ != devices_) return false;

    delta_ += other->delta_;
    return true;
}

// --- SetDevicePositionsCommand ---------------------------------------------

SetDevicePositionsCommand::SetDevicePositionsCommand(std::vector<std::pair<DeviceId, Vec2>> positions,
                                                     std::string description)
    : target_(std::move(positions)), description_(std::move(description)) {}

bool SetDevicePositionsCommand::execute(Project& project) {
    previous_.clear();
    bool changed = false;

    for (const auto& [id, position] : target_) {
        if (project.layout().isLocked(id)) continue;

        const Vec2 current = project.layout().position(id);
        if (current == position) continue;

        previous_.emplace_back(id, current);
        project.layout().setPosition(id, position);
        changed = true;
    }
    return changed;
}

void SetDevicePositionsCommand::undo(Project& project) {
    for (const auto& [id, position] : previous_) project.layout().setPosition(id, position);
    previous_.clear();
}

// --- RenameDeviceCommand ---------------------------------------------------

RenameDeviceCommand::RenameDeviceCommand(DeviceId device, std::string name)
    : device_(device), newName_(std::move(name)) {}

std::string RenameDeviceCommand::label() const { return std::format("Rename to {}", newName_); }

bool RenameDeviceCommand::execute(Project& project) {
    Device* device = project.network().findDevice(device_);
    if (device == nullptr) return false;
    if (newName_.empty() || device->name() == newName_) return false;

    oldName_ = device->name();
    device->setName(newName_);
    return true;
}

void RenameDeviceCommand::undo(Project& project) {
    if (Device* device = project.network().findDevice(device_)) device->setName(oldName_);
}

// --- SetDeviceDescriptionCommand -------------------------------------------

SetDeviceDescriptionCommand::SetDeviceDescriptionCommand(DeviceId device, std::string description)
    : device_(device), newValue_(std::move(description)) {}

bool SetDeviceDescriptionCommand::execute(Project& project) {
    Device* device = project.network().findDevice(device_);
    if (device == nullptr || device->description() == newValue_) return false;

    oldValue_ = device->description();
    device->setDescription(newValue_);
    return true;
}

void SetDeviceDescriptionCommand::undo(Project& project) {
    if (Device* device = project.network().findDevice(device_)) device->setDescription(oldValue_);
}

// --- DuplicateDevicesCommand -----------------------------------------------

DuplicateDevicesCommand::DuplicateDevicesCommand(std::vector<DeviceId> devices, Vec2 offset,
                                                 const DeviceRegistry& registry)
    : sources_(std::move(devices)), offset_(offset), registry_(registry) {}

std::string DuplicateDevicesCommand::label() const {
    return sources_.size() == 1 ? "Paste device" : std::format("Paste {} devices", sources_.size());
}

bool DuplicateDevicesCommand::execute(Project& project) {
    created_.clear();

    for (const DeviceId sourceId : sources_) {
        const Device* source = project.network().findDevice(sourceId);
        if (source == nullptr) continue;

        const std::string name = project.network().suggestDeviceName(source->type());
        auto copy = registry_.create(source->type(), DeviceId::generate(), name);
        if (!copy) continue;

        copy->setDescription(source->description());

        // Carry across the settings that describe the *hardware*, not the
        // addressing: a pasted device is a new device on the network.
        copy->clearInterfaces();
        for (const auto& iface : source->interfaces()) {
            Interface& created = copy->addInterface(InterfaceId::generate(), iface->name(),
                                                    iface->type());
            created.setDescription(iface->description());
            created.setAdminState(iface->adminState());
            created.setDuplex(iface->duplex());
            created.setSpeedMbps(iface->speedMbps());
            created.vlan() = iface->vlan();
            (void)created.setMtu(iface->mtu());
        }

        const DeviceId newId = copy->id();
        project.network().addDevice(std::move(copy));
        project.layout().setPosition(newId, project.layout().position(sourceId) + offset_);
        created_.push_back(newId);
    }

    if (created_.empty()) return false;
    project.network().refreshOperationalStates();
    return true;
}

void DuplicateDevicesCommand::undo(Project& project) {
    for (const DeviceId id : created_) {
        (void)project.network().removeDevice(id);
        project.layout().remove(id);
    }
    created_.clear();
    project.network().refreshOperationalStates();
}

} // namespace tnp::commands
