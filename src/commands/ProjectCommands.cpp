#include "commands/ProjectCommands.h"

#include <algorithm>
#include <format>

namespace tnp::commands {

using namespace core;

// --- Annotations -----------------------------------------------------------

AddAnnotationCommand::AddAnnotationCommand(Annotation annotation) : annotation_(std::move(annotation)) {
    if (!annotation_.id.isValid()) annotation_.id = AnnotationId::generate();
}

std::string AddAnnotationCommand::label() const {
    return std::format("Add {}", annotationKindName(annotation_.kind));
}

bool AddAnnotationCommand::execute(Project& project) {
    project.addAnnotation(annotation_);
    return true;
}

void AddAnnotationCommand::undo(Project& project) { project.removeAnnotation(annotation_.id); }

DeleteAnnotationsCommand::DeleteAnnotationsCommand(std::vector<AnnotationId> annotations)
    : ids_(std::move(annotations)) {}

std::string DeleteAnnotationsCommand::label() const {
    return ids_.size() == 1 ? "Delete annotation" : std::format("Delete {} annotations", ids_.size());
}

bool DeleteAnnotationsCommand::execute(Project& project) {
    removed_.clear();

    const std::vector<Annotation>& annotations = project.annotations();
    for (const AnnotationId id : ids_) {
        const auto it = std::find_if(annotations.begin(), annotations.end(),
                                     [id](const Annotation& entry) { return entry.id == id; });
        if (it == annotations.end()) continue;
        removed_.emplace_back(static_cast<std::size_t>(std::distance(annotations.begin(), it)), *it);
    }

    for (const auto& [index, annotation] : removed_) project.removeAnnotation(annotation.id);
    return !removed_.empty();
}

void DeleteAnnotationsCommand::undo(Project& project) {
    std::vector<Annotation> annotations = project.annotations();

    // Reinsert from the lowest index up so each stored index stays valid.
    std::sort(removed_.begin(), removed_.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (const auto& [index, annotation] : removed_) {
        const std::size_t at = std::min(index, annotations.size());
        annotations.insert(annotations.begin() + static_cast<std::ptrdiff_t>(at), annotation);
    }
    project.setAnnotations(std::move(annotations));
    removed_.clear();
}

UpdateAnnotationCommand::UpdateAnnotationCommand(Annotation annotation)
    : newValue_(std::move(annotation)) {}

bool UpdateAnnotationCommand::execute(Project& project) {
    Annotation* existing = project.findAnnotation(newValue_.id);
    if (existing == nullptr) return false;

    oldValue_ = *existing;
    *existing = newValue_;
    return true;
}

void UpdateAnnotationCommand::undo(Project& project) {
    if (Annotation* existing = project.findAnnotation(newValue_.id)) *existing = oldValue_;
}

bool UpdateAnnotationCommand::mergeWith(const Command& next) {
    const auto* other = dynamic_cast<const UpdateAnnotationCommand*>(&next);
    if (other == nullptr || other->newValue_.id != newValue_.id) return false;

    newValue_ = other->newValue_;
    return true;
}

// --- Tests -----------------------------------------------------------------

AddTestCommand::AddTestCommand(NetworkTest test) : test_(std::move(test)) {
    if (!test_.id.isValid()) test_.id = TestId::generate();
}

std::string AddTestCommand::label() const { return std::format("Add test '{}'", test_.name); }

bool AddTestCommand::execute(Project& project) {
    project.addTest(test_);
    return true;
}

void AddTestCommand::undo(Project& project) { project.removeTest(test_.id); }

DeleteTestCommand::DeleteTestCommand(TestId test) : id_(test) {}

std::string DeleteTestCommand::label() const {
    return removed_.name.empty() ? "Delete test" : std::format("Delete test '{}'", removed_.name);
}

bool DeleteTestCommand::execute(Project& project) {
    const std::vector<NetworkTest>& tests = project.tests();
    const auto it = std::find_if(tests.begin(), tests.end(),
                                 [this](const NetworkTest& test) { return test.id == id_; });
    if (it == tests.end()) return false;

    removed_ = *it;
    index_ = static_cast<std::size_t>(std::distance(tests.begin(), it));
    return project.removeTest(id_);
}

void DeleteTestCommand::undo(Project& project) {
    std::vector<NetworkTest> tests = project.tests();
    tests.insert(tests.begin() + static_cast<std::ptrdiff_t>(std::min(index_, tests.size())), removed_);
    project.setTests(std::move(tests));
}

UpdateTestCommand::UpdateTestCommand(NetworkTest test) : newValue_(std::move(test)) {}

bool UpdateTestCommand::execute(Project& project) {
    NetworkTest* existing = project.findTest(newValue_.id);
    if (existing == nullptr) return false;

    oldValue_ = *existing;
    *existing = newValue_;
    return true;
}

void UpdateTestCommand::undo(Project& project) {
    if (NetworkTest* existing = project.findTest(newValue_.id)) *existing = oldValue_;
}

// --- Metadata --------------------------------------------------------------

SetProjectMetadataCommand::SetProjectMetadataCommand(ProjectMetadata metadata)
    : newValue_(std::move(metadata)) {}

bool SetProjectMetadataCommand::execute(Project& project) {
    ProjectMetadata& current = project.metadata();
    const bool unchanged = current.name == newValue_.name &&
                           current.description == newValue_.description &&
                           current.author == newValue_.author && current.tags == newValue_.tags;
    if (unchanged) return false;

    oldValue_ = current;
    current.name = newValue_.name;
    current.description = newValue_.description;
    current.author = newValue_.author;
    current.tags = newValue_.tags;
    return true;
}

void SetProjectMetadataCommand::undo(Project& project) {
    ProjectMetadata& current = project.metadata();
    current.name = oldValue_.name;
    current.description = oldValue_.description;
    current.author = oldValue_.author;
    current.tags = oldValue_.tags;
}

} // namespace tnp::commands
