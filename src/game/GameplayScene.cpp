#include "game/GameplayScene.h"

#include "core/App.h"
#include "game/GameData.h"
#include "game/SaveSlots.h"
#include "game/TitleScene.h"
#include "ui/Ui.h"

#include <cstdio>
#include <sstream>

namespace {
constexpr float kFadeSpeed = 4.0f;  // full fade in 0.25s
constexpr float kAreaTitleTime = 2.5f;
constexpr float kToastTime = 2.5f;
constexpr float kDeathTime = 2.5f;
constexpr float kBannerTime = 6.0f;
constexpr float kRegenPerSecond = 0.4f;   // slow natural healing (hunger will gate this later)
constexpr float kRegenDelay = 5.0f;       // seconds after being hurt before healing starts
constexpr float kFoodHealFactor = 0.25f;  // until hunger exists, food heals a little
constexpr const char* kStartArea = "beach";
constexpr const char* kStartSpawn = "start";
constexpr float T = static_cast<float>(Tilemap::kTileSize);

enum PauseItem { Resume, SaveItem, SettingsItem, CreditsItem, AboutItem, QuitToTitle, ExitGame };

float difficultyDamageScale(Difficulty d) {
    return d == Difficulty::Easy ? 0.7f : d == Difficulty::Hard ? 1.35f : 1.0f;
}
float difficultyHealthScale(Difficulty d) {
    return d == Difficulty::Easy ? 0.8f : d == Difficulty::Hard ? 1.25f : 1.0f;
}
}  // namespace

// ============================================================================ setup & saving

GameplayScene::GameplayScene(App& app, int slot, bool newGame)
    : Scene(app), data_(app.data()), slot_(slot), rng_(std::random_device{}()) {
    player_.setAnimations(data_.anims.find("knight"));
    pauseMenu_.setItems({"Resume", "Save game", "Settings", "Credits", "About", "Save & quit to title",
                         "Save & exit game"});

    SaveGame save;
    if (!newGame && saveslots::load(slot_, save)) {
        world_ = save.world;
        character_ = save.character;
        inventory_ = save.inventory;
        playSeconds_ = save.playSeconds;
        player_.applyCharacter(character_);
        enterArea(save.area, kStartSpawn);
        player_.health = std::clamp(save.health, 1.0f, player_.maxHealth);
        if (save.hasPosition) {
            player_.placeAt({save.x, save.y});
            camera_.snapTo(player_.box.center(), area_.map.pixelWidth(), area_.map.pixelHeight(), App::kViewWidth,
                           App::kViewHeight);
        }
        toast("Welcome back, " + character_.name + ".");
        return;
    }

    // New expedition (or a dev game from the command line).
    newExplorer();
    const App::Options& opt = app.options();
    if (slot_ == kDevSlot) {
        std::stringstream give(opt.giveItems);
        for (std::string id; std::getline(give, id, ',');) {
            const ItemDef* def = data_.items.find(id);
            if (!def) {
                SDL_Log("--give: unknown item '%s'", id.c_str());
                continue;
            }
            // Weapons go straight into empty hands (melee right, ranged left) for quick testing.
            const std::size_t hand = def->isRanged() ? Inventory::LeftHand : Inventory::RightHand;
            if (def->slot == EquipSlot::Hand && inventory_.equipment.at(hand).empty()) {
                inventory_.equipment.at(hand) = {id, 1};
            } else {
                inventory_.add(data_.items, id, def->maxStack > 1 ? def->maxStack : 1);
            }
        }
    }
    const bool devStart = slot_ == kDevSlot && !opt.startArea.empty();
    enterArea(devStart ? opt.startArea : kStartArea, devStart ? opt.startSpawn : kStartSpawn);
    banner_ = {character_.name + " arrives at the beach.",
               "Health " + std::to_string(static_cast<int>(character_.maxHealth)) + "   Coins " +
                   std::to_string(inventory_.coins)};
    bannerTimer_ = kBannerTime;

    if (slot_ == kDevSlot) {
        if (opt.openUi == "inventory") openInventory(nullptr);
        else if (opt.openUi == "chest") openInventory(&world_.chest);
        else if (opt.openUi == "shop") openShop("beach_shop");
        else if (opt.openUi == "pause") openPause();
    }
}

