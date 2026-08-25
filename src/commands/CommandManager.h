#pragma once

#include "commands/Command.h"

#include <deque>
#include <functional>
#include <string>

namespace tnp::commands {

/// The undo/redo stack.
///
/// Also owns the "document has unsaved changes" flag, because the two answer the
/// same question: has anything happened since the last save? Tracking it here
/// means undoing back to the saved point correctly reports the document as
/// clean again.
class CommandManager {
public:
    explicit CommandManager(core::Project& project);

    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;

    /// Executes a command and pushes it onto the undo stack.
    ///
    /// Returns false when the command declined to change anything; in that case
    /// it is destroyed and the history is untouched.
    bool run(CommandPtr command);

    /// Why the last `run()` returned false. Empty when the command simply had
    /// nothing to do.
    [[nodiscard]] const std::string& lastFailure() const { return lastFailure_; }

    [[nodiscard]] bool canUndo() const { return !undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redoStack_.empty(); }

    /// Label of the change that undo/redo would apply, or an empty string.
    [[nodiscard]] std::string undoLabel() const;
    [[nodiscard]] std::string redoLabel() const;

    bool undo();
    bool redo();

    void clear();

    /// The project has unsaved changes.
    [[nodiscard]] bool isDirty() const { return position_ != savedPosition_; }
    void markSaved() { savedPosition_ = position_; }

    /// Called after every change, so the application can invalidate validation
    /// results and refresh the window title.
    void setChangeCallback(std::function<void()> callback) { onChanged_ = std::move(callback); }

    [[nodiscard]] std::size_t undoDepth() const { return undoStack_.size(); }
    [[nodiscard]] std::size_t redoDepth() const { return redoStack_.size(); }

    /// Oldest entries are dropped past this depth, so a long editing session has
    /// a bounded cost.
    void setHistoryLimit(std::size_t limit);
    [[nodiscard]] std::size_t historyLimit() const { return historyLimit_; }

    /// Merging window: two commands of the same kind executed within this many
    /// consecutive calls may be merged. Set to false to force the next command
    /// to start a new history entry (used when a drag ends).
    void breakMergeChain() { allowMerge_ = false; }

private:
    void notifyChanged();

    core::Project& project_;
    std::deque<CommandPtr> undoStack_;
    std::deque<CommandPtr> redoStack_;

    /// Monotonic counter of applied changes; compared with `savedPosition_` to
    /// answer `isDirty()` even after undoing back past the save point.
    i64 position_ = 0;
    i64 savedPosition_ = 0;

    std::string lastFailure_;
    std::size_t historyLimit_ = 200;
    bool allowMerge_ = false;
    std::function<void()> onChanged_;
};

} // namespace tnp::commands
