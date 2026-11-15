# The third batch of tattoo designs (owner, 2026-11-15: "maybe 20 more"; our own designs, colour and black-and-grey
# mixed, nothing demonic, devilish or sexual). Frames and helpers as designs_batch1.py / designs_batch2.py.
import math
import numpy as np
from designs_batch1 import new, done, line, ellipse, poly, wrap, INK
from designs_batch2 import bez, GREY


def leaf(d, base, tip, width, fill, w=3):
    mx, my = (base[0] + tip[0]) / 2, (base[1] + tip[1]) / 2
    nx, ny = -(tip[1] - base[1]) * width, (tip[0] - base[0]) * width
    pts = bez(base, (mx + nx, my + ny), tip, 16) + bez(tip, (mx - nx, my - ny), base, 16)[1:]
    poly(d, pts, fill=fill, outline=INK, w=w)
    line(d, [base, tip], max(1, w * 0.6), (0, 0, 0, 120))


def heart(cx, cy, s, n=60):
    return [(cx + s * 16 * math.sin(t) ** 3 / 17, cy - s * (13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)) / 17) for t in np.linspace(0, 2 * math.pi, n)]


# ------------------------------------------------------------------------------------------------ arms (1024 x 2048)
def hummingbird_forearm():
    """Colour: a hummingbird hovering at a small blossom, green back, ruby throat, wings a blur of blue."""
    img, d = new(1024, 2048); k = 40.0; cx, cy = 512, 0.55 * 2048
    def q(x, y): return (cx + x * k, cy + y * k)
    for i in range(5): leaf(d, q(-2.2, 2.2), q(-2.2 + 1.4 * math.cos(i * 1.26), 2.2 + 1.4 * math.sin(i * 1.26)), 0.35, (226, 90, 140, 255), 2.5)
    ellipse(d, *q(-2.2, 2.2), 0.3 * k, 0.3 * k, fill=(250, 210, 70, 255), outline=INK, w=2.5)
    for j, (a, L) in enumerate(((-2.1, 3.4), (-1.75, 3.0))):
        poly(d, [q(0.2, -0.3), q(0.2 + L * math.cos(a), -0.3 + L * math.sin(a)), q(0.9 + L * 0.8 * math.cos(a + 0.25), -0.3 + L * 0.8 * math.sin(a + 0.25))], fill=(70, 150, 200, 200 - 40 * j), outline=INK, w=2.5)
    body = [q(-1.0, 0.6), q(-0.2, -0.5), q(0.9, -0.6), q(1.3, 0.2), q(0.7, 1.4), q(0.2, 2.6), q(-0.3, 1.5)]
    poly(d, body, fill=(40, 150, 90, 255), outline=INK, w=3)
    poly(d, [q(-0.9, 0.55), q(-0.3, 0.1), q(0.1, 0.9), q(-0.4, 1.3)], fill=(200, 30, 60, 255))
    ellipse(d, *q(-0.55, -0.05), 0.55 * k, 0.5 * k, fill=(36, 130, 80, 255), outline=INK, w=3)
    line(d, [q(-1.0, 0.0), q(-2.0, 1.6)], 0.1 * k)                                          # the beak, into the blossom
    ellipse(d, *q(-0.6, -0.15), 0.09 * k, 0.09 * k, fill=INK)
    poly(d, [q(0.2, 2.4), q(0.7, 3.3), q(0.2, 3.0), q(-0.2, 3.4), q(-0.1, 2.5)], fill=(30, 100, 70, 255), outline=INK, w=2.5)
    return done(img)


