# The second batch of tattoo designs (owner, 2026-11-15: 50 per kind of place, our own designs, colour and black-and-grey
# mixed, nothing demonic, devilish or sexual). Same frames and helpers as designs_batch1.py: arm 1024x2048 (40 px/cm down,
# shoulder 0 -> knuckles 1, elbow 0.30, wrist 0.81, outer side at u 0.5), leg 1024x2048 (~28 px/cm round, 24 down; hip 0,
# knee 0.29, ankle 0.78), neck 1024x1024 (114 px/cm), fingers 1024x256 (four quarters; 80 px/cm across, 57 along).
import math
import numpy as np
from designs_batch1 import new, done, P, line, ellipse, poly, wrap, INK

GREY = lambda g, a=255: (g, g, g + 6, a)


def bez(p0, p1, p2, n=40):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0], (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in np.linspace(0, 1, n)]


# ------------------------------------------------------------------------------------------------ arms
def anchor_forearm():
    """Black and grey: a ship's anchor with a rope coiled round its shank, on the outer forearm."""
    img, d = new(1024, 2048); k = 40.0; cx = 512; top = 0.42 * 2048
    def q(x, y): return (cx + x * k, top + y * k)
    ellipse(d, *q(0, 0), 0.75 * k, 0.75 * k, outline=INK, w=0.32 * k)                # the ring
    poly(d, [q(-0.28, 0.7), q(0.28, 0.7), q(0.28, 8.6), q(-0.28, 8.6)], fill=GREY(70), outline=INK, w=0.08 * k)    # shank
    poly(d, [q(-2.2, 1.9), q(2.2, 1.9), q(2.2, 2.5), q(-2.2, 2.5)], fill=GREY(70), outline=INK, w=0.08 * k)        # stock
    for s in (-1, 1):                                                                  # the arms and flukes
        arc = [q(s * 3.2 * math.sin(t), 8.6 - 3.0 * (1 - math.cos(t)) * 0.0 - 1.4 * math.sin(t) * 0) for t in np.linspace(0, 1.25, 20)]
        arc = [q(s * 3.0 * math.sin(t), 8.6 - 2.6 * (1 - math.cos(t))) for t in np.linspace(0, 1.3, 24)]
        line(d, arc, 0.55 * k); line(d, arc, 0.32 * k, GREY(80))
        tx, ty = s * 3.0 * math.sin(1.3), 8.6 - 2.6 * (1 - math.cos(1.3))
        poly(d, [q(tx - s * 0.2, ty + 0.5), q(tx + s * 0.9, ty - 0.9), q(tx - s * 0.6, ty - 0.2)], fill=INK)
    for i in range(7):                                                                 # the rope, crossing the shank
        y = 2.9 + i * 0.75
        line(d, bez(q(-0.9, y), q(0, y + 0.55), q(0.9, y + 0.15)), 0.22 * k)
        line(d, bez(q(-0.9, y), q(0, y + 0.55), q(0.9, y + 0.15)), 0.1 * k, GREY(150))
    line(d, bez(q(0.9, 8.0), q(2.6, 9.6), q(1.4, 11.2)), 0.22 * k)
    return done(img)


def feather_forearm():
    """Black and grey: one long feather down the outer forearm, its barbs fine lines, a few split open."""
    img, d = new(1024, 2048); k = 40.0
    y0, y1 = 0.36 * 2048, 0.78 * 2048
    spine = [(512 + 30 * math.sin((y - y0) / (y1 - y0) * math.pi) , y) for y in np.linspace(y0, y1, 80)]
    rng = np.random.default_rng(5)
    for i, (x, y) in enumerate(spine[2:-12]):
        t = i / len(spine)
        L = (math.sin(min(1, t * 1.25) * math.pi) ** 0.6) * 1.6 * k + 0.2 * k
        for s in (-1, 1):
            gap = rng.random() < 0.06
            if gap: continue
            tip = (x + s * L, y - L * 0.55)
            g = int(35 + 70 * rng.random())
            line(d, bez((x, y), (x + s * L * 0.5, y - L * 0.05), tip, 12), 0.07 * k, GREY(g, 235))
    line(d, spine, 0.12 * k)
    line(d, spine[-14:], 0.16 * k)
    return done(img)


