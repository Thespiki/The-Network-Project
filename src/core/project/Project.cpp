#include "core/project/Project.h"

#include "utilities/Time.h"

#include <algorithm>

namespace tnp::core {

Project::Project() : network_(std::make_unique<Network>()) {
    metadata_.createdAt = currentTimestampIso8601();
    metadata_.modifiedAt = metadata_.createdAt;
}

Project::~Project() = default;

Annotation& Project::addAnnotation(Annotation annotation) {
    if (!annotation.id.isValid()) annotation.id = AnnotationId::generate();
    annotations_.push_back(std::move(annotation));
    return annotations_.back();
}

bool Project::removeAnnotation(AnnotationId id) {
    const auto it = std::find_if(annotations_.begin(), annotations_.end(),
                                 [id](const Annotation& annotation) { return annotation.id == id; });
    if (it == annotations_.end()) return false;
    annotations_.erase(it);
    return true;
}

Annotation* Project::findAnnotation(AnnotationId id) {
    const auto it = std::find_if(annotations_.begin(), annotations_.end(),
                                 [id](const Annotation& annotation) { return annotation.id == id; });
    return it == annotations_.end() ? nullptr : &*it;
}

const Annotation* Project::findAnnotation(AnnotationId id) const {
    return const_cast<Project*>(this)->findAnnotation(id);
}

void Project::setAnnotations(std::vector<Annotation> annotations) {
    annotations_ = std::move(annotations);
}

NetworkTest& Project::addTest(NetworkTest test) {
    if (!test.id.isValid()) test.id = TestId::generate();
    tests_.push_back(std::move(test));
    return tests_.back();
}

bool Project::removeTest(TestId id) {
    const auto it = std::find_if(tests_.begin(), tests_.end(),
                                 [id](const NetworkTest& test) { return test.id == id; });
    if (it == tests_.end()) return false;
    tests_.erase(it);
    return true;
}

NetworkTest* Project::findTest(TestId id) {
    const auto it = std::find_if(tests_.begin(), tests_.end(),
                                 [id](const NetworkTest& test) { return test.id == id; });
    return it == tests_.end() ? nullptr : &*it;
}

const NetworkTest* Project::findTest(TestId id) const {
    return const_cast<Project*>(this)->findTest(id);
}

void Project::setTests(std::vector<NetworkTest> tests) { tests_ = std::move(tests); }

void Project::reset() {
    metadata_ = ProjectMetadata{};
    metadata_.createdAt = currentTimestampIso8601();
    metadata_.modifiedAt = metadata_.createdAt;

    network_->clear();
    layout_.clear();
    layout_.viewOffset = Vec2{};
    layout_.viewZoom = 1.0f;
    simulation_ = SimulationSettings{};
    annotations_.clear();
    tests_.clear();
}

void Project::pruneDanglingReferences() {
    std::vector<DeviceId> orphanedPlacements;
    for (const auto& [device, placement] : layout_.placements()) {
        if (network_->findDevice(device) == nullptr) orphanedPlacements.push_back(device);
    }
    for (const DeviceId device : orphanedPlacements) layout_.remove(device);

    const auto removedTests = std::remove_if(tests_.begin(), tests_.end(), [this](const NetworkTest& test) {
        if (network_->findDevice(test.source) == nullptr) return true;
        if (test.destinationDevice.isValid() &&
            network_->findDevice(test.destinationDevice) == nullptr) {
            return true;
        }
        return !test.hasDestination();
    });
    tests_.erase(removedTests, tests_.end());
}

void Project::touch() { metadata_.modifiedAt = currentTimestampIso8601(); }

} // namespace tnp::core
