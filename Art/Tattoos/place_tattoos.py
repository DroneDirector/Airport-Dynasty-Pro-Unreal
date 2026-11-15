# Tattoos laid into the MetaHuman skin layouts (owner, 2026-11-15): every MetaHuman body shares one UV layout, every head
# another, so a tattoo placed once fits all the people. Uses Art/Tattoos/skin_map_<part>.npz (map_skin.py: each texel's
# 3D point on the body) and the designs (make_tattoos.py). Writes Art/Tattoos/Placed/T_Tattoo_<Part>_<name>.png (2048,
# RGBA, the layout's own) - the skin material lays one body and one head tattoo picture over its own colour.
#   python Art/Tattoos/place_tattoos.py
import math, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'Placed'); os.makedirs(OUT, exist_ok=True)


def load(part):
    d = np.load(os.path.join(HERE, 'skin_map_%s.npz' % part))
    bones = {str(n): p for n, p in zip(d['bone_names'], d['bone_pts'])}
    return d['P'], d['N'], bones


def frame(axis, side):
    a = axis / np.linalg.norm(axis); s = side - a * np.dot(side, a); s /= np.linalg.norm(s); t = np.cross(a, s)
    return a, s, t


def sample(img, u, v):
    """Bilinear sample of an RGBA image (array H x W x 4, 0-1) at u, v in 0-1 (v down); outside -> 0."""
    H, W = img.shape[:2]
    x = u * W - 0.5; y = v * H - 0.5
    ok = (u >= 0) & (u <= 1) & (v >= 0) & (v <= 1)
    x0 = np.clip(np.floor(x).astype(int), 0, W - 2); y0 = np.clip(np.floor(y).astype(int), 0, H - 2)
    fx = np.clip(x - x0, 0, 1)[..., None]; fy = np.clip(y - y0, 0, 1)[..., None]
    c = (img[y0, x0] * (1 - fx) + img[y0, x0 + 1] * fx) * (1 - fy) + (img[y0 + 1, x0] * (1 - fx) + img[y0 + 1, x0 + 1] * fx) * fy
    c[~ok] = 0
    return c


def place_patch(part, design, bone_a, bone_b, frac, side, size_cm, reach_cm=9.0):
    """A round/square design centred frac of the way from joint a to joint b, on the side of the limb facing `side`,
    size_cm across, wrapped round the limb (around = arc length on the skin)."""
    P, Nn, B = load(part)
    A = B[bone_a][:3]; Bb = B[bone_b][:3]
    axis, s, t = frame(Bb - A, np.array(side, float))
    C = A + (Bb - A) * frac
    d = P - C
    along = d @ axis
    rad = d - along[..., None] * axis
    r = np.linalg.norm(rad, axis=-1)
    ang = np.arctan2(rad @ t, rad @ s)
    around = ang * np.where(np.isfinite(r), r, 0)
    ok = np.isfinite(r) & (r < reach_cm) & (np.abs(along) < size_cm)
    img = np.asarray(design, np.float32) / 255.0
    u = np.where(ok, around / size_cm + 0.5, -1); v = np.where(ok, -along / size_cm + 0.5, -1)   # (v: up the limb is up in the picture)
    return sample(img, u, v)


