#include "entity/Player.h"

#include "core/Input.h"
#include "game/Character.h"
#include "world/Tilemap.h"

namespace {
constexpr float kHurtLockTime = 0.3f;
constexpr float kInvulnerableTime = 1.1f;
constexpr float kSwingHitStart = 0.05f;  // hit window within a swing
constexpr float kSwingHitEnd = 0.20f;
constexpr float kAttackAnimTime = 0.25f;
}  // namespace

void Player::applyCharacter(const Character& c) {
    const MovementStats base;
    stats.runSpeed = base.runSpeed * c.speed;
    stats.jumpVelocity = base.jumpVelocity * c.jump;
    maxHealth = c.maxHealth;
}

void Player::placeAt(Vec2 feet) {
    box.x = feet.x - box.w * 0.5f;
    box.y = feet.y - box.h;
    prevPos = box.pos();
    velocity = {};
    contact = {};
    contact.onGround = true;
    coyoteTimer_ = jumpBufferTimer_ = dropThroughTimer_ = 0.0f;
    jumping_ = false;
    hurtTimer_ = attackCooldown_ = 0.0f;
    swingTime_ = -1.0f;
}

void Player::startAttack(float reach, float damage, float cooldown) {
    attackCooldown_ = cooldown;
    swingReach_ = reach;
    swingDamage_ = damage;
    swingTime_ = reach > 0.0f ? 0.0f : -1.0f;
    attackAnimTime_ = kAttackAnimTime;
    ++attackId_;
    anim_.play("attack", true);
}

bool Player::swingHitbox(Rect& out) const {
    if (swingTime_ < kSwingHitStart || swingTime_ > kSwingHitEnd) return false;
    out.w = swingReach_;
    out.h = box.h + 8.0f;
    out.x = facing > 0 ? box.right() : box.left() - swingReach_;
    out.y = box.y - 6.0f;
    return true;
}

bool Player::hurt(float damage, float fromX) {
    if (invulnerable > 0.0f || dead()) return false;
    health = std::max(0.0f, health - damage);
    invulnerable = kInvulnerableTime;
    hurtTimer_ = kHurtLockTime;
    const float away = box.center().x >= fromX ? 1.0f : -1.0f;
    velocity = {away * 170.0f, -230.0f};
    jumping_ = false;
    swingTime_ = -1.0f;
    return true;
}

void Player::update(float dt, const Input& input, const Tilemap& map) {
    prevPos = box.pos();
    invulnerable = std::max(0.0f, invulnerable - dt);
    hurtTimer_ = std::max(0.0f, hurtTimer_ - dt);
    attackCooldown_ = std::max(0.0f, attackCooldown_ - dt);
    attackAnimTime_ = std::max(0.0f, attackAnimTime_ - dt);
    if (swingTime_ >= 0.0f) {
        swingTime_ += dt;
        if (swingTime_ > kSwingHitEnd) swingTime_ = -1.0f;
    }
    const bool control = hurtTimer_ <= 0.0f && !dead();

    // --- Horizontal: accelerate toward the target speed instead of snapping to it.
    float dir = 0.0f;
    if (control) dir = (input.down(Action::Right) ? 1.0f : 0.0f) - (input.down(Action::Left) ? 1.0f : 0.0f);
    // Keep facing the swing direction until it finishes.
    if (dir != 0.0f && swingTime_ < 0.0f) facing = dir > 0.0f ? 1 : -1;
    const float target = dir * stats.runSpeed;
    float accel = contact.onGround ? stats.groundAccel : stats.airAccel;
    if (dir == 0.0f && contact.onGround) accel = stats.groundFriction;
    if (!control) accel = contact.onGround ? stats.groundFriction * 0.5f : 200.0f;
    velocity.x = approach(velocity.x, target, accel * dt);

    // --- Jump timers (coyote time + input buffering make jumping forgiving).
    coyoteTimer_ = contact.onGround ? stats.coyoteTime : std::max(0.0f, coyoteTimer_ - dt);
    jumpBufferTimer_ =
        (control && input.pressed(Action::Jump)) ? stats.jumpBufferTime : std::max(0.0f, jumpBufferTimer_ - dt);
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

    chooseAnimation();
    anim_.speed = anim_.clip() == "run" ? std::max(0.5f, std::fabs(velocity.x) / 165.0f) : 1.0f;
    anim_.update(dt);
}

void Player::chooseAnimation() {
    if (dead()) anim_.play("dead");
    else if (hurtTimer_ > 0.0f) anim_.play("hurt");
    else if (attackAnimTime_ > 0.0f) anim_.play("attack");
    else if (!contact.onGround) anim_.play(velocity.y < 0.0f ? "jump" : "fall");
    else if (std::fabs(velocity.x) > 12.0f) anim_.play("run");
    else anim_.play("idle");
}

void Player::render(SDL_Renderer* renderer, Vec2 camera, float alpha, bool debug) const {
    const Vec2 p = lerp(prevPos, box.pos(), alpha);
    const float feetX = p.x + box.w * 0.5f - camera.x;
    const float feetY = p.y + box.h - camera.y;

    // Flicker while invulnerable after a hit.
    const bool visible = invulnerable <= 0.0f || hurtTimer_ > 0.0f || static_cast<int>(invulnerable * 14.0f) % 2 == 0;
    if (visible) anim_.draw(renderer, feetX, feetY, facing > 0);

    if (debug) {
        const SDL_FRect hit{std::round(p.x - camera.x), std::round(p.y - camera.y), box.w, box.h};
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderRect(renderer, &hit);
        Rect swing;
        if (swingHitbox(swing)) {
            const SDL_FRect s{swing.x - camera.x, swing.y - camera.y, swing.w, swing.h};
            SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
            SDL_RenderRect(renderer, &s);
        }
    }
}
