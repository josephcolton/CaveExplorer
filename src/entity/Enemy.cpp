#include "entity/Enemy.h"

#include "core/Sprites.h"
#include "world/Collision.h"
#include "world/Tilemap.h"

using nlohmann::json;

namespace {
constexpr float kGravity = 1400.0f;
constexpr float kMaxFall = 600.0f;
constexpr float kHurtTime = 0.3f;
constexpr float T = static_cast<float>(Tilemap::kTileSize);
}  // namespace

EnemyDef EnemyDef::fromJson(const std::string& id, const json& j) {
    EnemyDef d;
    d.id = id;
    d.name = j.value("name", id);
    d.behavior = j.value("behavior", "walker");
    d.anim = j.value("anim", "");
    d.icon = j.value("icon", "");
    d.iconFacesLeft = j.value("faces", "right") == "left";
    const auto tint = j.value("tint", std::vector<int>{255, 255, 255});
    if (tint.size() == 3) d.tint = {static_cast<Uint8>(tint[0]), static_cast<Uint8>(tint[1]), static_cast<Uint8>(tint[2]), 255};
    d.health = j.value("health", 5.0f);
    d.damage = j.value("damage", 5.0f);
    d.speed = j.value("speed", 50.0f);
    d.chaseSpeed = j.value("chase_speed", d.speed * 1.6f);
    d.sight = j.value("sight", 160.0f);
    const auto size = j.value("size", std::vector<float>{24, 16});
    if (size.size() == 2) { d.width = size[0]; d.height = size[1]; }
    for (const json& drop : j.value("drops", json::array())) {
        d.drops.push_back({drop.value("item", "coins"), drop.value("min", 1), drop.value("max", 1), drop.value("chance", 1.0f)});
    }
    return d;
}

void Enemy::init(const EnemyDef& d, const AnimDb& anims, Vec2 pos, float healthScale) {
    def = &d;
    box = {0, 0, d.width, d.height};
    if (d.flies()) {
        box.x = pos.x - box.w * 0.5f;
        box.y = pos.y - box.h * 0.5f;
    } else {
        box.x = pos.x - box.w * 0.5f;
        box.y = pos.y - box.h;
    }
    prevPos = box.pos();
    home_ = box.center();
    health = d.health * healthScale;
    if (!d.anim.empty()) {
        anim_.setSet(anims.find(d.anim));
        anim_.play("fly");
    }
    // Stagger idle motion so groups don't move in lockstep.
    time_ = pos.x * 0.013f;
}

bool Enemy::hit(float damage, float fromX, int attackId) {
    if (dead || attackId == lastAttackId_) return false;
    lastAttackId_ = attackId;
    health -= damage;
    hurtTimer_ = kHurtTime;
    const float away = box.center().x >= fromX ? 1.0f : -1.0f;
    velocity = {away * 170.0f, def->flies() ? -60.0f : -160.0f};
    if (health <= 0.0f) dead = true;
    return true;
}

void Enemy::touchedPlayer() {
    touchCooldown = 0.8f;
    if (def->flies()) {
        state_ = State::Retreat;
        stateTimer_ = 0.9f;
    }
}

void Enemy::update(float dt, const Tilemap& map, const Rect& player, bool playerAlive) {
    prevPos = box.pos();
    time_ += dt;
    hurtTimer_ = std::max(0.0f, hurtTimer_ - dt);
    touchCooldown = std::max(0.0f, touchCooldown - dt);
    if (def->flies()) updateFlyer(dt, map, player, playerAlive);
    else updateWalker(dt, map, player, playerAlive);
    anim_.update(dt);
}

