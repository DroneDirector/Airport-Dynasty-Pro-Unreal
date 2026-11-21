"""Our own leaf and needle textures for the trees (to-do 200, 2026-11-21: the crowns read as foliage up close). Drawn in
grey tones - the game tints them by season (SiteTrees.cpp) - from what beech leaves and spruce shoots look like in
reference photos: ovate beech leaves with a midrib, overlapping at random angles, dark gaps between them; spruce shoots
as dense short needle strokes along twigs.
  python -I Art/Trees/make_tree_textures.py  ->  Art/Trees/Textures/T_Leaves.png, T_Needles.png, T_BarkBeech.png, T_BarkConifer.png"""
import math, random
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter
OUT = Path(__file__).resolve().parent / 'Textures'; OUT.mkdir(exist_ok=True)
S = 1024
random.seed(7)

def leaves():
    im = Image.new('L', (S, S), 38); d = ImageDraw.Draw(im)
    for _ in range(2600):
        x, y = random.uniform(-40, S + 40), random.uniform(-40, S + 40)
        L = random.uniform(34, 58); W = L * random.uniform(0.48, 0.6); a = random.uniform(0, math.pi * 2)
        g = int(random.uniform(150, 245))
        pts = []
        for k in range(24):
            t = k / 23 * math.pi * 2
            # ovate: wider near the base, a pointed tip, a slightly wavy edge (beech)
            r = (1 + 0.25 * math.cos(t)) * (1 + 0.05 * math.sin(7 * t))
            px, py = math.cos(t) * L * 0.5 * r, math.sin(t) * W * 0.5 * (1 - 0.35 * max(0, math.cos(t)))
            pts.append((x + px * math.cos(a) - py * math.sin(a), y + px * math.sin(a) + py * math.cos(a)))
        d.polygon(pts, fill=g)
        # the midrib and a shade on one half
        d.line([(x - math.cos(a) * L * 0.45, y - math.sin(a) * L * 0.45), (x + math.cos(a) * L * 0.5, y + math.sin(a) * L * 0.5)], fill=max(0, g - 45), width=2)
    im = im.filter(ImageFilter.GaussianBlur(0.6))
    return im

def needles():
    im = Image.new('L', (S, S), 30); d = ImageDraw.Draw(im)
    for _ in range(420):
        x, y = random.uniform(0, S), random.uniform(0, S); a = random.uniform(-0.6, 0.6) + math.pi / 2; L = random.uniform(60, 140)
        for k in range(int(L / 3)):
            t = k * 3; cx, cy = x + math.cos(a) * t, y + math.sin(a) * t
            for side in (-1, 1):
                b = a + side * random.uniform(0.7, 1.1); n = random.uniform(9, 15); g = int(random.uniform(120, 220))
                d.line([(cx, cy), (cx + math.cos(b) * n, cy + math.sin(b) * n)], fill=g, width=2)
        d.line([(x, y), (x + math.cos(a) * L, y + math.sin(a) * L)], fill=70, width=3)
    return im.filter(ImageFilter.GaussianBlur(0.5))

def bark(smooth):
    im = Image.new('L', (S, S), 128); px = im.load()
    for y in range(S):
        for x in range(S):
            v = 128 + 40 * math.sin(x * (0.02 if smooth else 0.09) + 3 * math.sin(y * 0.013)) + random.uniform(-18, 18) * (0.4 if smooth else 1)
            if not smooth and (x + int(20 * math.sin(y * 0.02))) % 37 < 4: v -= 55   # (fissures in spruce bark)
            px[x, y] = max(0, min(255, int(v)))
    return im.filter(ImageFilter.GaussianBlur(1.2 if smooth else 0.8))

for name, im in (('T_Leaves', leaves()), ('T_Needles', needles()), ('T_BarkBeech', bark(True)), ('T_BarkConifer', bark(False))):
    im.convert('RGB').save(OUT / (name + '.png')); print(name)