def balloon_forearm():
    """Colour: a hot-air balloon with red, yellow and blue panels and a wicker basket, two small clouds."""
    img, d = new(1024, 2048); k = 40.0; cx, cy = 512, 0.52 * 2048; R = 2.3
    def q(x, y): return (cx + x * k, cy + y * k)
    for (x, y, s) in ((-2.8, 2.8, 0.8), (2.6, -2.4, 0.6)):
        for dx, r in ((-0.8, 0.55), (0, 0.8), (0.8, 0.55)): ellipse(d, *q(x + dx * s, y), r * s * k, r * s * k * 0.75, fill=(250, 250, 250, 255), outline=INK, w=2)
    def half(y):                                                     # the envelope's half-width at height y
        if y <= -0.6: return math.sqrt(max(0.0, R * R - (y + 0.6) ** 2))
        t = (y + 0.6) / 3.0; return R + (0.55 - R) * t ** 1.6
    ys = list(np.linspace(-0.6 - R, 2.4, 60))
    cols = [(206, 40, 46, 255), (246, 196, 50, 255), (40, 90, 170, 255), (246, 196, 50, 255), (206, 40, 46, 255)]
    F = [math.sin(-math.pi / 2 + math.pi * i / 5) for i in range(6)]
    for i in range(5):
        a = [q(F[i] * half(y), y) for y in ys]; b = [q(F[i + 1] * half(y), y) for y in ys]
        poly(d, a + b[::-1], fill=cols[i])
    for i in range(1, 5): line(d, [q(F[i] * half(y), y) for y in ys], 2.5)
    line(d, [q(-half(y), y) for y in ys] + [q(half(y), y) for y in ys[::-1]], 4)
    line(d, [q(-0.55, 2.4), q(0.55, 2.4)], 4)
    for s_ in (-1, 1): line(d, [q(s_ * 0.5, 2.4), q(s_ * 0.42, 3.3)], 2.5)
    poly(d, [q(-0.55, 3.3), q(0.55, 3.3), q(0.45, 4.0), q(-0.45, 4.0)], fill=(150, 104, 56, 255), outline=INK, w=2.5)
    for y in (3.53, 3.76): line(d, [q(-0.5, y), q(0.5, y)], 1.5, (90, 60, 30, 255))
    return done(img)

def key_forearm():
    """Black and grey: an old ornate key along the forearm - a looped bow, long shank, toothed bit."""
    img, d = new(1024, 2048); k = 40.0; cx = 512; y0 = 0.40 * 2048
    def q(x, y): return (cx + x * k, y0 + y * k)
    for a in range(3):                                                                    # the bow: three loops
        ang = -math.pi / 2 + (a - 1) * 1.1
        ellipse(d, *q(0.95 * math.cos(ang), 0.95 + 0.95 * math.sin(ang)), 0.62 * k, 0.62 * k, outline=INK, w=0.16 * k)
    ellipse(d, *q(0, 1.3), 0.5 * k, 0.5 * k, fill=GREY(80), outline=INK, w=0.12 * k)
    ellipse(d, *q(0, 1.3), 0.2 * k, 0.2 * k, fill=(0, 0, 0, 0))
    for y in (2.0, 2.35): ellipse(d, *q(0, y), 0.42 * k, 0.12 * k, fill=INK)
    poly(d, [q(-0.18, 2.3), q(0.18, 2.3), q(0.18, 9.4), q(-0.18, 9.4)], fill=GREY(90), outline=INK, w=0.08 * k)
    poly(d, [q(0.18, 8.0), q(1.4, 8.0), q(1.4, 8.45), q(0.9, 8.45), q(0.9, 8.75), q(1.4, 8.75), q(1.4, 9.4), q(0.18, 9.4)], fill=GREY(70), outline=INK, w=0.08 * k)
    return done(img)


def clock_upperarm():
    """Black and grey: a pocket-watch face with Roman-style tick marks and fine hands, a short chain."""
    img, d = new(1024, 2048); k = 40.0; cx, cy = 512, 0.17 * 2048; R = 3.0 * k
    ellipse(d, cx, cy, R * 1.12, R * 1.12, fill=GREY(110), outline=INK, w=8)
    ellipse(d, cx, cy, R, R, fill=(0, 0, 0, 0), outline=INK, w=4)
    for i in range(60):
        a = i * math.pi / 30; L = 0.22 if i % 5 == 0 else 0.08
        line(d, [(cx + math.cos(a) * R * 0.9, cy + math.sin(a) * R * 0.9), (cx + math.cos(a) * R * (0.9 - L), cy + math.sin(a) * R * (0.9 - L))], 5 if i % 5 == 0 else 2)
    for a, L, w in ((-math.pi / 2 + 0.9, 0.5, 9), (-math.pi / 2 - 1.3, 0.75, 6)):
        line(d, [(cx, cy), (cx + math.cos(a) * R * L, cy + math.sin(a) * R * L)], w)
    ellipse(d, cx, cy, 9, 9, fill=INK)
    ellipse(d, cx, cy - R * 1.22, 0.32 * k, 0.32 * k, outline=INK, w=6)
    for i in range(6):                                                                    # the chain links
        x = cx + 0.35 * k + i * 0.42 * k; y = cy - R * 1.3 - 0.25 * k * math.sin(i * 0.6)
        ellipse(d, x, y, 0.22 * k, 0.13 * k, outline=INK, w=4)
    return done(img)


