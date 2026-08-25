#pragma once

#include "core/network/Ids.h"
#include "utilities/Geometry.h"
#include "utilities/Types.h"

#include <string>
#include <string_view>

namespace tnp::core {

/// Kinds of drawing a user can place on the canvas.
enum class AnnotationKind : u8 {
    Text,          ///< a free-standing label
    Rectangle,     ///< an area outline, optionally filled
    Ellipse,
    Arrow,         ///< a line from `start` to `end` with a head
    NetworkLabel   ///< a subnet or zone caption, drawn with a subdued box
};

[[nodiscard]] std::string_view annotationKindName(AnnotationKind kind);

/// A drawing on the topology canvas.
///
/// Annotations are part of the project model, not of the renderer: they are
/// saved, undoable and exported, exactly like devices are.
struct Annotation {
    AnnotationId id = AnnotationId::generate();
    AnnotationKind kind = AnnotationKind::Text;

    /// For text and boxes these are the two corners; for an arrow, its ends.
    Vec2 start;
    Vec2 end;

    std::string text;

    /// Packed 0xAABBGGRR, matching the renderer's native layout so no
    /// conversion happens per frame.
    u32 color = 0xFFE0E0E0;
    u32 fillColor = 0x20FFFFFF;

    float fontSize = 16.0f;
    float thickness = 2.0f;
    bool filled = false;

    /// Draw order. Lower values are painted first.
    i32 zOrder = 0;

    [[nodiscard]] Rect bounds() const { return Rect::fromCorners(start, end); }
};

} // namespace tnp::core
