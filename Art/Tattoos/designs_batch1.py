# The first timed batch of tattoo designs (owner, 2026-11-15: 50 per kind of place - arms, legs, neck, fingers - our own
# designs, colour and black-and-grey mixed, nothing demonic, devilish or sexual; a timed first batch of 10 to measure the
# hours). Drawn here in each place's own frame (make_designs.py): sizes in real cm from the place maps
# (Art/Tattoos/Placed/Spots/spots.json): arm 40 px/cm (shoulder 0 -> knuckles 1, elbow 0.30, wrist 0.81, the outer side
# at u 0.5), leg 24 px/cm down and ~28 round the calf (hip 0, knee 0.29, ankle 0.78), neck 114 px/cm (a 9 cm square),
# fingers 80 px/cm across and 57 along (four quarters, knuckle to the last joint - never the nail).
import math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

SS = 2                                                             # (drawn at twice the size, then smoothed down)
INK = (22, 24, 30, 255)


def new(w, h): img = Image.new('RGBA', (w * SS, h * SS), (0, 0, 0, 0)); return img, ImageDraw.Draw(img)


def done(img):
    w, h = img.size
    return img.filter(ImageFilter.GaussianBlur(SS * 0.45)).resize((w // SS, h // SS), Image.LANCZOS)


def P(x, y): return (x * SS, y * SS)


def line(d, pts, w, c=INK): d.line([P(*p) for p in pts], fill=c, width=int(w * SS), joint='curve')


def ellipse(d, cx, cy, rx, ry, fill=None, outline=None, w=1):
    d.ellipse([P(cx - rx, cy - ry), P(cx + rx, cy + ry)], fill=fill, outline=outline, width=int(w * SS))


def poly(d, pts, fill=None, outline=None, w=1):
    d.polygon([P(*p) for p in pts], fill=fill, outline=outline, width=int(w * SS) if outline else 0)


def wrap(d, fn, W):
    """Draw fn(offset) three times (-W, 0, +W) so a band round a limb meets itself seamlessly."""
    for o in (-W, 0, W): fn(o)


# ------------------------------------------------------------------------------------------------ arms (1024 x 2048)
def fern_forearm():
    """Black and grey: three fern fronds rising from the wrist along the outer forearm to below the elbow."""
    img, d = new(1024, 2048); k = 40.0                            # px per cm
    rng = np.random.default_rng(3)
    for off, lean, top in ((-70, -0.10, 0.36), (0, 0.0, 0.33), (75, 0.12, 0.40)):
        y0 = 0.79 * 2048; y1 = top * 2048; n = 26
        stem = [(512 + off + lean * (y0 - y) + 18 * math.sin((y0 - y) / 260), y) for y in np.linspace(y0, y1, 60)]
        line(d, stem, 0.09 * k)
        for i in range(n):
            t = i / n; y = y0 + (y1 - y0) * (0.06 + 0.92 * t)
            x = 512 + off + lean * (y0 - y) + 18 * math.sin((y0 - y) / 260)
            L = (1 - t) ** 0.8 * 2.2 * k + 0.25 * k
            for sgn in (-1, 1):
                ang = math.radians(55 + 10 * t) * sgn
                tip = (x + sgn * L * math.sin(abs(ang)), y - L * math.cos(abs(ang)) * 0.55)
                mid = ((x + tip[0]) / 2, (y + tip[1]) / 2)
                # a leaflet: a slim leaf shape along stem -> tip, its own shade of grey
                nx, ny = tip[1] - y, -(tip[0] - x); nl = math.hypot(nx, ny) or 1; nx, ny = nx / nl * L * 0.16, ny / nl * L * 0.16
                g = int(40 + 50 * rng.random())
                poly(d, [(x, y), (mid[0] + nx, mid[1] + ny), tip, (mid[0] - nx, mid[1] - ny)], fill=(g, g, g + 6, 225), outline=INK, w=0.05 * k)
    return done(img)


def rose_upperarm():
    """Colour: a red rose in bloom with two leaves on the outer upper arm, black outline, soft shading."""
    img, d = new(1024, 2048); k = 108.0                         # (a rose about 9 cm across - 36 made it 3 cm)
    cx, cy = 512, 0.17 * 2048
    for lx, ly, rot in ((-1, 1, -0.6), (1, 1, 0.5)):              # leaves first (behind)
        a = rot; L = 3.2 * k
        tip = (cx + lx * L * math.cos(a) * 1.2, cy + ly * L * 0.75)
        base = (cx + lx * 1.4 * k, cy + 1.2 * k)
        mid = ((base[0] + tip[0]) / 2, (base[1] + tip[1]) / 2); nx, ny = -(tip[1] - base[1]) * 0.28, (tip[0] - base[0]) * 0.28
        poly(d, [base, (mid[0] + nx, mid[1] + ny), tip, (mid[0] - nx, mid[1] - ny)], fill=(46, 120, 60, 255), outline=INK, w=0.07 * k)
        line(d, [base, tip], 0.04 * k, (24, 60, 30, 255))
    # the bloom: rings of petals, darker toward the heart, each petal outlined
    for ring, (R, n, col) in enumerate(((2.6, 7, (196, 34, 48)), (1.9, 6, (172, 22, 40)), (1.25, 5, (146, 14, 32)), (0.65, 4, (110, 8, 24)))):
        for i in range(n):
            a = 2 * math.pi * i / n + ring * 0.45
            px, py = cx + math.cos(a) * R * k * 0.45, cy + math.sin(a) * R * k * 0.38
            ellipse(d, px, py, R * k * 0.55, R * k * 0.42, fill=col + (255,), outline=INK, w=0.06 * k)
    # light catching the petals' edges
    for i in range(7):
        a = 2 * math.pi * i / 7
        line(d, [(cx + math.cos(a) * 1.7 * k, cy + math.sin(a) * 1.3 * k), (cx + math.cos(a + 0.4) * 2.1 * k, cy + math.sin(a + 0.4) * 1.6 * k)], 0.08 * k, (240, 120, 120, 200))
    return done(img)


def mandala_band():
    """Black and grey: a geometric band round the upper arm - triangles, a dot row and a scalloped edge, seamless."""
    img, d = new(1024, 2048); W = 1024
    y0, y1 = 0.205 * 2048, 0.27 * 2048; h = y1 - y0; mid = (y0 + y1) / 2
    def band(o):
        line(d, [(o, y0), (o + W, y0)], 6); line(d, [(o, y1), (o + W, y1)], 6)
        n = 16
        for i in range(n):
            x0 = o + i * W / n; x1 = x0 + W / n; xm = (x0 + x1) / 2
            poly(d, [(x0, mid - h * 0.05), (xm, y0 + h * 0.12), (x1, mid - h * 0.05)], fill=INK if i % 2 == 0 else None, outline=INK, w=3)
            poly(d, [(x0, mid + h * 0.05), (xm, y1 - h * 0.12), (x1, mid + h * 0.05)], fill=None if i % 2 == 0 else INK, outline=INK, w=3)
            ellipse(d, xm, mid, 5, 5, fill=INK)
            ellipse(d, xm, y0 - 14, W / n * 0.45, 12, outline=INK, w=3)      # (the scallops above)
            ellipse(d, xm, y1 + 14, W / n * 0.45, 12, outline=INK, w=3)
    wrap(d, band, W)
    return done(img)


# ------------------------------------------------------------------------------------------------ legs (1024 x 2048)
def lighthouse_calf():
    """Colour: a lighthouse on a rock in a round frame - sea, sky and its beam - on the outer calf."""
    img, d = new(1024, 2048); kx, ky = 27.7, 24.0               # px per cm round / down the calf
    cx, cy = 512, 0.56 * 2048; R = 5.5                            # cm
    def sx(c): return cx + c * kx
    def sy(c): return cy + c * ky
    # sky (a soft gradient disc) and sea
    for i in range(30, 0, -1):
        r = R * i / 30; t = i / 30
        ellipse(d, cx, cy, r * kx, r * ky, fill=(int(250 - 70 * t), int(200 - 40 * t), int(120 + 100 * t), 255))
    poly(d, [(sx(-R), sy(1.2)), (sx(R), sy(1.2)), (sx(R * 0.8), sy(3.6)), (sx(-R * 0.8), sy(3.6))], fill=(30, 90, 150, 255))
    for j in range(3):                                            # waves
        yy = 1.6 + j * 0.8
        line(d, [(sx(-R * 0.9 + 0.3 * j + 0.6 * i), sy(yy + 0.18 * math.sin(i * 1.6))) for i in range(int(R * 3))], 0.12 * kx, (220, 240, 250, 255))
    # the beam
    poly(d, [(sx(0.2), sy(-2.2)), (sx(R * 0.95), sy(-3.6)), (sx(R * 0.95), sy(-1.6))], fill=(255, 236, 140, 210))
    # rock and tower (red and white stripes), the lamp room
    poly(d, [(sx(-2.6), sy(1.6)), (sx(-1.2), sy(0.6)), (sx(1.4), sy(0.7)), (sx(2.8), sy(1.8))], fill=(70, 66, 64, 255), outline=INK, w=3)
    for i in range(5):
        y_a = 0.8 - i * 0.62; y_b = y_a - 0.62; wa = 0.8 - i * 0.07; wb = wa - 0.07
        poly(d, [(sx(-wa), sy(y_a)), (sx(wa), sy(y_a)), (sx(wb), sy(y_b)), (sx(-wb), sy(y_b))], fill=(200, 30, 40, 255) if i % 2 == 0 else (245, 245, 240, 255), outline=INK, w=2.5)
    poly(d, [(sx(-0.55), sy(-2.3)), (sx(0.55), sy(-2.3)), (sx(0.55), sy(-2.9)), (sx(-0.55), sy(-2.9))], fill=(255, 220, 90, 255), outline=INK, w=2.5)
    poly(d, [(sx(-0.7), sy(-2.9)), (sx(0.7), sy(-2.9)), (sx(0), sy(-3.5))], fill=(40, 40, 46, 255), outline=INK, w=2.5)
    ellipse(d, cx, cy, R * kx, R * ky, outline=INK, w=7)        # the frame
    return done(img)


def pine_band_calf():
    """Black and grey: a ring of pines round the lower calf above the ankle, a thin line under them, seamless."""
    img, d = new(1024, 2048); W = 1024; base = 0.69 * 2048; rng = np.random.default_rng(11)
    trees = [(rng.random(), 2.2 + 1.6 * rng.random()) for _ in range(22)]
    def band(o):
        line(d, [(o, base + 10), (o + W, base + 10)], 5)
        for t, hcm in trees:
            x = o + t * W; h = hcm * 24.0; w = h * 0.36
            g = int(30 + 40 * (hcm - 2.2) / 1.6)
            for tier in range(4):                                  # tiers of branches
                yb = base - tier * h * 0.22; yt = yb - h * 0.42; ww = w * (1 - tier * 0.2)
                poly(d, [(x - ww / 2, yb), (x + ww / 2, yb), (x, yt)], fill=(g, g, g + 4, 255))
            line(d, [(x, base), (x, base + 8)], 4)
    wrap(d, band, W)
    return done(img)


def swallows_thigh():
    """Colour: two swallows in flight on the outer thigh - blue-black backs, red throats, forked tails."""
    img, d = new(1024, 2048); k = 22.0
    for (cx, cy, s, flip) in ((440, 0.13 * 2048, 1.0, 1), (610, 0.19 * 2048, 0.8, -1)):
        def q(x, y): return (cx + flip * x * k * s, cy + y * k * s)
        body = [q(-3.2, 0.2), q(-1.6, -0.6), q(0.6, -0.7), q(2.2, -0.2), q(2.6, 0.2), q(0.8, 0.8), q(-1.4, 0.9)]
        wing_up = [q(-0.6, -0.5), q(-2.4, -3.6), q(-1.0, -3.4), q(1.2, -0.6)]
        wing_dn = [q(-0.2, 0.6), q(-1.8, 3.0), q(-0.4, 2.6), q(1.0, 0.6)]
        tail = [q(-2.8, 0.1), q(-5.6, -1.0), q(-4.0, 0.2), q(-5.6, 1.4), q(-2.8, 0.5)]
        poly(d, tail, fill=(24, 40, 96, 255), outline=INK, w=2.5)
        poly(d, wing_dn, fill=(30, 54, 120, 255), outline=INK, w=2.5)
        poly(d, body, fill=(34, 62, 140, 255), outline=INK, w=2.5)
        poly(d, [q(1.4, 0.0), q(2.5, 0.2), q(1.2, 0.7), q(0.6, 0.4)], fill=(205, 40, 40, 255))        # the red throat
        poly(d, [q(-1.2, 0.5), q(1.0, 0.6), q(0.6, 0.85), q(-1.2, 0.85)], fill=(240, 236, 226, 255))  # the pale belly
        poly(d, wing_up, fill=(22, 36, 84, 255), outline=INK, w=2.5)
        ellipse(d, *q(1.9, -0.25), 2.4 * s, 2.4 * s, fill=(250, 250, 250, 255))
    return done(img)


# ------------------------------------------------------------------------------------------------ neck (1024 x 1024)
def star_neck():
    """Black and grey: a five-point star, each point half solid and half open, with a thin ring."""
    img, d = new(1024, 1024); cx = cy = 512; R = 3.4 * 114; r = R * 0.42
    ellipse(d, cx, cy, R * 1.08, R * 1.08, outline=INK, w=6)
    for i in range(5):
        a = -math.pi / 2 + i * 2 * math.pi / 5
        tip = (cx + R * math.cos(a), cy + R * math.sin(a))
        l = (cx + r * math.cos(a - math.pi / 5), cy + r * math.sin(a - math.pi / 5))
        rr = (cx + r * math.cos(a + math.pi / 5), cy + r * math.sin(a + math.pi / 5))
        poly(d, [(cx, cy), l, tip], fill=INK)
        poly(d, [(cx, cy), tip, rr], fill=(150, 150, 156, 255), outline=INK, w=5)
    return done(img)


def knot_neck():
    """Black and grey: an interlaced trefoil knot (our own drawing) - one band that crosses itself three times, over,
    under, over, a light band with dark edges."""
    img, d = new(1024, 1024); cx = cy = 512; S = 1.15 * 114; w = 0.46 * 114
    T = np.linspace(0, 2 * math.pi, 1500, endpoint=False)
    X = cx + S * (np.sin(T) + 2 * np.sin(2 * T)); Y = cy + S * (np.cos(T) - 2 * np.cos(2 * T)) * -1
    pts = list(zip(X, Y))
    def strand(a, b):                                             # (indices a..b along the band, wrapping)
        idx = [(a + k) % len(pts) for k in range((b - a) % len(pts) + 1)]
        q = [pts[k] for k in idx]
        line(d, q, w + 14); line(d, q, w - 4, (96, 96, 102, 255))      # (ink only darkens skin: a light grey band read as white paint)
    strand(0, len(pts) - 1)
    # the crossings: pairs of far-apart points of the band that meet
    P2 = np.c_[X, Y]; cr = []
    for i_ in range(0, len(T), 3):
        dd = np.hypot(P2[:, 0] - P2[i_, 0], P2[:, 1] - P2[i_, 1]); gap = np.abs(np.arange(len(T)) - i_); gap = np.minimum(gap, len(T) - gap)
        j_ = int(np.argmin(np.where(gap > len(T) // 8, dd, 1e9)))
        if dd[j_] < 3 and not any(min(abs(i_ - a), abs(i_ - b)) < 40 for a, b in cr): cr.append((i_, j_))
    # at each crossing one strand is laid again on top, alternating along the band: over, under, over ...
    for n_, (a, b) in enumerate(sorted(cr)):
        top = a if n_ % 2 == 0 else b
        strand(top - 45, top + 45)
    return done(img)


# ------------------------------------------------------------------------------------------------ fingers (1024 x 256)
def finger_symbols():
    """Black and grey: a symbol on each finger - sun, moon, star, wave (index to little finger)."""
    img, d = new(1024, 256)
    sq = 57.0 / 80.0                                                # (across is 80 px/cm, along 57: draw squashed across)
    for i in range(4):
        cx, cy = 128 + i * 256, 118; s = 62
        if i == 0:
            ellipse(d, cx, cy, s * 0.42 * sq, s * 0.42, fill=INK)
            for j in range(8):
                a = j * math.pi / 4
                line(d, [(cx + math.cos(a) * s * 0.58 * sq, cy + math.sin(a) * s * 0.58), (cx + math.cos(a) * s * 0.85 * sq, cy + math.sin(a) * s * 0.85)], 6)
        elif i == 1:
            ellipse(d, cx, cy, s * 0.7 * sq, s * 0.7, fill=INK)
            ellipse(d, cx + s * 0.3 * sq, cy - s * 0.1, s * 0.6 * sq, s * 0.6, fill=(0, 0, 0, 0))
        elif i == 2:
            pts = []
            for j in range(10):
                a = -math.pi / 2 + j * math.pi / 5; rr = s * (0.8 if j % 2 == 0 else 0.33)
                pts.append((cx + math.cos(a) * rr * sq, cy + math.sin(a) * rr))
            poly(d, pts, fill=INK)
        else:
            for j in range(2):
                line(d, [(cx + (x - 0.5) * s * 1.4 * sq, cy - 10 + j * 26 + 12 * math.sin(x * 2 * math.pi * 1.5)) for x in np.linspace(0, 1, 30)], 6)
    return done(img)


def finger_rings():
    """Colour: a thin ring on each finger - a teal band with gold dots - at the middle of the first joint."""
    img, d = new(1024, 256)
    for i in range(4):
        x0, x1 = i * 256 + 6, (i + 1) * 256 - 6; y = 82
        line(d, [(x0, y - 9), (x1, y - 9)], 4); line(d, [(x0, y + 9), (x1, y + 9)], 4)
        poly(d, [(x0, y - 7), (x1, y - 7), (x1, y + 7), (x0, y + 7)], fill=(20, 140, 150, 255))
        for j in range(6):
            ellipse(d, x0 + (j + 0.5) * (x1 - x0) / 6, y, 5, 5, fill=(230, 180, 40, 255))
    return done(img)


BATCH = [  # (id, name, kind, colour, draw)
    ('fern_forearm', 'Fern fronds (forearm)', 'arm', False, fern_forearm),
    ('rose_upperarm', 'Rose (upper arm)', 'arm', True, rose_upperarm),
    ('mandala_band', 'Geometric band (upper arm)', 'arm', False, mandala_band),
    ('lighthouse_calf', 'Lighthouse (calf)', 'leg', True, lighthouse_calf),
    ('pine_band_calf', 'Pine ring (lower calf)', 'leg', False, pine_band_calf),
    ('swallows_thigh', 'Two swallows (thigh)', 'leg', True, swallows_thigh),
    ('star_neck', 'Five-point star', 'neck', False, star_neck),
    ('knot_neck', 'Three-loop knot', 'neck', False, knot_neck),
    ('finger_symbols', 'Sun, moon, star, wave', 'fingers', False, finger_symbols),
    ('finger_rings', 'Teal rings with gold dots', 'fingers', True, finger_rings),
]
