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

    const json abilityDoc = loadJson("abilities.json");
    const json abilityList = abilityDoc.value("abilities", json::object());
    for (auto& [id, a] : abilityList.items()) {
        abilities[id] = {id, a.value("name", id), a.value("description", ""),
                         Modifiers::fromJson(a.value("modifiers", json::object()))};
    }

    const json npcDoc = loadJson("npcs.json");
    const json npcList = npcDoc.value("npcs", json::object());
    for (auto& [id, n] : npcList.items()) {
        npcs[id] = {id, n.value("name", id), n.value("icon", ""), n.value("shop", ""), n.value("greeting", ""),
                    n.value("idle", "Safe travels."), n.value("quests", std::vector<std::string>{})};
    }

    const json questDoc = loadJson("quests.json");
    const json questList = questDoc.value("quests", json::object());
    for (auto& [id, q] : questList.items()) quests[id] = QuestDef::fromJson(id, q);

    const json shrineDoc = loadJson("shrines.json");
    const json shrineList = shrineDoc.value("shrines", json::object());
    for (auto& [id, sh] : shrineList.items()) {
        ShrineDef d{id, sh.value("name", id), sh.value("icon", ""), sh.value("ability", ""), sh.value("duration", 60.0f)};
        const auto g = sh.value("glow", std::vector<int>{255, 255, 255});
        if (g.size() == 3) d.glow = {static_cast<Uint8>(g[0]), static_cast<Uint8>(g[1]), static_cast<Uint8>(g[2]), 255};
        shrines[id] = d;
    }

    characterConfig = loadJson("characters.json");
    sprites.load(assets);
    anims.load(assets);
    creditsText = readTextFile(dataPath("credits.txt")).value_or("See CREDITS.md");
}
