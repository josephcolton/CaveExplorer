#include "ui/Menus.h"

#include "core/App.h"
#include "core/Input.h"
#include "ui/Ui.h"

#include <sstream>

// ---------------------------------------------------------------------------- Menu

void Menu::setItems(std::vector<std::string> items, int cursor) {
    items_ = std::move(items);
    cursor_ = std::clamp(cursor, 0, std::max(0, static_cast<int>(items_.size()) - 1));
}

int Menu::update(const Input& input) {
    const int n = static_cast<int>(items_.size());
    if (n == 0) return kNone;
    if (input.pressed(Action::Up)) { cursor_ = (cursor_ + n - 1) % n; ui::sound("menu_move"); }
    if (input.pressed(Action::Down)) { cursor_ = (cursor_ + 1) % n; ui::sound("menu_move"); }
    if (input.pressed(Action::Confirm)) { ui::sound("menu_select"); return cursor_; }
    if (input.pressed(Action::Cancel)) { ui::sound("menu_back"); return kCancelled; }
    return kNone;
}

void Menu::render(SDL_Renderer* r, float centerX, float top) const {
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const bool sel = static_cast<int>(i) == cursor_;
        const float y = top + static_cast<float>(i) * lineHeight;
        if (sel) {
            const float w = ui::textWidth(items_[i]) + 28.0f;
            const SDL_FRect bar{centerX - w * 0.5f, y - 4, w, 16};
            SDL_SetRenderDrawColor(r, 90, 70, 40, 200);
            SDL_RenderFillRect(r, &bar);
            ui::text(r, centerX - w * 0.5f + 4, y, ">", ui::kGold);
        }
        ui::textCentered(r, centerX, y, items_[i], sel ? ui::kGold : ui::kText);
    }
}

// ---------------------------------------------------------------------------- Settings

namespace {
enum Row { MusicRow, SoundRow, FullscreenRow, BrightnessRow, DifficultyRow, ControlsRow, BackRow, RowCount };
constexpr SDL_FRect kSettingsPanel{150, 50, 340, 262};
}  // namespace

void SettingsScreen::open(App& app) {
    app_ = &app;
    cursor_ = 0;
    showingControls_ = false;
}

SettingsScreen::Result SettingsScreen::update() {
    if (showingControls_) {
        updateControls();
        return Result::Stay;
    }
    const Input& in = app_->input();

    Settings& s = app_->settings();
    if (in.pressed(Action::Up)) { cursor_ = (cursor_ + RowCount - 1) % RowCount; ui::sound("menu_move"); }
    if (in.pressed(Action::Down)) { cursor_ = (cursor_ + 1) % RowCount; ui::sound("menu_move"); }
    const int delta = (in.pressed(Action::Right) ? 1 : 0) - (in.pressed(Action::Left) ? 1 : 0);
    const bool confirm = in.pressed(Action::Confirm);

    bool changed = false;
    switch (cursor_) {
    case MusicRow:
        if (delta) { s.musicVolume = std::clamp(s.musicVolume + delta, 0, 10); changed = true; }
        break;
    case SoundRow:
        if (delta) { s.soundVolume = std::clamp(s.soundVolume + delta, 0, 10); changed = true; }
        break;
    case FullscreenRow:
        if (delta || confirm) { s.fullscreen = !s.fullscreen; changed = true; }
        break;
    case BrightnessRow:
        if (delta || confirm) {
            s.brightness = (s.brightness + (delta ? delta : 1) + 3) % 3;
            changed = true;
        }
        break;
    case DifficultyRow:
        if (delta || confirm) {
            const int d = (static_cast<int>(s.difficulty) + (delta ? delta : 1) + 3) % 3;
            s.difficulty = static_cast<Difficulty>(d);
            changed = true;
        }
        break;
    case ControlsRow:
        if (confirm) {
            showingControls_ = true;
            controlRow_ = 0;
            controlCol_ = 0;
            controlMessage_.clear();
        }
        break;
    case BackRow:
        if (confirm) {
            s.save();
            return Result::Close;
        }
        break;
    }
    if (changed) {
        app_->applySettings();
        ui::sound("menu_move");  // also previews the new sound volume
    }
    if (in.pressed(Action::Cancel)) {
        s.save();
        return Result::Close;
    }
    return Result::Stay;
}

