# The owner's town plan (Art/Maps/Town-Plan-v7.png, north up, 0.1728 m a pixel) placed on the game's plan, and a tracer
# for its streets (owner, 2026-11-15: "build the rest of the roads"). The plan's frame is the town box on the overview
# drawing (Art/Maps/Airport-Road-Town-Overview-v11.png, 1738 x 1438 m, measured: Tools/blueprint_grid.py's overview
# calibration), so a town pixel goes to the overview's pixels and from there to plan metres - the road from the airport
# traced off the overview meets the town's roundabout (9) where the town plan has it.
#   python Tools/town_plan.py <name> <start u> <start v> <toward u> <toward v> <stop u> <stop v>   (town plan pixels)
# writes the traced street (plan metres) into Data/road-traces.json under 'town-<name>'.
import json, math, os, sys
os.environ['ADP_MAP'] = 'overview'
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blueprint_grid as B
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'Maps', 'Town-Plan-v7.png')
# the town plan's frame (its border line's middle, town pixels) and the overview's town box (overview pixels)
FRAME = (237.0, 171.5, 10301.0, 8485.5)
BOX = (9926.0, 4723.0, 12047.0, 6478.0)
KX = (BOX[2] - BOX[0]) / (FRAME[2] - FRAME[0]); KY = (BOX[3] - BOX[1]) / (FRAME[3] - FRAME[1])
MPX = KX / B.PXM          # metres a town pixel (about 0.1728)

def to_plan(u, v):
    return B.to_plan(BOX[0] + (u - FRAME[0]) * KX, BOX[1] + (v - FRAME[1]) * KY)

def to_town(x, y):
    ix, iy = B.to_img(x, y)
    return FRAME[0] + (ix - BOX[0]) / KX, FRAME[1] + (iy - BOX[1]) / KY

_IM = None
def image():
    global _IM
    if _IM is None: _IM = Image.open(SRC).convert('RGB')
    return _IM

def is_road(c):
    # the town's asphalt (74, 78, 84), its yellow middle line, white dashes and edge lines
    return (abs(c[0] - 74) <= 9 and abs(c[1] - 78) <= 9 and abs(c[2] - 84) <= 9) or (c[0] > 220 and c[1] > 190 and c[2] < 140) or (c[0] > 225 and c[1] > 225 and c[2] > 225)

def trace(start, toward, stop, step_m=6.0, half_m=9.0, max_steps=3000):
    im = image(); W, H = im.size; px = im.load()
    x, y = start; dx, dy = toward[0] - x, toward[1] - y; n = math.hypot(dx, dy); dx, dy = dx / n, dy / n
    step = step_m / MPX; half = int(half_m / MPX); pts = [to_plan(x, y)]; lost = 0
    for _ in range(max_steps):
        nx, ny = x + dx * step, y + dy * step
        px_, py_ = -dy, dx; hits = []
        for t in range(-half, half + 1, 2):
            qx, qy = int(round(nx + px_ * t)), int(round(ny + py_ * t))
            if 0 <= qx < W and 0 <= qy < H and is_road(px[qx, qy]): hits.append(t)
        if len(hits) >= 6:
            runs, cur = [], [hits[0]]
            for h in hits[1:]:
                if h - cur[-1] <= 4: cur.append(h)
                else: runs.append(cur); cur = [h]
            runs.append(cur)
            run = min(runs, key=lambda r: abs(sum(r) / len(r)))
            off = sum(run) / len(run)
            nx, ny = nx + px_ * off, ny + py_ * off; lost = 0
        else:
            lost += 1
            if lost > 6: print('# lost the street at town px', int(nx), int(ny)); break
        ndx, ndy = nx - x, ny - y; m = math.hypot(ndx, ndy)
        if m > 0: dx, dy = 0.6 * dx + 0.4 * ndx / m, 0.6 * dy + 0.4 * ndy / m; mm = math.hypot(dx, dy); dx, dy = dx / mm, dy / mm
        x, y = nx, ny
        if lost == 0: pts.append(tuple(round(v, 2) for v in to_plan(x, y)))
        if math.hypot(x - stop[0], y - stop[1]) < step * 1.2: break
    return pts

if __name__ == '__main__':
    name = sys.argv[1]; a = [float(v) for v in sys.argv[2:8]]
    p = trace((a[0], a[1]), (a[2], a[3]), (a[4], a[5]))
    f = os.path.join(ROOT, 'Data', 'road-traces.json'); T = json.load(open(f, encoding='utf-8'))
    T['town-' + name] = p; json.dump(T, open(f, 'w', encoding='utf-8'), indent=1)
    print('town-%s: %d points, %.0f m' % (name, len(p), sum(math.dist(p[i], p[i + 1]) for i in range(len(p) - 1))))
