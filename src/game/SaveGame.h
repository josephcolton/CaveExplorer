#pragma once

#include "game/Character.h"
#include "game/Inventory.h"
#include "game/Quests.h"
#include "game/WorldSave.h"

#include <nlohmann/json.hpp>

#include <set>
#include <vector>
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
    std::vector<std::string> abilities;  // lasting abilities (until death)
    QuestLog quests;                     // this explorer's quests
    float hunger = 100.0f;               // 100 = well fed
    float lanternFuel = 0.0f;            // seconds left in the lantern
    struct TimedBuff {
        std::string ability;
        float remaining = 0.0f;  // seconds
    };
    std::vector<TimedBuff> buffs;        // shrine blessings still running

    float health = 100.0f;
    std::string area = "beach";
    std::string areaName = "The Beach";
    std::string entrySpawn = "start";  // the spawn point you entered this area by
    float x = 0.0f;  // feet position in the area; ignored if !hasPosition
    float y = 0.0f;
    bool hasPosition = false;
    double playSeconds = 0.0;
    std::string savedAt;  // human-readable local time

    nlohmann::json toJson() const;
    // Returns false if the JSON isn't a save we understand.
    bool fromJson(const nlohmann::json& j);
};
