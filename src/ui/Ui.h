#pragma once

#include <SDL3/SDL.h>

#include <functional>
#include <string>
#include <vector>

class Input;
class SpriteDb;
struct ItemStack;
class ItemDb;

// Small immediate-mode drawing helpers shared by the menus. Text uses SDL's
// built-in 8x8 debug font until we add SDL_ttf.
namespace ui {

constexpr float kCharW = 8.0f;
constexpr float kLineH = 11.0f;
constexpr float kSlotSize = 36.0f;

inline const SDL_Color kText{232, 228, 216, 255};
inline const SDL_Color kDim{150, 146, 140, 255};
inline const SDL_Color kGold{246, 204, 92, 255};
inline const SDL_Color kBad{230, 110, 100, 255};
inline const SDL_Color kGood{140, 220, 130, 255};

void text(SDL_Renderer* r, float x, float y, const std::string& s, SDL_Color c = kText);
void textCentered(SDL_Renderer* r, float cx, float y, const std::string& s, SDL_Color c = kText);
float textWidth(const std::string& s);
// Greedy word wrap to a max width in characters.
std::vector<std::string> wrap(const std::string& s, std::size_t maxChars);

void panel(SDL_Renderer* r, const SDL_FRect& rect, const char* title = nullptr);
void dimScreen(SDL_Renderer* r, Uint8 alpha = 140);

// Draws an item slot with its icon and stack count. ghostIcon is drawn faintly when empty.
void slot(SDL_Renderer* r, const SpriteDb& sprites, const ItemDb& db, const SDL_FRect& rect, const ItemStack& stack,
          bool cursor, bool selected, const char* emptyLabel = nullptr);

// Menu sounds: the app installs a hook; screens call ui::sound("menu_move") etc.
void setSoundHook(std::function<void(const char*)> hook);
void sound(const char* name);

// Moves a cursor between arbitrary rectangles: picks the nearest rect in the pressed direction.
int navigate(const std::vector<SDL_FRect>& rects, int current, const Input& input);

}  // namespace ui
