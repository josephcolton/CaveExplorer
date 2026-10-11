#pragma once

#include "core/Scene.h"
#include "game/SaveGame.h"
#include "ui/Menus.h"

#include <array>
#include <optional>

// Title screen: play (choose a save slot), settings, credits, about, quit.
class TitleScene : public Scene {
public:
    explicit TitleScene(App& app);

    void update(float dt) override;
    void render(SDL_Renderer* renderer, float alpha) override;

private:
    enum class Page { Main, Slots, SlotActions, ConfirmDelete, Settings, Credits, About };

    void refreshSlots();
    void openSlots();
    void startGame(int slot, bool newGame);

    Page page_ = Page::Main;
    Menu mainMenu_;
    Menu slotMenu_;
    Menu actionMenu_;
    Menu confirmMenu_;
    SettingsScreen settings_;
    TextScreen text_;
    std::array<std::optional<SaveGame>, SaveGame::kSlotCount> slots_;
    int chosenSlot_ = 1;
    float time_ = 0.0f;
};
