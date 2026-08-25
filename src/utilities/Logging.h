#pragma once

#include "utilities/Time.h"
#include "utilities/Types.h"

#include <deque>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tnp::logging {

enum class Level : u8 { Trace = 0, Debug, Info, Warning, Error };

[[nodiscard]] std::string_view levelName(Level level);
[[nodiscard]] std::optional<Level> levelFromName(std::string_view name);

/// One log line. Records carry the simulation time when they were produced
/// during a run, which is what makes the log panel correlate with the packet
/// timeline.
struct Record {
    Level level = Level::Info;
    std::string category;
    std::string message;
    std::string wallClock;
    std::optional<SimTime> simTime;
    u64 sequence = 0;
};

/// Destination for log records. Implementations must be safe to call from the
/// thread that emits records (TNP logs from the main thread and from autosave).
class Sink {
public:
    virtual ~Sink() = default;
    virtual void write(const Record& record) = 0;
};

/// Bounded in-memory history that backs the Log panel.
class RingBufferSink final : public Sink {
public:
    explicit RingBufferSink(std::size_t capacity = 5000) : capacity_(capacity) {}

    void write(const Record& record) override;

    /// Copy of the current history, oldest first.
    [[nodiscard]] std::vector<Record> snapshot() const;
    [[nodiscard]] std::size_t size() const;
    void clear();

private:
    mutable std::mutex mutex_;
    std::deque<Record> records_;
    std::size_t capacity_;
};

/// Writes to stderr. Used by the headless tool and during start-up, before the
/// UI exists.
class ConsoleSink final : public Sink {
public:
    explicit ConsoleSink(bool useColor = true) : useColor_(useColor) {}
    void write(const Record& record) override;

private:
    bool useColor_;
};

/// Appends to a log file. Failure to open is silent by design: losing the log
/// file must never prevent the application from starting.
class FileSink final : public Sink {
public:
    explicit FileSink(std::string path);
    ~FileSink() override;

    FileSink(const FileSink&) = delete;
    FileSink& operator=(const FileSink&) = delete;

    void write(const Record& record) override;
    [[nodiscard]] bool isOpen() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// Central log dispatcher.
///
/// This is the one piece of global mutable state in TNP. It is justified: every
/// layer needs to report diagnostics, and threading a logger reference through
/// address parsing or checksum code would be pure noise. The instance is
/// mutex-protected and sinks are owned by shared pointers so the UI can hold on
/// to the ring buffer.
class Logger {
public:
    [[nodiscard]] static Logger& instance();

    void addSink(std::shared_ptr<Sink> sink);
    void removeAllSinks();

    void setMinimumLevel(Level level);
    [[nodiscard]] Level minimumLevel() const;

    /// Stamps subsequent records with simulation time. Cleared with `std::nullopt`
    /// when no simulation is running.
    void setSimulationTime(std::optional<SimTime> time);

    void emit(Level level, std::string_view category, std::string message);

private:
    Logger() = default;

    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Sink>> sinks_;
    Level minimumLevel_ = Level::Debug;
    std::optional<SimTime> simTime_;
    u64 sequence_ = 0;
};

/// Emits an already-formatted message. Use this for runtime-built strings.
inline void message(Level level, std::string_view category, std::string text) {
    Logger::instance().emit(level, category, std::move(text));
}

// Formatting front-end. `std::format` gives compile-time checked format strings.
template <typename... Args>
void trace(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    Logger::instance().emit(Level::Trace, category, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void debug(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    Logger::instance().emit(Level::Debug, category, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void info(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    Logger::instance().emit(Level::Info, category, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void warning(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    Logger::instance().emit(Level::Warning, category, std::format(fmt, std::forward<Args>(args)...));
}
template <typename... Args>
void error(std::string_view category, std::format_string<Args...> fmt, Args&&... args) {
    Logger::instance().emit(Level::Error, category, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace tnp::logging
