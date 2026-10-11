#include "game/Character.h"

#include <cmath>

using nlohmann::json;

namespace {
float rollRange(const json& stats, const char* key, float lo, float hi, std::mt19937& rng) {
    if (stats.contains(key) && stats[key].is_array() && stats[key].size() == 2) {
        lo = stats[key][0].get<float>();
        hi = stats[key][1].get<float>();
    }
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}

std::string pick(const json& list, const char* fallback, std::mt19937& rng) {
    if (!list.is_array() || list.empty()) return fallback;
    const auto i = std::uniform_int_distribution<std::size_t>(0, list.size() - 1)(rng);
    return list[i].get<std::string>();
}
}  // namespace

Character Character::roll(const json& config, std::mt19937& rng, int number) {
    Character c;
    c.number = number;
    c.name = pick(config.value("first_names", json::array()), "Explorer", rng) + " " +
             pick(config.value("last_names", json::array()), "", rng);
    const json stats = config.value("stats", json::object());
    c.maxHealth = std::round(rollRange(stats, "max_health", 80, 120, rng));
    // Round multipliers to 2 decimals so they read nicely in the UI.
    c.strength = std::round(rollRange(stats, "strength", 0.85f, 1.2f, rng) * 100.0f) / 100.0f;
    c.speed = std::round(rollRange(stats, "speed", 0.92f, 1.1f, rng) * 100.0f) / 100.0f;
    c.jump = std::round(rollRange(stats, "jump", 0.94f, 1.07f, rng) * 100.0f) / 100.0f;
    c.startingCoins = static_cast<int>(std::round(rollRange(config, "starting_coins", 25, 60, rng)));
    return c;
}

json Character::toJson() const {
    return {{"name", name},           {"number", number}, {"max_health", maxHealth}, {"strength", strength},
            {"speed", speed},         {"jump", jump},     {"starting_coins", startingCoins}};
}

void Character::fromJson(const json& j) {
    name = j.value("name", "Explorer");
    number = j.value("number", 1);
    maxHealth = j.value("max_health", 100.0f);
    strength = j.value("strength", 1.0f);
    speed = j.value("speed", 1.0f);
    jump = j.value("jump", 1.0f);
    startingCoins = j.value("starting_coins", 40);
}
