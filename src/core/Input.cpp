#include "core/Input.h"

#include <algorithm>

using nlohmann::json;

namespace {
constexpr Sint16 kStickDeadzone = 12000;

struct ActionInfo {
    const char* id;
    const char* label;
};
constexpr ActionInfo kActions[kActionCount] = {
    {"Left", "Move left"},           {"Right", "Move right"},       {"Up", "Up / enter door"},
    {"Down", "Down"},                {"Jump", "Jump"},              {"UseRight", "Use right hand"},
    {"UseLeft", "Use left hand"},    {"Interact", "Interact"},      {"Inventory", "Inventory"},
    {"Drop", "Drop item"},           {"QuickMove", "Quick move item"}, {"Pause", "Pause menu"},       {"Confirm", "Menu: select"},
    {"Cancel", "Menu: back"},
};
}  // namespace

const char* actionLabel(Action a) { return kActions[static_cast<std::size_t>(a)].label; }
const char* actionId(Action a) { return kActions[static_cast<std::size_t>(a)].id; }

std::string KeyBinding::name() const {
    switch (type) {
    case Type::Key: {
        const char* n = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
        return (n && *n) ? n : "?";
    }
    case Type::Mouse:
        return code == SDL_BUTTON_LEFT ? "Mouse Left"
             : code == SDL_BUTTON_RIGHT ? "Mouse Right"
             : code == SDL_BUTTON_MIDDLE ? "Mouse Middle"
                                         : "Mouse " + std::to_string(code);
    default:
        return "-";
    }
}

void Input::init() { resetBindings(); }

void Input::shutdown() {
    for (SDL_Gamepad* pad : gamepads_) SDL_CloseGamepad(pad);
    gamepads_.clear();
}

void Input::resetBindings() {
    using K = KeyBinding;
    auto bind = [this](Action a, std::array<KeyBinding, kKeySlots> keys, std::array<SDL_GamepadButton, kPadSlots> pads) {
        keyBindings[idx(a)] = keys;
        padBindings[idx(a)] = pads;
    };
    constexpr SDL_GamepadButton none = SDL_GAMEPAD_BUTTON_INVALID;
    bind(Action::Left, {K::key(SDL_SCANCODE_A), K::key(SDL_SCANCODE_LEFT)}, {SDL_GAMEPAD_BUTTON_DPAD_LEFT, none});
    bind(Action::Right, {K::key(SDL_SCANCODE_D), K::key(SDL_SCANCODE_RIGHT)}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, none});
    bind(Action::Up, {K::key(SDL_SCANCODE_W), K::key(SDL_SCANCODE_UP)}, {SDL_GAMEPAD_BUTTON_DPAD_UP, none});
    bind(Action::Down, {K::key(SDL_SCANCODE_S), K::key(SDL_SCANCODE_DOWN)}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN, none});
    bind(Action::Jump, {K::key(SDL_SCANCODE_SPACE), K::key(SDL_SCANCODE_Z)}, {SDL_GAMEPAD_BUTTON_SOUTH, none});
    // Hands: J/K for WASD players, X/C for arrow-key players; mouse buttons work too.
    bind(Action::UseRight, {K::key(SDL_SCANCODE_J), K::key(SDL_SCANCODE_X)},
         {SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER});
    bind(Action::UseLeft, {K::key(SDL_SCANCODE_K), K::key(SDL_SCANCODE_C)}, {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, none});
    bind(Action::Interact, {K::key(SDL_SCANCODE_E), {}}, {SDL_GAMEPAD_BUTTON_NORTH, none});
    bind(Action::Inventory, {K::key(SDL_SCANCODE_TAB), K::key(SDL_SCANCODE_I)}, {SDL_GAMEPAD_BUTTON_BACK, none});
    bind(Action::Drop, {K::key(SDL_SCANCODE_Q), {}}, {SDL_GAMEPAD_BUTTON_RIGHT_STICK, none});
    bind(Action::QuickMove, {K::key(SDL_SCANCODE_LSHIFT), K::key(SDL_SCANCODE_RSHIFT)}, {SDL_GAMEPAD_BUTTON_WEST, none});
    bind(Action::Pause, {K::key(SDL_SCANCODE_ESCAPE), {}}, {SDL_GAMEPAD_BUTTON_START, none});
    bind(Action::Confirm, {K::key(SDL_SCANCODE_RETURN), K::key(SDL_SCANCODE_SPACE)}, {SDL_GAMEPAD_BUTTON_SOUTH, none});
    bind(Action::Cancel, {K::key(SDL_SCANCODE_ESCAPE), K::key(SDL_SCANCODE_BACKSPACE)}, {SDL_GAMEPAD_BUTTON_EAST, none});
    // Mouse clicks also swing: left click = right hand, right click = left hand.
    // (Kept outside the two rebindable slots so players don't lose them by accident.)
}