GameplayScene::~GameplayScene() {
    if (mode_ == Mode::Inventory) closeInventory();  // put corpse items back before saving
    saveGame();
}

void GameplayScene::newExplorer() {
    character_ = Character::roll(data_.characterConfig, rng_, world_.explorersLost + 1);
    inventory_ = Inventory();
    inventory_.coins = character_.startingCoins;
    player_.applyCharacter(character_);
    player_.health = player_.maxHealth;
}

void GameplayScene::saveGame() {
    if (slot_ == kDevSlot) return;
    SaveGame s;
    s.world = world_;
    s.character = character_;
    s.inventory = inventory_;
    s.health = player_.dead() ? player_.maxHealth : player_.health;
    s.area = area_.id;
    s.areaName = area_.displayName;
    s.x = player_.box.center().x;
    s.y = player_.box.bottom();
    s.hasPosition = !player_.dead();
    s.playSeconds = playSeconds_;
    if (!saveslots::write(slot_, s)) toast("Saving failed!");
}

void GameplayScene::handleDeath() {
    // Everything the explorer carried stays where they fell, resting on the ground below.
    CorpsePile pile;
    pile.area = area_.id;
    pile.x = player_.box.center().x;
    pile.y = player_.box.bottom();
    const int col = static_cast<int>(std::floor(pile.x / T));
    for (int row = static_cast<int>(std::floor((pile.y - 1.0f) / T)); row < area_.map.height; ++row) {
        if (area_.map.collisionAt(col, row) != TileCollision::None) {
            pile.y = static_cast<float>(row) * T;
            break;
        }
    }
    pile.explorer = character_.name;
    pile.coins = inventory_.coins;
    for (Container* c : {&inventory_.equipment, &inventory_.backpack}) {
        for (std::size_t i = 0; i < c->size(); ++i) {
            if (!c->at(i).empty()) pile.items.push_back(c->at(i));
        }
    }
    if (!pile.empty()) world_.corpses.push_back(std::move(pile));
    world_.explorersLost++;

    const std::string fallen = character_.name;
    newExplorer();
    mode_ = Mode::Play;
    fade_ = 1.0f;
    fadeDirection_ = -1;
    enterArea(kStartArea, kStartSpawn);
    char stats[96];
    std::snprintf(stats, sizeof stats, "Health %d  Strength %.2f  Speed %.2f  Jump %.2f",
                  static_cast<int>(character_.maxHealth), character_.strength, character_.speed, character_.jump);
    banner_ = {fallen + " was lost in the caves.", character_.name + " (explorer #" +
                                                       std::to_string(character_.number) + ") arrives at the beach.",
               stats, "Their belongings wait where they fell."};
    bannerTimer_ = kBannerTime;
    saveGame();
}

// ============================================================================ areas

void GameplayScene::enterArea(const std::string& areaId, const std::string& spawn) {
    // Areas are always loaded fresh from disk: enemies, obstacles and loot reset.
    if (!area_.load(areaId, app_.assets())) {
        SDL_Log("Failed to load area '%s'", areaId.c_str());
        if (areaId != kStartArea) enterArea(kStartArea, kStartSpawn);
        return;
    }
    const MapObject* sp = area_.findObject("spawn", spawn);
    if (!sp) sp = area_.findObject("spawn", kStartSpawn);
    if (!sp) {
        SDL_Log("Area '%s' has no spawn '%s'", areaId.c_str(), spawn.c_str());
        player_.placeAt({area_.map.pixelWidth() * 0.5f, 0.0f});
    } else {
        player_.placeAt({sp->rect.x, sp->rect.y});
    }
    camera_.snapTo(player_.box.center(), area_.map.pixelWidth(), area_.map.pixelHeight(), App::kViewWidth,
                   App::kViewHeight);
    areaTitleTimer_ = kAreaTitleTime;
    app_.audio().playMusic(area_.music);
    spawnAreaObjects();
    saveGame();  // autosave on every area change
}