def place_sleeve(part, unrolled, chain, side, reach_cm=10.0, fade_cm=6.0):
    """An unrolled sleeve picture (u: once round the arm, seamless; v: 0 at the top of the chain, 1 at its start) laid
    round the arm along a chain of joints (wrist first), its last fade_cm thinning out."""
    P, Nn, B = load(part)
    pts = [B[chain[0]][:3]] + [B[n][:3] for n in chain[1:]]
    seg = [(pts[i], pts[i + 1]) for i in range(len(pts) - 1)]
    lens = [np.linalg.norm(b - a) for a, b in seg]; L = sum(lens)
    best_d = np.full(P.shape[:2], np.inf); s_at = np.zeros(P.shape[:2]); ang_at = np.zeros(P.shape[:2])
    acc = 0.0
    for (a, b), ln in zip(seg, lens):
        axis, sd, td = frame(b - a, np.array(side, float))
        d = P - a
        al = np.clip(d @ axis, 0, ln)
        rad = d - al[..., None] * axis
        dist = np.linalg.norm(rad, axis=-1)
        better = np.isfinite(dist) & (dist < best_d)
        best_d = np.where(better, dist, best_d)
        s_at = np.where(better, acc + al, s_at)
        ang_at = np.where(better, np.arctan2(rad @ td, rad @ sd), ang_at)
        acc += ln
    ok = np.isfinite(best_d) & (best_d < reach_cm)
    # (the arm's skin ends below the shoulder joint - the joint sits inside the body: the sleeve is fitted to the length
    # of arm there really is, its top at the highest skin of the arm itself)
    L = float(np.percentile(s_at[ok & (best_d < reach_cm * 0.8)], 99.0))
    ok &= s_at <= L
    img = np.asarray(unrolled, np.float32) / 255.0
    u = np.where(ok, (ang_at / (2 * math.pi)) % 1.0, -1); v = np.where(ok, 1 - s_at / L, -1)
    c = sample(img, u, v)
    # the shoulder end thins out
    fade = np.clip((L - s_at) / fade_cm, 0, 1)
    c[..., 3] *= np.where(ok, fade, 0)
    return c


def save(c, name):
    Image.fromarray((np.clip(c, 0, 1) * 255).astype(np.uint8), 'RGBA').save(os.path.join(OUT, name + '.png'))


INK = np.array([24, 27, 34]) / 255.0


def over(lo, hi):
    """Two placed tattoo pictures (H x W x 4, 0-1) as one: hi over lo."""
    A = hi[..., 3:] + lo[..., 3:] * (1 - hi[..., 3:])
    rgb = (hi[..., :3] * hi[..., 3:] + lo[..., :3] * lo[..., 3:] * (1 - hi[..., 3:])) / np.maximum(A, 1e-4)
    return np.concatenate([rgb, A], -1)


def colour_on_skin(c, fill=3.0, sat=1.5, dark=0.72):
    """Colour ink sits in the skin: a watercolour-light design reads as faint on skin - fuller ink, a little deeper
    and more saturated colour (the owner saw the first neck aurora as hardly there, twice)."""
    c = c.copy()
    c[..., 3] = 1 - (1 - np.clip(c[..., 3], 0, 1)) ** fill      # (a soft wash becomes solid ink, its edge stays soft)
    g = c[..., :3].mean(-1, keepdims=True)
    c[..., :3] = np.clip((g + (c[..., :3] - g) * sat) * dark, 0, 1)
    return c