void SettingsScreen::render(SDL_Renderer* r) const {
    ui::dimScreen(r, 170);
    if (showingControls_) {
        renderControls(r);
        return;
    }

    const Settings& s = app_->settings();
    ui::panel(r, kSettingsPanel, "Settings");
    auto bar = [](int v) { return std::string(static_cast<std::size_t>(v), '#') + std::string(static_cast<std::size_t>(10 - v), '-'); };
    const std::string rows[RowCount][2] = {
        {"Music volume", "< " + bar(s.musicVolume) + " >"},
        {"Sound volume", "< " + bar(s.soundVolume) + " >"},
        {"Fullscreen", s.fullscreen ? "< On >" : "< Off >"},
        {"Cave brightness", s.brightness == 0 ? "< Dark >" : s.brightness == 1 ? "< Normal >" : "< Bright >"},
        {"Difficulty", std::string("< ") + difficultyName(s.difficulty) + " >"},
        {"Controls...", ""},
        {"Back", ""},
    };
    for (int i = 0; i < RowCount; ++i) {
        const float y = kSettingsPanel.y + 40 + static_cast<float>(i) * 26.0f;
        const SDL_Color c = i == cursor_ ? ui::kGold : ui::kText;
        if (i == cursor_) ui::text(r, kSettingsPanel.x + 12, y, ">", ui::kGold);
        ui::text(r, kSettingsPanel.x + 28, y, rows[i][0], c);
        ui::text(r, kSettingsPanel.x + 180, y, rows[i][1], c);
    }
    ui::text(r, kSettingsPanel.x + 12, kSettingsPanel.y + kSettingsPanel.h - 30,
             "Difficulty applies the next time an area loads.", ui::kDim);
    ui::text(r, kSettingsPanel.x + 12, kSettingsPanel.y + kSettingsPanel.h - 16, "Left/Right: change  Esc: back",
             ui::kDim);
}

// ---------------------------------------------------------------------------- Text page

void TextScreen::open(std::string title, const std::string& text) {
    title_ = std::move(title);
    lines_.clear();
    scroll_ = 0;
    std::istringstream in(text);
    for (std::string para; std::getline(in, para);) {
        if (para.empty()) {
            lines_.emplace_back();
            continue;
        }
        // Keep leading indentation on wrapped lines.
        const std::size_t indent = para.find_first_not_of(' ');
        const std::string pad(indent == std::string::npos ? 0 : indent, ' ');
        for (const std::string& l : ui::wrap(para, 64 - pad.size())) lines_.push_back(pad + l);
    }
}

bool TextScreen::update(const Input& input) {
    const int visible = 23;
    const int maxScroll = std::max(0, static_cast<int>(lines_.size()) - visible);
    if (input.pressed(Action::Down)) scroll_ = std::min(maxScroll, scroll_ + 3);
    if (input.pressed(Action::Up)) scroll_ = std::max(0, scroll_ - 3);
    return !(input.pressed(Action::Cancel) || input.pressed(Action::Confirm));
}

void TextScreen::render(SDL_Renderer* r) const {
    ui::dimScreen(r, 170);
    const SDL_FRect panel{50, 20, 540, 320};
    ui::panel(r, panel, title_.c_str());
    float y = panel.y + 26;
    for (std::size_t i = static_cast<std::size_t>(scroll_); i < lines_.size() && y < panel.y + panel.h - 26; ++i) {
        ui::text(r, panel.x + 14, y, lines_[i]);
        y += ui::kLineH;
    }
    ui::text(r, panel.x + 14, panel.y + panel.h - 16, "Up/Down: scroll   Esc: back", ui::kDim);
}

