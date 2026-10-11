#!/usr/bin/env python3
"""Level sanity check: can the explorer get everywhere they should - and nowhere they shouldn't?

For every map in assets/maps/ this works out which tiles an explorer can reach (walking, falling,
jumping, dropping through ledges, climbing ladders, swimming while their air lasts) and checks:

1. With every ability, everything is reachable and no spawn point is a trap (each can reach a door).
2. Gates hold: map objects marked `gated_by` (e.g. "highjump", "breath", "cut", "break") must be
   unreachable when the explorer lacks that ability - judged generously, so a lucky explorer with
   the best rolled stats still can't sneak in.

Abilities:
  cut       can clear bushes (machete, hand axe)          -> bush obstacles open
  break     can clear boulders (pickaxe)                   -> rock obstacles open
  highjump  Feather Charm / Leaping shrine                 -> jumps 4 tiles high instead of 2-3
  breath    Amulet of Deep Breathing / Deep Breath shrine  -> much longer underwater

    python tools/check_reach.py            # all maps
    python tools/check_reach.py cave_1     # one map
"""
import json
import sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAPS = ROOT / "assets" / "maps"
T = 32
ABILITIES = ("cut", "break", "highjump", "breath")


class Model:
    """How far the explorer can jump / swim. 'generous' = the best a lucky explorer could do."""

    def __init__(self, abilities, generous):
        self.abilities = set(abilities)
        if "highjump" in abilities:
            self.max_up = 5 if generous else 4
        else:
            self.max_up = 3 if generous else 2  # the best natural jump just clears 3 tiles
        self.max_across = 5 if generous else 4
        # Underwater tile moves before the explorer drowns (air plus the time drowning takes).
        if "breath" in abilities:
            self.air = 999 if generous else 75
        else:
            self.air = 50 if generous else 20