void GameplayScene::spawnAreaObjects() {
    pickups_.clear();
    enemies_.clear();
    projectiles_.clear();
    const Difficulty diff = app_.settings().difficulty;

    for (const MapObject& o : area_.objects) {
        if (o.type == "pickup") {
            Pickup p;
            p.uniqueId = o.getString("unique_id");
            if (!p.uniqueId.empty() && world_.takenUniques.count(p.uniqueId)) continue;
            const std::string item = o.getString("item");
            if (item == "coins") {
                p.coins = static_cast<int>(o.getFloat("count", 1));
            } else {
                if (!data_.items.find(item)) {
                    SDL_Log("%s: pickup %d has unknown item '%s'", area_.id.c_str(), o.id, item.c_str());
                    continue;
                }
                p.stack = {item, std::max(1, static_cast<int>(o.getFloat("count", 1)))};
            }
            // Point objects mark the bottom-centre; rectangles use their bottom edge.
            p.placeAt({o.rect.x + o.rect.w * 0.5f, o.rect.y + o.rect.h});
            pickups_.push_back(std::move(p));
        } else if (o.type == "enemy") {
            // Per-difficulty overrides: a different creature, or "none" to leave it out.
            std::string type = o.getString("enemy", o.name);
            const char* key = diff == Difficulty::Easy ? "enemy_easy" : diff == Difficulty::Hard ? "enemy_hard" : nullptr;
            if (key && !o.getString(key).empty()) type = o.getString(key);
            if (type.empty() || type == "none") continue;
            auto it = data_.enemies.find(type);
            if (it == data_.enemies.end()) {
                SDL_Log("%s: enemy %d has unknown type '%s'", area_.id.c_str(), o.id, type.c_str());
                continue;
            }
            Enemy e;
            e.init(it->second, data_.anims, {o.rect.x + o.rect.w * 0.5f, o.rect.y + o.rect.h},
                   o.getFloat("hp_scale", 1.0f) * difficultyHealthScale(diff));
            enemies_.push_back(std::move(e));
        }
    }
}

void GameplayScene::requestTransition(const std::string& areaId, const std::string& spawn) {
    if (fadeDirection_ == 1) return;
    pendingArea_ = areaId;
    pendingSpawn_ = spawn;
    fadeDirection_ = 1;
}

// ============================================================================ update

void GameplayScene::update(float dt) {
    for (Toast& t : toasts_) t.time -= dt;
    std::erase_if(toasts_, [](const Toast& t) { return t.time <= 0.0f; });
    messageCooldown_ = std::max(0.0f, messageCooldown_ - dt);

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
        if (req.kind == InventoryRequest::Kind::Close) closeInventory();
        else if (req.kind == InventoryRequest::Kind::Drop) dropStack(req.stack);
        else if (req.kind == InventoryRequest::Kind::Use) useItem(req.stack);
        return;  // the world is paused while a menu is open
    }
    case Mode::Shop:
        if (!shopScreen_.update(in)) mode_ = Mode::Play;
        return;
    case Mode::Pause:
        updatePause();
        return;
    case Mode::Settings:
        if (settingsScreen_.update() == SettingsScreen::Result::Close) mode_ = Mode::Pause;
        return;
    case Mode::Text:
        if (!textScreen_.update(in)) mode_ = textReturnMode_;
        return;
    case Mode::Dead:
        deathTimer_ -= dt;
        player_.update(dt, in, area_.map);
        updateEnemies(dt);
        camera_.update(dt, player_.box.center(), area_.map.pixelWidth(), area_.map.pixelHeight(), App::kViewWidth,
                       App::kViewHeight);
        if (deathTimer_ <= 0.0f) handleDeath();
        return;
    case Mode::Play:
        updatePlay(dt);
        return;
    }
}

