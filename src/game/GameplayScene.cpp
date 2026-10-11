#include "game/GameplayScene.h"

#include "core/App.h"
#include "ui/Ui.h"

#include <nlohmann/json.hpp>

#include <sstream>

using nlohmann::json;

namespace {
constexpr float kFadeSpeed = 4.0f;  // full fade in 0.25s
constexpr float kAreaTitleTime = 2.5f;
constexpr float kToastTime = 2.5f;
constexpr const char* kStartArea = "beach";
constexpr const char* kStartSpawn = "start";
constexpr int kStartingCoins = 40;
constexpr const char* kWorldSaveFile = "world.json";
}  // namespace

GameplayScene::GameplayScene(App& app) : Scene(app) {
    loadData();
    loadWorld();
    inventory_.coins = kStartingCoins;

    // Debug: --give id,id,...
    std::stringstream give(app.options().giveItems);
    for (std::string id; std::getline(give, id, ',');) {
        if (items_.find(id)) inventory_.add(items_, id, 1);
        else SDL_Log("--give: unknown item '%s'", id.c_str());
    }

    enterArea(app.options().startArea, app.options().startSpawn);

    // Debug: --ui inventory|chest|shop
    const std::string& ui = app.options().openUi;
    if (ui == "inventory") openInventory(nullptr);
    else if (ui == "chest") openInventory(&world_.chest);
    else if (ui == "shop") openShop("beach_shop");
}

GameplayScene::~GameplayScene() { saveWorld(); }

// ---------------------------------------------------------------------------- data & saving

void GameplayScene::loadData() {
    std::string error;
    if (auto text = readTextFile(dataPath("items.json")); !text || !items_.loadFromJson(*text, &error)) {
        SDL_Log("items.json: %s", error.c_str());
    }
    if (auto text = readTextFile(dataPath("shops.json"))) {
        json doc = json::parse(*text, nullptr, false);
        const json shops = doc.value("shops", json::object());
        for (auto& [id, s] : shops.items()) {
            ShopDef def;
            def.id = id;
            def.name = s.value("name", id);
            def.greeting = s.value("greeting", "");
            def.stock = s.value("stock", std::vector<std::string>{});
            shops_[id] = std::move(def);
        }
    }
    sprites_.load(app_.assets());
}

void GameplayScene::loadWorld() {
    if (auto text = readTextFile(savePath(kWorldSaveFile))) {
        json doc = json::parse(*text, nullptr, false);
        if (!doc.is_discarded()) world_.fromJson(doc);
    }
}

void GameplayScene::saveWorld() {
    writeTextFile(savePath(kWorldSaveFile), world_.toJson().dump(1));
}

// ---------------------------------------------------------------------------- areas

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
    camera_.snapTo(player_.box.center(), area_.map.pixelWidth(), area_.map.pixelHeight(), App::kViewWidth,
                   App::kViewHeight);
    areaTitleTimer_ = kAreaTitleTime;
    spawnAreaObjects();
}

void GameplayScene::spawnAreaObjects() {
    pickups_.clear();
    for (const MapObject& o : area_.objects) {
        if (o.type != "pickup") continue;
        Pickup p;
        p.uniqueId = o.getString("unique_id");
        if (!p.uniqueId.empty() && world_.takenUniques.count(p.uniqueId)) continue;
        const std::string item = o.getString("item");
        if (item == "coins") {
            p.coins = static_cast<int>(o.getFloat("count", 1));
        } else {
            if (!items_.find(item)) {
                SDL_Log("%s: pickup %d has unknown item '%s'", area_.id.c_str(), o.id, item.c_str());
                continue;
            }
            p.stack = {item, std::max(1, static_cast<int>(o.getFloat("count", 1)))};
        }
        // Point objects mark the bottom-centre; rectangles use their bottom edge.
        p.placeAt({o.rect.x + o.rect.w * 0.5f, o.rect.y + o.rect.h});
        pickups_.push_back(std::move(p));
    }
}

void GameplayScene::requestTransition(const std::string& areaId, const std::string& spawn) {
    if (fadeDirection_ == 1) return;
    pendingArea_ = areaId;
    pendingSpawn_ = spawn;
    fadeDirection_ = 1;
}

// ---------------------------------------------------------------------------- update

