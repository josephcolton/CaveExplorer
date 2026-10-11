# Cave Explorer — Design

You play an explorer who starts on a beach (with a shop), walks through a forest (with a storage
chest) and goes down into a cave complex made of hand-built areas. Gear opens up new areas,
Metroidvania-style, so you keep coming back for more. When an explorer dies, everything they carried
stays where they fell, across runs, until a later explorer — with newly rolled stats — picks it up.

## Status (version 0.3.0)

Built and playable:

- **Engine:** SDL3 (with SDL3_image and SDL3_mixer), fixed 60 Hz simulation with interpolated
  rendering at 640x360, scaled up in whole steps. Windows and Linux builds, packaged by CPack, built
  and tested by GitHub Actions on every push.
- **Movement:** smooth, sub-pixel movement (never snapped to the grid); coyote time, jump buffering,
  variable jump height; one-way wooden ledges (Down + Jump drops through); ladders; slick moss;
  swimming with air and drowning.
- **World:** 7 areas — Beach, Forest, and the Upper Caves hub with four gated exits (Mossy Hollow,
  Drowned Tunnel / Sunken Grotto, Bat Roost, Crystal Mine). Doors with fades, per-area music with
  crossfades, darkness and lights.
- **Items:** 9 backpack slots + right hand, left hand, feet, body and head. Items can have effects
  while carried or while worn/held. Pickups, the persistent forest chest, the beach shop and Old Hob's
  shop, coins, selling loot.
- **Combat:** each hand has its own button; melee swings with reach and cooldown; slings fire stones.
  Creatures: bats (fly, swoop), rats and beetles (walk, chase, avoid ledges), giant bats on Hard.
- **Death loop:** corpse piles, new explorers with rolled stats, three save slots, autosave.
- **Gates:** thorny bushes (cut), boulders (break), high ledges (Feather Charm / Leaping shrine), a
  long flooded tunnel (Amulet of Deep Breathing / Deep Breath shrine).