void GameplayScene::updatePlay(float dt) {
    if (fadeDirection_ == 1) return;  // freeze while the screen goes dark
    playSeconds_ += dt;
    bannerTimer_ = std::max(0.0f, bannerTimer_ - dt);

    const Input& in = app_.input();
    if (fadeDirection_ == 0) {
        if (in.pressed(Action::Pause)) { openPause(); return; }
        if (in.pressed(Action::Inventory)) { openInventory(nullptr); return; }
    }

    const float healthBefore = player_.health;
    player_.update(dt, in, area_.map);
    updateAttacks();
    updateProjectiles(dt);
    updateEnemies(dt);
    updatePickups(dt);
    camera_.update(dt, player_.box.center(), area_.map.pixelWidth(), area_.map.pixelHeight(), App::kViewWidth,
                   App::kViewHeight);
    areaTitleTimer_ = std::max(0.0f, areaTitleTimer_ - dt);

    // Slow natural healing once you've been out of danger for a while.
    if (player_.health < healthBefore) regenDelay_ = kRegenDelay;
    regenDelay_ = std::max(0.0f, regenDelay_ - dt);
    if (regenDelay_ <= 0.0f && !player_.dead()) {
        player_.health = std::min(player_.maxHealth, player_.health + kRegenPerSecond * dt);
    }

    if (player_.dead()) {
        mode_ = Mode::Dead;
        deathTimer_ = kDeathTime;
        return;
    }
    if (fadeDirection_ == 0) handleInteraction();
}

void GameplayScene::updateAttacks() {
    const Input& in = app_.input();
    const std::pair<Action, std::size_t> hands[] = {{Action::UseRight, Inventory::RightHand},
                                                    {Action::UseLeft, Inventory::LeftHand}};
    for (const auto& [action, slot] : hands) {
        if (!in.pressed(action) || !player_.canAttack()) continue;
        const ItemStack& held = inventory_.equipment.at(slot);
        const ItemDef* def = held.empty() ? nullptr : &data_.items.get(held.id);

        if (def && def->isRanged()) {
            // Find ammo in the backpack.
            std::size_t ammoSlot = inventory_.backpack.size();
            for (std::size_t i = 0; i < inventory_.backpack.size(); ++i) {
                if (inventory_.backpack.at(i).id == def->ammo) ammoSlot = i;
            }
            if (ammoSlot == inventory_.backpack.size()) {
                if (messageCooldown_ <= 0.0f) {
                    toast("No " + data_.items.get(def->ammo).name + " in your pack");
                    messageCooldown_ = 1.5f;
                }
                continue;
            }
            inventory_.backpack.takeFrom(ammoSlot, 1);
            Projectile p;
            p.icon = data_.items.get(def->ammo).icon;
            p.damage = static_cast<float>(def->damage) * character_.strength;
            p.attackId = nextProjectileId_++;
            const float f = static_cast<float>(player_.facing);
            p.launch(player_.box.center() + Vec2{f * 10.0f, -6.0f}, {f * def->projectileSpeed, -140.0f});
            projectiles_.push_back(p);
            player_.startAttack(0.0f, 0.0f, def->cooldown);
        } else if (def && def->isMelee()) {
            player_.startAttack(def->reach, static_cast<float>(def->damage) * character_.strength, def->cooldown);
        } else {
            // Bare hands (or a lantern/shield/food): a weak shove.
            player_.startAttack(16.0f, 1.0f * character_.strength, 0.4f);
        }
    }

    Rect swing;
    if (player_.swingHitbox(swing)) {
        for (Enemy& e : enemies_) {
            if (e.box.overlaps(swing)) e.hit(player_.swingDamage(), player_.box.center().x, player_.attackId());
        }
    }
}

void GameplayScene::updateProjectiles(float dt) {
    for (Projectile& p : projectiles_) {
        p.update(dt, area_.map);
        if (p.dead) continue;
        for (Enemy& e : enemies_) {
            if (!e.dead && e.box.overlaps(p.box) && e.hit(p.damage, p.box.center().x, p.attackId)) {
                p.dead = true;
                break;
            }
        }
    }
    std::erase_if(projectiles_, [](const Projectile& p) { return p.dead; });
}

