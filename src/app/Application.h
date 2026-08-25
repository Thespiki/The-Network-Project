#pragma once

#include "app/ApplicationConfig.h"
#include "app/LearningNarrator.h"
#include "app/ProjectFile.h"
#include "app/Selection.h"
#include "app/TopologyExport.h"
#include "cli/DeviceShell.h"
#include "commands/CommandManager.h"
#include "core/devices/DeviceRegistry.h"
#include "core/project/Project.h"
#include "simulation/Simulator.h"
#include "utilities/Logging.h"
#include "testing/NetworkTestRunner.h"
#include "validation/NetworkValidator.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace tnp::app {

/// Everything the application owns, with no user interface attached.
///
/// The window is a client of this class, not the other way round: `Application`
/// holds the project, the undo stack, the simulator, the validator and the
/// console, and the headless tool drives exactly the same object. That is what
/// keeps every feature testable without opening a window.
class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// Installs log sinks, reads preferences and looks for a recoverable session.
    void initialize();

    /// Writes preferences and clears the recovery marker. A shutdown that goes
    /// through here is a *clean* one, which is precisely what the next start-up
    /// checks for.
    void shutdown();

    // --- Document ----------------------------------------------------------
    void newProject();
    [[nodiscard]] Status open(const std::filesystem::path& path);
    [[nodiscard]] Status save();
    [[nodiscard]] Status saveAs(const std::filesystem::path& path);

    [[nodiscard]] bool hasPath() const { return !path_.empty(); }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

    /// "Campus network - TNP" or "Untitled *".
    [[nodiscard]] std::string documentTitle() const;
    [[nodiscard]] bool isDirty() const { return commands_->isDirty(); }

    /// Problems reported by the last load, if any.
    [[nodiscard]] const std::vector<std::string>& loadWarnings() const { return loadWarnings_; }

    // --- Crash recovery ----------------------------------------------------
    /// True when the previous session ended without a clean shutdown and an
    /// autosave is available.
    [[nodiscard]] bool hasRecoverableSession() const { return recoveryAvailable_; }

    /// Path the recoverable session was originally saved to, if it had one.
    [[nodiscard]] const std::string& recoveredFrom() const { return recoveredFrom_; }

    [[nodiscard]] Status recoverSession();
    void discardRecovery();

    // --- Per-frame ---------------------------------------------------------
    /// Advances the simulation and services autosave. `wallDelta` is real time
    /// since the previous call.
    void update(Duration wallDelta);

    // --- Accessors ---------------------------------------------------------
    [[nodiscard]] core::Project& project() { return *project_; }
    [[nodiscard]] const core::Project& project() const { return *project_; }
    [[nodiscard]] commands::CommandManager& commands() { return *commands_; }
    [[nodiscard]] sim::Simulator& simulator() { return *simulator_; }
    [[nodiscard]] Selection& selection() { return selection_; }
    [[nodiscard]] const Selection& selection() const { return selection_; }
    [[nodiscard]] cli::DeviceShell& shell() { return *shell_; }
    [[nodiscard]] LearningNarrator& learning() { return *learning_; }
    [[nodiscard]] ApplicationConfig& config() { return config_; }
    [[nodiscard]] const core::DeviceRegistry& deviceRegistry() const { return registry_; }
    [[nodiscard]] validation::NetworkValidator& validator() { return validator_; }

    /// In-memory log history, for the log panel.
    [[nodiscard]] const std::shared_ptr<logging::RingBufferSink>& logBuffer() const { return logBuffer_; }

    // --- Operations --------------------------------------------------------
    /// Adds a device of `type` at `position` and selects it.
    core::DeviceId addDevice(core::DeviceType type, Vec2 position);

    /// Deletes the selected devices, links and annotations in one undo step.
    void deleteSelection();

    /// Cables two interfaces. The failure message is suitable for a status bar.
    [[nodiscard]] Status connectInterfaces(core::InterfaceId a, core::InterfaceId b);

    /// The current validation report, recomputed only when the model changed.
    [[nodiscard]] const validation::ValidationReport& validationReport();

    /// Marks the cached validation report stale. Called when anything that a
    /// rule might look at changes.
    void invalidateValidation() { validationDirty_ = true; }

    [[nodiscard]] testing::TestRunSummary runTests();

    /// Results of the last test run, for the panel.
    [[nodiscard]] const testing::TestRunSummary& lastTestRun() const { return lastTestRun_; }

    [[nodiscard]] Result<std::string> exportSvg(const SvgExportOptions& options = {}) const;

    void setLearningModeEnabled(bool enabled);

    /// Removes selection entries whose object has gone. Call after undo/redo.
    void pruneSelection();

private:
    void attachDocument(std::filesystem::path path);
    void writeAutosave();
    void clearRecoveryFiles();
    [[nodiscard]] std::filesystem::path autosavePath() const;
    [[nodiscard]] std::filesystem::path recoveryMarkerPath() const;
    void detectRecoverableSession();

    const core::DeviceRegistry& registry_;

    std::unique_ptr<core::Project> project_;
    std::unique_ptr<commands::CommandManager> commands_;
    std::unique_ptr<sim::Simulator> simulator_;
    std::unique_ptr<cli::DeviceShell> shell_;
    std::unique_ptr<LearningNarrator> learning_;

    Selection selection_;
    validation::NetworkValidator validator_;
    validation::ValidationReport validationReport_;
    bool validationDirty_ = true;

    testing::TestRunSummary lastTestRun_;

    ApplicationConfig config_;
    std::shared_ptr<logging::RingBufferSink> logBuffer_;

    std::filesystem::path path_;
    std::vector<std::string> loadWarnings_;

    Duration sinceAutosave_ = Duration::zero();
    bool recoveryAvailable_ = false;
    std::string recoveredFrom_;
};

} // namespace tnp::app
