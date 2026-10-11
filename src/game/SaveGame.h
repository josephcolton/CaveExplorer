#pragma once

#include "game/Character.h"
#include "game/Inventory.h"
#include "game/WorldSave.h"

#include <nlohmann/json.hpp>

#include <set>
#include <string>

// The complete contents of one save slot: the persistent world plus the
// current explorer and where they are.
struct SaveGame {
    static constexpr int kVersion = 1;
    static constexpr int kSlotCount = 3;

    WorldSave world;
    Character character;
    Inventory inventory;
    // Placed coins/items ("area#objectId") this explorer already took. They stay gone until a
    // new explorer arrives, so walking back and forth can't farm them.
    std::set<std::string> takenPickups;
    float health = 100.0f;
    std::string area = "beach";
    std::string areaName = "The Beach";
    float x = 0.0f;  // feet position in the area; ignored if !hasPosition
    float y = 0.0f;
    bool hasPosition = false;
    double playSeconds = 0.0;
    std::string savedAt;  // human-readable local time

    nlohmann::json toJson() const;
    // Returns false if the JSON isn't a save we understand.
    bool fromJson(const nlohmann::json& j);
};
