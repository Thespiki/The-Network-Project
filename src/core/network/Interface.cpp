#include "core/network/Interface.h"

#include "utilities/StringUtilities.h"

#include <algorithm>
#include <format>

namespace tnp::core {

std::string_view adminStateName(AdminState state) {
    return state == AdminState::Up ? "up" : "administratively down";
}

std::string_view operationalStateName(OperationalState state) {
    return state == OperationalState::Up ? "up" : "down";
}

std::string_view duplexModeName(DuplexMode mode) {
    switch (mode) {
        case DuplexMode::Auto: return "auto";
        case DuplexMode::Half: return "half";
        case DuplexMode::Full: return "full";
    }
    return "auto";
}

Interface::Interface(InterfaceId id, DeviceId owner, std::string name, InterfaceType type)
    : id_(id), owner_(owner), name_(std::move(name)), type_(type) {
    speedMbps_ = interfaceTypeDefaultSpeedMbps(type);
    if (interfaceTypeIsEthernetLike(type)) mac_ = MacAddress::generateUnicast();
    if (type == InterfaceType::Loopback) operState_ = OperationalState::Up;
}

void Interface::setType(InterfaceType type) {
    type_ = type;
    speedMbps_ = interfaceTypeDefaultSpeedMbps(type);
    if (interfaceTypeIsEthernetLike(type) && mac_.isZero()) mac_ = MacAddress::generateUnicast();
}

std::string Interface::shortName() const {
    const std::string_view fullName = interfaceTypeName(type_);
    if (strings::startsWith(name_, fullName)) {
        return std::string{interfaceTypePrefix(type_)} + name_.substr(fullName.size());
    }
    return name_;
}

std::optional<Ipv4Prefix> Interface::primaryIpv4() const {
    if (ipv4_.empty()) return std::nullopt;
    return ipv4_.front();
}

std::optional<Ipv6Prefix> Interface::primaryIpv6() const {
    if (ipv6_.empty()) return std::nullopt;
    return ipv6_.front();
}

bool Interface::hasIpv4Address(Ipv4Address address) const {
    return std::any_of(ipv4_.begin(), ipv4_.end(),
                       [address](const Ipv4Prefix& prefix) { return prefix.address() == address; });
}

Status Interface::addIpv4Address(const Ipv4Prefix& prefix) {
    if (!prefix.address().isAssignableToHost()) {
        return Status::failure(std::format("{} cannot be assigned to an interface",
                                           prefix.address().toString()));
    }
    if (!prefix.isUsableHostAddress(prefix.address())) {
        const bool isNetwork = prefix.address() == prefix.networkAddress();
        return Status::failure(std::format("{} is the {} address of {}",
                                           prefix.address().toString(),
                                           isNetwork ? "network" : "broadcast",
                                           prefix.toNetworkString()));
    }
    if (hasIpv4Address(prefix.address())) {
        return Status::failure(std::format("{} is already configured on {}",
                                           prefix.address().toString(), name_));
    }
    ipv4_.push_back(prefix);
    return Status::ok();
}

bool Interface::removeIpv4Address(const Ipv4Prefix& prefix) {
    const auto it = std::find(ipv4_.begin(), ipv4_.end(), prefix);
    if (it == ipv4_.end()) return false;
    ipv4_.erase(it);
    return true;
}

Status Interface::addIpv6Address(const Ipv6Prefix& prefix) {
    if (prefix.address().isUnspecified() || prefix.address().isMulticast()) {
        return Status::failure(std::format("{} cannot be assigned to an interface",
                                           prefix.address().toString()));
    }
    const auto it = std::find_if(ipv6_.begin(), ipv6_.end(), [&](const Ipv6Prefix& existing) {
        return existing.address() == prefix.address();
    });
    if (it != ipv6_.end()) {
        return Status::failure(std::format("{} is already configured on {}",
                                           prefix.address().toString(), name_));
    }
    ipv6_.push_back(prefix);
    return Status::ok();
}

bool Interface::removeIpv6Address(const Ipv6Prefix& prefix) {
    const auto it = std::find(ipv6_.begin(), ipv6_.end(), prefix);
    if (it == ipv6_.end()) return false;
    ipv6_.erase(it);
    return true;
}

Status Interface::setMtu(u32 value) {
    if (value < kMinMtu || value > kMaxMtu) {
        return Status::failure(std::format("MTU must be between {} and {} bytes", kMinMtu, kMaxMtu));
    }
    mtu_ = value;
    return Status::ok();
}

std::string Interface::statusSummary() const {
    const std::string address = ipv4_.empty() ? std::string{"unassigned"} : ipv4_.front().toString();
    return std::format("{} is {}, line protocol is {} [{}]",
                       name_, adminStateName(adminState_),
                       operationalStateName(operState_), address);
}

} // namespace tnp::core
