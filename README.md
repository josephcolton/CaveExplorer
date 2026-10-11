# Cave Explorer

A 2D platformer about exploring a deep cave complex. Start on the beach, buy gear at the
shop, pass through the forest and descend into the caves. When an explorer dies, everything
they carried stays where they fell — for the next explorer to find.

Built with C++20 and SDL3. Runs on Windows and Linux (Ubuntu). See [docs/DESIGN.md](docs/DESIGN.md)
for the design and roadmap.

## Building

Requirements: CMake 3.25+, a C++20 compiler (GCC 11+, Clang 14+, or MSVC 2022), Git, and
internet access on the first configure (SDL3, SDL3_image, nlohmann/json and doctest are
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
| Right-hand item    | J                 | X (west) / RB      |
| Left-hand item     | K                 | LB                 |
| Fullscreen         | F11 / Alt+Enter   |                    |
| Debug overlay      | F3                |                    |

## Maps

Areas are [Tiled](https://www.mapeditor.org) maps in `assets/maps/*.tmj` using 32x32 tiles.
- Tile layers: `background` (decoration) and `ground` (custom property `collides = true`).
  A layer with `foreground = true` is drawn in front of the player.
- Tile collision comes from the tileset tile property `collision` = `solid` or `oneway`.
- Object layer `objects`:
  - `spawn` (point, named) — where the player appears.
  - `door` (rectangle) with `target_area`, `target_spawn`, and `trigger` = `touch` (area edge)
    or `interact` (press Up).

`tools/make_maps.py` generated the starting maps and placeholder tileset; it will not overwrite
existing maps unless run with `--force`. Edit the maps in Tiled from here on.

Objects also include `pickup` (point; `item` id or `coins`, `count`, optional `unique_id` so it
never respawns once taken), `chest` (the persistent storage chest), `shop` (`shop_id` from
`data/shops.json`) and `decor` (`sprite` name from `data/sprites.json`, drawn on its bottom-left).

## Game data

- `data/items.json` — every item: name, icon, equipment slot, stack size, value, damage, food...
- `data/shops.json` — shop names and stock.
- `data/sprites.json` — named sprites in the sprite sheets.
- Item icons are packed by `tools/pack_icons.py` from the list in `tools/icons.txt`.

The persistent world save (chest contents, unique items taken) is `world.json` in the
per-user data folder (`%APPDATA%\CaveExplorer\CaveExplorer` on Windows,
`~/.local/share/CaveExplorer/CaveExplorer` on Linux). Delete it to reset the world.

See [CREDITS.md](CREDITS.md) for art and library credits.

## Debug command-line options

```
--area <id> --spawn <name>   start in a given area
--debug                      start with the debug overlay on
--shot <file.bmp> --frames N run N frames, save a screenshot, and exit
--hold-right                 simulate holding Right (automated movement checks)
--give <id,id,...>           start with these items in the backpack
--ui inventory|chest|shop    open a screen immediately
```
