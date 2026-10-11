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
    if (input.pressed(Action::Up)) cursor_ = (cursor_ + n - 1) % n;
    if (input.pressed(Action::Down)) cursor_ = (cursor_ + 1) % n;
    if (input.pressed(Action::Confirm)) return cursor_;
    if (input.pressed(Action::Cancel)) return kCancelled;
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
enum Row { MusicRow, SoundRow, FullscreenRow, DifficultyRow, ControlsRow, BackRow, RowCount };
constexpr SDL_FRect kSettingsPanel{150, 60, 340, 240};
}  // namespace

void SettingsScreen::open(App& app) {
    app_ = &app;
    cursor_ = 0;
    showingControls_ = false;
}

SettingsScreen::Result SettingsScreen::update() {
    const Input& in = app_->input();
    if (showingControls_) {
        if (in.pressed(Action::Cancel) || in.pressed(Action::Confirm)) showingControls_ = false;
        if (in.down(Action::Down)) controlsScroll_ += 2.0f;
        if (in.down(Action::Up)) controlsScroll_ = std::max(0.0f, controlsScroll_ - 2.0f);
        return Result::Stay;
    }

    Settings& s = app_->settings();
    if (in.pressed(Action::Up)) cursor_ = (cursor_ + RowCount - 1) % RowCount;
    if (in.pressed(Action::Down)) cursor_ = (cursor_ + 1) % RowCount;
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
            controlsScroll_ = 0.0f;
        }
        break;
    case BackRow:
        if (confirm) {
            s.save();
            return Result::Close;
        }
        break;
    }
    if (changed) app_->applySettings();
    if (in.pressed(Action::Cancel)) {
        s.save();
        return Result::Close;
    }
    return Result::Stay;
}

void SettingsScreen::render(SDL_Renderer* r) const {
    ui::dimScreen(r, 170);
    if (showingControls_) {
        const SDL_FRect panel{90, 30, 460, 300};
        ui::panel(r, panel, "Controls");
        SDL_Rect clip{static_cast<int>(panel.x), static_cast<int>(panel.y + 24), static_cast<int>(panel.w),
                      static_cast<int>(panel.h - 44)};
        SDL_SetRenderClipRect(r, &clip);
        float y = panel.y + 28 - controlsScroll_;
        std::istringstream lines(describeControls(app_->input()));
        for (std::string line; std::getline(lines, line);) {
            ui::text(r, panel.x + 14, y, line);
            y += ui::kLineH;
        }
        SDL_SetRenderClipRect(r, nullptr);
        ui::text(r, panel.x + 14, panel.y + panel.h - 16, "Rebinding comes in a later update.  Esc: back", ui::kDim);
        return;
    }

    const Settings& s = app_->settings();
    ui::panel(r, kSettingsPanel, "Settings");
    auto bar = [](int v) { return std::string(static_cast<std::size_t>(v), '#') + std::string(static_cast<std::size_t>(10 - v), '-'); };
    const std::string rows[RowCount][2] = {
        {"Music volume", "< " + bar(s.musicVolume) + " >"},
        {"Sound volume", "< " + bar(s.soundVolume) + " >"},
        {"Fullscreen", s.fullscreen ? "< On >" : "< Off >"},
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

// ---------------------------------------------------------------------------- Controls text

std::string describeControls(const Input& input) {
    static const std::pair<Action, const char*> rows[] = {
        {Action::Left, "Move left"},          {Action::Right, "Move right"},
        {Action::Up, "Up / enter door"},      {Action::Down, "Down"},
        {Action::Jump, "Jump"},               {Action::UseRight, "Use right hand"},
        {Action::UseLeft, "Use left hand"},   {Action::Interact, "Interact / use item"},
        {Action::Inventory, "Inventory"},     {Action::Drop, "Drop item (inventory)"},
        {Action::Pause, "Pause menu"},        {Action::Confirm, "Menu: select"},
        {Action::Cancel, "Menu: back"},
    };
    std::ostringstream out;
    for (const auto& [action, label] : rows) {
        const auto i = static_cast<std::size_t>(action);
        std::string keys;
        for (SDL_Scancode sc : input.keyBindings[i]) keys += (keys.empty() ? "" : ", ") + std::string(SDL_GetScancodeName(sc));
        std::string pad;
        for (SDL_GamepadButton b : input.padBindings[i]) {
            const char* n = SDL_GetGamepadStringForButton(b);
            pad += (pad.empty() ? "" : ", ") + std::string(n ? n : "?");
        }
        std::string line = std::string(label);
        line.resize(24, ' ');
        line += keys;
        line.resize(46, ' ');
        out << line << pad << "\n";
    }
    out << "\nAlso: F11 or Alt+Enter toggles fullscreen.\n";
    return out.str();
}
