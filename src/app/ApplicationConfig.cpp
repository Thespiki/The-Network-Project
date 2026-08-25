#include "app/ApplicationConfig.h"

#include "utilities/FileSystem.h"
#include "utilities/Logging.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace tnp::app {
namespace {

using Json = nlohmann::json;

template <typename T>
T value(const Json& node, const char* key, T fallback) {
    if (!node.is_object() || !node.contains(key)) return fallback;
    try {
        return node.at(key).get<T>();
    } catch (const std::exception&) {
        return fallback;
    }
}

} // namespace

std::filesystem::path ApplicationConfig::configPath() {
    return files::userConfigDirectory() / "config.json";
}

ApplicationConfig ApplicationConfig::load() {
    ApplicationConfig config;

    const auto text = files::readTextFile(configPath());
    if (!text) return config; // first run

    Json root;
    try {
        root = Json::parse(text.value());
    } catch (const std::exception& error) {
        logging::warning("config", "preferences could not be read ({}); defaults were used", error.what());
        return config;
    }

    config.windowWidth = value(root, "windowWidth", config.windowWidth);
    config.windowHeight = value(root, "windowHeight", config.windowHeight);
    config.maximized = value(root, "maximized", config.maximized);
    config.uiScale = value(root, "uiScale", config.uiScale);
    config.autosaveEnabled = value(root, "autosaveEnabled", config.autosaveEnabled);
    config.autosaveIntervalSeconds = value(root, "autosaveIntervalSeconds", config.autosaveIntervalSeconds);
    config.showGrid = value(root, "showGrid", config.showGrid);
    config.snapToGrid = value(root, "snapToGrid", config.snapToGrid);
    config.learningModeEnabled = value(root, "learningModeEnabled", config.learningModeEnabled);
    config.logLevel = value(root, "logLevel", config.logLevel);

    if (root.contains("recentFiles") && root.at("recentFiles").is_array()) {
        for (const Json& entry : root.at("recentFiles")) {
            if (entry.is_string()) config.recentFiles.push_back(entry.get<std::string>());
        }
    }

    // Guard against a hand-edited file producing an unusable window.
    config.windowWidth = std::clamp(config.windowWidth, 640, 16384);
    config.windowHeight = std::clamp(config.windowHeight, 480, 16384);
    config.uiScale = std::clamp(config.uiScale, 0.5f, 3.0f);
    config.autosaveIntervalSeconds = std::clamp(config.autosaveIntervalSeconds, 15, 3600);

    return config;
}

Status ApplicationConfig::save() const {
    Json root;
    root["windowWidth"] = windowWidth;
    root["windowHeight"] = windowHeight;
    root["maximized"] = maximized;
    root["uiScale"] = uiScale;
    root["autosaveEnabled"] = autosaveEnabled;
    root["autosaveIntervalSeconds"] = autosaveIntervalSeconds;
    root["showGrid"] = showGrid;
    root["snapToGrid"] = snapToGrid;
    root["learningModeEnabled"] = learningModeEnabled;
    root["logLevel"] = logLevel;
    root["recentFiles"] = recentFiles;

    return files::writeTextFileAtomic(configPath(), root.dump(2));
}

void ApplicationConfig::addRecentFile(const std::filesystem::path& path) {
    const std::string text = path.string();
    removeRecentFile(path);
    recentFiles.insert(recentFiles.begin(), text);
    if (recentFiles.size() > kMaxRecentFiles) recentFiles.resize(kMaxRecentFiles);
}

void ApplicationConfig::removeRecentFile(const std::filesystem::path& path) {
    const std::string text = path.string();
    recentFiles.erase(std::remove(recentFiles.begin(), recentFiles.end(), text), recentFiles.end());
}

} // namespace tnp::app
