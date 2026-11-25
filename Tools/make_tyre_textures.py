# Draws the height maps of the vehicles' tyres (to-do 261): Art/Tyre/T_TyreTread.png (a tile of tread: u round the tyre,
# v across its width) and T_TyreSide.png (four strips of sidewall, one per size of tyre: u round the tyre, v from the
# shoulder at the top inwards). White is high. Makers and sizes are invented. Tools/set_vehicle_dirt_material.py imports
# them; the shader is TYRE in Tools/set_truck_materials.py.
#   python Tools/make_tyre_textures.py
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art', 'Tyre')
os.makedirs(OUT, exist_ok=True)


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


# ---- the tread: 512 x 512, whole tiles round ----
S = 512
u, v = np.meshgrid((np.arange(S) + 0.5) / S, (np.arange(S) + 0.5) / S)
tri = lambda x: np.abs((x % 1.0) * 2 - 1) * 2 - 1                # -1..1..-1
high = np.ones((S, S))


def groove(dist, half, depth, soft=0.006):
    # cut where the distance to the groove's middle (in tiles) is below half its width
    global high
    high = np.minimum(high, 1 - depth * (1 - smooth(half - soft, half + soft, np.abs(dist))))


ROWS = 6                                                         # blocks round a tile
for c, amp, ph in ((0.23, 0.008, 0.0), (0.41, 0.014, 0.25), (0.59, 0.014, 0.75), (0.77, 0.008, 0.5)):
    groove(v - c - amp * tri(u * ROWS + ph), 0.015, 1.0)         # the main grooves, slightly zigzag
for c0, c1, ph, slant, half, depth in ((0.23, 0.41, 0.15, 0.18, 0.011, 0.8), (0.41, 0.59, 0.6, -0.22, 0.008, 0.6), (0.59, 0.77, 0.35, 0.18, 0.011, 0.8),
                                       (-0.1, 0.17, 0.0, 0.1, 0.02, 1.0), (0.83, 1.1, 0.5, 0.1, 0.02, 1.0)):
    inside = (v > c0) & (v < c1)
    x = u * ROWS + ph + (v - c0) * slant * ROWS / 2
    d = np.abs((x % 1.0) - 0.5) / ROWS
    h0 = high.copy(); groove(d, half, depth); high = np.where(inside, high, h0)        # cuts between the blocks
    x2 = u * ROWS + ph + 0.5 + (v - c0) * slant * ROWS / 2 + 0.06 * tri(v * 22)
    d2 = np.abs((x2 % 1.0) - 0.5) / ROWS
    h0 = high.copy(); groove(d2, 0.0025, 0.4, 0.003); high = np.where(inside, high, h0)  # sipes
# the shoulder: a narrow slot along the edge of the running face
for c in (0.135, 0.865):
    groove(v - c, 0.005, 0.5, 0.004)
Image.fromarray((np.clip(high, 0, 1) * 255).astype(np.uint8), 'L').save(os.path.join(OUT, 'T_TyreTread.png'))

# ---- the sidewall: 1024 x 512, four strips of 128 ----
W, RH, SC = 1024, 128, 4


def font(px):
    for f in ('arialbd.ttf', 'DejaVuSans-Bold.ttf'):
        try:
            return ImageFont.truetype(f, px)
        except OSError:
            pass
    raise SystemExit('no bold font found')


def strip(size_text, load_text):
    im = Image.new('L', (W * SC, RH * SC), 102)
    d = ImageDraw.Draw(im)
    for y, hh, val in ((9, 4, 190), (18, 2, 150), (104, 3, 180), (112, 6, 200), (122, 3, 160)):   # ribs and the rim's line
        d.rectangle([0, y * SC, W * SC, (y + hh) * SC], fill=val)
    for x in range(0, W, 8):                                     # the fine ribbing under the shoulder
        d.rectangle([x * SC, 22 * SC, (x + 3) * SC, 27 * SC], fill=145)

    def put(text, x, y, px, wide=1.0, val=255):
        f = font(px * SC)
        box = f.getbbox(text)
        t = Image.new('L', (box[2] - box[0] + 8, box[3] - box[1] + 8), 0)
        ImageDraw.Draw(t).text((4 - box[0], 4 - box[1]), text, font=f, fill=255)
        t = t.resize((int(t.width * wide), t.height), Image.LANCZOS)
        im.paste(val, (int(x * SC - t.width / 2), int(y * SC - t.height / 2)), t)
    put('ADP RADIAL', 235, 65, 50)
    put(size_text, 715, 58, 34)
    put(load_text, 715, 91, 14, 1.25, 215)
    put('ADP', 478, 92, 13, 1.2, 200)
    return im.resize((W, RH), Image.LANCZOS)


side = Image.new('L', (W, RH * 4), 102)
for i, (a, b) in enumerate((('315/70 R22.5', 'TUBELESS  REGROOVABLE  APRON'), ('225/75 R17.5', 'TUBELESS  STEEL BELTED  APRON'),
                            ('6.00 R9', 'INDUSTRIAL  GROUND SUPPORT'), ('4.00-8', 'INDUSTRIAL  GROUND SUPPORT'))):
    side.paste(strip(a, b), (0, i * RH))
side.filter(ImageFilter.GaussianBlur(0.6)).save(os.path.join(OUT, 'T_TyreSide.png'))
print('drawn: %s' % os.path.abspath(OUT))
