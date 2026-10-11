#pragma once

#include "core/Animation.h"
#include "core/Math.h"
#include "world/Collision.h"

#include <SDL3/SDL.h>

class Input;
class Tilemap;
struct Character;

// Movement tuning. Values are in pixels and seconds. The explorer's rolled stats
// scale these (and later gear, buffs and hunger will too).
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

    void setAnimations(const AnimSet* set) { anim_.setSet(set); }
    void applyCharacter(const Character& c);
    void placeAt(Vec2 feet);  // feet = bottom-center
    void update(float dt, const Input& input, const Tilemap& map);
    void render(SDL_Renderer* renderer, Vec2 camera, float alpha, bool debug) const;

    // Combat
    bool canAttack() const { return attackCooldown_ <= 0.0f && hurtTimer_ <= 0.0f && !dead(); }
    // Starts a melee swing (reach/damage) or, with reach 0, just the throw animation.
    void startAttack(float reach, float damage, float cooldown);
    // The active melee hitbox, if this frame is inside the swing's hit window.
    bool swingHitbox(Rect& out) const;
    float swingDamage() const { return swingDamage_; }
    int attackId() const { return attackId_; }
    // Returns true if damage was taken (false while invulnerable or dead).
    bool hurt(float damage, float fromX);
    bool dead() const { return health <= 0.0f; }

    Rect box{0, 0, kWidth, kHeight};
    Vec2 prevPos;
    Vec2 velocity;
    int facing = 1;
    MovementStats stats;
    MoveResult contact;
    float health = 100.0f;
    float maxHealth = 100.0f;
    float invulnerable = 0.0f;

private:
    void chooseAnimation();

    Animator anim_;
    float coyoteTimer_ = 0.0f;
    float jumpBufferTimer_ = 0.0f;
    float dropThroughTimer_ = 0.0f;
    bool jumping_ = false;
    float hurtTimer_ = 0.0f;       // knocked back: no control
    float attackCooldown_ = 0.0f;
    float swingTime_ = -1.0f;      // seconds since the swing started, <0 when not swinging
    float swingReach_ = 0.0f;
    float swingDamage_ = 0.0f;
    float attackAnimTime_ = 0.0f;
    int attackId_ = 0;
};
