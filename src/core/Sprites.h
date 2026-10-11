#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>

class Assets;

// Named sprites (data/sprites.json) and item icons (assets/sprites/icons.json).
class SpriteDb {
public:
    bool load(Assets& assets);

    // Draws a named sprite with its top-left at (x, y).
    void draw(SDL_Renderer* r, const std::string& name, float x, float y, bool flipX = false) const;
    // Size of a named sprite, or 0x0 if unknown.
    SDL_FPoint size(const std::string& name) const;

    // Draws a 32x32 item icon with its top-left at (x, y). Unknown icons draw a "?" box.
    void drawIcon(SDL_Renderer* r, const std::string& icon, float x, float y, float size = 32.0f) const;

private:
    struct Sprite {
        SDL_Texture* texture = nullptr;
        SDL_FRect src{};
    };
    std::unordered_map<std::string, Sprite> sprites_;
    std::unordered_map<std::string, SDL_FRect> icons_;
    SDL_Texture* iconTexture_ = nullptr;
};
