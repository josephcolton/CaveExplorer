#include "core/Assets.h"

#include <SDL3_image/SDL_image.h>

#include <filesystem>

std::string assetPath(const std::string& relative) {
    const char* base = SDL_GetBasePath();
    return std::string(base ? base : "") + "assets/" + relative;
}

std::string dataPath(const std::string& relative) {
    const char* base = SDL_GetBasePath();
    return std::string(base ? base : "") + "data/" + relative;
}

std::string savePath(const std::string& file) {
    char* pref = SDL_GetPrefPath("CaveExplorer", "CaveExplorer");
    std::string dir = pref ? pref : "";
    SDL_free(pref);
    return dir + file;
}

bool writeTextFile(const std::string& path, const std::string& text) {
    // Write to a temp file first so a crash mid-write can't corrupt the save.
    const std::string tmp = path + ".tmp";
    if (!SDL_SaveFile(tmp.c_str(), text.data(), text.size())) {
        SDL_Log("Could not write %s: %s", tmp.c_str(), SDL_GetError());
        return false;
    }
    if (!SDL_RenamePath(tmp.c_str(), path.c_str())) {
        SDL_Log("Could not replace %s: %s", path.c_str(), SDL_GetError());
        return false;
    }
    return true;
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

    SDL_Surface* surface = IMG_Load(key.c_str());
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
