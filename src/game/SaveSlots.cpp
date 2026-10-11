#include "game/SaveSlots.h"

#include "core/Assets.h"

#include <SDL3/SDL.h>

#include <cstdio>

namespace saveslots {

namespace {
std::string path(int slot) { return savePath("slot" + std::to_string(slot) + ".json"); }

std::string now() {
    SDL_Time ticks = 0;
    SDL_DateTime dt{};
    if (!SDL_GetCurrentTime(&ticks) || !SDL_TimeToDateTime(ticks, &dt, true)) return "";
    char buf[32];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d %02d:%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute);
    return buf;
}
}  // namespace

bool exists(int slot) { return SDL_GetPathInfo(path(slot).c_str(), nullptr); }

bool load(int slot, SaveGame& out) {
    auto text = readTextFile(path(slot));
    if (!text) return false;
    const auto j = nlohmann::json::parse(*text, nullptr, false);
    return !j.is_discarded() && out.fromJson(j);
}

bool write(int slot, SaveGame& save) {
    save.savedAt = now();
    return writeTextFile(path(slot), save.toJson().dump(1));
}

void remove(int slot) { SDL_RemovePath(path(slot).c_str()); }

std::string formatPlayTime(double seconds) {
    const long s = static_cast<long>(seconds);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60);
    return buf;
}

}  // namespace saveslots
