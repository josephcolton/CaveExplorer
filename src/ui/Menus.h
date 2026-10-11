#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

class App;
class Input;

// A vertical list of choices. update() returns the chosen index on Confirm,
// kCancelled on Cancel, or kNone.
class Menu {
public:
    static constexpr int kNone = -1;
    static constexpr int kCancelled = -2;

    void setItems(std::vector<std::string> items, int cursor = 0);
    int update(const Input& input);
    void render(SDL_Renderer* r, float centerX, float top) const;
    int cursor() const { return cursor_; }
    float lineHeight = 18.0f;

private:
    std::vector<std::string> items_;
    int cursor_ = 0;
};

// Settings: volumes, fullscreen, difficulty, and a read-only controls list.
// Changes apply immediately and are saved when the screen closes.
class SettingsScreen {
public:
    enum class Result { Stay, Close };
    void open(App& app);
    Result update();
    void render(SDL_Renderer* r) const;

private:
    void updateControls();
    void renderControls(SDL_Renderer* r) const;

    App* app_ = nullptr;
    int cursor_ = 0;
    bool showingControls_ = false;
    int controlRow_ = 0;
    int controlCol_ = 0;  // 0-1 keyboard/mouse slots, 2 gamepad
    std::string controlMessage_;
};

// A scrolling page of text (credits, about).
class TextScreen {
public:
    void open(std::string title, const std::string& text);
    bool update(const Input& input);  // false when closed
    void render(SDL_Renderer* r) const;

private:
    std::string title_;
    std::vector<std::string> lines_;
    int scroll_ = 0;
};

// The About page text.
std::string aboutText();

// Readable gamepad button name ("Pad A", "LB"...).
std::string padButtonName(SDL_GamepadButton b);
