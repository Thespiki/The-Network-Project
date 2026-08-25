#pragma once

#include "core/network/Ids.h"
#include "utilities/Time.h"
#include "utilities/Types.h"

#include <optional>
#include <string>
#include <string_view>

namespace tnp::core {

/// Physical medium of a link. Affects the default propagation delay and how the
/// canvas draws the cable.
enum class LinkMedium : u8 {
    Copper,   ///< straight-through / crossover twisted pair
    Fiber,
    Serial,
    Wireless,
    Virtual   ///< logical adjacency, e.g. to a cloud
};

[[nodiscard]] std::string_view linkMediumName(LinkMedium medium);
[[nodiscard]] Duration linkMediumDefaultDelay(LinkMedium medium);

/// One end of a link: always an interface, together with the device owning it.
///
/// The device identifier is stored redundantly so the simulator can route a
/// frame without a reverse index lookup on every hop.
struct LinkEndpoint {
    DeviceId device;
    InterfaceId interface;

    [[nodiscard]] bool isValid() const { return device.isValid() && interface.isValid(); }
    bool operator==(const LinkEndpoint&) const = default;
};

/// A cable between two interfaces.
class Link {
public:
    Link(LinkId id, LinkEndpoint a, LinkEndpoint b, LinkMedium medium);

    [[nodiscard]] LinkId id() const { return id_; }
    [[nodiscard]] const LinkEndpoint& endpointA() const { return a_; }
    [[nodiscard]] const LinkEndpoint& endpointB() const { return b_; }

    [[nodiscard]] LinkMedium medium() const { return medium_; }
    void setMedium(LinkMedium medium) { medium_ = medium; }

    /// One-way signal propagation delay. Serialized so a project reproduces the
    /// same timings on any machine.
    [[nodiscard]] Duration propagationDelay() const { return propagationDelay_; }
    void setPropagationDelay(Duration delay) { propagationDelay_ = delay; }

    /// Effective line rate, normally the slower of the two interfaces.
    [[nodiscard]] u64 bandwidthMbps() const { return bandwidthMbps_; }
    void setBandwidthMbps(u64 value) { bandwidthMbps_ = value; }

    /// A disabled link stays in the project but carries no traffic, which is how
    /// users simulate a cable fault.
    [[nodiscard]] bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    [[nodiscard]] const std::string& label() const { return label_; }
    void setLabel(std::string label) { label_ = std::move(label); }

    [[nodiscard]] bool involves(DeviceId device) const;
    [[nodiscard]] bool involves(InterfaceId interface) const;

    /// The endpoint at the far side of `interface`, or nullopt when the link
    /// does not touch that interface.
    [[nodiscard]] std::optional<LinkEndpoint> peerOf(InterfaceId interface) const;
    [[nodiscard]] std::optional<LinkEndpoint> endpointOf(InterfaceId interface) const;

    /// Time to put `bytes` on the wire plus propagation. Zero-bandwidth links
    /// are treated as instantaneous transmission.
    [[nodiscard]] Duration transferTimeFor(std::size_t bytes) const;

private:
    LinkId id_;
    LinkEndpoint a_;
    LinkEndpoint b_;
    LinkMedium medium_ = LinkMedium::Copper;
    Duration propagationDelay_ = Duration::zero();
    u64 bandwidthMbps_ = 1000;
    bool enabled_ = true;
    std::string label_;
};

} // namespace tnp::core
