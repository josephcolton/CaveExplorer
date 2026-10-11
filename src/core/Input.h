#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <vector>

// Logical game actions. Gameplay code only ever asks about these, never raw keys,
// so rebinding (keyboard and gamepad) is a matter of editing the binding tables.
enum class Action {
    Left,
    Right,
    Up,
    Down,
    Jump,
    UseRight,  // item held in the right hand (usually melee)
    UseLeft,   // item held in the left hand (ranged / torch / shield)
    Interact,
    Inventory,
    Pause,
    Confirm,
    Cancel,
    Count
};

constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

class Input {
public:
    void init();
    void shutdown();

    void handleEvent(const SDL_Event& e);

    bool down(Action a) const { return down_[idx(a)]; }
    bool pressed(Action a) const { return pressed_[idx(a)]; }
    bool released(Action a) const { return released_[idx(a)]; }

    // Called after each fixed update tick so edge events are seen exactly once.
    void consumeEdges();

    std::array<std::vector<SDL_Scancode>, kActionCount> keyBindings;
    std::array<std::vector<SDL_GamepadButton>, kActionCount> padBindings;

private:
    static std::size_t idx(Action a) { return static_cast<std::size_t>(a); }
    void setDefaultBindings();
    void refresh();

    std::array<bool, SDL_SCANCODE_COUNT> keysHeld_{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttonsHeld_{};
    std::array<bool, kActionCount> stickHeld_{};

    std::array<bool, kActionCount> down_{};
    std::array<bool, kActionCount> pressed_{};
    std::array<bool, kActionCount> released_{};

    std::vector<SDL_Gamepad*> gamepads_;
};
