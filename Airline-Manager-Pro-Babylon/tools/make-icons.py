# The game's app icon: a glossy neon die (rendered in Blender: cyan top, magenta and violet sides, rounded edges) with
# a white airliner climbing across it, on a deep violet plate with a magenta glow and a faint neon floor grid. Writes the Android icons
# (scripts/android/res), the PC icons (pc/app.ico, and the Unreal edition's Build/Windows/Application.ico and
# Installer/app.ico) and www/icon.png. Drawn here, our own.
import os
from PIL import Image, ImageDraw, ImageFilter
here = os.path.dirname(os.path.abspath(__file__)); top = os.path.join(here, '..')
BG = (22, 12, 52, 255)
CUBE = Image.open(os.path.join(here, 'icon-cube.png')).convert('RGBA')   # rendered in Blender (Art/Icon/make_cube_icon.py)


def backdrop(S, shape):
    """Deep violet, lighter in the middle, a soft magenta glow behind the die and a faint neon floor grid."""
    import math
    bg = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    g = Image.radial_gradient('L').resize((S, S), Image.LANCZOS)          # 0 in the middle, 255 at the edge
    inner, outer = (74, 34, 138), (14, 6, 38)
    col = Image.merge('RGB', [g.point(lambda v, a=inner[i], b=outer[i]: int(a + (b - a) * min(1, v / 255) ** 0.8)) for i in range(3)])
    grid = Image.new('RGBA', (S, S), (0, 0, 0, 0)); gd = ImageDraw.Draw(grid)
    hz = S * 0.70; w = max(1, int(S * 0.004))
    for i in range(-12, 13):
        gd.line([(S / 2 + i * S * 0.02, hz), (S / 2 + i * S * 0.16, S)], fill=(255, 63, 184, 70), width=w)
    y = hz; step = S * 0.012
    while y < S:
        gd.line([(0, y), (S, y)], fill=(255, 63, 184, 60), width=w); step *= 1.35; y += step
    fade = Image.linear_gradient('L').resize((S, S)).point(lambda v: max(0, min(255, int((v - 180) * 3.2))))
    grid.putalpha(Image.composite(grid.getchannel('A'), Image.new('L', (S, S), 0), fade))
    plate = col.convert('RGBA'); plate.alpha_composite(grid)
    mask = Image.new('L', (S, S), 0); md = ImageDraw.Draw(mask)
    if shape == 'square': md.rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.22), fill=255)
    else: md.ellipse([0, 0, S - 1, S - 1], fill=255)
    bg.paste(plate, (0, 0), mask)
    return bg


def art(S, bg):
    import math
    im = backdrop(S, bg) if bg in ('square', 'round') else Image.new('RGBA', (S, S), (0, 0, 0, 0))
    k = 0.62 if bg == 'fg' else 0.78; c = S / 2; r = S * k / 2
    # the glow behind the die
    glow = Image.new('RGBA', (S, S), (0, 0, 0, 0)); gd = ImageDraw.Draw(glow)
    gd.ellipse([c - r * 1.05, c - r * 1.05, c + r * 1.05, c + r * 1.05], fill=(255, 63, 184, 120))
    im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(S * 0.07)))
    # the die (rendered for k = 0.78; scaled for the others)
    n = int(S * k / 0.78); cube = CUBE.resize((n, n), Image.LANCZOS)
    im.alpha_composite(cube, (int(c - n / 2), int(c - n / 2)))
    # a white airliner climbing across the cube
    pl = Image.new('RGBA', (S, S), (0, 0, 0, 0)); pd = ImageDraw.Draw(pl); u = r / 100
    body = [(-70, -6), (60, -6), (80, 0), (60, 6), (-70, 6), (-78, 0)]
    wing = [(-5, 0), (25, 0), (-15, 46), (-30, 46)]; wing2 = [(-5, 0), (25, 0), (-15, -46), (-30, -46)]
    tail = [(-62, 0), (-50, 0), (-72, 22), (-80, 22)]; tail2 = [(-62, 0), (-50, 0), (-72, -22), (-80, -22)]
    a = math.radians(-24)
    def tr(p): x, y = p; return (c + (x * math.cos(a) - y * math.sin(a)) * u * 0.95, c + (x * math.sin(a) + y * math.cos(a)) * u * 0.95 + r * 0.05)
    for poly in (wing, wing2, tail, tail2, body): pd.polygon([tr(p) for p in poly], fill=(255, 255, 255, 255))
    sh = pl.filter(ImageFilter.GaussianBlur(S * 0.012)); sh2 = Image.new('RGBA', (S, S), (10, 0, 30, 0)); sh2.putalpha(sh.getchannel('A').point(lambda v: int(v * 0.7)))
    im.alpha_composite(sh2); im.alpha_composite(pl)
    return im
res = os.path.join(top, 'scripts', 'android', 'res')
for dpi, n, f in [('mdpi', 48, 108), ('hdpi', 72, 162), ('xhdpi', 96, 216), ('xxhdpi', 144, 324), ('xxxhdpi', 192, 432)]:
    dd = os.path.join(res, 'mipmap-' + dpi); os.makedirs(dd, exist_ok=True)
    art(1024, 'square').resize((n, n), Image.LANCZOS).save(os.path.join(dd, 'ic_launcher.png'))
    art(1024, 'round').resize((n, n), Image.LANCZOS).save(os.path.join(dd, 'ic_launcher_round.png'))
    art(1024, 'fg').resize((f, f), Image.LANCZOS).save(os.path.join(dd, 'ic_launcher_foreground.png'))
with open(os.path.join(res, 'values', 'ic_launcher_background.xml'), 'w') as fh:
    fh.write('<?xml version="1.0" encoding="utf-8"?>\n<resources>\n    <color name="ic_launcher_background">#160C34</color>\n</resources>\n')
big = art(1024, 'square'); big.save(os.path.join(top, 'pc', 'app.ico'), sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
big.resize((512, 512), Image.LANCZOS).save(os.path.join(top, 'www', 'icon.png'))
ICO = [(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (96, 96), (128, 128), (256, 256)]
unreal = os.path.join(top, '..')
for p in (os.path.join(unreal, 'Build', 'Windows', 'Application.ico'), os.path.join(unreal, 'Installer', 'app.ico')):
    big.save(p, sizes=ICO)
big.save(os.path.join(unreal, 'Art', 'Icon', 'Icon-Cube-1024.png'))
print('icons written')
