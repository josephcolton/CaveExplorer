#pragma once

#include "core/Math.h"

#include <SDL3/SDL.h>

#include <string>

class SpriteDb;
class Tilemap;

// Thrown or fired objects (sling stones, arrows...). Drawn as a 16x16 icon.
struct Projectile {
    Rect box{0, 0, 8, 8};
    Vec2 prevPos;
    Vec2 velocity;
    float gravity = 500.0f;
    float damage = 1.0f;
    std::string icon;
    int attackId = 0;
    float life = 3.0f;
    bool dead = false;

    void launch(Vec2 center, Vec2 vel);
    void update(float dt, const Tilemap& map);
    void render(SDL_Renderer* r, const SpriteDb& sprites, Vec2 camera, float alpha) const;
};
