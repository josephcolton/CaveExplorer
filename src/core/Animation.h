#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

class Assets;

struct AnimClip {
    std::vector<int> frames;
    float fps = 8.0f;
    bool loop = true;
    float duration() const { return frames.empty() ? 0.0f : static_cast<float>(frames.size()) / fps; }
};

// One sprite sheet of fixed-size cells plus named clips (data/animations.json).
struct AnimSet {
    SDL_Texture* texture = nullptr;
    int cellW = 32;
    int cellH = 32;
    int columns = 1;
    float anchorX = 16.0f;  // point within a cell that sits on the entity's anchor
    float anchorY = 32.0f;
    bool facesLeft = false;
    std::unordered_map<std::string, AnimClip> clips;
};

class AnimDb {
public:
    bool load(Assets& assets);
    const AnimSet* find(const std::string& name) const;

private:
    std::unordered_map<std::string, AnimSet> sets_;
};

// Plays clips from one AnimSet.
class Animator {
public:
    void setSet(const AnimSet* set) { set_ = set; }
    // Switches clip (keeps time if it's already playing unless restart is true).
    void play(const std::string& clip, bool restart = false);
    void update(float dt) { time_ += dt * speed; }
    bool finished() const;
    const std::string& clip() const { return clip_; }

    // Draws with the set's anchor at (x, y) in screen pixels. facingRight flips as needed.
    void draw(SDL_Renderer* r, float x, float y, bool facingRight, SDL_Color tint = {255, 255, 255, 255}) const;

    float speed = 1.0f;

private:
    const AnimSet* set_ = nullptr;
    std::string clip_;
    float time_ = 0.0f;
};