// ---------------------------------------------------------------------------- Controls

namespace {
// Actions players may rebind. Pause (Esc / Start) and the menu keys stay fixed so menus always work.
constexpr Action kRebindable[] = {Action::Left,     Action::Right,   Action::Up,       Action::Down,
                                  Action::Jump,     Action::UseRight, Action::UseLeft, Action::Interact,
                                  Action::Inventory, Action::Drop, Action::QuickMove};
constexpr int kRebindCount = static_cast<int>(std::size(kRebindable));
constexpr int kResetRow = kRebindCount;
constexpr int kBackRow = kRebindCount + 1;
constexpr SDL_FRect kControlsPanel{30, 16, 580, 330};
}  // namespace

std::string padButtonName(SDL_GamepadButton b) {
    switch (b) {
    case SDL_GAMEPAD_BUTTON_INVALID: return "-";
    case SDL_GAMEPAD_BUTTON_SOUTH: return "Pad A";
    case SDL_GAMEPAD_BUTTON_EAST: return "Pad B";
    case SDL_GAMEPAD_BUTTON_WEST: return "Pad X";
    case SDL_GAMEPAD_BUTTON_NORTH: return "Pad Y";
    case SDL_GAMEPAD_BUTTON_BACK: return "Back";
    case SDL_GAMEPAD_BUTTON_START: return "Start";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return "L-Stick";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return "R-Stick";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return "LB";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "RB";
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "D-Up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "D-Down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "D-Left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "D-Right";
    default: {
        const char* n = SDL_GetGamepadStringForButton(b);
        return n ? n : "?";
    }
    }
}

void SettingsScreen::updateControls() {
    Input& in = app_->input();
    if (in.capturing()) {
        const Input::Capture c = in.takeCapture();
        if (c.kind == Input::Capture::Kind::Waiting) return;
        const auto a = static_cast<std::size_t>(kRebindable[controlRow_]);
        if (c.kind == Input::Capture::Kind::Cancelled) {
            controlMessage_ = "Unchanged.";
        } else if (controlCol_ < 2 && c.kind == Input::Capture::Kind::Key) {
            // A key can only do one thing: take it away from any other action that had it.
            for (int other = 0; other < kRebindCount; ++other) {
                for (KeyBinding& k : in.keyBindings[static_cast<std::size_t>(kRebindable[other])]) {
                    if (k == c.key) {
                        k = {};
                        if (static_cast<std::size_t>(kRebindable[other]) != a) {
                            controlMessage_ = c.key.name() + " was removed from \"" + actionLabel(kRebindable[other]) + "\". ";
                        }
                    }
                }
            }
            in.keyBindings[a][static_cast<std::size_t>(controlCol_)] = c.key;
            controlMessage_ += std::string(actionLabel(kRebindable[controlRow_])) + ": " + c.key.name();
        } else if (controlCol_ == 2 && c.kind == Input::Capture::Kind::Pad) {
            in.padBindings[a][0] = c.pad;
            controlMessage_ = std::string(actionLabel(kRebindable[controlRow_])) + ": " + padButtonName(c.pad);
        } else {
            controlMessage_ = controlCol_ == 2 ? "Press a gamepad button for this column." : "Press a key or mouse button.";
        }
        app_->settings().bindings = in.bindingsToJson();
        return;
    }

    const int rows = kRebindCount + 2;
    if (in.pressed(Action::Up)) controlRow_ = (controlRow_ + rows - 1) % rows;
    if (in.pressed(Action::Down)) controlRow_ = (controlRow_ + 1) % rows;
    if (in.pressed(Action::Left)) controlCol_ = (controlCol_ + 2) % 3;
    if (in.pressed(Action::Right)) controlCol_ = (controlCol_ + 1) % 3;
    if (in.pressed(Action::Cancel)) {
        showingControls_ = false;
        app_->settings().save();
        return;
    }
    if (!in.pressed(Action::Confirm)) return;
    if (controlRow_ == kBackRow) {
        showingControls_ = false;
        app_->settings().save();
    } else if (controlRow_ == kResetRow) {
        in.resetBindings();
        app_->settings().bindings = in.bindingsToJson();
        controlMessage_ = "Controls reset to the defaults.";
    } else {
        in.startCapture();
        controlMessage_.clear();
    }
}

