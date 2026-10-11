#include "entity/Pickup.h"

#include "core/Sprites.h"
#include "world/Collision.h"
#include "world/Tilemap.h"

namespace {
constexpr float kGravity = 1400.0f;
constexpr float kMaxFall = 600.0f;
constexpr float kGroundFriction = 600.0f;
}  // namespace

void Pickup::placeAt(Vec2 feet) {
    box.x = feet.x - box.w * 0.5f;
    box.y = feet.y - box.h;
    prevPos = box.pos();
}

void Pickup::update(float dt, const Tilemap& map) {
    prevPos = box.pos();
    age += dt;
    pickupDelay = std::max(0.0f, pickupDelay - dt);
    velocity.y = std::min(velocity.y + kGravity * dt, kMaxFall);
    const MoveResult r = moveAndCollide(map, box, velocity, dt, false);
    if (r.onGround) velocity.x = approach(velocity.x, 0.0f, kGroundFriction * dt);
}

void Pickup::render(SDL_Renderer* r, const SpriteDb& sprites, const std::string& icon, Vec2 camera, float alpha) const {
    const Vec2 p = lerp(prevPos, box.pos(), alpha);
    const float bob = std::round(std::sin(age * 3.0f) * 2.0f) - 2.0f;
    // Icons are 32x32; centre on the box, resting on its bottom edge.
    sprites.drawIcon(r, icon, p.x + box.w * 0.5f - 16.0f - camera.x, p.y + box.h - 32.0f + bob - camera.y);
}
