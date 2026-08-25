#pragma once

#include "core/devices/Ipv4Stack.h"
#include "core/network/DeviceContext.h"
#include "core/network/Network.h"
#include "core/project/SimulationSettings.h"
#include "simulation/NetworkScheduler.h"
#include "simulation/Packet.h"
#include "simulation/SimulationState.h"
#include "utilities/Result.h"

#include <functional>
#include <string>
#include <vector>

namespace tnp::sim {

/// The simulation engine.
///
/// Owns the clock, the event queue, the packet history and the trace log, and
/// implements `core::DeviceContext` so devices can send frames and set timers
/// without knowing anything about the topology or about time.
///
/// It has no dependency on the UI. The renderer reads `packetsInFlight()` and
/// interpolates; the log panel reads `traceLog()`; the headless tool drives the
/// same engine through `runUntilIdle()`. Nothing about a run depends on whether
/// a window is open.
class Simulator final : public core::DeviceContext {
public:
    Simulator(core::Network& network, core::SimulationSettings settings);
    ~Simulator() override;

    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;

    // --- Configuration -----------------------------------------------------
    void applySettings(const core::SimulationSettings& settings);
    [[nodiscard]] const core::SimulationSettings& settings() const { return settings_; }

    // --- Run control -------------------------------------------------------
    /// Starts a run. From `Stopped` this resets every device and powers the
    /// network on; from `Paused` it simply resumes.
    void start();
    void pause();
    void resume();

    /// Ends the run and clears volatile state, leaving configuration untouched.
    void stop();

    /// Equivalent to `stop()` followed by clearing the trace and packet history.
    void reset();

    /// Processes exactly one event, whatever its timestamp. Works while paused,
    /// which is what makes stepping through an exchange possible.
    bool step();

    /// Advances by `wallClockDelta` scaled by the speed multiplier. Called once
    /// per rendered frame.
    void advance(Duration wallClockDelta);

    /// Drains the queue until it is empty or `budget` of simulated time passes.
    /// Returns the number of events processed. Used by tests and the headless
    /// tool, where there is no frame loop.
    std::size_t runUntilIdle(Duration budget);

    [[nodiscard]] SimulationState state() const { return state_; }
    [[nodiscard]] bool isRunning() const { return state_ == SimulationState::Running; }
    [[nodiscard]] bool isActive() const { return state_ != SimulationState::Stopped; }

    [[nodiscard]] const SimulationStatistics& statistics() const { return statistics_; }
    [[nodiscard]] std::size_t pendingEventCount() const { return scheduler_.pendingEventCount(); }

    // --- Observation -------------------------------------------------------
    [[nodiscard]] const std::vector<core::TraceEvent>& traceLog() const { return trace_; }
    void clearTrace();

    /// Called for every trace event as it is produced. Used by the test runner
    /// and by the learning narrator, neither of which polls.
    using TraceObserver = std::function<void(const core::TraceEvent&)>;
    u32 addTraceObserver(TraceObserver observer);
    void removeTraceObserver(u32 token);

    [[nodiscard]] PacketRegistry& packets() { return packets_; }
    [[nodiscard]] const PacketRegistry& packets() const { return packets_; }

    /// Packets currently travelling a wire, for the canvas animation.
    [[nodiscard]] const std::vector<PacketInFlight>& packetsInFlight() const { return inFlight_; }

    // --- Application entry points ------------------------------------------
    /// Starts a ping from `device`. Starts the run first when the engine is
    /// stopped, so a user can just type `ping` without pressing play.
    [[nodiscard]] Result<core::PingId> ping(core::DeviceId device, const core::PingRequest& request);

    // --- core::DeviceContext ------------------------------------------------
    [[nodiscard]] SimTime now() const override { return scheduler_.now(); }

    [[nodiscard]] core::Frame makeFrame(const core::Device& origin, ByteBuffer bytes,
                                        core::FrameCategory category, std::string summary) override;
    [[nodiscard]] core::Frame makeForwardedFrame(core::FrameIdentity identity, ByteBuffer bytes,
                                                 core::FrameCategory category,
                                                 std::string summary) override;

    void transmit(core::Device& sender, core::Interface& out, core::Frame frame) override;
    void loopback(core::Device& device, core::Interface& iface, core::Frame frame) override;

    void scheduleTimer(core::Device& device, core::TimerId timer, Duration delay) override;
    void cancelTimer(core::Device& device, core::TimerId timer) override;

    void trace(core::TraceEvent event) override;

    [[nodiscard]] std::string deviceName(core::DeviceId device) const override;
    [[nodiscard]] core::TimerId nextTimerId() override { return ++nextTimerId_; }

private:
    void powerOnAllDevices();
    void dispatch(SimulationEvent& event);
    void deliverFrame(core::Device& device, core::Interface& iface, const core::Frame& frame);
    void retireFlight(u64 arrivalEvent);
    void dropFrame(const core::Device& sender, const core::Interface& out, const core::Frame& frame,
                   std::string_view reason);
    void forwardTraceToLog(const core::TraceEvent& event);

    core::Network& network_;
    core::SimulationSettings settings_;

    NetworkScheduler scheduler_;
    SimulationState state_ = SimulationState::Stopped;
    SimulationStatistics statistics_;

    PacketRegistry packets_;
    std::vector<PacketInFlight> inFlight_;
    std::vector<core::TraceEvent> trace_;

    struct ObserverEntry {
        u32 token = 0;
        TraceObserver observer;
    };
    std::vector<ObserverEntry> observers_;
    u32 nextObserverToken_ = 1;

    core::TimerId nextTimerId_ = 0;
    u64 nextTraceSequence_ = 0;
};

} // namespace tnp::sim