void SettingsScreen::renderControls(SDL_Renderer* r) const {
    const Input& in = app_->input();
    const SDL_FRect& p = kControlsPanel;
    ui::panel(r, p, "Controls");
    const float colX[3] = {p.x + 210, p.x + 330, p.x + 450};
    ui::text(r, colX[0], p.y + 24, "Key 1", ui::kDim);
    ui::text(r, colX[1], p.y + 24, "Key 2", ui::kDim);
    ui::text(r, colX[2], p.y + 24, "Gamepad", ui::kDim);

    for (int row = 0; row < kRebindCount + 2; ++row) {
        const float y = p.y + 40 + static_cast<float>(row) * 18.0f + (row >= kRebindCount ? 8.0f : 0.0f);
        const bool selRow = row == controlRow_;
        if (row >= kRebindCount) {
            ui::text(r, p.x + 16, y, (selRow ? "> " : "  ") + std::string(row == kResetRow ? "Reset to defaults" : "Back"),
                     selRow ? ui::kGold : ui::kText);
            continue;
        }
        const auto a = static_cast<std::size_t>(kRebindable[row]);
        ui::text(r, p.x + 16, y, (selRow ? "> " : "  ") + std::string(actionLabel(kRebindable[row])),
                 selRow ? ui::kGold : ui::kText);
        const std::string cells[3] = {in.keyBindings[a][0].name(), in.keyBindings[a][1].name(),
                                      padButtonName(in.padBindings[a][0])};
        for (int c = 0; c < 3; ++c) {
            const bool sel = selRow && c == controlCol_;
            if (sel) {
                const SDL_FRect box{colX[c] - 4, y - 3, 112, 14};
                SDL_SetRenderDrawColor(r, 90, 70, 40, 255);
                SDL_RenderFillRect(r, &box);
            }
            std::string text = sel && in.capturing() ? "press..." : cells[c];
            if (text.size() > 13) text = text.substr(0, 13);
            ui::text(r, colX[c], y, text, sel ? ui::kGold : ui::kText);
        }
    }
    const float footer = p.y + p.h - 44;
    if (!controlMessage_.empty()) ui::text(r, p.x + 16, footer, controlMessage_.substr(0, 68), ui::kGood);
    ui::text(r, p.x + 16, footer + 13, "Left click / right click also use the right / left hand.", ui::kDim);
    ui::text(r, p.x + 16, footer + 26,
             in.capturing() ? "Press the new key, mouse or pad button.  Esc: keep the old one."
                            : "Enter: change   Esc: back   (Pause = Esc / Start, fixed)",
             ui::kDim);
}

// ---------------------------------------------------------------------------- About

std::string aboutText() {
    return std::string("Cave Explorer  v") + App::kVersion +
           "\n\nA 2D platformer about exploring a deep cave complex. Start on the beach, buy gear at "
           "the shop, pass through the forest and descend into the caves.\n\n"
           "When an explorer dies, everything they carried stays where they fell - for the next "
           "explorer to find. The storage chest in the forest keeps its contents across explorers.\n\n"
           "Each save slot is its own world: its own chest, its own fallen explorers.\n\n"
           "Code licensed under the GNU GPL v3.\n"
           "github.com/josephcolton/CaveExplorer\n\n"
           "Built with SDL3.";
}
