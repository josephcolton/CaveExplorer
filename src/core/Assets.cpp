#include "core/Assets.h"

#include <filesystem>

std::string assetPath(const std::string& relative) {
    const char* base = SDL_GetBasePath();
    return std::string(base ? base : "") + "assets/" + relative;
}

std::optional<std::string> readTextFile(const std::string& path) {
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (!data) {
        SDL_Log("Could not read %s: %s", path.c_str(), SDL_GetError());
        return std::nullopt;
    }
    std::string text(static_cast<const char*>(data), size);
    SDL_free(data);
    return text;
}

void Assets::clear() {
    for (auto& [path, tex] : textures_) SDL_DestroyTexture(tex);
    textures_.clear();
}

SDL_Texture* Assets::texture(const std::string& path, std::optional<SDL_Color> colorKey) {
    const std::string key = std::filesystem::path(path).lexically_normal().generic_string();
    if (auto it = textures_.find(key); it != textures_.end()) return it->second;

    SDL_Surface* surface = SDL_LoadBMP(key.c_str());
    if (!surface) {
        SDL_Log("Could not load image %s: %s", key.c_str(), SDL_GetError());
        return nullptr;
    }
    if (colorKey) {
        SDL_SetSurfaceColorKey(surface, true, SDL_MapSurfaceRGB(surface, colorKey->r, colorKey->g, colorKey->b));
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
    if (!tex) {
        SDL_Log("Could not create texture %s: %s", key.c_str(), SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    textures_[key] = tex;
    return tex;
}
