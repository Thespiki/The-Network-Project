#pragma once

#include "commands/Command.h"

#include <vector>

namespace tnp::commands {

class AddAnnotationCommand final : public Command {
public:
    explicit AddAnnotationCommand(core::Annotation annotation);

    [[nodiscard]] std::string_view kind() const override { return "add-annotation"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

    [[nodiscard]] core::AnnotationId annotationId() const { return annotation_.id; }

private:
    core::Annotation annotation_;
};

class DeleteAnnotationsCommand final : public Command {
public:
    explicit DeleteAnnotationsCommand(std::vector<core::AnnotationId> annotations);

    [[nodiscard]] std::string_view kind() const override { return "delete-annotations"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    std::vector<core::AnnotationId> ids_;
    /// Removed annotations with the index they occupied, so undo restores draw
    /// order as well as content.
    std::vector<std::pair<std::size_t, core::Annotation>> removed_;
};

class UpdateAnnotationCommand final : public Command {
public:
    explicit UpdateAnnotationCommand(core::Annotation annotation);

    [[nodiscard]] std::string_view kind() const override { return "update-annotation"; }
    [[nodiscard]] std::string label() const override { return "Edit annotation"; }

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;
    [[nodiscard]] bool mergeWith(const Command& next) override;

private:
    core::Annotation newValue_;
    core::Annotation oldValue_;
};

class AddTestCommand final : public Command {
public:
    explicit AddTestCommand(core::NetworkTest test);

    [[nodiscard]] std::string_view kind() const override { return "add-test"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

    [[nodiscard]] core::TestId testId() const { return test_.id; }

private:
    core::NetworkTest test_;
};

class DeleteTestCommand final : public Command {
public:
    explicit DeleteTestCommand(core::TestId test);

    [[nodiscard]] std::string_view kind() const override { return "delete-test"; }
    [[nodiscard]] std::string label() const override;

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::TestId id_;
    core::NetworkTest removed_;
    std::size_t index_ = 0;
};

class UpdateTestCommand final : public Command {
public:
    explicit UpdateTestCommand(core::NetworkTest test);

    [[nodiscard]] std::string_view kind() const override { return "update-test"; }
    [[nodiscard]] std::string label() const override { return "Edit test"; }

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::NetworkTest newValue_;
    core::NetworkTest oldValue_;
};

/// Edits the project's name, description, author and tags.
class SetProjectMetadataCommand final : public Command {
public:
    explicit SetProjectMetadataCommand(core::ProjectMetadata metadata);

    [[nodiscard]] std::string_view kind() const override { return "set-project-metadata"; }
    [[nodiscard]] std::string label() const override { return "Edit project details"; }

    [[nodiscard]] bool execute(core::Project& project) override;
    void undo(core::Project& project) override;

private:
    core::ProjectMetadata newValue_;
    core::ProjectMetadata oldValue_;
};

} // namespace tnp::commands
