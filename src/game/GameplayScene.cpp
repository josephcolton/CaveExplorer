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
constexpr const char* kStartArea = "beach";
constexpr const char* kStartSpawn = "start";
constexpr float T = static_cast<float>(Tilemap::kTileSize);

enum PauseItem { Resume, RestartItem, SaveItem, SettingsItem, CreditsItem, AboutItem, QuitToTitle, ExitGame };

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
    pauseMenu_.setItems({"Resume", "Restart area", "Save game", "Settings", "Credits", "About", "Save & quit to title",
                         "Save & exit game"});

    SaveGame save;
    if (!newGame && saveslots::load(slot_, save)) {
        world_ = save.world;
        character_ = save.character;
        inventory_ = save.inventory;
        takenPickups_ = save.takenPickups;
        abilities_ = save.abilities;
        hunger_ = save.hunger;
        buffs_ = save.buffs;
        quests_ = save.quests;
        playSeconds_ = save.playSeconds;
        refreshStats();
        enterArea(save.area, save.entrySpawn);
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
            // Gear goes straight into its empty equipment slot (melee right hand, ranged left).
            std::size_t slot = Inventory::EquipCount;
            switch (def->slot) {
            case EquipSlot::Hand: slot = def->isRanged() ? Inventory::LeftHand : Inventory::RightHand; break;
            case EquipSlot::Feet: slot = Inventory::Feet; break;
            case EquipSlot::Body: slot = Inventory::Body; break;
            case EquipSlot::Head: slot = Inventory::Head; break;
            default: break;
            }
            if (slot != Inventory::EquipCount && inventory_.equipment.at(slot).empty()) {
                inventory_.equipment.at(slot) = {id, 1};
            } else {
                inventory_.add(data_.items, id, def->maxStack > 1 ? def->maxStack : 1);
            }
        }
    }
    if (slot_ == kDevSlot && opt.hunger >= 0.0f) hunger_ = std::min(hunger::kMax, opt.hunger);
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
    takenPickups_.clear();  // freebies return for the new explorer
    abilities_.clear();     // abilities and quest progress die with the explorer
    quests_.clear();
    hunger_ = hunger::kMax;
    buffs_.clear();
    player_.health = 1.0f;  // refreshStats clamps; set full health after
    refreshStats();
    player_.health = player_.maxHealth;
}

