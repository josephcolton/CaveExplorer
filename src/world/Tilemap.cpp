#include "world/Tilemap.h"

#include "core/App.h"

namespace {
// Tiled stores flip/rotation flags in the top bits of each gid.
constexpr std::uint32_t kGidMask = 0x0FFFFFFFu;
}

const Tileset* Tilemap::tilesetFor(std::uint32_t gid) const {
    const Tileset* found = nullptr;
    for (const Tileset& ts : tilesets) {
        if (static_cast<int>(gid) >= ts.firstGid) found = &ts;
    }
    return found;
}

void Tilemap::buildCollision() {
    collision_.assign(static_cast<std::size_t>(width * height), TileCollision::None);
    blocked_.assign(static_cast<std::size_t>(width * height), false);
    slippery_.assign(static_cast<std::size_t>(width * height), false);
    for (const TileLayer& layer : layers) {
        if (!layer.collides) continue;
        for (std::size_t i = 0; i < layer.gids.size() && i < collision_.size(); ++i) {
            const std::uint32_t gid = layer.gids[i] & kGidMask;
            if (gid == 0) continue;
            const Tileset* ts = tilesetFor(gid);
            if (!ts) continue;
            const int local = static_cast<int>(gid) - ts->firstGid;
            if (local < 0 || local >= static_cast<int>(ts->collision.size())) continue;
            const TileCollision c = ts->collision[static_cast<std::size_t>(local)];
            // Solid wins over one-way if layers overlap.
            if (c == TileCollision::Solid || collision_[i] == TileCollision::None) collision_[i] = c;
            if (static_cast<std::size_t>(local) < ts->slippery.size() && ts->slippery[static_cast<std::size_t>(local)]) {
                slippery_[i] = true;
            }
        }
    }
}

TileCollision Tilemap::collisionAt(int tx, int ty) const {
    if (tx < 0 || tx >= width || ty >= height) return TileCollision::Solid;
    if (ty < 0) return TileCollision::None;
    const auto i = static_cast<std::size_t>(ty * width + tx);
    return blocked_[i] ? TileCollision::Solid : collision_[i];
}

bool Tilemap::isSlippery(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width || ty >= height) return false;
    return slippery_[static_cast<std::size_t>(ty * width + tx)];
}

void Tilemap::setBlocked(int tx, int ty, bool blocked) {
    if (tx < 0 || ty < 0 || tx >= width || ty >= height) return;
    blocked_[static_cast<std::size_t>(ty * width + tx)] = blocked;
}

void Tilemap::render(SDL_Renderer* renderer, Vec2 camera, bool foreground) const {
    const int x0 = std::max(0, static_cast<int>(std::floor(camera.x / kTileSize)));
    const int y0 = std::max(0, static_cast<int>(std::floor(camera.y / kTileSize)));
    const int x1 = std::min(width - 1, static_cast<int>((camera.x + App::kViewWidth) / kTileSize));
    const int y1 = std::min(height - 1, static_cast<int>((camera.y + App::kViewHeight) / kTileSize));

    for (const TileLayer& layer : layers) {
        if (!layer.visible || layer.foreground != foreground) continue;
        for (int ty = y0; ty <= y1; ++ty) {
            for (int tx = x0; tx <= x1; ++tx) {
                const std::uint32_t gid = layer.gids[static_cast<std::size_t>(ty * width + tx)] & kGidMask;
                if (gid == 0) continue;
                const Tileset* ts = tilesetFor(gid);
                if (!ts || !ts->texture) continue;
                const int local = static_cast<int>(gid) - ts->firstGid;
                const SDL_FRect src{static_cast<float>((local % ts->columns) * ts->tileWidth),
                                    static_cast<float>((local / ts->columns) * ts->tileHeight),
                                    static_cast<float>(ts->tileWidth), static_cast<float>(ts->tileHeight)};
                // Tiles taller than the grid (e.g. 32x64 decorations) are anchored at the bottom, like Tiled.
                const SDL_FRect dst{static_cast<float>(tx * kTileSize) - camera.x,
                                    static_cast<float>((ty + 1) * kTileSize - ts->tileHeight) - camera.y,
                                    static_cast<float>(ts->tileWidth), static_cast<float>(ts->tileHeight)};
                SDL_RenderTexture(renderer, ts->texture, &src, &dst);
            }
        }
    }
}
