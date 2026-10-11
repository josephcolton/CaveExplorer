#include "game/GameData.h"

#include "core/Assets.h"

using nlohmann::json;

namespace {
json loadJson(const char* file) {
    auto text = readTextFile(dataPath(file));
    if (!text) return json::object();
    json j = json::parse(*text, nullptr, false);
    if (j.is_discarded()) {
        SDL_Log("%s is not valid JSON", file);
        return json::object();
    }
    return j;
}
}  // namespace

void GameData::load(Assets& assets) {
    std::string error;
    if (auto text = readTextFile(dataPath("items.json")); !text || !items.loadFromJson(*text, &error)) {
        SDL_Log("items.json: %s", error.c_str());
    }

    const json shopDoc = loadJson("shops.json");
    const json shopList = shopDoc.value("shops", json::object());
    for (auto& [id, s] : shopList.items()) {
        ShopDef def;
        def.id = id;
        def.name = s.value("name", id);
        def.greeting = s.value("greeting", "");
        def.stock = s.value("stock", std::vector<std::string>{});
        shops[id] = std::move(def);
    }

    const json enemyDoc = loadJson("enemies.json");
    const json enemyList = enemyDoc.value("enemies", json::object());
    for (auto& [id, e] : enemyList.items()) enemies[id] = EnemyDef::fromJson(id, e);

    const json obstacleDoc = loadJson("obstacles.json");
    const json obstacleList = obstacleDoc.value("obstacles", json::object());
    for (auto& [id, o] : obstacleList.items()) obstacles[id] = ObstacleDef::fromJson(id, o);

    characterConfig = loadJson("characters.json");
    sprites.load(assets);
    anims.load(assets);
    creditsText = readTextFile(dataPath("credits.txt")).value_or("See CREDITS.md");
}
