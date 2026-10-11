#include "core/Settings.h"

#include "core/Assets.h"

#include <nlohmann/json.hpp>

#include <algorithm>

using nlohmann::json;

namespace {
constexpr const char* kFile = "settings.json";
}

const char* difficultyName(Difficulty d) {
    switch (d) {
    case Difficulty::Easy: return "Easy";
    case Difficulty::Hard: return "Hard";
    default: return "Normal";
    }
}

void Settings::load() {
    auto text = readTextFile(savePath(kFile));
    if (!text) return;  // first run: keep defaults
    json j = json::parse(*text, nullptr, false);
    if (!j.is_object()) return;
    musicVolume = std::clamp(j.value("music_volume", musicVolume), 0, 10);
    soundVolume = std::clamp(j.value("sound_volume", soundVolume), 0, 10);
    fullscreen = j.value("fullscreen", fullscreen);
    difficulty = static_cast<Difficulty>(std::clamp(j.value("difficulty", 1), 0, 2));
    bindings = j.value("bindings", json());
}

void Settings::save() const {
    const json j = {{"music_volume", musicVolume},
                    {"sound_volume", soundVolume},
                    {"fullscreen", fullscreen},
                    {"difficulty", static_cast<int>(difficulty)},
                    {"bindings", bindings}};
    writeTextFile(savePath(kFile), j.dump(1));
}
