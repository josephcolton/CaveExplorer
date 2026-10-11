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
    "b": (5, "background", None, (44, 40, 52)),     # cave back wall
    "~": (6, "background", None, (60, 120, 200)),   # water (decor for now)
    "T": (7, "background", None, (100, 70, 44)),    # tree trunk
    "L": (8, "background", None, (50, 120, 50)),    # leaves
    "H": (9, "background", None, (170, 120, 80)),   # hut wall
    "C": (10, "background", None, (16, 14, 20)),    # cave mouth darkness
    "r": (11, "ground", "solid", (80, 76, 88)),     # darker rock (variation)
}
COLUMNS, ROWS = 8, 2


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
    if tid == 6:  # water ripples
        return (r, g, b) if (y + (x // 6)) % 8 else (120, 170, 230)
    edge = x in (0, 31) or y in (0, 31)
    if edge and tid in (0, 2, 3, 11):
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
        tiles.append({"id": tid, "properties": props})
    return {
        "firstgid": 1, "name": "placeholder", "image": "../tiles/placeholder.bmp",
        "imagewidth": COLUMNS * T, "imageheight": ROWS * T, "tilewidth": T, "tileheight": T,
        "columns": COLUMNS, "tilecount": COLUMNS * ROWS, "margin": 0, "spacing": 0,
        "transparentcolor": "#ff00ff", "tiles": tiles,
    }


# ------------------------------------------------------------------------ maps
class Map:
    def __init__(self, area_id, w, h, display, bg, fill=".", backdrop=None, music=""):
        self.id, self.w, self.h, self.display, self.bg = area_id, w, h, display, bg
        self.music = music  # file in assets/music/
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

    def door(self, x0, y0, x1, y1, target_area, target_spawn, trigger="interact"):
        self.objects.append({
            "name": f"to_{target_area}", "type": "door",
            "x": x0 * T, "y": y0 * T, "width": (x1 - x0 + 1) * T, "height": (y1 - y0 + 1) * T,
            "properties": [
                {"name": "target_area", "type": "string", "value": target_area},
                {"name": "target_spawn", "type": "string", "value": target_spawn},
                {"name": "trigger", "type": "string", "value": trigger},
            ],
        })

    def obj(self, otype, x, y, w, h, name="", **props):
        o = {"name": name, "type": otype, "x": x, "y": y, "width": w, "height": h}
        if props:
            o["properties"] = [
                {"name": k, "type": "bool" if isinstance(v, bool) else
                 ("int" if isinstance(v, int) else "string"), "value": v}
                for k, v in props.items()]
        self.objects.append(o)

    def decor(self, sprite, tx, ground_row, w, h, **props):
        # Sprite's bottom-left sits on top of tile row ground_row at column tx.
        self.obj("decor", tx * T, ground_row * T - h, w, h, name=sprite, sprite=sprite, **props)

    def pickup(self, item, tx, ground_row, count=1, unique_id=None):
        props = {"item": item, "count": count}
        if unique_id:
            props["unique_id"] = unique_id
        self.obj("pickup", tx * T + T / 2, ground_row * T, 0, 0, name=item, **props)
        self.objects[-1]["point"] = True

    def to_tmj(self):
        layers = {"background": [], "ground": []}
        for row in self.grid:
            for ch in row:
                t = TILES.get(ch)
                for name in layers:
                    layers[name].append(t[0] + 1 if t and t[1] == name else 0)
                if ch == "=" and self.backdrop:
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

        def tile_layer(lid, name, data, props):
            return {"id": lid, "name": name, "type": "tilelayer", "x": 0, "y": 0, "width": self.w,
                    "height": self.h, "opacity": 1, "visible": True, "data": data, "properties": props}

        return {
            "type": "map", "version": "1.10", "tiledversion": "1.10.2", "orientation": "orthogonal",
            "renderorder": "right-down", "infinite": False, "width": self.w, "height": self.h,
            "tilewidth": T, "tileheight": T, "backgroundcolor": self.bg,
            "nextlayerid": 4, "nextobjectid": len(objects) + 1,
            "properties": [{"name": "display_name", "type": "string", "value": self.display},
                           {"name": "music", "type": "string", "value": self.music}],
            "tilesets": [tileset_json()],
            "layers": [
                tile_layer(1, "background", layers["background"], []),
                tile_layer(2, "ground", layers["ground"],
                           [{"name": "collides", "type": "bool", "value": True}]),
                {"id": 3, "name": "objects", "type": "objectgroup", "draworder": "topdown",
                 "x": 0, "y": 0, "opacity": 1, "visible": True, "objects": objects},
            ],
        }


def beach():
    m = Map("beach", 48, 14, "The Beach", "#8ecdf0")
    m.fill(0, 9, 7, 10, "~")
    m.fill(0, 11, 47, 13, "S")
    m.fill(14, 10, 18, 10, "S")
    m.fill(16, 9, 17, 9, "S")
    m.fill(24, 7, 27, 7, "=")
    m.fill(30, 7, 35, 10, "H")  # shop hut (placeholder tiles)
    m.obj("shop", 30 * T, 7 * T, 6 * T, 4 * T, name="shop", shop_id="beach_shop")
    m.decor("lamp_post", 28, 11, 32, 64)
    m.decor("signpost", 43, 11, 32, 64)
    m.pickup("coins", 12, 11, count=5)
    m.pickup("coins", 25, 7, count=10)
    m.pickup("banana", 16, 9)
    m.spawn("start", 10, 11)
    m.spawn("from_forest", 44, 11)
    m.door(47, 8, 47, 10, "forest", "from_beach", trigger="touch")
    return m


def forest():
    m = Map("forest", 48, 14, "The Forest", "#6fa8c8", music="forest.ogg")
    m.fill(0, 11, 47, 11, "G")
    m.fill(0, 12, 47, 13, "D")
    m.fill(20, 9, 26, 9, "G")
    m.fill(20, 10, 26, 10, "D")
    for x in (3, 11, 27):
        m.decor("tree", x, 11, 128, 128)
    m.decor("tree", 21, 9, 128, 128)
    m.decor("flowers", 9, 11, 32, 32)
    m.decor("flowers", 17, 11, 32, 32)
    m.decor("fence", 34, 11, 96, 32)
    m.decor("signpost", 40, 11, 32, 64)
    m.fill(32, 8, 35, 8, "=")
    m.obj("chest", 38 * T, 10 * T, T, T, name="chest")
    m.pickup("apple", 24, 9)
    # Cliff with the cave mouth.
    m.fill(42, 0, 47, 13, "#")
    m.fill(43, 9, 44, 10, "C")
    m.fill(42, 11, 47, 11, "#")
    m.spawn("from_beach", 2, 11)
    m.spawn("from_cave", 41, 11)
    m.door(0, 8, 0, 10, "beach", "from_forest", trigger="touch")
    m.door(43, 9, 44, 10, "cave_1", "from_forest")
    return m


def cave_1():
    m = Map("cave_1", 40, 30, "Upper Caves", "#0c0a10", fill="#", backdrop="b")
    m.fill(1, 1, 8, 4, "b")      # entrance chamber
    m.fill(8, 2, 25, 4, "b")     # tunnel east
    m.fill(22, 5, 25, 17, "b")   # shaft down
    for i, y in enumerate(range(7, 18, 2)):  # zig-zag one-way ledges to climb back up
        x = 22 if i % 2 == 0 else 24
        m.fill(x, y, x + 1, y, "=")
    m.fill(5, 18, 36, 26, "b")   # big chamber
    m.fill(10, 24, 13, 26, "r")  # step
    m.fill(16, 22, 18, 22, "=")
    m.fill(29, 22, 32, 22, "=")
    m.fill(36, 23, 39, 26, "b")  # passage east
    m.pickup("stones", 6, 5, count=6)
    m.pickup("bone", 12, 24)
    m.pickup("coins", 20, 27, count=8)
    m.pickup("crystal", 30, 22)
    m.spawn("from_forest", 4, 5)
    m.spawn("from_cave_2", 36, 27)
    m.door(2, 3, 3, 4, "forest", "from_cave")
    m.door(39, 23, 39, 26, "cave_2", "from_cave_1", trigger="touch")
    return m


def cave_2():
    m = Map("cave_2", 40, 20, "Mossy Hollow", "#0c0a10", fill="#", backdrop="b")
    m.fill(0, 3, 38, 16, "b")
    m.fill(8, 14, 11, 16, "r")
    m.fill(14, 12, 17, 12, "=")
    m.fill(20, 13, 26, 16, "r")
    m.fill(28, 10, 31, 10, "=")
    m.pickup("machete", 35, 17, unique_id="cave2_machete")
    m.pickup("crystal", 29, 10)
    m.pickup("coins", 23, 13, count=12)
    m.spawn("from_cave_1", 2, 17)
    m.door(0, 14, 0, 16, "cave_1", "from_cave_2", trigger="touch")
    return m


def main():
    force = "--force" in sys.argv
    write_tileset(TILES_DIR / "placeholder.bmp")
    MAPS_DIR.mkdir(parents=True, exist_ok=True)
    for m in (beach(), forest(), cave_1(), cave_2()):
        path = MAPS_DIR / f"{m.id}.tmj"
        if path.exists() and not force:
            print(f"skip   {path.relative_to(ROOT)} (exists; use --force to overwrite)")
            continue
        path.write_text(json.dumps(m.to_tmj(), indent=1))
        print(f"wrote  {path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
