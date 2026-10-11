#include "game/TitleScene.h"

#include "core/App.h"
#include "game/GameplayScene.h"
#include "game/SaveSlots.h"
#include "ui/Ui.h"

namespace {
enum MainItem { Play, SettingsItem, Credits, About, Quit };

}  // namespace

TitleScene::TitleScene(App& app) : Scene(app) {
    mainMenu_.setItems({"Play", "Settings", "Credits", "About", "Quit"});
    app.audio().playMusic("");
    const std::string& ui = app.options().openUi;  // debug: open a page directly
    if (ui == "slots") openSlots();
    else if (ui == "settings") { settings_.open(app_); page_ = Page::Settings; }
    else if (ui == "credits") { text_.open("Credits", app_.data().creditsText); page_ = Page::Credits; }
    else if (ui == "about") { text_.open("About", aboutText()); page_ = Page::About; }
}

void TitleScene::refreshSlots() {
    std::vector<std::string> items;
    for (int i = 0; i < SaveGame::kSlotCount; ++i) {
        SaveGame save;
        if (saveslots::load(i + 1, save)) slots_[static_cast<std::size_t>(i)] = std::move(save);
        else slots_[static_cast<std::size_t>(i)].reset();
        items.push_back("Slot " + std::to_string(i + 1));
    }
    items.push_back("Back");
    slotMenu_.setItems(items, slotMenu_.cursor());
    slotMenu_.lineHeight = 46.0f;
}

void TitleScene::openSlots() {
    refreshSlots();
    page_ = Page::Slots;
}

void TitleScene::startGame(int slot, bool newGame) {
    if (newGame) saveslots::remove(slot);
    app_.setScene(std::make_unique<GameplayScene>(app_, slot, newGame));
}

void TitleScene::update(float dt) {
    time_ += dt;
    const Input& in = app_.input();
    switch (page_) {
    case Page::Main: {
        const int choice = mainMenu_.update(in);
        if (choice == Play) openSlots();
        else if (choice == SettingsItem) { settings_.open(app_); page_ = Page::Settings; }
        else if (choice == Credits) { text_.open("Credits", app_.data().creditsText); page_ = Page::Credits; }
        else if (choice == About) { text_.open("About", aboutText()); page_ = Page::About; }
        else if (choice == Quit) app_.quit();
        break;
    }
    case Page::Slots: {
        const int choice = slotMenu_.update(in);
        if (choice == Menu::kCancelled || choice == SaveGame::kSlotCount) {
            page_ = Page::Main;
        } else if (choice >= 0) {
            chosenSlot_ = choice + 1;
            if (!slots_[static_cast<std::size_t>(choice)]) {
                startGame(chosenSlot_, true);
            } else {
                actionMenu_.setItems({"Continue", "Delete", "Back"});
                page_ = Page::SlotActions;
            }
        }
        break;
    }
    case Page::SlotActions: {
        const int choice = actionMenu_.update(in);
        if (choice == 0) startGame(chosenSlot_, false);
        else if (choice == 1) { confirmMenu_.setItems({"No, keep it", "Yes, delete it"}); page_ = Page::ConfirmDelete; }
        else if (choice == 2 || choice == Menu::kCancelled) page_ = Page::Slots;
        break;
    }
    case Page::ConfirmDelete: {
        const int choice = confirmMenu_.update(in);
        if (choice == 1) {
            saveslots::remove(chosenSlot_);
            refreshSlots();
            page_ = Page::Slots;
        } else if (choice == 0 || choice == Menu::kCancelled) {
            page_ = Page::SlotActions;
        }
        break;
    }
    case Page::Settings:
        if (settings_.update() == SettingsScreen::Result::Close) page_ = Page::Main;
        break;
    case Page::Credits:
    case Page::About:
        if (!text_.update(in)) page_ = Page::Main;
        break;
    }
}

