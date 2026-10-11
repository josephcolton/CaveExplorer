#include "world/Area.h"

#include "core/Assets.h"

#include <filesystem>

using nlohmann::json;

namespace {

std::unordered_map<std::string, json> readProperties(const json& node) {
    std::unordered_map<std::string, json> props;
    if (auto it = node.find("properties"); it != node.end() && it->is_array()) {
        for (const json& p : *it) props[p.value("name", "")] = p.value("value", json());
    }
    return props;
}

// Tiled colors are "#rrggbb" or "#aarrggbb".
std::optional<SDL_Color> parseColor(const std::string& s) {
    if (s.size() != 7 && s.size() != 9) return std::nullopt;
    const unsigned long v = std::stoul(s.substr(1), nullptr, 16);
    SDL_Color c;
    c.r = static_cast<Uint8>((v >> 16) & 0xFF);
    c.g = static_cast<Uint8>((v >> 8) & 0xFF);
    c.b = static_cast<Uint8>(v & 0xFF);
    c.a = s.size() == 9 ? static_cast<Uint8>((v >> 24) & 0xFF) : 255;
    return c;
}

TileCollision parseCollision(const std::string& s) {
    if (s == "solid") return TileCollision::Solid;
    if (s == "oneway") return TileCollision::OneWay;
    return TileCollision::None;
}

}  // namespace

std::string MapObject::getString(const std::string& key, const std::string& fallback) const {
    auto it = properties.find(key);
    return (it != properties.end() && it->second.is_string()) ? it->second.get<std::string>() : fallback;
}

float MapObject::getFloat(const std::string& key, float fallback) const {
    auto it = properties.find(key);
    return (it != properties.end() && it->second.is_number()) ? it->second.get<float>() : fallback;
}

bool MapObject::getBool(const std::string& key, bool fallback) const {
    auto it = properties.find(key);
    return (it != properties.end() && it->second.is_boolean()) ? it->second.get<bool>() : fallback;
}

const MapObject* Area::findObject(const std::string& type, const std::string& name) const {
    for (const MapObject& o : objects) {
        if (o.type == type && o.name == name) return &o;
    }
    return nullptr;
}

bool Area::load(const std::string& areaId, Assets& assets) {
    const std::string path = assetPath("maps/" + areaId + ".tmj");
    auto text = readTextFile(path);
    if (!text) return false;

    json doc = json::parse(*text, nullptr, false);
    if (doc.is_discarded()) {
        SDL_Log("Invalid JSON in %s", path.c_str());
        return false;
    }

    id = areaId;
    map = Tilemap{};
    objects.clear();
    map.width = doc.value("width", 0);
    map.height = doc.value("height", 0);
    if (auto c = parseColor(doc.value("backgroundcolor", ""))) background = *c;

    const auto mapProps = readProperties(doc);
    auto nameIt = mapProps.find("display_name");
    displayName = (nameIt != mapProps.end() && nameIt->second.is_string()) ? nameIt->second.get<std::string>() : areaId;
    auto musicIt = mapProps.find("music");
    music = (musicIt != mapProps.end() && musicIt->second.is_string()) ? musicIt->second.get<std::string>() : "";

    const std::filesystem::path mapDir = std::filesystem::path(path).parent_path();

    for (const json& tsj : doc.value("tilesets", json::array())) {
        if (tsj.contains("source")) {
            SDL_Log("%s: external tilesets (.tsj) are not supported yet; embed the tileset", path.c_str());
            continue;
        }
        Tileset ts;
        ts.firstGid = tsj.value("firstgid", 1);
        ts.tileCount = tsj.value("tilecount", 0);
        ts.columns = std::max(1, tsj.value("columns", 1));
        ts.tileWidth = tsj.value("tilewidth", Tilemap::kTileSize);
        ts.tileHeight = tsj.value("tileheight", Tilemap::kTileSize);
        ts.collision.assign(static_cast<std::size_t>(ts.tileCount), TileCollision::None);
        ts.slippery.assign(static_cast<std::size_t>(ts.tileCount), false);

        const std::string image = (mapDir / tsj.value("image", "")).string();
        ts.texture = assets.texture(image, parseColor(tsj.value("transparentcolor", "")));

        for (const json& tile : tsj.value("tiles", json::array())) {
            const int localId = tile.value("id", -1);
            if (localId < 0 || localId >= ts.tileCount) continue;
            const auto props = readProperties(tile);
            if (auto it = props.find("collision"); it != props.end() && it->second.is_string()) {
                ts.collision[static_cast<std::size_t>(localId)] = parseCollision(it->second.get<std::string>());
            }
            if (auto it = props.find("surface"); it != props.end() && it->second == "slippery") {
                ts.slippery[static_cast<std::size_t>(localId)] = true;
            }
        }
        map.tilesets.push_back(std::move(ts));
    }

    for (const json& layer : doc.value("layers", json::array())) {
        const std::string type = layer.value("type", "");
        const auto props = readProperties(layer);
        auto boolProp = [&](const char* key) {
            auto it = props.find(key);
            return it != props.end() && it->second.is_boolean() && it->second.get<bool>();
        };

        if (type == "tilelayer") {
            TileLayer tl;
            tl.name = layer.value("name", "");
            tl.visible = layer.value("visible", true);
            tl.collides = boolProp("collides");
            tl.foreground = boolProp("foreground");
            tl.gids = layer.value("data", std::vector<std::uint32_t>{});
            tl.gids.resize(static_cast<std::size_t>(map.width * map.height), 0);
            map.layers.push_back(std::move(tl));
        } else if (type == "objectgroup") {
            for (const json& o : layer.value("objects", json::array())) {
                MapObject obj;
                obj.id = o.value("id", 0);
                obj.name = o.value("name", "");
                obj.type = o.value("type", o.value("class", ""));
                obj.rect = {o.value("x", 0.0f), o.value("y", 0.0f), o.value("width", 0.0f), o.value("height", 0.0f)};
                obj.properties = readProperties(o);
                objects.push_back(std::move(obj));
            }
        }
    }

    map.buildCollision();
    return true;
}