void GameplayScene::updateEnemies(float dt) {
    const float damageScale = difficultyDamageScale(app_.settings().difficulty);
    for (Enemy& e : enemies_) {
        e.update(dt, area_.map, player_.box, !player_.dead());
        if (!e.dead && e.touchCooldown <= 0.0f && e.box.overlaps(player_.box) && mode_ == Mode::Play) {
            if (player_.hurt(e.def->damage * damageScale, e.box.center().x)) {
                e.touchedPlayer();
                if (app_.debugDraw) SDL_Log("%s hit the explorer: health %.0f", e.def->name.c_str(), player_.health);
            }
        }
        if (e.dead) {
            if (app_.debugDraw) SDL_Log("killed %s", e.def->name.c_str());
            spawnDrops(e);
        }
    }
    std::erase_if(enemies_, [](const Enemy& e) { return e.dead; });
}

void GameplayScene::spawnDrops(const Enemy& e) {
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    for (const EnemyDrop& d : e.def->drops) {
        if (roll(rng_) > d.chance) continue;
        const int count = std::uniform_int_distribution<int>(d.min, std::max(d.min, d.max))(rng_);
        Pickup p;
        if (d.item == "coins") p.coins = count;
        else p.stack = {d.item, count};
        p.placeAt({e.box.center().x, e.box.center().y + Pickup::kSize * 0.5f});
        p.velocity = {(roll(rng_) - 0.5f) * 160.0f, -200.0f - roll(rng_) * 80.0f};
        p.pickupDelay = 0.3f;
        pickups_.push_back(std::move(p));
    }
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
            const ItemDef& def = data_.items.get(p.stack.id);
            const int left = inventory_.add(data_.items, p.stack.id, p.stack.count);
            const int taken = p.stack.count - left;
            if (taken > 0) toast(taken > 1 ? def.name + " x" + std::to_string(taken) : def.name);
            p.stack.count = left;
            if (left == 0) {
                p.removed = true;
            } else if (messageCooldown_ <= 0.0f) {
                toast("Backpack full");
                messageCooldown_ = 2.0f;
            }
        }
        if (p.removed && !p.uniqueId.empty()) {
            world_.takenUniques.insert(p.uniqueId);
            saveGame();
        }
    }
    std::erase_if(pickups_, [](const Pickup& p) { return p.removed; });
}

Rect GameplayScene::corpseRect(const CorpsePile& c) const { return {c.x - 16.0f, c.y - 28.0f, 32.0f, 28.0f}; }

GameplayScene::Interactable GameplayScene::findInteractable() const {
    Interactable found;
    for (std::size_t i = 0; i < world_.corpses.size(); ++i) {
        const CorpsePile& c = world_.corpses[i];
        if (c.area == area_.id && corpseRect(c).overlaps(player_.box)) {
            found.kind = Interactable::Kind::Corpse;
            found.corpse = static_cast<int>(i);
            return found;
        }
    }
    for (const MapObject& o : area_.objects) {
        if (!o.rect.overlaps(player_.box)) continue;
        using K = Interactable::Kind;
        if (o.type == "chest") found.kind = K::Chest;
        else if (o.type == "shop") found.kind = K::Shop;
        else if (o.type == "door") found.kind = o.getString("trigger", "interact") == "touch" ? K::TouchDoor : K::Door;
        else continue;
        found.object = &o;
        return found;
    }
    return found;
}

void GameplayScene::handleInteraction() {
    const Interactable it = findInteractable();
    const Input& in = app_.input();
    const bool pressed = in.pressed(Action::Up) || in.pressed(Action::Interact);
    using K = Interactable::Kind;
    switch (it.kind) {
    case K::TouchDoor:
        requestTransition(it.object->getString("target_area"), it.object->getString("target_spawn"));
        break;
    case K::Door:
        if (pressed) requestTransition(it.object->getString("target_area"), it.object->getString("target_spawn"));
        break;
    case K::Chest:
        if (pressed) openInventory(&world_.chest);
        break;
    case K::Shop:
        if (pressed) openShop(it.object->getString("shop_id"));
        break;
    case K::Corpse:
        if (pressed) openCorpse(it.corpse);
        break;
    case K::None:
        break;
    }
}

// ============================================================================ menus