void TitleScene::render(SDL_Renderer* r, float) {
    // Backdrop: dark cave gradient with a faint glow.
    for (int y = 0; y < App::kViewHeight; y += 4) {
        const float t = static_cast<float>(y) / App::kViewHeight;
        SDL_SetRenderDrawColor(r, static_cast<Uint8>(14 + 20 * t), static_cast<Uint8>(12 + 14 * t),
                               static_cast<Uint8>(22 + 10 * t), 255);
        const SDL_FRect band{0, static_cast<float>(y), App::kViewWidth, 4};
        SDL_RenderFillRect(r, &band);
    }

    // Title, drawn at 4x with the debug font.
    SDL_SetRenderScale(r, 4.0f, 4.0f);
    const char* title = "CAVE EXPLORER";
    const float tx = (App::kViewWidth / 4.0f - ui::textWidth(title)) * 0.5f;
    SDL_SetRenderDrawColor(r, 60, 30, 10, 255);
    SDL_RenderDebugText(r, tx + 0.5f, 10.5f, title);
    SDL_SetRenderDrawColor(r, ui::kGold.r, ui::kGold.g, ui::kGold.b, 255);
    SDL_RenderDebugText(r, tx, 10, title);
    SDL_SetRenderScale(r, 1.0f, 1.0f);
    ui::textCentered(r, App::kViewWidth * 0.5f, 84, "Every explorer leaves something behind.", ui::kDim);
    ui::text(r, App::kViewWidth - 8 - ui::textWidth(std::string("v") + App::kVersion), App::kViewHeight - 14,
             std::string("v") + App::kVersion, ui::kDim);

    switch (page_) {
    case Page::Main:
        mainMenu_.render(r, App::kViewWidth * 0.5f, 140);
        break;
    case Page::Slots:
    case Page::SlotActions:
    case Page::ConfirmDelete: {
        const SDL_FRect panel{120, 110, 400, 220};
        ui::panel(r, panel, "Choose a save slot");
        for (int i = 0; i < SaveGame::kSlotCount; ++i) {
            const bool sel = slotMenu_.cursor() == i;
            const SDL_FRect row{panel.x + 10, panel.y + 26 + static_cast<float>(i) * 46.0f, panel.w - 20, 40};
            SDL_SetRenderDrawColor(r, sel ? 80 : 40, sel ? 62 : 34, sel ? 36 : 40, 255);
            SDL_RenderFillRect(r, &row);
            const auto& slot = slots_[static_cast<std::size_t>(i)];
            const SDL_Color c = sel ? ui::kGold : ui::kText;
            ui::text(r, row.x + 8, row.y + 6, "Slot " + std::to_string(i + 1), c);
            if (slot) {
                ui::text(r, row.x + 80, row.y + 6,
                         slot->character.name + " (explorer #" + std::to_string(slot->character.number) + ")", c);
                ui::text(r, row.x + 80, row.y + 20,
                         slot->areaName + "  -  " + saveslots::formatPlayTime(slot->playSeconds) + "  -  " +
                             std::to_string(slot->world.explorersLost) + " lost  -  " + slot->savedAt,
                         ui::kDim);
            } else {
                ui::text(r, row.x + 80, row.y + 13, "Empty - start a new expedition", ui::kDim);
            }
        }
        const bool backSel = slotMenu_.cursor() == SaveGame::kSlotCount;
        ui::textCentered(r, panel.x + panel.w * 0.5f, panel.y + panel.h - 24, backSel ? "> Back <" : "Back",
                         backSel ? ui::kGold : ui::kText);

        if (page_ != Page::Slots) {
            const SDL_FRect pop{220, 180, 200, 80};
            ui::panel(r, pop, ("Slot " + std::to_string(chosenSlot_)).c_str());
            (page_ == Page::SlotActions ? actionMenu_ : confirmMenu_).render(r, pop.x + pop.w * 0.5f, pop.y + 28);
        }
        break;
    }
    case Page::Settings:
        settings_.render(r);
        break;
    case Page::Credits:
    case Page::About:
        text_.render(r);
        break;
    }
}
