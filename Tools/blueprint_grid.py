# The owner's airport blueprint (Art/Maps/Airport-Only-v11.png, north up) with the airport's own plan grid over it
# (plan metres, as Data/airfield.json and Data/roads.json: x along the runway, y across it), to read the roads' and
# other features' positions off the drawing exactly. Calibrated on the runway's two ends (2026-11-14): the runway end
# 08 (plan x -1919, y 646) at image (508.7, 2755.0), end 26 (x 2081) at (5032.3, 927.7) - 1.2197 px per metre, the
# runway at 22 degrees (heading 068); checked on the apron (its centre lands where the game has it).
#   python Tools/blueprint_grid.py <plan x> <plan y> <half size m> <grid m> <out.png> [scale]
#   python Tools/blueprint_grid.py --to-plan <image x> <image y>     (one point, image pixels -> plan metres)
import math, sys
from PIL import Image, ImageDraw, ImageFont

Image.MAX_IMAGE_PIXELS = None
SRC = r'C:/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal/Art/Maps/Airport-Only-v11.png'
L = (508.7, 2755.0); R = (5032.3, 927.7)
dx, dy = R[0] - L[0], R[1] - L[1]
PXM = math.hypot(dx, dy) / 4000.0
U = (dx / math.hypot(dx, dy), dy / math.hypot(dx, dy))      # plan +x in the image
V = (U[1], -U[0])                                            # plan +y in the image (up-left: away from the terminal)

def to_img(x, y):
    a, b = (x + 1919.0) * PXM, (y - 646.0) * PXM
    return L[0] + a * U[0] + b * V[0], L[1] + a * U[1] + b * V[1]

def to_plan(ix, iy):
    px, py = ix - L[0], iy - L[1]
    return -1919.0 + (px * U[0] + py * U[1]) / PXM, 646.0 + (px * V[0] + py * V[1]) / PXM

if __name__ == '__main__':
    if sys.argv[1] == '--to-plan':
        print('%.1f %.1f' % to_plan(float(sys.argv[2]), float(sys.argv[3]))); sys.exit()
    cx, cy, half, step, out = float(sys.argv[1]), float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), sys.argv[5]
    sc = float(sys.argv[6]) if len(sys.argv) > 6 else 2.0
    im = Image.open(SRC).convert('RGB')
    # the crop: the axis-aligned box round the plan square, then the grid drawn on it
    cs = [to_img(cx + sx * half, cy + sy * half) for sx in (-1, 1) for sy in (-1, 1)]
    x0, y0 = int(min(c[0] for c in cs)), int(min(c[1] for c in cs)); x1, y1 = int(max(c[0] for c in cs)), int(max(c[1] for c in cs))
    c = im.crop((x0, y0, x1, y1)).resize((int((x1 - x0) * sc), int((y1 - y0) * sc)), Image.LANCZOS)
    d = ImageDraw.Draw(c)
    try: f = ImageFont.truetype(r'C:/Program Files/Blender Foundation/Blender 3.3/3.3/datafiles/fonts/droidsans.ttf', 13)
    except Exception: f = None
    k0 = math.floor((cx - half) / step); k1 = math.ceil((cx + half) / step)
    j0 = math.floor((cy - half) / step); j1 = math.ceil((cy + half) / step)
    P = lambda x, y: ((to_img(x, y)[0] - x0) * sc, (to_img(x, y)[1] - y0) * sc)
    for k in range(k0, k1 + 1):
        x = k * step; d.line([P(x, j0 * step), P(x, j1 * step)], fill=(255, 40, 40), width=1)
        d.text(P(x, cy - half * 0.95), 'x%d' % x, fill=(255, 80, 80), font=f)
    for j in range(j0, j1 + 1):
        y = j * step; d.line([P(k0 * step, y), P(k1 * step, y)], fill=(40, 200, 255), width=1)
        d.text(P(cx - half * 0.95, y), 'y%d' % y, fill=(80, 220, 255), font=f)
    c.save(out); print(out, c.size)