def koi_upperarm():
    """Colour: an orange and white koi swimming round the outer upper arm among blue water swirls."""
    img, d = new(1024, 2048); k = 40.0; cx, cy = 512, 0.15 * 2048
    for j in range(5):                                                                 # the water
        yy = cy - 3.5 * k + j * 1.7 * k
        pts = [(x, yy + 0.35 * k * math.sin(x / 60 + j)) for x in np.linspace(cx - 5 * k, cx + 5 * k, 60)]
        line(d, pts, 0.18 * k, (40, 110, 175, 230))
    def q(x, y): return (cx + x * k, cy + y * k)
    body = [q(-3.4, 0), q(-2.2, -0.9), q(0, -1.3), q(2.0, -1.0), q(3.1, -0.3), q(3.3, 0.2), q(2.4, 0.9), q(0, 1.2), q(-2.2, 0.7)]
    tail = [q(-3.2, 0), q(-5.0, -1.6), q(-4.4, 0), q(-5.0, 1.6)]
    poly(d, tail, fill=(236, 120, 40, 255), outline=INK, w=0.09 * k)
    for s in (-1, 1): poly(d, [q(0.6, s * 1.0), q(-0.6, s * 2.3), q(-0.8, s * 1.1)], fill=(240, 150, 70, 255), outline=INK, w=0.07 * k)
    poly(d, body, fill=(245, 240, 232, 255), outline=INK, w=0.1 * k)
    for (x, y, rx, ry) in ((1.6, -0.2, 1.1, 0.75), (-0.4, 0.2, 1.0, 0.7), (-2.2, -0.1, 0.6, 0.4)):
        ellipse(d, *q(x, y), rx * k, ry * k, fill=(232, 96, 30, 255))
    for i in range(6):                                                                 # scale arcs
        for j in (-1, 0, 1):
            x, y = -2.0 + i * 0.7, j * 0.5
            line(d, bez(q(x, y - 0.25), q(x + 0.25, y), q(x, y + 0.25), 8), 0.04 * k, (90, 40, 20, 160))
    ellipse(d, *q(2.6, -0.3), 0.14 * k, 0.14 * k, fill=INK)
    line(d, bez(q(3.2, 0.3), q(3.8, 0.9), q(3.5, 1.4), 10), 0.05 * k)                  # the barbel
    return done(img)


def mountains_band():
    """Black and grey: a band of sharp mountain ridges round the forearm, snow lines in bare skin, seamless."""
    img, d = new(1024, 2048); W = 1024; base = 0.70 * 2048
    rng = np.random.default_rng(21); peaks = [(rng.random(), 1.6 + 1.8 * rng.random()) for _ in range(9)]
    def band(o):
        line(d, [(o, base + 6), (o + W, base + 6)], 5)
        for t, h in sorted(peaks, key=lambda p: -p[1]):
            x = o + t * W; hh = h * 40; w = hh * 1.1
            g = int(40 + 50 * (h - 1.6) / 1.8)
            poly(d, [(x - w, base), (x, base - hh), (x + w, base)], fill=GREY(g), outline=INK, w=3)
            poly(d, [(x, base - hh), (x + w * 0.22, base - hh * 0.78), (x + w * 0.06, base - hh * 0.72), (x - w * 0.12, base - hh * 0.82)], fill=(0, 0, 0, 0))
    wrap(d, band, W)
    return done(img)


