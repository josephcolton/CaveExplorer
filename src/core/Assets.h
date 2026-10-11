#pragma once

#include <SDL3/SDL.h>

#include <optional>
#include <string>
#include <unordered_map>

// Absolute path to a file under the game's assets/ directory (next to the executable).
std::string assetPath(const std::string& relative);

// Absolute path to a file under the game's data/ directory.
std::string dataPath(const std::string& relative);

// Path inside the per-user save directory (e.g. %APPDATA%/CaveExplorer/CaveExplorer/).
std::string savePath(const std::string& file);

// Writes text to a file. Returns false (and logs) on failure.
bool writeTextFile(const std::string& path, const std::string& text);

// Reads a whole file as text. Returns nullopt (and logs) on failure.
std::optional<std::string> readTextFile(const std::string& path);

// Owns and caches textures by (normalized) path.
class Assets {
public:
    void init(SDL_Renderer* renderer) { renderer_ = renderer; }
    void clear();

    // Loads a .png or .bmp texture. Pixels matching colorKey (if given) become transparent.
    SDL_Texture* texture(const std::string& path, std::optional<SDL_Color> colorKey = std::nullopt);

private:
    SDL_Renderer* renderer_ = nullptr;
    std::unordered_map<std::string, SDL_Texture*> textures_;
};
