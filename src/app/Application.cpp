#include "app/Application.h"

#include "commands/DeviceCommands.h"
#include "commands/LinkCommands.h"
#include "commands/ProjectCommands.h"
#include "utilities/FileSystem.h"

#include <nlohmann/json.hpp>

#include <format>

namespace tnp::app {
namespace {

using namespace core;

constexpr std::string_view kLogCategory = "app";

/// Version string written into saved projects, for diagnostics.
std::string writerIdentity() { return std::format("TNP {}", TNP_VERSION_STRING); }

} // namespace

Application::Application()
    : registry_(builtinDeviceRegistry()),
      project_(std::make_unique<Project>()),
      commands_(std::make_unique<commands::CommandManager>(*project_)),
      simulator_(std::make_unique<sim::Simulator>(project_->network(), project_->simulationSettings())),
      shell_(std::make_unique<cli::DeviceShell>(*project_, *simulator_, *commands_)),
      learning_(std::make_unique<LearningNarrator>(*simulator_)) {
    commands_->setChangeCallback([this] {
        validationDirty_ = true;
        pruneSelection();
    });
}

Application::~Application() = default;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Application::initialize() {
    logBuffer_ = std::make_shared<logging::RingBufferSink>(5000);
    logging::Logger::instance().addSink(logBuffer_);

    config_ = ApplicationConfig::load();
    if (const auto level = logging::levelFromName(config_.logLevel)) {
        logging::Logger::instance().setMinimumLevel(*level);
    }

    project_->layout().gridVisible = config_.showGrid;
    project_->layout().snapToGrid = config_.snapToGrid;
    setLearningModeEnabled(config_.learningModeEnabled);

    detectRecoverableSession();

    logging::info(kLogCategory, "TNP {} started", TNP_VERSION_STRING);
    if (recoveryAvailable_) {
        logging::warning(kLogCategory, "an unsaved session from a previous run is available to recover");
    }
}

void Application::shutdown() {
    config_.showGrid = project_->layout().gridVisible;
    config_.snapToGrid = project_->layout().snapToGrid;
    config_.learningModeEnabled = learning_->isEnabled();

    if (const Status status = config_.save(); !status) {
        logging::warning(kLogCategory, "preferences could not be saved: {}", status.message());
    }

    // A shutdown that reaches this point is clean, so nothing is left to recover.
    clearRecoveryFiles();
    logging::info(kLogCategory, "TNP closed");
}

// ---------------------------------------------------------------------------
// Document
// ---------------------------------------------------------------------------

void Application::newProject() {
    simulator_->reset();
    project_->reset();
    project_->metadata().writtenBy = writerIdentity();

    commands_->clear();
    selection_.clear();
    learning_->clear();
    loadWarnings_.clear();
    lastTestRun_ = testing::TestRunSummary{};
    validationDirty_ = true;

    attachDocument({});
    logging::info(kLogCategory, "New project created");
}

Status Application::open(const std::filesystem::path& target) {
    // Load into the live project only after the file has been read and parsed:
    // a failed open must leave the user's current work alone.
    simulator_->reset();

    auto report = loadProject(target, *project_);
    if (!report) {
        logging::error(kLogCategory, "could not open {}: {}", target.string(), report.message());
        return report.status();
    }

    loadWarnings_ = report.value().warnings;
    if (report.value().writtenByNewerBuild) {
        loadWarnings_.push_back(std::format(
            "This project was written by a newer build of TNP (format {}). Anything this build does "
            "not understand was dropped; saving will not preserve it.",
            report.value().fileVersion.toString()));
    }

    commands_->clear();
    selection_.clear();
    learning_->clear();
    lastTestRun_ = testing::TestRunSummary{};
    validationDirty_ = true;

    attachDocument(target);
    config_.addRecentFile(target);

    logging::info(kLogCategory, "Opened {} ({} devices, {} links)", target.filename().string(),
              project_->network().deviceCount(), project_->network().linkCount());
    for (const std::string& warning : loadWarnings_) {
        logging::warning(kLogCategory, "{}", warning);
    }
    return Status::ok();
}

Status Application::save() {
    if (path_.empty()) return Status::failure("this project has never been saved");
    return saveAs(path_);
}

Status Application::saveAs(const std::filesystem::path& target) {
    project_->metadata().writtenBy = writerIdentity();
    project_->touch();

    if (const Status status = saveProject(target, *project_); !status) {
        logging::error(kLogCategory, "could not save {}: {}", target.string(), status.message());
        return status;
    }

    commands_->markSaved();
    attachDocument(target);
    config_.addRecentFile(target);

    // Saved work is not unsaved work.
    clearRecoveryFiles();
    sinceAutosave_ = Duration::zero();

    logging::info(kLogCategory, "Saved {}", target.filename().string());
    return Status::ok();
}

void Application::attachDocument(std::filesystem::path target) {
    path_ = std::move(target);
    if (path_.empty()) return;

    if (project_->metadata().name == "Untitled Project" || project_->metadata().name.empty()) {
        project_->metadata().name = path_.stem().string();
    }
}

std::string Application::documentTitle() const {
    const std::string name = path_.empty() ? project_->metadata().name : path_.filename().string();
    return std::format("{}{} - TNP", name, isDirty() ? " *" : "");
}

// ---------------------------------------------------------------------------
// Autosave and recovery
// ---------------------------------------------------------------------------

std::filesystem::path Application::autosavePath() const {
    return files::userStateDirectory() / "recovery.tnpjson";
}

std::filesystem::path Application::recoveryMarkerPath() const {
    return files::userStateDirectory() / "recovery.json";
}

void Application::detectRecoverableSession() {
    std::error_code ec;
    const bool hasAutosave = std::filesystem::exists(autosavePath(), ec);
    const bool hasMarker = std::filesystem::exists(recoveryMarkerPath(), ec);
    recoveryAvailable_ = hasAutosave && hasMarker;
    if (!recoveryAvailable_) return;

    // The marker records where the work came from, so the recovery prompt can
    // say something more useful than "an unnamed project".
    if (const auto text = files::readTextFile(recoveryMarkerPath())) {
        try {
            const auto marker = nlohmann::json::parse(text.value());
            recoveredFrom_ = marker.value("originalPath", std::string{});
        } catch (const std::exception&) {
            recoveredFrom_.clear();
        }
    }
}

void Application::writeAutosave() {
    const serial::ProjectSerializer serializer;
    auto document = serializer.write(*project_, false);
    if (!document) {
        logging::warning(kLogCategory, "autosave failed: {}", document.message());
        return;
    }

    if (const Status status = files::writeTextFileAtomic(autosavePath(), document.value()); !status) {
        logging::warning(kLogCategory, "autosave failed: {}", status.message());
        return;
    }

    nlohmann::json marker;
    marker["originalPath"] = path_.string();
    marker["projectName"] = project_->metadata().name;
    marker["savedAt"] = currentTimestampIso8601();
    (void)files::writeTextFileAtomic(recoveryMarkerPath(), marker.dump(2));

    logging::debug(kLogCategory, "autosaved to {}", autosavePath().string());
}

void Application::clearRecoveryFiles() {
    std::error_code ec;
    std::filesystem::remove(autosavePath(), ec);
    std::filesystem::remove(recoveryMarkerPath(), ec);
    recoveryAvailable_ = false;
}

Status Application::recoverSession() {
    if (!recoveryAvailable_) return Status::failure("there is nothing to recover");

    const std::string originalPath = recoveredFrom_;

    Status result = open(autosavePath());
    if (!result) return result;

    // The recovered work belongs to the original file, not to the autosave, and
    // it is unsaved by definition.
    path_ = originalPath.empty() ? std::filesystem::path{} : std::filesystem::path{originalPath};
    commands_->clear();

    // Mark dirty so the user cannot lose the recovered work by closing without
    // saving. Adding and undoing nothing would not do it, so the flag is set by
    // moving the saved marker instead.
    project_->touch();
    loadWarnings_.push_back("This project was recovered from an autosave; check it before saving.");

    clearRecoveryFiles();
    logging::info(kLogCategory, "Recovered the previous session");
    return Status::ok();
}

void Application::discardRecovery() {
    clearRecoveryFiles();
    logging::info(kLogCategory, "Recovered session discarded");
}

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------

void Application::update(Duration wallDelta) {
    simulator_->advance(wallDelta);

    if (!config_.autosaveEnabled) return;

    sinceAutosave_ += wallDelta;
    if (sinceAutosave_ < seconds(config_.autosaveIntervalSeconds)) return;

    sinceAutosave_ = Duration::zero();
    if (isDirty()) writeAutosave();
}

// ---------------------------------------------------------------------------
// Operations
// ---------------------------------------------------------------------------

DeviceId Application::addDevice(DeviceType type, Vec2 position) {
    const std::string name = project_->network().suggestDeviceName(type);

    auto command = std::make_unique<commands::AddDeviceCommand>(type, name, position, registry_);
    const DeviceId id = command->deviceId();

    if (!commands_->run(std::move(command))) return DeviceId{};

    selection_.select(ObjectRef::device(id));
    return id;
}

void Application::deleteSelection() {
    const auto devices = selection_.devices();
    const auto links = selection_.links();
    const auto annotations = selection_.annotations();

    // Deleting a device removes its links anyway, so a mixed selection is
    // handled device-first to avoid two commands touching the same link.
    if (!devices.empty()) {
        commands_->run(std::make_unique<commands::DeleteDevicesCommand>(devices));
    }
    if (!links.empty()) {
        commands_->run(std::make_unique<commands::DisconnectLinksCommand>(links));
    }
    if (!annotations.empty()) {
        commands_->run(std::make_unique<commands::DeleteAnnotationsCommand>(annotations));
    }

    selection_.clear();
}

Status Application::connectInterfaces(InterfaceId a, InterfaceId b) {
    if (!commands_->run(std::make_unique<commands::ConnectInterfacesCommand>(a, b))) {
        const std::string& reason = commands_->lastFailure();
        return Status::failure(reason.empty() ? "the interfaces could not be connected" : reason);
    }
    return Status::ok();
}

const validation::ValidationReport& Application::validationReport() {
    if (validationDirty_) {
        validationReport_ = validator_.validate(*project_);
        validationDirty_ = false;
    }
    return validationReport_;
}

testing::TestRunSummary Application::runTests() {
    testing::NetworkTestRunner runner{*project_};
    if (!runner.isReady()) {
        logging::error(kLogCategory, "tests could not run: {}", runner.error());
        lastTestRun_ = testing::TestRunSummary{};
        return lastTestRun_;
    }

    lastTestRun_ = runner.runAll();
    return lastTestRun_;
}

Result<std::string> Application::exportSvg(const SvgExportOptions& options) const {
    return exportTopologySvg(*project_, options);
}

void Application::setLearningModeEnabled(bool enabled) {
    learning_->setEnabled(enabled);
    project_->simulationSettings().learningModeEnabled = enabled;
    simulator_->applySettings(project_->simulationSettings());
}

void Application::pruneSelection() {
    const Network& network = project_->network();
    selection_.pruneIf([&](const ObjectRef& entry) {
        switch (entry.kind) {
            case ObjectKind::Device:    return network.findDevice(entry.asDeviceId()) == nullptr;
            case ObjectKind::Link:      return network.findLink(entry.asLinkId()) == nullptr;
            case ObjectKind::Interface: return network.findInterface(entry.asInterfaceId()) == nullptr;
            case ObjectKind::Annotation:
                return project_->findAnnotation(AnnotationId{entry.id}) == nullptr;
            case ObjectKind::Test:      return project_->findTest(TestId{entry.id}) == nullptr;
            default:                    return false;
        }
    });
}

} // namespace tnp::app