def puffin_forearm():
    """Colour: a puffin standing on a rock, its striped beak bright, on the inner-to-outer forearm."""
    img, d = new(1024, 2048); k = 42.0; cx, cy = 512, 0.58 * 2048
    def q(x, y): return (cx + x * k, cy + y * k)
    poly(d, [q(-2.6, 3.2), q(-1.6, 2.4), q(1.4, 2.3), q(2.8, 3.3)], fill=(90, 86, 80, 255), outline=INK, w=0.08 * k)
    body = [q(-1.0, -1.4), q(0.4, -1.6), q(1.2, -0.6), q(1.3, 1.0), q(0.6, 2.4), q(-0.8, 2.4), q(-1.4, 1.0)]
    poly(d, body, fill=(28, 28, 34, 255), outline=INK, w=0.08 * k)
    poly(d, [q(0.15, -0.6), q(1.1, -0.4), q(1.2, 1.0), q(0.5, 2.2), q(-0.2, 1.6), q(-0.1, 0.0)], fill=(246, 244, 238, 255))   # the white breast
    ellipse(d, *q(0.0, -2.2), 1.05 * k, 0.95 * k, fill=(28, 28, 34, 255), outline=INK, w=0.08 * k)
    ellipse(d, *q(0.25, -2.15), 0.7 * k, 0.62 * k, fill=(236, 236, 230, 255))                                                  # face
    beak = [q(0.75, -2.6), q(2.0, -2.25), q(2.1, -1.95), q(0.75, -1.65)]
    poly(d, beak, fill=(236, 110, 30, 255), outline=INK, w=0.07 * k)
    poly(d, [q(0.75, -2.6), q(1.15, -2.48), q(1.15, -1.78), q(0.75, -1.65)], fill=(60, 120, 170, 255))
    line(d, [q(1.45, -2.38), q(1.45, -1.85)], 0.06 * k, (250, 220, 120, 255))
    ellipse(d, *q(0.45, -2.3), 0.12 * k, 0.12 * k, fill=INK); line(d, [q(0.32, -2.1), q(0.6, -2.0)], 0.04 * k, (220, 80, 60, 255))
    for x in (-0.4, 0.3): line(d, [q(x, 2.3), q(x, 2.7)], 0.12 * k, (236, 110, 30, 255))
    return done(img)


def plane_path_forearm():
    """Black and grey: a small airliner climbing away up the forearm, a dotted flight path looping from the wrist."""
    img, d = new(1024, 2048); k = 40.0
    path = [(512 + 70 * math.sin(t * 2.2) + 40 * t, 0.78 * 2048 - t * 0.36 * 2048) for t in np.linspace(0, 1, 46)]
    for i, (x, y) in enumerate(path[:-3]):
        if i % 2 == 0: ellipse(d, x, y, 0.1 * k, 0.1 * k, fill=INK)
    x, y = path[-1]; a = math.atan2(path[-1][1] - path[-3][1], path[-1][0] - path[-3][0])
    def q(u, v): return (x + (u * math.cos(a) - v * math.sin(a)) * k, y + (u * math.sin(a) + v * math.cos(a)) * k)
    poly(d, [q(-1.8, -0.18), q(1.3, -0.2), q(1.9, 0), q(1.3, 0.2), q(-1.8, 0.18)], fill=GREY(60), outline=INK, w=0.06 * k)    # fuselage
    for s in (-1, 1):
        poly(d, [q(0.3, s * 0.15), q(-0.6, s * 1.9), q(-0.95, s * 1.9), q(-0.4, s * 0.15)], fill=GREY(90), outline=INK, w=0.06 * k)
        poly(d, [q(-1.4, s * 0.12), q(-1.9, s * 0.75), q(-2.05, s * 0.75), q(-1.8, s * 0.12)], fill=GREY(90), outline=INK, w=0.05 * k)
    return done(img)


