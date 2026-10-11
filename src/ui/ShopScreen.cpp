#include "ui/ShopScreen.h"

#include "core/Input.h"
#include "core/Sprites.h"
#include "ui/Ui.h"

namespace {
constexpr SDL_FRect kStockPanel{16, 24, 608, 150};
constexpr SDL_FRect kPackPanel{16, 184, 608, 64};
constexpr SDL_FRect kInfoPanel{16, 258, 608, 78};
constexpr float kPitch = 40.0f;
constexpr std::size_t kStockColumns = 14;
}  // namespace

void ShopScreen::open(const ItemDb& db, const SpriteDb& sprites, Inventory& inv, const ShopDef& shop) {
    db_ = &db;
    sprites_ = &sprites;
    inv_ = &inv;
    shop_ = &shop;
    cursor_ = 0;
    message_ = shop.greeting;
    messageBad_ = false;

    rects_.clear();
    for (std::size_t i = 0; i < shop.stock.size(); ++i) {
        rects_.push_back({kStockPanel.x + 16 + static_cast<float>(i % kStockColumns) * kPitch,
                          kStockPanel.y + 30 + static_cast<float>(i / kStockColumns) * (kPitch + 12), ui::kSlotSize,
                          ui::kSlotSize});
    }
    for (std::size_t i = 0; i < Inventory::kBackpackSlots; ++i) {
        rects_.push_back({kPackPanel.x + 110 + static_cast<float>(i) * kPitch, kPackPanel.y + 14, ui::kSlotSize,
                          ui::kSlotSize});
    }
}

void ShopScreen::buy(const std::string& id) {
    const ItemDef& def = db_->get(id);
    if (inv_->coins < def.value) {
        message_ = "You can't afford the " + def.name + ".";
        messageBad_ = true;
        ui::sound("error");
    } else if (inv_->backpack.roomFor(*db_, id) <= 0) {
        message_ = "Your backpack is full.";
        messageBad_ = true;
        ui::sound("error");
    } else {
        inv_->backpack.add(*db_, id, 1);
        inv_->coins -= def.value;
        ui::sound("buy");
        message_ = "Bought " + def.name + " for " + std::to_string(def.value) + ".";
        messageBad_ = false;
    }
}

void ShopScreen::sell(std::size_t backpackIndex) {
    const ItemStack& s = inv_->backpack.at(backpackIndex);
    if (s.empty()) return;
    const ItemDef& def = db_->get(s.id);
    if (!def.sellable || def.sellPrice() <= 0) {
        message_ = "\"I can't buy that from you.\"";
        messageBad_ = true;
        return;
    }
    ui::sound("buy");
    inv_->coins += def.sellPrice();
    inv_->backpack.takeFrom(backpackIndex, 1);
    message_ = "Sold " + def.name + " for " + std::to_string(def.sellPrice()) + ".";
    messageBad_ = false;
}

bool ShopScreen::update(const Input& input) {
    if (input.pressed(Action::Cancel) || input.pressed(Action::Interact)) return false;
    cursor_ = ui::navigate(rects_, cursor_, input);
    if (input.pressed(Action::Confirm)) {
        if (onStock()) buy(shop_->stock[static_cast<std::size_t>(cursor_)]);
        else sell(static_cast<std::size_t>(cursor_) - shop_->stock.size());
    }
    return true;
}

void ShopScreen::render(SDL_Renderer* r) const {
    ui::dimScreen(r);
    ui::panel(r, kStockPanel, shop_->name.c_str());
    ui::panel(r, kPackPanel, "Backpack");
    ui::panel(r, kInfoPanel);

    for (std::size_t i = 0; i < shop_->stock.size(); ++i) {
        const ItemDef& def = db_->get(shop_->stock[i]);
        const SDL_FRect& rc = rects_[i];
        ui::slot(r, *sprites_, *db_, rc, ItemStack{def.id, 1}, static_cast<int>(i) == cursor_, false);
        const std::string price = std::to_string(def.value);
        ui::textCentered(r, rc.x + rc.w * 0.5f, rc.y + rc.h + 3, price,
                         inv_->coins >= def.value ? ui::kGold : ui::kBad);
    }
    for (std::size_t i = 0; i < Inventory::kBackpackSlots; ++i) {
        const std::size_t ri = shop_->stock.size() + i;
        ui::slot(r, *sprites_, *db_, rects_[ri], inv_->backpack.at(i), static_cast<int>(ri) == cursor_, false);
    }
    sprites_->drawIcon(r, "coins", kPackPanel.x + 508, kPackPanel.y + 16);
    ui::text(r, kPackPanel.x + 544, kPackPanel.y + 28, std::to_string(inv_->coins), ui::kGold);

    // Info about the item under the cursor.
    const ItemStack stack = onStock() ? ItemStack{shop_->stock[static_cast<std::size_t>(cursor_)], 1}
                                      : inv_->backpack.at(static_cast<std::size_t>(cursor_) - shop_->stock.size());
    if (!stack.empty()) {
        const ItemDef& def = db_->get(stack.id);
        ui::text(r, kInfoPanel.x + 12, kInfoPanel.y + 10, def.name, ui::kGold);
        const std::string price = onStock() ? "Buy for " + std::to_string(def.value)
                                            : (def.sellable ? "Sell for " + std::to_string(def.sellPrice()) : "Not for sale");
        ui::text(r, kInfoPanel.x + kInfoPanel.w - 12 - ui::textWidth(price), kInfoPanel.y + 10, price, ui::kGold);
        const auto lines = ui::wrap(def.description, 72);
        auto effects = def.equipped.describe();
        const auto carried = def.carried.describe();
        effects.insert(effects.end(), carried.begin(), carried.end());
        std::string line = lines.empty() ? "" : lines[0];
        for (const std::string& e : effects) line += (line.empty() ? "" : "  ") + std::string("[") + e + "]";
        ui::text(r, kInfoPanel.x + 12, kInfoPanel.y + 24, line.substr(0, 74));
    }
    ui::text(r, kInfoPanel.x + 12, kInfoPanel.y + 42, message_, messageBad_ ? ui::kBad : ui::kGood);
    ui::text(r, kInfoPanel.x + 12, kInfoPanel.y + 60, "Enter: buy / sell one   Esc: leave", ui::kDim);
}
