#pragma once

#include "utilities/Types.h"

#include <chrono>
#include <string>

namespace tnp {

/// Duration inside the simulated world. Nanosecond resolution keeps serialization
/// integral and lets link propagation delays stay exact.
using Duration = std::chrono::nanoseconds;

/// Clock of the simulated world.
///
/// Deliberately distinct from any wall clock: simulation time only advances when
/// the scheduler drains events, never because a frame was rendered.
struct SimClock {
    using rep        = Duration::rep;
    using period     = Duration::period;
    using duration   = Duration;
    using time_point = std::chrono::time_point<SimClock, Duration>;
    static constexpr bool is_steady = true;
};

using SimTime = SimClock::time_point;

[[nodiscard]] constexpr SimTime simTimeZero() { return SimTime{Duration::zero()}; }

[[nodiscard]] constexpr Duration nanoseconds(i64 value) { return Duration{value}; }
[[nodiscard]] constexpr Duration microseconds(i64 value) { return std::chrono::duration_cast<Duration>(std::chrono::microseconds{value}); }
[[nodiscard]] constexpr Duration milliseconds(i64 value) { return std::chrono::duration_cast<Duration>(std::chrono::milliseconds{value}); }
[[nodiscard]] constexpr Duration seconds(i64 value)      { return std::chrono::duration_cast<Duration>(std::chrono::seconds{value}); }

/// Renders a duration with an adaptive unit, e.g. "812 ns", "1.40 ms", "2.5 s".
[[nodiscard]] std::string formatDuration(Duration duration);

/// Renders simulation time as elapsed-since-start, e.g. "00:00:01.250".
[[nodiscard]] std::string formatSimTime(SimTime time);

/// Local wall-clock time as "HH:MM:SS.mmm" for log records.
[[nodiscard]] std::string formatWallClockNow();

/// UTC timestamp in ISO-8601 ("2026-08-24T18:20:05Z"), used in project metadata.
[[nodiscard]] std::string currentTimestampIso8601();

} // namespace tnp
