# Cave Explorer

A 2D platformer about exploring a deep cave complex. Start on the beach, buy gear at the shop,
pass through the forest and descend into the caves. Each new piece of gear opens another part of
the caves, so you keep coming back for more. When an explorer dies, everything they carried stays
where they fell — for the next explorer to find.

Built with C++20 and SDL3. Runs on Windows and Linux (Ubuntu). The design and roadmap are in
[docs/DESIGN.md](docs/DESIGN.md).

## Download and play

Ready-to-play builds for **Windows** (`.zip`) and **Linux** (`.tar.gz`) are made for every change:
open the repository's **Actions** tab, pick the latest successful "Build" run, and download
`CaveExplorer-Windows` or `CaveExplorer-Linux` under *Artifacts* (you need to be signed in to GitHub).

- **Windows:** unzip anywhere and run `cave_explorer.exe`.
- **Linux:** `tar xzf CaveExplorer-*.tar.gz`, then run `./cave_explorer` inside the folder. The SDL
  libraries it needs are included. (`cave-explorer.desktop` is a starting point for a menu entry;
  edit its `Exec=` and `Icon=` paths.)

## How to play

### Controls (defaults)

Everything except Pause and the menu keys can be changed in **Settings → Controls** (two keys or
mouse buttons plus a gamepad button per action).

| Action                        | Keyboard / mouse                 | Gamepad            |
|-------------------------------|----------------------------------|--------------------|
| Move                          | A/D or ←/→                       | D-pad / left stick |
| Jump                          | Space or Z                       | A (south)          |
| Drop through a wooden ledge   | Down + Jump                      | Down + A           |
| Enter a doorway / climb up    | W or ↑                           | Up                 |
| Climb down                    | S or ↓ (Jump lets go)            | Down               |
| Use right hand                | J, X or left click               | X (west) / RB      |
| Use left hand                 | K, C or right click              | LB                 |
| Interact (talk, open, search, pray) | E                          | Y (north)          |
| Inventory                     | Tab or I                         | Back / View        |
| Drop item (inventory)         | Q                                | Right stick click  |
| Quick move (inventory/chest)  | Shift                            | X (west)           |
| Pause menu                    | Esc                              | Start              |
| Fullscreen                    | F11 / Alt+Enter                  |                    |
| Debug overlay                 | F3                               |                    |

### Playing

- **Save slots:** Title screen → Play shows three slots. An empty slot starts a new expedition; an
  existing one can be continued or deleted. Each slot is its own world, with its own chest, fallen
  explorers and unique items already taken.
- **Saving:** the game autosaves when you change areas, close the chest, take a unique item or die,
  and whenever you leave through the pause menu. "Save game" in the pause menu saves on demand.
- **Hands:** hold weapons and tools in your hands (inventory, Tab). Each hand has its own button.
  Slings fire stones from your backpack. A lantern or shield in a hand gives up that hand's attack.
- **Items:** 9 backpack slots plus head, body, feet and two hands. Some items help just by being
  carried (Feather Charm, Amulet of Deep Breathing); others when worn or held (boots, helmets,
  the buckler, the lantern). Point at an item in the inventory to see what it does; point at an empty
  slot to see your explorer's stats and active effects.
- **The chest** in the forest keeps its contents across explorers. Shift sends a whole stack across.
- **Obstacles:** bushes need something sharp (machete, hand axe); boulders need a pickaxe. Swing with
  the hand that holds the tool. Obstacles never trap you: from the far side you can push through.
- **What comes back:** creatures and obstacles return every time an area loads. Coins and items lying
  in the world don't: once taken they stay gone until the next explorer. Unique items never return.
- **Stuck?** Esc → **Restart area** puts you back at the area's entrance on the surface side.
  Creatures and obstacles reset; your health, items and quests don't.
- **Ladders:** Up to grab and climb, Down to climb down, Jump to let go. You can stand on top.
- **Water:** you float and tread water at the surface. Hold Down to dive, Up or Jump to swim up, and
  Jump at the surface (or swim into a low shore) to climb out. Underwater you have 10 seconds of air
  (the bar above your head); after that you start drowning.
