#pragma once

#include "commands/Command.h"
#include "core/devices/DeviceRegistry.h"

#include <vector>

namespace tnp::commands {

/// Places a new device on the canvas.
class AddDeviceCommand final : public Command {
public:
    AddDeviceCommand(core::DeviceType type, std::string name, Vec2 position,
                     const core::DeviceRegistry& registry = core::builtinDeviceRegistry());

    [[nodiscard]] std::string_view kind() const override { return "add-device"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

    /// Identifier of the device that was created, so the caller can select it.
    [[nodiscard]] core::DeviceId deviceId() const { return id_; }

private:
    core::DeviceType type_;
    std::string name_;
    Vec2 position_;
    const core::DeviceRegistry& registry_;

    core::DeviceId id_;
    /// Kept between undo and redo so the device and its links come back exactly
    /// as they were, rather than being rebuilt from scratch.
    core::RemovedDevice removed_;
};

/// Removes devices and every link attached to them.
class DeleteDevicesCommand final : public Command {
public:
    explicit DeleteDevicesCommand(std::vector<core::DeviceId> devices);

    [[nodiscard]] std::string_view kind() const override { return "delete-devices"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    std::vector<core::DeviceId> devices_;
    std::string label_;

    struct Removed {
        core::RemovedDevice device;
        core::DevicePlacement placement;
        bool hadPlacement = false;
    };
    std::vector<Removed> removed_;
};

/// Moves devices on the canvas. Consecutive moves of the same selection merge,
/// so one drag is one undo step.
class MoveDevicesCommand final : public Command {
public:
    MoveDevicesCommand(std::vector<core::DeviceId> devices, Vec2 delta);

    [[nodiscard]] std::string_view kind() const override { return "move-devices"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;
    [[nodiscard]] bool mergeWith(const Command& next) override;

private:
    std::vector<core::DeviceId> devices_;
    Vec2 delta_;
};

/// Sets the exact position of devices, used by alignment and distribution.
class SetDevicePositionsCommand final : public Command {
public:
    SetDevicePositionsCommand(std::vector<std::pair<core::DeviceId, Vec2>> positions,
                              std::string description);

    [[nodiscard]] std::string_view kind() const override { return "set-device-positions"; }
    [[nodiscard]] std::string label() const override { return description_; }

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    std::vector<std::pair<core::DeviceId, Vec2>> target_;
    std::vector<std::pair<core::DeviceId, Vec2>> previous_;
    std::string description_;
};

class RenameDeviceCommand final : public Command {
public:
    RenameDeviceCommand(core::DeviceId device, std::string name);

    [[nodiscard]] std::string_view kind() const override { return "rename-device"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::DeviceId device_;
    std::string newName_;
    std::string oldName_;
};

class SetDeviceDescriptionCommand final : public Command {
public:
    SetDeviceDescriptionCommand(core::DeviceId device, std::string description);

    [[nodiscard]] std::string_view kind() const override { return "set-device-description"; }
    [[nodiscard]] std::string label() const override { return "Edit description"; }

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::DeviceId device_;
    std::string newValue_;
    std::string oldValue_;
};

/// Duplicates devices, without their links.
///
/// Copies deliberately do not carry cables: pasting a switch that silently
/// rewired itself into the existing topology would be the opposite of helpful.
/// Addresses are cleared for the same reason - a paste must never create a
/// duplicate-address error.
class DuplicateDevicesCommand final : public Command {
public:
    DuplicateDevicesCommand(std::vector<core::DeviceId> devices, Vec2 offset,
                            const core::DeviceRegistry& registry = core::builtinDeviceRegistry());

    [[nodiscard]] std::string_view kind() const override { return "duplicate-devices"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

    [[nodiscard]] const std::vector<core::DeviceId>& createdDevices() const { return created_; }

private:
    std::vector<core::DeviceId> sources_;
    Vec2 offset_;
    const core::DeviceRegistry& registry_;
    std::vector<core::DeviceId> created_;
};

} // namespace tnp::commands
