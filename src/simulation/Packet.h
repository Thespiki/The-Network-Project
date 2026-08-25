#pragma once

#include "core/network/Frame.h"
#include "core/network/Ids.h"
#include "utilities/Time.h"
#include "utilities/Types.h"

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace tnp::sim {

/// One step in a packet's journey.
struct PacketHop {
    SimTime time{};
    core::DeviceId device;
    core::InterfaceId interface;
    /// What happened here: "transmitted", "received", "forwarded", "dropped".
    std::string action;
    /// Free-form context, e.g. "TTL 64 -> 63".
    std::string detail;
};

/// The history of one packet, as the inspector shows it.
///
/// A packet keeps its identity across hops, so a single record accumulates the
/// whole path even though the bytes are re-encapsulated at every router.
struct PacketRecord {
    core::PacketId id;
    core::DeviceId origin;
    core::FrameCategory category = core::FrameCategory::Unknown;
    std::string summary;
    SimTime createdAt{};

    /// Most recent encapsulation observed. This is what the byte view and the
    /// protocol decoder work on.
    ByteBuffer bytes;

    std::vector<PacketHop> hops;

    [[nodiscard]] std::size_t size() const { return bytes.size(); }
    [[nodiscard]] SimTime lastSeen() const { return hops.empty() ? createdAt : hops.back().time; }
};

/// A packet currently travelling a wire.
///
/// The canvas animates from this: it holds the two endpoints and the departure
/// and arrival instants, and the renderer interpolates. The simulator never
/// computes a screen position, and the renderer never invents a packet.
struct PacketInFlight {
    core::PacketId packet;
    core::LinkId link;
    core::DeviceId fromDevice;
    core::DeviceId toDevice;
    core::InterfaceId fromInterface;
    core::InterfaceId toInterface;
    SimTime departure{};
    SimTime arrival{};
    core::FrameCategory category = core::FrameCategory::Unknown;
    std::size_t sizeBytes = 0;

    /// Sequence number of the arrival event this flight will end with. Used to
    /// retire the entry when the frame lands.
    u64 arrivalEvent = 0;

    /// Position along the link at `time`, from 0 (sender) to 1 (receiver).
    [[nodiscard]] float progressAt(SimTime time) const;
};

/// Bounded store of packet histories.
///
/// Oldest records are discarded once the capacity is reached, so a long run has
/// a bounded memory cost while the recent traffic a user actually inspects is
/// always available.
class PacketRegistry {
public:
    /// Records a newly created packet, or refreshes the bytes of one already
    /// known. Returns the record.
    PacketRecord& observe(const core::Frame& frame);

    void addHop(core::PacketId packet, PacketHop hop);

    [[nodiscard]] const PacketRecord* find(core::PacketId packet) const;

    /// Identifiers from oldest to newest.
    [[nodiscard]] const std::deque<core::PacketId>& order() const { return order_; }
    [[nodiscard]] std::size_t size() const { return records_.size(); }

    void setCapacity(std::size_t capacity);
    [[nodiscard]] std::size_t capacity() const { return capacity_; }

    void clear();

private:
    void trim();

    std::map<core::PacketId, PacketRecord> records_;
    std::deque<core::PacketId> order_;
    std::size_t capacity_ = 2000;
};

} // namespace tnp::sim
