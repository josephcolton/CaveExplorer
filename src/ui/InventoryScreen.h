#pragma once

#include "game/Inventory.h"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

class Input;
class SpriteDb;

// What the inventory screen asks the game to do (it doesn't touch the world itself).
struct InventoryRequest {
    enum class Kind { None, Close, Drop, Use } kind = Kind::None;
    ItemStack stack;  // for Drop/Use: what was removed from the inventory
};

// Backpack + equipment ("paper doll"), optionally side by side with a storage chest.
// Moving items: Confirm on a slot selects it, Confirm on another slot moves/merges/swaps.
class InventoryScreen {
public:
    void open(const ItemDb& db, const SpriteDb& sprites, Inventory& inv, Container* chest,
              std::string chestTitle = "Storage Chest");
    InventoryRequest update(const Input& input);
    void render(SDL_Renderer* r) const;

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

    std::vector<Slot> slots_;
    std::vector<SDL_FRect> rects_;
    int cursor_ = 0;
    int selected_ = -1;
    std::string message_;
    bool messageBad_ = false;
};
