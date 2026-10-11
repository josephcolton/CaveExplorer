#!/usr/bin/env python3
"""Builds the game icon from the knight sprite: assets/sprites/icon.png (window icon) and
packaging/windows/cave_explorer.ico (the .exe icon). Requires Pillow.

    python tools/make_icon.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent


def main():
    sheet = Image.open(ROOT / "assets" / "sprites" / "knight.png").convert("RGBA")
    knight = sheet.crop((0, 0, 96, 64))  # idle frame
    knight = knight.crop(knight.getbbox())

    size = 64
    icon = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(icon)
    # Cave mouth: dark arch with a warm rim.
    d.rounded_rectangle((2, 2, size - 3, size - 3), radius=12, fill=(28, 22, 34, 255), outline=(246, 204, 92, 255), width=3)
    d.ellipse((12, 10, size - 13, size + 18), fill=(12, 10, 16, 255))
    icon.alpha_composite(knight, ((size - knight.width) // 2, size - 6 - knight.height))

    icon.save(ROOT / "assets" / "sprites" / "icon.png")
    big = icon.resize((256, 256), Image.NEAREST)
    big.save(ROOT / "packaging" / "windows" / "cave_explorer.ico",
             sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    print("wrote assets/sprites/icon.png and packaging/windows/cave_explorer.ico")


if __name__ == "__main__":
    main()