def sleeve_design(W=1024, H=3072):
    """Black and grey full sleeve, unrolled (u round the arm, seamless; v from the shoulder (0) to the wrist (1)): a
    braided band at the wrist, curling waves up the forearm with dotwork, a band of triangles at the elbow, mountains
    with pines under a large moon on the upper arm, dotwork sky and birds, fading out at the shoulder."""
    K = 2; w, h = W * K, H * K
    a = Image.new('L', (w, h), 0); d = ImageDraw.Draw(a)
    rng = np.random.default_rng(7)
    lw = 5 * K
    # the colour version (owner: one with about 20 colours): the same black linework and dotwork, the solid black fills
    # become colour, and colour fills every part under the lines
    solid = Image.new('L', (w, h), 0); sd = ImageDraw.Draw(solid)
    cimg = Image.new('RGB', (w, h), (0, 0, 0)); cd = ImageDraw.Draw(cimg); wash = Image.new('L', (w, h), 0); wsd = ImageDraw.Draw(wash)
    hexc = lambda c: tuple(int(c[i:i + 2], 16) for i in (1, 3, 5))
    PALETTE = []
    def col(c):
        if c not in PALETTE: PALETTE.append(c)
        return hexc(c)
    def cpoly(pts, c):
        for off in (-w, 0, w):
            q = [(x + off, y) for x, y in pts]; cd.polygon(q, fill=col(c)); wsd.polygon(q, fill=255)
    def cband(y0, y1, cs):
        cs = [np.array(col(c), float) for c in cs]
        for y in range(int(y0), int(y1)):
            t = (y - y0) / max(1.0, y1 - y0) * (len(cs) - 1); i = min(int(t), len(cs) - 2); f = t - i
            c = tuple(int(v) for v in cs[i] * (1 - f) + cs[i + 1] * f)
            cd.line([(0, y), (w, y)], fill=c); wsd.line([(0, y), (w, y)], fill=255)
    def wrap_line(pts, width):
        for off in (-w, 0, w):
            d.line([(x + off, y) for x, y in pts], fill=255, width=width, joint='curve')
    def wrap_ellipse(box, **kw):
        for off in (-w, 0, w):
            d.ellipse([box[0] + off, box[1], box[2] + off, box[3]], **kw)
    def wrap_poly(pts, **kw):
        for off in (-w, 0, w):
            d.polygon([(x + off, y) for x, y in pts], **kw)
    xs = np.linspace(0, w, 400)
    # wrist braid (v 0.93-0.99)
    y0, y1 = 0.935 * h, 0.985 * h; mid = (y0 + y1) / 2; amp = (y1 - y0) / 2
    cband(y0, mid, ['#c99a2e', '#8a5a2b']); cband(mid, y1, ['#8a5a2b', '#c99a2e'])
    for ph in (0, math.pi):
        wrap_line([(x, mid + amp * 0.85 * math.sin(x / w * 2 * math.pi * 6 + ph)) for x in xs], lw * 2)
    wrap_line([(x, y0) for x in xs], lw); wrap_line([(x, y1) for x in xs], lw)
    # forearm waves (v 0.52-0.92): rows of curling crests, dotwork shade under each crest
    dots = Image.new('L', (w, h), 0); dd = ImageDraw.Draw(dots)
    rows = 6
    for r in range(rows):
        base = (0.55 + 0.37 * r / (rows - 1)) * h
        n = 4 + (r % 2)
        ph = rng.random() * w
        crest = [(x, base - 0.018 * h * (0.5 + 0.5 * math.sin((x + ph) / w * 2 * math.pi * n))) for x in xs]
        cpoly(crest + [(w, 0.93 * h), (0, 0.93 * h)], ['#cfeff0', '#5fd3c9', '#1aa3a3', '#1f6fb2', '#123a6b', '#0b2447'][r])
        wrap_line(crest, lw)
        # curls at each crest top
        for k in range(n):
            cx = (k / n * w - ph) % w + w / (4 * n); cy = base - 0.018 * h
            for rr in (0.014, 0.009):
                R = rr * h; wrap_ellipse([cx - R, cy - R, cx + R, cy + R], outline=255, width=lw)
        # dotwork under the crest line, thinning downward
        for _ in range(5200):
            x = rng.random() * w; t = rng.random() ** 1.7
            y = base - 0.018 * h * (0.5 + 0.5 * math.sin((x + ph) / w * 2 * math.pi * n)) + t * 0.05 * h
            s = K * (1.1 + 0.8 * rng.random()); dd.ellipse([x - s, y - s, x + s, y + s], fill=255)
    # elbow band of triangles (v 0.46-0.51)
    t0, t1 = 0.462 * h, 0.508 * h
    cband(t0, t1, ['#d9a441', '#d9a441'])
    wrap_line([(x, t0) for x in xs], lw); wrap_line([(x, t1) for x in xs], lw)
    m = 16
    for k in range(m):
        x0 = k / m * w; x1 = (k + 1) / m * w
        tri = [(x0, t1), ((x0 + x1) / 2, t0), (x1, t1)]
        wrap_poly(tri, outline=255)
        cpoly(tri, '#c8313b' if k % 2 == 0 else '#e8742a')
        if k % 2 == 0:
            for off in (-w, 0, w): sd.polygon([(x + off, y) for x, y in tri], fill=255)
    # upper arm (v 0.06-0.45): moon, mountains, pines, dotwork sky, birds
    moon_c = (0.5 * w, 0.17 * h); R = 0.07 * h
    cband(0.03 * h, 0.36 * h, ['#2b1d5c', '#5b2a86', '#b0306a', '#e8584a', '#f07b3f', '#ffd25a'])
    for off in (-w, 0, w):
        cd.ellipse([moon_c[0] - R * 1.35 + off, moon_c[1] - R * 1.35, moon_c[0] + R * 1.35 + off, moon_c[1] + R * 1.35], fill=col('#f4b942'))
        cd.ellipse([moon_c[0] - R + off, moon_c[1] - R, moon_c[0] + R + off, moon_c[1] + R], fill=col('#fff1c1'))
    wrap_ellipse([moon_c[0] - R, moon_c[1] - R, moon_c[0] + R, moon_c[1] + R], outline=255, width=lw)
    for _ in range(9000):   # sky dotwork, dense high, fading down, the moon bare
        x = rng.random() * w; y = (0.05 + 0.33 * rng.random() ** 0.6) * h
        if math.hypot(x - moon_c[0], y - moon_c[1]) < R * 1.15: continue
        if rng.random() < (y / h - 0.05) / 0.33: continue
        s = K * (1.0 + 0.8 * rng.random()); dd.ellipse([x - s, y - s, x + s, y + s], fill=255)
    ridge = lambda x: 0.33 * h - 0.06 * h * abs(math.sin(x / w * 2 * math.pi * 2.5 + 0.4)) - 0.025 * h * abs(math.sin(x / w * 2 * math.pi * 7 + 1.0))
    wrap_line([(x, ridge(x)) for x in xs], lw)
    # mountain shade: solid ink below the ridge to a lower line, snow left bare near the peaks
    for x in np.linspace(0, w, 900):
        top = ridge(x); bot = 0.39 * h
        snow = top + 0.02 * h
        for off in (-w, 0, w): sd.line([(x + off, snow), (x + off, bot)], fill=255, width=max(2, int(w / 900) + 2))
    cpoly([(x, ridge(x)) for x in xs] + [(w, 0.39 * h), (0, 0.39 * h)], '#dfe8ff')
    cpoly([(x, ridge(x) + 0.02 * h) for x in xs] + [(w, 0.39 * h), (0, 0.39 * h)], '#4a3f8c')
    cpoly([(x, max(ridge(x) + 0.03 * h, 0.35 * h + 0.012 * h * math.sin(x / w * 2 * math.pi * 3))) for x in xs] + [(w, 0.39 * h), (0, 0.39 * h)], '#3d5a99')
    cband(0.39 * h, 0.43 * h, ['#7a9a3a', '#7a9a3a'])
    # pines along the foot of the mountains
    for k in range(22):
        px = (k + rng.random() * 0.6) / 22 * w; base = 0.425 * h; ht = (0.04 + 0.03 * rng.random()) * h; wd = ht * 0.32
        pine = [(px, base - ht), (px - wd, base), (px + wd, base)]
        for off in (-w, 0, w): sd.polygon([(x + off, y) for x, y in pine], fill=255)
        wrap_poly(pine, outline=255)
        cpoly(pine, '#1f5e3b' if k % 2 == 0 else '#2f8a4c')
        wrap_line([(px, base), (px, base + 0.008 * h)], lw)
    wrap_line([(x, 0.43 * h) for x in xs], lw)
    # birds
    for k in range(7):
        bx = rng.random() * w; by = (0.09 + 0.12 * rng.random()) * h; bs = 0.012 * h
        wrap_line([(bx - bs, by), (bx - bs * 0.4, by - bs * 0.45), (bx, by)], lw); wrap_line([(bx, by), (bx + bs * 0.4, by - bs * 0.45), (bx + bs, by)], lw)
    # shoulder top: a thin closing line that fades (the placer fades the last cm too)
    small = lambda im: np.asarray(im.filter(ImageFilter.GaussianBlur(K * 0.6)).resize((W, H), Image.LANCZOS)).astype(np.float32) / 255.0
    line = np.maximum(np.asarray(a), np.asarray(dots)).astype(np.uint8)
    al = np.maximum(line, np.asarray(solid))
    rgba = np.zeros((H, W, 4), np.uint8); rgba[..., :3] = (INK * 255).astype(np.uint8); rgba[..., 3] = (small(Image.fromarray(al)) * 255).astype(np.uint8)
    Image.fromarray(rgba, 'RGBA').save(os.path.join(HERE, 'tattoo_sleeve_waves_mountains.png'))
    # colour: the colours (a touch soft, as ink spreads in skin) under the black lines and dots
    ink = small(Image.fromarray(line)); wa = small(wash) * 0.9
    cc = np.asarray(cimg.filter(ImageFilter.GaussianBlur(K * 1.2)).resize((W, H), Image.LANCZOS)).astype(np.float32) / 255.0
    A = np.clip(ink + wa * (1 - ink), 0, 1)
    rgb = (INK[None, None, :] * ink[..., None] + cc * (wa * (1 - ink))[..., None]) / np.maximum(A[..., None], 1e-4)
    rgba_c = np.dstack([np.clip(rgb, 0, 1) * 255, A * 255]).astype(np.uint8)
    Image.fromarray(rgba_c, 'RGBA').save(os.path.join(HERE, 'tattoo_sleeve_waves_mountains_colour.png'))
    print('colour sleeve: %d colours %s' % (len(PALETTE), PALETTE))
    return Image.fromarray(rgba, 'RGBA'), Image.fromarray(rgba_c, 'RGBA')


