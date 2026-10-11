#pragma once

#include "core/Scene.h"
#include "core/Sprites.h"
#include "entity/Pickup.h"
#include "entity/Player.h"
#include "game/Inventory.h"
#include "game/Items.h"
#include "game/WorldSave.h"
#include "ui/InventoryScreen.h"
#include "ui/ShopScreen.h"
#include "world/Area.h"
#include "world/Camera.h"

#include <string>
#include <unordered_map>
#include <vector>

class GameplayScene : public Scene {
public:
    explicit GameplayScene(App& app);
    ~GameplayScene() override;

    void update(float dt) override;
    void render(SDL_Renderer* renderer, float alpha) override;

private:
    enum class Mode { Play, Inventory, Shop };

    void loadData();
    void loadWorld();
    void saveWorld();

    // Starts a fade-out; the area swap happens when the screen is black.
    void requestTransition(const std::string& areaId, const std::string& spawn);
    void enterArea(const std::string& areaId, const std::string& spawn);
    void spawnAreaObjects();

    void updatePlay(float dt);
    void updatePickups(float dt);
    void handleInteraction();
    const MapObject* interactableUnderPlayer() const;
    void dropStack(const ItemStack& stack);
    void useItem(const ItemStack& stack);
    void openInventory(Container* chest);
    void openShop(const std::string& shopId);

    void toast(const std::string& text);
    void renderWorldObjects(SDL_Renderer* r, Vec2 cam, bool foreground) const;
    void renderHud(SDL_Renderer* r) const;

    // Data
    ItemDb items_;
    std::unordered_map<std::string, ShopDef> shops_;
    SpriteDb sprites_;

    // Persistent across explorers / for this explorer
    WorldSave world_;
    Inventory inventory_;

    // Current area
    Area area_;
    Player player_;
    Camera camera_;
    std::vector<Pickup> pickups_;

    // UI
    Mode mode_ = Mode::Play;
    InventoryScreen inventoryScreen_;
    ShopScreen shopScreen_;
    bool chestOpen_ = false;
    struct Toast {
        std::string text;
        float time;
    };
    std::vector<Toast> toasts_;
    float fullMessageCooldown_ = 0.0f;

    std::string pendingArea_;
    std::string pendingSpawn_;
    float fade_ = 1.0f;        // 0 = clear, 1 = black
    int fadeDirection_ = -1;   // -1 fading in, +1 fading out, 0 idle
    float areaTitleTimer_ = 0.0f;
};
