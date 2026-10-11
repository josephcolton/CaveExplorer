#pragma once

#include "core/Math.h"
#include "world/Tilemap.h"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

class Assets;

// An object placed in the Tiled "objects" layer (spawn points, doors, enemies, ...).
struct MapObject {
    int id = 0;
    std::string name;
    std::string type;
    Rect rect;  // point objects have w = h = 0
    std::unordered_map<std::string, nlohmann::json> properties;

    std::string getString(const std::string& key, const std::string& fallback = "") const;
    float getFloat(const std::string& key, float fallback = 0.0f) const;
    bool getBool(const std::string& key, bool fallback = false) const;
};

// One loaded area of the world (beach, forest, cave_1, ...). Loaded fresh from its
// .tmj file every time the player enters, which is what "refreshes" the area.
class Area {
public:
    bool load(const std::string& areaId, Assets& assets);

    std::string id;
    std::string displayName;
    std::string music;  // file in assets/soundtrack/, empty = silence
    float darkness = 0.0f;  // 0 = daylight, 1 = pitch black
    std::string restartSpawn;  // where "Restart area" puts you (the way back toward the surface)
    SDL_Color background{0, 0, 0, 255};
    Tilemap map;
    std::vector<MapObject> objects;

    const MapObject* findObject(const std::string& type, const std::string& name) const;
};
