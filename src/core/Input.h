#pragma once

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

// Logical game actions. Gameplay code only ever asks about these, never raw keys,
// so rebinding (keyboard, mouse and gamepad) is a matter of editing the binding tables.
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
    Drop,
    QuickMove,  // inventory: send a stack straight to the other side
    Pause,
    Confirm,
    Cancel,
    Count
};

constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

const char* actionLabel(Action a);  // "Use right hand"
const char* actionId(Action a);     // "UseRight" (for settings files and scripts)

// A keyboard key or mouse button.
struct KeyBinding {
    enum class Type : Uint8 { None, Key, Mouse };
    Type type = Type::None;
    int code = 0;  // SDL_Scancode, or SDL mouse button (1 = left, 2 = middle, 3 = right)

    static KeyBinding key(SDL_Scancode s) { return {Type::Key, static_cast<int>(s)}; }
    static KeyBinding mouse(int button) { return {Type::Mouse, button}; }
    bool empty() const { return type == Type::None; }
    std::string name() const;
    bool operator==(const KeyBinding&) const = default;
};

class Input {
public:
    static constexpr std::size_t kKeySlots = 2;  // keyboard/mouse bindings per action
    static constexpr std::size_t kPadSlots = 2;  // gamepad buttons per action

    void init();
    void shutdown();

    void handleEvent(const SDL_Event& e);

    bool down(Action a) const { return down_[idx(a)]; }
    bool pressed(Action a) const { return pressed_[idx(a)]; }
    bool released(Action a) const { return released_[idx(a)]; }

    // Debug/automation: holds an action as if a key were down.
    void setScripted(Action a, bool held) { scriptHeld_[idx(a)] = held; refresh(); }

    // Called after each fixed update tick so edge events are seen exactly once.
    void consumeEdges();

    // --- Bindings
    std::array<std::array<KeyBinding, kKeySlots>, kActionCount> keyBindings{};
    std::array<std::array<SDL_GamepadButton, kPadSlots>, kActionCount> padBindings{};

    void resetBindings();
    nlohmann::json bindingsToJson() const;
    void bindingsFromJson(const nlohmann::json& j);
    // Keys bound to an action, for on-screen prompts (e.g. "J/X").
    std::string keyName(Action a) const;

    // --- Rebinding: the next key/mouse/gamepad press is captured instead of acted on.
    struct Capture {
        enum class Kind { Waiting, Key, Pad, Cancelled } kind = Kind::Waiting;
        KeyBinding key;
        SDL_GamepadButton pad = SDL_GAMEPAD_BUTTON_INVALID;
    };
    void startCapture();
    bool capturing() const { return capturing_; }
    // The result once something was pressed (capturing stops), otherwise Kind::Waiting.
    Capture takeCapture();

private:
    static std::size_t idx(Action a) { return static_cast<std::size_t>(a); }
    void refresh();

    std::array<bool, SDL_SCANCODE_COUNT> keysHeld_{};
    std::array<bool, 8> mouseHeld_{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> buttonsHeld_{};
    std::array<bool, kActionCount> stickHeld_{};
    std::array<bool, kActionCount> scriptHeld_{};

    std::array<bool, kActionCount> down_{};
    std::array<bool, kActionCount> pressed_{};
    std::array<bool, kActionCount> released_{};

    bool capturing_ = false;
    Capture capture_;

    std::vector<SDL_Gamepad*> gamepads_;
};
