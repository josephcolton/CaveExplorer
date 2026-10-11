#pragma once

#include <nlohmann/json.hpp>

#include <set>
#include <string>
#include <vector>

class ItemDb;
struct Inventory;

// Changes to an explorer's abilities, from gear (carried or worn), quest rewards and
// (later) shrines. Multipliers stack by multiplying; max health adds.
struct Modifiers {
    float runSpeed = 1.0f;
    float jump = 1.0f;
    float damage = 1.0f;       // damage dealt
    float defense = 1.0f;      // damage taken (0.8 = take 20% less)
    float maxHealth = 0.0f;    // added to max health
    float lightRadius = 0.0f;  // light carried with you (the biggest source wins)
    std::set<std::string> flags;  // e.g. "nonslip", "warm", "light"

    void combine(const Modifiers& o);
    bool empty() const;
    // Human-readable effects, e.g. "Jump +15%", "Damage taken -10%".
    std::vector<std::string> describe() const;

    static Modifiers fromJson(const nlohmann::json& j);
    bool has(const std::string& flag) const { return flags.count(flag) > 0; }
};

// A lasting ability (until the explorer dies), e.g. a quest reward.
struct AbilityDef {
    std::string id;
    std::string name;
    std::string description;
    Modifiers modifiers;
};

// Sum of everything the explorer's gear does: "carried" effects for every item anywhere
// on them, plus "equipped" effects for items in equipment slots (hands, feet, body, head).
Modifiers gearModifiers(const Inventory& inv, const ItemDb& db);
