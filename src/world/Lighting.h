#pragma once

#include "core/Math.h"

#include <SDL3/SDL.h>

// Darkness and light. Each frame a "light map" is cleared to the area's ambient light,
// soft glows are added for every light source, and the result is multiplied over the
// world: unlit places go dark, lit places keep their colour (tinted by the light).
class Lighting {
public:
    bool init(SDL_Renderer* renderer, int width, int height);
    void shutdown();

    // ambient: how much of the world shows with no light at all (white = fully lit).
    void begin(SDL_Renderer* r, SDL_Color ambient);
    // A soft round light centred at a screen position. intensity scales its brightness.
    void addLight(SDL_Renderer* r, Vec2 screenPos, float radius, SDL_Color color, float intensity = 1.0f);
    // Multiplies the light map over what has been drawn so far.
    void end(SDL_Renderer* r);

private:
    SDL_Texture* lightmap_ = nullptr;
    SDL_Texture* glow_ = nullptr;
    SDL_Texture* previousTarget_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};
