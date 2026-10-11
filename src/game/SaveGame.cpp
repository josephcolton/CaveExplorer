#include "game/SaveGame.h"

using nlohmann::json;

json SaveGame::toJson() const {
    return {{"version", kVersion},
            {"world", world.toJson()},
            {"character", character.toJson()},
            {"inventory", inventory.toJson()},
            {"taken_pickups", takenPickups},
            {"health", health},
            {"area", area},
            {"area_name", areaName},
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
    health = j.value("health", character.maxHealth);
    area = j.value("area", "beach");
    areaName = j.value("area_name", area);
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
