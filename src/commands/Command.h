#pragma once

#include "core/project/Project.h"

#include <memory>
#include <string>
#include <string_view>

namespace tnp::commands {

/// One undoable change to a project.
///
/// Every command stores only what it needs to reverse *itself* - the device that
/// was deleted, the addresses that were replaced, the offset a selection moved
/// by. Nothing snapshots the whole application: a project with hundreds of
/// devices would make that unusable, and a diff-based history is what lets undo
/// stay instant regardless of project size.
class Command {
public:
    virtual ~Command() = default;

    /// Stable identifier of the command kind, used for merging and for tests.
    [[nodiscard]] virtual std::string_view kind() const = 0;

    /// What the Edit menu shows: "Add Router1", "Move 3 devices".
    [[nodiscard]] virtual std::string label() const = 0;

    /// Performs the change. Returning false means nothing happened, and the
    /// command is discarded instead of joining the undo stack - which is what
    /// keeps a no-op edit from filling the history.
    [[nodiscard]] virtual bool execute(core::Project& project) = 0;

    virtual void undo(core::Project& project) = 0;

    /// Why `execute` declined, when it returned false and the reason is worth
    /// showing. Read through `CommandManager::lastFailure()`: a command that
    /// does nothing is destroyed immediately, so the caller must not hold a
    /// pointer to it.
    [[nodiscard]] virtual std::string failureReason() const { return {}; }

    /// Absorbs `next` if the two form one logical edit.
    ///
    /// Used for dragging: a drag produces a command per frame, and without
    /// merging a single gesture would need dozens of undos to reverse.
    [[nodiscard]] virtual bool mergeWith(const Command& next) {
        (void)next;
        return false;
    }
};

using CommandPtr = std::unique_ptr<Command>;

} // namespace tnp::commands
