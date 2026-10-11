#pragma once

#include "core/Math.h"

// Smoothly follows a target and stays inside the area bounds.
class Camera {
public:
    void snapTo(Vec2 focus, float mapW, float mapH, float viewW, float viewH) {
        pos_ = clampTo(focus - Vec2{viewW * 0.5f, viewH * 0.5f}, mapW, mapH, viewW, viewH);
        prev_ = pos_;
    }

    void update(float dt, Vec2 focus, float mapW, float mapH, float viewW, float viewH) {
        prev_ = pos_;
        const Vec2 target = clampTo(focus - Vec2{viewW * 0.5f, viewH * 0.5f}, mapW, mapH, viewW, viewH);
        const float t = 1.0f - std::exp(-kStiffness * dt);
        pos_ = clampTo(lerp(pos_, target, t), mapW, mapH, viewW, viewH);
    }

    // Interpolated and rounded to whole pixels so tiles never shimmer.
    Vec2 view(float alpha) const {
        const Vec2 p = lerp(prev_, pos_, alpha);
        return {std::round(p.x), std::round(p.y)};
    }

private:
    static constexpr float kStiffness = 10.0f;

    static Vec2 clampTo(Vec2 p, float mapW, float mapH, float viewW, float viewH) {
        // Areas smaller than the screen are centred.
        p.x = mapW <= viewW ? (mapW - viewW) * 0.5f : std::clamp(p.x, 0.0f, mapW - viewW);
        p.y = mapH <= viewH ? (mapH - viewH) * 0.5f : std::clamp(p.y, 0.0f, mapH - viewH);
        return p;
    }

    Vec2 pos_;
    Vec2 prev_;
};
