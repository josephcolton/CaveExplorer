#pragma once

#include <nlohmann/json.hpp>

#include <string>

enum class Difficulty { Easy = 0, Normal = 1, Hard = 2 };

const char* difficultyName(Difficulty d);

// Player preferences shared by all save slots (settings.json in the save folder).
struct Settings {
    int musicVolume = 7;  // 0..10
    int soundVolume = 8;  // 0..10
    bool fullscreen = false;
    Difficulty difficulty = Difficulty::Normal;
    nlohmann::json bindings;  // control bindings (empty = defaults)

    void load();
    void save() const;
};
