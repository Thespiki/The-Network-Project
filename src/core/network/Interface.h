#pragma once

#include "core/network/Ids.h"
#include "core/network/InterfaceType.h"
#include "core/network/MacAddress.h"
#include "core/network/Subnet.h"
#include "core/network/Vlan.h"
#include "utilities/Result.h"
#include "utilities/Types.h"

#include <optional>
#include <string>
#include <vector>

namespace tnp::core {

/// Whether the administrator has enabled the interface ("no shutdown").
enum class AdminState : u8 { Up, Down };

/// Whether the interface can actually pass traffic right now. Derived state:
/// admin-up plus an enabled link with a healthy peer.
enum class OperationalState : u8 { Up, Down };

enum class DuplexMode : u8 { Auto, Half, Full };

[[nodiscard]] std::string_view adminStateName(AdminState state);
[[nodiscard]] std::string_view operationalStateName(OperationalState state);
[[nodiscard]] std::string_view duplexModeName(DuplexMode mode);

/// Traffic counters, reset when the simulation is reset.
struct InterfaceCounters {
    u64 framesSent = 0;
    u64 framesReceived = 0;
    u64 bytesSent = 0;
    u64 bytesReceived = 0;
    u64 framesDropped = 0;
    u64 errors = 0;

    void reset() { *this = InterfaceCounters{}; }
};

inline constexpr u32 kDefaultMtu = 1500;
inline constexpr u32 kMinMtu = 68;    ///< smallest MTU IPv4 must support
inline constexpr u32 kMaxMtu = 9216;  ///< common jumbo-frame ceiling

/// A network interface: a first-class, addressable, connectable object.
///
/// Links attach to interfaces, never to devices, so a device with eight ports
/// really has eight independent attachment points with their own MAC address,
/// addressing, VLAN membership and counters.
class Interface {
public:
    Interface(InterfaceId id, DeviceId owner, std::string name, InterfaceType type);

    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;

    // --- Identity ----------------------------------------------------------
    [[nodiscard]] InterfaceId id() const { return id_; }
    [[nodiscard]] DeviceId ownerId() const { return owner_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    /// "Gi0/1" for "GigabitEthernet0/1".
    [[nodiscard]] std::string shortName() const;

    /// User-supplied label; falls back to `name()` when empty.
    [[nodiscard]] const std::string& displayName() const { return displayName_.empty() ? name_ : displayName_; }
    void setDisplayName(std::string value) { displayName_ = std::move(value); }

    [[nodiscard]] InterfaceType type() const { return type_; }
    void setType(InterfaceType type);

    [[nodiscard]] const std::string& description() const { return description_; }
    void setDescription(std::string value) { description_ = std::move(value); }

    // --- Layer 2 -----------------------------------------------------------
    [[nodiscard]] MacAddress macAddress() const { return mac_; }
    void setMacAddress(MacAddress mac) { mac_ = mac; }

    [[nodiscard]] VlanConfiguration& vlan() { return vlan_; }
    [[nodiscard]] const VlanConfiguration& vlan() const { return vlan_; }

    // --- Layer 3 -----------------------------------------------------------
    [[nodiscard]] const std::vector<Ipv4Prefix>& ipv4Addresses() const { return ipv4_; }
    [[nodiscard]] const std::vector<Ipv6Prefix>& ipv6Addresses() const { return ipv6_; }

    /// The address used as the source for traffic originated by this interface.
    [[nodiscard]] std::optional<Ipv4Prefix> primaryIpv4() const;
    [[nodiscard]] std::optional<Ipv6Prefix> primaryIpv6() const;

    [[nodiscard]] bool hasIpv4Address(Ipv4Address address) const;

    /// When set, the host requests its address for this interface over DHCP
    /// instead of using a statically configured one.
    [[nodiscard]] bool ipv4DhcpEnabled() const { return dhcpEnabled_; }
    void setIpv4DhcpEnabled(bool enabled) { dhcpEnabled_ = enabled; }

    /// Rejects addresses that cannot sit on a host interface (network address,
    /// broadcast address, multicast, 0.0.0.0) and duplicates on this interface.
    [[nodiscard]] Status addIpv4Address(const Ipv4Prefix& prefix);
    bool removeIpv4Address(const Ipv4Prefix& prefix);
    void clearIpv4Addresses() { ipv4_.clear(); }

    [[nodiscard]] Status addIpv6Address(const Ipv6Prefix& prefix);
    bool removeIpv6Address(const Ipv6Prefix& prefix);
    void clearIpv6Addresses() { ipv6_.clear(); }

    // --- Physical ----------------------------------------------------------
    [[nodiscard]] u32 mtu() const { return mtu_; }
    [[nodiscard]] Status setMtu(u32 value);

    [[nodiscard]] u64 speedMbps() const { return speedMbps_; }
    void setSpeedMbps(u64 value) { speedMbps_ = value; }

    [[nodiscard]] DuplexMode duplex() const { return duplex_; }
    void setDuplex(DuplexMode mode) { duplex_ = mode; }

    // --- State -------------------------------------------------------------
    [[nodiscard]] AdminState adminState() const { return adminState_; }
    void setAdminState(AdminState state) { adminState_ = state; }
    [[nodiscard]] bool isAdminUp() const { return adminState_ == AdminState::Up; }

    /// Cached operational state. Owned by `Network`, which recomputes it when a
    /// link is added, removed or disabled: interfaces do not know the topology.
    [[nodiscard]] OperationalState operationalState() const { return operState_; }
    void setOperationalState(OperationalState state) { operState_ = state; }
    [[nodiscard]] bool isOperational() const { return operState_ == OperationalState::Up; }

    // --- Connectivity ------------------------------------------------------
    [[nodiscard]] LinkId linkId() const { return link_; }
    [[nodiscard]] bool isConnected() const { return link_.isValid(); }
    void attachLink(LinkId link) { link_ = link; }
    void detachLink() { link_ = LinkId{}; }

    [[nodiscard]] bool isConnectable() const { return interfaceTypeIsConnectable(type_); }

    // --- Counters ----------------------------------------------------------
    [[nodiscard]] InterfaceCounters& counters() { return counters_; }
    [[nodiscard]] const InterfaceCounters& counters() const { return counters_; }

    /// One-line status used by the CLI and the properties panel.
    [[nodiscard]] std::string statusSummary() const;

private:
    InterfaceId id_;
    DeviceId owner_;
    std::string name_;
    std::string displayName_;
    std::string description_;
    InterfaceType type_ = InterfaceType::GigabitEthernet;

    MacAddress mac_;
    VlanConfiguration vlan_;

    std::vector<Ipv4Prefix> ipv4_;
    std::vector<Ipv6Prefix> ipv6_;
    bool dhcpEnabled_ = false;

    u32 mtu_ = kDefaultMtu;
    u64 speedMbps_ = 0;
    DuplexMode duplex_ = DuplexMode::Auto;

    AdminState adminState_ = AdminState::Up;
    OperationalState operState_ = OperationalState::Down;

    LinkId link_;
    InterfaceCounters counters_;
};

} // namespace tnp::core
