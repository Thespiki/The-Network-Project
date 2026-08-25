#include "app/Selection.h"

namespace tnp::app {

using namespace core;

void Selection::select(ObjectRef object) {
    items_.clear();
    if (object.isValid()) items_.push_back(object);
}

void Selection::add(ObjectRef object) {
    if (!object.isValid() || contains(object)) return;
    items_.push_back(object);
}

void Selection::remove(const ObjectRef& object) {
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [&](const ObjectRef& entry) { return entry == object; }),
                 items_.end());
}

void Selection::toggle(ObjectRef object) {
    if (contains(object)) remove(object);
    else                  add(object);
}

bool Selection::contains(const ObjectRef& object) const {
    return std::any_of(items_.begin(), items_.end(),
                       [&](const ObjectRef& entry) { return entry == object; });
}

ObjectRef Selection::primary() const {
    return items_.empty() ? ObjectRef{} : items_.back();
}

std::vector<DeviceId> Selection::devices() const {
    std::vector<DeviceId> result;
    for (const ObjectRef& entry : items_) {
        if (entry.kind == ObjectKind::Device) result.push_back(entry.asDeviceId());
    }
    return result;
}

std::vector<LinkId> Selection::links() const {
    std::vector<LinkId> result;
    for (const ObjectRef& entry : items_) {
        if (entry.kind == ObjectKind::Link) result.push_back(entry.asLinkId());
    }
    return result;
}

std::vector<AnnotationId> Selection::annotations() const {
    std::vector<AnnotationId> result;
    for (const ObjectRef& entry : items_) {
        if (entry.kind == ObjectKind::Annotation) result.push_back(AnnotationId{entry.id});
    }
    return result;
}

} // namespace tnp::app
