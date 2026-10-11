#include "core/Sprites.h"

#include "core/Assets.h"

#include <nlohmann/json.hpp>

using nlohmann::json;

bool SpriteDb::load(Assets& assets) {
    bool ok = true;

    if (auto text = readTextFile(assetPath("sprites/icons.json"))) {
        json doc = json::parse(*text, nullptr, false);
        iconTexture_ = assets.texture(assetPath("sprites/icons.png"));
        const int size = doc.value("tile_size", 32);
        const int columns = doc.value("columns", 16);
        const json icons = doc.value("icons", json::object());
        for (auto& [name, index] : icons.items()) {
            const int i = index.get<int>();
            icons_[name] = SDL_FRect{static_cast<float>((i % columns) * size), static_cast<float>((i / columns) * size),
                                     static_cast<float>(size), static_cast<float>(size)};
        }
    } else {
        ok = false;
    }

    if (auto text = readTextFile(dataPath("sprites.json"))) {
        json doc = json::parse(*text, nullptr, false);
        const json sprites = doc.value("sprites", json::object());
        for (auto& [name, def] : sprites.items()) {
            const auto rect = def.value("rect", std::vector<float>{0, 0, 32, 32});
            if (rect.size() != 4) continue;
            Sprite s;
            s.texture = assets.texture(assetPath(def.value("image", "")));
            s.src = {rect[0], rect[1], rect[2], rect[3]};
            sprites_[name] = s;
        }
    } else {
        ok = false;
    }
    return ok;
}

void SpriteDb::draw(SDL_Renderer* r, const std::string& name, float x, float y, bool flipX) const {
    auto it = sprites_.find(name);
    if (it == sprites_.end() || !it->second.texture) return;
    const Sprite& s = it->second;
    const SDL_FRect dst{std::round(x), std::round(y), s.src.w, s.src.h};
    SDL_RenderTextureRotated(r, s.texture, &s.src, &dst, 0.0, nullptr, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

SDL_FPoint SpriteDb::size(const std::string& name) const {
    auto it = sprites_.find(name);
    return it == sprites_.end() ? SDL_FPoint{0, 0} : SDL_FPoint{it->second.src.w, it->second.src.h};
}

void SpriteDb::drawIcon(SDL_Renderer* r, const std::string& icon, float x, float y, float size, bool flipX,
                        SDL_Color tint) const {
    const SDL_FRect dst{std::round(x), std::round(y), size, size};
    auto it = icons_.find(icon);
    if (it == icons_.end() || !iconTexture_) {
        SDL_SetRenderDrawColor(r, 200, 60, 200, 255);
        SDL_RenderRect(r, &dst);
        SDL_RenderDebugText(r, dst.x + size / 2 - 4, dst.y + size / 2 - 4, "?");
        return;
    }
    SDL_SetTextureColorMod(iconTexture_, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(iconTexture_, tint.a);
    SDL_RenderTextureRotated(r, iconTexture_, &it->second, &dst, 0.0, nullptr, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(iconTexture_, 255, 255, 255);
    SDL_SetTextureAlphaMod(iconTexture_, 255);
}