json Input::bindingsToJson() const {
    json j = json::object();
    for (std::size_t a = 0; a < kActionCount; ++a) {
        json keys = json::array();
        for (const KeyBinding& k : keyBindings[a]) {
            keys.push_back(k.type == KeyBinding::Type::Key     ? json{{"key", k.code}}
                           : k.type == KeyBinding::Type::Mouse ? json{{"mouse", k.code}}
                                                               : json(nullptr));
        }
        json pads = json::array();
        for (SDL_GamepadButton b : padBindings[a]) pads.push_back(static_cast<int>(b));
        j[kActions[a].id] = {{"keys", keys}, {"pad", pads}};
    }
    return j;
}

void Input::bindingsFromJson(const json& j) {
    if (!j.is_object()) return;
    for (std::size_t a = 0; a < kActionCount; ++a) {
        auto it = j.find(kActions[a].id);
        if (it == j.end() || !it->is_object()) continue;
        const json keys = it->value("keys", json::array());
        for (std::size_t s = 0; s < kKeySlots; ++s) {
            KeyBinding k;
            if (s < keys.size() && keys[s].is_object()) {
                if (keys[s].contains("key")) k = KeyBinding::key(static_cast<SDL_Scancode>(keys[s]["key"].get<int>()));
                else if (keys[s].contains("mouse")) k = KeyBinding::mouse(keys[s]["mouse"].get<int>());
            }
            if (k.type == KeyBinding::Type::Key && (k.code <= 0 || k.code >= SDL_SCANCODE_COUNT)) k = {};
            keyBindings[a][s] = k;
        }
        const json pads = it->value("pad", json::array());
        for (std::size_t s = 0; s < kPadSlots; ++s) {
            const int b = s < pads.size() && pads[s].is_number_integer() ? pads[s].get<int>() : -1;
            padBindings[a][s] = (b >= 0 && b < SDL_GAMEPAD_BUTTON_COUNT) ? static_cast<SDL_GamepadButton>(b)
                                                                         : SDL_GAMEPAD_BUTTON_INVALID;
        }
    }
    refresh();
}

std::string Input::keyName(Action a) const {
    std::string out;
    std::string first;
    for (const KeyBinding& k : keyBindings[idx(a)]) {
        if (k.empty()) continue;
        std::string n = k.name();
        // "Left Shift" / "Right Shift" -> "Shift" (shown once) to keep prompts short.
        for (const char* mod : {"Shift", "Ctrl", "Alt"}) {
            if (n == std::string("Left ") + mod || n == std::string("Right ") + mod) n = mod;
        }
        if (n == first) continue;
        if (first.empty()) first = n;
        out += (out.empty() ? "" : "/") + n;
    }
    return out.empty() ? "?" : out;
}

void Input::startCapture() {
    capturing_ = true;
    capture_ = {};
}

Input::Capture Input::takeCapture() {
    if (capture_.kind == Capture::Kind::Waiting) return capture_;
    Capture c = capture_;
    capture_ = {};
    capturing_ = false;
    return c;
}

void Input::handleEvent(const SDL_Event& e) {
    // While rebinding, the first press is captured and doesn't trigger any action.
    if (capturing_ && capture_.kind == Capture::Kind::Waiting) {
        if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
                capture_.kind = Capture::Kind::Cancelled;
            } else {
                capture_.kind = Capture::Kind::Key;
                capture_.key = KeyBinding::key(e.key.scancode);
            }
            return;
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            capture_.kind = Capture::Kind::Key;
            capture_.key = KeyBinding::mouse(e.button.button);
            return;
        }
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            capture_.kind = e.gbutton.button == SDL_GAMEPAD_BUTTON_START ? Capture::Kind::Cancelled : Capture::Kind::Pad;
            capture_.pad = static_cast<SDL_GamepadButton>(e.gbutton.button);
            return;
        }
    }

    switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        if (!e.key.repeat && e.key.scancode < SDL_SCANCODE_COUNT) {
            keysHeld_[e.key.scancode] = (e.type == SDL_EVENT_KEY_DOWN);
            refresh();
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button < mouseHeld_.size()) {
            mouseHeld_[e.button.button] = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
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
        bool held = stickHeld_[a] || scriptHeld_[a];
        for (const KeyBinding& k : keyBindings[a]) {
            if (k.type == KeyBinding::Type::Key) held = held || keysHeld_[static_cast<std::size_t>(k.code)];
            else if (k.type == KeyBinding::Type::Mouse && k.code >= 0 && k.code < 8) held = held || mouseHeld_[static_cast<std::size_t>(k.code)];
        }
        for (SDL_GamepadButton b : padBindings[a]) {
            if (b != SDL_GAMEPAD_BUTTON_INVALID) held = held || buttonsHeld_[static_cast<std::size_t>(b)];
        }
        // Mouse clicks always swing the hands (left click = right hand, right click = left hand).
        if (a == idx(Action::UseRight)) held = held || mouseHeld_[SDL_BUTTON_LEFT];
        if (a == idx(Action::UseLeft)) held = held || mouseHeld_[SDL_BUTTON_RIGHT];

        if (held && !down_[a]) pressed_[a] = true;
        if (!held && down_[a]) released_[a] = true;
        down_[a] = held;
    }
}

void Input::consumeEdges() {
    pressed_.fill(false);
    released_.fill(false);
}
