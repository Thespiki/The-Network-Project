#include "utilities/Time.h"

#include <chrono>
#include <ctime>
#include <format>

namespace tnp {
namespace {

constexpr i64 kNsPerUs = 1'000;
constexpr i64 kNsPerMs = 1'000'000;
constexpr i64 kNsPerS  = 1'000'000'000;

} // namespace

std::string formatDuration(Duration duration) {
    const i64 ns = duration.count();
    const i64 magnitude = ns < 0 ? -ns : ns;

    if (magnitude < kNsPerUs) return std::format("{} ns", ns);
    if (magnitude < kNsPerMs) return std::format("{:.2f} us", static_cast<double>(ns) / kNsPerUs);
    if (magnitude < kNsPerS)  return std::format("{:.2f} ms", static_cast<double>(ns) / kNsPerMs);
    return std::format("{:.3f} s", static_cast<double>(ns) / kNsPerS);
}

std::string formatSimTime(SimTime time) {
    // Microsecond resolution: a complete ping across a small topology takes tens
    // of microseconds, so millisecond stamps would show every step at 0.000.
    const i64 totalNs = time.time_since_epoch().count();
    const i64 totalUs = totalNs / kNsPerUs;
    const i64 hours   = totalUs / 3'600'000'000;
    const i64 minutes = (totalUs / 60'000'000) % 60;
    const i64 secs    = (totalUs / 1'000'000) % 60;
    const i64 micros  = totalUs % 1'000'000;
    return std::format("{:02}:{:02}:{:02}.{:06}", hours, minutes, secs, micros);
}

std::string formatWallClockNow() {
    using namespace std::chrono;
    const auto now     = system_clock::now();
    const auto epoch   = now.time_since_epoch();
    const auto millis  = duration_cast<std::chrono::milliseconds>(epoch).count() % 1'000;
    const std::time_t seconds = system_clock::to_time_t(now);

    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    return std::format("{:02}:{:02}:{:02}.{:03}", local.tm_hour, local.tm_min, local.tm_sec, millis);
}

std::string currentTimestampIso8601() {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
                       utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                       utc.tm_hour, utc.tm_min, utc.tm_sec);
}

} // namespace tnp
