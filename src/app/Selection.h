#pragma once

#include "core/network/Ids.h"

#include <algorithm>
#include <vector>

namespace tnp::app {

/// What the user currently has selected.
///
/// Holds `ObjectRef`s rather than typed identifiers so one selection can span
/// devices, links and annotations - which is what box-select produces - without
/// the canvas maintaining three parallel lists.
class Selection {
public:
    void clear() { items_.clear(); }

    /// Replaces the selection with one object.
    void select(core::ObjectRef object);

    void add(core::ObjectRef object);
    void remove(const core::ObjectRef& object);
    void toggle(core::ObjectRef object);

    [[nodiscard]] bool contains(const core::ObjectRef& object) const;

    [[nodiscard]] const std::vector<core::ObjectRef>& items() const { return items_; }
    [[nodiscard]] bool empty() const { return items_.empty(); }
    [[nodiscard]] std::size_t size() const { return items_.size(); }

    /// The object whose properties are shown: the last one selected.
    [[nodiscard]] core::ObjectRef primary() const;

    [[nodiscard]] std::vector<core::DeviceId> devices() const;
    [[nodiscard]] std::vector<core::LinkId> links() const;
    [[nodiscard]] std::vector<core::AnnotationId> annotations() const;

    /// Drops entries whose object no longer exists. Called after a delete or an
    /// undo, so the panels never chase a dangling reference.
    template <typename Predicate>
    void pruneIf(Predicate isGone) {
        items_.erase(std::remove_if(items_.begin(), items_.end(), isGone), items_.end());
    }

private:
    std::vector<core::ObjectRef> items_;
};

} // namespace tnp::app
