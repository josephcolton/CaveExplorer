#pragma once

#include "core/Math.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <string>

class SpriteDb;
class Tilemap;

// A kind of obstacle from data/obstacles.json.
struct ObstacleDef {
    std::string id;
    std::string name;
    std::string requiredTag;  // item tag needed to clear it ("cut", "break")
    int hits = 1;
    std::string icon;      // 32x32 icon drawn per tile, or...
    std::string sprite;    // ...a named sprite drawn per tile
    std::string hint;
    std::string verb = "Clear";
    std::string cleared;
    SDL_Color debris{140, 140, 140, 255};
    // Pushing through from the far side (so obstacles never trap you).
    float pushDamage = 0.0f;
    std::string pushMessage;
    std::string hitSound = "chop";
    std::string clearSound = "chop";

    static ObstacleDef fromJson(const std::string& id, const nlohmann::json& j);
};

// Blocks whole tiles until cleared with the right tool. Comes back when the area reloads.
struct Obstacle {
    const ObstacleDef* def = nullptr;
    int tx = 0, ty = 0, tw = 1, th = 1;  // tile rectangle
    int hitsLeft = 1;
    int lastAttackId = -1;
    float shake = 0.0f;
    bool cleared = false;
    int passableFrom = 0;    // -1: can be pushed through from the left, +1 from the right, 0: never
    float pushTime = 0.0f;   // how long the explorer has been pushing against it

    Rect rect() const;
    void block(Tilemap& map, bool blocked) const;
    void update(float dt) { shake = std::max(0.0f, shake - dt); }
    void render(SDL_Renderer* r, const SpriteDb& sprites, Vec2 camera) const;
};
