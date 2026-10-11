#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// Which equipment slot an item can go in. Anything can go in the backpack or chest.
enum class EquipSlot { None, Hand, Feet, Body, Head };

const char* equipSlotName(EquipSlot slot);

// Static description of an item type, loaded from data/items.json.
struct ItemDef {
    std::string id;
    std::string name;
    std::string description;
    std::string icon;
    std::string category;  // weapon, ranged, ammo, light, tool, clothing, food, potion, valuable...
    EquipSlot slot = EquipSlot::None;
    int maxStack = 1;
    int value = 0;  // shop price; the shop buys for half
    bool sellable = true;
    bool unique = false;
    // Gameplay numbers; systems that use them arrive in later steps.
    int damage = 0;
    float reach = 24.0f;            // melee: hitbox length in front of the explorer (px)
    float cooldown = 0.35f;         // seconds between attacks / shots
    std::string ammo;               // ranged: item id consumed per shot (empty = melee)
    float projectileSpeed = 400.0f; // ranged: launch speed (px/s)
    float food = 0.0f;
    float heal = 0.0f;
    std::vector<std::string> tags;  // e.g. "cut" (clears bushes), "break" (breaks rocks)

    bool hasTag(const std::string& tag) const;
    bool isRanged() const { return !ammo.empty(); }
    bool isMelee() const { return damage > 0 && ammo.empty(); }
    int sellPrice() const { return value > 1 ? value / 2 : (value > 0 ? 1 : 0); }
};

class ItemDb {
public:
    // Parses the JSON text of data/items.json. Returns false and fills error on failure.
    bool loadFromJson(const std::string& text, std::string* error = nullptr);

    const ItemDef* find(const std::string& id) const;
    const ItemDef& get(const std::string& id) const;  // falls back to an "unknown item" def
    std::size_t size() const { return items_.size(); }

private:
    std::unordered_map<std::string, ItemDef> items_;
    ItemDef unknown_ = [] {
        ItemDef d;
        d.id = "unknown";
        d.name = "???";
        d.sellable = false;
        return d;
    }();
};
