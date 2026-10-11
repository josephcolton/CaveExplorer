#include "game/Items.h"

#include <nlohmann/json.hpp>

#include <algorithm>

using nlohmann::json;

namespace {
EquipSlot parseSlot(const std::string& s) {
    if (s == "hand") return EquipSlot::Hand;
    if (s == "feet") return EquipSlot::Feet;
    if (s == "body") return EquipSlot::Body;
    if (s == "head") return EquipSlot::Head;
    return EquipSlot::None;
}
}  // namespace

const char* equipSlotName(EquipSlot slot) {
    switch (slot) {
    case EquipSlot::Hand: return "Hand";
    case EquipSlot::Feet: return "Feet";
    case EquipSlot::Body: return "Body";
    case EquipSlot::Head: return "Head";
    default: return "";
    }
}

bool ItemDef::hasTag(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

bool ItemDb::loadFromJson(const std::string& text, std::string* error) {
    json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("items") || !doc["items"].is_array()) {
        if (error) *error = "items.json must be an object with an \"items\" array";
        return false;
    }
    items_.clear();
    for (const json& j : doc["items"]) {
        ItemDef d;
        d.id = j.value("id", "");
        if (d.id.empty()) {
            if (error) *error = "item without id";
            return false;
        }
        d.name = j.value("name", d.id);
        d.description = j.value("description", "");
        d.icon = j.value("icon", d.id);
        d.category = j.value("category", "");
        d.slot = parseSlot(j.value("slot", ""));
        d.maxStack = std::max(1, j.value("stack", 1));
        d.value = j.value("value", 0);
        d.sellable = j.value("sellable", true);
        d.unique = j.value("unique", false);
        d.damage = j.value("damage", 0);
        d.food = j.value("food", 0.0f);
        d.heal = j.value("heal", 0.0f);
        d.tags = j.value("tags", std::vector<std::string>{});
        items_[d.id] = std::move(d);
    }
    return true;
}

const ItemDef* ItemDb::find(const std::string& id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : &it->second;
}

const ItemDef& ItemDb::get(const std::string& id) const {
    const ItemDef* d = find(id);
    return d ? *d : unknown_;
}
