# The FlightGear A320-family textures (GPL v2, see THIRD-PARTY-NOTICES) made plain for the game (owner 2026-11-28):
# white fuselage and wings, neutral light-grey fin, no registration, no airline stripes, no maker or oil-brand words.
# Panel lines, windows, doors and the safety placards stay. Writes Art/A320/Textures/FG/*.png (read by make_a320_fg.py).
#   python Art/A320/clean_fg_textures.py
import numpy as np
from PIL import Image, ImageFilter
from pathlib import Path

SRC = Path('F:/FlightGear-Aircraft/Airbus/A320-family/A320-family/Models/Fuselage/res')
OUT = Path(__file__).resolve().parent / 'Textures' / 'FG'
OUT.mkdir(parents=True, exist_ok=True)


def blur(a, r):
    # a box blur by running sums (rows, then columns)
    a = np.pad(a.astype(np.float64), r, mode='edge')
    for ax in (0, 1):
        c = np.cumsum(a, axis=ax); c = np.insert(c, 0, 0, axis=ax)
        n = a.shape[ax] - 2 * r
        a = (np.take(c, range(2 * r + 1, 2 * r + 1 + n), axis=ax) - np.take(c, range(0, n), axis=ax)) / (2 * r + 1)
        if ax == 0: a = a  # rows done; columns next
    return a.astype(np.float32)


def repaint(rgb, mask, white):
    # the painted areas become `white`, keeping their own shading (panel lines, seams) as a ratio to the local base
    lum = rgb.mean(axis=2)
    m = mask.astype(np.float32)
    base = blur(lum * m, 14) / np.maximum(blur(m, 14), 1e-3)
    ratio = np.clip(lum / np.maximum(base, 1.0), 0.0, 1.06)
    for c in range(3):
        rgb[..., c] = np.where(mask, white[c] * ratio, rgb[..., c])


def hsv(rgb):
    mx, mn = rgb.max(axis=2), rgb.min(axis=2)
    d = np.maximum(mx - mn, 1e-3)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    h = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    return h, (mx - mn) / np.maximum(mx, 1e-3), mx


def fuse():
    im = Image.open(SRC / 'Fuse-Main.png'); a = np.asarray(im).astype(np.float32)
    rgb, alpha = a[..., :3].copy(), a[..., 3:]
    # the tail fin's airline stripes (tan and black, two sides) -> plain neutral light grey
    fin = np.zeros(rgb.shape[:2], bool); fin[1765:2600, 3135:4030] = True
    h, s, v = hsv(rgb)
    finpaint = fin & ((s > 0.12) | (v < 60))
    rgb[finpaint] = (206, 208, 211)
    # the olive, tan and blue base colours -> white, shading kept
    h, s, v = hsv(rgb)
    paint = ((s > 0.10) & (((h > 20) & (h < 85) & (s < 0.62)) | ((h > 185) & (h < 235)))) & ~finpaint
    repaint(rgb, paint, (238, 238, 238))
    # the registrations (black on a white box, both sides): painted over with the white around them
    for x0, y0, x1, y1 in ((2560, 420, 3000, 540), (2560, 1230, 3000, 1360)):
        sub = rgb[y0:y1, x0:x1]; ys, xs = np.nonzero(sub.mean(axis=2) < 90)
        if len(xs):
            bx0, bx1, by0, by1 = x0 + xs.min() - 14, x0 + xs.max() + 14, y0 + ys.min() - 10, y0 + ys.max() + 10
            rgb[by0:by1, bx0:bx1] = rgb[by0 - 14, bx0 - 10]
            print('registration cleared', bx0, by0, bx1, by1)
    Image.fromarray(np.concatenate([np.clip(rgb, 0, 255), alpha], axis=2).astype(np.uint8), 'RGBA').save(OUT / 'Fuse-Main.png')


def plain_copy(name, rects=(), colour=(255, 255, 255)):
    im = Image.open(SRC / (name + '.png')); a = np.asarray(im).copy()
    for x0, y0, x1, y1 in rects:
        a[y0:y1, x0:x1, :3] = colour
    Image.fromarray(a, im.mode).save(OUT / (name + '.png'))


fuse()
# placards: the oil brand and the engine makers' headings blanked (the safety words stay)
plain_copy('placards', ((1700, 90, 1840, 150), (1530, 145, 1640, 190), (1530, 500, 1660, 550), (1530, 850, 1660, 895),
                        (1530, 1190, 1680, 1235)))
# the gear, the inside parts and the engine have no marks of a maker or an airline
for n in ('engine-CFM56', 'Static-Elements', 'LandingGears'):
    plain_copy(n)
print('DONE textures ->', OUT)
