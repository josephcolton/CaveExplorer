#!/usr/bin/env python3
"""Bootstrap generator for placeholder art and the first Tiled maps.

Writes assets/tiles/placeholder.bmp and assets/maps/*.tmj. Maps are normal Tiled
JSON files: open them in Tiled (https://www.mapeditor.org) and edit from there.
Existing maps are NOT overwritten unless --force is given, so hand edits are safe.

    python tools/make_maps.py [--force]
"""
import json
import struct
import sys
from pathlib import Path

from make_tiled_project import COLOR_PROPS, ENUM_PROPS

ROOT = Path(__file__).resolve().parent.parent
TILES_DIR = ROOT / "assets" / "tiles"
MAPS_DIR = ROOT / "assets" / "maps"
T = 32
KEY = (255, 0, 255)  # transparent color

# char -> (local tile id, layer, collision, base color)
TILES = {
    "S": (0, "ground", "solid", (222, 196, 132)),   # sand
    "G": (1, "ground", "solid", (86, 150, 60)),     # grass-topped dirt
    "D": (2, "ground", "solid", (120, 86, 56)),     # dirt
    "#": (3, "ground", "solid", (96, 92, 104)),     # cave rock
    "=": (4, "ground", "oneway", (150, 104, 60)),   # wooden one-way platform
    "b": (5, "background", None, (72, 64, 82)),     # cave back wall (light enough to show lamplight)
    "~": (6, "water", None, (60, 120, 200)),        # water (swimmable)
    "T": (7, "background", None, (100, 70, 44)),    # tree trunk
    "L": (8, "background", None, (50, 120, 50)),    # leaves
    "H": (9, "background", None, (170, 120, 80)),   # hut wall
    "C": (10, "background", None, (16, 14, 20)),    # cave mouth darkness
    "r": (11, "ground", "solid", (80, 76, 88)),     # darker rock (variation)
    "m": (12, "ground", "solid", (60, 104, 84)),    # slick mossy rock (slippery)
}
COLUMNS, ROWS = 8, 2
SLIPPERY = {"m"}
LIQUID = {"~"}


