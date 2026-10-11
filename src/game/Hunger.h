#pragma once

#include "game/Modifiers.h"

// Hunger: 100 = well fed, 0 = starving. Drains while exploring; food refills it.
namespace hunger {

constexpr float kMax = 100.0f;
constexpr float kHungry = 35.0f;    // below this, natural healing stops
constexpr float kStarving = 15.0f;  // below this, you're slower and jump lower
constexpr float kDrainPerSecond = kMax / (15.0f * 60.0f);  // full to empty in ~15 minutes

enum class Level { Fed, Hungry, Starving };

inline Level level(float h) {
    if (h < kStarving) return Level::Starving;
    if (h < kHungry) return Level::Hungry;
    return Level::Fed;
}

inline bool canHeal(float h) { return h >= kHungry; }

// Penalties while starving.
inline Modifiers penalty(float h) {
    Modifiers m;
    if (level(h) == Level::Starving) {
        m.runSpeed = 0.8f;
        m.jump = 0.88f;
    }
    return m;
}

}  // namespace hunger