void GameplayScene::update(float dt) {
    for (Toast& t : toasts_) t.time -= dt;
    std::erase_if(toasts_, [](const Toast& t) { return t.time <= 0.0f; });
    fullMessageCooldown_ = std::max(0.0f, fullMessageCooldown_ - dt);

    if (fadeDirection_ != 0) {
        fade_ += static_cast<float>(fadeDirection_) * kFadeSpeed * dt;
        if (fadeDirection_ == 1 && fade_ >= 1.0f) {
            fade_ = 1.0f;
            enterArea(pendingArea_, pendingSpawn_);
            fadeDirection_ = -1;
        } else if (fadeDirection_ == -1 && fade_ <= 0.0f) {
            fade_ = 0.0f;
            fadeDirection_ = 0;
        }
    }

    const Input& in = app_.input();
    switch (mode_) {
    case Mode::Inventory: {
        const InventoryRequest req = inventoryScreen_.update(in);
        if (req.kind == InventoryRequest::Kind::Close) {
            mode_ = Mode::Play;
            if (chestOpen_) saveWorld();
            chestOpen_ = false;
        } else if (req.kind == InventoryRequest::Kind::Drop) {
            dropStack(req.stack);
        } else if (req.kind == InventoryRequest::Kind::Use) {
            useItem(req.stack);
        }
        return;  // the world is paused while the menu is open
    }
    case Mode::Shop:
        if (!shopScreen_.update(in)) mode_ = Mode::Play;
        return;
    case Mode::Play:
        updatePlay(dt);
        return;
    }
}

void GameplayScene::updatePlay(float dt) {
    if (fadeDirection_ == 1) return;  // freeze while the screen goes dark

    const Input& in = app_.input();
    if (fadeDirection_ == 0 && in.pressed(Action::Inventory)) {
        openInventory(nullptr);
        return;
    }

    player_.update(dt, in, area_.map);
    updatePickups(dt);
    camera_.update(dt, player_.box.center(), area_.map.pixelWidth(), area_.map.pixelHeight(), App::kViewWidth,
                   App::kViewHeight);
    areaTitleTimer_ = std::max(0.0f, areaTitleTimer_ - dt);

    if (fadeDirection_ == 0) handleInteraction();
}

void GameplayScene::updatePickups(float dt) {
    for (Pickup& p : pickups_) {
        p.update(dt, area_.map);
        if (p.pickupDelay > 0.0f || !p.box.overlaps(player_.box)) continue;

        if (p.coins > 0) {
            inventory_.coins += p.coins;
            toast("+" + std::to_string(p.coins) + " coins");
            p.removed = true;
        } else {
            const ItemDef& def = items_.get(p.stack.id);
            const int left = inventory_.add(items_, p.stack.id, p.stack.count);
            const int taken = p.stack.count - left;
            if (taken > 0) toast(taken > 1 ? def.name + " x" + std::to_string(taken) : def.name);
            p.stack.count = left;
            if (left == 0) {
                p.removed = true;
            } else if (fullMessageCooldown_ <= 0.0f) {
                toast("Backpack full");
                fullMessageCooldown_ = 2.0f;
            }
        }
        if (p.removed && !p.uniqueId.empty()) {
            world_.takenUniques.insert(p.uniqueId);
            saveWorld();
        }
    }
    std::erase_if(pickups_, [](const Pickup& p) { return p.removed; });
}

const MapObject* GameplayScene::interactableUnderPlayer() const {
    for (const MapObject& o : area_.objects) {
        const bool interactive = o.type == "chest" || o.type == "shop" ||
                                 (o.type == "door" && o.getString("trigger", "interact") != "touch");
        const bool touchDoor = o.type == "door" && !interactive;
        if ((interactive || touchDoor) && o.rect.overlaps(player_.box)) return &o;
    }
    return nullptr;
}

void GameplayScene::handleInteraction() {
    const MapObject* o = interactableUnderPlayer();
    if (!o) return;
    const Input& in = app_.input();
    const bool pressed = in.pressed(Action::Up) || in.pressed(Action::Interact);

    if (o->type == "door") {
        // "touch" doors are area edges; other doors (cave mouths, hatches) need Up/Interact.
        if (o->getString("trigger", "interact") == "touch" || pressed) {
            requestTransition(o->getString("target_area"), o->getString("target_spawn"));
        }
    } else if (pressed && o->type == "chest") {
        openInventory(&world_.chest);
    } else if (pressed && o->type == "shop") {
        openShop(o->getString("shop_id"));
    }
}

void GameplayScene::openInventory(Container* chest) {
    chestOpen_ = chest != nullptr;
    inventoryScreen_.open(items_, sprites_, inventory_, chest);
    mode_ = Mode::Inventory;
}

void GameplayScene::openShop(const std::string& shopId) {
    auto it = shops_.find(shopId);
    if (it == shops_.end()) {
        SDL_Log("Unknown shop '%s'", shopId.c_str());
        return;
    }
    shopScreen_.open(items_, sprites_, inventory_, it->second);
    mode_ = Mode::Shop;
}

void GameplayScene::dropStack(const ItemStack& stack) {
    Pickup p;
    p.stack = stack;
    p.placeAt({player_.box.center().x, player_.box.bottom()});
    p.velocity = {static_cast<float>(player_.facing) * 90.0f, -220.0f};
    p.pickupDelay = 1.0f;
    pickups_.push_back(std::move(p));
    inventoryScreen_.setMessage("Dropped. Items left behind are lost when you leave the area.");
}

