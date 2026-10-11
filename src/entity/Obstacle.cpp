#include "entity/Obstacle.h"

#include "core/Sprites.h"
#include "world/Tilemap.h"

using nlohmann::json;

namespace {
constexpr float T = static_cast<float>(Tilemap::kTileSize);
}

ObstacleDef ObstacleDef::fromJson(const std::string& id, const json& j) {
    ObstacleDef d;
    d.id = id;
    d.name = j.value("name", id);
    d.requiredTag = j.value("requires", "");
    d.hits = std::max(1, j.value("hits", 1));
    d.icon = j.value("icon", "");
    d.sprite = j.value("sprite", "");
    d.hint = j.value("hint", "Something blocks the way.");
    d.verb = j.value("verb", "Clear");
    d.cleared = j.value("cleared", "The way is clear.");
    const auto c = j.value("debris", std::vector<int>{140, 140, 140});
    if (c.size() == 3) d.debris = {static_cast<Uint8>(c[0]), static_cast<Uint8>(c[1]), static_cast<Uint8>(c[2]), 255};
    return d;
}

Rect Obstacle::rect() const {
    return {static_cast<float>(tx) * T, static_cast<float>(ty) * T, static_cast<float>(tw) * T,
            static_cast<float>(th) * T};
}

void Obstacle::block(Tilemap& map, bool blocked) const {
    for (int y = ty; y < ty + th; ++y) {
        for (int x = tx; x < tx + tw; ++x) map.setBlocked(x, y, blocked);
    }
}

void Obstacle::render(SDL_Renderer* r, const SpriteDb& sprites, Vec2 camera) const {
    if (cleared) return;
    // A struck obstacle jitters briefly; damage shows as it darkens.
    const float jitter = shake > 0.0f ? std::sin(shake * 90.0f) * 2.0f : 0.0f;
    const auto shade = static_cast<Uint8>(255 - 50 * (def->hits - hitsLeft) / std::max(1, def->hits));
    for (int y = ty; y < ty + th; ++y) {
        for (int x = tx; x < tx + tw; ++x) {
            const float px = static_cast<float>(x) * T - camera.x + jitter;
            const float py = static_cast<float>(y) * T - camera.y;
            // Mirror alternate tiles so a column of the same art looks less repetitive.
            const bool flip = ((x + y) & 1) != 0;
            if (!def->icon.empty()) sprites.drawIcon(r, def->icon, px, py, 32.0f, flip, {shade, shade, shade, 255});
            else sprites.draw(r, def->sprite, px, py, flip);
        }
    }
}