void GameplayScene::openPause() {
    pauseMenu_.setItems({"Resume", slot_ == kDevSlot ? "Save game (dev game: off)" : "Save game", "Settings", "Credits",
                         "About", "Save & quit to title", "Save & exit game"});
    pauseMessage_.clear();
    mode_ = Mode::Pause;
}

void GameplayScene::updatePause() {
    const int choice = pauseMenu_.update(app_.input());
    switch (choice) {
    case Menu::kCancelled:
    case Resume:
        mode_ = Mode::Play;
        break;
    case SaveItem:
        saveGame();
        pauseMessage_ = slot_ == kDevSlot ? "Dev games are not saved." : "Saved to slot " + std::to_string(slot_) + ".";
        break;
    case SettingsItem:
        settingsScreen_.open(app_);
        mode_ = Mode::Settings;
        break;
    case CreditsItem:
        textScreen_.open("Credits", data_.creditsText);
        textReturnMode_ = Mode::Pause;
        mode_ = Mode::Text;
        break;
    case AboutItem:
        textScreen_.open("About", aboutText());
        textReturnMode_ = Mode::Pause;
        mode_ = Mode::Text;
        break;
    case QuitToTitle:
        // Saving happens in the destructor when the scene is replaced.
        app_.setScene(std::make_unique<TitleScene>(app_));
        break;
    case ExitGame:
        app_.quit();
        break;
    default:
        break;
    }
}

void GameplayScene::openInventory(Container* chest, const std::string& title) {
    chestOpen_ = chest != nullptr;
    inventoryScreen_.open(data_.items, data_.sprites, inventory_, chest, title);
    mode_ = Mode::Inventory;
}

void GameplayScene::openCorpse(int index) {
    CorpsePile& pile = world_.corpses[static_cast<std::size_t>(index)];
    if (pile.coins > 0) {
        inventory_.coins += pile.coins;
        toast("+" + std::to_string(pile.coins) + " coins");
        pile.coins = 0;
    }
    corpseContainer_ = Container(std::max<std::size_t>(1, pile.items.size()));
    for (std::size_t i = 0; i < pile.items.size(); ++i) corpseContainer_.at(i) = pile.items[i];
    openCorpse_ = index;
    openInventory(&corpseContainer_, "Remains of " + pile.explorer);
}

void GameplayScene::closeInventory() {
    mode_ = Mode::Play;
    if (openCorpse_ >= 0) {
        CorpsePile& pile = world_.corpses[static_cast<std::size_t>(openCorpse_)];
        pile.items.clear();
        for (std::size_t i = 0; i < corpseContainer_.size(); ++i) {
            if (!corpseContainer_.at(i).empty()) pile.items.push_back(corpseContainer_.at(i));
        }
        if (pile.empty()) {
            toast("You gather what remains of " + pile.explorer + ".");
            world_.corpses.erase(world_.corpses.begin() + openCorpse_);
        }
        openCorpse_ = -1;
        saveGame();
    } else if (chestOpen_) {
        saveGame();
    }
    chestOpen_ = false;
}

void GameplayScene::openShop(const std::string& shopId) {
    auto it = data_.shops.find(shopId);
    if (it == data_.shops.end()) {
        SDL_Log("Unknown shop '%s'", shopId.c_str());
        return;
    }
    shopScreen_.open(data_.items, data_.sprites, inventory_, it->second);
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
    const ItemDef& def = data_.items.get(stack.id);
    const float heal = def.heal + def.food * kFoodHealFactor;
    player_.health = std::min(player_.maxHealth, player_.health + heal);
    inventoryScreen_.setMessage((def.food > 0.0f ? "You eat the " : "You drink the ") + def.name + ".");
}

void GameplayScene::toast(const std::string& text) {
    toasts_.push_back({text, kToastTime});
    if (toasts_.size() > 5) toasts_.erase(toasts_.begin());
}

// ============================================================================ render

