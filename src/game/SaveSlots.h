#pragma once

#include "game/SaveGame.h"

#include <string>

// Reading and writing save slots (slot1.json ... slot3.json in the per-user save folder).
namespace saveslots {

bool exists(int slot);
bool load(int slot, SaveGame& out);
// Stamps savedAt and writes atomically.
bool write(int slot, SaveGame& save);
void remove(int slot);

std::string formatPlayTime(double seconds);  // "1:02:03"

}  // namespace saveslots
