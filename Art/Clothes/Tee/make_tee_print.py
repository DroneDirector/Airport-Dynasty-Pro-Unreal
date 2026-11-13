# The tee's first print (owner, 2026-11-13: a very nice custom image with some text, our own design, no brands):
# "NORTHERN SKIES" - an aurora over a mountain ridge and a calm fjord, a jet climbing out across it, the text set in the
# title screen's Audiowide. Drawn here from scratch, 2048 x 2048 with a transparent background, as a screen print would
# be (a few flat inks with soft gradients), so the tee's cotton shows round it. Swap the PNG for any other print.
#   python Art/Clothes/Tee/make_tee_print.py   ->  Art/Clothes/Tee/print_northern_skies.png (+ _preview.png on cotton)
import math, random
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

HERE = Path(__file__).resolve().parent
FONT = HERE.parent.parent.parent / 'Content' / 'Fonts' / 'Audiowide-Regular.ttf'
S = 2048
rng = random.Random(62)

def lerp(a, b, t): return a + (b - a) * t
def mix(c1, c2, t): return tuple(int(lerp(a, b, t)) for a, b in zip(c1, c2))

# the badge: a rounded shield the scene sits in
W, H = 1500, 1150; X0, Y0 = (S - W) // 2, 330
scene = Image.new('RGBA', (W, H), (0, 0, 0, 0))
d = ImageDraw.Draw(scene)
# the night sky: deep navy to a teal glow at the horizon
for y in range(H):
    t = y / H
    d.line([(0, y), (W, y)], fill=mix((8, 14, 40), (22, 70, 92), t ** 1.6) + (255,))
# stars
for _ in range(260):
    x, y = rng.randrange(W), rng.randrange(int(H * 0.55)); r = rng.choice((1, 1, 1, 2, 2, 3))
    d.ellipse((x - r, y - r, x + r, y + r), fill=(235, 240, 255, rng.randrange(120, 255)))
# the aurora: three soft ribbons of green and violet curtains
aur = Image.new('RGBA', (W, H), (0, 0, 0, 0)); ad = ImageDraw.Draw(aur)
for k, (col, base, amp, ph) in enumerate((((60, 255, 170), 0.28, 0.10, 0.0), ((40, 220, 200), 0.36, 0.08, 1.7), ((170, 90, 255), 0.22, 0.07, 3.1))):
    for x in range(0, W, 2):
        t = x / W
        yc = H * (base + amp * math.sin(t * 6.0 + ph) + 0.03 * math.sin(t * 17 + ph * 2))
        length = H * (0.10 + 0.08 * (0.5 + 0.5 * math.sin(t * 11 + ph)))
        for j in range(int(length)):
            a = int(150 * (1 - j / length) ** 1.5 * (0.6 + 0.4 * math.sin(t * 40 + k)))
            ad.point((x, int(yc + j)), fill=col + (max(0, a),)); ad.point((x + 1, int(yc + j)), fill=col + (max(0, a),))
aur = aur.filter(ImageFilter.GaussianBlur(6))
scene = Image.alpha_composite(scene, aur); d = ImageDraw.Draw(scene)
# the mountains: two ridges, a snow-capped peak
def ridge(yb, amp, seed, col):
    r_ = random.Random(seed); pts = [(0, H)]; y = yb
    for x in range(0, W + 40, 40):
        y = yb - amp * abs(math.sin(x / W * math.pi * 1.6 + seed)) - r_.uniform(0, amp * 0.25)
        pts.append((x, y))
    pts.append((W, H)); d.polygon(pts, fill=col)
    return pts
ridge(H * 0.70, H * 0.20, 3, (18, 32, 52, 255))
far = ridge(H * 0.78, H * 0.30, 7, (12, 22, 36, 255))
# snow on the near ridge's tops
for (x1, y1), (x2, y2) in zip(far[1:-2], far[2:-1]):
    if min(y1, y2) < H * 0.62:
        d.polygon([(x1, y1), (x2, y2), (x2, y2 + 18), (x1, y1 + 22)], fill=(225, 235, 245, 255))
