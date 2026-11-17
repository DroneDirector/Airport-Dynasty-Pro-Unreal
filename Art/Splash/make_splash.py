# Pixel Norma company splash (to-do 64): the owner's own logo, animated, on black.
#   python make_splash.py [width] [out_dir]      width 3840 = 4K final, 1920 = quick preview
# Source: PixelNorma-source.jpg, sharpened x4 with Real-ESRGAN (BSD-3, commercial use OK):
#   realesrgan-ncnn-vulkan -i PixelNorma-source.jpg -o up-realesrgan-x4plus-anime.png -n realesrgan-x4plus-anime -s 4
# Timeline (30 fps, 7.5 s):
#   0.0-2.0  sparks in the logo's own colours fly in from the dark and gather where the logo will be
#   1.2-2.8  the fox and rock build up from big pixels to razor sharp ("pixel")
#   2.6-4.4  the letters of PIXEL then NORMA switch on one by one with a neon flicker
#   4.4-5.4  a light sweep across the whole logo, glow pulse
#   5.4-6.8  hold, the pixel trail sparkles
#   6.8-7.5  fade to black
import sys, os, math
import numpy as np, cv2

HERE = os.path.dirname(os.path.abspath(__file__))
W = int(sys.argv[1]) if len(sys.argv) > 1 else 1920
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, 'frames_%d' % W)
H = W * 9 // 16
FPS, DUR = 30, 7.5
NF = int(FPS * DUR)
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(7)

# ---- the logo at final size: about half the screen height, centred ----
up = cv2.imread(os.path.join(HERE, 'up-realesrgan-x4plus-anime.png')).astype(np.float32) / 255.0  # BGR, x4 of source
SX0, SX1, SY0, SY1 = 270, 1125, 60, 768            # source crop (logo bbox 330..1063 x 107..735, plus glow room)
crop = up[SY0 * 4:SY1 * 4, SX0 * 4:SX1 * 4]
LH = int(H * 0.86)                                  # logo height on screen (logo itself ~70% of the height, ~55% of the width)
s = LH / crop.shape[0]
LW = int(round(crop.shape[1] * s))
logo = cv2.resize(crop, (LW, LH), interpolation=cv2.INTER_AREA)
logo = np.clip((logo - 0.006) / 0.994, 0, 1)        # true black around it, no grey box edge
# feather the canvas edges so no glow ever ends in a straight line
fe = np.ones((LH, LW), np.float32); n = int(LH * 0.06)
ramp = (np.linspace(0, 1, n) ** 2).astype(np.float32)
fe[:n] *= ramp[:, None]; fe[-n:] *= ramp[::-1, None]; fe[:, :n] *= ramp[None]; fe[:, -n:] *= ramp[None, ::-1]
logo *= fe[..., None]
OX, OY = (W - LW) // 2, (H - LH) // 2 + int(H * 0.01)
def src(x, y): return (x - SX0) / (SX1 - SX0) * LW, (y - SY0) / (SY1 - SY0) * LH   # source px -> logo px