if __name__ == '__main__':
    compass = Image.open(os.path.join(HERE, 'tattoo_compass_rose.png'))
    aurora = Image.open(os.path.join(HERE, 'tattoo_aurora_mountains.png'))
    sleeve, sleeve_c = sleeve_design()
    # the compass on the left forearm's outer side, halfway down (10 cm across); the sleeve on the right arm
    cmp_ = place_patch('body', compass, 'lowerarm_l', 'hand_l', 0.5, (0, 0, 1), 10.0); save(cmp_, 'T_Tattoo_Body_CompassForearm')
    slv = place_sleeve('body', sleeve, ['hand_r', 'lowerarm_r', 'upperarm_r'], (0, 0, 1)); save(slv, 'T_Tattoo_Body_SleeveRight')
    slc = place_sleeve('body', sleeve_c, ['hand_r', 'lowerarm_r', 'upperarm_r'], (0, 0, 1)); save(slc, 'T_Tattoo_Body_SleeveRightColour')
    # one body picture holds all of a person's body tattoos: the samples (compass + sleeve, black or colour)
    save(over(cmp_, slv), 'T_Tattoo_Body_Sample'); save(over(cmp_, slc), 'T_Tattoo_Body_SampleColour')
    # the aurora on the left side of the neck (6 cm across); colour ink on skin shows only when it is strong
    save(colour_on_skin(place_patch('face', aurora, 'neck_01', 'head', 0.25, (1, 0, 0), 6.0, reach_cm=8.0)), 'T_Tattoo_Face_AuroraNeck')
    Image.new('RGBA', (4, 4), (0, 0, 0, 0)).save(os.path.join(OUT, 'T_Tattoo_None.png'))   # (the skin's default: no tattoo)
    print('placed')
