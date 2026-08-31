#!/usr/bin/env python3

import os
import re
import math
import argparse
import xml.etree.ElementTree as ET

from PIL import Image


MAX_ATLAS_SIZE = 4096


def next_pow2(v):
    p = 1
    while p < v:
        p <<= 1
    return p


def find_case_insensitive_path(base_dir, relative_path):
    cur = base_dir

    for part in relative_path.replace("\\", "/").split("/"):
        if not os.path.isdir(cur):
            return None

        found = None

        for entry in os.listdir(cur):
            if entry.lower() == part.lower():
                found = entry
                break

        if found is None:
            return None

        cur = os.path.join(cur, found)

    return cur

def parse_map(map_path):
    with open(map_path, "r", encoding="utf-8", errors="ignore") as f:
        first_line = f.readline()

    m = re.search(r'tileset\s*=\s*"([^"]+)"', first_line, re.IGNORECASE)

    if not m:
        raise RuntimeError(
            f"Could not find tileset attribute in {map_path}"
        )

    return m.group(1)

def parse_tileset(tileset_path):
    textures = []

    with open(tileset_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()

            if not line:
                continue

            parts = line.split()

            if len(parts) < 2:
                continue

            texname = parts[1]

            if texname.lower() == "missingimage":
                continue

            if texname not in textures:
                textures.append(texname)

    return textures


def load_textures(texture_names, gfx_dir):
    textures = []

    for name in texture_names:
        png_name = name + ".png"
        path = find_case_insensitive_path(gfx_dir, png_name)

        if not path:
            print(f"WARNING: missing texture {png_name}")
            continue

        img = Image.open(path).convert("RGBA")

        textures.append({
            "name": name,
            "image": img,
            "width": img.width,
            "height": img.height,
        })

    return textures


def pack_textures(textures, atlas_w):
    x = 0
    y = 0
    row_h = 0

    placements = []

    for tex in textures:
        w = tex["width"]
        h = tex["height"]

        if w > atlas_w:
            return None

        if x + w > atlas_w:
            x = 0
            y += row_h
            row_h = 0

        placements.append({
            "texture": tex,
            "x": x,
            "y": y,
        })

        x += w
        row_h = max(row_h, h)

    total_h = y + row_h

    return placements, total_h


def build_atlas(textures):
    textures.sort(
        key=lambda t: max(t["width"], t["height"]),
        reverse=True
    )

    max_tex_w = max(t["width"] for t in textures)

    candidate_widths = []
    w = next_pow2(max_tex_w)

    while w <= MAX_ATLAS_SIZE:
        candidate_widths.append(w)
        w <<= 1

    best = None

    for atlas_w in candidate_widths:
        result = pack_textures(textures, atlas_w)

        if result is None:
            continue

        placements, used_h = result

        atlas_h = next_pow2(used_h)

        if atlas_h > MAX_ATLAS_SIZE:
            continue

        area = atlas_w * atlas_h

        if best is None or area < best["area"]:
            best = {
                "width": atlas_w,
                "height": atlas_h,
                "placements": placements,
                "area": area,
            }

    if best is None:
        raise RuntimeError(
            "Textures do not fit into a 4096x4096 atlas"
        )

    return best


def save_atlas(atlas_info, output_png):
    atlas = Image.new(
        "RGBA",
        (atlas_info["width"], atlas_info["height"]),
        (0, 0, 0, 0)
    )

    for p in atlas_info["placements"]:
        atlas.paste(
            p["texture"]["image"],
            (p["x"], p["y"])
        )

    os.makedirs(os.path.dirname(output_png), exist_ok=True)
    atlas.save(output_png)


def save_metadata(atlas_info, output_txt):
    with open(output_txt, "w", encoding="utf-8") as f:
        for p in atlas_info["placements"]:
            tex = p["texture"]

            f.write(
                f"{tex['name']} "
                f"{p['x']} {p['y']} "
                f"{tex['width']} {tex['height']}\n"
            )


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "map_name",
        help="Map filename, e.g. naijacave.xml"
    )

    parser.add_argument(
        "--root",
        default=".",
        help="Aquaria data root"
    )

    args = parser.parse_args()

    root_dir = os.path.abspath(args.root)

    maps_dir = os.path.join(root_dir, "data", "maps")
    tilesets_dir = os.path.join(root_dir, "data", "tilesets")
    gfx_dir = os.path.join(root_dir, "gfx")
    out_dir = os.path.join(gfx_dir, "tilesets")

    map_path = find_case_insensitive_path(
        maps_dir,
        args.map_name
    )

    if not map_path:
        raise RuntimeError(
            f"Map not found: {args.map_name}"
        )

    tileset_name = parse_map(map_path)
    tileset_name = tileset_name.lower()

    tileset_file = find_case_insensitive_path(
        tilesets_dir,
        tileset_name + ".txt"
    )

    if not tileset_file:
        raise RuntimeError(
            f"Tileset not found: {tileset_name}.txt"
        )

    print(f"Map: {os.path.basename(map_path)}")
    print(f"Tileset: {tileset_name}")

    texture_names = parse_tileset(tileset_file)

    textures = load_textures(texture_names, gfx_dir)

    if not textures:
        raise RuntimeError("No textures loaded")

    atlas_info = build_atlas(textures)

    atlas_png = os.path.join(
        out_dir,
        tileset_name + ".png"
    )

    atlas_meta = os.path.join(
        out_dir,
        tileset_name + ".txt"
    )

    save_atlas(atlas_info, atlas_png)
    save_metadata(atlas_info, atlas_meta)

    print(
        f"Atlas created: {atlas_png} "
        f"({atlas_info['width']}x{atlas_info['height']})"
    )

    print(f"Metadata: {atlas_meta}")


if __name__ == "__main__":
    main()

