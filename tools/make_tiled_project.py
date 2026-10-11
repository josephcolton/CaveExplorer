#!/usr/bin/env python3
"""Writes CaveExplorer.tiled-project: the Tiled project with our object types.

Open the project in Tiled (File > Open File or Project...) and every object kind the game
understands (door, pickup, enemy, shrine...) becomes a class with fill-in fields, defaults and
dropdowns. The dropdown lists (items, creatures, shrines, areas, music...) are read from the
game's data files, so re-run this whenever you add any of those:

    python tools/make_tiled_project.py

tools/make_maps.py imports ENUM_PROPS / COLOR_PROPS from here so generated maps use the same types.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"

# Property name -> dropdown (enum) type, shared with make_maps.py.
ENUM_PROPS = {
    "trigger": "Trigger",
    "target_area": "Area",
    "gated_by": "Ability",
    "item": "Item",
    "enemy": "Enemy",
    "enemy_easy": "EnemyVariant",
    "enemy_hard": "EnemyVariant",
    "shrine_id": "Shrine",
    "npc_id": "Npc",
    "shop_id": "Shop",
    "obstacle": "Obstacle",
    "passable_from": "Side",
    "sprite": "Sprite",
    "music": "Music",
}
COLOR_PROPS = {"glow", "color"}


def keys(file, section):
    return sorted(k for k in json.loads((DATA / file).read_text()).get(section, {}) if not k.startswith("_"))


def enums():
    items = [it["id"] for it in json.loads((DATA / "items.json").read_text())["items"]]
    enemies = keys("enemies.json", "enemies")
    return {
        "Trigger": (["interact", "touch"], False),
        "Ability": (["cut", "break", "highjump", "breath"], True),  # flags: tick any number
        "Area": (sorted(p.stem for p in (ROOT / "assets" / "maps").glob("*.tmj")), False),
        "Item": (["coins"] + items, False),
        "Enemy": (enemies, False),
        "EnemyVariant": (["same", "none"] + enemies, False),
        "Shrine": (keys("shrines.json", "shrines"), False),
        "Npc": (keys("npcs.json", "npcs"), False),
        "Shop": (keys("shops.json", "shops"), False),
        "Obstacle": (keys("obstacles.json", "obstacles"), False),
        "Side": (["never", "left", "right"], False),
        "Sprite": (keys("sprites.json", "sprites"), False),
        "Music": (["none"] + sorted(p.name for p in (ROOT / "assets" / "soundtrack").iterdir()
                                    if p.suffix in (".ogg", ".mp3", ".wav", ".flac")), False),
    }


def m(name, kind, default, enum=None):
    """One field of a class. kind: string, int, float, bool, color."""
    member = {"name": name, "type": kind, "value": default}
    if enum:
        member["propertyType"] = enum
    return member


# Object classes (the object's "Class" in Tiled) -> (colour in the editor, fields, help text).
CLASSES = {
    "spawn": ("#ff43a047", [], "Where the explorer appears. Name it; doors refer to it by name."),
    "door": ("#ffd81b60", [
        m("target_area", "string", "beach", "Area"),
        m("target_spawn", "string", "start"),
        m("trigger", "string", "interact", "Trigger"),
        m("label", "string", "Enter"),
        m("glow", "color", ""),
        m("gated_by", "string", "", "Ability"),
    ], "A way to another area: 'touch' at an area's edge, 'interact' (press Up) for doorways."),
    "pickup": ("#fffdd835", [
        m("item", "string", "coins", "Item"),
        m("count", "int", 1),
        m("unique_id", "string", ""),
        m("gated_by", "string", "", "Ability"),
    ], "An item or coins lying in the world (a point at its feet). unique_id = never comes back once taken."),
    "enemy": ("#ffe53935", [
        m("enemy", "string", "rat", "Enemy"),
        m("enemy_easy", "string", "same", "EnemyVariant"),
        m("enemy_hard", "string", "same", "EnemyVariant"),
        m("hp_scale", "float", 1.0),
    ], "A creature (point at a walker's feet or a flyer's centre); swap or remove per difficulty."),
    "shrine": ("#ff8e24aa", [m("shrine_id", "string", "leap", "Shrine"), m("gated_by", "string", "", "Ability")],
               "A shrine (point at its feet)."),
    "npc": ("#ff00acc1", [m("npc_id", "string", "hob", "Npc")], "A person (point at their feet)."),
    "shop": ("#ff00897b", [m("shop_id", "string", "beach_shop", "Shop")], "A shop counter (rectangle)."),
    "chest": ("#ff6d4c41", [], "The storage chest (rectangle, one tile)."),
    "obstacle": ("#ff757575", [
        m("obstacle", "string", "bush", "Obstacle"),
        m("passable_from", "string", "never", "Side"),
    ], "Blocks whole tiles until cleared with the right tool (rectangle on the tile grid)."),
    "ladder": ("#ff8d6e63", [], "Climbable, one tile wide (rectangle); its top can be stood on."),
    "light": ("#ffffca28", [
        m("radius", "float", 80.0),
        m("color", "color", "#ffffe6b4"),
        m("intensity", "float", 1.0),
        m("flicker", "bool", False),
    ], "A light in dark areas (point at its centre)."),
    "decor": ("#ff9ccc65", [
        m("sprite", "string", "tree", "Sprite"),
        m("flip", "bool", False),
        m("foreground", "bool", False),
    ], "A decoration sprite, drawn from the object's bottom-left corner."),
}

MAP_CLASS = ("area", [
    m("display_name", "string", "New Area"),
    m("music", "string", "none", "Music"),
    m("darkness", "float", 0.0),
    m("restart_spawn", "string", "start"),
])


def main():
    types, next_id = [], 1
    for name, (values, flags) in enums().items():
        types.append({"id": next_id, "name": name, "type": "enum", "storageType": "string",
                      "values": values, "valuesAsFlags": flags})
        next_id += 1
    for name, (color, members, _help) in CLASSES.items():
        types.append({"id": next_id, "name": name, "type": "class", "useAs": ["object"], "color": color,
                      "drawFill": True, "members": members})
        next_id += 1
    types.append({"id": next_id, "name": MAP_CLASS[0], "type": "class", "useAs": ["map"], "color": "#ff607d8b",
                  "drawFill": True, "members": MAP_CLASS[1]})

    project = {
        "automappingRulesFile": "",
        "commands": [],
        "compatibilityVersion": 1100,
        "extensionsPath": "tiled-extensions",
        "folders": ["assets/maps"],
        "properties": [],
        "propertyTypes": types,
    }
    out = ROOT / "CaveExplorer.tiled-project"
    out.write_text(json.dumps(project, indent=1) + "\n")
    print(f"wrote {out.name}: {len(CLASSES)} object classes, {len(enums())} dropdown lists")


if __name__ == "__main__":
    main()