void Enemy::updateWalker(float dt, const Tilemap& map, const Rect& player, bool playerAlive) {
    const Vec2 me = box.center();
    const Vec2 p = player.center();
    const bool sees = playerAlive && std::fabs(p.x - me.x) < def->sight && std::fabs(p.y - me.y) < 48.0f;
    state_ = sees ? State::Chase : State::Patrol;

    if (hurtTimer_ <= 0.0f) {
        if (state_ == State::Chase && std::fabs(p.x - me.x) > 4.0f) dir_ = p.x > me.x ? 1 : -1;
        float target = static_cast<float>(dir_) * (state_ == State::Chase ? def->chaseSpeed : def->speed);

        // Don't walk off ledges: patrols turn around, chasers stop at the edge.
        if (onGround_) {
            const float footX = dir_ > 0 ? box.right() + 1.0f : box.left() - 1.0f;
            const int col = static_cast<int>(std::floor(footX / T));
            const int row = static_cast<int>(std::floor((box.bottom() + 1.0f) / T));
            if (map.collisionAt(col, row) == TileCollision::None) {
                if (state_ == State::Patrol) dir_ = -dir_;
                target = 0.0f;
            }
        }
        velocity.x = approach(velocity.x, target, 700.0f * dt);
    } else {
        velocity.x = approach(velocity.x, 0.0f, 400.0f * dt);
    }

    velocity.y = std::min(velocity.y + kGravity * dt, kMaxFall);
    const MoveResult r = moveAndCollide(map, box, velocity, dt, false);
    onGround_ = r.onGround;
    if (state_ == State::Patrol && (r.hitWallLeft || r.hitWallRight)) dir_ = -dir_;
}

void Enemy::updateFlyer(float dt, const Tilemap& map, const Rect& player, bool playerAlive) {
    const Vec2 me = box.center();
    const Vec2 p = player.center();
    const float dx = p.x - me.x;
    const float dy = p.y - me.y;
    const bool sees = playerAlive && dx * dx + dy * dy < def->sight * def->sight;

    Vec2 target;
    if (state_ == State::Retreat) {
        stateTimer_ -= dt;
        target = {me.x - (dx > 0 ? 60.0f : -60.0f), home_.y - 24.0f};
        if (stateTimer_ <= 0.0f) state_ = State::Patrol;
    } else if (sees) {
        state_ = State::Chase;
        target = p;
    } else {
        state_ = State::Patrol;
        target = home_ + Vec2{std::sin(time_ * 1.1f) * 28.0f, std::sin(time_ * 2.3f) * 10.0f};
    }

    if (hurtTimer_ <= 0.0f) {
        const Vec2 to = target - me;
        const float dist = std::sqrt(to.x * to.x + to.y * to.y);
        const float speed = state_ == State::Chase ? def->speed : def->speed * 0.6f;
        const Vec2 desired = dist > 2.0f ? to * (speed * std::min(1.0f, dist / 24.0f) / dist) : Vec2{};
        velocity.x = approach(velocity.x, desired.x, 320.0f * dt);
        velocity.y = approach(velocity.y, desired.y, 320.0f * dt);
        if (std::fabs(velocity.x) > 5.0f) dir_ = velocity.x > 0 ? 1 : -1;
    } else {
        velocity.x = approach(velocity.x, 0.0f, 300.0f * dt);
        velocity.y = approach(velocity.y, 0.0f, 300.0f * dt);
    }
    anim_.play(state_ == State::Chase ? "fly" : "glide");
    moveAndCollide(map, box, velocity, dt, true);
}

void Enemy::render(SDL_Renderer* r, const SpriteDb& sprites, Vec2 camera, float alpha) const {
    const Vec2 p = lerp(prevPos, box.pos(), alpha);
    SDL_Color tint = def->tint;
    if (hurtTimer_ > 0.0f && static_cast<int>(hurtTimer_ * 20.0f) % 2 == 0) tint = {255, 80, 80, 255};

    const float cx = p.x + box.w * 0.5f - camera.x;
    if (!def->anim.empty()) {
        anim_.draw(r, cx, p.y + box.h * 0.5f - camera.y, dir_ > 0, tint);
    } else {
        // Static icon, standing on the box's bottom edge, flipped to face its direction.
        const bool flip = (dir_ > 0) == def->iconFacesLeft;
        sprites.drawIcon(r, def->icon, cx - 16.0f, p.y + box.h - 32.0f - camera.y, 32.0f, flip, tint);
    }
}
