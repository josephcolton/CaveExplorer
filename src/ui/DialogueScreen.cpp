#include "ui/DialogueScreen.h"

#include "core/Sprites.h"
#include "ui/Ui.h"

namespace {
constexpr SDL_FRect kPanel{40, 186, 560, 160};
constexpr std::size_t kWrap = 58;
}  // namespace

void DialogueScreen::open(const SpriteDb& sprites, std::string name, std::string icon) {
    sprites_ = &sprites;
    name_ = std::move(name);
    icon_ = std::move(icon);
}

void DialogueScreen::say(const std::string& text, std::vector<std::string> replies) {
    lines_ = ui::wrap(text, kWrap);
    replyText_ = replies;
    replies_.setItems(std::move(replies));
}

void DialogueScreen::render(SDL_Renderer* r) const {
    ui::dimScreen(r, 90);
    ui::panel(r, kPanel);
    // Portrait at 2x.
    const SDL_FRect frame{kPanel.x + 10, kPanel.y + 10, 68, 68};
    SDL_SetRenderDrawColor(r, 40, 34, 40, 255);
    SDL_RenderFillRect(r, &frame);
    sprites_->drawIcon(r, icon_, frame.x + 2, frame.y + 2, 64.0f);
    ui::textCentered(r, frame.x + frame.w * 0.5f, frame.y + frame.h + 6, name_, ui::kGold);

    float y = kPanel.y + 12;
    for (const std::string& line : lines_) {
        ui::text(r, kPanel.x + 92, y, line);
        y += ui::kLineH;
    }

    // Replies, left-aligned under the text.
    y = std::max(y + 8.0f, kPanel.y + 70.0f);
    for (std::size_t i = 0; i < replyText_.size(); ++i) {
        const bool sel = static_cast<int>(i) == replies_.cursor();
        ui::text(r, kPanel.x + 92, y, (sel ? "> " : "  ") + replyText_[i], sel ? ui::kGold : ui::kDim);
        y += 13.0f;
    }
}
