# Cave Explorer Platformer — Plan

## Context
New project in an empty `platformer/` directory. Goal: a 2D platformer on SDL3 that runs on Windows
and Ubuntu. You play an explorer who starts on a beach (with a shop), walks through a small forest
(with a storage chest), and goes down into a cave complex made of hand-built areas. Gear opens up new
areas, Metroidvania-style. When a character dies, everything they carried drops where they died. The
pile stays there across runs until a later character, with newly rolled stats, picks it up. This plan
covers the design decisions so far and how to build the first playable version.

## Progress
- [x] 1. Project setup, window, fixed-step loop, input
- [x] 2. Tiled areas, player physics, camera
- [x] 3. Beach → Forest → Cave 1 → Cave 2 with doors
- [x] 4. Items, inventory (9 + 5 equipment slots), pickups, persistent chest, beach shop, coins
- [x] Music: looping per-area tracks with crossfade (forest done; deep_cave.ogg reserved for a deep area)
- [x] 5. Combat: melee per hand, sling + stones, bats (flyers), rats and beetles (walkers), drops,
       per-difficulty enemy placement; temporary knight player sprite
- [x] 6. Death loop: corpse piles (searchable, persistent), new explorer with rolled stats
- [x] 9 (part). Title screen, 3 save slots, pause menu (save, settings, credits, about, quit),
       settings (music/sound volume, fullscreen, difficulty, controls list), autosaves
- [x] 7. Obstacles: thorny bush (cut: machete/hand axe) gates Mossy Hollow; boulder (break:
       pickaxe) guards a treasure alcove. Machete moved to the Upper Caves; shop sells hand axe
       and pickaxe as fallbacks. Fixed the forest cave mouth (it was walled off).
