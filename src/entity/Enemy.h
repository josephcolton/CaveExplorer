#pragma once

#include "core/Animation.h"
#include "core/Math.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <string>
#include <vector>

class SpriteDb;
class Tilemap;

struct EnemyDrop {
    std::string item;  // item id or "coins"
    int min = 1;
    int max = 1;
    float chance = 1.0f;
};

// A creature type from data/enemies.json.
struct EnemyDef {
    std::string id;
    std::string name;
    std::string behavior = "walker";  // walker | flyer
    std::string anim;                 // animation set (preferred), or...
    std::string icon;                 // ...a static 32x32 icon
    bool iconFacesLeft = false;
    SDL_Color tint{255, 255, 255, 255};
    float health = 5.0f;
    float damage = 5.0f;
    float speed = 50.0f;
    float chaseSpeed = 80.0f;
    float sight = 160.0f;
    float width = 24.0f;
    float height = 16.0f;
    std::vector<EnemyDrop> drops;

    bool flies() const { return behavior == "flyer"; }
    static EnemyDef fromJson(const std::string& id, const nlohmann::json& j);
};

class Enemy {
public:
    // pos = feet (walkers) or centre (flyers).
    void init(const EnemyDef& def, const AnimDb& anims, Vec2 pos, float healthScale);
    void update(float dt, const Tilemap& map, const Rect& player, bool playerAlive);
    void render(SDL_Renderer* r, const SpriteDb& sprites, Vec2 camera, float alpha) const;

    // Applies damage from an attack. attackId stops one swing from hitting twice.
    // Returns true if the hit landed.
    bool hit(float damage, float fromX, int attackId);
    // Called after this enemy damages the player (flyers back off for a moment).
    void touchedPlayer();

    const EnemyDef* def = nullptr;
    Rect box;
    Vec2 prevPos;
    Vec2 velocity;
    float health = 1.0f;
    bool dead = false;
    float touchCooldown = 0.0f;

private:
    enum class State { Patrol, Chase, Retreat };
    void updateWalker(float dt, const Tilemap& map, const Rect& player, bool playerAlive);
    void updateFlyer(float dt, const Tilemap& map, const Rect& player, bool playerAlive);

    Animator anim_;
    State state_ = State::Patrol;
    Vec2 home_;
    int dir_ = -1;
    float hurtTimer_ = 0.0f;
    float stateTimer_ = 0.0f;
    float time_ = 0.0f;
    int lastAttackId_ = -1;
    bool onGround_ = false;
};
