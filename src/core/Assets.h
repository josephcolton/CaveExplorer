#pragma once

#include <SDL3/SDL.h>

#include <optional>
#include <string>
#include <unordered_map>

// Absolute path to a file under the game's assets/ directory (next to the executable).
std::string assetPath(const std::string& relative);

// Reads a whole file as text. Returns nullopt (and logs) on failure.
std::optional<std::string> readTextFile(const std::string& path);

// Owns and caches textures by (normalized) path.
class Assets {
public:
    void init(SDL_Renderer* renderer) { renderer_ = renderer; }
    void clear();

    // Loads a .bmp texture. Pixels matching colorKey (if given) become transparent.
    SDL_Texture* texture(const std::string& path, std::optional<SDL_Color> colorKey = std::nullopt);

private:
    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::string, SDL_Texture*> textures_;
};
