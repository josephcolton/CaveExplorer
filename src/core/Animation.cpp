#include "core/Animation.h"

#include "core/Assets.h"

#include <nlohmann/json.hpp>

#include <cmath>

using nlohmann::json;

bool AnimDb::load(Assets& assets) {
    auto text = readTextFile(dataPath("animations.json"));
    if (!text) return false;
    const json doc = json::parse(*text, nullptr, false);
    const json sets = doc.value("sets", json::object());
    for (auto& [name, j] : sets.items()) {
        AnimSet s;
        s.texture = assets.texture(assetPath(j.value("image", "")));
        const auto cell = j.value("cell", std::vector<int>{32, 32});
        const auto anchor = j.value("anchor", std::vector<float>{16, 32});
        if (cell.size() == 2) { s.cellW = cell[0]; s.cellH = cell[1]; }
        if (anchor.size() == 2) { s.anchorX = anchor[0]; s.anchorY = anchor[1]; }
        s.columns = std::max(1, j.value("columns", 1));
        s.facesLeft = j.value("faces", "right") == "left";
        const json clips = j.value("clips", json::object());
        for (auto& [clipName, c] : clips.items()) {
            AnimClip clip;
            clip.frames = c.value("frames", std::vector<int>{0});
            clip.fps = std::max(0.1f, c.value("fps", 8.0f));
            clip.loop = c.value("loop", true);
            s.clips[clipName] = std::move(clip);
        }
        sets_[name] = std::move(s);
    }
    return true;
}

const AnimSet* AnimDb::find(const std::string& name) const {
    auto it = sets_.find(name);
    return it == sets_.end() ? nullptr : &it->second;
}

void Animator::play(const std::string& clip, bool restart) {
    if (clip == clip_ && !restart) return;
    clip_ = clip;
    time_ = 0.0f;
}

bool Animator::finished() const {
    if (!set_) return true;
    auto it = set_->clips.find(clip_);
    return it == set_->clips.end() || (!it->second.loop && time_ >= it->second.duration());
}

void Animator::draw(SDL_Renderer* r, float x, float y, bool facingRight, SDL_Color tint) const {
    if (!set_ || !set_->texture) return;
    auto it = set_->clips.find(clip_);
    if (it == set_->clips.end() || it->second.frames.empty()) return;
    const AnimClip& c = it->second;

    auto index = static_cast<std::size_t>(time_ * c.fps);
    index = c.loop ? index % c.frames.size() : std::min(index, c.frames.size() - 1);
    const int frame = c.frames[index];

    const SDL_FRect src{static_cast<float>((frame % set_->columns) * set_->cellW),
                        static_cast<float>((frame / set_->columns) * set_->cellH), static_cast<float>(set_->cellW),
                        static_cast<float>(set_->cellH)};
    const bool flip = facingRight == set_->facesLeft;
    // When flipped, the anchor mirrors too.
    const float ax = flip ? static_cast<float>(set_->cellW) - set_->anchorX : set_->anchorX;
    const SDL_FRect dst{std::round(x - ax), std::round(y - set_->anchorY), src.w, src.h};
    SDL_SetTextureColorMod(set_->texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(set_->texture, tint.a);
    SDL_RenderTextureRotated(r, set_->texture, &src, &dst, 0.0, nullptr, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(set_->texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(set_->texture, 255);
}
