#pragma once

#include "core/network/Network.h"
#include "core/project/Annotation.h"
#include "core/project/Layout.h"
#include "core/project/NetworkTest.h"
#include "core/project/ProjectMetadata.h"
#include "core/project/SimulationSettings.h"

#include <memory>
#include <vector>

namespace tnp::core {

/// Everything that is saved to a `.tnp` file.
///
/// A project owns the topology, where it is drawn, what is written on it, the
/// tests that assert it works, and the simulation options it should run with.
/// It owns no runtime state: the simulator, the undo stack and the selection all
/// live above it, so loading a project twice yields two identical models.
class Project {
public:
    Project();
    ~Project();

    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;

    [[nodiscard]] ProjectMetadata& metadata() { return metadata_; }
    [[nodiscard]] const ProjectMetadata& metadata() const { return metadata_; }

    [[nodiscard]] Network& network() { return *network_; }
    [[nodiscard]] const Network& network() const { return *network_; }

    [[nodiscard]] Layout& layout() { return layout_; }
    [[nodiscard]] const Layout& layout() const { return layout_; }

    [[nodiscard]] SimulationSettings& simulationSettings() { return simulation_; }
    [[nodiscard]] const SimulationSettings& simulationSettings() const { return simulation_; }

    // --- Annotations -------------------------------------------------------
    [[nodiscard]] const std::vector<Annotation>& annotations() const { return annotations_; }
    Annotation& addAnnotation(Annotation annotation);
    bool removeAnnotation(AnnotationId id);
    [[nodiscard]] Annotation* findAnnotation(AnnotationId id);
    [[nodiscard]] const Annotation* findAnnotation(AnnotationId id) const;
    void setAnnotations(std::vector<Annotation> annotations);

    // --- Tests -------------------------------------------------------------
    [[nodiscard]] const std::vector<NetworkTest>& tests() const { return tests_; }
    NetworkTest& addTest(NetworkTest test);
    bool removeTest(TestId id);
    [[nodiscard]] NetworkTest* findTest(TestId id);
    [[nodiscard]] const NetworkTest* findTest(TestId id) const;
    void setTests(std::vector<NetworkTest> tests);

    // --- Housekeeping ------------------------------------------------------
    /// Empties the project back to a new, unnamed state.
    void reset();

    /// Drops layout entries, annotations and tests that refer to devices which
    /// no longer exist. Runs after loading, where a hand-edited file may contain
    /// dangling references.
    void pruneDanglingReferences();

    /// Stamps `modifiedAt`. Called by the application when the model changes.
    void touch();

private:
    ProjectMetadata metadata_;
    std::unique_ptr<Network> network_;
    Layout layout_;
    SimulationSettings simulation_;
    std::vector<Annotation> annotations_;
    std::vector<NetworkTest> tests_;
};

} // namespace tnp::core
