# Turns the Blender render into the game's icon files: rounded corners, a thin gold rim, and every size Windows uses.
# Writes Icon-1024.png and Icon-256.png here, Build/Windows/Application.ico (the game's exe, so the desktop shortcut and
# the window) and Installer/app.ico (the installer).
import os
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
S = 1024
art = Image.open(os.path.join(HERE, 'render.png')).convert('RGB').resize((S, S), Image.LANCZOS)

# #region ── Rounded plate with a gold rim ──
R = 200            # corner radius
RIM = 22           # gold rim width
mask = Image.new('L', (S * 4, S * 4), 0)
ImageDraw.Draw(mask).rounded_rectangle((0, 0, S * 4 - 1, S * 4 - 1), radius=R * 4, fill=255)
mask = mask.resize((S, S), Image.LANCZOS)
inner = Image.new('L', (S * 4, S * 4), 0)
ImageDraw.Draw(inner).rounded_rectangle((RIM * 4, RIM * 4, (S - RIM) * 4 - 1, (S - RIM) * 4 - 1), radius=(R - RIM) * 4, fill=255)
inner = inner.resize((S, S), Image.LANCZOS)
# the rim: a gold gradient, lighter at the top
gold = Image.new('RGB', (S, S))
gd = ImageDraw.Draw(gold)
for y in range(S):
    t = y / (S - 1)
    c = (int(255 - 70 * t), int(214 - 90 * t), int(120 - 80 * t))
    gd.line([(0, y), (S, y)], fill=c)
icon = Image.composite(art, gold, inner)
# a thin dark line between the rim and the picture
edge = inner.filter(ImageFilter.FIND_EDGES).filter(ImageFilter.GaussianBlur(1.2))
icon = Image.composite(Image.new('RGB', (S, S), (20, 14, 6)), icon, edge.point(lambda v: min(255, v * 3)))
icon = icon.convert('RGBA'); icon.putalpha(mask)
# #endregion

icon.save(os.path.join(HERE, 'Icon-1024.png'))
icon.resize((256, 256), Image.LANCZOS).save(os.path.join(HERE, 'Icon-256.png'))


def small(n):
    im = icon.resize((n, n), Image.LANCZOS)
    if n <= 48:
        im = im.filter(ImageFilter.UnsharpMask(radius=0.8, percent=90, threshold=0))
    return im

sizes = [256, 128, 96, 64, 48, 40, 32, 24, 20, 16]
frames = {n: small(n) for n in sizes}
for path in (os.path.join(ROOT, 'Build', 'Windows', 'Application.ico'), os.path.join(ROOT, 'Installer', 'app.ico')):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    frames[256].save(path, format='ICO', sizes=[(n, n) for n in sizes], append_images=[frames[n] for n in sizes[1:]])
    print('wrote', path)
# a preview of the small sizes side by side, on light and dark backgrounds
pv = Image.new('RGBA', (900, 360), (240, 240, 240, 255))
ImageDraw.Draw(pv).rectangle((0, 180, 900, 360), fill=(28, 30, 36, 255))
x = 20
for n in (256, 64, 48, 32, 24, 16):
    for yb in (0, 180):
        pv.alpha_composite(frames[n] if n in frames else small(n), (x, yb + (180 - min(n, 160)) // 2)) if n <= 160 else None
    if n <= 160: x += n + 30
big = frames[128]
pv.alpha_composite(big, (x + 10, 26)); pv.alpha_composite(big, (x + 10, 206))
pv.save(os.path.join(HERE, 'Icon-sizes-preview.png'))