void GameplayScene::saveGame() {
    if (slot_ == kDevSlot) return;
    SaveGame s;
    s.world = world_;
    s.character = character_;
    s.inventory = inventory_;
    s.takenPickups = takenPickups_;
    s.abilities = abilities_;
    s.quests = quests_;
    s.hunger = hunger_;
    s.buffs = buffs_;
    s.health = player_.dead() ? player_.maxHealth : player_.health;
    s.area = area_.id;
    s.areaName = area_.displayName;
    s.entrySpawn = entrySpawn_;
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
    if (!sp) {  // fall back to the first spawn point in the area
        for (const MapObject& o : area_.objects) {
            if (o.type == "spawn") {
                sp = &o;
                break;
            }
        }
    }
    entrySpawn_ = sp ? sp->name : spawn;
    int tileX = 0, tileY = 0;
    if (std::sscanf(spawn.c_str(), "%d,%d", &tileX, &tileY) == 2) {
        // Debug: "--spawn tx,ty" stands the player on top of tile row ty in column tx.
        player_.placeAt({(static_cast<float>(tileX) + 0.5f) * T, static_cast<float>(tileY) * T});
    } else if (!sp) {
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
    obstacles_.clear();
    shrines_.clear();
    particles_.clear();
    const Difficulty diff = app_.settings().difficulty;

    for (const MapObject& o : area_.objects) {
        if (o.type == "pickup") {
            Pickup p;
            p.uniqueId = o.getString("unique_id");
            if (!p.uniqueId.empty() && world_.takenUniques.count(p.uniqueId)) continue;
            p.placedKey = area_.id + "#" + std::to_string(o.id);
            if (takenPickups_.count(p.placedKey)) continue;
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
        } else if (o.type == "shrine") {
            auto it = data_.shrines.find(o.getString("shrine_id"));
            if (it == data_.shrines.end()) {
                SDL_Log("%s: shrine %d has unknown shrine_id", area_.id.c_str(), o.id);
                continue;
            }
            shrines_.push_back({&it->second, {o.rect.x, o.rect.y + o.rect.h}});
        } else if (o.type == "obstacle") {
            auto it = data_.obstacles.find(o.getString("obstacle", o.name));
            if (it == data_.obstacles.end()) {
                SDL_Log("%s: obstacle %d has unknown type '%s'", area_.id.c_str(), o.id, o.name.c_str());
                continue;
            }
            Obstacle ob;
            ob.def = &it->second;
            ob.tx = static_cast<int>(std::round(o.rect.x / T));
            ob.ty = static_cast<int>(std::round(o.rect.y / T));
            ob.tw = std::max(1, static_cast<int>(std::round(o.rect.w / T)));
            ob.th = std::max(1, static_cast<int>(std::round(o.rect.h / T)));
            ob.hitsLeft = ob.def->hits;
            const std::string side = o.getString("passable_from");
            ob.passableFrom = side == "left" ? -1 : side == "right" ? 1 : 0;
            ob.block(area_.map, true);
            obstacles_.push_back(ob);
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
        else if (req.kind == InventoryRequest::Kind::Use && useItem(req.stack) && req.container) {
            req.container->takeFrom(req.index, 1);
        }
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
    case Mode::Dialogue:
        updateDialogue();
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
    refreshStats();  // gear can change at any time (pickups, menus)
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
    for (Obstacle& ob : obstacles_) ob.update(dt);
    updateObstaclePushing(dt);
    // Safety net: falling out of the level puts you back at the entrance, a little bruised.
    if (player_.box.top() > area_.map.pixelHeight() + 64.0f && fadeDirection_ == 0) {
        player_.hurt(10.0f, player_.box.center().x);
        restartArea("You climb back up to the entrance.");
    }
    for (Particle& p : particles_) {
        p.vel.y += 600.0f * dt;
        p.pos += p.vel * dt;
        p.life -= dt;
    }
    std::erase_if(particles_, [](const Particle& p) { return p.life <= 0.0f; });
    camera_.update(dt, player_.box.center(), area_.map.pixelWidth(), area_.map.pixelHeight(), App::kViewWidth,
                   App::kViewHeight);
    areaTitleTimer_ = std::max(0.0f, areaTitleTimer_ - dt);

    // Slow natural healing once you've been out of danger for a while.
    if (player_.health < healthBefore) regenDelay_ = kRegenDelay;
    regenDelay_ = std::max(0.0f, regenDelay_ - dt);
    updateSurvival(dt);
    if (regenDelay_ <= 0.0f && !player_.dead() && hunger::canHeal(hunger_)) {
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
            p.damage = static_cast<float>(def->damage) * character_.strength * currentModifiers().damage;
            p.attackId = nextProjectileId_++;
            const float f = static_cast<float>(player_.facing);
            p.launch(player_.box.center() + Vec2{f * 10.0f, -6.0f}, {f * def->projectileSpeed, -140.0f});
            projectiles_.push_back(p);
            player_.startAttack(0.0f, 0.0f, def->cooldown);
        } else if (def && def->isMelee()) {
            swingItem_ = held.id;
            player_.startAttack(def->reach,
                                static_cast<float>(def->damage) * character_.strength * currentModifiers().damage,
                                def->cooldown);
        } else {
            // Bare hands (or a lantern/shield/food): a weak shove.
            swingItem_ = held.empty() ? "" : held.id;
            player_.startAttack(16.0f, 1.0f * character_.strength, 0.4f);
        }
    }

    Rect swing;
    if (player_.swingHitbox(swing)) {
        for (Enemy& e : enemies_) {
            if (e.box.overlaps(swing)) e.hit(player_.swingDamage(), player_.box.center().x, player_.attackId());
        }
        hitObstacles();
    }
}

void GameplayScene::hitObstacles() {
    Rect swing;
    if (!player_.swingHitbox(swing)) return;
    for (Obstacle& ob : obstacles_) {
        if (ob.cleared || ob.lastAttackId == player_.attackId() || !ob.rect().overlaps(swing)) continue;
        ob.lastAttackId = player_.attackId();
        ob.shake = 0.25f;
        const Rect r = ob.rect();
        const Vec2 contact{player_.facing > 0 ? r.left() : r.right(), player_.box.center().y};
        const ItemDef* tool = swingItem_.empty() ? nullptr : data_.items.find(swingItem_);
        if (!tool || !tool->hasTag(ob.def->requiredTag)) {
            burst(contact, {200, 200, 200, 255}, 3);
            if (messageCooldown_ <= 0.0f) {
                toast(obstacleHint(ob));
                messageCooldown_ = 2.0f;
            }
            continue;
        }
        burst(contact, ob.def->debris, 6);
        if (--ob.hitsLeft <= 0) {
            ob.cleared = true;
            ob.block(area_.map, false);
            for (int i = 0; i < ob.tw * ob.th; ++i) {
                burst({r.x + T * (0.5f + static_cast<float>(i % ob.tw)), r.y + T * (0.5f + static_cast<float>(i / ob.tw))},
                      ob.def->debris, 10);
            }
            toast(ob.def->cleared);
        }
    }
}

void GameplayScene::burst(Vec2 at, SDL_Color color, int count) {
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (int i = 0; i < count; ++i) {
        particles_.push_back({at, {u(rng_) * 140.0f, -60.0f + u(rng_) * 140.0f}, 0.5f + 0.3f * u(rng_), color});
    }
}

const Obstacle* GameplayScene::obstacleNearPlayer() const {
    Rect reach = player_.box;
    reach.x -= 10.0f;
    reach.w += 20.0f;
    for (const Obstacle& ob : obstacles_) {
        if (!ob.cleared && ob.rect().overlaps(reach)) return &ob;
    }
    return nullptr;
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
    const float damageScale = difficultyDamageScale(app_.settings().difficulty) * currentModifiers().defense;
    const Modifiers mods = currentModifiers();
    const bool invisible = mods.has("invisible");
    const bool invincible = mods.has("invincible");
    for (Enemy& e : enemies_) {
        e.update(dt, area_.map, player_.box, !player_.dead() && !invisible);
        if (!e.dead && !invincible && e.touchCooldown <= 0.0f && e.box.overlaps(player_.box) && mode_ == Mode::Play) {
            if (player_.hurt(e.def->damage * damageScale, e.box.center().x)) {
                e.touchedPlayer();
                if (app_.debugDraw) SDL_Log("%s hit the explorer: health %.0f", e.def->name.c_str(), player_.health);
            }
        }
        if (e.dead) {
            if (app_.debugDraw) SDL_Log("killed %s", e.def->name.c_str());
            for (const std::string& id : quests_.onKill(e.def->id, data_.quests)) {
                const QuestDef& q = data_.quests.at(id);
                const int n = quests_.progress(q, inventory_);
                toast(q.title + ": " + std::to_string(n) + "/" + std::to_string(q.count) +
                      (n >= q.count ? " - done! Go back and tell them." : ""));
            }
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
        if (p.removed && !p.placedKey.empty()) takenPickups_.insert(p.placedKey);
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
    for (std::size_t i = 0; i < shrines_.size(); ++i) {
        const Shrine& sh = shrines_[i];
        const Rect near{sh.feet.x - 20.0f, sh.feet.y - 36.0f, 40.0f, 36.0f};
        if (near.overlaps(player_.box)) {
            found.kind = Interactable::Kind::Shrine;
            found.corpse = static_cast<int>(i);  // index into shrines_
            return found;
        }
    }
    for (const MapObject& o : area_.objects) {
        using K = Interactable::Kind;
        if (o.type == "npc") {
            // NPCs are point objects at their feet.
            const Rect talk{o.rect.x - 18.0f, o.rect.y - 40.0f, 36.0f, 40.0f};
            if (talk.overlaps(player_.box) && data_.npcs.count(o.getString("npc_id"))) {
                found.kind = K::Npc;
                found.object = &o;
                return found;
            }
            continue;
        }
        if (!o.rect.overlaps(player_.box)) continue;
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
    case K::Shrine:
        if (pressed) useShrine(static_cast<std::size_t>(it.corpse));
        break;
    case K::Npc:
        if (pressed) talkTo(data_.npcs.at(it.object->getString("npc_id")));
        break;
    case K::None:
        break;
    }
}

// ============================================================================ people & quests

Modifiers GameplayScene::currentModifiers() const {
    Modifiers m = gearModifiers(inventory_, data_.items);
    for (const SaveGame::TimedBuff& b : buffs_) {
        if (auto it = data_.abilities.find(b.ability); it != data_.abilities.end()) m.combine(it->second.modifiers);
    }
    m.combine(hunger::penalty(hunger_));
    for (const std::string& id : abilities_) {
        if (auto it = data_.abilities.find(id); it != data_.abilities.end()) m.combine(it->second.modifiers);
    }
    return m;
}

void GameplayScene::refreshStats() { player_.applyStats(character_, currentModifiers()); }

std::vector<std::string> GameplayScene::statsSummary() const {
    const Modifiers m = currentModifiers();
    char buf[96];
    std::vector<std::string> out;
    out.push_back(character_.name + "  (explorer #" + std::to_string(character_.number) + ")");
    std::snprintf(buf, sizeof buf, "Health %d / %d", static_cast<int>(std::ceil(player_.health)),
                  static_cast<int>(player_.maxHealth));
    out.emplace_back(buf);
    std::snprintf(buf, sizeof buf, "Strength %.2f   Speed %.2f   Jump %.2f", character_.strength * m.damage,
                  character_.speed * m.runSpeed, character_.jump * m.jump);
    out.emplace_back(buf);
    if (m.defense != 1.0f) {
        std::snprintf(buf, sizeof buf, "Damage taken %d%%", static_cast<int>(std::lround(m.defense * 100.0f)));
        out.emplace_back(buf);
    }
    const char* hungerWord = hunger::level(hunger_) == hunger::Level::Starving ? "starving"
                             : hunger::level(hunger_) == hunger::Level::Hungry ? "hungry" : "fed";
    out.push_back("Hunger " + std::to_string(static_cast<int>(hunger_)) + " / 100 (" + hungerWord + ")");
    std::vector<std::string> effects;
    for (const SaveGame::TimedBuff& b : buffs_) {
        if (auto it = data_.abilities.find(b.ability); it != data_.abilities.end()) {
            effects.push_back(it->second.name + " (" + std::to_string(static_cast<int>(std::ceil(b.remaining))) + "s)");
        }
    }
    if (hunger::level(hunger_) == hunger::Level::Starving) effects.emplace_back("Starving: slower, weaker jumps");
    for (const std::string& id : abilities_) {
        if (auto it = data_.abilities.find(id); it != data_.abilities.end()) effects.push_back(it->second.name);
    }
    for (const std::string& e : m.describe()) effects.push_back(e);
    if (!effects.empty()) {
        out.emplace_back("");
        out.emplace_back("Active effects:");
        for (const std::string& e : effects) out.push_back("  " + e);
    }
    return out;
}

const QuestDef* GameplayScene::nextQuestFor(const NpcDef& npc) const {
    for (const std::string& id : npc.quests) {
        auto it = data_.quests.find(id);
        if (it == data_.quests.end()) continue;
        if (quests_.status(id) != QuestState::Status::Done) return &it->second;
    }
    return nullptr;
}

void GameplayScene::talkTo(const NpcDef& npc) {
    talkingTo_ = &npc;
    dialogue_.open(data_.sprites, npc.name, npc.icon);
    showDialogueRoot(npc.greeting);
    mode_ = Mode::Dialogue;
}

void GameplayScene::showDialogueRoot(const std::string& text) {
    using K = DialogueChoice::Kind;
    choices_.clear();
    std::vector<std::string> labels;
    if (!talkingTo_->shop.empty()) {
        choices_.push_back({K::Trade, ""});
        labels.emplace_back("Let's trade.");
    }
    if (const QuestDef* q = nextQuestFor(*talkingTo_)) {
        const auto status = quests_.status(q->id);
        if (status == QuestState::Status::NotStarted) {
            choices_.push_back({K::AskWork, q->id});
            labels.emplace_back("Need a hand with anything?");
        } else if (quests_.ready(*q, inventory_)) {
            choices_.push_back({K::TurnIn, q->id});
            labels.push_back("About \"" + q->title + "\" - it's done.");
        } else {
            choices_.push_back({K::Report, q->id});
            labels.push_back("About \"" + q->title + "\"...");
        }
    }
    choices_.push_back({K::Goodbye, ""});
    labels.emplace_back("Goodbye.");
    dialogue_.say(text, labels);
}

void GameplayScene::updateDialogue() {
    using K = DialogueChoice::Kind;
    const int pick = dialogue_.update(app_.input());
    if (pick == Menu::kNone) return;
    if (pick == Menu::kCancelled || pick >= static_cast<int>(choices_.size())) {
        mode_ = Mode::Play;
        return;
    }
    const DialogueChoice choice = choices_[static_cast<std::size_t>(pick)];
    const QuestDef* q = choice.quest.empty() ? nullptr : &data_.quests.at(choice.quest);
    switch (choice.kind) {
    case K::Trade:
        openShop(talkingTo_->shop);
        break;
    case K::AskWork:
        choices_ = {{K::Accept, q->id}, {K::Decline, q->id}};
        dialogue_.say(q->intro, {"I'll do it.", "Not right now."});
        break;
    case K::Accept:
        quests_.start(q->id);
        toast("New task: " + q->title);
        showDialogueRoot(q->accepted);
        break;
    case K::Decline:
        showDialogueRoot("Suit yourself.");
        break;
    case K::Report:
        showDialogueRoot(fillQuestText(q->progress, quests_.progress(*q, inventory_), q->count));
        break;
    case K::TurnIn: {
        QuestLog::Reward reward;
        if (!quests_.turnIn(*q, inventory_, data_.items, reward)) break;
        std::string text = q->complete;
        if (!reward.item.empty()) {
            text += "  [Received " + data_.items.get(reward.item).name + "]";
            if (reward.itemsThatDidNotFit > 0) {
                ItemStack extra{reward.item, reward.itemsThatDidNotFit};
                dropStack(extra);
                text += " (your pack was full - it's on the ground)";
            }
        }
        if (reward.coins > 0) text += "  [+" + std::to_string(reward.coins) + " coins]";
        if (!reward.ability.empty()) {
            abilities_.push_back(reward.ability);
            auto it = data_.abilities.find(reward.ability);
            if (it != data_.abilities.end()) text += "  [Learned " + it->second.name + "]";
        }
        refreshStats();
        toast("Task complete: " + q->title);
        showDialogueRoot(text);
        saveGame();
        break;
    }
    case K::Goodbye:
        mode_ = Mode::Play;
        break;
    }
}

void GameplayScene::restartArea(const std::string& reason) {
    // Reload the area as if walking back in through the doorway we used. Health, items and
    // quests are untouched; freebies already taken stay taken.
    toast(reason);
    requestTransition(area_.id, entrySpawn_);
}

void GameplayScene::updateObstaclePushing(float dt) {
    const Input& in = app_.input();
    for (Obstacle& ob : obstacles_) {
        if (ob.cleared || ob.passableFrom == 0) continue;
        const Rect r = ob.rect();
        // Standing right against the obstacle on its passable side and pushing toward it.
        const bool fromRight = ob.passableFrom > 0 && std::fabs(player_.box.left() - r.right()) < 1.5f &&
                               in.down(Action::Left);
        const bool fromLeft = ob.passableFrom < 0 && std::fabs(player_.box.right() - r.left()) < 1.5f &&
                              in.down(Action::Right);
        const bool overlapsY = player_.box.bottom() > r.top() && player_.box.top() < r.bottom();
        if ((fromRight || fromLeft) && overlapsY) {
            ob.pushTime += dt;
            ob.shake = std::max(ob.shake, 0.1f);
            if (ob.pushTime >= 0.5f) {
                ob.cleared = true;
                ob.block(area_.map, false);
                burst(r.center(), ob.def->debris, 14);
                if (ob.def->pushDamage > 0.0f) {
                    // A scratch, not a hit: no knockback (it could throw you back the way you came).
                    player_.health = std::max(1.0f, player_.health - ob.def->pushDamage);
                    player_.invulnerable = 0.4f;
                    regenDelay_ = kRegenDelay;
                }
                toast(ob.def->pushMessage);
            }
        } else {
            ob.pushTime = 0.0f;
        }
    }
}

std::string GameplayScene::obstacleHint(const Obstacle& ob) const {
    // Name the exact problem: the right tool in the other hand, in the pack, or not at all.
    const Input& in = app_.input();
    const std::pair<std::size_t, Action> hands[] = {{Inventory::RightHand, Action::UseRight},
                                                    {Inventory::LeftHand, Action::UseLeft}};
    for (const auto& [slot, action] : hands) {
        const ItemStack& held = inventory_.equipment.at(slot);
        if (!held.empty() && data_.items.get(held.id).hasTag(ob.def->requiredTag)) {
            return "Your " + data_.items.get(held.id).name + " is in your " +
                   (slot == Inventory::RightHand ? "right" : "left") + " hand - press " + in.keyName(action) + ".";
        }
    }
    for (std::size_t i = 0; i < inventory_.backpack.size(); ++i) {
        const ItemStack& s = inventory_.backpack.at(i);
        if (!s.empty() && data_.items.get(s.id).hasTag(ob.def->requiredTag)) {
            return "Hold the " + data_.items.get(s.id).name + " in a hand to use it (" + in.keyName(Action::Inventory) +
                   ": inventory).";
        }
    }
    return ob.def->hint;
}

// ============================================================================ menus

void GameplayScene::openPause() {
    pauseMenu_.setItems({"Resume", "Restart area (back to the entrance)",
                         slot_ == kDevSlot ? "Save game (dev game: off)" : "Save game", "Settings", "Credits",
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
    case RestartItem:
        mode_ = Mode::Play;
        restartArea("You retrace your steps to the entrance.");
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
    inventoryScreen_.setSummaryProvider([this] { return statsSummary(); });
    const Input& in = app_.input();
    inventoryScreen_.setKeyHints("Enter: move   " + in.keyName(Action::Interact) + ": use   " +
                                     in.keyName(Action::Drop) + ": drop   " + in.keyName(Action::Inventory) + ": close",
                                 "Enter: place here   Esc: cancel");
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

bool GameplayScene::useItem(const ItemStack& stack) {
    const ItemDef& def = data_.items.get(stack.id);
    if (def.food > 0.0f) {
        if (hunger_ >= hunger::kMax - 1.0f) {
            inventoryScreen_.setMessage("You're not hungry.", true);
            return false;
        }
        const int before = static_cast<int>(hunger_);
        hunger_ = std::min(hunger::kMax, hunger_ + def.food);
        inventoryScreen_.setMessage("You eat the " + def.name + ". Hunger " + std::to_string(before) + " -> " +
                                    std::to_string(static_cast<int>(hunger_)) + ".");
    }
    if (def.heal > 0.0f) {
        if (def.food <= 0.0f && player_.health >= player_.maxHealth) {
            inventoryScreen_.setMessage("You're already in good health.", true);
            return false;
        }
        player_.health = std::min(player_.maxHealth, player_.health + def.heal);
        if (def.food <= 0.0f) inventoryScreen_.setMessage("You drink the " + def.name + ".");
    }
    return true;
}

void GameplayScene::updateSurvival(float dt) {
    // Hunger drains while exploring; crossing a threshold gets a warning.
    const hunger::Level before = hunger::level(hunger_);
    hunger_ = std::max(0.0f, hunger_ - hunger::kDrainPerSecond * dt);
    const hunger::Level after = hunger::level(hunger_);
    if (after != before) {
        if (after == hunger::Level::Hungry) toast("You're getting hungry. Wounds won't heal until you eat.");
        if (after == hunger::Level::Starving) toast("You're starving! You feel weak and slow.");
    }

    // Shrine blessings run out.
    for (SaveGame::TimedBuff& b : buffs_) {
        b.remaining -= dt;
        if (b.remaining <= 0.0f) {
            auto it = data_.abilities.find(b.ability);
            toast((it != data_.abilities.end() ? it->second.name : b.ability) + " wears off.");
        }
    }
    std::erase_if(buffs_, [](const SaveGame::TimedBuff& b) { return b.remaining <= 0.0f; });
}

void GameplayScene::useShrine(std::size_t index) {
    Shrine& sh = shrines_[index];
    auto it = data_.abilities.find(sh.def->ability);
    if (it == data_.abilities.end()) return;
    const AbilityDef& ability = it->second;
    if (sh.used) {
        toast("The shrine is dormant. Perhaps another day.");
        return;
    }
    if (sh.def->duration <= 0.0f) {
        // Lasting blessings don't stack.
        if (std::find(abilities_.begin(), abilities_.end(), ability.id) != abilities_.end()) {
            toast("You already carry this blessing.");
            return;
        }
        abilities_.push_back(ability.id);
        toast(ability.description + " (" + ability.name + ", until death)");
        saveGame();
    } else {
        // Using the same kind again just refreshes the timer.
        auto b = std::find_if(buffs_.begin(), buffs_.end(), [&](const auto& x) { return x.ability == ability.id; });
        if (b != buffs_.end()) b->remaining = sh.def->duration;
        else buffs_.push_back({ability.id, sh.def->duration});
        toast(ability.description + " (" + ability.name + ", " + std::to_string(static_cast<int>(sh.def->duration)) + "s)");
    }
    sh.used = true;
    burst({sh.feet.x, sh.feet.y - 18.0f}, sh.def->glow, 24);
    refreshStats();
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
        if (o.type == "door" && !foreground && o.getString("trigger", "interact") != "touch") {
            // Make doorways easy to spot: optional light spilling through, and a bobbing arrow.
            const std::string glow = o.getString("glow");
            if (glow.size() == 7) {
                const unsigned long c = std::stoul(glow.substr(1), nullptr, 16);
                const auto cr = static_cast<Uint8>(c >> 16), cg = static_cast<Uint8>(c >> 8), cb = static_cast<Uint8>(c);
                for (int i = 0; i < 6; ++i) {
                    const float inset = static_cast<float>(i) * 3.0f;
                    const SDL_FRect g{o.rect.x + inset - cam.x, o.rect.y + inset - cam.y, o.rect.w - inset * 2, o.rect.h - inset};
                    SDL_SetRenderDrawColor(r, cr, cg, cb, 28);
                    SDL_RenderFillRect(r, &g);
                }
            }
            const float bob = std::sin(static_cast<float>(SDL_GetTicks()) * 0.006f) * 3.0f;
            ui::textCentered(r, o.rect.x + o.rect.w * 0.5f - cam.x, o.rect.y - 14.0f + bob - cam.y, "^", ui::kGold);
            continue;
        }
        if (o.type == "npc" && !foreground) {
            auto it = data_.npcs.find(o.getString("npc_id"));
            if (it == data_.npcs.end()) continue;
            // Face the explorer.
            const bool flip = player_.box.center().x < o.rect.x;
            data_.sprites.drawIcon(r, it->second.icon, o.rect.x - 16.0f - cam.x, o.rect.y - 32.0f - cam.y, 32.0f, flip);
            continue;
        }
        if (sprite.empty() || o.getBool("foreground") != foreground) continue;
        // Sprites sit on the bottom-left corner of their object.
        const SDL_FPoint size = data_.sprites.size(sprite);
        data_.sprites.draw(r, sprite, o.rect.x - cam.x, o.rect.y + o.rect.h - size.y - cam.y, o.getBool("flip"));
    }
    if (foreground) return;
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(SDL_GetTicks()) * 0.004f);
    for (const Shrine& sh : shrines_) {
        const float cx = sh.feet.x - cam.x, cy = sh.feet.y - 16.0f - cam.y;
        if (!sh.used) {
            for (int i = 0; i < 4; ++i) {
                const float s = 20.0f + static_cast<float>(i) * 7.0f + pulse * 4.0f;
                const SDL_FRect g{cx - s * 0.5f, cy - s * 0.5f - 4.0f, s, s + 4.0f};
                SDL_SetRenderDrawColor(r, sh.def->glow.r, sh.def->glow.g, sh.def->glow.b, static_cast<Uint8>(34 - i * 7));
                SDL_RenderFillRect(r, &g);
            }
        }
        const Uint8 shade = sh.used ? 110 : 255;
        data_.sprites.drawIcon(r, sh.def->icon, cx - 16.0f, cy - 16.0f, 32.0f, false, {shade, shade, shade, 255});
    }
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

    // Hunger bar under the health bar.
    const SDL_FRect foodBack{88, 21, 120, 6};
    SDL_SetRenderDrawColor(r, 30, 20, 10, 255);
    SDL_RenderFillRect(r, &foodBack);
    const float full = std::clamp(hunger_ / hunger::kMax, 0.0f, 1.0f);
    const SDL_FRect food{foodBack.x + 1, foodBack.y + 1, (foodBack.w - 2) * full, foodBack.h - 2};
    SDL_SetRenderDrawColor(r, 220, static_cast<Uint8>(120 + 60 * full), 40, 255);
    SDL_RenderFillRect(r, &food);
    SDL_SetRenderDrawColor(r, 120, 98, 70, 255);
    SDL_RenderRect(r, &foodBack);
    if (hunger::level(hunger_) != hunger::Level::Fed) {
        const bool starving = hunger::level(hunger_) == hunger::Level::Starving;
        ui::text(r, foodBack.x + foodBack.w + 6, foodBack.y - 1, starving ? "Starving!" : "Hungry",
                 starving ? ui::kBad : ui::kGold);
    }
    data_.sprites.drawIcon(r, "coins", 86, 28, 20);
    ui::text(r, 110, 35, std::to_string(inventory_.coins), ui::kGold);
    // Shrine blessings with time left.
    float by = 52.0f;
    for (const SaveGame::TimedBuff& b : buffs_) {
        auto it = data_.abilities.find(b.ability);
        if (it == data_.abilities.end()) continue;
        const int secs = static_cast<int>(std::ceil(b.remaining));
        const bool ending = b.remaining < 5.0f && static_cast<int>(b.remaining * 4.0f) % 2 == 0;
        ui::text(r, 8, by, it->second.name + " " + std::to_string(secs) + "s", ending ? ui::kDim : ui::kGood);
        by += 11.0f;
    }
    ui::text(r, App::kViewWidth - 8 - ui::textWidth(character_.name), 8, character_.name, ui::kDim);
    // Active tasks under the name.
    float qy = 22.0f;
    for (const std::string& id : quests_.active()) {
        auto it = data_.quests.find(id);
        if (it == data_.quests.end()) continue;
        const QuestDef& q = it->second;
        const int n = quests_.progress(q, inventory_);
        const std::string line = q.title + "  " + std::to_string(n) + "/" + std::to_string(q.count);
        ui::text(r, App::kViewWidth - 8 - ui::textWidth(line), qy, line, n >= q.count ? ui::kGood : ui::kText);
        qy += 12.0f;
    }

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
    for (const Obstacle& ob : obstacles_) ob.render(renderer, data_.sprites, cam);
    for (const Enemy& e : enemies_) e.render(renderer, data_.sprites, cam, alpha);
    player_.render(renderer, cam, alpha, app_.debugDraw);
    for (const Projectile& p : projectiles_) p.render(renderer, data_.sprites, cam, alpha);
    for (const Particle& p : particles_) {
        const SDL_FRect d{std::round(p.pos.x - cam.x), std::round(p.pos.y - cam.y), 3, 3};
        const auto a = static_cast<Uint8>(255 * std::clamp(p.life * 2.0f, 0.0f, 1.0f));
        SDL_SetRenderDrawColor(renderer, p.color.r, p.color.g, p.color.b, a);
        SDL_RenderFillRect(renderer, &d);
    }
    area_.map.render(renderer, cam, true);
    renderWorldObjects(renderer, cam, true);

    if (mode_ == Mode::Play && fadeDirection_ == 0) {
        const Interactable it = findInteractable();
        std::string prompt;
        using K = Interactable::Kind;
        const Input& keys = app_.input();
        const std::string use = "[" + keys.keyName(Action::Interact) + "] ";
        if (it.kind == K::Chest) prompt = use + "Open chest";
        else if (it.kind == K::Shop) prompt = use + "Shop";
        else if (it.kind == K::Door) prompt = "[" + keys.keyName(Action::Up) + "] " + it.object->getString("label", "Enter");
        else if (it.kind == K::Corpse) prompt = use + "Search " + world_.corpses[static_cast<std::size_t>(it.corpse)].explorer;
        else if (it.kind == K::Npc) prompt = use + "Talk to " + data_.npcs.at(it.object->getString("npc_id")).name;
        else if (it.kind == K::Shrine) {
            const Shrine& sh = shrines_[static_cast<std::size_t>(it.corpse)];
            prompt = sh.used ? sh.def->name + " (dormant)" : use + "Pray at the " + sh.def->name;
        }
        if (prompt.empty()) {
            if (const Obstacle* ob = obstacleNearPlayer()) {
                // Suggest the hand holding a suitable tool, or just name the obstacle.
                prompt = ob->def->name;
                const std::pair<std::size_t, Action> hands[] = {{Inventory::RightHand, Action::UseRight},
                                                                {Inventory::LeftHand, Action::UseLeft}};
                for (const auto& [slot, action] : hands) {
                    const ItemStack& held = inventory_.equipment.at(slot);
                    if (!held.empty() && data_.items.get(held.id).hasTag(ob->def->requiredTag)) {
                        prompt = "[" + keys.keyName(action) + "] " + ob->def->verb + " the " + ob->def->name;
                        break;
                    }
                }
            }
        }
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
        SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
        for (const Obstacle& ob : obstacles_) {
            const Rect r = ob.rect();
            const SDL_FRect rc{r.x - cam.x, r.y - cam.y, r.w, r.h};
            if (!ob.cleared) SDL_RenderRect(renderer, &rc);
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
    case Mode::Dialogue: dialogue_.render(renderer); break;
    default: break;
    }

    if (fade_ > 0.0f) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(255 * std::clamp(fade_, 0.0f, 1.0f)));
        SDL_RenderFillRect(renderer, &screen);
    }
}
