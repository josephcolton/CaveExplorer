#!/usr/bin/env python3
"""Pack the icons listed in tools/icons.txt into assets/sprites/icons.png + icons.json.

    python tools/pack_icons.py "<path to unzipped 'crawl-tiles Oct-5-2010'>"

Requires Pillow (pip install pillow).
"""
import json
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "tools" / "icons.txt"
OUT_DIR = ROOT / "assets" / "sprites"
COLUMNS, SIZE = 16, 32


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    pack = Path(sys.argv[1])
    entries = []
    for line in MANIFEST.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            name, rel = line.split(None, 1)
            entries.append((name, rel))

    rows = (len(entries) + COLUMNS - 1) // COLUMNS
    atlas = Image.new("RGBA", (COLUMNS * SIZE, rows * SIZE), (0, 0, 0, 0))
    index = {}
    for i, (name, rel) in enumerate(entries):
        if name in index:
            sys.exit(f"duplicate icon name: {name}")
        icon = Image.open(pack / rel).convert("RGBA")
        if icon.size != (SIZE, SIZE):
            sys.exit(f"{rel}: expected {SIZE}x{SIZE}, got {icon.size}")
        atlas.paste(icon, ((i % COLUMNS) * SIZE, (i // COLUMNS) * SIZE))
        index[name] = i

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    atlas.save(OUT_DIR / "icons.png")
    meta = {"tile_size": SIZE, "columns": COLUMNS, "icons": index}
    (OUT_DIR / "icons.json").write_text(json.dumps(meta, indent=1))
    print(f"packed {len(entries)} icons into {OUT_DIR / 'icons.png'}")


if __name__ == "__main__":
    main()