lum = logo.max(2)
yy, xx = np.mgrid[0:LH, 0:LW].astype(np.float32)
# letters: each one's own shape cut from the sharpened logo (its outline and holes, no boxes) - only the letters go
# dark and switch on; the sign panel, its outline and everything else stay as they are.
#   PIXEL: the bright letters are separate blobs; NORMA: bright pixels split at the gaps between its letters.
def letter_masks():
    x0, y0, x1, y1 = 440 * 4, 484 * 4, 990 * 4, 712 * 4          # lettering area in the x4 image
    c = up[y0:y1, x0:x1]; Lc = c.max(2)
    bright = (Lc > 0.33).astype(np.uint8)
    n, lab, st, _ = cv2.connectedComponentsWithStats((Lc > 0.4).astype(np.uint8), 8)
    pix = sorted([i for i in range(1, n) if st[i, 4] > 30000 and st[i, 1] < 120 and st[i, 3] < 420], key=lambda i: st[i, 0])
    assert len(pix) == 5, pix
    subs = [(lab == i).astype(np.float32) for i in pix]           # PIXEL: each letter's own blob, nothing else in its box
    cuts = [218, 562, 875, 1212, 1586, 1938]                        # NORMA: gaps between N | O | R | M | A
    # NORMA: each letter's outline and holes traced by hand (norma_outlines.py) - exactly the letter, nothing around it
    sys.path.insert(0, HERE); from norma_outlines import NORMA
    for outer, holes in NORMA:
        q = np.zeros(Lc.shape, np.uint8)
        cv2.fillPoly(q, [np.array(outer, np.int32)], 255, cv2.LINE_AA)
        for h in holes: cv2.fillPoly(q, [np.array(h, np.int32)], 0, cv2.LINE_AA)
        subs.append(q.astype(np.float32) / 255.0)
    nodil = set()                                                   # grown a hair like PIXEL: covers the light rim edge
    ms = []
    for k_, sub in enumerate(subs):
        g_ = 11 if k_ >= 5 else 7                                    # NORMA a bit more: its rim glows wider
        sub = cv2.dilate(sub, np.ones((g_, g_), np.uint8))           # the letter's own edge and inner glow
        m = np.zeros(up.shape[:2], np.float32); m[y0:y1, x0:x1] = sub
        m = m[SY0 * 4:SY1 * 4, SX0 * 4:SX1 * 4]
        m = cv2.resize(m, (LW, LH), interpolation=cv2.INTER_AREA)
        ms.append(np.clip(cv2.GaussianBlur(m, (0, 0), max(0.6, LH * 0.0006)), 0, 1))
    return ms
LET = letter_masks()
OFF = 0.06                                                        # an unlit letter: off, no glow, just its dark tube

def smooth(t, a, b):
    x = min(max((t - a) / (b - a), 0.0), 1.0)
    return x * x * (3 - 2 * x)

# ---- sparks: targets on bright logo pixels, colour from the logo ----
NS = 1400
pts = np.argwhere(lum > 0.45)
pick = pts[rng.choice(len(pts), NS, replace=True, p=None)]
tgt = np.stack([pick[:, 1] + OX, pick[:, 0] + OY], 1).astype(np.float32)
col = logo[pick[:, 0], pick[:, 1]] * 1.4
ang = rng.uniform(0, 2 * math.pi, NS)
rad = rng.uniform(0.55, 1.1, NS) * W
start = np.stack([W / 2 + np.cos(ang) * rad, H / 2 + np.sin(ang) * rad * 0.7], 1).astype(np.float32)
swirl = rng.uniform(-1.2, 1.2, NS)
t0 = rng.uniform(0.0, 0.9, NS); t1 = t0 + rng.uniform(0.9, 1.3, NS)
psz = np.maximum(1, (rng.uniform(0.0016, 0.004, NS) * W)).astype(int)

# trail pixels (the fox's breath and the tail swirl) sparkle during the hold
def region_mask(x0, y0, x1, y1):
    a, b = src(x0, y0), src(x1, y1)
    return ((xx >= a[0]) & (xx < b[0]) & (yy >= a[1]) & (yy < b[1])).astype(np.float32)
