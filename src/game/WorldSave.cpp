#include "game/WorldSave.h"

using nlohmann::json;

json CorpsePile::toJson() const {
    json list = json::array();
    for (const ItemStack& s : items) list.push_back({{"id", s.id}, {"count", s.count}});
    return {{"area", area}, {"x", x}, {"y", y}, {"explorer", explorer}, {"coins", coins}, {"items", list}};
}

void CorpsePile::fromJson(const json& j) {
    area = j.value("area", "");
    x = j.value("x", 0.0f);
    y = j.value("y", 0.0f);
    explorer = j.value("explorer", "");
    coins = j.value("coins", 0);
    items.clear();
    for (const json& s : j.value("items", json::array())) {
        ItemStack st{s.value("id", ""), s.value("count", 0)};
        if (!st.empty()) items.push_back(st);
    }
}

json WorldSave::toJson() const {
    json piles = json::array();
    for (const CorpsePile& c : corpses) piles.push_back(c.toJson());
    return {{"chest", chest.toJson()},
            {"taken_uniques", takenUniques},
            {"corpses", piles},
            {"explorers_lost", explorersLost}};
}

void WorldSave::fromJson(const json& j) {
    chest.fromJson(j.value("chest", json::array()));
    takenUniques = j.value("taken_uniques", std::set<std::string>{});
    corpses.clear();
    for (const json& c : j.value("corpses", json::array())) {
        CorpsePile pile;
        pile.fromJson(c);
        if (!pile.empty()) corpses.push_back(std::move(pile));
    }
    explorersLost = j.value("explorers_lost", 0);
}