- **Survival:** health with slow healing, hunger, lantern + lamp oil, air underwater.
- **Shrines:** Leaping, Swiftness, Might, Warding, Shadows, Deep Breath (timed); Vigor (until death).
- **People and quests:** Old Hob trades and offers two jobs (kill 3 rats → non-slip boots; bring 3
  bones → Miner's Grit).
- **Menus:** title, save slots, pause (resume, restart area, save, settings, credits, about, quit),
  settings (music/sound volume, fullscreen, cave brightness, difficulty, control rebinding).
- **Sound:** 32 sound effects; 4 area tracks plus 5 reserved for future areas.
- **Tools:** Tiled project with our object types; map checker (reachability, trap spawns, gates);
  scripted input and screenshots for automated testing; unit tests for the game logic.

## Design decisions

### World and progression
- **Hand-built areas** made in Tiled. Only one area is loaded at a time.
- **The Upper Caves are the hub:** new areas branch off it, each needing something new, so players
  keep coming back. Gates are verified by `tools/check_reach.py`, which removes one ability at a time
  and checks that gated places stay out of reach (judged generously, for the luckiest explorer).
- **Area refresh:** creatures, obstacles and shrines come back every time an area loads.
- **Freebies don't respawn:** coins and items placed in the world, once taken, stay gone for the rest
  of that explorer's life; they return for the next explorer (after a death). This stops farming by
  walking back and forth.
- **Unique items** (machete, charms, the hat...) never come back once taken. They exist only on an
  explorer, in a corpse pile or in the chest. If one could be lost for good, there must be a fallback
  (the shop sells a hand axe and a pickaxe for this reason).
- **Never stuck:** obstacles can be pushed through from their far side; Restart area returns you to
  the area's surface-side entrance; falling out of a level returns you to the entrance.

### Explorers and death
- Each explorer has rolled stats (health, strength, speed, jump) and a random name.
- On death, everything carried (and coins) becomes a corpse pile where they fell. A new explorer
  arrives at the beach.
- **What carries over between explorers:** corpse piles, the forest chest, unique items already taken,
  and the count of explorers lost. Quest progress, abilities, hunger and lamp oil die with the explorer.

### Items and abilities
- 9 backpack slots + 5 equipment slots (two hands, feet, body, head). Limited space is a deliberate
  tension: some items only help while carried.
- The explorer's stats are rebuilt every tick from: rolled stats × gear (carried + equipped) ×
  abilities (quests, shrines) × hunger penalties.
- **Hands:** a weapon in each hand gives two attacks; a lantern or shield in a hand gives up that
  hand's attack.

### Survival
- **Health** heals slowly when you're out of danger and not hungry.
- **Hunger** lasts about 15 minutes; Hungry stops healing, Starving slows you. (Starving does no
  damage — a possible future change.)
- **Light:** caves have a darkness level; lights add coloured glows to a light map multiplied over the
  world. The lantern burns lamp oil only in dark places.
- **Water:** buoyant; 10 s of air, then drowning at 25 health per second.

### Combat and difficulty
- Mostly melee; ranged weapons need ammo from the backpack.
- Difficulty is set per creature placement in Tiled: a creature can be swapped for another or removed
  on Easy/Hard, and creatures get more or less health; damage taken also scales.

### Saves
- Three slots; each is a whole world plus the current explorer and where they are.
- Autosave on area change, chest/corpse close, unique pickup, death and quitting.

### Art and audio
- 32x32 tiles; most characters fit in 32x32, bigger art in multiples of 32, projectiles 16x16.
- Free CC0 / CC-BY packs when they fit (credited in `CREDITS.md`), otherwise AI-generated art from the
  user. Current art is a mix of placeholder tiles, Dungeon Crawl icons, the Nature tileset props and
  Jetrel's knight (a temporary player sprite).
- Music per area; tracks are kept in `assets/soundtrack/` with credits in its README.

## Architecture

```
src/
  main.cpp            command-line options
  core/               App (window, fixed-step loop, scenes, scripted input, screenshots),
                      Input (actions, bindings, rebinding), Audio (music + sound effects),
                      Assets, Settings, Sprites/Animation
  world/              Area (loads .tmj maps), Tilemap (tiles, collision, water, slippery, runtime
                      blocking), Collision (AABB vs tiles), Camera, Lighting (light map)
  entity/             Player (movement, ladders, swimming, combat state), Enemy (walker/flyer AI),
                      Pickup, Projectile, Obstacle
  game/               TitleScene, GameplayScene (the game: areas, combat, interactions, survival,
                      dialogue, HUD), GameData (loads data/), SaveSlots
                      -- plain game logic, no SDL, unit-tested: --
                      Items, Inventory, Modifiers, Quests, Character, WorldSave, SaveGame, Hunger
  ui/                 Ui helpers, Menus (menu, settings, controls, text pages), InventoryScreen,
                      ShopScreen, DialogueScreen
data/                 game content as JSON
assets/               maps (Tiled), tiles, sprites, soundtrack, sounds
tools/                map checker, Tiled project generator, asset packers
tests/                doctest unit tests for the game logic
```

Key mechanisms:
- **Areas load fresh** from their `.tmj` file every time; map objects (spawns, doors, pickups,
  creatures, shrines, people, obstacles, ladders, lights, decor) are interpreted by `GameplayScene`.
- **Rendering:** world → lights (multiplied light map) → prompts and air bar → HUD → menus → fade.
- **Data-driven:** items, creatures, obstacles, shrines, abilities, people, quests, shops, sounds and
  animations are JSON; maps are Tiled files using the classes in `CaveExplorer.tiled-project`.

## Roadmap and ideas

Planned:
- **Deeper areas** beyond the hub, with new gates (e.g. cold: the Warm Cloak; non-slip boots on ice).
- **Underground city:** a deep, inhabited area with traders and quest givers. Music:
  `underground_city.mp3` ("Hero Within", Ove Melaa).
- **Boss arenas:** bosses in their own small areas, each with its own music — `boss_1.ogg`
  ("Gloomey"), `boss_2.ogg` ("Infection By Chewing"), `boss_3.mp3` ("JRPG End Dungeon").
- **Deep cave music:** `deep_cave.ogg` currently plays in the Drowned Tunnel; it was chosen for a deep
  cave area and can move there.
- **Move the Amulet of Deep Breathing** from Mossy Hollow to a deeper area once one exists.
- **Real art:** an animated side-view explorer (the knight is temporary) and side-view ground/ledge
  tiles for the beach, forest and caves.

Ideas not yet decided:
- Starving that slowly hurts; torches as a cheap light; arrows for the bow (it has no ammo yet).
- More creatures and people; more quests; a map screen.
- Rules for rolling explorer stats, shop prices and the enemy roster are still open to tuning.
