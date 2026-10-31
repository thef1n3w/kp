#!/usr/bin/env python3
"""
Generate game/res/regions_mask.png for antivirus game.
Each country is painted with a unique color: R = region_index + 1, G=B=0.
Ocean = black (0,0,0).

Requirements: pip install Pillow
Run from project root: python generate_mask.py
"""

import json, os, sys, urllib.request

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Run: pip install Pillow"); sys.exit(1)

MASK_W, MASK_H = 2048, 1024
GEOJSON_CACHE = "ne_110m_countries.geojson"
GEOJSON_URL   = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_110m_admin_0_countries.geojson"
REGIONS_JSON  = "game/res/regions.json"
OUTPUT        = "game/res/regions_mask.png"

# Russian name → English names to try in Natural Earth
NAME_MAP = {
    "США":               ["United States of America"],
    "Канада":            ["Canada"],
    "Мексика":           ["Mexico"],
    "Куба":              ["Cuba"],
    "Бразилия":          ["Brazil"],
    "Аргентина":         ["Argentina"],
    "Колумбия":          ["Colombia"],
    "Великобритания":    ["United Kingdom"],
    "Франция":           ["France"],
    "Германия":          ["Germany"],
    "Испания":           ["Spain"],
    "Италия":            ["Italy"],
    "Польша":            ["Poland"],
    "Украина":           ["Ukraine"],
    "Россия":            ["Russia"],
    "Турция":            ["Turkey"],
    "Египет":            ["Egypt"],
    "Алжир":             ["Algeria"],
    "Нигерия":           ["Nigeria"],
    "Конго":             ["Democratic Republic of the Congo", "Dem. Rep. Congo", "Congo"],
    "Эфиопия":           ["Ethiopia"],
    "ЮАР":               ["South Africa"],
    "Кения":             ["Kenya"],
    "Ирак":              ["Iraq"],
    "Саудовская Аравия": ["Saudi Arabia"],
    "Иран":              ["Iran"],
    "Казахстан":         ["Kazakhstan"],
    "Афганистан":        ["Afghanistan"],
    "Пакистан":          ["Pakistan"],
    "Индия":             ["India"],
    "Китай":             ["China"],
    "Япония":            ["Japan"],
    "Южная Корея":       ["South Korea", "Korea"],
    "Вьетнам":           ["Vietnam"],
    "Таиланд":           ["Thailand"],
    "Индонезия":         ["Indonesia"],
    "Австралия":         ["Australia"],
    "Новая Зеландия":    ["New Zealand"],
}

def to_px(lon, lat):
    return ((lon + 180) / 360 * MASK_W, (90 - lat) / 180 * MASK_H)

def ring_to_pixels(ring):
    return [to_px(c[0], c[1]) for c in ring]

def draw_geom(draw, geom, color):
    t = geom["type"]
    if t == "Polygon":
        polys = [geom["coordinates"]]
    elif t == "MultiPolygon":
        polys = geom["coordinates"]
    else:
        return
    for poly in polys:
        exterior = ring_to_pixels(poly[0])
        draw.polygon(exterior, fill=color)
        for hole in poly[1:]:
            draw.polygon(ring_to_pixels(hole), fill=(0, 0, 0))

def main():
    with open(REGIONS_JSON, encoding="utf-8") as f:
        regions = json.load(f)["regions"]
    print(f"Regions: {len(regions)}")

    if not os.path.exists(GEOJSON_CACHE):
        print("Downloading Natural Earth GeoJSON...")
        urllib.request.urlretrieve(GEOJSON_URL, GEOJSON_CACHE)
        print("Downloaded.")

    with open(GEOJSON_CACHE, encoding="utf-8") as f:
        features = json.load(f)["features"]

    # Build lookup by multiple name fields
    name_lookup = {}
    for feat in features:
        p = feat["properties"]
        for key in ("NAME", "NAME_EN", "ADMIN", "SOVEREIGNT", "NAME_LONG"):
            n = p.get(key, "")
            if n:
                name_lookup.setdefault(n, feat)
        # Russian name field
        n_ru = p.get("NAME_RU", "")
        if n_ru:
            name_lookup.setdefault(n_ru, feat)

    img = Image.new("RGB", (MASK_W, MASK_H), (0, 0, 0))
    draw = ImageDraw.Draw(img)

    matched, missing = [], []
    for i, reg in enumerate(regions):
        name = reg["name"]
        color = (i + 1, 0, 0)
        feat = name_lookup.get(name)
        if not feat:
            for en in NAME_MAP.get(name, []):
                feat = name_lookup.get(en)
                if feat: break
        if feat and feat.get("geometry"):
            draw_geom(draw, feat["geometry"], color)
            matched.append(name)
        else:
            missing.append(name)
            print(f"  MISSING: {name}")

    img.save(OUTPUT)
    print(f"\nSaved: {OUTPUT}")
    print(f"Matched {len(matched)}/{len(regions)}, missing: {len(missing)}")

if __name__ == "__main__":
    main()
