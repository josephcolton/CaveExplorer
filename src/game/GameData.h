#pragma once

#include "core/Animation.h"
#include "core/Sprites.h"
#include "entity/Enemy.h"
#include "entity/Obstacle.h"
#include "game/Items.h"
#include "ui/ShopScreen.h"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>

class Assets;

// Static game content loaded once at startup from data/ and assets/.
struct GameData {
    ItemDb items;
    std::unordered_map<std::string, ShopDef> shops;
    std::unordered_map<std::string, EnemyDef> enemies;
    std::unordered_map<std::string, ObstacleDef> obstacles;
    nlohmann::json characterConfig;
    SpriteDb sprites;
    AnimDb anims;
    std::string creditsText;

    void load(Assets& assets);
};
