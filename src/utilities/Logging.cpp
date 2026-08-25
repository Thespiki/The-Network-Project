#include "utilities/Logging.h"

#include <array>
#include <fstream>
#include <iostream>
#include <utility>

namespace tnp::logging {
namespace {

constexpr std::array<std::string_view, 5> kLevelNames = {"TRACE", "DEBUG", "INFO", "WARNING", "ERROR"};

/// ANSI colours: grey, cyan, default, yellow, red.
constexpr std::array<std::string_view, 5> kLevelColors = {
    "\033[90m", "\033[36m", "\033[0m", "\033[33m", "\033[31m"};

std::string formatLine(const Record& record) {
    std::string line = "[" + record.wallClock + "] ";
    line += std::format("{:<7} ", levelName(record.level));
    if (record.simTime) line += "@" + formatSimTime(*record.simTime) + " ";
    if (!record.category.empty()) line += "[" + record.category + "] ";
    line += record.message;
    return line;
}

} // namespace

std::string_view levelName(Level level) {
    const auto index = static_cast<std::size_t>(level);
    return index < kLevelNames.size() ? kLevelNames[index] : std::string_view{"?"};
}

std::optional<Level> levelFromName(std::string_view name) {
    for (std::size_t i = 0; i < kLevelNames.size(); ++i) {
        if (kLevelNames[i].size() != name.size()) continue;
        bool equal = true;
        for (std::size_t c = 0; c < name.size(); ++c) {
            const char a = static_cast<char>(std::toupper(static_cast<unsigned char>(name[c])));
            if (a != kLevelNames[i][c]) { equal = false; break; }
        }
        if (equal) return static_cast<Level>(i);
    }
    return std::nullopt;
}

// --- RingBufferSink --------------------------------------------------------

void RingBufferSink::write(const Record& record) {
    const std::lock_guard lock{mutex_};
    records_.push_back(record);
    while (records_.size() > capacity_) records_.pop_front();
}

std::vector<Record> RingBufferSink::snapshot() const {
    const std::lock_guard lock{mutex_};
    return {records_.begin(), records_.end()};
}

std::size_t RingBufferSink::size() const {
    const std::lock_guard lock{mutex_};
    return records_.size();
}

void RingBufferSink::clear() {
    const std::lock_guard lock{mutex_};
    records_.clear();
}

// --- ConsoleSink -----------------------------------------------------------

void ConsoleSink::write(const Record& record) {
    const auto index = static_cast<std::size_t>(record.level);
    if (useColor_ && index < kLevelColors.size()) {
        std::cerr << kLevelColors[index] << formatLine(record) << "\033[0m" << '\n';
    } else {
        std::cerr << formatLine(record) << '\n';
    }
}

// --- FileSink --------------------------------------------------------------

struct FileSink::Impl {
    std::ofstream stream;
    std::mutex mutex;
};

FileSink::FileSink(std::string path) : impl_(std::make_unique<Impl>()) {
    impl_->stream.open(path, std::ios::out | std::ios::app);
}

FileSink::~FileSink() = default;

bool FileSink::isOpen() const { return impl_->stream.is_open(); }

void FileSink::write(const Record& record) {
    const std::lock_guard lock{impl_->mutex};
    if (!impl_->stream.is_open()) return;
    impl_->stream << formatLine(record) << '\n';
    impl_->stream.flush();
}

// --- Logger ----------------------------------------------------------------

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::addSink(std::shared_ptr<Sink> sink) {
    if (!sink) return;
    const std::lock_guard lock{mutex_};
    sinks_.push_back(std::move(sink));
}

void Logger::removeAllSinks() {
    const std::lock_guard lock{mutex_};
    sinks_.clear();
}

void Logger::setMinimumLevel(Level level) {
    const std::lock_guard lock{mutex_};
    minimumLevel_ = level;
}

Level Logger::minimumLevel() const {
    const std::lock_guard lock{mutex_};
    return minimumLevel_;
}

void Logger::setSimulationTime(std::optional<SimTime> time) {
    const std::lock_guard lock{mutex_};
    simTime_ = time;
}

void Logger::emit(Level level, std::string_view category, std::string message) {
    std::vector<std::shared_ptr<Sink>> sinks;
    Record record;
    {
        const std::lock_guard lock{mutex_};
        if (level < minimumLevel_) return;

        record.level     = level;
        record.category  = std::string{category};
        record.message   = std::move(message);
        record.wallClock = formatWallClockNow();
        record.simTime   = simTime_;
        record.sequence  = ++sequence_;
        sinks = sinks_; // copied so sinks may log without deadlocking
    }
    for (const auto& sink : sinks) sink->write(record);
}

} // namespace tnp::logging
