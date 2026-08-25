#pragma once

#include "utilities/Result.h"
#include "utilities/Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace tnp::app {

/// User preferences, stored outside any project.
///
/// Kept separate from `SimulationSettings`: those belong to the project and
/// travel with it, these describe this machine and this person.
struct ApplicationConfig {
    // Window
    int windowWidth = 1600;
    int windowHeight = 950;
    bool maximized = false;
    float uiScale = 1.0f;

    // Behaviour
    bool autosaveEnabled = true;
    int autosaveIntervalSeconds = 120;
    bool showGrid = true;
    bool snapToGrid = false;
    bool learningModeEnabled = false;

    /// Minimum level shown in the log panel, by name ("TRACE".."ERROR").
    std::string logLevel = "DEBUG";

    /// Most recently opened projects, newest first.
    std::vector<std::string> recentFiles;
    static constexpr std::size_t kMaxRecentFiles = 10;

    /// Path of the configuration file for this user.
    [[nodiscard]] static std::filesystem::path configPath();

    /// Reads the configuration, falling back to defaults when it is missing or
    /// unreadable. Preferences are never important enough to block start-up.
    [[nodiscard]] static ApplicationConfig load();

    [[nodiscard]] Status save() const;

    void addRecentFile(const std::filesystem::path& path);
    void removeRecentFile(const std::filesystem::path& path);
};

} // namespace tnp::app
