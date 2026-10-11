#pragma once

#include "game/Items.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct ItemStack {
    std::string id;
    int count = 0;

    bool empty() const { return count <= 0 || id.empty(); }
    void clear() { id.clear(); count = 0; }
};

// A fixed number of slots. Each slot may be restricted to one EquipSlot kind
// (EquipSlot::None = accepts anything). Used for the backpack, equipment, and chests.
class Container {
public:
    Container() = default;
    explicit Container(std::size_t size, EquipSlot kind = EquipSlot::None);
    Container(std::vector<EquipSlot> kinds);

    std::size_t size() const { return slots_.size(); }
    ItemStack& at(std::size_t i) { return slots_[i]; }
    const ItemStack& at(std::size_t i) const { return slots_[i]; }
    EquipSlot kindAt(std::size_t i) const { return kinds_[i]; }

    bool accepts(std::size_t i, const ItemDef& def) const;

    // Adds items, topping up existing stacks first. Returns how many did NOT fit.
    int add(const ItemDb& db, const std::string& id, int count);
    // How many of this item could be added right now.
    int roomFor(const ItemDb& db, const std::string& id) const;
    int countOf(const std::string& id) const;
    // Removes up to count items from slot i. Returns how many were removed.
    int takeFrom(std::size_t i, int count);
    bool isEmpty() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::vector<ItemStack> slots_;
    std::vector<EquipSlot> kinds_;
};

// Move the stack in from[i] onto to[j]: moves into an empty slot, merges same items
// (up to the stack limit), or swaps two different items when both slots allow it.
// Returns false if nothing could move. from and to may be the same container.
bool transferStack(const ItemDb& db, Container& from, std::size_t i, Container& to, std::size_t j);

// The player's carried items.
struct Inventory {
    enum Equip : std::size_t { RightHand = 0, LeftHand, Feet, Body, Head, EquipCount };
    static constexpr std::size_t kBackpackSlots = 9;

    Inventory();

    Container backpack;
    Container equipment;
    int coins = 0;

    // Picking things up: goes into the backpack. Returns how many did NOT fit.
    int add(const ItemDb& db, const std::string& id, int count) { return backpack.add(db, id, count); }
    // True if the item is anywhere on the character (carried abilities use this).
    bool carries(const std::string& id) const { return backpack.countOf(id) + equipment.countOf(id) > 0; }

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};
