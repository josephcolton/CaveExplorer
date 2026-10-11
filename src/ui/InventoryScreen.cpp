#include "ui/InventoryScreen.h"

#include "core/Input.h"
#include "core/Sprites.h"
#include "ui/Ui.h"

namespace {
constexpr SDL_FRect kGearPanel{16, 24, 200, 312};
constexpr SDL_FRect kChestPanel{228, 24, 396, 150};
constexpr float kPitch = 40.0f;
}  // namespace

void InventoryScreen::open(const ItemDb& db, const SpriteDb& sprites, Inventory& inv, Container* chest) {
    db_ = &db;
    sprites_ = &sprites;
    inv_ = &inv;
    chest_ = chest;
    selected_ = -1;
    message_.clear();
    buildLayout();
    // Start on the first backpack slot.
    cursor_ = static_cast<int>(Inventory::EquipCount);
}

void InventoryScreen::buildLayout() {
    slots_.clear();
    const float cx = kGearPanel.x + kGearPanel.w * 0.5f;
    const float s = ui::kSlotSize;
    auto add = [&](Container* c, std::size_t i, float x, float y, const char* label) {
        slots_.push_back({c, i, SDL_FRect{x, y, s, s}, label});
    };
    // Paper doll: head on top, hands either side of the body, feet below.
    Container* eq = &inv_->equipment;
    add(eq, Inventory::RightHand, cx - s * 0.5f - 52, 76, "R");
    add(eq, Inventory::LeftHand, cx - s * 0.5f + 52, 76, "L");
    add(eq, Inventory::Feet, cx - s * 0.5f, 116, "Feet");
    add(eq, Inventory::Body, cx - s * 0.5f, 76, "Body");
    add(eq, Inventory::Head, cx - s * 0.5f, 36, "Head");
    // Backpack 3x3.
    for (std::size_t i = 0; i < Inventory::kBackpackSlots; ++i) {
        add(&inv_->backpack, i, cx - s * 0.5f + (static_cast<float>(i % 3) - 1) * kPitch,
            176 + static_cast<float>(i / 3) * kPitch, nullptr);
    }
    if (chest_) {
        const std::size_t cols = 9;
        for (std::size_t i = 0; i < chest_->size(); ++i) {
            add(chest_, i, kChestPanel.x + 18 + static_cast<float>(i % cols) * kPitch,
                kChestPanel.y + 26 + static_cast<float>(i / cols) * kPitch, nullptr);
        }
    }
    rects_.clear();
    for (const Slot& sl : slots_) rects_.push_back(sl.rect);
}

InventoryRequest InventoryScreen::update(const Input& input) {
    InventoryRequest req;
    cursor_ = ui::navigate(rects_, cursor_, input);
    const Slot& cur = slots_[static_cast<std::size_t>(cursor_)];

    if (input.pressed(Action::Cancel) || input.pressed(Action::Inventory)) {
        if (selected_ >= 0 && !input.pressed(Action::Inventory)) {
            selected_ = -1;
        } else {
            req.kind = InventoryRequest::Kind::Close;
        }
        return req;
    }

    if (input.pressed(Action::Confirm)) {
        if (selected_ < 0) {
            if (!stackAt(cursor_).empty()) {
                selected_ = cursor_;
                message_.clear();
            }
        } else {
            const Slot& from = slots_[static_cast<std::size_t>(selected_)];
            if (selected_ == cursor_) {
                selected_ = -1;
            } else if (transferStack(*db_, *from.container, from.index, *cur.container, cur.index)) {
                selected_ = -1;
                message_.clear();
            } else {
                setMessage("That doesn't fit there.", true);
            }
        }
        return req;
    }

    const ItemStack& stack = stackAt(cursor_);
    if (stack.empty()) return req;

    if (input.pressed(Action::Drop)) {
        req.kind = InventoryRequest::Kind::Drop;
        req.stack = stack;
        cur.container->takeFrom(cur.index, stack.count);
        selected_ = -1;
    } else if (input.pressed(Action::Interact)) {
        const ItemDef& def = db_->get(stack.id);
        if (def.food > 0.0f || def.heal > 0.0f) {
            req.kind = InventoryRequest::Kind::Use;
            req.stack = {stack.id, 1};
            cur.container->takeFrom(cur.index, 1);
            if (stackAt(cursor_).empty() && selected_ == cursor_) selected_ = -1;
        } else {
            setMessage("You can't use that directly.", true);
        }
    }
    return req;
}

void InventoryScreen::render(SDL_Renderer* r) const {
    ui::dimScreen(r);
    ui::panel(r, kGearPanel, "Explorer");
    ui::text(r, kGearPanel.x + 8, 162, "Backpack", ui::kGold);
    if (chest_) ui::panel(r, kChestPanel, "Storage Chest");

    for (std::size_t i = 0; i < slots_.size(); ++i) {
        const Slot& s = slots_[i];
        ui::slot(r, *sprites_, *db_, s.rect, s.container->at(s.index), static_cast<int>(i) == cursor_,
                 static_cast<int>(i) == selected_, s.emptyLabel);
    }

    // Coins
    sprites_->drawIcon(r, "coins", kGearPanel.x + 10, kGearPanel.y + kGearPanel.h - 38);
    ui::text(r, kGearPanel.x + 46, kGearPanel.y + kGearPanel.h - 26, std::to_string(inv_->coins) + " coins", ui::kGold);

    // Details of the item under the cursor.
    const SDL_FRect info = chest_ ? SDL_FRect{228, 186, 396, 150} : SDL_FRect{228, 24, 396, 312};
    ui::panel(r, info);
    float y = info.y + 10;
    const ItemStack& stack = stackAt(cursor_);
    if (!stack.empty()) {
        const ItemDef& def = db_->get(stack.id);
        sprites_->drawIcon(r, def.icon, info.x + 10, y);
        ui::text(r, info.x + 50, y + 4, def.name, ui::kGold);
        std::string sub = def.category;
        if (def.slot != EquipSlot::None) sub += std::string("  -  ") + equipSlotName(def.slot);
        ui::text(r, info.x + 50, y + 17, sub, ui::kDim);
        y += 42;
        for (const std::string& line : ui::wrap(def.description, 46)) {
            ui::text(r, info.x + 12, y, line);
            y += ui::kLineH;
        }
        y += 4;
        if (def.damage > 0) { ui::text(r, info.x + 12, y, "Damage " + std::to_string(def.damage)); y += ui::kLineH; }
        if (def.food > 0) { ui::text(r, info.x + 12, y, "Food +" + std::to_string(static_cast<int>(def.food))); y += ui::kLineH; }
        if (def.heal > 0) { ui::text(r, info.x + 12, y, "Heals " + std::to_string(static_cast<int>(def.heal))); y += ui::kLineH; }
        const int worth = def.sellPrice();
        ui::text(r, info.x + 12, y, "Worth " + std::to_string(worth) + (worth == 1 ? " coin" : " coins"), ui::kDim);
    }

    if (!message_.empty()) ui::text(r, info.x + 12, info.y + info.h - 34, message_, messageBad_ ? ui::kBad : ui::kGood);
    ui::text(r, info.x + 12, info.y + info.h - 18,
             selected_ >= 0 ? "Enter: place here   Esc: cancel" : "Enter: move   E: use   Q: drop   Tab: close",
             ui::kDim);
}
