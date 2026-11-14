# The airport sign's face (owner, 2026-11-14: "a sign which shows how far it is to the airport, on the side of the road";
# his references: the blue airport sign over the road at Venice, and the Faroese blue information signs on two posts).
# The name and the distance are NOT in the picture (owner: when the airport is copied to the United States or elsewhere
# the sign must change by itself): the game writes them at run time from Data/airfield.json (RoadSigns.cpp), with our
# own glyphs drawn here. Writes:
#   T_RoadSign_AirportBase.png  2048 x 1092 (the panel's 3.0 x 1.6 m): blue reflective sheeting, the white border, an
#                               aircraft in each top corner, the rule, the arrow up - weathered (sheeting grain, a faint
#                               fade, road grime rising from the bottom edge)
#   T_SignGlyphs.png            the letters (white, alpha = coverage), Droid Sans (Apache 2.0, shipped with Blender),
#   Data/sign-glyphs.json       and where each is and how wide it runs
# Layout (for the game, in the picture's pixels): name centred at y 140-330 between x 380 and 1668; the local word for
# airport + "Airport" centred at y 470-590; the distance at x 1184 (centre), y 770-960.
#   python Art/RoadSign/make_roadsign_texture.py
import json, math, os
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FONT = r'C:/Program Files/Blender Foundation/Blender 3.3/3.3/datafiles/fonts/droidsans.ttf'
W, H = 2048, 1092
BLUE, WHITE = (18, 74, 160), (246, 248, 250)
rng = np.random.default_rng(7)

def plane(d, cx, cy, s, ang):
    """A top-view airliner (our own outline), centred at cx, cy, s pixels long, its nose at ang degrees."""
    pts = [(0.50, 0.0), (0.46, 0.05), (0.14, 0.05), (-0.02, 0.50), (-0.10, 0.50), (0.0, 0.05), (-0.34, 0.05),
           (-0.44, 0.20), (-0.49, 0.20), (-0.44, 0.0)]
    full = pts + [(x, -y) for x, y in reversed(pts)]
    a = math.radians(ang); c, sn = math.cos(a), math.sin(a)
    d.polygon([(cx + (x * c - y * sn) * s, cy - (x * sn + y * c) * s) for x, y in full], fill=WHITE)
    d.ellipse((cx - s * 0.06, cy - s * 0.06, cx + s * 0.06, cy + s * 0.06), fill=WHITE)

def arrow_up(d, cx, top, h, w):
    d.polygon([(cx, top), (cx + w / 2, top + h * 0.45), (cx + w * 0.16, top + h * 0.45), (cx + w * 0.16, top + h),
               (cx - w * 0.16, top + h), (cx - w * 0.16, top + h * 0.45), (cx - w / 2, top + h * 0.45)], fill=WHITE)

# ---- the base face ----
im = Image.new('RGB', (W, H), BLUE); d = ImageDraw.Draw(im)
m, bw, rad = 26, 40, 70
d.rounded_rectangle((m, m, W - m, H - m), radius=rad, outline=WHITE, width=bw)
plane(d, 220, 215, 200, 30); plane(d, W - 220, 215, 200, 30)
d.rectangle((300, 680, W - 300, 690), fill=WHITE)
arrow_up(d, W / 2 - 330, 760, 230, 170)
a = np.asarray(im).astype(np.float32) / 255.0
grain = rng.normal(0, 0.012, (H, W, 1))
fade = np.asarray(Image.fromarray((rng.random((12, 22)) * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)).astype(np.float32)[..., None] / 255.0
y = np.linspace(0, 1, H)[:, None, None]
streak = np.asarray(Image.fromarray((rng.random((1, 160)) * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC)).astype(np.float32)[..., None] / 255.0
grime = np.clip((y - 0.72) / 0.28, 0, 1) ** 1.6 * (0.55 + 0.45 * streak) * 0.32
a = a * (0.97 + 0.05 * fade) + grain
a = a * (1 - grime) + np.array([0.33, 0.30, 0.25]) * grime
Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.6)).save(os.path.join(HERE, 'T_RoadSign_AirportBase.png'))
print('wrote the base face')

# ---- the glyphs: every letter an airport's name may need (Latin, the Nordic and most European accents) ----
SIZE, PAD, AW = 160, 10, 2048
chars = [chr(c) for c in range(32, 127)] + list('ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõöøùúûüýþÿŁłŃńŚśŹźŻżĄąĘęČčŠšŽžŘřĎďŤťŇňŮůĞğİıŞş·–’')
f = ImageFont.truetype(FONT, SIZE)
asc, desc = f.getmetrics()
lineh = asc + desc + 2 * PAD
x, row = 0, 0; glyphs = {}
atlas = Image.new('L', (AW, 4096), 0); ad = ImageDraw.Draw(atlas)
for ch in chars:
    l, t, r, b = ad.textbbox((0, 0), ch, font=f)
    adv = f.getlength(ch)
    w = int(math.ceil(max(r, adv))) + 2 * PAD
    if x + w > AW: x = 0; row += 1
    ad.text((x + PAD, row * lineh + PAD), ch, font=f, fill=255, stroke_width=2, stroke_fill=255)   # (a touch bolder: road-sign weight)
    glyphs[ch] = [x, row * lineh, w, lineh, round(adv + 2, 1)]
    x += w
used = (row + 1) * lineh
hgt = 1 << (used - 1).bit_length()
alpha = atlas.crop((0, 0, AW, hgt))
rgba = Image.merge('RGBA', (Image.new('L', alpha.size, 255),) * 3 + (alpha,))
rgba.save(os.path.join(HERE, 'T_SignGlyphs.png'))
json.dump({'source': 'Art/RoadSign/make_roadsign_texture.py', 'size': SIZE, 'pad': PAD, 'line': lineh, 'ascent': asc,
           'atlas': [AW, hgt], 'glyphs': glyphs}, open(os.path.join(ROOT, 'Data', 'sign-glyphs.json'), 'w', encoding='utf-8'), ensure_ascii=False)
print('wrote %d glyphs, atlas %d x %d' % (len(glyphs), AW, hgt))
