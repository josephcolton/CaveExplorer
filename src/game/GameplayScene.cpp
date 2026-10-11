#include "game/GameplayScene.h"

#include "core/App.h"

namespace {
constexpr float kFadeSpeed = 4.0f;  // full fade in 0.25s
constexpr float kAreaTitleTime = 2.5f;
constexpr const char* kStartArea = "beach";
constexpr const char* kStartSpawn = "start";
}  // namespace

GameplayScene::GameplayScene(App& app) : Scene(app) {
    enterArea(app.options().startArea, app.options().startSpawn);
}

void GameplayScene::enterArea(const std::string& areaId, const std::string& spawn) {
    // Areas are always loaded fresh from disk: enemies, obstacles and loot reset.
    if (!area_.load(areaId, app_.assets())) {
        SDL_Log("Failed to load area '%s'", areaId.c_str());
        if (areaId != kStartArea) enterArea(kStartArea, kStartSpawn);
        return;
    }

    const MapObject* sp = area_.findObject("spawn", spawn);
    if (!sp) {
        SDL_Log("Area '%s' has no spawn '%s'", areaId.c_str(), spawn.c_str());
        player_.placeAt({area_.map.pixelWidth() * 0.5f, 0.0f});
    } else {
        player_.placeAt({sp->rect.x, sp->rect.y});
    }
    camera_.snapTo(player_.box.center(), area_.map.pixelWidth(), area_.map.pixelHeight(),
                   App::kViewWidth, App::kViewHeight);
    areaTitleTimer_ = kAreaTitleTime;
}

void GameplayScene::requestTransition(const std::string& areaId, const std::string& spawn) {
    if (fadeDirection_ == 1) return;
    pendingArea_ = areaId;
    pendingSpawn_ = spawn;
    fadeDirection_ = 1;
}

const MapObject* GameplayScene::doorUnderPlayer() const {
    for (const MapObject& o : area_.objects) {
        if (o.type == "door" && o.rect.overlaps(player_.box)) return &o;
    }
    return nullptr;
}

void GameplayScene::checkDoors() {
    const MapObject* door = doorUnderPlayer();
    if (!door) return;
    // "touch" doors are area edges; other doors (cave mouths, hatches) need Up/Interact.
    const bool touch = door->getString("trigger", "interact") == "touch";
    const Input& in = app_.input();
    if (touch || in.pressed(Action::Up) || in.pressed(Action::Interact)) {
        requestTransition(door->getString("target_area"), door->getString("target_spawn"));
    }
}

void GameplayScene::update(float dt) {
    if (fadeDirection_ != 0) {
        fade_ += fadeDirection_ * kFadeSpeed * dt;
        if (fadeDirection_ == 1 && fade_ >= 1.0f) {
            fade_ = 1.0f;
            enterArea(pendingArea_, pendingSpawn_);
            fadeDirection_ = -1;
        } else if (fadeDirection_ == -1 && fade_ <= 0.0f) {
            fade_ = 0.0f;
            fadeDirection_ = 0;
        }
    }
    if (fadeDirection_ == 1) return;  // freeze while the screen goes dark

    player_.update(dt, app_.input(), area_.map);
    camera_.update(dt, player_.box.center(), area_.map.pixelWidth(), area_.map.pixelHeight(),
                   App::kViewWidth, App::kViewHeight);
    areaTitleTimer_ = std::max(0.0f, areaTitleTimer_ - dt);

    if (fadeDirection_ == 0) checkDoors();
}

void GameplayScene::render(SDL_Renderer* renderer, float alpha) {
    const SDL_Color bg = area_.background;
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    const SDL_FRect screen{0, 0, App::kViewWidth, App::kViewHeight};
    SDL_RenderFillRect(renderer, &screen);

    const Vec2 cam = camera_.view(alpha);
    area_.map.render(renderer, cam, false);
    player_.render(renderer, cam, alpha, app_.debugDraw);
    area_.map.render(renderer, cam, true);

    if (const MapObject* door = doorUnderPlayer();
        door && fadeDirection_ == 0 && door->getString("trigger", "interact") != "touch") {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, std::round(player_.box.center().x - cam.x - 36), std::round(player_.box.y - cam.y - 14),
                            "[Up] Enter");
    }

    if (app_.debugDraw) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        for (const MapObject& o : area_.objects) {
            const SDL_FRect r{o.rect.x - cam.x, o.rect.y - cam.y, std::max(o.rect.w, 4.0f), std::max(o.rect.h, 4.0f)};
            SDL_RenderRect(renderer, &r);
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(renderer, 4, 4, "%s  pos %.1f,%.1f  vel %.0f,%.0f  %s", area_.id.c_str(),
                                  player_.box.x, player_.box.y, player_.velocity.x, player_.velocity.y,
                                  player_.contact.onGround ? "ground" : "air");
    }

    if (areaTitleTimer_ > 0.0f) {
        const float a = std::min(1.0f, areaTitleTimer_);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, static_cast<Uint8>(255 * a));
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        const float w = static_cast<float>(area_.displayName.size() * 8);
        SDL_RenderDebugText(renderer, std::round((App::kViewWidth - w) * 0.5f), 40, area_.displayName.c_str());
    }

    if (fade_ > 0.0f) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(255 * std::clamp(fade_, 0.0f, 1.0f)));
        SDL_RenderFillRect(renderer, &screen);
    }
}