void GameplayScene::renderWorldObjects(SDL_Renderer* r, Vec2 cam, bool foreground) const {
    for (const MapObject& o : area_.objects) {
        std::string sprite;
        if (o.type == "decor") sprite = o.getString("sprite");
        else if (o.type == "chest") sprite = "chest";
        if (sprite.empty() || o.getBool("foreground") != foreground) continue;
        // Sprites sit on the bottom-left corner of their object.
        const SDL_FPoint size = data_.sprites.size(sprite);
        data_.sprites.draw(r, sprite, o.rect.x - cam.x, o.rect.y + o.rect.h - size.y - cam.y, o.getBool("flip"));
    }
    if (foreground) return;
    for (const CorpsePile& c : world_.corpses) {
        if (c.area != area_.id) continue;
        data_.sprites.drawIcon(r, "remains", c.x - 16.0f - cam.x, c.y - 30.0f - cam.y);
    }
}

void GameplayScene::renderHud(SDL_Renderer* r) const {
    // Hands (right / left), health, coins and the explorer's name in the top-left corner.
    const SDL_FRect right{6, 6, ui::kSlotSize, ui::kSlotSize};
    const SDL_FRect left{46, 6, ui::kSlotSize, ui::kSlotSize};
    for (const auto& [rect, slot, label] : {std::tuple{right, Inventory::RightHand, "R"},
                                            std::tuple{left, Inventory::LeftHand, "L"}}) {
        const ItemStack& held = inventory_.equipment.at(slot);
        ui::slot(r, data_.sprites, data_.items, rect, held, false, false, label);
        if (!held.empty() && data_.items.get(held.id).isRanged()) {
            const std::string ammo = std::to_string(inventory_.backpack.countOf(data_.items.get(held.id).ammo));
            ui::text(r, rect.x + rect.w - 2 - ui::textWidth(ammo), rect.y + rect.h - 9, ammo, ui::kGold);
        }
    }

    const SDL_FRect barBack{88, 8, 120, 10};
    SDL_SetRenderDrawColor(r, 30, 10, 10, 255);
    SDL_RenderFillRect(r, &barBack);
    const float frac = std::clamp(player_.health / player_.maxHealth, 0.0f, 1.0f);
    const SDL_FRect bar{barBack.x + 1, barBack.y + 1, (barBack.w - 2) * frac, barBack.h - 2};
    SDL_SetRenderDrawColor(r, static_cast<Uint8>(200 - 80 * frac), static_cast<Uint8>(40 + 140 * frac), 50, 255);
    SDL_RenderFillRect(r, &bar);
    SDL_SetRenderDrawColor(r, 120, 98, 70, 255);
    SDL_RenderRect(r, &barBack);
    ui::text(r, barBack.x + barBack.w + 6, barBack.y + 1,
             std::to_string(static_cast<int>(std::ceil(player_.health))) + "/" +
                 std::to_string(static_cast<int>(player_.maxHealth)));

    data_.sprites.drawIcon(r, "coins", 86, 20, 24);
    ui::text(r, 112, 30, std::to_string(inventory_.coins), ui::kGold);
    ui::text(r, App::kViewWidth - 8 - ui::textWidth(character_.name), 8, character_.name, ui::kDim);

    float y = App::kViewHeight - 16.0f;
    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        ui::text(r, 8, y, it->text, SDL_Color{255, 255, 255, static_cast<Uint8>(255 * std::min(1.0f, it->time))});
        y -= 12.0f;
    }
}

