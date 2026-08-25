#include "commands/InterfaceCommands.h"

#include <format>

namespace tnp::commands {

using namespace core;

namespace {

/// Resolves an interface through its owning device, so a command never holds a
/// raw pointer across an undo.
Interface* findInterface(Project& project, DeviceId device, InterfaceId iface) {
    Device* owner = project.network().findDevice(device);
    return owner == nullptr ? nullptr : owner->findInterface(iface);
}

} // namespace

InterfaceSettings InterfaceSettings::capture(const Interface& iface) {
    InterfaceSettings settings;
    settings.name = iface.name();
    settings.displayName = iface.displayName();
    settings.description = iface.description();
    settings.mac = iface.macAddress();
    settings.adminState = iface.adminState();
    settings.mtu = iface.mtu();
    settings.speedMbps = iface.speedMbps();
    settings.duplex = iface.duplex();
    settings.dhcp = iface.ipv4DhcpEnabled();
    settings.ipv4 = iface.ipv4Addresses();
    settings.ipv6 = iface.ipv6Addresses();
    settings.vlan = iface.vlan();
    return settings;
}

Status InterfaceSettings::validate() const {
    if (mtu < kMinMtu || mtu > kMaxMtu) {
        return Status::failure(std::format("MTU must be between {} and {} bytes", kMinMtu, kMaxMtu));
    }
    for (const Ipv4Prefix& prefix : ipv4) {
        if (!prefix.isUsableHostAddress(prefix.address())) {
            return Status::failure(std::format("{} cannot be configured on an interface",
                                               prefix.toString()));
        }
    }
    if (!isValidVlanId(vlan.accessVlan) || !isValidVlanId(vlan.nativeVlan)) {
        return Status::failure(std::format("VLAN ids must be between {} and {}", kMinVlanId, kMaxVlanId));
    }
    return Status::ok();
}

void InterfaceSettings::applyTo(Interface& iface) const {
    if (!name.empty()) iface.setName(name);
    iface.setDisplayName(displayName == name ? std::string{} : displayName);
    iface.setDescription(description);
    iface.setMacAddress(mac);
    iface.setAdminState(adminState);
    (void)iface.setMtu(mtu);
    iface.setSpeedMbps(speedMbps);
    iface.setDuplex(duplex);
    iface.setIpv4DhcpEnabled(dhcp);
    iface.vlan() = vlan;

    iface.clearIpv4Addresses();
    for (const Ipv4Prefix& prefix : ipv4) (void)iface.addIpv4Address(prefix);

    iface.clearIpv6Addresses();
    for (const Ipv6Prefix& prefix : ipv6) (void)iface.addIpv6Address(prefix);
}

// --- ConfigureInterfaceCommand ---------------------------------------------

ConfigureInterfaceCommand::ConfigureInterfaceCommand(DeviceId device, InterfaceId iface,
                                                     InterfaceSettings settings)
    : device_(device), interface_(iface), newSettings_(std::move(settings)) {}

std::string ConfigureInterfaceCommand::label() const {
    return interfaceName_.empty() ? "Configure interface"
                                  : std::format("Configure {}", interfaceName_);
}

bool ConfigureInterfaceCommand::execute(Project& project) {
    Interface* iface = findInterface(project, device_, interface_);
    if (iface == nullptr) return false;

    const InterfaceSettings current = InterfaceSettings::capture(*iface);
    if (current == newSettings_) return false;

    if (const Status valid = newSettings_.validate(); !valid) {
        failure_ = valid.message();
        return false;
    }

    oldSettings_ = current;
    interfaceName_ = iface->name();
    newSettings_.applyTo(*iface);

    // Addressing feeds connected routes, and admin state feeds link state.
    project.network().refreshOperationalStates();
    return true;
}

void ConfigureInterfaceCommand::undo(Project& project) {
    if (Interface* iface = findInterface(project, device_, interface_)) {
        oldSettings_.applyTo(*iface);
        project.network().refreshOperationalStates();
    }
}

// --- AddIpv4AddressCommand -------------------------------------------------

AddIpv4AddressCommand::AddIpv4AddressCommand(DeviceId device, InterfaceId iface, Ipv4Prefix address)
    : device_(device), interface_(iface), address_(address) {}

std::string AddIpv4AddressCommand::label() const {
    return std::format("Add address {}", address_.toString());
}

bool AddIpv4AddressCommand::execute(Project& project) {
    Interface* iface = findInterface(project, device_, interface_);
    if (iface == nullptr) {
        failure_ = "the interface no longer exists";
        return false;
    }

    const Status status = iface->addIpv4Address(address_);
    if (!status) {
        failure_ = status.message();
        return false;
    }

    project.network().refreshOperationalStates();
    return true;
}

void AddIpv4AddressCommand::undo(Project& project) {
    if (Interface* iface = findInterface(project, device_, interface_)) {
        iface->removeIpv4Address(address_);
        project.network().refreshOperationalStates();
    }
}

// --- RemoveIpv4AddressCommand ----------------------------------------------

RemoveIpv4AddressCommand::RemoveIpv4AddressCommand(DeviceId device, InterfaceId iface,
                                                   Ipv4Prefix address)
    : device_(device), interface_(iface), address_(address) {}

std::string RemoveIpv4AddressCommand::label() const {
    return std::format("Remove address {}", address_.toString());
}

bool RemoveIpv4AddressCommand::execute(Project& project) {
    Interface* iface = findInterface(project, device_, interface_);
    if (iface == nullptr || !iface->removeIpv4Address(address_)) return false;

    project.network().refreshOperationalStates();
    return true;
}

void RemoveIpv4AddressCommand::undo(Project& project) {
    if (Interface* iface = findInterface(project, device_, interface_)) {
        (void)iface->addIpv4Address(address_);
        project.network().refreshOperationalStates();
    }
}

// --- SetInterfaceAdminStateCommand -----------------------------------------

SetInterfaceAdminStateCommand::SetInterfaceAdminStateCommand(DeviceId device, InterfaceId iface,
                                                             AdminState state)
    : device_(device), interface_(iface), newState_(state) {}

std::string SetInterfaceAdminStateCommand::label() const {
    return newState_ == AdminState::Up ? "Enable interface" : "Shut down interface";
}

bool SetInterfaceAdminStateCommand::execute(Project& project) {
    Interface* iface = findInterface(project, device_, interface_);
    if (iface == nullptr || iface->adminState() == newState_) return false;

    oldState_ = iface->adminState();
    iface->setAdminState(newState_);
    project.network().refreshOperationalStates();
    return true;
}

void SetInterfaceAdminStateCommand::undo(Project& project) {
    if (Interface* iface = findInterface(project, device_, interface_)) {
        iface->setAdminState(oldState_);
        project.network().refreshOperationalStates();
    }
}

} // namespace tnp::commands