def propeller_forearm():
    """Black and grey: a vintage two-blade wooden propeller with its hub, laid along the forearm."""
    img, d = new(1024, 2048); k = 40.0; cx, cy = 512, 0.56 * 2048
    for s in (-1, 1):
        blade = [(cx + s * 0.25 * k, cy)] + [(cx + s * (0.3 + 0.55 * math.sin(t * math.pi) ** 0.7) * k * (1 if i < 20 else 1), cy + s * t * 5.6 * k) for i, t in enumerate(np.linspace(0.05, 1, 24))]
        blade += [(cx - s * (0.2 + 0.35 * math.sin(t * math.pi) ** 0.8) * k, cy + s * t * 5.6 * k) for t in np.linspace(1, 0.05, 24)]
        poly(d, blade, fill=GREY(95), outline=INK, w=4)
        line(d, [(cx, cy + s * 0.8 * k), (cx + 0.05 * k, cy + s * 5.0 * k)], 3, GREY(150))
        poly(d, [(cx - 0.5 * k, cy + s * 4.9 * k), (cx + 0.6 * k, cy + s * 4.9 * k), (cx + 0.3 * k, cy + s * 5.6 * k), (cx - 0.3 * k, cy + s * 5.6 * k)], fill=GREY(160), outline=INK, w=3)
    ellipse(d, cx, cy, 0.8 * k, 0.8 * k, fill=GREY(60), outline=INK, w=5)
    for i in range(6): ellipse(d, cx + math.cos(i * 1.047) * 0.5 * k, cy + math.sin(i * 1.047) * 0.5 * k, 4, 4, fill=GREY(170))
    return done(img)


def chevron_band():
    """Black and grey: a band of interlocking chevrons and two fine lines round the upper arm, seamless."""
    img, d = new(1024, 2048); W = 1024; y0, y1 = 0.215 * 2048, 0.26 * 2048; h = y1 - y0
    def band(o):
        line(d, [(o, y0 - 12), (o + W, y0 - 12)], 4); line(d, [(o, y1 + 12), (o + W, y1 + 12)], 4)
        n = 20
        for i in range(n):
            x0 = o + i * W / n; x1 = x0 + W / n; xm = (x0 + x1) / 2
            poly(d, [(x0, y0), (xm, y0 + h * 0.45), (x1, y0), (x1, y0 + h * 0.3), (xm, y0 + h * 0.75), (x0, y0 + h * 0.3)], fill=INK)
            poly(d, [(x0, y0 + h * 0.62), (xm, y1), (x1, y0 + h * 0.62), (x1, y0 + h * 0.8), (xm, y1 + 4), (x0, y0 + h * 0.8)], fill=GREY(110))
    wrap(d, band, W)
    return done(img)


# ------------------------------------------------------------------------------------------------ legs (1024 x 2048)
def lotus_calf():
    """Colour: a pink lotus on the water with two round leaves, petals shading to white at the tips."""
    img, d = new(1024, 2048); kx, ky = 27.7, 24.0; cx, cy = 512, 0.55 * 2048
    def q(x, y): return (cx + x * kx, cy + y * ky)
    for (x, s) in ((-3.0, 1), (3.2, -1)):
        ellipse(d, *q(x, 1.6), 1.9 * kx, 0.7 * ky, fill=(50, 130, 80, 255), outline=INK, w=3)
        line(d, [q(x, 1.6), q(x + s * 1.6, 1.3)], 2.5, (20, 70, 40, 255))
    for j in range(3): line(d, [q(-4 + 0.5 * i + 0.4 * j, 2.6 + j * 0.5 + 0.12 * math.sin(i * 2)) for i in range(17)], 3, (60, 120, 180, 255))
    for ring, (n, L, c) in enumerate(((5, 3.4, (236, 120, 160)), (4, 2.9, (244, 150, 184)), (3, 2.2, (250, 196, 214)))):
        for i in range(n):
            a = -math.pi / 2 + (i - (n - 1) / 2) * (1.9 / max(1, n - 1)) * (1 - ring * 0.2)
            leaf(d, q(0, 1.4), q(L * math.cos(a) * 0.95, 1.4 + L * math.sin(a)), 0.36, c + (255,), 3)
    return done(img)


