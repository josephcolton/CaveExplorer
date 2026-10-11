#include "game/Modifiers.h"

#include "game/Inventory.h"

#include <cmath>

using nlohmann::json;

namespace {
std::string percent(const char* label, float mult) {
    const int pct = static_cast<int>(std::lround((mult - 1.0f) * 100.0f));
    return std::string(label) + (pct >= 0 ? " +" : " ") + std::to_string(pct) + "%";
}

const char* flagText(const std::string& flag) {
    if (flag == "nonslip") return "Sure footing on slick rock";
    if (flag == "warm") return "Keeps out the cold";
    if (flag == "light") return "Gives light";
    return nullptr;
}
}  // namespace

void Modifiers::combine(const Modifiers& o) {
    runSpeed *= o.runSpeed;
    jump *= o.jump;
    damage *= o.damage;
    defense *= o.defense;
    maxHealth += o.maxHealth;
    flags.insert(o.flags.begin(), o.flags.end());
}

bool Modifiers::empty() const {
    return runSpeed == 1.0f && jump == 1.0f && damage == 1.0f && defense == 1.0f && maxHealth == 0.0f && flags.empty();
}

std::vector<std::string> Modifiers::describe() const {
    std::vector<std::string> out;
    if (maxHealth != 0.0f) out.push_back("Max health " + std::string(maxHealth > 0 ? "+" : "") +
                                         std::to_string(static_cast<int>(maxHealth)));
    if (runSpeed != 1.0f) out.push_back(percent("Speed", runSpeed));
    if (jump != 1.0f) out.push_back(percent("Jump", jump));
    if (damage != 1.0f) out.push_back(percent("Damage", damage));
    if (defense != 1.0f) out.push_back(percent("Damage taken", defense));
    for (const std::string& f : flags) {
        if (const char* t = flagText(f)) out.emplace_back(t);
    }
    return out;
}

Modifiers Modifiers::fromJson(const json& j) {
    Modifiers m;
    if (!j.is_object()) return m;
    m.runSpeed = j.value("speed", 1.0f);
    m.jump = j.value("jump", 1.0f);
    m.damage = j.value("damage", 1.0f);
    m.defense = j.value("defense", 1.0f);
    m.maxHealth = j.value("max_health", 0.0f);
    const auto flags = j.value("flags", std::vector<std::string>{});
    m.flags.insert(flags.begin(), flags.end());
    return m;
}

Modifiers gearModifiers(const Inventory& inv, const ItemDb& db) {
    Modifiers total;
    for (std::size_t i = 0; i < inv.backpack.size(); ++i) {
        const ItemStack& s = inv.backpack.at(i);
        if (!s.empty()) total.combine(db.get(s.id).carried);
    }
    for (std::size_t i = 0; i < inv.equipment.size(); ++i) {
        const ItemStack& s = inv.equipment.at(i);
        if (s.empty()) continue;
        total.combine(db.get(s.id).carried);
        total.combine(db.get(s.id).equipped);
    }
    return total;
}