void GameplayScene::renderBanner(SDL_Renderer* r) const {
    if (bannerTimer_ <= 0.0f || banner_.empty()) return;
    const float a = std::min(1.0f, bannerTimer_);
    const float h = 16.0f + static_cast<float>(banner_.size()) * 13.0f;
    const SDL_FRect box{100, 250, 440, h};
    SDL_SetRenderDrawColor(r, 20, 16, 24, static_cast<Uint8>(220 * a));
    SDL_RenderFillRect(r, &box);
    for (std::size_t i = 0; i < banner_.size(); ++i) {
        const SDL_Color c = i == 0 ? ui::kGold : ui::kText;
        ui::textCentered(r, box.x + box.w * 0.5f, box.y + 9 + static_cast<float>(i) * 13.0f, banner_[i],
                         SDL_Color{c.r, c.g, c.b, static_cast<Uint8>(255 * a)});
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
        p.render(renderer, data_.sprites, p.coins > 0 ? "coins" : data_.items.get(p.stack.id).icon, cam, alpha);
    }
    for (const Enemy& e : enemies_) e.render(renderer, data_.sprites, cam, alpha);
    player_.render(renderer, cam, alpha, app_.debugDraw);
    for (const Projectile& p : projectiles_) p.render(renderer, data_.sprites, cam, alpha);
    area_.map.render(renderer, cam, true);
    renderWorldObjects(renderer, cam, true);

    if (mode_ == Mode::Play && fadeDirection_ == 0) {
        const Interactable it = findInteractable();
        std::string prompt;
        using K = Interactable::Kind;
        if (it.kind == K::Chest) prompt = "[E] Open chest";
        else if (it.kind == K::Shop) prompt = "[E] Shop";
        else if (it.kind == K::Door) prompt = "[Up] Enter";
        else if (it.kind == K::Corpse) prompt = "[E] Search " + world_.corpses[static_cast<std::size_t>(it.corpse)].explorer;
        if (!prompt.empty()) {
            ui::textCentered(renderer, player_.box.center().x - cam.x, player_.box.y - cam.y - 22, prompt);
        }
    }

    if (app_.debugDraw) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        for (const MapObject& o : area_.objects) {
            const SDL_FRect rc{o.rect.x - cam.x, o.rect.y - cam.y, std::max(o.rect.w, 4.0f), std::max(o.rect.h, 4.0f)};
            SDL_RenderRect(renderer, &rc);
        }
        SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
        for (const Enemy& e : enemies_) {
            const SDL_FRect rc{e.box.x - cam.x, e.box.y - cam.y, e.box.w, e.box.h};
            SDL_RenderRect(renderer, &rc);
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(renderer, 4, 50, "%s  pos %.1f,%.1f  vel %.0f,%.0f  %s", area_.id.c_str(),
                                  player_.box.x, player_.box.y, player_.velocity.x, player_.velocity.y,
                                  player_.contact.onGround ? "ground" : "air");
    }

    if (areaTitleTimer_ > 0.0f) {
        const float a = std::min(1.0f, areaTitleTimer_);
        ui::textCentered(renderer, App::kViewWidth * 0.5f, 52, area_.displayName,
                         SDL_Color{255, 255, 255, static_cast<Uint8>(255 * a)});
    }

    if (mode_ == Mode::Dead) {
        const float t = 1.0f - std::max(0.0f, deathTimer_) / kDeathTime;
        SDL_SetRenderDrawColor(renderer, 60, 0, 0, static_cast<Uint8>(200 * t));
        SDL_RenderFillRect(renderer, &screen);
        ui::textCentered(renderer, App::kViewWidth * 0.5f, 160, character_.name + " has fallen.",
                         SDL_Color{255, 220, 220, static_cast<Uint8>(255 * std::min(1.0f, t * 2.0f))});
    }

    if (mode_ == Mode::Play || mode_ == Mode::Dead) renderHud(renderer);
    if (mode_ == Mode::Play) renderBanner(renderer);

    switch (mode_) {
    case Mode::Inventory: inventoryScreen_.render(renderer); break;
    case Mode::Shop: shopScreen_.render(renderer); break;
    case Mode::Pause: {
        ui::dimScreen(renderer, 150);
        const SDL_FRect panel{200, 60, 240, 210};
        ui::panel(renderer, panel, "Paused");
        pauseMenu_.render(renderer, panel.x + panel.w * 0.5f, panel.y + 34);
        if (!pauseMessage_.empty()) ui::textCentered(renderer, panel.x + panel.w * 0.5f, panel.y + panel.h - 18, pauseMessage_, ui::kGood);
        break;
    }
    case Mode::Settings: settingsScreen_.render(renderer); break;
    case Mode::Text: textScreen_.render(renderer); break;
    default: break;
    }

    if (fade_ > 0.0f) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(255 * std::clamp(fade_, 0.0f, 1.0f)));
        SDL_RenderFillRect(renderer, &screen);
    }
}
