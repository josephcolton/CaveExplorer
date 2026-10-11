#pragma once

#include "game/Inventory.h"

#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <vector>

// Everything a fallen explorer was carrying, left where they died.
struct CorpsePile {
    std::string area;
    float x = 0.0f;  // feet position
    float y = 0.0f;
    std::string explorer;
    int coins = 0;
    std::vector<ItemStack> items;

    bool empty() const { return coins <= 0 && items.empty(); }
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

// Everything that persists across explorers (characters) within one save slot:
// the forest chest, unique items already taken, and corpse piles.
struct WorldSave {
    static constexpr std::size_t kChestSlots = 24;

    WorldSave() : chest(kChestSlots) {}

    Container chest;
    std::set<std::string> takenUniques;
    std::vector<CorpsePile> corpses;
    int explorersLost = 0;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};
