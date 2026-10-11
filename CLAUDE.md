# CLAUDE.md

Guidance for Claude (or any AI assistant) working in this repository. Read the README for what the
game is and how to play/build it, and `docs/DESIGN.md` for the design decisions and roadmap.

## Project in one paragraph

Cave Explorer is a 2D cave-exploring platformer in C++20 on SDL3 (+ SDL3_image, SDL3_mixer), built
with CMake, for Windows and Linux. Areas are Tiled maps (`assets/maps/*.tmj`); almost all content
(items, creatures, shrines, people, quests, sounds...) is JSON in `data/`. The Upper Caves are a hub
with exits gated by abilities (cut, break, highjump, breath). Explorers die permanently and leave
their belongings behind; save slots keep the world.

## Build, run, test

```sh
cmake -S . -B build -G Ninja && cmake --build build     # build (copies assets/ and data/ next to the exe)
build/cave_explorer                                      # run (cave_explorer.exe on Windows)
build/cave_tests                                         # unit tests (doctest), or: ctest --test-dir build
python tools/check_reach.py                              # map check: reachability, trap spawns, gates
```

- On the user's Windows machine the toolchain is MinGW GCC 13.2 (Strawberry Perl) + Ninja + CMake 3.29.
- **If the game is running, `build/cave_explorer.exe` is locked** and linking fails ("Permission
  denied"). Don't kill the user's game: build into a separate directory instead, e.g.
  `cmake -S . -B build-test -DCAVE_WINDOWS_CONSOLE=ON` (`build-*/` is git-ignored). The console
  option keeps log output visible.
- CI (`.github/workflows/build.yml`) builds with MSVC on Windows and GCC on Ubuntu, runs the unit
  tests and the map check, and packages with CPack. The repo is public, so run status can be read
  from `https://api.github.com/repos/josephcolton/CaveExplorer/actions/runs` without `gh` (job logs
  need sign-in; the workflow turns configure errors into public annotations).

## Verifying changes in the game

You can't play the game, so verify with the debug options (see README → Developer tools):

- `--area <id> --spawn <name|tx,ty>` starts a throwaway dev game (never saves) where you want.
- `--script "Right:10-60,Jump:20,UseRight:80"` holds actions during tick ranges (60 ticks/s).
- `--shot out.bmp --ticks N` saves a screenshot after N ticks and exits; `--debug` shows positions
  and hitboxes; `--give id,id` starts with items; `--ui inventory|chest|shop|pause|slots|...`.
- Look at the screenshots (convert BMP→PNG with Pillow) before claiming something works. Scripted
  runs have caught real bugs here (unreachable areas, swimming getting stuck, lights left behind).
- **Never run the real save-slot flow on the user's slots** — the title-screen slot list is
  read-only and safe to screenshot; anything that starts a game in a slot writes to their saves.

## Rules for this project

- **Maps are hand-edited in Tiled now. Never run `tools/make_maps.py --force`** — it overwrites
  every map. Change maps by editing the `.tmj` JSON directly (check `git diff assets/maps` for the
  user's edits first). Keep property types consistent with the Tiled project: enum properties carry
  `"propertytype"` (see `ENUM_PROPS` in `tools/make_tiled_project.py`), colours are type `color` as
  `#aarrggbb`, decimals are type `float`. Run `tools/check_reach.py` after any map change.
- **New content in `data/`** (items, creatures, shrines, people, shops, sprites, music, maps):
  re-run `python tools/make_tiled_project.py` so Tiled's dropdowns include it.
- **Gated content:** mark objects with `gated_by` so the checker proves the gate holds.
- **Game logic without SDL** (items, inventory, modifiers, quests, saves, hunger) lives in the
  `cave_logic` library and has unit tests in `tests/`. Keep it SDL-free and add tests when changing it.
- **Art, music and sounds need clear licenses** (CC0 / CC-BY). Credit every third-party asset in
  `CREDITS.md`, the in-game `data/credits.txt`, and the README in `assets/soundtrack/` or
  `assets/sounds/`. Kept music goes in `assets/soundtrack/` under a descriptive name.
- **`assets/music/` is the user's scratch folder** for auditioning downloads; it's git-ignored.
  Copy chosen tracks out of it — don't move or delete the user's files.
- Match the surrounding code style: C++20, 4-space indent, `camelCase_` members, short comments that
  explain *why*, player-facing text in plain language.

## Git

- Commit and push to `main` at stable points (the user has asked for this). Review the staged file
  list **before** committing — `git add -A` has previously swept in a 96 MB music folder and a
  Python cache. `build-run.bat` belongs in the repo (the user's helper).
- End commit messages with the `Co-Authored-By` line given by the environment.
- Line-ending warnings (CRLF → LF) on commit are expected and harmless (`.gitattributes`).

## Where things are

| Path | What |
|------|------|
| `src/core/` | App (loop, scenes, scripted input, screenshots), Input, Audio, Assets, Settings, Sprites/Animation |
| `src/world/` | Area (map loading), Tilemap, Collision, Camera, Lighting |
| `src/entity/` | Player (movement, ladders, swimming), Enemy, Pickup, Projectile, Obstacle |
| `src/game/` | TitleScene, GameplayScene (most gameplay), GameData, SaveSlots; plain logic: Items, Inventory, Modifiers, Quests, Character, WorldSave, SaveGame, Hunger |
| `src/ui/` | menus, settings/controls, inventory, shop, dialogue |
| `data/` | JSON content |
| `assets/` | maps, tiles, sprites, soundtrack, sounds |
| `tools/` | check_reach, make_tiled_project, make_maps (bootstrap only), asset packers |
| `docs/DESIGN.md` | design decisions, status, roadmap |
