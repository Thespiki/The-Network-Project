#include "commands/CommandManager.h"

#include "utilities/Logging.h"

namespace tnp::commands {

CommandManager::CommandManager(core::Project& project) : project_(project) {}

bool CommandManager::run(CommandPtr command) {
    if (!command) return false;

    lastFailure_.clear();

    if (!command->execute(project_)) {
        // Nothing changed: do not pollute the history with a no-op. The reason is
        // captured here because the command is about to be destroyed.
        lastFailure_ = command->failureReason();
        return false;
    }

    project_.touch();

    // Any new change invalidates the redo branch.
    redoStack_.clear();

    if (allowMerge_ && !undoStack_.empty() && undoStack_.back()->kind() == command->kind() &&
        undoStack_.back()->mergeWith(*command)) {
        // Merged into the previous entry; the position still advances so the
        // document stays marked dirty.
        ++position_;
        notifyChanged();
        return true;
    }

    undoStack_.push_back(std::move(command));
    while (undoStack_.size() > historyLimit_) {
        undoStack_.pop_front();
        // The dropped entry can never be undone again, so the saved marker has
        // to move with it or `isDirty()` would go stale.
        if (savedPosition_ > 0) --savedPosition_;
    }

    allowMerge_ = true;
    ++position_;
    notifyChanged();
    return true;
}

bool CommandManager::undo() {
    if (undoStack_.empty()) return false;

    CommandPtr command = std::move(undoStack_.back());
    undoStack_.pop_back();

    command->undo(project_);
    project_.touch();

    logging::debug("commands", "undo: {}", command->label());

    redoStack_.push_back(std::move(command));
    --position_;
    allowMerge_ = false;
    notifyChanged();
    return true;
}

bool CommandManager::redo() {
    if (redoStack_.empty()) return false;

    CommandPtr command = std::move(redoStack_.back());
    redoStack_.pop_back();

    if (!command->execute(project_)) {
        // The model moved on and the change no longer applies; drop it rather
        // than leaving a redo entry that does nothing.
        logging::warning("commands", "redo of '{}' no longer applies and was discarded", command->label());
        allowMerge_ = false;
        notifyChanged();
        return false;
    }
    project_.touch();

    logging::debug("commands", "redo: {}", command->label());

    undoStack_.push_back(std::move(command));
    ++position_;
    allowMerge_ = false;
    notifyChanged();
    return true;
}

std::string CommandManager::undoLabel() const {
    return undoStack_.empty() ? std::string{} : undoStack_.back()->label();
}

std::string CommandManager::redoLabel() const {
    return redoStack_.empty() ? std::string{} : redoStack_.back()->label();
}

void CommandManager::clear() {
    undoStack_.clear();
    redoStack_.clear();
    position_ = 0;
    savedPosition_ = 0;
    allowMerge_ = false;
}

void CommandManager::setHistoryLimit(std::size_t limit) {
    historyLimit_ = limit == 0 ? 1 : limit;
    while (undoStack_.size() > historyLimit_) undoStack_.pop_front();
}

void CommandManager::notifyChanged() {
    if (onChanged_) onChanged_();
}

} // namespace tnp::commands
