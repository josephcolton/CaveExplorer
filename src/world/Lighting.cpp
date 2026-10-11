#include "world/Lighting.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kGlowSize = 128;
}

bool Lighting::init(SDL_Renderer* renderer, int width, int height) {
    width_ = width;
    height_ = height;
    lightmap_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!lightmap_) {
        SDL_Log("Lighting disabled: %s", SDL_GetError());
        return false;
    }
    // Multiply: the screen keeps (light colour x world colour).
    SDL_SetTextureBlendMode(lightmap_, SDL_BLENDMODE_MOD);
    SDL_SetTextureScaleMode(lightmap_, SDL_SCALEMODE_NEAREST);

    // A soft radial glow: bright centre fading smoothly to nothing at the edge.
    SDL_Surface* s = SDL_CreateSurface(kGlowSize, kGlowSize, SDL_PIXELFORMAT_RGBA8888);
    if (!s) return false;
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    auto* px = static_cast<Uint32*>(s->pixels);
    const float c = (kGlowSize - 1) * 0.5f;
    for (int y = 0; y < kGlowSize; ++y) {
        for (int x = 0; x < kGlowSize; ++x) {
            const float d = std::sqrt((x - c) * (x - c) + (y - c) * (y - c)) / c;
            const float f = d >= 1.0f ? 0.0f : (1.0f - d * d) * (1.0f - d * d);
            const auto v = static_cast<Uint8>(255.0f * f);
            px[y * (s->pitch / 4) + x] = SDL_MapRGBA(fmt, nullptr, v, v, v, 255);
        }
    }
    glow_ = SDL_CreateTextureFromSurface(renderer, s);
    SDL_DestroySurface(s);
    if (!glow_) return false;
    SDL_SetTextureBlendMode(glow_, SDL_BLENDMODE_ADD);  // lights add up where they overlap
    SDL_SetTextureScaleMode(glow_, SDL_SCALEMODE_LINEAR);
    return true;
}

void Lighting::shutdown() {
    if (lightmap_) SDL_DestroyTexture(lightmap_);
    if (glow_) SDL_DestroyTexture(glow_);
    lightmap_ = glow_ = nullptr;
}

void Lighting::begin(SDL_Renderer* r, SDL_Color ambient) {
    if (!lightmap_) return;
    previousTarget_ = SDL_GetRenderTarget(r);
    SDL_SetRenderTarget(r, lightmap_);
    SDL_SetRenderDrawColor(r, ambient.r, ambient.g, ambient.b, 255);
    SDL_RenderClear(r);
}

void Lighting::addLight(SDL_Renderer* r, Vec2 p, float radius, SDL_Color color, float intensity) {
    if (!lightmap_ || radius <= 0.0f) return;
    // Skip lights that can't touch the screen.
    if (p.x + radius < 0 || p.y + radius < 0 || p.x - radius > width_ || p.y - radius > height_) return;
    const float k = std::clamp(intensity, 0.0f, 1.0f);
    SDL_SetTextureColorMod(glow_, static_cast<Uint8>(color.r * k), static_cast<Uint8>(color.g * k),
                           static_cast<Uint8>(color.b * k));
    const SDL_FRect dst{p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f};
    SDL_RenderTexture(r, glow_, nullptr, &dst);
}

void Lighting::end(SDL_Renderer* r) {
    if (!lightmap_) return;
    SDL_SetRenderTarget(r, previousTarget_);
    const SDL_FRect all{0, 0, static_cast<float>(width_), static_cast<float>(height_)};
    SDL_RenderTexture(r, lightmap_, nullptr, &all);
}
