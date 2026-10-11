#include "ui/Ui.h"

#include "core/App.h"
#include "core/Input.h"
#include "core/Sprites.h"
#include "game/Inventory.h"

#include <cmath>
#include <limits>
#include <sstream>

namespace ui {

namespace {
std::function<void(const char*)>& soundHook() {
    static std::function<void(const char*)> hook;
    return hook;
}
}  // namespace

void setSoundHook(std::function<void(const char*)> hook) { soundHook() = std::move(hook); }
void sound(const char* name) {
    if (soundHook()) soundHook()(name);
}

void text(SDL_Renderer* r, float x, float y, const std::string& s, SDL_Color c) {
    x = std::round(x);
    y = std::round(y);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
    SDL_RenderDebugText(r, x + 1, y + 1, s.c_str());
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDebugText(r, x, y, s.c_str());
}

float textWidth(const std::string& s) { return static_cast<float>(s.size()) * kCharW; }

void textCentered(SDL_Renderer* r, float cx, float y, const std::string& s, SDL_Color c) {
    text(r, cx - textWidth(s) * 0.5f, y, s, c);
}

std::vector<std::string> wrap(const std::string& s, std::size_t maxChars) {
    std::vector<std::string> lines;
    std::istringstream words(s);
    std::string word, line;
    while (words >> word) {
        if (!line.empty() && line.size() + 1 + word.size() > maxChars) {
            lines.push_back(line);
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

void panel(SDL_Renderer* r, const SDL_FRect& rect, const char* title) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 24, 20, 28, 255);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, 120, 98, 70, 255);
    SDL_RenderRect(r, &rect);
    const SDL_FRect inner{rect.x + 2, rect.y + 2, rect.w - 4, rect.h - 4};
    SDL_SetRenderDrawColor(r, 60, 50, 44, 255);
    SDL_RenderRect(r, &inner);
    if (title) text(r, rect.x + 8, rect.y + 7, title, kGold);
}

void dimScreen(SDL_Renderer* r, Uint8 alpha) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, alpha);
    const SDL_FRect all{0, 0, App::kViewWidth, App::kViewHeight};
    SDL_RenderFillRect(r, &all);
}

void slot(SDL_Renderer* r, const SpriteDb& sprites, const ItemDb& db, const SDL_FRect& rect, const ItemStack& stack,
          bool cursor, bool selected, const char* emptyLabel) {
    SDL_SetRenderDrawColor(r, selected ? 70 : 40, selected ? 60 : 36, selected ? 30 : 44, 255);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, 90, 80, 70, 255);
    SDL_RenderRect(r, &rect);

    if (!stack.empty()) {
        sprites.drawIcon(r, db.get(stack.id).icon, rect.x + (rect.w - 32) * 0.5f, rect.y + (rect.h - 32) * 0.5f);
        if (stack.count > 1) {
            const std::string n = std::to_string(stack.count);
            text(r, rect.x + rect.w - 2 - textWidth(n), rect.y + rect.h - 9, n);
        }
    } else if (emptyLabel) {
        textCentered(r, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f - 4, emptyLabel, SDL_Color{80, 76, 72, 255});
    }

    if (selected) {
        SDL_SetRenderDrawColor(r, kGold.r, kGold.g, kGold.b, 255);
        SDL_RenderRect(r, &rect);
    }
    if (cursor) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        const SDL_FRect a{rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4};
        const SDL_FRect b{rect.x - 1, rect.y - 1, rect.w + 2, rect.h + 2};
        SDL_RenderRect(r, &a);
        SDL_RenderRect(r, &b);
    }
}

int navigate(const std::vector<SDL_FRect>& rects, int current, const Input& input) {
    if (rects.empty()) return 0;
    if (current < 0 || current >= static_cast<int>(rects.size())) return 0;
    float dx = 0, dy = 0;
    if (input.pressed(Action::Left)) dx = -1;
    else if (input.pressed(Action::Right)) dx = 1;
    else if (input.pressed(Action::Up)) dy = -1;
    else if (input.pressed(Action::Down)) dy = 1;
    else return current;

    const SDL_FRect& c = rects[static_cast<std::size_t>(current)];
    const float cx = c.x + c.w * 0.5f, cy = c.y + c.h * 0.5f;
    int best = current;
    float bestScore = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < rects.size(); ++i) {
        if (static_cast<int>(i) == current) continue;
        const float ox = rects[i].x + rects[i].w * 0.5f - cx;
        const float oy = rects[i].y + rects[i].h * 0.5f - cy;
        const float along = ox * dx + oy * dy;  // distance in the pressed direction
        if (along <= 1.0f) continue;
        const float across = std::fabs(dx != 0 ? oy : ox);
        const float score = along + across * 2.5f;
        if (score < bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    if (best != current) sound("menu_move");
    return best;
}

}  // namespace ui
