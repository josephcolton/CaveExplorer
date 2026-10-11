#pragma once

#include "core/Scene.h"
#include "entity/Enemy.h"
#include "entity/Obstacle.h"
#include "entity/Pickup.h"
#include "entity/Player.h"
#include "entity/Projectile.h"
#include "game/Character.h"
#include "game/Inventory.h"
#include "game/Hunger.h"
#include "game/Modifiers.h"
#include "game/SaveGame.h"
#include "game/Quests.h"
#include "game/WorldSave.h"
#include "ui/DialogueScreen.h"
#include "ui/InventoryScreen.h"
#include "ui/Menus.h"
#include "ui/ShopScreen.h"
#include "world/Area.h"
#include "world/Camera.h"
#include "world/Lighting.h"

#include <random>
#include <set>
#include <string>
#include <vector>

struct GameData;
struct NpcDef;
struct ShrineDef;

class GameplayScene : public Scene {
public:
    // Slot 0 is a throwaway game used by the debug command-line options; it never saves.
    static constexpr int kDevSlot = 0;

    GameplayScene(App& app, int slot, bool newGame);
    ~GameplayScene() override;

    void update(float dt) override;
    void render(SDL_Renderer* renderer, float alpha) override;

private:
    enum class Mode { Play, Inventory, Shop, Pause, Settings, Text, Dialogue, Dead };

    struct Interactable {
        enum class Kind { None, Door, TouchDoor, Chest, Shop, Corpse, Npc, Shrine } kind = Kind::None;
        const MapObject* object = nullptr;
        int corpse = -1;
    };

    // --- explorers & saving
    void newExplorer();
    void saveGame();
    void handleDeath();

    // --- areas
    // Starts a fade-out; the area swap happens when the screen is black.
    void requestTransition(const std::string& areaId, const std::string& spawn);
    void enterArea(const std::string& areaId, const std::string& spawn);
    void spawnAreaObjects();

    // --- play
    void updatePlay(float dt);
    void updateAttacks();
    void updateEnemies(float dt);
    void updateProjectiles(float dt);
    void updatePickups(float dt);
    void spawnDrops(const Enemy& e);
    void hitObstacles();
    void burst(Vec2 at, SDL_Color color, int count);
    const Obstacle* obstacleNearPlayer() const;
    void handleInteraction();
    Interactable findInteractable() const;
    Rect corpseRect(const CorpsePile& c) const;

    // --- menus
    void updatePause();
    void restartArea(const std::string& reason);
    void updateObstaclePushing(float dt);
    std::string obstacleHint(const Obstacle& ob) const;
    void talkTo(const NpcDef& npc);
    void showDialogueRoot(const std::string& text);
    void updateDialogue();
    const QuestDef* nextQuestFor(const NpcDef& npc) const;
    // Gear + lasting abilities.
    Modifiers currentModifiers() const;
    void refreshStats();
    std::vector<std::string> statsSummary() const;
    void openPause();
    void openInventory(Container* chest, const std::string& title = "Storage Chest");
    void openCorpse(int index);
    void closeInventory();
    void openShop(const std::string& shopId);
    void dropStack(const ItemStack& stack);
    // Returns true if the item was used up.
    bool useItem(const ItemStack& stack);
    void useShrine(std::size_t index);
    void updateSurvival(float dt);
    // The equipped light that's actually shining (radius 0 = none), handling fuel.
    const ItemDef* equippedFuelLight() const;
    float carriedLightRadius() const;
    void renderLighting(SDL_Renderer* r, Vec2 cam);

    void toast(const std::string& text);
    void renderWorldObjects(SDL_Renderer* r, Vec2 cam, bool foreground) const;
    void renderHud(SDL_Renderer* r) const;
    void renderBanner(SDL_Renderer* r) const;

    const GameData& data_;
    int slot_;
    std::mt19937 rng_;

    // Saved state
    WorldSave world_;
    Character character_;
    Inventory inventory_;
    std::set<std::string> takenPickups_;  // this explorer's collected freebies
    std::vector<std::string> abilities_;   // lasting abilities until death
    QuestLog quests_;
    float hunger_ = hunger::kMax;
    float lanternFuel_ = 0.0f;  // seconds of oil left in a lit lantern
    std::vector<SaveGame::TimedBuff> buffs_;  // shrine blessings with time left
    double playSeconds_ = 0.0;

    // Current area
    Area area_;
    Lighting lighting_;
    float lightTime_ = 0.0f;  // drives flicker
    Player player_;
    Camera camera_;
    std::vector<Pickup> pickups_;
    std::vector<Enemy> enemies_;
    std::vector<Projectile> projectiles_;
    std::vector<Obstacle> obstacles_;
    std::vector<Rect> ladders_;
    struct Shrine {
        const ShrineDef* def;
        Vec2 feet;
        bool used = false;  // dormant until the area reloads
    };
    std::vector<Shrine> shrines_;
    struct Particle {
        Vec2 pos;
        Vec2 vel;
        float life;
        SDL_Color color;
    };
    std::vector<Particle> particles_;
    std::string swingItem_;  // item used for the current melee swing ("" = bare hands)
    int nextProjectileId_ = 1'000'000;  // separate id space from melee swings

    // UI
    Mode mode_ = Mode::Play;
    Mode textReturnMode_ = Mode::Pause;
    InventoryScreen inventoryScreen_;
    ShopScreen shopScreen_;
    Menu pauseMenu_;
    SettingsScreen settingsScreen_;
    TextScreen textScreen_;
    DialogueScreen dialogue_;
    const NpcDef* talkingTo_ = nullptr;
    struct DialogueChoice {
        enum class Kind { Trade, AskWork, Accept, Decline, Report, TurnIn, Goodbye } kind;
        std::string quest;
    };
    std::vector<DialogueChoice> choices_;
    std::string pauseMessage_;
    bool chestOpen_ = false;
    int openCorpse_ = -1;
    Container corpseContainer_;

    struct Toast {
        std::string text;
        float time;
    };
    std::vector<Toast> toasts_;
    float messageCooldown_ = 0.0f;
    std::vector<std::string> banner_;  // "a new explorer arrives" card
    float bannerTimer_ = 0.0f;
    float deathTimer_ = 0.0f;
    float regenDelay_ = 0.0f;

    std::string entrySpawn_ = "start";  // how we entered the current area
    std::string pendingArea_;
    std::string pendingSpawn_;
    float fade_ = 1.0f;        // 0 = clear, 1 = black
    int fadeDirection_ = -1;   // -1 fading in, +1 fading out, 0 idle
    float areaTitleTimer_ = 0.0f;
};
