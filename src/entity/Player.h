#pragma once

#include "core/Math.h"
#include "world/Collision.h"

#include <SDL3/SDL.h>

class Input;
class Tilemap;

// Movement tuning. Values are in pixels and seconds. Later these become the
// base values that character stats, gear, buffs and hunger modify.
struct MovementStats {
    float runSpeed = 165.0f;
    float groundAccel = 1500.0f;
    float groundFriction = 1900.0f;
    float airAccel = 1000.0f;
    float gravity = 1500.0f;
    float fallGravityMultiplier = 1.5f;  // falling is snappier than rising
    float jumpVelocity = 520.0f;          // ~2.8 tiles of jump height
    float jumpCutMultiplier = 0.45f;      // releasing jump early shortens the jump
    float maxFallSpeed = 640.0f;
    float coyoteTime = 0.10f;
    float jumpBufferTime = 0.12f;
};

class Player {
public:
    // Collision box is narrower than a 32px tile so the player can stand on tile edges.
    static constexpr float kWidth = 18.0f;
    static constexpr float kHeight = 30.0f;

    void placeAt(Vec2 feet);  // feet = bottom-center
    void update(float dt, const Input& input, const Tilemap& map);
    void render(SDL_Renderer* renderer, Vec2 camera, float alpha, bool debug) const;

    Rect box{0, 0, kWidth, kHeight};
    Vec2 prevPos;
    Vec2 velocity;
    int facing = 1;
    MovementStats stats;
    MoveResult contact;

private:
    float coyoteTimer_ = 0.0f;
    float jumpBufferTimer_ = 0.0f;
    float dropThroughTimer_ = 0.0f;
    bool jumping_ = false;
};
