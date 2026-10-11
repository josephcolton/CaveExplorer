#pragma once

#include <nlohmann/json.hpp>

#include <random>
#include <string>

// One explorer. Each new explorer (after a death, or a new game) is rolled with
// fresh stats from data/characters.json.
struct Character {
    std::string name;
    int number = 1;            // 1st, 2nd, 3rd... explorer in this save
    float maxHealth = 100.0f;
    float strength = 1.0f;     // melee/ranged damage multiplier
    float speed = 1.0f;        // run speed multiplier
    float jump = 1.0f;         // jump velocity multiplier
    int startingCoins = 40;

    // config is the parsed data/characters.json.
    static Character roll(const nlohmann::json& config, std::mt19937& rng, int number);

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};
