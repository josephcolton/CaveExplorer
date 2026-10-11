#pragma once

#include "core/Math.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

enum class TileCollision : std::uint8_t {
    None,
    Solid,
    OneWay,  // can be jumped through from below and dropped through with Down+Jump
};

struct Tileset {
    int firstGid = 1;
    int tileCount = 0;
    int columns = 1;
    int tileWidth = 32;
    int tileHeight = 32;
    SDL_Texture* texture = nullptr;
    std::vector<TileCollision> collision;  // indexed by local tile id
    std::vector<bool> slippery;            // surface = slippery (mossy/wet/icy)
};

struct TileLayer {
    std::string name;
    std::vector<std::uint32_t> gids;  // width*height, 0 = empty
    bool collides = false;             // tiles in this layer are tested for collision
    bool foreground = false;           // drawn in front of entities
    bool visible = true;
};

class Tilemap {
public:
    static constexpr int kTileSize = 32;

    int width = 0;   // in tiles
    int height = 0;  // in tiles
    std::vector<Tileset> tilesets;
    std::vector<TileLayer> layers;

    float pixelWidth() const { return static_cast<float>(width * kTileSize); }
    float pixelHeight() const { return static_cast<float>(height * kTileSize); }

    // Collision at a tile coordinate. Left/right/bottom edges of the map are solid walls.
    TileCollision collisionAt(int tx, int ty) const;

    void render(SDL_Renderer* renderer, Vec2 camera, bool foreground) const;

    // Rebuild the merged collision grid after layers/tilesets change.
    void buildCollision();

    // Slick ground (moss, ice): little grip unless wearing non-slip boots.
    bool isSlippery(int tx, int ty) const;

    // Runtime blockers (obstacles such as bushes and boulders) make tiles solid until cleared.
    void setBlocked(int tx, int ty, bool blocked);
    // Runtime one-way tiles: the tops of ladders can be stood on.
    void setOneWay(int tx, int ty);

private:
    const Tileset* tilesetFor(std::uint32_t gid) const;
    std::vector<TileCollision> collision_;
    std::vector<bool> blocked_;
    std::vector<bool> slippery_;
    std::vector<bool> extraOneWay_;
};