trail = np.clip(region_mask(890, 100, 1040, 230) + region_mask(420, 190, 610, 300), 0, 1)
tw_seed = rng.uniform(0, 2 * math.pi, (LH // 6 + 1, LW // 6 + 1)).astype(np.float32)

def bloom(img, sig):
    sm = cv2.resize(img, (W // 4, H // 4), interpolation=cv2.INTER_AREA)
    sm = cv2.GaussianBlur(sm, (0, 0), sig / 4)
    return cv2.resize(sm, (W, H), interpolation=cv2.INTER_LINEAR)

def pixelate(img, block):
    if block <= 1.01: return img
    h, w = img.shape[:2]
    sw, sh = max(1, int(w / block)), max(1, int(h / block))
    sm = cv2.resize(img, (sw, sh), interpolation=cv2.INTER_AREA)
    return cv2.resize(sm, (w, h), interpolation=cv2.INTER_NEAREST)

ONLY = [int(v) for v in os.environ.get('SPLASH_FRAMES', '').split(',') if v]   # test: just these frames
for f in (ONLY or range(NF)):
    t = f / FPS
    frame = np.zeros((H, W, 3), np.float32)

    # letters: one by one, each flickers on like neon; until then only the letters themselves are dark
    dim = np.ones((LH, LW), np.float32)
    for i, m in enumerate(LET):
        on = 2.6 + i * 0.17
        x = (t - on) / 0.32
        if x <= 0: v = 0.0
        elif x >= 1: v = 1.0
        else: v = [0.6, 0.0, 0.9, 0.1, 0.3, 1.0, 0.7, 1.0][int(x * 8)]
        if v < 1: dim *= 1 - m * (1 - (OFF + (1 - OFF) * v))
    # the whole logo builds up from big pixels, each step snapping to a finer grid
    k = smooth(t, 1.2, 2.8)
    if k > 0:
        steps = [96, 64, 40, 24, 14, 8, 4, 2, 1]
        block = steps[min(int(k * len(steps)), len(steps) - 1)] * W / 3840
        L = pixelate(logo * dim[..., None], block) * min(1.0, k * 2.2)
    else:
        L = np.zeros_like(logo)
    # light sweep
    sw = smooth(t, 4.4, 5.4)
    if 0 < sw < 1:
        band = np.exp(-(((xx + yy * 0.45) / (LW + LH * 0.45) - (sw * 1.4 - 0.2)) / 0.05) ** 2)
        L = L * (1 + 0.9 * band[..., None])
    # sparkle on the pixel trails during the hold
    sp = smooth(t, 5.0, 5.4) * (1 - smooth(t, 6.6, 7.0))
    if sp > 0:
        tw = np.sin(tw_seed * 3 + t * 9) * 0.5 + 0.5
        tw = cv2.resize(tw ** 6, (LW, LH), interpolation=cv2.INTER_NEAREST)
        L = L * (1 + sp * 0.8 * (tw * trail)[..., None])

    frame[OY:OY + LH, OX:OX + LW] = L

    # sparks
    if t < 2.8:
        sb = np.zeros((H, W, 3), np.float32)
        u = np.clip((t - t0) / (t1 - t0), 0, 1)
        e = 1 - (1 - u) ** 3
        live = (t >= t0) & (u < 1.0)
        a = (np.sin(u * math.pi) ** 0.7) * live
        aa = ang + swirl * (1 - e) * 1.6
        r = (1 - e)
        px = tgt[:, 0] + (start[:, 0] - tgt[:, 0]) * r * np.cos(swirl * (1 - e)) - (start[:, 1] - tgt[:, 1]) * r * np.sin(swirl * (1 - e)) * 0.3
        py = tgt[:, 1] + (start[:, 1] - tgt[:, 1]) * r
        for i in np.nonzero(a > 0.02)[0]:
            x, y, z = int(px[i]), int(py[i]), psz[i]
            if -z < x < W and -z < y < H:
                cv2.rectangle(sb, (x, y), (x + z, y + z), (col[i] * a[i]).tolist(), -1)
        frame += sb + bloom(sb, W * 0.006) * 2.0

    # glow (neon) + pulse after the sweep
    # the logo already glows: only a light extra glow while it builds, then a short pulse after the sweep
    g = 0.10 + 0.25 * (1 - smooth(t, 2.0, 4.4)) + 0.30 * math.exp(-((t - 5.0) / 0.35) ** 2)
    frame += bloom(frame, W * 0.012) * g

    fade = 1 - smooth(t, 6.8, 7.5)
    frame *= fade
    # dither before 8-bit: no banding in the glow
    frame = frame * 255 + rng.uniform(-0.5, 0.5, frame.shape).astype(np.float32)
    cv2.imwrite(os.path.join(OUT, 'f%04d.png' % f), np.clip(frame, 0, 255).astype(np.uint8))
    if f % 30 == 0: print('frame', f, '/', NF, flush=True)
print('done', OUT)