class Level:
    def __init__(self, path, obstacle_defs):
        doc = json.loads(path.read_text())
        self.name = path.stem
        self.w, self.h = doc["width"], doc["height"]
        coll, liquid = {}, set()
        for ts in doc["tilesets"]:
            for tile in ts.get("tiles", []):
                for p in tile.get("properties", []):
                    if p["name"] == "collision":
                        coll[ts["firstgid"] + tile["id"]] = p["value"]
                    if p["name"] == "liquid":
                        liquid.add(ts["firstgid"] + tile["id"])
        self.base = [["none"] * self.w for _ in range(self.h)]
        self.water = set()
        self.objects = []
        for layer in doc["layers"]:
            if layer["type"] == "tilelayer":
                collides = any(p["name"] == "collides" and p["value"] for p in layer.get("properties", []))
                for i, gid in enumerate(layer["data"]):
                    gid &= 0x0FFFFFFF
                    if collides and gid in coll:
                        self.base[i // self.w][i % self.w] = coll[gid]
                    if gid in liquid:
                        self.water.add((i % self.w, i // self.w))
            elif layer["type"] == "objectgroup":
                self.objects = layer["objects"]
        self.ladders = set()
        self.obstacles = []  # (cells, required ability)
        for o in self.objects:
            if o.get("type") == "ladder":
                x = int(o["x"] // T)
                top = int(o["y"] // T)
                for y in range(top, int((o["y"] + o["height"]) // T)):
                    self.ladders.add((x, y))
                if self.base[top][x] == "none":
                    self.base[top][x] = "oneway"  # the top of a ladder can be stood on
            elif o.get("type") == "obstacle":
                kind = prop(o, "obstacle", o.get("name"))
                need = obstacle_defs.get(kind, {}).get("requires", "")
                self.obstacles.append((object_cells(o, point_area=False), need))

    def use(self, model):
        self.model = model
        self.grid = [row[:] for row in self.base]
        for cells, need in self.obstacles:
            if need not in model.abilities:
                for x, y in cells:
                    self.grid[y][x] = "solid"

    def at(self, x, y):
        if x < 0 or x >= self.w or y >= self.h:
            return "solid"
        if y < 0:
            return "none"
        return self.grid[y][x]

    def open(self, x, y):
        return self.at(x, y) != "solid"

    def submerged(self, x, y):
        """Head underwater: water here, and water or rock just above."""
        return (x, y) in self.water and ((x, y - 1) in self.water or self.at(x, y - 1) == "solid")

    def fall(self, x, y, through_oneway=False):
        first = True
        while y < self.h:
            below = self.at(x, y + 1)
            if (x, y) in self.ladders or (x, y) in self.water:
                return (x, y)
            if below == "solid" or (below == "oneway" and not (through_oneway and first)):
                return (x, y) if self.open(x, y) else None
            y += 1
            first = False
        return None

    def neighbours(self, x, y):
        m = self.model
        out = []
        if (x, y) in self.water:
            # Swim in any direction; surface swimmers can also jump out (handled below).
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if self.open(nx, ny):
                    land = (nx, ny) if (nx, ny) in self.water else self.fall(nx, ny)
                    if land:
                        out.append(land)
            if self.submerged(x, y):
                return out
        for dx in (-1, 1):
            if self.open(x + dx, y):
                land = self.fall(x + dx, y)
                if land:
                    out.append(land)
        if self.at(x, y + 1) == "oneway":
            land = self.fall(x, y + 1)
            if land:
                out.append(land)
        if (x, y) in self.ladders or (x, y + 1) in self.ladders:
            for dy in (-1, 1):
                if (x, y + dy) in self.ladders and self.open(x, y + dy):
                    out.append((x, y + dy))
        for up in range(0, m.max_up + 1):
            if any(self.at(x, y - k) == "solid" for k in range(1, up + 1)):
                break
            top = y - up
            for dx in range(-m.max_across, m.max_across + 1):
                if dx == 0 and up == 0:
                    continue
                step = 1 if dx > 0 else -1
                if not all(self.open(x + step * k, top) for k in range(1, abs(dx) + 1)):
                    continue
                land = self.fall(x + dx, top)
                if land:
                    out.append(land)
        return out

    def reachable(self, start):
        """Cells reachable from start. Underwater moves use up air; surfacing refills it."""
        air0 = self.model.air
        best = {start: air0}
        queue = deque([(start, air0)])
        while queue:
            (x, y), air = queue.popleft()
            if best.get((x, y), -1) > air:
                continue
            for n in self.neighbours(x, y):
                a = air - 1 if self.submerged(*n) else air0
                if a < 0:
                    continue
                if best.get(n, -1) < a:
                    best[n] = a
                    queue.append((n, a))
        return set(best)


def prop(o, name, default=""):
    return next((p["value"] for p in o.get("properties", []) if p["name"] == name), default)


def object_cells(o, point_area=True):
    x0, y0 = o["x"], o["y"]
    w, h = o.get("width", 0), o.get("height", 0)
    if o.get("point") or (w == 0 and h == 0):
        tx, ty = int(x0 // T), int((y0 - 1) // T)
        return {(tx + dx, ty + dy) for dx in (-1, 0, 1) for dy in (0, -1)} if point_area else {(tx, ty)}
    return {(x, y) for x in range(int(x0 // T), int((x0 + w - 1) // T) + 1)
            for y in range(int(y0 // T), int((y0 + h - 1) // T) + 1)}


def spawn_start(lvl, sp):
    return lvl.fall(int(sp["x"] // T), int((sp["y"] - 1) // T))


def describe(o):
    return f"{o.get('type')} '{o.get('name')}' at tile ({int(o['x'] // T)}, {int(o['y'] // T)})"


def check(path, obstacle_defs):
    lvl = Level(path, obstacle_defs)
    problems = []
    spawns = [o for o in lvl.objects if o.get("type") == "spawn"]
    doors = [o for o in lvl.objects if o.get("type") == "door"]
    targets = [o for o in lvl.objects if o.get("type") in ("pickup", "shrine", "npc", "chest", "shop", "door")]

    # 1. Everything reachable with every ability; no spawn is a trap.
    lvl.use(Model(ABILITIES, generous=False))
    reach_all = set()
    for sp in spawns:
        start = spawn_start(lvl, sp)
        if not start:
            problems.append(f"spawn '{sp['name']}' isn't on solid ground")
            continue
        reach = lvl.reachable(start)
        reach_all |= reach
        if doors and not any(object_cells(d) & reach for d in doors):
            problems.append(f"STUCK: from spawn '{sp['name']}' no door can be reached")
    for o in targets:
        if not object_cells(o) & reach_all:
            problems.append(f"unreachable even with every ability: {describe(o)}")

    # 2. Each gate holds: without that ability (but with all others), gated things stay out of reach.
    main = next((s for s in spawns if s["name"] == prop_map(path, "restart_spawn")), spawns[0] if spawns else None)
    for gate in ABILITIES:
        lvl.use(Model([a for a in ABILITIES if a != gate], generous=True))
        start = spawn_start(lvl, main) if main else None
        reach = lvl.reachable(start) if start else set()
        for o in targets:
            gates = [g.strip() for g in str(prop(o, "gated_by")).split(",") if g.strip()]
            if gate in gates and object_cells(o) & reach:
                problems.append(f"GATE LEAK: {describe(o)} is reachable without '{gate}'")
    return lvl.name, problems


def prop_map(path, name):
    doc = json.loads(path.read_text())
    return next((p["value"] for p in doc.get("properties", []) if p["name"] == name), "")


def main():
    obstacle_defs = json.loads((ROOT / "data" / "obstacles.json").read_text()).get("obstacles", {})
    names = sys.argv[1:]
    paths = [MAPS / f"{n}.tmj" for n in names] if names else sorted(MAPS.glob("*.tmj"))
    bad = 0
    for p in paths:
        name, problems = check(p, obstacle_defs)
        gated = sum(1 for o in Level(p, obstacle_defs).objects if prop(o, "gated_by"))
        print(f"{name}: {'ok' if not problems else f'{len(problems)} problem(s)'}"
              + (f"  ({gated} gated object(s) checked)" if gated else ""))
        for msg in problems:
            print(f"   - {msg}")
        bad += len(problems)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
