#include "core/Input.h"

#include <algorithm>

namespace {
constexpr Sint16 kStickDeadzone = 12000;
}

void Input::init() {
    setDefaultBindings();
}

void Input::shutdown() {
    for (SDL_Gamepad* pad : gamepads_) SDL_CloseGamepad(pad);
    gamepads_.clear();
}

void Input::setDefaultBindings() {
    auto bind = [this](Action a, std::vector<SDL_Scancode> keys, std::vector<SDL_GamepadButton> buttons) {
        keyBindings[idx(a)] = std::move(keys);
        padBindings[idx(a)] = std::move(buttons);
    };
    bind(Action::Left, {SDL_SCANCODE_A, SDL_SCANCODE_LEFT}, {SDL_GAMEPAD_BUTTON_DPAD_LEFT});
    bind(Action::Right, {SDL_SCANCODE_D, SDL_SCANCODE_RIGHT}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT});
    bind(Action::Up, {SDL_SCANCODE_W, SDL_SCANCODE_UP}, {SDL_GAMEPAD_BUTTON_DPAD_UP});
    bind(Action::Down, {SDL_SCANCODE_S, SDL_SCANCODE_DOWN}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN});
    bind(Action::Jump, {SDL_SCANCODE_SPACE}, {SDL_GAMEPAD_BUTTON_SOUTH});
    bind(Action::UseRight, {SDL_SCANCODE_J}, {SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER});
    bind(Action::UseLeft, {SDL_SCANCODE_K}, {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER});
    bind(Action::Interact, {SDL_SCANCODE_E}, {SDL_GAMEPAD_BUTTON_NORTH});
    bind(Action::Inventory, {SDL_SCANCODE_I, SDL_SCANCODE_TAB}, {SDL_GAMEPAD_BUTTON_BACK});
    bind(Action::Pause, {SDL_SCANCODE_ESCAPE}, {SDL_GAMEPAD_BUTTON_START});
    bind(Action::Confirm, {SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE}, {SDL_GAMEPAD_BUTTON_SOUTH});
    bind(Action::Cancel, {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_BACKSPACE}, {SDL_GAMEPAD_BUTTON_EAST});
}

void Input::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (!e.key.repeat && e.key.scancode < SDL_SCANCODE_COUNT) {
            keysHeld_[e.key.scancode] = (e.type == SDL_EVENT_KEY_DOWN);
            refresh();
        }
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        if (SDL_Gamepad* pad = SDL_OpenGamepad(e.gdevice.which)) gamepads_.push_back(pad);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED: {
        SDL_Gamepad* pad = SDL_GetGamepadFromID(e.gdevice.which);
        if (pad) {
            gamepads_.erase(std::remove(gamepads_.begin(), gamepads_.end(), pad), gamepads_.end());
            SDL_CloseGamepad(pad);
        }
        buttonsHeld_.fill(false);
        stickHeld_.fill(false);
        refresh();
        break;
    }
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        if (e.gbutton.button < SDL_GAMEPAD_BUTTON_COUNT) {
            buttonsHeld_[e.gbutton.button] = (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            refresh();
        }
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
            stickHeld_[idx(Action::Left)] = e.gaxis.value < -kStickDeadzone;
            stickHeld_[idx(Action::Right)] = e.gaxis.value > kStickDeadzone;
            refresh();
        } else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
            stickHeld_[idx(Action::Up)] = e.gaxis.value < -kStickDeadzone;
            stickHeld_[idx(Action::Down)] = e.gaxis.value > kStickDeadzone;
            refresh();
        }
        break;
    default:
        break;
    }
}

void Input::refresh() {
    for (std::size_t a = 0; a < kActionCount; ++a) {
        bool held = stickHeld_[a];
        for (SDL_Scancode sc : keyBindings[a]) held = held || keysHeld_[sc];
        for (SDL_GamepadButton b : padBindings[a]) held = held || buttonsHeld_[b];

        if (held && !down_[a]) pressed_[a] = true;
        if (!held && down_[a]) released_[a] = true;
        down_[a] = held;
    }
}

void Input::consumeEdges() {
    pressed_.fill(false);
    released_.fill(false);
}
