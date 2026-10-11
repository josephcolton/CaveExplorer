#!/usr/bin/env python3
"""Convert PauR's "Nature tileset" sample into assets/sprites/nature.png with real transparency.

Source: https://opengameart.org/content/nature-tileset (CC-BY 3.0, by Paulina Riva)
The sample sheet has its checkerboard preview background baked in; those two colors
are made transparent.

    python tools/pack_nature.py <path to tilesetOpenGame.png>
"""
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
CHECKER = {(255, 255, 255), (208, 228, 227)}


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    img = Image.open(sys.argv[1]).convert("RGBA")
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            if px[x, y][:3] in CHECKER:
                px[x, y] = (0, 0, 0, 0)
    out = ROOT / "assets" / "sprites" / "nature.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
