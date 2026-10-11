#!/usr/bin/env python3
"""Level sanity check: can the explorer actually get everywhere?

For every map in assets/maps/, starting from every spawn point, works out which tiles a
basic explorer can stand on (walking, falling, jumping, dropping through ledges, climbing
ladders) with no gear or shrine blessings. Reports:
  - objects (pickups, shrines, people, doors, chests, shops) that can't be reached, and
  - spawn points you can't get back to the area's exits from (you'd be stuck).

    python tools/check_reach.py            # all maps
    python tools/check_reach.py cave_1     # one map

The movement model is a conservative, tile-based approximation of the game's physics:
jumps up to MAX_UP tiles high and MAX_ACROSS tiles across. Obstacles count as passable,
since they can always be cleared or pushed through.
"""
import json
import sys
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAPS = ROOT / "assets" / "maps"
T = 32
MAX_UP = 2       # the explorer jumps ~2.8 tiles; 2 is a comfortable margin
MAX_ACROSS = 4   # tiles covered by a running jump


class Level:
    def __init__(self, path):
        doc = json.loads(path.read_text())
        self.name = path.stem
        self.w, self.h = doc["width"], doc["height"]
        coll = {}
        liquid = set()
        for ts in doc["tilesets"]:
            for tile in ts.get("tiles", []):
                for p in tile.get("properties", []):
                    if p["name"] == "collision":
                        coll[ts["firstgid"] + tile["id"]] = p["value"]
                    if p["name"] == "liquid":
                        liquid.add(ts["firstgid"] + tile["id"])
        self.grid = [["none"] * self.w for _ in range(self.h)]
        self.water = set()
        self.objects = []
        for layer in doc["layers"]:
            if layer["type"] == "tilelayer" and any(
                    p["name"] == "collides" and p["value"] for p in layer.get("properties", [])):
                for i, gid in enumerate(layer["data"]):
                    c = coll.get(gid & 0x0FFFFFFF)
                    if c:
                        self.grid[i // self.w][i % self.w] = c
            if layer["type"] == "tilelayer":
                for i, gid in enumerate(layer["data"]):
                    if (gid & 0x0FFFFFFF) in liquid:
                        self.water.add((i % self.w, i // self.w))
            if layer["type"] == "objectgroup":
                self.objects = layer["objects"]
        self.ladders = set()
        for o in self.objects:
            if o.get("type") == "ladder":
                x = int(o["x"] // T)
                for y in range(int(o["y"] // T), int((o["y"] + o["height"]) // T)):
                    self.ladders.add((x, y))
                # The top of a ladder can be stood on.
                top = int(o["y"] // T)
                if self.grid[top][x] == "none":
                    self.grid[top][x] = "oneway"

    def at(self, x, y):
        if x < 0 or x >= self.w or y >= self.h:
            return "solid"
        if y < 0:
            return "none"
        return self.grid[y][x]

    def open(self, x, y):
        return self.at(x, y) != "solid"

    def standable(self, x, y):
        return self.open(x, y) and ((x, y) in self.water or self.at(x, y + 1) in ("solid", "oneway") or (x, y) in self.ladders)

    def fall(self, x, y, through_oneway=False):
        """Drop from (x, y) until landing. Returns the landing cell or None."""
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
        out = []
        # Walk sideways (and fall off edges).
        for dx in (-1, 1):
            if self.open(x + dx, y):
                land = self.fall(x + dx, y)
                if land:
                    out.append(land)
        # Drop through a one-way ledge.
        if self.at(x, y + 1) == "oneway":
            land = self.fall(x, y + 1, through_oneway=False)
            if land:
                out.append(land)
        # Swim in any direction through water (and climb out at the surface with a jump).
        if (x, y) in self.water:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if self.open(x + dx, y + dy):
                    out.append((x + dx, y + dy) if (x + dx, y + dy) in self.water else (self.fall(x + dx, y + dy) or (x, y)))
        # Climb ladders.
        if (x, y) in self.ladders or (x, y + 1) in self.ladders:
            for dy in (-1, 1):
                if (x, y + dy) in self.ladders and self.open(x, y + dy):
                    out.append((x, y + dy))
            if (x, y + 1) in self.ladders and self.open(x, y + 1):
                out.append((x, y + 1))
        # Jumps: rise straight up (one-way ledges don't stop you), then drift sideways.
        for up in range(0, MAX_UP + 1):
            if any(self.at(x, y - k) == "solid" for k in range(1, up + 1)):
                break
            top = y - up
            for dx in range(-MAX_ACROSS, MAX_ACROSS + 1):
                if dx == 0 and up == 0:
                    continue
                step = 1 if dx > 0 else -1
                path_clear = all(self.open(x + step * k, top) for k in range(1, abs(dx) + 1))
                if not path_clear:
                    continue
                land = self.fall(x + dx, top)
                if land:
                    out.append(land)
        return out

    def reachable(self, start):
        seen = {start}
        queue = deque([start])
        while queue:
            for n in self.neighbours(*queue.popleft()):
                if n not in seen:
                    seen.add(n)
                    queue.append(n)
        return seen


def object_cells(o):
    """Cells where standing counts as reaching the object."""
    x0, y0 = o["x"], o["y"]
    w, h = o.get("width", 0), o.get("height", 0)
    if o.get("point") or (w == 0 and h == 0):
        tx, ty = int(x0 // T), int((y0 - 1) // T)
        return {(tx + dx, ty + dy) for dx in (-1, 0, 1) for dy in (0, -1)}
    return {(x, y) for x in range(int(x0 // T), int((x0 + w - 1) // T) + 1)
            for y in range(int(y0 // T), int((y0 + h - 1) // T) + 1)}


def check(path):
    lvl = Level(path)
    problems = []
    spawns = [o for o in lvl.objects if o.get("type") == "spawn"]
    exits = [o for o in lvl.objects if o.get("type") == "door"]
    reach_all = set()
    for sp in spawns:
        start = lvl.fall(int(sp["x"] // T), int((sp["y"] - 1) // T))
        if not start:
            problems.append(f"spawn '{sp['name']}' isn't on solid ground")
            continue
        reach = lvl.reachable(start)
        reach_all |= reach
        if exits and not any(object_cells(d) & reach for d in exits):
            problems.append(f"STUCK: from spawn '{sp['name']}' no door can be reached")
        for d in exits:
            if not object_cells(d) & reach:
                problems.append(f"from spawn '{sp['name']}': can't reach the door to "
                                f"{next((p['value'] for p in d.get('properties', []) if p['name'] == 'target_area'), '?')}")
    for o in lvl.objects:
        if o.get("type") in ("pickup", "shrine", "npc", "chest", "shop") and not object_cells(o) & reach_all:
            problems.append(f"unreachable {o['type']} '{o.get('name')}' at tile "
                            f"({int(o['x'] // T)}, {int(o['y'] // T)})")
    return lvl.name, problems


def main():
    names = sys.argv[1:]
    paths = [MAPS / f"{n}.tmj" for n in names] if names else sorted(MAPS.glob("*.tmj"))
    bad = 0
    for p in paths:
        name, problems = check(p)
        status = "ok" if not problems else f"{len(problems)} problem(s)"
        print(f"{name}: {status}")
        for msg in problems:
            print(f"   - {msg}")
        bad += len(problems)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
