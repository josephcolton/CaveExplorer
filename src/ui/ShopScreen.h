#pragma once

#include "game/Inventory.h"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

class Input;
class SpriteDb;

struct ShopDef {
    std::string id;
    std::string name;
    std::string greeting;
    std::vector<std::string> stock;  // item ids; unlimited supply at the item's value
};

// Buy from the shop's stock or sell from the backpack, one item per press.
// Used by the beach shop and (later) cave traders.
class ShopScreen {
public:
    void open(const ItemDb& db, const SpriteDb& sprites, Inventory& inv, const ShopDef& shop);
    bool update(const Input& input);  // false when closed
    void render(SDL_Renderer* r) const;

private:
    void buy(const std::string& id);
    void sell(std::size_t backpackIndex);
    bool onStock() const { return cursor_ < static_cast<int>(shop_->stock.size()); }

    const ItemDb* db_ = nullptr;
    const SpriteDb* sprites_ = nullptr;
    Inventory* inv_ = nullptr;
    const ShopDef* shop_ = nullptr;

    std::vector<SDL_FRect> rects_;  // stock slots first, then the 9 backpack slots
    int cursor_ = 0;
    std::string message_;
    bool messageBad_ = false;
};