# --------------------------------------------------------------------- tileset
def tile_pixel(tid, color, x, y):
    r, g, b = color
    h = ((x * 73856093) ^ (y * 19349663) ^ (tid * 83492791)) & 0xFF
    n = (h % 17) - 8  # small deterministic noise
    shade = lambda c, d: max(0, min(255, c + d))
    if tid == 4:  # platform: only the top 8px are wood
        if y >= 8:
            return KEY
        return (shade(r, -30), shade(g, -30), shade(b, -30)) if y in (0, 7) else (shade(r, n), shade(g, n), shade(b, n))
    if tid == 8:  # leaves: ragged blob
        if ((x - 16) ** 2 + (y - 16) ** 2) > 17 ** 2 or h < 30:
            return KEY
    if tid == 7 and not (10 <= x <= 21):  # trunk
        return KEY
    if tid == 1 and y < 7:  # grass top
        return (shade(70, n), shade(170, n), shade(60, n))
    if tid == 12:  # moss: wet sheen streaks on top
        if y < 6:
            return (shade(90, n), shade(170, n), shade(120, n)) if (x + y) % 7 else (200, 240, 220)
    if tid == 6:  # water ripples
        return (r, g, b) if (y + (x // 6)) % 8 else (120, 170, 230)
    edge = x in (0, 31) or y in (0, 31)
    if edge and tid in (0, 2, 3, 11, 12):
        return (shade(r, -18), shade(g, -18), shade(b, -18))
    return (shade(r, n), shade(g, n), shade(b, n))


def write_tileset(path):
    w, h = COLUMNS * T, ROWS * T
    pixels = [[KEY] * w for _ in range(h)]
    for ch, (tid, _, _, color) in TILES.items():
        ox, oy = (tid % COLUMNS) * T, (tid // COLUMNS) * T
        for y in range(T):
            for x in range(T):
                pixels[oy + y][ox + x] = tile_pixel(tid, color, x, y)
    row_bytes = w * 3
    pad = (4 - row_bytes % 4) % 4
    data = bytearray()
    for y in range(h - 1, -1, -1):  # BMP rows are bottom-up
        for r, g, b in pixels[y]:
            data += bytes((b, g, r))
        data += b"\0" * pad
    header = struct.pack("<2sIHHI", b"BM", 54 + len(data), 0, 0, 54)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + info + data)


def tileset_json():
    tiles = []
    for ch, (tid, _, coll, _) in sorted(TILES.items(), key=lambda kv: kv[1][0]):
        props = [{"name": "char", "type": "string", "value": ch}]
        if coll:
            props.append({"name": "collision", "type": "string", "value": coll})
        if ch in LIQUID:
            props.append({"name": "liquid", "type": "string", "value": "water"})
        if ch in SLIPPERY:
            props.append({"name": "surface", "type": "string", "value": "slippery"})
        tiles.append({"id": tid, "properties": props})
    return {
        "firstgid": 1, "name": "placeholder", "image": "../tiles/placeholder.bmp",
        "imagewidth": COLUMNS * T, "imageheight": ROWS * T, "tilewidth": T, "tileheight": T,
        "columns": COLUMNS, "tilecount": COLUMNS * ROWS, "margin": 0, "spacing": 0,
        "transparentcolor": "#ff00ff", "tiles": tiles,
    }


# ------------------------------------------------------------------------ maps
class Map:
    def __init__(self, area_id, w, h, display, bg, fill=".", backdrop=None, music="", restart="start",
                 darkness=0.0):
        self.id, self.w, self.h, self.display, self.bg = area_id, w, h, display, bg
        self.music = music  # file in assets/soundtrack/
        self.darkness = darkness  # 0 = daylight, 1 = pitch black
        self.restart = restart  # spawn used by "Restart area": the way back toward the surface
        self.backdrop = backdrop  # background tile drawn behind see-through ground tiles
        self.grid = [[fill] * w for _ in range(h)]
        self.objects = []

    def fill(self, x0, y0, x1, y1, ch):
        for y in range(max(0, y0), min(self.h, y1 + 1)):
            for x in range(max(0, x0), min(self.w, x1 + 1)):
                self.grid[y][x] = ch

    def spawn(self, name, tx, ty):
        # Point at the player's feet: bottom-centre of tile column tx, standing on top of row ty.
        self.objects.append({"name": name, "type": "spawn", "x": tx * T + T / 2, "y": ty * T, "point": True})

    def door(self, x0, y0, x1, y1, target_area, target_spawn, trigger="interact", label=None, glow=None,
             gated_by=None):
        self.objects.append({
            "name": f"to_{target_area}", "type": "door",
            "x": x0 * T, "y": y0 * T, "width": (x1 - x0 + 1) * T, "height": (y1 - y0 + 1) * T,
            "properties": [
                {"name": "target_area", "type": "string", "value": target_area},
                {"name": "target_spawn", "type": "string", "value": target_spawn},
                {"name": "trigger", "type": "string", "value": trigger},
            ],
        })
        if label:
            self.objects[-1]["properties"].append({"name": "label", "type": "string", "value": label})
        if glow:
            self.objects[-1]["properties"].append({"name": "glow", "type": "string", "value": glow})
        if gated_by:
            self.objects[-1]["properties"].append({"name": "gated_by", "type": "string", "value": gated_by})

    def obj(self, otype, x, y, w, h, name="", **props):
        o = {"name": name, "type": otype, "x": x, "y": y, "width": w, "height": h}
        if props:
            o["properties"] = [
                {"name": k, "type": "bool" if isinstance(v, bool) else
                 ("int" if isinstance(v, int) else ("float" if isinstance(v, float) else "string")), "value": v}
                for k, v in props.items()]
        self.objects.append(o)

    def decor(self, sprite, tx, ground_row, w, h, **props):
        # Sprite's bottom-left sits on top of tile row ground_row at column tx.
        self.obj("decor", tx * T, ground_row * T - h, w, h, name=sprite, sprite=sprite, **props)

    def pickup(self, item, tx, ground_row, count=1, unique_id=None, gated_by=None):
        # gated_by: ability needed to reach it (checked by tools/check_reach.py)
        props = {"item": item, "count": count}
        if gated_by:
            props["gated_by"] = gated_by
        if unique_id:
            props["unique_id"] = unique_id
        self.obj("pickup", tx * T + T / 2, ground_row * T, 0, 0, name=item, **props)
        self.objects[-1]["point"] = True

    def obstacle(self, kind, x0, y0, x1, y1, passable_from=None):
        props = {"obstacle": kind}
        if passable_from:
            props["passable_from"] = passable_from  # can be pushed through from this side
        self.obj("obstacle", x0 * T, y0 * T, (x1 - x0 + 1) * T, (y1 - y0 + 1) * T, name=kind, **props)

    def npc(self, npc_id, tx, row):
        self.obj("npc", tx * T + T / 2, row * T, 0, 0, name=npc_id, npc_id=npc_id)
        self.objects[-1]["point"] = True

    def light(self, tx, ty, radius, color="#ffe6b4", flicker=False, intensity=1.0):
        # A point light centred on tile (tx, ty).
        self.obj("light", tx * T + T / 2, ty * T + T / 2, 0, 0, name="light", radius=radius, color=color,
                 flicker=flicker, intensity=intensity)
        self.objects[-1]["point"] = True

    def ladder(self, tx, top_row, bottom_row):
        # Climbable from top_row down to bottom_row; the top can be stood on.
        self.obj("ladder", tx * T, top_row * T, T, (bottom_row - top_row + 1) * T, name="ladder")

    def shrine(self, shrine_id, tx, row, gated_by=None):
        extra = {"gated_by": gated_by} if gated_by else {}
        self.obj("shrine", tx * T + T / 2, row * T, 0, 0, name=shrine_id, shrine_id=shrine_id, **extra)
        self.objects[-1]["point"] = True

    def enemy(self, kind, tx, row, flying=False, **props):
        # Walkers: point = feet on top of tile row `row`. Flyers: point = centre of that tile.
        y = row * T + (T / 2 if flying else 0)
        self.obj("enemy", tx * T + T / 2, y, 0, 0, name=kind, enemy=kind, **props)
        self.objects[-1]["point"] = True

    def to_tmj(self):
        layers = {"background": [], "ground": [], "water": []}
        for row in self.grid:
            for ch in row:
                t = TILES.get(ch)
                for name in layers:
                    layers[name].append(t[0] + 1 if t and t[1] == name else 0)
                if ch in ("=", "~") and self.backdrop:
                    layers["background"][-1] = TILES[self.backdrop][0] + 1
        objects = []
        for i, o in enumerate(self.objects, start=1):
            obj = {"id": i, "name": o["name"], "type": o["type"], "x": o["x"], "y": o["y"],
                   "width": o.get("width", 0), "height": o.get("height", 0), "rotation": 0, "visible": True}
            if o.get("point"):
                obj["point"] = True
            if "properties" in o:
                obj["properties"] = o["properties"]
            objects.append(obj)

        def tile_layer(lid, name, data, props, opacity=1):
            return {"id": lid, "name": name, "type": "tilelayer", "x": 0, "y": 0, "width": self.w,
                    "height": self.h, "opacity": opacity, "visible": True, "data": data, "properties": props}

        objects = [typed(o) for o in objects]
        map_props = [typed_prop(p) for p in [
            {"name": "display_name", "type": "string", "value": self.display},
            {"name": "music", "type": "string", "value": self.music or "none"},
            {"name": "restart_spawn", "type": "string", "value": self.restart},
            {"name": "darkness", "type": "float", "value": self.darkness}]]
        return {
            "type": "map",
            "class": "area", "version": "1.10", "tiledversion": "1.10.2", "orientation": "orthogonal",
            "renderorder": "right-down", "infinite": False, "width": self.w, "height": self.h,
            "tilewidth": T, "tileheight": T, "backgroundcolor": self.bg,
            "nextlayerid": 5, "nextobjectid": len(objects) + 1,
            "properties": map_props,
            "tilesets": [tileset_json()],
            "layers": [
                tile_layer(1, "background", layers["background"], []),
                tile_layer(2, "ground", layers["ground"],
                           [{"name": "collides", "type": "bool", "value": True}]),
                # Water is drawn in front of the explorer, see-through.
                tile_layer(4, "water", layers["water"],
                           [{"name": "foreground", "type": "bool", "value": True}], opacity=0.65),
                {"id": 3, "name": "objects", "type": "objectgroup", "draworder": "topdown",
                 "x": 0, "y": 0, "opacity": 1, "visible": True, "objects": objects},
            ],
        }


def typed_prop(p):
    """Mark properties with the Tiled project's types so Tiled shows dropdowns / colour pickers."""
    p = dict(p)
    if p["name"] in ENUM_PROPS:
        p["propertytype"] = ENUM_PROPS[p["name"]]
        p["type"] = "string"
    if p["name"] in COLOR_PROPS and isinstance(p["value"], str):
        p["type"] = "color"
        if len(p["value"]) == 7:
            p["value"] = "#ff" + p["value"][1:]  # Tiled stores colours as #aarrggbb
    return p


def typed(o):
    if "properties" in o:
        o = dict(o, properties=[typed_prop(p) for p in o["properties"]])
    return o


def beach():
    m = Map("beach", 48, 14, "The Beach", "#8ecdf0", music="beach.ogg")
    m.fill(0, 11, 47, 13, "S")
    m.fill(0, 11, 7, 12, "~")    # the sea: a pool two tiles deep, sand below
    m.fill(14, 10, 18, 10, "S")
    m.fill(16, 9, 17, 9, "S")
    m.fill(24, 9, 27, 9, "=")
    m.fill(30, 7, 35, 10, "H")  # shop hut (placeholder tiles)
    m.obj("shop", 30 * T, 7 * T, 6 * T, 4 * T, name="shop", shop_id="beach_shop")
    m.decor("lamp_post", 28, 11, 32, 64)
    m.decor("signpost", 43, 11, 32, 64)
    m.pickup("coins", 12, 11, count=5)
    m.pickup("coins", 25, 9, count=10)
    m.pickup("banana", 16, 9)
    m.pickup("coins", 2, 13, count=15)  # on the seabed
    m.shrine("breath", 21, 11)
    m.spawn("start", 10, 11)
    m.spawn("from_forest", 44, 11)
    m.door(47, 8, 47, 10, "forest", "from_beach", trigger="touch")
    return m


def forest():
    m = Map("forest", 48, 14, "The Forest", "#6fa8c8", music="forest.ogg", restart="from_beach")
    m.fill(0, 11, 47, 11, "G")
    m.fill(0, 12, 47, 13, "D")
    m.fill(20, 9, 26, 9, "G")
    m.fill(20, 10, 26, 10, "D")
    # A proper forest: overlapping trees, some mirrored for variety.
    for x, flip in ((-2, False), (1, True), (4, False), (7, True), (10, False), (13, True), (16, False),
                    (27, True), (30, False), (36, True), (39, False)):
        m.decor("tree", x, 11, 128, 128, flip=flip)
    m.decor("tree", 19, 9, 128, 128, flip=True)
    m.decor("tree", 22, 9, 128, 128)
    m.decor("flowers", 9, 11, 32, 32)
    m.decor("flowers", 17, 11, 32, 32)
    m.decor("fence", 34, 11, 96, 32)
    m.decor("signpost", 40, 11, 32, 64)
    m.fill(32, 8, 35, 8, "=")
    m.obj("chest", 38 * T, 10 * T, T, T, name="chest")
    m.pickup("apple", 24, 9)
    # Cliff with the cave mouth.
    m.fill(43, 0, 47, 13, "#")
    m.fill(43, 9, 44, 10, "C")  # cave mouth, open on its left side
    m.spawn("from_beach", 2, 11)
    m.spawn("from_cave", 41, 11)
    m.door(0, 8, 0, 10, "beach", "from_forest", trigger="touch")
    m.door(43, 9, 44, 10, "cave_1", "from_forest", label="Enter the cave")
    return m


def cave_1():
    m = Map("cave_1", 40, 30, "Upper Caves", "#0c0a10", fill="#", backdrop="b", music="upper_caves.ogg",
            restart="from_forest", darkness=0.85)
    m.fill(1, 1, 8, 4, "b")      # entrance chamber
    m.fill(8, 2, 25, 4, "b")     # tunnel east
    m.fill(22, 5, 25, 17, "b")   # shaft down
    for i, y in enumerate(range(7, 18, 2)):  # zig-zag one-way ledges to climb back up
        x = 22 if i % 2 == 0 else 24
        m.fill(x, y, x + 1, y, "=")
    m.fill(5, 18, 36, 26, "b")   # big chamber
    m.fill(10, 25, 13, 26, "r")  # step (2 tiles: jumpable)
    m.fill(15, 23, 17, 23, "=")  # machete ledge, reached from the step
    m.fill(29, 25, 32, 25, "=")
    m.ladder(22, 5, 26)          # from the chamber floor up the shaft to the tunnel
    # West passage -> Drowned Tunnel (open; crossing its flooded tunnel needs more breath).
    m.fill(0, 25, 4, 26, "b")
    m.spawn("from_cave_3", 2, 27)
    m.door(0, 25, 0, 26, "cave_3", "from_cave_1", trigger="touch")
    # A ledge high on the chamber's east wall -> Bat Roost (needs a high jump).
    m.fill(33, 21, 36, 21, "r")
    m.fill(37, 18, 39, 20, "b")
    m.light(35, 19, 50, "#c8dcff", intensity=0.5)  # a cold draught of light from above
    m.spawn("from_cave_4", 37, 21)
    m.door(39, 18, 39, 20, "cave_4", "from_cave_1", trigger="touch", gated_by="highjump")
    # The top tunnel continues east past a boulder -> Crystal Mine (needs a pickaxe).
    m.fill(26, 2, 39, 4, "b")
    m.obstacle("rock", 31, 2, 31, 4, passable_from="right")
    m.light(36, 3, 45, "#6ec8ff", intensity=0.5)  # crystals glint beyond the boulder
    m.spawn("from_cave_5", 37, 5)
    m.door(39, 2, 39, 4, "cave_5", "from_cave_1", trigger="touch", gated_by="break")
    m.light(30, 23, 70, "#6ec8ff", intensity=0.7)  # crystal seam above the east ledge
    m.light(12, 23, 50, "#6ec8ff", intensity=0.5)
    m.light(22, 4, 60, "#ffd9a0", flicker=True, intensity=0.6)  # a candle stub at the top of the ladder
    m.fill(36, 23, 39, 26, "b")  # passage east
    m.pickup("stones", 6, 5, count=6)
    m.npc("hob", 7, 5)  # Old Hob, trader and quest giver
    m.shrine("swift", 15, 5)   # in the tunnel east
    m.shrine("leap", 7, 27)    # big chamber floor
    m.pickup("bone", 12, 25)
    m.pickup("coins", 20, 27, count=8)
    m.pickup("crystal", 30, 25)
    m.pickup("machete", 16, 23, unique_id="cave1_machete")
    m.obstacle("bush", 37, 23, 37, 26, passable_from="right")  # blocks the way to Mossy Hollow
    m.enemy("bat", 23, 11, flying=True, enemy_easy="none")
    m.enemy("bat", 18, 20, flying=True)
    m.enemy("rat", 15, 27)
    m.enemy("rat", 26, 27, enemy_easy="none")
    m.enemy("none", 33, 27, enemy_hard="beetle")
    m.spawn("from_forest", 4, 5)
    m.spawn("from_cave_2", 36, 27)
    m.door(2, 3, 3, 4, "forest", "from_cave", label="Leave the cave", glow="#fff0c0")
    m.door(39, 23, 39, 26, "cave_2", "from_cave_1", trigger="touch", gated_by="cut")
    return m


def cave_2():
    m = Map("cave_2", 40, 20, "Mossy Hollow", "#0c0a10", fill="#", backdrop="b", restart="from_cave_1",
            darkness=0.93)
    m.fill(0, 3, 38, 16, "b")
    m.fill(8, 15, 11, 16, "r")
    m.fill(12, 17, 31, 17, "m")  # slick moss floor
    for x in (14, 20, 26, 31):  # the moss glows faintly
        m.light(x, 16, 55, "#5ad296", intensity=0.55)
    m.light(23, 14, 45, "#5ad296", intensity=0.45)
    m.light(36, 15, 60, "#a0c8ff", intensity=0.6)  # the alcove
    m.fill(14, 13, 17, 13, "=")
    m.fill(20, 15, 26, 16, "r")
    m.fill(20, 15, 26, 15, "m")
    m.fill(28, 13, 31, 13, "=")
    # Treasure alcove behind a boulder (needs a pickaxe).
    m.fill(33, 3, 33, 14, "#")
    m.obstacle("rock", 33, 15, 33, 16, passable_from="right")
    m.pickup("feather_charm", 36, 17, unique_id="cave2_charm", gated_by="break")
    m.shrine("vigor", 37, 17, gated_by="break")  # lasting blessing, behind the boulder
    # A high ledge above the rock step: the Amulet of Deep Breathing (needs a high jump).
    # (Temporary home - it will move once more areas exist.)
    m.fill(3, 11, 6, 11, "r")
    m.pickup("breath_amulet", 4, 11, unique_id="hollow_amulet", gated_by="highjump")
    m.light(4, 10, 40, "#a0e0ff", intensity=0.6)
    m.shrine("ward", 4, 17)
    m.shrine("shadow", 9, 15)  # on the rock step
    m.shrine("might", 31, 13)  # on the high ledge
    m.pickup("crystal", 35, 17, count=2, gated_by="break")
    m.pickup("crystal", 29, 13)
    m.pickup("bone", 17, 17)
    m.pickup("coins", 23, 15, count=12)
    m.enemy("bat", 12, 7, flying=True)
    m.enemy("bat", 30, 6, flying=True, enemy_hard="giant_bat")
    m.enemy("beetle", 23, 15, enemy_easy="rat")
    m.enemy("rat", 6, 17)
    m.spawn("from_cave_1", 2, 17)
    m.door(0, 14, 0, 16, "cave_1", "from_cave_2", trigger="touch")
    return m


def cave_3():
    # The Drowned Tunnel: jump into a pool, swim down, along a long flooded tunnel and up into the
    # Sunken Grotto. Far longer than one breath: needs the Amulet of Deep Breathing (or the
    # Deep Breath shrine).
    m = Map("cave_3", 72, 24, "Drowned Tunnel", "#06080c", fill="#", backdrop="b", music="deep_cave.ogg",
            restart="from_cave_1", darkness=0.9)
    m.fill(0, 4, 9, 8, "b")       # entry cavern
    m.fill(6, 9, 9, 20, "~")      # the pool: a flooded shaft going down
    m.fill(6, 18, 58, 20, "~")    # the long tunnel
    m.fill(55, 9, 58, 17, "~")    # and back up on the far side
    m.fill(50, 3, 71, 8, "b")     # the Sunken Grotto
    for x in range(10, 58, 8):    # glowing algae light the way
        m.light(x, 19, 60, "#50e0c0", intensity=0.6)
    m.light(8, 7, 50, "#a0e0ff", intensity=0.5)
    m.light(64, 5, 80, "#6ec8ff", intensity=0.8)
    m.pickup("wizard_hat", 66, 9, unique_id="grotto_hat", gated_by="breath")
    m.pickup("crystal", 62, 9, count=3, gated_by="breath")
    m.pickup("coins", 69, 9, count=25, gated_by="breath")
    m.pickup("healing_potion", 60, 9, gated_by="breath")
    m.enemy("bat", 4, 5, flying=True, enemy_easy="none")
    m.spawn("from_cave_1", 2, 9)
    m.door(0, 6, 0, 8, "cave_1", "from_cave_3", trigger="touch")
    return m


def cave_4():
    # The Bat Roost: a tall cavern reached by a high jump from the Upper Caves. Climb it.
    m = Map("cave_4", 36, 32, "Bat Roost", "#0a0a10", fill="#", backdrop="b", music="upper_caves.ogg",
            restart="from_cave_1", darkness=0.85)
    m.fill(0, 27, 6, 29, "b")     # entry passage
    m.fill(6, 3, 33, 29, "b")     # the roost
    starts = (8, 13, 18, 23, 28, 23, 18, 13, 8, 13, 18, 23)
    for i, x in enumerate(starts):  # ledges zig-zag up, two tiles apart
        m.fill(x, 28 - 2 * i, x + 3, 28 - 2 * i, "=")
    m.fill(22, 4, 28, 4, "r")     # the roost's top shelf
    m.fill(22, 4, 22, 5, "r")
    m.pickup("warm_cloak", 26, 4, unique_id="roost_cloak")
    m.pickup("crystal", 24, 4, count=2)
    for i, x in enumerate(starts[1:9:2]):
        m.pickup("coins", x + 1, 26 - 4 * i, count=4)
    m.light(25, 3, 60, "#ffd9a0", flicker=True, intensity=0.6)
    for x, y in ((12, 22), (25, 18), (10, 12), (28, 9), (17, 7)):
        m.enemy("bat", x, y, flying=True)
    m.enemy("bat", 20, 14, flying=True, enemy_hard="giant_bat", enemy_easy="none")
    m.enemy("rat", 20, 30)
    m.spawn("from_cave_1", 3, 30)
    m.door(0, 27, 0, 29, "cave_1", "from_cave_4", trigger="touch")
    return m


def cave_5():
    # The Crystal Mine: behind the boulder in the Upper Caves' top tunnel. Very dark; crystals
    # light the way down a series of terraces.
    m = Map("cave_5", 48, 22, "Crystal Mine", "#05050a", fill="#", backdrop="b", restart="from_cave_1",
            darkness=0.95)
    m.fill(0, 2, 6, 4, "b")       # entry tunnel
    m.fill(6, 2, 46, 19, "b")     # the mine
    for x0, x1, top in ((6, 13, 5), (14, 21, 7), (22, 29, 9), (30, 37, 11), (38, 46, 13)):
        m.fill(x0, top, x1, 19, "r")  # terraces stepping down two tiles at a time
    m.fill(30, 7, 32, 7, "=")
    for x, top in ((11, 5), (18, 7), (26, 9), (34, 11), (41, 13)):
        m.pickup("crystal", x, top, count=2)
        m.light(x, top - 1, 55, "#6ec8ff", intensity=0.7)
    m.pickup("helmet", 45, 13, unique_id="mine_helmet")
    m.pickup("coins", 31, 7, count=15)
    m.pickup("lamp_oil", 20, 7, count=2)
    m.enemy("beetle", 25, 9)
    m.enemy("rat", 35, 11)
    m.enemy("beetle", 43, 13, enemy_easy="rat")
    m.enemy("bat", 30, 4, flying=True)
    m.spawn("from_cave_1", 3, 5)
    m.door(0, 2, 0, 4, "cave_1", "from_cave_5", trigger="touch")
    return m


def main():
    force = "--force" in sys.argv
    write_tileset(TILES_DIR / "placeholder.bmp")
    MAPS_DIR.mkdir(parents=True, exist_ok=True)
    for m in (beach(), forest(), cave_1(), cave_2(), cave_3(), cave_4(), cave_5()):
        path = MAPS_DIR / f"{m.id}.tmj"
        if path.exists() and not force:
            print(f"skip   {path.relative_to(ROOT)} (exists; use --force to overwrite)")
            continue
        path.write_text(json.dumps(m.to_tmj(), indent=1))
        print(f"wrote  {path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
