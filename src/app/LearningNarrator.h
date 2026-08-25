#pragma once

#include "simulation/Simulator.h"

#include <string>
#include <vector>

namespace tnp::app {

/// One thing explained in plain language.
struct LearningStep {
    SimTime time{};
    core::TraceCategory category = core::TraceCategory::Device;
    core::DeviceId device;

    /// Short statement of what just happened.
    std::string headline;

    /// Why it happened and what it means.
    std::string explanation;
};

/// Turns engine events into explanations.
///
/// The narrator consumes the same structured `TraceEvent` stream everything else
/// does and reads its named fields - "target-ip", "ttl", "next-hop" - to build
/// sentences. No explanation text exists anywhere in the engine or the renderer,
/// which is what allows the wording to change, or be translated, without
/// touching a protocol implementation.
class LearningNarrator {
public:
    explicit LearningNarrator(sim::Simulator& simulator);
    ~LearningNarrator();

    LearningNarrator(const LearningNarrator&) = delete;
    LearningNarrator& operator=(const LearningNarrator&) = delete;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return enabled_; }

    [[nodiscard]] const std::vector<LearningStep>& steps() const { return steps_; }
    void clear() { steps_.clear(); }

    /// Steps are dropped past this count so a long run stays bounded.
    void setHistoryLimit(std::size_t limit) { limit_ = limit; }

private:
    void onTrace(const core::TraceEvent& event);
    [[nodiscard]] std::optional<LearningStep> explain(const core::TraceEvent& event) const;

    sim::Simulator& simulator_;
    bool enabled_ = false;
    std::vector<LearningStep> steps_;
    std::size_t limit_ = 500;
    u32 token_ = 0;
};

} // namespace tnp::app
