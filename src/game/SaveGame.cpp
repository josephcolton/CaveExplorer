#include "game/SaveGame.h"

using nlohmann::json;

json SaveGame::toJson() const {
    return {{"version", kVersion},
            {"world", world.toJson()},
            {"character", character.toJson()},
            {"inventory", inventory.toJson()},
            {"taken_pickups", takenPickups},
            {"abilities", abilities},
            {"quests", quests.toJson()},
            {"hunger", hunger},
            {"buffs", [&] {
                 json list = json::array();
                 for (const TimedBuff& b : buffs) list.push_back({{"ability", b.ability}, {"remaining", b.remaining}});
                 return list;
             }()},
            {"health", health},
            {"area", area},
            {"area_name", areaName},
            {"entry_spawn", entrySpawn},
            {"position", hasPosition ? json{x, y} : json(nullptr)},
            {"play_seconds", playSeconds},
            {"saved_at", savedAt}};
}

bool SaveGame::fromJson(const json& j) {
    if (!j.is_object() || j.value("version", 0) < 1 || j.value("version", 0) > kVersion) return false;
    world.fromJson(j.value("world", json::object()));
    character.fromJson(j.value("character", json::object()));
    inventory.fromJson(j.value("inventory", json::object()));
    takenPickups = j.value("taken_pickups", std::set<std::string>{});
    abilities = j.value("abilities", std::vector<std::string>{});
    quests.fromJson(j.value("quests", json::object()));
    hunger = j.value("hunger", 100.0f);
    buffs.clear();
    for (const json& b : j.value("buffs", json::array())) {
        buffs.push_back({b.value("ability", ""), b.value("remaining", 0.0f)});
    }
    health = j.value("health", character.maxHealth);
    area = j.value("area", "beach");
    areaName = j.value("area_name", area);
    entrySpawn = j.value("entry_spawn", "start");
    const json pos = j.value("position", json());
    hasPosition = pos.is_array() && pos.size() == 2;
    if (hasPosition) {
        x = pos[0].get<float>();
        y = pos[1].get<float>();
    }
    playSeconds = j.value("play_seconds", 0.0);
    savedAt = j.value("saved_at", "");
    return true;
}
