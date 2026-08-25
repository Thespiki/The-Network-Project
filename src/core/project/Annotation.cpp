#include "core/project/Annotation.h"

namespace tnp::core {

std::string_view annotationKindName(AnnotationKind kind) {
    switch (kind) {
        case AnnotationKind::Text:         return "Text";
        case AnnotationKind::Rectangle:    return "Rectangle";
        case AnnotationKind::Ellipse:      return "Ellipse";
        case AnnotationKind::Arrow:        return "Arrow";
        case AnnotationKind::NetworkLabel: return "Network label";
    }
    return "Text";
}

} // namespace tnp::core
