#pragma once

#include "game/Inventory.h"

#include <SDL3/SDL.h>

#include <functional>
#include <string>
#include <vector>

class Input;
class SpriteDb;

// What the inventory screen asks the game to do (it doesn't touch the world itself).
struct InventoryRequest {
    enum class Kind { None, Close, Drop, Use } kind = Kind::None;
    ItemStack stack;  // Drop: what was removed. Use: the item to use (not yet removed)
    Container* container = nullptr;  // Use: where it is, so the game can consume it
    std::size_t index = 0;
};

// Backpack + equipment ("paper doll"), optionally side by side with a storage chest.
// Moving items: Confirm on a slot selects it, Confirm on another slot moves/merges/swaps.
class InventoryScreen {
public:
    void open(const ItemDb& db, const SpriteDb& sprites, Inventory& inv, Container* chest,
              std::string chestTitle = "Storage Chest");
    InventoryRequest update(const Input& input);
    void render(SDL_Renderer* r) const;

    // Lines shown in the details panel when the cursor is on an empty slot (explorer stats).
    void setSummaryProvider(std::function<std::vector<std::string>()> f) { summary_ = std::move(f); }

    // Footer hints, e.g. "Enter: move  E: use ..." (normal) and while an item is picked up.
    void setKeyHints(std::string normal, std::string placing) {
        hintNormal_ = std::move(normal);
        hintPlacing_ = std::move(placing);
    }

    void setMessage(const std::string& msg, bool bad = false) { message_ = msg; messageBad_ = bad; }

private:
    struct Slot {
        Container* container;
        std::size_t index;
        SDL_FRect rect;
        const char* emptyLabel;
    };
    void buildLayout();
    const ItemStack& stackAt(int i) const { return slots_[i].container->at(slots_[i].index); }

    const ItemDb* db_ = nullptr;
    const SpriteDb* sprites_ = nullptr;
    Inventory* inv_ = nullptr;
    Container* chest_ = nullptr;
    std::string chestTitle_;
    std::string hintNormal_ = "Enter: move   E: use   Q: drop   Tab: close";
    std::string hintPlacing_ = "Enter: place here   Esc: cancel";
    std::function<std::vector<std::string>()> summary_;

    std::vector<Slot> slots_;
    std::vector<SDL_FRect> rects_;
    int cursor_ = 0;
    int selected_ = -1;
    std::string message_;
    bool messageBad_ = false;
};