# the fjord: still water reflecting the aurora
water_top = int(H * 0.80)
d.rectangle((0, water_top, W, H), fill=(10, 26, 44, 255))
refl = aur.crop((0, 0, W, water_top)).transpose(Image.FLIP_TOP_BOTTOM).resize((W, H - water_top))
refl.putalpha(refl.getchannel('A').point(lambda v: int(v * 0.45)))
scene.alpha_composite(refl, (0, water_top)); d = ImageDraw.Draw(scene)
for y in range(water_top + 8, H, 14):
    d.line([(rng.randrange(0, W // 3), y), (rng.randrange(2 * W // 3, W), y)], fill=(120, 200, 210, 40), width=2)
# the jet climbing out: a clean silhouette with its contrail
def jet(cx, cy, s, ang):
    pts = [(-1.0, 0), (-0.75, -0.06), (0.55, -0.07), (0.95, -0.02), (1.0, 0), (0.95, 0.02), (0.55, 0.07), (-0.75, 0.06)]
    wing = [(0.05, 0.05), (-0.25, 0.85), (-0.42, 0.85), (-0.20, 0.06)]
    wing2 = [(0.05, -0.05), (-0.25, -0.85), (-0.42, -0.85), (-0.20, -0.06)]
    tail = [(-0.80, 0.03), (-0.98, 0.32), (-1.05, 0.32), (-0.98, 0.03)]
    ca, sa = math.cos(ang), math.sin(ang)
    for poly in (wing, wing2, tail, pts):
        d.polygon([(cx + s * (x * ca - y * sa), cy + s * (x * sa + y * ca)) for x, y in poly], fill=(240, 244, 250, 255))
ang = math.radians(-18)
for i in range(70):
    t = i / 70
    x = W * 0.70 - 520 * t * math.cos(ang); y = H * 0.30 - 520 * t * math.sin(ang)
    d.ellipse((x - 3 - 6 * t, y - 3 - 6 * t, x + 3 + 6 * t, y + 3 + 6 * t), fill=(230, 240, 255, int(140 * (1 - t))))
jet(W * 0.74, H * 0.28, 70, ang)
# the badge's shape: a wide rounded shield, with a thin light outline
mask = Image.new('L', (W, H), 0); md = ImageDraw.Draw(mask)
md.rounded_rectangle((0, 0, W, H - 160), 90, fill=255)
md.polygon([(0, H - 260), (W, H - 260), (W // 2, H)], fill=255)
scene.putalpha(ImageChops.multiply(scene.getchannel('A'), mask))
edge = mask.filter(ImageFilter.MaxFilter(15))
out = Image.new('RGBA', (S, S), (0, 0, 0, 0))
ring = Image.new('RGBA', (W, H), (232, 240, 248, 255)); ring.putalpha(ImageChops.subtract(edge, mask.filter(ImageFilter.MinFilter(3))))
out.alpha_composite(scene, (X0, Y0)); out.alpha_composite(ring, (X0, Y0))
# the text: NORTHERN SKIES above, ICELAND · EST. 2026 below
od = ImageDraw.Draw(out)
def text(t, size, y, col, spacing=0):
    f = ImageFont.truetype(str(FONT), size)
    w = sum(f.getlength(ch) for ch in t) + spacing * (len(t) - 1); x = (S - w) / 2
    for ch in t:
        od.text((x, y), ch, font=f, fill=col); x += f.getlength(ch) + spacing
text('NORTHERN SKIES', 150, 110, (16, 30, 58, 255), 6)   # navy ink: it shows on a light tee
text('ICELAND  ·  EST. 2026', 88, Y0 + H + 50, (18, 120, 112, 255), 16)
# a printed look: tiny speckle where the ink is thin, so it reads as a screen print, not a sticker
a = out.getchannel('A'); noise = Image.effect_noise((S, S), 40).point(lambda v: 255 if v > 30 else 200)
out.putalpha(ImageChops.multiply(a, noise))
out.save(HERE / 'print_northern_skies.png')
prev = Image.new('RGBA', (S, S), (238, 238, 234, 255)); prev.alpha_composite(out); prev.convert('RGB').resize((1024, 1024)).save(HERE / 'print_northern_skies_preview.png')
print('PRINT DONE')