- [x] 7b. Item effects (carried / worn), lasting abilities, slick moss + non-slip boots, Old Hob
       (trader, two quests: kill 3 rats -> non-slip boots; bring 3 bones -> Miner's Grit)
- [x] Quality: Restart area (Esc menu), push through obstacles from the far side, visible cave
       doorways, control rebinding (keys, mouse, gamepad) with closer defaults, clearer hints
- [x] 8. Shrines (Leaping, Swiftness, Might, Warding, Shadows: timed; Vigor: until death) placed
       in both caves; hunger with healing gate and starving penalties; food fills hunger
- [x] Music: beach (HitCtrl), Upper Caves (Spring Spring); boss_3 reserved
- [x] 9. Sound effects (32 events, CC0 packs, data/sounds.json), sound volume setting
- [x] Ladders; level fixes so everything is reachable (tools/check_reach.py); Shift quick move
- [x] 10. Packaging: CPack zip (Windows) / tar.gz (Linux) with bundled SDL libraries, game icon,
       no console window on Windows; GitHub Actions builds, tests and packages both on every push
- [x] Light and darkness: per-area darkness, light map with coloured glows (exits, shrines,
       moss, crystals, Hob's lamp), lantern + lamp oil fuel, Cave brightness setting
- [x] Restart area returns to each area's surface-side entrance (map property restart_spawn)

## Art sources
- Item icons: Dungeon Crawl 32x32 tiles (CC0).
- Outdoor props (tree, chest, fence, signs...): Nature tileset by Paulina Riva (CC-BY 3.0, credit required).
- Candidate for later: "castle platformer" by Jetrel (CC0) has an animated bat enemy, spikes
  and cave backgrounds; its knight could be a temporary animated player.
- Still needed: animated side-view explorer, side-view ground/ledge tiles for beach, forest and caves.

## Decisions made
- **Tech:** C++20, SDL3 3.4 (+ SDL3_image, SDL3_mixer, later SDL3_ttf), CMake. Builds on Windows and Ubuntu.
- **World:** hand-built areas made in the **Tiled** editor (.tmj JSON files). Only one area is loaded
  at a time.
- **Area refresh:** every time an area loads, **everything in it resets** (enemies, loot, obstacles,
  shrines) as if it had never been visited. Only **corpse piles** and the **storage chest** survive a
  reload.
- **What carries over between characters:** corpse loot and the forest chest. Nothing else.
- **Art grid:** the world is 32x32 tiles. Most characters, including the player, fit in 32x32. Bigger
  creatures and art come in multiples of 32 (32x64, 64x64, 128x128...). Projectiles are 16x16.
- **Movement feel:** smooth movement, never snapped to the grid. Positions and velocities use floats.
  Collision boxes are set separately from sprite size, so the player can stand on the edge of a tile.
  Includes coyote time (a short window to still jump after walking off a ledge), jump buffering (an
  early jump press still counts when you land), and variable jump height.
- **Art and audio:** free asset packs (CC0 or similar licenses) when they fit. Otherwise the user
  generates assets with an AI image tool. Rectangles stand in until real art exists.
- **Combat:** mostly melee (swinging a weapon); ranged combat gets deeper as the player finds new
  weapons and abilities. Each hand has its own attack button (see "Hands and combat" below).
- **Inventory:** 9 backpack slots + 5 equipment slots (see below). The forest chest stores items
  between characters.
- **Difficulty:** each enemy placed in Tiled has per-difficulty properties. It can become a different
  enemy type, have stronger or weaker stats, or not appear at all on easier settings.
- **First milestone:** the vertical slice described below.

## Architecture
```
platformer/
  CMakeLists.txt            # FetchContent: SDL3, SDL3_image, SDL3_mixer, SDL3_ttf, nlohmann_json
  src/
    main.cpp
    core/    App (fixed 60 Hz update, rendering decoupled), Input (actions, keyboard+gamepad,
             rebinding), Assets (textures/sounds/fonts cache), Settings, Audio
    world/   Area (loads .tmj: tile layers + object layer), Tilemap, Collision (AABB vs tiles,
             one-way platforms, axis-separated resolution), Camera (follows player, kept inside the area)
    entity/  Entity base (pos, vel, hitbox, sprite), Player, Enemy (+ simple AI states),
             Projectile, Pickup, Obstacle (breakable / cuttable), Door/Transition, Chest,
             Shrine, CorpsePile, ShopKeeper
    game/    Item defs, Inventory, Character (randomly rolled stats), Buffs (timed / until-death),
             Combat (melee swing hitbox with active frames, knockback, i-frames), Difficulty
    ui/      Menu system (main, pause, settings: audio/music/controls/difficulty/display),
             HUD, inventory/chest/shop screens
    save/    Persistent world save (JSON in SDL_GetPrefPath): corpse piles, chest contents,
             character counter; settings.json
  data/      items.json, enemies.json, shop.json, npcs.json, quests.json, characters.json
  assets/    maps/*.tmj + tilesets, sprites/, audio/, fonts/
```
Key mechanisms:
- **Rendering:** a 640x360 logical resolution (20x11.25 tiles on screen), scaled up with SDL3's
  integer logical presentation. Windowed, borderless and fullscreen modes, plus a vsync toggle.
- **Placing objects in Tiled:** objects carry a `type` (enemy, door, obstacle, shrine, pickup, chest,
  shop) plus custom properties, e.g. `enemy_easy=none`, `enemy_normal=bat`, `enemy_hard=giant_bat`,
  `hp_scale`, `requires=machete`, `target_area=cave_1`, `target_spawn=from_beach`.
- **Area changes:** the current Area is destroyed, the target .tmj is loaded fresh, and saved corpse
  piles for that area are added. The player appears at the named spawn point.
- **Death:** inventory and equipped items become a CorpsePile record {area, x, y, items}. The world
  save is written, then a new Character is rolled from characters.json and starts on the beach.

## Vertical slice (first playable) — build order
1. Set up CMake with dependencies fetched automatically. Window, fixed-timestep loop, input actions,
   logical resolution. Builds on Windows (MSVC) and Ubuntu (GCC).
2. Load areas from Tiled and draw them. Player physics with float movement and AABB collision. Camera.
3. Three areas: Beach → Forest → Cave 1 → Cave 2, connected by doors, refreshed on every load.
4. Items, limited inventory, pickups, the forest chest (persistent), and a shop on the beach with
   currency.
5. Combat: melee swing (right hand) and a projectile weapon (left hand), one button per hand. 1–2 enemy types, health,
   hurt and death.
6. Death loop: drop a corpse pile, roll a new character, recover the pile on a later run.
7. One gated obstacle (a bush the machete cuts, or a rock a pick breaks) blocking Cave 2.
7b. Inventory UI: 9 backpack slots + 5 equipment slots, abilities from carried items (e.g. non-slip
   boots), and the stat rebuild described above. One cave NPC who trades and gives one fetch/kill quest.
8. One shrine giving a timed buff (e.g. a higher jump). Hunger meter with slow natural healing
   that stops when hunger is low, and a speed/jump penalty when starving.
9. Menus: main, pause, and settings (sound and music volume, control rebinding, difficulty, display).
   Saving and loading of settings and the world.
10. Packaging: a Windows zip with DLLs and a Linux tarball (an AppImage later).

## Verification
- Build and run on Windows and Ubuntu from a clean checkout (`cmake -B build && cmake --build build`).
- Manual playthrough checklist:
  - The player can stand on a tile edge, and movement feels smooth.
  - Doors load areas and enemies come back on re-entry.
  - Chest contents survive both death and restarting the game.
  - Dying drops a pile that a new character (with different stats) can recover.
  - The gated obstacle needs the right tool and comes back after the area reloads.
  - The shrine buff wears off when its timer runs out.
  - Difficulty swaps or removes enemies.
  - Settings persist after a restart.
- Unit tests (doctest) for collision resolution, inventory, and save serialization.

## Additional decisions
- **Unique items stay taken:** a unique item (e.g. the cave machete) is tagged `unique_id` in Tiled.
  Once picked up, its ID goes into the world save, and from then on it exists only on a character, in
  a corpse pile, or in the chest. Refreshing an area never brings it back. (If an item can get lost
  for good, we'll add a fallback way to get it again.)
- **Currency:** coins dropped by enemies or found in loot spots, plus selling loot (gems, ore,
  creature parts) at the beach shop.
- **Survival systems:**
  - **Hunger:** drains over time and is refilled by eating food. The player slowly heals naturally,
    but healing stops when hunger is low. Below a further threshold, run speed and jump height drop.
    This goes into the vertical slice (it's cheap to build and tied to the shop's food).
  - **Light and darkness:** torches or lanterns that burn fuel. Deeper areas are dark. Built after the
    slice, using a darkness overlay texture with light shapes cut out of it.
  - **Oxygen and water:** flooded passages, swimming physics, an air meter. Built after the slice.
- **Inventory layout:** 9 backpack slots plus 5 equipment slots: **Right hand**, **Left hand**,
  **Footwear**, **Clothing**, **Headgear**.
  - Hands hold weapons, torches or a flashlight.
  - Footwear: shoes, non-slip boots...
  - Clothing: cold-weather gear, wetsuit...
  - Headgear: hat, headlamp, baseball cap, wizard hat...
  - Each piece of gear can grant abilities or resistances.
- **Abilities granted by items:** some items give an ability simply by being carried (backpack or
  equipped). The ability is gone the moment the item isn't carried. This is the main source of
  tension over the limited inventory space. In code, the player's stats and abilities are rebuilt each
  time the inventory changes, from: the character's rolled stats + equipped gear + abilities from
  carried items + active buffs (timed and until-death) + hunger penalties.
- **Cave NPCs:** some trade (buy and sell with their own stock and prices). Some give simple quests
  (kill creature X, bring item Y, maybe reach place Z). Quest rewards are an item or an ability.
  - NPCs, their stock and their quests live in `data/npcs.json` and `data/quests.json`.
  - NPCs are placed in Tiled with an `npc_id`.
  - A simple dialogue box UI (talk / trade / quest).
  - Quest objectives are tracked by listening for game events (EnemyKilled, ItemDelivered,
    AreaEntered).
- **Hands and combat:** each hand has its own action button (right = Action A, left = Action B). A
  sword in the right hand and a sling in the left gives melee and ranged attacks. A torch or flashlight
  in one hand gives up that hand's attack, which is a deliberate trade-off. Switching weapons means
  swapping items between the backpack and a hand. (This replaces the earlier melee/projectile
  "switch".)
- **Quests and death:** quest progress and abilities earned from quests die with the character. The
  next explorer can do the quests again. Reward *items* end up in the corpse pile like everything
  else.
- **NPC memory:** while a character lives, NPC quest state and stock changes are remembered, even
  though the rest of the area refreshes. All of it is cleared when the character dies. Stored in a
  per-character `RunState` that sits next to the persistent `WorldSave`.
- **Boss arenas:** bosses live in their own small areas (mini-areas just for the fight), each with
  its own music. `boss_1.ogg` ("Gloomey") and `boss_2.ogg` ("Infection By Chewing"), both by Ove Melaa,
  are reserved for the first two bosses; `boss_3.mp3`
  ("JRPG End Dungeon", HydroGene, CC0) for a third.
- **Underground city:** a deep, inhabited area (traders, quest givers). Music: `underground_city.mp3`
  ("Hero Within", Ove Melaa).
- **Freebies don't respawn:** placed coins/items, once taken, stay gone for the rest of that
  explorer's life (tracked per explorer in the save); they return for the next explorer. Creatures,
  obstacles and enemy drops still refresh whenever an area loads. This stops beach/forest farming.
- **Never stuck:** obstacles can be pushed through from their far side; "Restart area" in the
  Esc menu reloads the area at the doorway you entered by (keeping health/items/quests); falling
  out of a level returns you to the entrance with a little damage.
- **Save slots:** three slots, each a separate world (chest, corpse piles, unique items taken,
  explorers lost) plus the current explorer and their position. Starting a new game in a slot
  wipes it. Autosaves on area change, chest/corpse close, unique pickup, death and quitting.
- Still to decide while building: rules for rolling character stats, shop prices, the enemy roster.
