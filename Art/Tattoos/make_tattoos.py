# The game's own tattoos (owner, 2026-11-15: "create our own tattoos ... inspiration from the internet ... greyish,
# blackish and colourish ... no demonic, devilish, no horny ones"; 35 to choose from, two samples first). Drawn here
# in code, our own designs from references of the styles (not copies of anyone's tattoo): each an RGBA picture, its
# colour the ink's, its alpha how much ink there is - the skin material lays it under the skin (ink a little soft and
# bluish, the skin's own detail through it).
#   python Art/Tattoos/make_tattoos.py     -> Art/Tattoos/<name>.png (1024 x 1024) + previews on skin
import math, os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
S = 1024          # the picture
K = 4             # drawn at four times and scaled down: smooth lines
N = S * K
INK = (24, 27, 34)   # black tattoo ink (a cool near-black)


def canvas(): return Image.new('L', (N, N), 0)


def to_png(alpha_l, rgb, name):
    """alpha (L image at N) and colour (array N x N x 3 or a tuple) -> the RGBA tattoo at S."""
    a = alpha_l.resize((S, S), Image.LANCZOS)
    if isinstance(rgb, tuple): col = Image.new('RGB', (S, S), rgb)
    else: col = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), 'RGB').resize((S, S), Image.LANCZOS)
    im = col.copy(); im.putalpha(a)
    im.save(os.path.join(HERE, name + '.png'))
    return im


