#include "entity/Player.h"

#include "core/Input.h"
#include "world/Tilemap.h"

void Player::placeAt(Vec2 feet) {
    box.x = feet.x - box.w * 0.5f;
    box.y = feet.y - box.h;
    prevPos = box.pos();
    velocity = {};
    contact = {};
    contact.onGround = true;
    coyoteTimer_ = jumpBufferTimer_ = dropThroughTimer_ = 0.0f;
    jumping_ = false;
}

void Player::update(float dt, const Input& input, const Tilemap& map) {
    prevPos = box.pos();

    // --- Horizontal: accelerate toward the target speed instead of snapping to it.
    const float dir = (input.down(Action::Right) ? 1.0f : 0.0f) - (input.down(Action::Left) ? 1.0f : 0.0f);
    if (dir != 0.0f) facing = dir > 0.0f ? 1 : -1;
    const float target = dir * stats.runSpeed;
    float accel = contact.onGround ? stats.groundAccel : stats.airAccel;
    if (dir == 0.0f && contact.onGround) accel = stats.groundFriction;
    velocity.x = approach(velocity.x, target, accel * dt);

    // --- Jump timers (coyote time + input buffering make jumping forgiving).
    coyoteTimer_ = contact.onGround ? stats.coyoteTime : std::max(0.0f, coyoteTimer_ - dt);
    jumpBufferTimer_ = input.pressed(Action::Jump) ? stats.jumpBufferTime : std::max(0.0f, jumpBufferTimer_ - dt);
    dropThroughTimer_ = std::max(0.0f, dropThroughTimer_ - dt);

    if (jumpBufferTimer_ > 0.0f && contact.onGround && contact.onOneWay && input.down(Action::Down)) {
        // Down + Jump on a one-way platform drops through it.
        dropThroughTimer_ = 0.2f;
        jumpBufferTimer_ = 0.0f;
        coyoteTimer_ = 0.0f;
    } else if (jumpBufferTimer_ > 0.0f && coyoteTimer_ > 0.0f) {
        velocity.y = -stats.jumpVelocity;
        jumping_ = true;
        jumpBufferTimer_ = 0.0f;
        coyoteTimer_ = 0.0f;
    }

    if (jumping_ && !input.down(Action::Jump) && velocity.y < 0.0f) {
        velocity.y *= stats.jumpCutMultiplier;
        jumping_ = false;
    }
    if (velocity.y >= 0.0f) jumping_ = false;

    // --- Gravity
    const float g = velocity.y > 0.0f ? stats.gravity * stats.fallGravityMultiplier : stats.gravity;
    velocity.y = std::min(velocity.y + g * dt, stats.maxFallSpeed);

    contact = moveAndCollide(map, box, velocity, dt, dropThroughTimer_ > 0.0f);
}

void Player::render(SDL_Renderer* renderer, Vec2 camera, float alpha, bool debug) const {
    const Vec2 p = lerp(prevPos, box.pos(), alpha);
    // Placeholder sprite: a 32x32 frame centred on the collision box's feet.
    const float sx = std::round(p.x + box.w * 0.5f - 16.0f - camera.x);
    const float sy = std::round(p.y + box.h - 32.0f - camera.y);

    const SDL_FRect body{sx + 7, sy + 8, 18, 24};
    SDL_SetRenderDrawColor(renderer, 214, 140, 60, 255);
    SDL_RenderFillRect(renderer, &body);
    const SDL_FRect hat{sx + 5, sy + 2, 22, 7};
    SDL_SetRenderDrawColor(renderer, 120, 84, 40, 255);
    SDL_RenderFillRect(renderer, &hat);
    const SDL_FRect eye{sx + (facing > 0 ? 19.0f : 9.0f), sy + 12, 4, 4};
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderFillRect(renderer, &eye);

    if (debug) {
        const SDL_FRect hit{std::round(p.x - camera.x), std::round(p.y - camera.y), box.w, box.h};
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderRect(renderer, &hit);
    }
}
