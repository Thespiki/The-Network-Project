#include "simulation/Simulator.h"

#include "core/devices/Ipv4Stack.h"
#include "simulation/PacketDecoder.h"
#include "utilities/Logging.h"

#include <algorithm>
#include <format>

namespace tnp::sim {
namespace {

using namespace core;

constexpr std::string_view kLogCategory = "sim";

/// A frame a device sends to itself still costs a moment of simulated time, so
/// the loopback stays visible on the timeline instead of collapsing into the
/// instant that produced it.
constexpr Duration kLoopbackDelay = microseconds(1);

/// Ceiling on how many events one `advance()` call may process.
///
/// A broadcast storm or a routing loop can generate work faster than time
/// advances. Without a cap the window would stop responding; with it, the
/// simulation simply falls behind and the user can pause and look.
constexpr std::size_t kMaxEventsPerAdvance = 20000;

/// Log level for a trace event.
///
/// Only failures and application-level results reach the application log; the
/// full stream stays in the trace log, which the Events panel reads. Otherwise a
/// single ping would bury every other message.
logging::Level levelFor(TraceKind kind) {
    switch (kind) {
        case TraceKind::FrameDropped:
        case TraceKind::FrameFilteredByVlan:
        case TraceKind::IpPacketDropped:
        case TraceKind::IpTtlExpired:
        case TraceKind::IpNoRouteToHost:
        case TraceKind::IpChecksumInvalid:
        case TraceKind::IpFragmentationNeeded:
        case TraceKind::ArpTimedOut:
        case TraceKind::UdpPortUnreachable:
        case TraceKind::FirewallDenied:
        case TraceKind::PingTimedOut:
        case TraceKind::DhcpNoAddressAvailable:
        case TraceKind::DnsNameNotFound:
            return logging::Level::Warning;

        case TraceKind::DevicePoweredOn:
        case TraceKind::PingStarted:
        case TraceKind::PingReplyReceived:
        case TraceKind::PingFinished:
        case TraceKind::DhcpLeaseAssigned:
        case TraceKind::DnsNameResolved:
        case TraceKind::IcmpDestinationUnreachableReceived:
        case TraceKind::IcmpTimeExceededReceived:
            return logging::Level::Info;

        default:
            return logging::Level::Trace;
    }
}

} // namespace

Simulator::Simulator(Network& network, SimulationSettings settings)
    : network_(network), settings_(std::move(settings)) {
    packets_.setCapacity(settings_.packetHistoryLimit);
}

Simulator::~Simulator() = default;

void Simulator::applySettings(const SimulationSettings& settings) {
    settings_ = settings;
    packets_.setCapacity(settings_.packetHistoryLimit);
}

// ---------------------------------------------------------------------------
// Run control
// ---------------------------------------------------------------------------

void Simulator::start() {
    if (state_ == SimulationState::Paused) {
        state_ = SimulationState::Running;
        logging::info(kLogCategory, "Simulation resumed at {}", formatSimTime(now()));
        return;
    }
    if (state_ == SimulationState::Running) return;

    reset();
    state_ = SimulationState::Running;
    powerOnAllDevices();
    logging::info(kLogCategory, "Simulation started with {} device(s) and {} link(s)",
              network_.deviceCount(), network_.linkCount());
}

void Simulator::pause() {
    if (state_ != SimulationState::Running) return;
    state_ = SimulationState::Paused;
    logging::info(kLogCategory, "Simulation paused at {}", formatSimTime(now()));
}

void Simulator::resume() {
    if (state_ != SimulationState::Paused) return;
    state_ = SimulationState::Running;
    logging::info(kLogCategory, "Simulation resumed at {}", formatSimTime(now()));
}

void Simulator::stop() {
    if (state_ == SimulationState::Stopped) return;
    state_ = SimulationState::Stopped;

    scheduler_.reset();
    inFlight_.clear();

    for (const auto& device : network_.devices()) device->onReset();
    network_.refreshOperationalStates();

    logging::info(kLogCategory, "Simulation stopped after {} event(s)", statistics_.eventsProcessed);
    logging::Logger::instance().setSimulationTime(std::nullopt);
}

void Simulator::reset() {
    stop();
    scheduler_.reset();
    statistics_.reset();
    packets_.clear();
    inFlight_.clear();
    trace_.clear();
    nextTimerId_ = 0;
    nextTraceSequence_ = 0;
}

void Simulator::powerOnAllDevices() {
    // Interface state has to be correct before any device builds a routing
    // table from it.
    network_.refreshOperationalStates();
    for (const auto& device : network_.devices()) device->onPowerOn(*this);
}

bool Simulator::step() {
    if (state_ == SimulationState::Stopped) start();

    auto event = scheduler_.nextEvent();
    if (!event) return false;

    logging::Logger::instance().setSimulationTime(scheduler_.now());
    dispatch(*event);
    return true;
}

void Simulator::advance(Duration wallClockDelta) {
    if (state_ != SimulationState::Running) return;
    if (wallClockDelta < Duration::zero()) return;

    const double scaled = static_cast<double>(wallClockDelta.count()) * settings_.speedMultiplier;
    Duration simulatedDelta = nanoseconds(static_cast<i64>(scaled));
    if (simulatedDelta > settings_.maximumStepPerFrame) simulatedDelta = settings_.maximumStepPerFrame;

    const SimTime target = scheduler_.now() + simulatedDelta;

    std::size_t processed = 0;
    while (processed < kMaxEventsPerAdvance) {
        auto event = scheduler_.nextDueEvent(target);
        if (!event) break;

        logging::Logger::instance().setSimulationTime(scheduler_.now());
        dispatch(*event);
        ++processed;
    }

    // Only jump the clock forward when nothing was left behind: otherwise the
    // remaining events would appear to have happened in the past.
    if (processed < kMaxEventsPerAdvance) scheduler_.advanceClockTo(target);

    // Retire flights whose arrival was cancelled with the device they targeted.
    const SimTime current = scheduler_.now();
    inFlight_.erase(std::remove_if(inFlight_.begin(), inFlight_.end(),
                                   [current](const PacketInFlight& flight) {
                                       return flight.arrival < current;
                                   }),
                    inFlight_.end());

    logging::Logger::instance().setSimulationTime(current);
}

std::size_t Simulator::runUntilIdle(Duration budget) {
    if (state_ == SimulationState::Stopped) start();

    const SimTime deadline = scheduler_.now() + budget;
    std::size_t processed = 0;

    while (processed < kMaxEventsPerAdvance) {
        auto event = scheduler_.nextDueEvent(deadline);
        if (!event) break;

        logging::Logger::instance().setSimulationTime(scheduler_.now());
        dispatch(*event);
        ++processed;
    }

    scheduler_.advanceClockTo(deadline);
    inFlight_.clear();
    return processed;
}

// ---------------------------------------------------------------------------
// Event dispatch
// ---------------------------------------------------------------------------

void Simulator::dispatch(SimulationEvent& event) {
    ++statistics_.eventsProcessed;

    if (auto* arrival = std::get_if<FrameArrival>(&event.payload)) {
        retireFlight(event.sequence);

        Device* device = network_.findDevice(arrival->device);
        Interface* iface = network_.findInterface(arrival->interface);
        if (device == nullptr || iface == nullptr) return;

        if (!iface->isOperational()) {
            ++statistics_.framesDropped;
            trace(TraceEvent{.kind = TraceKind::FrameDropped,
                             .time = now(),
                             .device = device->id(),
                             .interface = iface->id(),
                             .packet = arrival->frame.id,
                             .summary = std::format("{} is down; frame discarded on arrival",
                                                    iface->name())});
            return;
        }

        deliverFrame(*device, *iface, arrival->frame);
        return;
    }

    if (auto* timer = std::get_if<DeviceTimerExpiry>(&event.payload)) {
        if (Device* device = network_.findDevice(timer->device)) {
            device->onTimer(*this, timer->timer);
        }
        return;
    }

    if (auto* loop = std::get_if<LoopbackDelivery>(&event.payload)) {
        Device* device = network_.findDevice(loop->device);
        Interface* iface = network_.findInterface(loop->interface);
        if (device == nullptr || iface == nullptr) return;
        deliverFrame(*device, *iface, loop->frame);
    }
}

void Simulator::deliverFrame(Device& device, Interface& iface, const Frame& frame) {
    ++statistics_.framesDelivered;
    iface.counters().framesReceived += 1;
    iface.counters().bytesReceived += frame.size();

    packets_.observe(frame);
    packets_.addHop(frame.id, PacketHop{now(), device.id(), iface.id(), "received",
                                        std::format("{} bytes on {}", frame.size(), iface.name())});

    trace(TraceEvent{.kind = TraceKind::FrameReceived,
                     .time = now(),
                     .device = device.id(),
                     .interface = iface.id(),
                     .packet = frame.id,
                     .summary = std::format("{} received {} bytes on {}", device.name(), frame.size(),
                                            iface.name())}
              .with("bytes", std::to_string(frame.size()))
              .with("interface", iface.name()));

    device.onFrameReceived(*this, iface, frame);
}

void Simulator::retireFlight(u64 arrivalEvent) {
    const auto it = std::find_if(inFlight_.begin(), inFlight_.end(),
                                 [arrivalEvent](const PacketInFlight& flight) {
                                     return flight.arrivalEvent == arrivalEvent;
                                 });
    if (it != inFlight_.end()) inFlight_.erase(it);
}

// ---------------------------------------------------------------------------
// core::DeviceContext
// ---------------------------------------------------------------------------

Frame Simulator::makeFrame(const Device& origin, ByteBuffer bytes, FrameCategory category,
                           std::string summary) {
    Frame frame;
    frame.id = PacketId::generate();
    frame.origin = origin.id();
    frame.createdAt = now();
    frame.hopCount = 0;
    frame.category = category == FrameCategory::Unknown ? classifyFrame(bytes) : category;
    frame.summary = summary.empty() ? describeFrame(bytes) : std::move(summary);
    frame.bytes = std::move(bytes);

    ++statistics_.packetsCreated;
    packets_.observe(frame);
    packets_.addHop(frame.id, PacketHop{now(), origin.id(), InterfaceId{}, "created", frame.summary});

    return frame;
}

Frame Simulator::makeForwardedFrame(FrameIdentity identity, ByteBuffer bytes, FrameCategory category,
                                    std::string summary) {
    Frame frame;
    frame.id = identity.id;
    frame.origin = identity.origin;
    frame.createdAt = identity.createdAt;
    frame.hopCount = identity.hopCount;
    frame.category = category;
    frame.summary = std::move(summary);
    frame.bytes = std::move(bytes);
    return frame;
}

void Simulator::dropFrame(const Device& sender, const Interface& out, const Frame& frame,
                          std::string_view reason) {
    ++statistics_.framesDropped;
    trace(TraceEvent{.kind = TraceKind::FrameDropped,
                     .time = now(),
                     .device = sender.id(),
                     .interface = out.id(),
                     .packet = frame.id,
                     .summary = std::format("{} could not send on {}: {}", sender.name(), out.name(),
                                            reason)}
              .with("reason", std::string{reason})
              .with("interface", out.name()));
}

void Simulator::transmit(Device& sender, Interface& out, Frame frame) {
    if (!out.isAdminUp()) {
        dropFrame(sender, out, frame, "the interface is administratively down");
        return;
    }

    Link* link = network_.linkOfInterface(out.id());
    if (link == nullptr) {
        dropFrame(sender, out, frame, "nothing is connected to this interface");
        return;
    }
    if (!link->isEnabled()) {
        dropFrame(sender, out, frame, "the link is disabled");
        return;
    }

    const auto peer = link->peerOf(out.id());
    Interface* peerInterface = peer ? network_.findInterface(peer->interface) : nullptr;
    if (peerInterface == nullptr) {
        dropFrame(sender, out, frame, "the link has no peer interface");
        return;
    }
    if (!peerInterface->isAdminUp()) {
        dropFrame(sender, out, frame, "the interface at the far end is down");
        return;
    }

    ++statistics_.framesTransmitted;
    out.counters().framesSent += 1;
    out.counters().bytesSent += frame.size();

    const Duration travel = link->transferTimeFor(frame.size());
    const SimTime arrival = now() + travel;

    packets_.observe(frame);
    packets_.addHop(frame.id, PacketHop{now(), sender.id(), out.id(), "transmitted",
                                        std::format("{} bytes, arriving in {}", frame.size(),
                                                    formatDuration(travel))});

    trace(TraceEvent{.kind = TraceKind::FrameTransmitted,
                     .time = now(),
                     .device = sender.id(),
                     .interface = out.id(),
                     .packet = frame.id,
                     .summary = std::format("{} sent {} bytes out {} ({})", sender.name(), frame.size(),
                                            out.name(), frame.summary)}
              .with("bytes", std::to_string(frame.size()))
              .with("interface", out.name())
              .with("travel-time", formatDuration(travel)));

    PacketInFlight flight;
    flight.packet = frame.id;
    flight.link = link->id();
    flight.fromDevice = sender.id();
    flight.toDevice = peer->device;
    flight.fromInterface = out.id();
    flight.toInterface = peer->interface;
    flight.departure = now();
    flight.arrival = arrival;
    flight.category = frame.category;
    flight.sizeBytes = frame.size();

    FrameArrival event{link->id(), peer->device, peer->interface, std::move(frame)};
    flight.arrivalEvent = scheduler_.scheduleAt(arrival, std::move(event));

    inFlight_.push_back(std::move(flight));
}

void Simulator::loopback(Device& device, Interface& iface, Frame frame) {
    packets_.observe(frame);
    packets_.addHop(frame.id, PacketHop{now(), device.id(), iface.id(), "looped back",
                                        "addressed to this device"});

    scheduler_.schedule(kLoopbackDelay, LoopbackDelivery{device.id(), iface.id(), std::move(frame)});
}

void Simulator::scheduleTimer(Device& device, TimerId timer, Duration delay) {
    scheduler_.schedule(delay, DeviceTimerExpiry{device.id(), timer});
}

void Simulator::cancelTimer(Device&, TimerId timer) { scheduler_.cancelTimer(timer); }

void Simulator::trace(TraceEvent event) {
    event.sequence = ++nextTraceSequence_;
    if (event.time == SimTime{}) event.time = now();

    ++statistics_.traceEvents;

    for (const auto& entry : observers_) entry.observer(event);
    forwardTraceToLog(event);

    trace_.push_back(std::move(event));
    if (trace_.size() > settings_.traceHistoryLimit) {
        // Drop the oldest quarter at once: erasing from the front of a vector one
        // element at a time would be quadratic over a long run.
        const std::size_t drop = trace_.size() / 4;
        trace_.erase(trace_.begin(), trace_.begin() + static_cast<std::ptrdiff_t>(drop));
    }
}

void Simulator::forwardTraceToLog(const TraceEvent& event) {
    const logging::Level level = levelFor(event.kind);
    if (level == logging::Level::Trace) return;

    const std::string device = deviceName(event.device);
    logging::message(level, "net", device.empty() ? event.summary
                                              : std::format("{}: {}", device, event.summary));
}

std::string Simulator::deviceName(DeviceId device) const {
    const Device* found = network_.findDevice(device);
    return found == nullptr ? std::string{} : found->name();
}

// ---------------------------------------------------------------------------
// Observation
// ---------------------------------------------------------------------------

void Simulator::clearTrace() {
    trace_.clear();
    nextTraceSequence_ = 0;
}

u32 Simulator::addTraceObserver(TraceObserver observer) {
    if (!observer) return 0;
    const u32 token = nextObserverToken_++;
    observers_.push_back(ObserverEntry{token, std::move(observer)});
    return token;
}

void Simulator::removeTraceObserver(u32 token) {
    const auto it = std::find_if(observers_.begin(), observers_.end(),
                                 [token](const ObserverEntry& entry) { return entry.token == token; });
    if (it != observers_.end()) observers_.erase(it);
}

// ---------------------------------------------------------------------------
// Application entry points
// ---------------------------------------------------------------------------

Result<PingId> Simulator::ping(DeviceId device, const PingRequest& request) {
    Device* source = network_.findDevice(device);
    if (source == nullptr) return Result<PingId>::failure("device not found");

    Ipv4Stack* stack = source->ipv4Stack();
    if (stack == nullptr) {
        return Result<PingId>::failure(std::format("{} has no IPv4 stack and cannot send a ping",
                                                   source->name()));
    }

    if (state_ == SimulationState::Stopped) start();
    return stack->startPing(*this, request);
}

} // namespace tnp::sim
