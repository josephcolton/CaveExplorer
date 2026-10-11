# Cave Explorer

A 2D platformer about exploring a deep cave complex. Start on the beach, buy gear at the
shop, pass through the forest and descend into the caves. When an explorer dies, everything
they carried stays where they fell — for the next explorer to find.

Built with C++20 and SDL3. Runs on Windows and Linux (Ubuntu). See [docs/DESIGN.md](docs/DESIGN.md)
for the design and roadmap.

## Download and play

Ready-to-play builds for **Windows** (`.zip`) and **Linux** (`.tar.gz`) are produced for every
change: open the repository's **Actions** tab, pick the latest successful "Build" run, and download
`CaveExplorer-Windows` or `CaveExplorer-Linux` under *Artifacts*.

- **Windows:** unzip anywhere and run `cave_explorer.exe`.
- **Linux:** `tar xzf CaveExplorer-*.tar.gz`, then run `./cave_explorer` inside the folder.
  The SDL libraries it needs are included. (`cave-explorer.desktop` is a starting point for a
  menu entry; edit its `Exec=` and `Icon=` paths.)

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

To make a package like the downloadable ones:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
cd build-release && cpack     # -> CaveExplorer-<version>-windows-x64.zip / -linux-x64.tar.gz
```

On Windows the game has no console window; configure with `-DCAVE_WINDOWS_CONSOLE=ON` to get one
showing log messages while developing. The version number comes from `project(... VERSION ...)`
in `CMakeLists.txt`.

## Controls (defaults)

All of these can be changed in **Settings → Controls** (two keys or mouse buttons plus a gamepad
button per action). Pause (Esc / Start) and the menu keys are fixed.

| Action             | Keyboard / mouse           | Gamepad            |
|--------------------|----------------------------|--------------------|
| Move               | A/D or ←/→                 | D-pad / left stick |
| Jump               | Space or Z                 | A (south)          |
| Drop through ledge | Down + Jump                | Down + A           |
| Enter door / cave  | W or ↑                     | Up                 |
| Use right hand     | J, X or left click         | X (west) / RB      |
| Use left hand      | K, C or right click        | LB                 |
| Interact (talk, open, search) | E               | Y (north)          |
| Inventory          | Tab or I                   | Back / View        |
| Drop item (in inventory) | Q                    | Right stick click  |
| Quick move (inventory/chest) | Shift            | X (west)           |
| Climb ladder       | W/↑ and S/↓ (Jump to let go) | D-pad          |
| Pause menu         | Esc                        | Start              |
| Fullscreen         | F11 / Alt+Enter            |                    |
| Debug overlay      | F3                         |                    |

## Maps

Areas are [Tiled](https://www.mapeditor.org) maps in `assets/maps/*.tmj` using 32x32 tiles.
- Map properties: `display_name` (shown on entry), `restart_spawn` (where Esc → Restart area puts
  you: the spawn on the way back toward the surface) and `music` (a file in `assets/soundtrack/`, looped
  while in the area and crossfaded on area changes; empty = silence).
- Tile layers: `background` (decoration) and `ground` (custom property `collides = true`).
  A layer with `foreground = true` is drawn in front of the player.
- Tile collision comes from the tileset tile property `collision` = `solid` or `oneway`;
  `surface` = `slippery` makes slick ground (moss) that only non-slip boots grip.
- Object layer `objects`:
  - `spawn` (point, named) — where the player appears.
  - `door` (rectangle) with `target_area`, `target_spawn`, and `trigger` = `touch` (area edge)
    or `interact` (press Up). Interactive doors show a ↑ marker; optional `label` (prompt text)
    and `glow` (`#rrggbb` light spilling through, e.g. daylight at a cave exit).

`tools/check_reach.py` checks every map for places the explorer can't reach (or can't leave)
using the game's jump limits. Run it after editing maps.

`tools/make_maps.py` generated the starting maps and placeholder tileset; it will not overwrite
existing maps unless run with `--force`. Edit the maps in Tiled from here on.

Objects also include `obstacle` (a rectangle covering whole tiles; `obstacle` type from
`data/obstacles.json`, e.g. `bush` needs a tool tagged `cut`, `rock` needs `break`;
`passable_from` = `left`/`right` lets players push through from that side), `npc` (point at the
feet; `npc_id` from `data/npcs.json`), `shrine` (point at the feet; `shrine_id`), `ladder` (a one-tile-wide rectangle; its top can be
stood on), `enemy` (point at the feet of a walker or the centre of a flyer; `enemy` type
from `data/enemies.json`, optional `enemy_easy` / `enemy_hard` overrides where `none` removes it,
and `hp_scale`), `pickup` (point; `item` id or `coins`, `count`, optional `unique_id` so it
never respawns once taken), `chest` (the persistent storage chest), `shop` (`shop_id` from
`data/shops.json`) and `decor` (`sprite` name from `data/sprites.json`, drawn on its bottom-left).

## Music

Tracks the game uses live in `assets/soundtrack/`. `assets/music/` is ignored by git and is a
scratch area for auditioning downloads. Current tracks: `beach.ogg` (The Beach), `forest.ogg` (The Forest), `upper_caves.ogg`
(Upper Caves) and `deep_cave.ogg`
(reserved for a deep cave area) `boss_1.ogg` / `boss_2.ogg` / `boss_3.mp3` (boss arenas) and
`underground_city.mp3` (reserved for the underground city).

## Game data

- `data/items.json` — every item: name, icon, equipment slot, stack size, value, damage, food...
- `data/shops.json` — shop names and stock.
- `data/sprites.json` — named sprites in the sprite sheets.
- `data/animations.json` — animated sprite sheets (the knight, the bat) and their clips.
- `data/enemies.json` — creatures: behaviour (walker/flyer), health, damage, speed, drops.
- `data/characters.json` — names and stat ranges for newly rolled explorers.
- `data/obstacles.json` — bushes, boulders and other blockers: required tool tag, hits, art, hint.
- `data/sounds.json` — which sound file(s) play for each game event, and how loud.
- `data/shrines.json` — shrines: which ability they grant and for how long (0 = until death).
- `data/npcs.json`, `data/quests.json`, `data/abilities.json` — people, their jobs, and the lasting
  abilities jobs can teach. Items get effects through `carried` / `equipped` in `items.json`.
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
--hunger <0-100>             dev game: start this hungry
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
- Bushes need something sharp (machete, hand axe); boulders need a pickaxe. Swing at them with
  the hand that holds the tool. Obstacles never trap you: from the far side you can push through
  (a bush scratches a little).
- Stuck? **Esc → Restart area** puts you back at the area's entrance from the surface side. Creatures and
  obstacles reset; your health, items and quests don't.
- Some items help just by being carried (the Feather Charm), others when worn or held (boots,
  helmets, the buckler). Hover an item in the inventory to see its effects; hover an empty slot
  for your explorer's stats.
- Hunger drains slowly as you explore (a full belly lasts about 15 minutes). Below "Hungry" your
  wounds stop healing; when "Starving" you're slower and can't jump as high. Eat food (E in the
  inventory) to fill up; healing potions restore health.
- Shrines (glowing altars) grant blessings: Leaping, Swiftness, Might, Warding (can't be hurt),
  Shadows (creatures can't see you) for a short time, or Vigor (+10 max health) until death.
  A shrine goes dormant once used and recharges when its area reloads.
- People in the caves trade and hand out small jobs. Jobs and the abilities they teach last until
  the explorer dies.
- When an explorer dies, everything they carried stays where they fell (search it with E), and a
  new explorer with different stats arrives at the beach.
```
```
