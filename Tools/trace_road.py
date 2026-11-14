# Traces a road on the owner's overview drawing (Art/Maps/Airport-Road-Town-Overview-v11.png) into plan metres, so the
# game's road follows the drawing exactly (owner, 2026-11-14: "make it follow perfectly"). The drawing's roads are a grey
# fill (75, 79, 85) between dark edges: from a start point and heading, it steps ~8 m at a time, looks across the road
# at each step and takes the middle of the grey there, and steers by it; it stops near a stop point (or on losing the
# road). Prints/returns the points in plan metres (Tools/blueprint_grid.py's calibration, ADP_MAP=overview).
#   python Tools/trace_road.py <start x> <start y> <heading x> <heading y> <stop x> <stop y>     (plan metres)
import math, os, sys
os.environ['ADP_MAP'] = 'overview'
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blueprint_grid as B
from PIL import Image

Image.MAX_IMAGE_PIXELS = None
_IM = None
def image():
    global _IM
    if _IM is None: _IM = Image.open(B.SRC).convert('RGB')
    return _IM

def is_road(c):
    return abs(c[0] - 75) <= 10 and abs(c[1] - 79) <= 10 and abs(c[2] - 85) <= 10 or (c[0] > 150 and c[1] > 150 and c[2] > 150 and abs(c[0] - c[2]) < 30)

def trace(start, head, stop, step_m=8.0, half_px=22, max_steps=2000):
    im = image(); W, H = im.size; px = im.load()
    x, y = B.to_img(*start)
    hx, hy = B.to_img(start[0] + head[0], start[1] + head[1]); dx, dy = hx - x, hy - y
    n = math.hypot(dx, dy); dx, dy = dx / n, dy / n
    sx, sy = B.to_img(*stop)
    step = step_m * B.PXM; pts = [start]; lost = 0
    for _ in range(max_steps):
        nx, ny = x + dx * step, y + dy * step
        # across the road: the grey pixels' middle (the dashed centre line counts as road)
        px_, py_ = -dy, dx; hits = []
        for t in range(-half_px, half_px + 1):
            qx, qy = int(round(nx + px_ * t)), int(round(ny + py_ * t))
            if 0 <= qx < W and 0 <= qy < H and is_road(px[qx, qy]): hits.append(t)
        if len(hits) >= 3:
            # the run of hits nearest the middle (another road crossing the window is ignored)
            runs, cur = [], [hits[0]]
            for h in hits[1:]:
                if h - cur[-1] <= 2: cur.append(h)
                else: runs.append(cur); cur = [h]
            runs.append(cur)
            run = min(runs, key=lambda r: abs(sum(r) / len(r)))
            off = sum(run) / len(run)
            nx, ny = nx + px_ * off, ny + py_ * off; lost = 0
        else:
            lost += 1
            if lost > 10: print('# lost the road at', ['%.0f' % v for v in B.to_plan(nx, ny)]); break
        ndx, ndy = nx - x, ny - y; m = math.hypot(ndx, ndy)
        if m > 0: dx, dy = 0.6 * dx + 0.4 * ndx / m, 0.6 * dy + 0.4 * ndy / m; mm = math.hypot(dx, dy); dx, dy = dx / mm, dy / mm
        x, y = nx, ny
        pts.append(tuple(round(v, 1) for v in B.to_plan(x, y)))
        if math.hypot(x - sx, y - sy) < step * 1.2: break
    return pts

if __name__ == '__main__':
    a = [float(v) for v in sys.argv[1:7]]
    p = trace((a[0], a[1]), (a[2], a[3]), (a[4], a[5]))
    print(len(p), 'points'); print(p[::max(1, len(p) // 12)])
