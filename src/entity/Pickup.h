#pragma once

#include "core/Math.h"
#include "game/Inventory.h"

#include <SDL3/SDL.h>

#include <string>

class SpriteDb;
class Tilemap;

// An item (or pile of coins) lying in the world. Falls under gravity.
struct Pickup {
    static constexpr float kSize = 20.0f;

    Rect box{0, 0, kSize, kSize};
    Vec2 prevPos;
    Vec2 velocity;
    ItemStack stack;           // empty when this is a coin pickup
    int coins = 0;
    std::string uniqueId;      // non-empty: remembered in the world save once taken
    float age = 0.0f;
    float pickupDelay = 0.0f;  // seconds before it can be collected (after dropping)
    bool removed = false;

    void placeAt(Vec2 feet);
    void update(float dt, const Tilemap& map);
    void render(SDL_Renderer* r, const SpriteDb& sprites, const std::string& icon, Vec2 camera, float alpha) const;
};