# ------------------------------------------------------------------------------------------------ legs
def wave_calf():
    """Colour: a great curling ocean wave with white foam fingers, in a rounded frame on the outer calf."""
    img, d = new(1024, 2048); kx, ky = 27.7, 24.0; cx, cy = 512, 0.55 * 2048
    def q(x, y): return (cx + x * kx, cy + y * ky)
    poly(d, [q(-5, -5), q(5, -5), q(5, 5), q(-5, 5)], fill=(248, 232, 196, 255))
    ellipse(d, *q(2.6, -2.8), 1.1 * kx, 1.1 * ky, fill=(214, 70, 50, 255))                              # the sun
    crest = [q(-5, 5)] + [q(-5 + 7.5 * t, 5 - 9.0 * math.sin(t * math.pi * 0.62)) for t in np.linspace(0, 1, 30)] + [q(3.0, -1.6), q(2.4, -0.4), q(3.6, 1.6), q(5, 2.6), q(5, 5)]
    poly(d, crest, fill=(32, 84, 150, 255), outline=INK, w=4)
    for j in range(4):
        r = 2.4 - j * 0.5
        arc = [q(0.6 + r * math.cos(t), -1.8 + r * math.sin(t)) for t in np.linspace(math.pi * 0.9, math.pi * 2.3, 30)]
        line(d, arc, 3.2, (210, 232, 246, 255) if j % 2 == 0 else INK)
    for i in range(7):                                                                                   # foam fingers
        t = 0.45 + i * 0.07
        x0, y0 = -5 + 7.5 * t, 5 - 9.0 * math.sin(t * math.pi * 0.62)
        line(d, bez((cx + x0 * kx, cy + y0 * ky), (cx + (x0 + 0.5) * kx, cy + (y0 - 0.9) * ky), (cx + (x0 + 1.0) * kx, cy + (y0 - 0.3) * ky), 10), 5, (246, 250, 252, 255))
    poly(d, [q(-5, -5), q(5, -5), q(5, 5), q(-5, 5)], outline=INK, w=7)
    return done(img)


def whale_tail_calf():
    """Black and grey: a whale's tail rising out of the sea, drops falling from it, on the outer calf."""
    img, d = new(1024, 2048); kx, ky = 27.7, 24.0; cx, cy = 512, 0.50 * 2048
    def q(x, y): return (cx + x * kx, cy + y * ky)
    fluke = [q(-1.0, 2.3), q(-0.6, 0.7), q(-1.3, -0.4), q(-2.6, -1.1), q(-3.9, -2.4), q(-3.3, -0.9), q(-2.2, -0.1), q(-0.9, -0.1), q(0, -0.7),
             q(0.9, -0.1), q(2.2, -0.1), q(3.3, -0.9), q(3.9, -2.4), q(2.6, -1.1), q(1.3, -0.4), q(0.6, 0.7), q(1.0, 2.3)]
    poly(d, fluke, fill=GREY(52), outline=INK, w=4)
    line(d, [q(0, -0.5), q(0, 1.6)], 2.5, GREY(110))
    for j in range(3):
        yy = 2.2 + j * 0.8
        line(d, [q(-4 + 0.6 * i + 0.3 * j, yy + 0.2 * math.sin(i * 1.7)) for i in range(14)], 4, INK)
    for (x, y) in ((-3.4, -1.6), (-3.0, -0.6), (3.5, -1.4), (3.1, -0.3), (-2.0, 0.4)):
        poly(d, [q(x, y - 0.35), q(x + 0.14, y), q(x, y + 0.12), q(x - 0.14, y)], fill=GREY(80))
    return done(img)


def sun_thigh():
    """Black and grey: a geometric sun - dotwork disc, long and short rays - on the outer thigh."""
    img, d = new(1024, 2048); k = 25.0; cx, cy = 512, 0.15 * 2048
    for i in range(24):
        a = i * math.pi / 12; L = (4.6 if i % 2 == 0 else 3.4) * k
        poly(d, [(cx + math.cos(a - 0.07) * 2.0 * k, cy + math.sin(a - 0.07) * 2.0 * k), (cx + math.cos(a) * L, cy + math.sin(a) * L),
                 (cx + math.cos(a + 0.07) * 2.0 * k, cy + math.sin(a + 0.07) * 2.0 * k)], fill=INK if i % 2 == 0 else GREY(110))
    ellipse(d, cx, cy, 1.85 * k, 1.85 * k, fill=(0, 0, 0, 0), outline=INK, w=5)
    rng = np.random.default_rng(8)
    for _ in range(900):                                                                                  # dotwork, denser at the rim
        r = 1.75 * k * math.sqrt(rng.random()) ** 0.35; a = rng.random() * 2 * math.pi
        ellipse(d, cx + r * math.cos(a), cy + r * math.sin(a), 1.4, 1.4, fill=INK)
    ellipse(d, cx, cy, 0.7 * k, 0.7 * k, outline=INK, w=4)
    return done(img)