- **Darkness:** the caves are dark. Hold a **lantern**: it burns **lamp oil** from your pack (one
  flask lasts about four minutes, only while it's dark) and refills itself. Glowing moss, crystals,
  shrines and daylight at exits give some light too. **Settings → Cave brightness** helps if it's
  too dark to play comfortably.
- **Hunger** drains slowly (a full belly lasts about 15 minutes). When "Hungry", wounds stop healing;
  when "Starving", you're slower and jump lower. Eat food (E in the inventory); potions heal.
- **Shrines** (glowing altars, press E) grant blessings for a while — Leaping, Swiftness, Might,
  Warding (can't be hurt), Shadows (creatures can't see you), Deep Breath (double air) — or Vigor
  (+10 max health) until death. A used shrine recharges when its area reloads.
- **People** in the caves trade and hand out small jobs. Jobs and the abilities they teach last until
  the explorer dies.
- **Death:** everything the explorer carried stays where they fell (search it with E), and a new
  explorer with different stats arrives at the beach.

### The world so far

```
Beach (shop, sea, Shrine of Deep Breath)
  -> Forest (storage chest)
     -> Upper Caves (Old Hob) - the hub
          |- east, past a thorny bush [machete / hand axe] ........ Mossy Hollow
          |- west (open) -> Drowned Tunnel: a long flooded swim
          |                 [Amulet of Deep Breathing / Deep Breath shrine] -> Sunken Grotto
          |- a ledge high on the east wall [Feather Charm / Leaping shrine] .. Bat Roost
          '- the top tunnel, past a boulder [pickaxe] .................. Crystal Mine
```

In Mossy Hollow a boulder [pickaxe] hides the Feather Charm, and a high ledge [high jump] holds the
Amulet of Deep Breathing (its temporary home until deeper areas exist).

## Building

Requirements: CMake 3.25+, a C++20 compiler (GCC 11+, Clang 14+ or MSVC 2022), Git, Python 3 (for
the tools), and internet access the first time you configure (SDL3, SDL3_image, SDL3_mixer,
nlohmann/json and doctest are downloaded automatically).

On Ubuntu, install SDL's system dependencies first:

```sh
sudo apt install build-essential cmake ninja-build git libx11-dev libxext-dev libxrandr-dev \
  libxcursor-dev libxi-dev libxss-dev libxtst-dev libwayland-dev libxkbcommon-dev \
  libegl1-mesa-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev
```

Then, on Windows or Linux:

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/cave_explorer        # build\cave_explorer.exe on Windows (or run build-run.bat)
```

- **Unit tests:** `ctest --test-dir build` or `build/cave_tests`.
- **Map check:** `python tools/check_reach.py` (see *Editing maps* below).
- **Windows console:** the game has no console window; configure with `-DCAVE_WINDOWS_CONSOLE=ON`
  to get one showing log messages while developing.
- **Version number:** `project(... VERSION ...)` in `CMakeLists.txt`.

To make a package like the downloadable ones:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
cd build-release && cpack     # -> CaveExplorer-<version>-windows-x64.zip / -linux-x64.tar.gz
```

**Continuous integration** (`.github/workflows/build.yml`) builds, runs the unit tests and the map
check, and packages the game on Windows (MSVC) and Ubuntu for every push.

## Editing maps with Tiled

Every area is a [Tiled](https://www.mapeditor.org) map in `assets/maps/*.tmj` using 32x32 tiles.

1. Install Tiled (free).
2. **Open the project first:** *File → Open File or Project…* → `CaveExplorer.tiled-project` in the
   repository root. The Project panel lists the maps; double-click one to open it. The project
   teaches Tiled our object types, so properties show as fill-in fields with dropdowns (items,
   creatures, shrines, areas, music...) instead of free text.
3. Paint tiles from the `placeholder` tileset. Edit map-wide settings under *Map → Map Properties*.
4. **To add an object:** pick the rectangle tool (or the point tool for spawns, pickups, creatures,
   shrines, people and lights), draw it, then set its **Class** in the Properties panel. Its fields
   appear with their defaults; change what you need.
5. Save, rebuild (`cmake --build build` copies the maps next to the game), and play.
6. Run `python tools/check_reach.py`. It uses the game's jump, climb and swim limits to check that
   everything can be reached with every ability, that no spawn point is a trap, and that anything
   marked `gated_by` stays out of reach without that ability.

When you add items, creatures, shrines, people, shops, sprites, music or a new map, run
`python tools/make_tiled_project.py` to refresh the project's dropdown lists.

**Important:** `tools/make_maps.py` generated the starting maps. It must not be run with `--force`
anymore — that would overwrite the hand-edited maps.

### Map reference

**Map properties** (class `area`): `display_name` (shown when you enter), `music` (a track in
`assets/soundtrack/`, or `none`), `darkness` (0 = daylight, 1 = pitch black) and `restart_spawn`
(where Restart area puts you).

**Layers:** `background` (decoration), `ground` (property `collides = true`: anything solid),
`water` (see-through, `foreground = true`, drawn over the explorer) and `objects`. Any tile layer
with `foreground = true` is drawn in front of the explorer.

**Tileset tile properties:** `collision` = `solid` or `oneway`; `surface` = `slippery` (moss: only
non-slip boots grip it); `liquid` = `water`.

**Objects** (set the object's Class):

| Class      | Shape            | Fields |
|------------|------------------|--------|
| `spawn`    | point (feet)     | its *name* — doors and restart refer to it |
| `door`     | rectangle        | `target_area`, `target_spawn`, `trigger` (`touch` at area edges, `interact` = press Up), `label`, `glow` (light spilling through), `gated_by` |
| `pickup`   | point (feet)     | `item` (or `coins`), `count`, `unique_id` (never returns once taken), `gated_by` |
| `enemy`    | point            | `enemy`, `enemy_easy` / `enemy_hard` (`same`, `none` or another creature), `hp_scale`; walkers at their feet, flyers at their centre |
| `shrine`   | point (feet)     | `shrine_id`, `gated_by` |
| `npc`      | point (feet)     | `npc_id` |
| `shop`     | rectangle        | `shop_id` |
| `chest`    | rectangle (1 tile) | — the persistent storage chest |
| `obstacle` | rectangle on whole tiles | `obstacle` (`bush` needs a `cut` tool, `rock` needs `break`), `passable_from` (`left`/`right`: push through from that side) |
| `ladder`   | rectangle, 1 tile wide | — its top can be stood on |
| `light`    | point (centre)   | `radius`, `color`, `intensity`, `flicker` |
| `decor`    | rectangle        | `sprite`, `flip`, `foreground` (drawn from the bottom-left corner) |

`gated_by` is any of `cut`, `break`, `highjump`, `breath` — what the explorer needs to reach it.

## Game data

Almost everything about the game is plain JSON in `data/`:

| File | What it holds |
|------|---------------|
| `items.json` | every item: name, icon, equipment slot, stack size, value, damage, food, effects when `carried` / `equipped`, lantern fuel, glow |
| `shops.json` | shops and their stock |
| `enemies.json` | creatures: walker or flyer, health, damage, speed, drops |
| `obstacles.json` | bushes, boulders: the tool tag they need, hits, art, hint, sounds |
| `shrines.json` | shrines: the ability they grant and for how long (0 = until death) |
| `abilities.json` | abilities from shrines and quests (speed, jump, damage, defense, max health, air, light...) |
| `npcs.json`, `quests.json` | people, their shops and jobs |
| `characters.json` | names and stat ranges for newly rolled explorers |
| `sounds.json` | which sound file(s) play for each game event, and how loud |
| `animations.json`, `sprites.json` | animated sheets (knight, bat) and named sprites |
| `credits.txt` | the in-game credits page |

## Music and sound

- `assets/soundtrack/` — the music the game uses (each area picks its track). Its README lists every
  track with its artist, license and download link.
- `assets/sounds/` — sound effects (all CC0); its README credits the packs.
- `assets/music/` is ignored by git: a scratch folder for auditioning downloads.

## Saves and settings

Save slots (`slot1.json` … `slot3.json`) and `settings.json` live in the per-user data folder:
`%APPDATA%\CaveExplorer\CaveExplorer` on Windows, `~/.local/share/CaveExplorer/CaveExplorer` on
Linux. Delete a slot file to reset that world.

## Developer tools

Debug command-line options (`cave_explorer --area cave_1 --debug`, etc.):

```
--area <id> --spawn <name>   start in a given area (spawn may also be tile coords "tx,ty")
--debug                      start with the debug overlay on
--shot <file.bmp> --ticks N  run N simulation ticks (60 per second), save a screenshot, and exit
--script "Right:0-120,UseRight:130"
                             hold actions during tick ranges (automated testing)
--give <id,id,...>           start with these items (gear goes straight into empty slots)
--hunger <0-100>             start this hungry
--ui inventory|chest|shop|pause      open an in-game screen immediately
--ui slots|settings|credits|about    open a title-screen page immediately
```

`--area`, `--give` and the in-game `--ui` pages start a throwaway "dev game" that never saves.

Scripts in `tools/` (Python 3; the asset scripts need Pillow):

| Script | Does |
|--------|------|
| `check_reach.py` | checks every map: reachability, no trap spawns, gates hold |
| `make_tiled_project.py` | regenerates `CaveExplorer.tiled-project` from `data/` |
| `make_maps.py` | generated the original maps and placeholder tileset (don't use `--force` now) |
| `pack_icons.py` | packs item/creature icons from the Dungeon Crawl tile pack (list in `icons.txt`) |
| `pack_castle.py`, `pack_nature.py` | cut the knight, bat and outdoor props out of their packs |
| `make_icon.py` | builds the game icon |

## Credits and license

The code is licensed under the GNU GPL v3 (see [LICENSE](LICENSE)). Art, music and sound come from
other artists under CC0 / CC-BY licenses — see [CREDITS.md](CREDITS.md).
