#pragma once

#include "ui/Menus.h"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

class Input;
class SpriteDb;

// A conversation box: portrait, name, what they say, and your possible replies.
class DialogueScreen {
public:
    void open(const SpriteDb& sprites, std::string name, std::string icon);
    void say(const std::string& text, std::vector<std::string> replies);
    // Returns the chosen reply index, Menu::kCancelled, or Menu::kNone.
    int update(const Input& input) { return replies_.update(input); }
    void render(SDL_Renderer* r) const;

private:
    const SpriteDb* sprites_ = nullptr;
    std::string name_;
    std::string icon_;
    std::vector<std::string> lines_;
    std::vector<std::string> replyText_;
    Menu replies_;
};
