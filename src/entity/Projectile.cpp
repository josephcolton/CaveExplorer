#include "entity/Projectile.h"

#include "core/Sprites.h"
#include "world/Collision.h"

void Projectile::launch(Vec2 center, Vec2 vel) {
    box.x = center.x - box.w * 0.5f;
    box.y = center.y - box.h * 0.5f;
    prevPos = box.pos();
    velocity = vel;
}

void Projectile::update(float dt, const Tilemap& map) {
    prevPos = box.pos();
    life -= dt;
    velocity.y += gravity * dt;
    const MoveResult r = moveAndCollide(map, box, velocity, dt, true);
    if (life <= 0.0f || r.onGround || r.hitCeiling || r.hitWallLeft || r.hitWallRight) dead = true;
}

void Projectile::render(SDL_Renderer* r, const SpriteDb& sprites, Vec2 camera, float alpha) const {
    const Vec2 p = lerp(prevPos, box.pos(), alpha);
    sprites.drawIcon(r, icon, p.x + box.w * 0.5f - 8.0f - camera.x, p.y + box.h * 0.5f - 8.0f - camera.y, 16.0f);
}
