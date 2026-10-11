#pragma once

#include <algorithm>
#include <cmath>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
};

inline Vec2 lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }

inline float approach(float value, float target, float delta) {
    if (value < target) return std::min(value + delta, target);
    return std::max(value - delta, target);
}

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float left() const { return x; }
    float right() const { return x + w; }
    float top() const { return y; }
    float bottom() const { return y + h; }
    Vec2 pos() const { return {x, y}; }
    Vec2 center() const { return {x + w * 0.5f, y + h * 0.5f}; }

    bool overlaps(const Rect& o) const {
        return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
    }
};