def sunflower_thigh():
    """Colour: a sunflower head - two rings of golden petals round a dark seed spiral - on the outer thigh."""
    img, d = new(1024, 2048); k = 24.0; cx, cy = 512, 0.15 * 2048
    for ring, (n, L, c) in enumerate(((22, 5.6, (232, 168, 30)), (22, 4.9, (250, 200, 50)))):
        for i in range(n):
            a = (i + ring * 0.5) * 2 * math.pi / n
            leaf(d, (cx + math.cos(a) * 2.2 * k, cy + math.sin(a) * 2.2 * k), (cx + math.cos(a) * L * k, cy + math.sin(a) * L * k), 0.26, c + (255,), 2.5)
    ellipse(d, cx, cy, 2.5 * k, 2.5 * k, fill=(76, 46, 24, 255), outline=INK, w=4)
    gold = math.pi * (3 - math.sqrt(5))
    for i in range(260):                                                                  # the seeds in a golden-angle spiral
        r = 2.3 * k * math.sqrt(i / 260); a = i * gold
        ellipse(d, cx + r * math.cos(a), cy + r * math.sin(a), 2.6, 2.6, fill=(36, 22, 12, 255))
    return done(img)


def bee_calf():
    """Colour: a honeybee in flight, striped body, clear veined wings, a dotted flight trail."""
    img, d = new(1024, 2048); kx, ky = 27.7, 24.0; cx, cy = 512, 0.52 * 2048
    def q(x, y): return (cx + x * kx, cy + y * ky)
    for i in range(12): ellipse(d, *q(-4.6 + i * 0.32, 2.6 - 0.5 * math.sin(i * 0.45) - i * 0.12), 2.2, 2.2, fill=INK)
    for s, a in ((-1, 0.0), (1, 0.4)):
        ellipse(d, *q(-0.4 + s * 0.2, -1.5 + a * 0.4), 1.4 * kx, 0.9 * ky, fill=(220, 236, 246, 200), outline=INK, w=3)
        line(d, [q(-1.2 + s * 0.2, -1.4 + a * 0.4), q(0.4 + s * 0.2, -1.6 + a * 0.4)], 1.5, (0, 0, 0, 140))
    ellipse(d, *q(0, 0), 2.0 * kx, 1.25 * ky, fill=(240, 186, 40, 255), outline=INK, w=4)
    for x in (-0.9, -0.1, 0.7): poly(d, [q(x - 0.25, -1.15), q(x + 0.25, -1.2), q(x + 0.25, 1.2), q(x - 0.25, 1.15)], fill=(30, 26, 22, 255))
    ellipse(d, *q(0, 0), 2.0 * kx, 1.25 * ky, outline=INK, w=4)
    ellipse(d, *q(2.3, -0.1), 0.8 * kx, 0.75 * ky, fill=(30, 26, 22, 255), outline=INK, w=3)
    poly(d, [q(-2.0, -0.15), q(-2.6, 0), q(-2.0, 0.15)], fill=INK)
    for s in (-1, 1): line(d, bez(q(2.7, -0.5), q(3.4, -1.2 + s * 0.3), q(3.6, -1.6 + s * 0.4), 10), 2.5)
    return done(img)


