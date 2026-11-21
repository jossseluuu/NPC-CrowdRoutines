#pragma once
// Small geometry helpers. World units are metres, time units are seconds.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace npc {

struct Vec2 {
    float x = 0.f, y = 0.f;
    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}
    constexpr Vec2& operator+=(Vec2 b) { x += b.x; y += b.y; return *this; }
    constexpr Vec2& operator-=(Vec2 b) { x -= b.x; y -= b.y; return *this; }
    constexpr Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    friend constexpr Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
    friend constexpr Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
    friend constexpr Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
    friend constexpr Vec2 operator*(float s, Vec2 a) { return {a.x * s, a.y * s}; }
    friend constexpr bool operator==(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }
};

constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
constexpr float cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
constexpr float length_sq(Vec2 v) { return v.x * v.x + v.y * v.y; }
inline float length(Vec2 v) { return std::sqrt(length_sq(v)); }
inline float distance(Vec2 a, Vec2 b) { return length(a - b); }
inline Vec2 normalized(Vec2 v) {
    const float l = length(v);
    return l > 1e-6f ? v * (1.f / l) : Vec2{};
}
constexpr Vec2 perp(Vec2 v) { return {-v.y, v.x}; }
constexpr Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
constexpr float lerpf(float a, float b, float t) { return a + (b - a) * t; }
constexpr float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

struct Rect {
    Vec2 min, max;
    constexpr bool contains(Vec2 p) const { return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y; }
    constexpr Vec2 center() const { return (min + max) * 0.5f; }
    constexpr Vec2 size() const { return max - min; }
};

using Polygon = std::vector<Vec2>;

inline bool point_in_polygon(const Polygon& poly, Vec2 p) {
    bool inside = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const Vec2 a = poly[i], b = poly[j];
        if (((a.y > p.y) != (b.y > p.y)) && (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x)) inside = !inside;
    }
    return inside;
}

inline float polygon_area(const Polygon& poly) {
    float a = 0.f;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) a += cross(poly[j], poly[i]);
    return std::abs(a) * 0.5f;
}

inline Vec2 polygon_centroid(const Polygon& poly) {
    Vec2 c{};
    if (poly.empty()) return c;
    for (auto p : poly) c += p;
    return c * (1.f / static_cast<float>(poly.size()));
}

inline Rect polygon_bounds(const Polygon& poly) {
    Rect r{{1e30f, 1e30f}, {-1e30f, -1e30f}};
    for (auto p : poly) {
        r.min.x = std::min(r.min.x, p.x); r.min.y = std::min(r.min.y, p.y);
        r.max.x = std::max(r.max.x, p.x); r.max.y = std::max(r.max.y, p.y);
    }
    return r;
}

/// Shrinks a convex polygon towards its centroid by `amount` metres (approximate inset).
inline Polygon inset(const Polygon& poly, float amount) {
    const Vec2 c = polygon_centroid(poly);
    Polygon out;
    out.reserve(poly.size());
    for (auto p : poly) {
        const Vec2 d = p - c;
        const float l = length(d);
        out.push_back(l > amount ? c + d * ((l - amount) / l) : c);
    }
    return out;
}

/// Distance from p to the segment ab, and the closest point parameter t in [0,1].
inline float distance_to_segment(Vec2 p, Vec2 a, Vec2 b, float* t_out = nullptr) {
    const Vec2 ab = b - a;
    const float l2 = length_sq(ab);
    float t = l2 > 0.f ? std::clamp(dot(p - a, ab) / l2, 0.f, 1.f) : 0.f;
    if (t_out) *t_out = t;
    return distance(p, a + ab * t);
}

} // namespace npc