void GameplayScene::useItem(const ItemStack& stack) {
    const ItemDef& def = items_.get(stack.id);
    // Health and hunger arrive in later steps; for now using an item just consumes it.
    inventoryScreen_.setMessage(def.food > 0.0f ? "You eat the " + def.name + "." : "You drink the " + def.name + ".");
}

void GameplayScene::toast(const std::string& text) {
    toasts_.push_back({text, kToastTime});
    if (toasts_.size() > 5) toasts_.erase(toasts_.begin());
}

// ---------------------------------------------------------------------------- render

void GameplayScene::renderWorldObjects(SDL_Renderer* r, Vec2 cam, bool foreground) const {
    for (const MapObject& o : area_.objects) {
        std::string sprite;
        if (o.type == "decor") sprite = o.getString("sprite");
        else if (o.type == "chest") sprite = "chest";
        if (sprite.empty() || o.getBool("foreground") != foreground) continue;
        // Sprites sit on the bottom-left corner of their object.
        const SDL_FPoint size = sprites_.size(sprite);
        sprites_.draw(r, sprite, o.rect.x - cam.x, o.rect.y + o.rect.h - size.y - cam.y, o.getBool("flip"));
    }
}

void GameplayScene::renderHud(SDL_Renderer* r) const {
    // Hands (right / left) and coins in the top-left corner.
    const SDL_FRect right{6, 6, ui::kSlotSize, ui::kSlotSize};
    const SDL_FRect left{46, 6, ui::kSlotSize, ui::kSlotSize};
    ui::slot(r, sprites_, items_, right, inventory_.equipment.at(Inventory::RightHand), false, false, "R");
    ui::slot(r, sprites_, items_, left, inventory_.equipment.at(Inventory::LeftHand), false, false, "L");
    sprites_.drawIcon(r, "coins", 88, 8);
    ui::text(r, 122, 20, std::to_string(inventory_.coins), ui::kGold);

    float y = App::kViewHeight - 16.0f;
    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        ui::text(r, 8, y, it->text, SDL_Color{255, 255, 255, static_cast<Uint8>(255 * std::min(1.0f, it->time))});
        y -= 12.0f;
    }
}

void GameplayScene::render(SDL_Renderer* renderer, float alpha) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    const SDL_Color bg = area_.background;
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    const SDL_FRect screen{0, 0, App::kViewWidth, App::kViewHeight};
    SDL_RenderFillRect(renderer, &screen);

    const Vec2 cam = camera_.view(alpha);
    area_.map.render(renderer, cam, false);
    renderWorldObjects(renderer, cam, false);
    for (const Pickup& p : pickups_) {
        p.render(renderer, sprites_, p.coins > 0 ? "coins" : items_.get(p.stack.id).icon, cam, alpha);
    }
    player_.render(renderer, cam, alpha, app_.debugDraw);
    area_.map.render(renderer, cam, true);
    renderWorldObjects(renderer, cam, true);

    if (const MapObject* o = interactableUnderPlayer(); o && mode_ == Mode::Play && fadeDirection_ == 0) {
        std::string prompt;
        if (o->type == "chest") prompt = "[E] Open chest";
        else if (o->type == "shop") prompt = "[E] Shop";
        else if (o->getString("trigger", "interact") != "touch") prompt = "[Up] Enter";
        if (!prompt.empty()) {
            ui::textCentered(renderer, player_.box.center().x - cam.x, player_.box.y - cam.y - 16, prompt);
        }
    }

    if (app_.debugDraw) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        for (const MapObject& o : area_.objects) {
            const SDL_FRect rc{o.rect.x - cam.x, o.rect.y - cam.y, std::max(o.rect.w, 4.0f), std::max(o.rect.h, 4.0f)};
            SDL_RenderRect(renderer, &rc);
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(renderer, 4, 50, "%s  pos %.1f,%.1f  vel %.0f,%.0f  %s", area_.id.c_str(),
                                  player_.box.x, player_.box.y, player_.velocity.x, player_.velocity.y,
                                  player_.contact.onGround ? "ground" : "air");
    }

    if (areaTitleTimer_ > 0.0f) {
        const float a = std::min(1.0f, areaTitleTimer_);
        ui::textCentered(renderer, App::kViewWidth * 0.5f, 40, area_.displayName,
                         SDL_Color{255, 255, 255, static_cast<Uint8>(255 * a)});
    }

    if (mode_ == Mode::Play) renderHud(renderer);

    if (mode_ == Mode::Inventory) inventoryScreen_.render(renderer);
    if (mode_ == Mode::Shop) shopScreen_.render(renderer);

    if (fade_ > 0.0f) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(255 * std::clamp(fade_, 0.0f, 1.0f)));
        SDL_RenderFillRect(renderer, &screen);
    }
}