def compass_rose():
    """Black and grey: a fine-line compass rose - a 16-point star (north, east, south and west long, each point half solid
    ink and half dotwork-shaded), two fine rings with ticks, a dotwork band between them breaking up the black, an N."""
    c = N / 2
    ink = canvas(); d = ImageDraw.Draw(ink)
    rng = np.random.default_rng(11)
    def pol(r, a): return (c + r * math.sin(a), c - r * math.cos(a))
    lw = 7 * K // 2
    # the rings
    R1, R2, R3 = 0.43 * N, 0.395 * N, 0.24 * N
    for r, w in ((R1, lw), (R2, lw), (R3, lw * 2 // 3)):
        d.ellipse([c - r, c - r, c + r, c + r], outline=255, width=w)
    # ticks on the outer band: every 5 degrees, long ones every 45
    for k in range(72):
        a = math.radians(k * 5)
        l = (R1 - R2) * (0.85 if k % 9 == 0 else 0.45)
        d.line([pol(R1 - 2, a), pol(R1 - l, a)], fill=255, width=lw if k % 9 == 0 else lw // 2)
    # dotwork band inside the inner ring: dots thicker toward the ring (a soft gradient of black)
    dots = canvas(); dd = ImageDraw.Draw(dots)
    for _ in range(26000):
        r = R3 + (R2 - R3) * (1 - math.sqrt(rng.random()))
        a = rng.random() * 2 * math.pi
        t = (r - R3) / (R2 - R3)
        if rng.random() > 0.15 + 0.75 * t: continue
        x, y = pol(r, a); s = K * (1.2 + 1.4 * rng.random())
        dd.ellipse([x - s, y - s, x + s, y + s], fill=255)
    # the star: points (length, half width, count, phase)
    # the points, smallest first and the long ones over them (as a compass rose is drawn): each point half solid ink,
    # half light dotwork, its outline fine; a point covers what lies under it
    tone = canvas(); td = ImageDraw.Draw(tone)            # 0 none, 128 shaded, 255 solid
    lines = canvas(); li = ImageDraw.Draw(lines)
    for L, Wd, n, ph in ((0.21 * N, 0.035 * N, 8, math.pi / 8), (0.30 * N, 0.055 * N, 4, math.pi / 4), (0.40 * N, 0.07 * N, 4, 0.0)):
        for k in range(n):
            a = ph + 2 * math.pi * k / n
            tip, left, right = pol(L, a), pol(Wd, a - math.pi / 2), pol(Wd, a + math.pi / 2)
            ctr = (c, c)
            td.polygon([tip, left, ctr], fill=255)
            td.polygon([tip, right, ctr], fill=128)
            li.polygon([tip, left, ctr, right], fill=0)    # (what's under this point's outline is hidden)
            li.line([tip, left, ctr, right, tip], fill=255, width=lw)
            li.line([ctr, tip], fill=255, width=lw // 2)
    T = np.asarray(tone)
    solid = (T == 255).astype(np.uint8) * 255
    shade_m = (T == 128)
    ys, xs = np.nonzero(shade_m[::K, ::K]); ys *= K; xs *= K
    dotsb = canvas(); db = ImageDraw.Draw(dotsb)
    for x, y in zip(xs, ys):
        # (sparse and fine: the shaded half reads grey beside the solid one - the classic two-tone point)
        if rng.random() < 0.12:
            s = K * (0.7 + 0.6 * rng.random()); db.ellipse([x - s, y - s, x + s, y + s], fill=255)
    shaded = (np.asarray(dotsb) * shade_m).astype(np.uint8)
    # (the band's dots stay off the star)
    dots_a = (np.asarray(dots) * (T == 0)).astype(np.uint8)
    a = np.maximum.reduce([np.asarray(ink) * (T == 0), dots_a, solid, np.asarray(lines), shaded]).astype(np.uint8)
    # the centre: a small ring and a dot; N above the top
    img = Image.fromarray(a.astype(np.uint8)); d = ImageDraw.Draw(img)
    r = 0.028 * N; d.ellipse([c - r, c - r, c + r, c + r], fill=0, outline=255, width=lw)
    r = 0.009 * N; d.ellipse([c - r, c - r, c + r, c + r], fill=255)
    # the N: three strokes
    nx, ny, nh, nw = c, c - 0.465 * N, 0.045 * N, 0.028 * N
    d.line([(nx - nw / 2, ny + nh / 2), (nx - nw / 2, ny - nh / 2), (nx + nw / 2, ny + nh / 2), (nx + nw / 2, ny - nh / 2)], fill=255, width=lw)
    # healed ink: a hair of softness
    img = img.filter(ImageFilter.GaussianBlur(K * 0.6))
    return to_png(img, INK, 'tattoo_compass_rose')


def aurora_mountains():
    """Colour: a small circle - black fine-line mountains with snow lines under northern lights: curtains of green
    rising into violet, folded and uneven, soft watercolour edges; a few stars; the mountains a light grey wash fading
    down, no heavy black block."""
    c = N / 2; R = 0.44 * N
    cs = S / 2; Rs = R / K
    rng = np.random.default_rng(5)
    y, x = np.mgrid[0:S, 0:S].astype(np.float32)
    dist = np.hypot(x - cs, y - cs)
    inside = np.clip((Rs - dist) / 2.0, 0, 1)
    def smooth_noise(cell, sig, seed):
        r = np.random.default_rng(seed)
        im = Image.fromarray((r.random((S // cell + 2, S // cell + 2)) * 255).astype(np.uint8)).resize((S, S), Image.BICUBIC).filter(ImageFilter.GaussianBlur(sig))
        return np.asarray(im, np.float32) / 255.0
    n1, n2, n3 = smooth_noise(16, 8, 1), smooth_noise(6, 3, 2), smooth_noise(40, 14, 3)
    # the curtains' lower edge: a long wave across the sky
    base = cs - 0.02 * S + 0.07 * S * np.sin(x / S * 5.5 + 0.9) + 0.025 * S * np.sin(x / S * 15.0 + 2.0) + 0.03 * S * (n3 - 0.5)
    h = base - y                                          # height above the edge (curtains rise from it)
    folds = 0.5 + 0.5 * np.sin(x / S * 46.0 + 4.0 * n3 + 1.5 * np.sin(x / S * 9.0))
    folds = 0.35 + 0.65 * folds ** 1.6
    rise = np.clip(h / (0.30 * S), 0, 1)
    lower = np.exp(-np.maximum(-h, 0) / (0.012 * S))     # a soft lower edge
    upper = np.clip(1 - rise, 0, 1) ** 0.7                # fading out high up
    glow = lower * upper * folds * (0.85 + 0.3 * (n1 - 0.5))
    # colour up the curtain: green at the edge, teal, then violet at the top
    green, teal, violet = np.array([46, 210, 120], np.float32), np.array([40, 180, 185], np.float32), np.array([150, 80, 210], np.float32)
    t = rise[..., None]
    col = np.where(t < 0.45, green + (teal - green) * (t / 0.45), teal + (violet - teal) * ((t - 0.45) / 0.55))
    # the mountains: a ridge line
    def ridge_at(xx):
        return (cs + 0.16 * S - 0.20 * S * np.maximum(0, 1 - np.abs(xx - (cs - 0.10 * S)) / (0.24 * S)) ** 1.2
                - 0.13 * S * np.maximum(0, 1 - np.abs(xx - (cs + 0.20 * S)) / (0.17 * S)) ** 1.2
                - 0.05 * S * np.maximum(0, 1 - np.abs(xx - (cs + 0.04 * S)) / (0.09 * S)))
    ridge = ridge_at(x)
    below = y > ridge
    # a light grey wash on the mountains, fading downward (no black block); the land under them left bare
    mfade = np.clip(1 - (y - ridge) / (0.16 * S), 0, 1)
    mount_a = below * mfade * 0.38 * (0.9 + 0.2 * (n1 - 0.5)) * inside
    wash_a = np.clip(glow * 1.25, 0, 0.92) * (~below) * inside
    # line art at N: the ring, the ridge, snow lines, stars
    line = canvas(); d = ImageDraw.Draw(line)
    lw = 6 * K // 2
    d.ellipse([c - R, c - R, c + R, c + R], outline=255, width=lw)
    xs = np.arange(0, S, 1.0); rid = ridge_at(xs)
    pts = [(px * K, py * K) for px, py in zip(xs, rid) if math.hypot(px - cs, py - cs) < Rs - 2]
    d.line(pts, fill=255, width=lw, joint='curve')
    for pk, wdt, nlines in ((cs - 0.10 * S, 0.24 * S, 7), (cs + 0.20 * S, 0.17 * S, 5)):
        for k in range(nlines):
            u = (k - (nlines - 1) / 2) / ((nlines - 1) / 2)
            x0 = pk + u * wdt * 0.32; y0 = float(np.interp(x0, xs, rid))
            ln = (0.035 + 0.03 * (1 - abs(u))) * S
            d.line([(x0 * K, y0 * K), ((x0 + u * 0.01 * S) * K, (y0 + ln) * K)], fill=255, width=lw // 2)
    # the horizon: a gentle foothill line below the peaks
    fh = [(px * K, (cs + 0.24 * S + 0.012 * S * math.sin(px / S * 9)) * K) for px in xs if math.hypot(px - cs, cs + 0.24 * S - cs) < Rs - 2]
    d.line(fh, fill=255, width=lw // 2)
    for _ in range(30):
        sx, sy = cs + (rng.random() - 0.5) * 1.6 * Rs, cs - Rs * (0.2 + 0.72 * rng.random())
        if math.hypot(sx - cs, sy - cs) > Rs * 0.9: continue
        s_ = K * (1.5 + 2.5 * rng.random()); d.ellipse([sx * K - s_, sy * K - s_, sx * K + s_, sy * K + s_], fill=255)
    la = np.asarray(line.resize((S, S), Image.LANCZOS), np.float32) / 255.0
    ink_a = np.clip(np.maximum(la, mount_a), 0, 1)
    alpha = np.clip(ink_a + wash_a * (1 - ink_a), 0, 1)
    rgb = (np.array(INK, np.float32)[None, None, :] * ink_a[..., None] + col * (wash_a * (1 - ink_a))[..., None]) / np.maximum(alpha[..., None], 1e-4)
    im = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), 'RGB').filter(ImageFilter.GaussianBlur(0.5))
    im.putalpha(Image.fromarray((alpha * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.5)))
    im.save(os.path.join(HERE, 'tattoo_aurora_mountains.png'))
    return im


def preview(t, name):
    """The tattoo on a plain skin tone, as ink sits under skin (a little soft, multiplied)."""
    skin = np.ones((S, S, 3), np.float32) * np.array([226, 190, 165]) / 255.0
    a = np.asarray(t, np.float32) / 255.0
    ink = a[..., :3]; al = a[..., 3:4] * 0.92
    out = skin * (1 - al) + (skin * ink * 0.95) * al
    Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8)).save(os.path.join(HERE, name + '_on_skin.png'))


if __name__ == '__main__':
    a = compass_rose(); preview(a, 'tattoo_compass_rose')
    b = aurora_mountains(); preview(b, 'tattoo_aurora_mountains')
    print('done')