def arrow_thigh():
    """Black and grey: a long arrow with a feathered fletching and a small geometric head, down the outer thigh."""
    img, d = new(1024, 2048); k = 24.0; cx = 512; y0, y1 = 0.04 * 2048, 0.26 * 2048
    line(d, [(cx, y0 + 1.2 * k), (cx, y1 - 1.6 * k)], 6)
    poly(d, [(cx, y1), (cx - 0.75 * k, y1 - 1.8 * k), (cx, y1 - 1.4 * k), (cx + 0.75 * k, y1 - 1.8 * k)], fill=INK)
    for i in range(3):
        y = y0 + i * 0.9 * k
        for s in (-1, 1):
            poly(d, [(cx, y + 0.4 * k), (cx + s * 0.9 * k, y), (cx + s * 0.9 * k, y + 0.7 * k), (cx, y + 1.2 * k)], fill=GREY(60 + 40 * i), outline=INK, w=3)
    for t in (0.35, 0.45, 0.55):
        y = y0 + (y1 - y0) * t; ellipse(d, cx, y, 0.22 * k, 0.22 * k, fill=(0, 0, 0, 0), outline=INK, w=3)
    return done(img)


def triangles_calf():
    """Black and grey: three nested triangles in fine lines with dotwork filling the smallest, on the outer calf."""
    img, d = new(1024, 2048); kx, ky = 27.7, 24.0; cx, cy = 512, 0.53 * 2048
    def q(x, y): return (cx + x * kx, cy + y * ky)
    for s, w in ((4.6, 5), (3.3, 4), (2.0, 4)):
        poly(d, [q(0, -s), q(s * 0.95, s * 0.6), q(-s * 0.95, s * 0.6)], outline=INK, w=w)
    rng = np.random.default_rng(4)
    for _ in range(420):
        u, v = rng.random(), rng.random()
        if u + v > 1: u, v = 1 - u, 1 - v
        x = 0 + u * (1.9 - 0) + v * (-1.9 - 0); y = -2.0 + u * 3.2 + v * 3.2
        ellipse(d, *q(x, y), 1.6, 1.6, fill=INK)
    line(d, [q(0, -6.0), q(0, -4.8)], 3); line(d, [q(0, 3.0), q(0, 4.6)], 3)
    ellipse(d, *q(0, -6.4), 5, 5, fill=INK); ellipse(d, *q(0, 5.0), 5, 5, fill=INK)
    return done(img)


def mushrooms_calf():
    """Colour: two red toadstools with white spots in a tuft of grass, low on the outer calf."""
    img, d = new(1024, 2048); kx, ky = 27.7, 24.0; cx, cy = 512, 0.64 * 2048
    def q(x, y): return (cx + x * kx, cy + y * ky)
    for (x, s) in ((-0.9, 1.0), (1.2, 0.7)):
        poly(d, [q(x - 0.35 * s, 0), q(x + 0.35 * s, 0), q(x + 0.28 * s, -2.2 * s), q(x - 0.28 * s, -2.2 * s)], fill=(244, 236, 220, 255), outline=INK, w=3)
        cap = [q(x + 1.6 * s * math.cos(t), -2.1 * s - 1.4 * s * math.sin(t)) for t in np.linspace(0, math.pi, 30)]
        poly(d, cap, fill=(206, 36, 36, 255), outline=INK, w=3.5)
        for (u, v, r) in ((-0.8, 0.5, 0.22), (0.1, 0.95, 0.28), (0.85, 0.45, 0.2), (-0.3, 0.3, 0.15)):
            ellipse(d, *q(x + u * s, -2.1 * s - v * s), r * s * kx, r * s * ky, fill=(250, 250, 246, 255))
    rng = np.random.default_rng(2)
    for i in range(22):
        x = -2.4 + i * 0.23; h = 0.6 + 0.6 * rng.random(); lean = (rng.random() - 0.5) * 0.8
        line(d, bez(q(x, 0.3), q(x + lean * 0.4, 0.3 - h * 0.6), q(x + lean, 0.3 - h), 8), 3, (40, 110, 50, 255))
    return done(img)


