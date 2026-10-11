#pragma once

#include "core/Math.h"

class Tilemap;

struct MoveResult {
    bool onGround = false;
    bool onOneWay = false;  // standing on a one-way platform (can drop through)
    bool hitCeiling = false;
    bool hitWallLeft = false;
    bool hitWallRight = false;
};

// Moves an axis-aligned box through the tilemap with sub-pixel precision.
// Resolves X then Y. Velocity components are zeroed on impact.
// Assumes per-step movement is less than one tile (true at 60 Hz for all our speeds).
MoveResult moveAndCollide(const Tilemap& map, Rect& box, Vec2& velocity, float dt, bool dropThroughOneWay);
