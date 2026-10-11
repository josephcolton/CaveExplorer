#include "game/WorldSave.h"

using nlohmann::json;

json WorldSave::toJson() const {
    return {{"version", kVersion}, {"chest", chest.toJson()}, {"taken_uniques", takenUniques}};
}

void WorldSave::fromJson(const json& j) {
    chest.fromJson(j.value("chest", json::array()));
    takenUniques = j.value("taken_uniques", std::set<std::string>{});
}