# ------------------------------------------------------------------------------------------------ neck (1024 x 1024)
def rose_small_neck():
    """Colour: a small closed rosebud on a short stem with one leaf."""
    img, d = new(1024, 1024); k = 114.0; cx, cy = 512, 430
    def q(x, y): return (cx + x * k, cy + y * k)
    line(d, bez(q(0, 0.6), q(0.3, 1.6), q(-0.1, 2.8)), 0.1 * k, (40, 100, 50, 255))
    leaf(d, q(0.15, 1.6), q(1.2, 1.1), 0.32, (50, 130, 66, 255), 4)
    for (dx, rx, c) in ((-0.45, 0.6, (176, 20, 40)), (0.45, 0.6, (176, 20, 40)), (0, 0.62, (206, 36, 52))):
        poly(d, [q(dx + rx * math.cos(t), 0.0 - 0.9 * math.sin(t) * (1 if t < math.pi else 0.6)) for t in np.linspace(0, 2 * math.pi, 40)], fill=c + (255,), outline=INK, w=5)
    line(d, bez(q(-0.35, -0.35), q(0, -0.7), q(0.35, -0.35), 12), 5, (110, 10, 24, 255))
    poly(d, [q(-0.7, 0.35), q(0, 0.75), q(0.7, 0.35), q(0.2, 0.55), q(0, 0.4), q(-0.2, 0.55)], fill=(50, 130, 66, 255), outline=INK, w=4)
    return done(img)


def three_dots_neck():
    """Black and grey: three dots in a small triangle inside a fine circle - minimal."""
    img, d = new(1024, 1024); k = 114.0; cx, cy = 512, 512
    for (x, y) in ((0, -0.62), (-0.6, 0.42), (0.6, 0.42)): ellipse(d, cx + x * k, cy + y * k, 0.2 * k, 0.2 * k, fill=INK)
    ellipse(d, cx, cy, 1.5 * k, 1.5 * k, outline=INK, w=5)
    return done(img)

def moon_stars_neck():
    """Black and grey: a crescent moon with three small four-point stars round it."""
    img, d = new(1024, 1024); k = 114.0; cx, cy = 470, 520
    ellipse(d, cx, cy, 1.9 * k, 1.9 * k, fill=INK)
    ellipse(d, cx + 0.85 * k, cy - 0.35 * k, 1.7 * k, 1.7 * k, fill=(0, 0, 0, 0))
    for (x, y, s) in ((2.3, -1.6, 0.55), (2.9, 0.2, 0.35), (1.6, 1.6, 0.42)):
        X, Y = 512 + x * k * 0.9, 512 + y * k
        poly(d, [(X, Y - s * k), (X + s * 0.25 * k, Y - s * 0.25 * k), (X + s * k, Y), (X + s * 0.25 * k, Y + s * 0.25 * k), (X, Y + s * k), (X - s * 0.25 * k, Y + s * 0.25 * k), (X - s * k, Y), (X - s * 0.25 * k, Y - s * 0.25 * k)], fill=GREY(70))
    return done(img)


def heart_line_neck():
    """Black and grey: a heartbeat trace that runs into a small heart and out the other side."""
    img, d = new(1024, 1024); k = 114.0; s = 1.35; hx = 0.7
    h = heart(hx, 0.15, s, 80)
    L = hx - 16 / 17 * s * 0.95
    line(d, [(512 + x * k, 512 + y * k) for x, y in ((-3.7, -0.25), (-2.5, -0.25), (-2.2, -1.1), (-1.8, 0.6), (-1.5, -0.25), (L, -0.25))], 7)
    line(d, [(512 + x * k, 512 + y * k) for x, y in h], 7)
    line(d, [(512 + (hx + 16 / 17 * s * 0.95) * k, 512 - 0.25 * k), (512 + 3.7 * k, 512 - 0.25 * k)], 7)
    return done(img)

def radar_neck():
    """Black and grey: an airport radar scope - rings, cross lines, a sweep and three small blips."""
    img, d = new(1024, 1024); k = 114.0; cx = cy = 512; R = 3.4 * k
    for a in np.linspace(-0.9, 0, 18):                                                    # the sweep's fading wedge
        g = int(200 + 50 * a / 0.9)
        poly(d, [(cx, cy), (cx + R * math.cos(a - 0.06), cy + R * math.sin(a - 0.06)), (cx + R * math.cos(a), cy + R * math.sin(a))], fill=(g - 120, g - 120, g - 110, int(70 + 140 * (1 + a / 0.9))))
    for r in (1, 0.66, 0.33): ellipse(d, cx, cy, R * r, R * r, outline=INK, w=6 if r == 1 else 3)
    line(d, [(cx - R, cy), (cx + R, cy)], 2); line(d, [(cx, cy - R), (cx, cy + R)], 2)
    line(d, [(cx, cy), (cx + R, cy)], 6)
    for (x, y) in ((0.4, -0.5), (-0.55, 0.3), (0.2, 0.6)): ellipse(d, cx + x * R, cy + y * R, 11, 11, fill=INK)
    return done(img)


