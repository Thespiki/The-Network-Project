#pragma once

#include <cmath>

namespace tnp {

/// A point or offset in project coordinates.
///
/// Lives in the utility layer because both the domain model (device placement,
/// annotations) and the renderer need it. Project coordinates are independent of
/// zoom and of the window: the canvas converts to screen space at draw time.
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float xValue, float yValue) : x(xValue), y(yValue) {}

    constexpr Vec2 operator+(const Vec2& other) const { return {x + other.x, y + other.y}; }
    constexpr Vec2 operator-(const Vec2& other) const { return {x - other.x, y - other.y}; }
    constexpr Vec2 operator*(float scale) const { return {x * scale, y * scale}; }
    constexpr Vec2 operator/(float scale) const { return {x / scale, y / scale}; }

    constexpr Vec2& operator+=(const Vec2& other) { x += other.x; y += other.y; return *this; }
    constexpr Vec2& operator-=(const Vec2& other) { x -= other.x; y -= other.y; return *this; }

    [[nodiscard]] float length() const { return std::sqrt(x * x + y * y); }
    [[nodiscard]] float lengthSquared() const { return x * x + y * y; }

    [[nodiscard]] Vec2 normalized() const {
        const float len = length();
        return len > 0.0f ? Vec2{x / len, y / len} : Vec2{};
    }

    bool operator==(const Vec2&) const = default;
};

[[nodiscard]] inline Vec2 lerp(const Vec2& from, const Vec2& to, float t) {
    return {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t};
}

/// An axis-aligned rectangle, used for selection and annotations.
struct Rect {
    Vec2 min;
    Vec2 max;

    constexpr Rect() = default;
    constexpr Rect(Vec2 minCorner, Vec2 maxCorner) : min(minCorner), max(maxCorner) {}

    [[nodiscard]] static Rect fromCorners(Vec2 a, Vec2 b) {
        return Rect{{a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y},
                    {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y}};
    }
    [[nodiscard]] static Rect fromCenter(Vec2 center, Vec2 halfSize) {
        return Rect{center - halfSize, center + halfSize};
    }

    [[nodiscard]] float width() const { return max.x - min.x; }
    [[nodiscard]] float height() const { return max.y - min.y; }
    [[nodiscard]] Vec2 size() const { return max - min; }
    [[nodiscard]] Vec2 center() const { return {(min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f}; }

    [[nodiscard]] bool contains(const Vec2& point) const {
        return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
    }
    [[nodiscard]] bool intersects(const Rect& other) const {
        return !(other.min.x > max.x || other.max.x < min.x ||
                 other.min.y > max.y || other.max.y < min.y);
    }

    void expandToInclude(const Vec2& point) {
        if (point.x < min.x) min.x = point.x;
        if (point.y < min.y) min.y = point.y;
        if (point.x > max.x) max.x = point.x;
        if (point.y > max.y) max.y = point.y;
    }

    [[nodiscard]] Rect expanded(float margin) const {
        return Rect{{min.x - margin, min.y - margin}, {max.x + margin, max.y + margin}};
    }
};

} // namespace tnp
