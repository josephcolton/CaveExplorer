#pragma once

#include "game/Inventory.h"

#include <nlohmann/json.hpp>

#include <set>
#include <string>

// Everything that persists across explorers (characters): the forest chest,
// unique items already taken from the world, and (later) corpse piles.
struct WorldSave {
    static constexpr int kVersion = 1;
    static constexpr std::size_t kChestSlots = 24;

    WorldSave() : chest(kChestSlots) {}

    Container chest;
    std::set<std::string> takenUniques;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};