# ------------------------------------------------------------------------------------------------ neck
def wings_neck():
    """Colour: a pair of golden pilot's wings with a small blue globe in the middle."""
    img, d = new(1024, 1024); cx = cy = 512; k = 114.0
    def q(x, y): return (cx + x * k, cy + y * k)
    for s in (-1, 1):
        for j in range(5):                                                                                # feathers, longest on top
            y = -0.55 + j * 0.28; L = 3.6 - j * 0.45
            poly(d, [q(s * 0.55, y), q(s * L, y - 0.18 + j * 0.02), q(s * (L - 0.25), y + 0.22), q(s * 0.55, y + 0.26)], fill=(214, 168, 52, 255) if j % 2 == 0 else (238, 196, 84, 255), outline=INK, w=0.035 * k)
    ellipse(d, cx, cy, 0.62 * k, 0.62 * k, fill=(40, 96, 170, 255), outline=INK, w=0.05 * k)
    for t in (-0.3, 0, 0.3): line(d, [q(-0.6, t), q(0.6, t)], 0.03 * k, (220, 236, 250, 255))
    ellipse(d, cx, cy, 0.25 * k, 0.6 * k, outline=(220, 236, 250, 255), w=0.03 * k)
    return done(img)


def moon_phases_neck():
    """Black and grey: five moon phases in a row, new moon to full and back, small dots between."""
    img, d = new(1024, 1024); k = 114.0; cy = 512
    for i, f in enumerate((-0.6, -0.25, 0, 0.25, 0.6)):
        cx = 512 + (i - 2) * 1.55 * k; r = 0.55 * k
        ellipse(d, cx, cy, r, r, fill=INK)
        if f != 0:
            ellipse(d, cx + (r * 2 - abs(f) * 2.2 * r) * (1 if f > 0 else -1) * 0.7, cy, r * 1.02, r * 1.02, fill=(0, 0, 0, 0))
        else:
            ellipse(d, cx, cy, r, r, outline=INK, w=5); ellipse(d, cx, cy, r * 0.92, r * 0.92, fill=GREY(120))
        if i < 4: ellipse(d, cx + 0.78 * k, cy, 4, 4, fill=INK)
    return done(img)


# ------------------------------------------------------------------------------------------------ fingers
def finger_dots():
    """Black and grey: a short line and three dots on each finger, stepping down from index to little finger."""
    img, d = new(1024, 256)
    for i in range(4):
        cx = 128 + i * 256
        line(d, [(cx - 34, 70), (cx + 34, 70)], 5)
        for j in range(3 - (i // 2)):
            ellipse(d, cx + (j - (2 - i // 2) / 2) * 26, 118, 7, 7, fill=INK)
    return done(img)


BATCH = [  # (id, name, kind, colour, draw)
    ('anchor_forearm', 'Anchor with rope (forearm)', 'arm', False, anchor_forearm),
    ('feather_forearm', 'Long feather (forearm)', 'arm', False, feather_forearm),
    ('koi_upperarm', 'Koi in the water (upper arm)', 'arm', True, koi_upperarm),
    ('mountains_band', 'Mountain ridge band (forearm)', 'arm', False, mountains_band),
    ('puffin_forearm', 'Puffin on a rock (forearm)', 'arm', True, puffin_forearm),
    ('plane_path_forearm', 'Airliner and flight path (forearm)', 'arm', False, plane_path_forearm),
    ('wave_calf', 'Great wave (calf)', 'leg', True, wave_calf),
    ('whale_tail_calf', 'Whale tail (calf)', 'leg', False, whale_tail_calf),
    ('sun_thigh', 'Dotwork sun (thigh)', 'leg', False, sun_thigh),
    ('wings_neck', 'Pilot wings', 'neck', True, wings_neck),
    ('moon_phases_neck', 'Moon phases', 'neck', False, moon_phases_neck),
    ('finger_dots', 'Line and dots', 'fingers', False, finger_dots),
]