# ------------------------------------------------------------------------------------------------ fingers (1024 x 256)
def finger_hearts():
    """Colour: a small red heart on each finger."""
    img, d = new(1024, 256); sq = 57.0 / 80.0
    for i in range(4):
        cx = 128 + i * 256; h = heart(0, 0, 1, 40)
        poly(d, [(cx + x * 44 * sq, 112 + y * 44) for x, y in h], fill=(206, 30, 46, 255), outline=INK, w=4)
    return done(img)


def finger_chevrons():
    """Black and grey: stacked chevrons, one more on each finger from index to little finger."""
    img, d = new(1024, 256)
    for i in range(4):
        cx = 128 + i * 256
        for j in range(i + 1):
            y = 70 + j * 22
            line(d, [(cx - 30, y), (cx, y + 16), (cx + 30, y)], 6)
    return done(img)


def finger_stars():
    """Black and grey: a tiny open star on each finger, alternating with a dot."""
    img, d = new(1024, 256); sq = 57.0 / 80.0
    for i in range(4):
        cx, cy, s = 128 + i * 256, 112, 40
        if i % 2 == 0:
            pts = [(cx + math.cos(-math.pi / 2 + j * math.pi / 5) * s * (1 if j % 2 == 0 else 0.42) * sq, cy + math.sin(-math.pi / 2 + j * math.pi / 5) * s * (1 if j % 2 == 0 else 0.42)) for j in range(10)]
            poly(d, pts, outline=INK, w=4)
        else:
            ellipse(d, cx, cy, 10 * sq, 10, fill=INK)
    return done(img)


BATCH = [  # (id, name, kind, colour, draw)
    ('hummingbird_forearm', 'Hummingbird at a blossom (forearm)', 'arm', True, hummingbird_forearm),
    ('balloon_forearm', 'Hot-air balloon (forearm)', 'arm', True, balloon_forearm),
    ('key_forearm', 'Old key (forearm)', 'arm', False, key_forearm),
    ('clock_upperarm', 'Pocket watch (upper arm)', 'arm', False, clock_upperarm),
    ('propeller_forearm', 'Wooden propeller (forearm)', 'arm', False, propeller_forearm),
    ('chevron_band', 'Chevron band (upper arm)', 'arm', False, chevron_band),
    ('lotus_calf', 'Lotus on the water (calf)', 'leg', True, lotus_calf),
    ('sunflower_thigh', 'Sunflower (thigh)', 'leg', True, sunflower_thigh),
    ('bee_calf', 'Honeybee (calf)', 'leg', True, bee_calf),
    ('arrow_thigh', 'Arrow (thigh)', 'leg', False, arrow_thigh),
    ('triangles_calf', 'Nested triangles (calf)', 'leg', False, triangles_calf),
    ('mushrooms_calf', 'Toadstools (calf)', 'leg', True, mushrooms_calf),
    ('rose_small_neck', 'Rosebud', 'neck', True, rose_small_neck),
    ('three_dots_neck', 'Three dots', 'neck', False, three_dots_neck),
    ('moon_stars_neck', 'Moon and stars', 'neck', False, moon_stars_neck),
    ('heart_line_neck', 'Heartbeat heart', 'neck', False, heart_line_neck),
    ('radar_neck', 'Radar scope', 'neck', False, radar_neck),
    ('finger_hearts', 'Red hearts', 'fingers', True, finger_hearts),
    ('finger_chevrons', 'Chevrons', 'fingers', False, finger_chevrons),
    ('finger_stars', 'Stars and dots', 'fingers', False, finger_stars),
]
