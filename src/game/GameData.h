#pragma once

#include "core/Animation.h"
#include "core/Sprites.h"
#include "entity/Enemy.h"
#include "entity/Obstacle.h"
#include "game/Items.h"
#include "game/Modifiers.h"
#include "game/Quests.h"
#include "ui/ShopScreen.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class Assets;

// A person in the caves (data/npcs.json).
struct NpcDef {
    std::string id;
    std::string name;
    std::string icon;
    std::string shop;      // shop id, or empty
    std::string greeting;
    std::string idle;      // what they say once you've done all their quests
    std::vector<std::string> quests;  // offered in order, one at a time
};

// Static game content loaded once at startup from data/ and assets/.
struct GameData {
    ItemDb items;
    std::unordered_map<std::string, ShopDef> shops;
    std::unordered_map<std::string, EnemyDef> enemies;
    std::unordered_map<std::string, ObstacleDef> obstacles;
    std::unordered_map<std::string, AbilityDef> abilities;
    std::unordered_map<std::string, NpcDef> npcs;
    std::map<std::string, QuestDef> quests;
    nlohmann::json characterConfig;
    SpriteDb sprites;
    AnimDb anims;
    std::string creditsText;

    void load(Assets& assets);
};
