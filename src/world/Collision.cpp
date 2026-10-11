#include "world/Collision.h"

#include "world/Tilemap.h"

namespace {
constexpr float kEps = 0.001f;
constexpr float T = static_cast<float>(Tilemap::kTileSize);

int tileOf(float v) { return static_cast<int>(std::floor(v / T)); }
}  // namespace

MoveResult moveAndCollide(const Tilemap& map, Rect& box, Vec2& velocity, float dt, bool dropThroughOneWay) {
    MoveResult result;

    // --- X axis: only solid tiles block sideways movement.
    const float dx = velocity.x * dt;
    box.x += dx;
    {
        const int rowTop = tileOf(box.top());
        const int rowBottom = tileOf(box.bottom() - kEps);
        if (dx > 0.0f) {
            const int col = tileOf(box.right() - kEps);
            for (int row = rowTop; row <= rowBottom; ++row) {
                if (map.collisionAt(col, row) == TileCollision::Solid) {
                    box.x = col * T - box.w;
                    velocity.x = 0.0f;
                    result.hitWallRight = true;
                    break;
                }
            }
        } else if (dx < 0.0f) {
            const int col = tileOf(box.left());
            for (int row = rowTop; row <= rowBottom; ++row) {
                if (map.collisionAt(col, row) == TileCollision::Solid) {
                    box.x = (col + 1) * T;
                    velocity.x = 0.0f;
                    result.hitWallLeft = true;
                    break;
                }
            }
        }
    }

    // --- Y axis
    const float oldBottom = box.bottom();
    const float dy = velocity.y * dt;
    box.y += dy;
    {
        const int colLeft = tileOf(box.left());
        const int colRight = tileOf(box.right() - kEps);
        if (dy > 0.0f) {
            const int row = tileOf(box.bottom() - kEps);
            const float rowTop = row * T;
            for (int col = colLeft; col <= colRight; ++col) {
                const TileCollision c = map.collisionAt(col, row);
                const bool landsOnOneWay =
                    c == TileCollision::OneWay && !dropThroughOneWay && oldBottom <= rowTop + kEps;
                if (c == TileCollision::Solid || landsOnOneWay) {
                    box.y = rowTop - box.h;
                    velocity.y = 0.0f;
                    result.onGround = true;
                    result.onOneWay = (c == TileCollision::OneWay);
                    // Keep scanning: if any tile under us is solid we are not "only" on a one-way.
                    for (int c2 = colLeft; c2 <= colRight; ++c2) {
                        if (map.collisionAt(c2, row) == TileCollision::Solid) result.onOneWay = false;
                    }
                    break;
                }
            }
        } else if (dy < 0.0f) {
            const int row = tileOf(box.top());
            for (int col = colLeft; col <= colRight; ++col) {
                if (map.collisionAt(col, row) == TileCollision::Solid) {
                    box.y = (row + 1) * T;
                    velocity.y = 0.0f;
                    result.hitCeiling = true;
                    break;
                }
            }
        }
    }

    return result;
}
