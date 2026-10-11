# Cave Explorer

A 2D platformer about exploring a deep cave complex. Start on the beach, buy gear at the
shop, pass through the forest and descend into the caves. When an explorer dies, everything
they carried stays where they fell — for the next explorer to find.

Built with C++20 and SDL3. Runs on Windows and Linux (Ubuntu). See [docs/DESIGN.md](docs/DESIGN.md)
for the design and roadmap.

## Building

Requirements: CMake 3.25+, a C++20 compiler (GCC 11+, Clang 14+, or MSVC 2022), Git, and
internet access on the first configure (SDL3, SDL3_image, SDL3_mixer, nlohmann/json and doctest are
fetched automatically). Run the unit tests with `ctest --test-dir build` or `build/cave_tests`.

On Ubuntu, install SDL's system dependencies first:

```sh
sudo apt install build-essential cmake ninja-build git libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxss-dev libxtst-dev libwayland-dev libxkbcommon-dev \
  libegl1-mesa-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev
```

Then (Windows or Linux):

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/cave_explorer        # cave_explorer.exe on Windows
```

## Controls (defaults)

| Action             | Keyboard          | Gamepad            |
|--------------------|-------------------|--------------------|
| Move               | A/D or ←/→        | D-pad / left stick |
| Jump               | Space             | A (south)          |
| Drop through ledge | Down + Space      | Down + A           |
| Enter door / cave  | W / ↑ / E         | Up / Y (north)     |
| Open chest / shop  | E                 | Y (north)          |
| Inventory          | Tab / I           | Back / View        |
| Menus: move / pick | Arrows, Enter     | D-pad, A           |
| Menus: use / drop  | E / Q             | Y / right stick    |
| Use right hand     | J                 | X (west) / RB      |
| Use left hand      | K                 | LB                 |
| Pause menu         | Esc               | Start              |
| Fullscreen         | F11 / Alt+Enter   |                    |
| Debug overlay      | F3                |                    |

## Maps

Areas are [Tiled](https://www.mapeditor.org) maps in `assets/maps/*.tmj` using 32x32 tiles.
- Map properties: `display_name` (shown on entry) and `music` (a file in `assets/soundtrack/`, looped
  while in the area and crossfaded on area changes; empty = silence).
- Tile layers: `background` (decoration) and `ground` (custom property `collides = true`).
  A layer with `foreground = true` is drawn in front of the player.
- Tile collision comes from the tileset tile property `collision` = `solid` or `oneway`.
- Object layer `objects`:
  - `spawn` (point, named) — where the player appears.
  - `door` (rectangle) with `target_area`, `target_spawn`, and `trigger` = `touch` (area edge)
    or `interact` (press Up).

`tools/make_maps.py` generated the starting maps and placeholder tileset; it will not overwrite
existing maps unless run with `--force`. Edit the maps in Tiled from here on.

Objects also include `obstacle` (a rectangle covering whole tiles; `obstacle` type from
`data/obstacles.json`, e.g. `bush` needs a tool tagged `cut`, `rock` needs `break`), `enemy` (point at the feet of a walker or the centre of a flyer; `enemy` type
from `data/enemies.json`, optional `enemy_easy` / `enemy_hard` overrides where `none` removes it,
and `hp_scale`), `pickup` (point; `item` id or `coins`, `count`, optional `unique_id` so it
never respawns once taken), `chest` (the persistent storage chest), `shop` (`shop_id` from
`data/shops.json`) and `decor` (`sprite` name from `data/sprites.json`, drawn on its bottom-left).

## Music

Tracks the game uses live in `assets/soundtrack/`. `assets/music/` is ignored by git and is a
scratch area for auditioning downloads. Current tracks: `forest.ogg` (The Forest) and `deep_cave.ogg`
(reserved for a deep cave area) `boss_1.ogg` / `boss_2.ogg` (the first two boss arenas) and
`underground_city.mp3` (reserved for the underground city).

## Game data

- `data/items.json` — every item: name, icon, equipment slot, stack size, value, damage, food...
- `data/shops.json` — shop names and stock.
- `data/sprites.json` — named sprites in the sprite sheets.
- `data/animations.json` — animated sprite sheets (the knight, the bat) and their clips.
- `data/enemies.json` — creatures: behaviour (walker/flyer), health, damage, speed, drops.
- `data/characters.json` — names and stat ranges for newly rolled explorers.
- `data/obstacles.json` — bushes, boulders and other blockers: required tool tag, hits, art, hint.
- `data/credits.txt` — the in-game credits page.
- Item icons are packed by `tools/pack_icons.py` from the list in `tools/icons.txt`.

Save slots (`slot1.json` ... `slot3.json`) and `settings.json` live in the per-user data folder (`%APPDATA%\CaveExplorer\CaveExplorer` on Windows,
`~/.local/share/CaveExplorer/CaveExplorer` on Linux). Delete a slot file to reset that world.

See [CREDITS.md](CREDITS.md) for art and library credits.

## Debug command-line options

```
--area <id> --spawn <name>   start in a given area (spawn may also be tile coords "tx,ty")
--debug                      start with the debug overlay on
--shot <file.bmp> --ticks N  run N simulation ticks (60/s), save a screenshot, and exit
--script "Right:0-120,UseRight:130"
                             hold actions during tick ranges (automated testing)
--give <id,id,...>           start with these items (weapons go into empty hands)
--ui inventory|chest|shop|pause      open an in-game screen immediately
--ui slots|settings|credits|about    open a title-screen page immediately
```
`--area`, `--give` and the in-game `--ui` pages start a throwaway "dev game" that never saves.

## Playing

- **Title screen → Play** lists three save slots. An empty slot starts a new expedition;
  an existing one can be continued or deleted.
- Each slot is its own world: its own forest chest, its own fallen explorers and their belongings.
- The game autosaves when you change areas, close the chest, take a unique item, or die, and
  whenever you leave through the pause menu (Esc). "Save game" in the pause menu saves on demand.
- Hold weapons in your hands (inventory, Tab): the right hand attacks with J, the left with K.
  Slings fire stones from your backpack.
- Creatures and obstacles come back every time an area loads. Coins and items lying in the world
  do not: once an explorer takes them they stay gone until the next explorer arrives (after a
  death). Unique items never come back.
- Bushes need something sharp (machete, hand axe); boulders need a pickaxe. Swing at them.
- When an explorer dies, everything they carried stays where they fell (search it with E), and a
  new explorer with different stats arrives at the beach.
```
```
