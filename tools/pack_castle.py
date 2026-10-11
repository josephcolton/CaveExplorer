#!/usr/bin/env python3
"""Cut the knight ("Elisa") and bat animations out of Jetrel's "castle platformer" pack.

Source: https://opengameart.org/content/castle-platformer (CC0)
Each frame in the source sheets is outlined with a red box on an olive background; both are
made transparent. Frames are re-packed into fixed-size cells with the character's feet (or
centre, for the bat) on a common anchor, so the game can flip and position them simply.

    python tools/pack_castle.py "<path to unzipped 'untitled folder'>"
"""
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
RED = (249, 48, 61, 255)
BG = (111, 109, 81, 255)


def frames(path, bands):
    im = Image.open(path).convert("RGBA")
    px = im.load()
    out = []
    for top, bottom in bands:
        x = 0
        while x < im.width:
            if px[x, top] == RED:
                start = x
                while x < im.width and px[x, top] == RED:
                    x += 1
                if x - start >= 10:
                    fr = im.crop((start + 1, top + 1, x - 1, bottom)).copy()
                    p = fr.load()
                    for yy in range(fr.height):
                        for xx in range(fr.width):
                            if p[xx, yy] in (BG, RED):
                                p[xx, yy] = (0, 0, 0, 0)
                    out.append(fr)
            x += 1
    return out


def pack(frames_, cell_w, cell_h, anchor, foot_anchor, out_path, columns=8):
    rows = (len(frames_) + columns - 1) // columns
    sheet = Image.new("RGBA", (columns * cell_w, rows * cell_h), (0, 0, 0, 0))
    for i, fr in enumerate(frames_):
        bb = fr.getbbox()
        if foot_anchor:
            # Horizontal anchor = centre of the lowest 8 rows (the feet); vertical = bottom.
            feet = fr.crop((0, bb[3] - 8, fr.width, bb[3])).getbbox()
            ax = (feet[0] + feet[2]) // 2
            ay = bb[3]
        else:
            ax, ay = (bb[0] + bb[2]) // 2, (bb[1] + bb[3]) // 2
        ox = (i % columns) * cell_w + anchor[0] - ax
        oy = (i // columns) * cell_h + anchor[1] - ay
        sheet.alpha_composite(fr, (ox, oy)) if ox >= 0 and oy >= 0 else sheet.paste(fr, (ox, oy), fr)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out_path)
    print(f"wrote {out_path} ({len(frames_)} frames)")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    src = Path(sys.argv[1])
    knight = frames(src / "elisa-spritesheet1.png", [(3, 58), (61, 116), (120, 175), (178, 241), (243, 306)])
    pack(knight, 96, 64, (48, 62), True, ROOT / "assets" / "sprites" / "knight.png")
    bat = frames(src / "bat.png", [(1, 33), (35, 67), (69, 101)])
    pack(bat, 32, 32, (16, 16), False, ROOT / "assets" / "sprites" / "bat.png", columns=10)


if __name__ == "__main__":
    main()
