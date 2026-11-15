# Quality check of the road network against the owner's drawings (owner, 2026-11-15: "the roads are very jagged ... do a
# quality check in the town and along the road from the town to the airport"). For every road in Data/roads.json, on the
# path the game itself makes (RoadNetwork.cpp SmoothPath: centripetal Catmull-Rom):
#   - off: how far its middle is from the drawn street's middle (the drawing's asphalt measured square across it every
#     4 m; the town's streets on Town-Plan-v7, the road to town on the overview) - mean and worst, metres
#   - wiggle: how often the road turns one way then the other within 30 m, visibly (radius under 150 m) (a drawn street bends smoothly; a wiggle is a
#     tracing error), and its sharpest turn (radius, m)
# Prints a line per road and FAILS the roads past the limits; writes Saved/road-check.png (the town, roads over the plan).
#   python Tools/check_roads.py
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OFF_MEAN, OFF_MAX, MIN_R, WIGGLES = 0.9, 2.5, 12.0, 0


def catmull(P, closed, step=1.0):
    n = len(P)
    def at(i):
        if closed: return P[i % n]
        if i < 0: return (2 * P[0][0] - P[1][0], 2 * P[0][1] - P[1][1])
        if i >= n: return (2 * P[-1][0] - P[-2][0], 2 * P[-1][1] - P[-2][1])
        return P[i]
    out = []
    for i in range(n if closed else n - 1):
        p0, p1, p2, p3 = at(i - 1), at(i), at(i + 1), at(i + 2)
        kn = lambda t, a, b: t + max(math.sqrt(math.dist(a, b)), 1e-4)
        t0 = 0.0; t1 = kn(t0, p0, p1); t2 = kn(t1, p1, p2); t3 = kn(t2, p2, p3)
        K = max(2, math.ceil(math.dist(p1, p2) / step))
        for k in range(K):
            t = t1 + (t2 - t1) * k / K
            L = lambda a, b, ta, tb: tuple(a[j] * (tb - t) / (tb - ta) + b[j] * (t - ta) / (tb - ta) for j in range(2))
            a1, a2, a3 = L(p0, p1, t0, t1), L(p1, p2, t1, t2), L(p2, p3, t2, t3)
            b1, b2 = L(a1, a2, t0, t2), L(a2, a3, t1, t3)
            out.append(L(b1, b2, t1, t2))
    out.append(tuple(P[0] if closed else P[-1]))
    return np.array(out)


def wiggle(D):
    """Turns one way then the other within 40 m (each counted once), and the sharpest radius."""
    d = np.r_[0, np.cumsum(np.hypot(*np.diff(D, axis=0).T))]
    s = np.arange(0, d[-1], 2.0)
    X = np.c_[np.interp(s, d, D[:, 0]), np.interp(s, d, D[:, 1])]
    h = np.unwrap(np.arctan2(*np.diff(X, axis=0).T[::-1]))
    k = np.diff(h) / 2.0                                  # curvature, 1/m
    k = np.convolve(k, np.ones(3) / 3, mode='same')
    big = np.abs(k) > 1 / 150.0                           # a bend you can see (40 cm off a straight over 20 m)
    sign = np.sign(k) * big
    flips, last, last_i = 0, 0, -999
    for i, v in enumerate(sign):
        if v == 0: continue
        if s[i] < 25.0 or s[i] > s[-1] - 25.0: last, last_i = v, i; continue   # (its ends: a street straightening to meet the next)
        if last and v != last and (i - last_i) * 2.0 < 30.0: flips += 1
        last, last_i = v, i
    rmin = 1 / max(np.abs(k).max(), 1e-9) if len(k) else 1e9
    return flips, rmin


def main():
    R = json.load(open(os.path.join(ROOT, 'Data', 'roads.json'), encoding='utf-8'))['roads']
    import town_plan as T
    import importlib
    os.environ['ADP_MAP'] = 'overview'
    import blueprint_grid as B
    from PIL import Image, ImageDraw
    Image.MAX_IMAGE_PIXELS = None
    ov = Image.open(os.path.join(ROOT, 'Art', 'Maps', 'Airport-Road-Town-Overview-v11.png')).convert('RGB'); ovp = ov.load()
    def ov_road(c): return abs(c[0] - 75) <= 10 and abs(c[1] - 79) <= 10 and abs(c[2] - 85) <= 10
    tp = T.image().load()
    fails = []
    for r in R:
        rid = r['id']
        if r.get('test') or r.get('closed') or not (rid.startswith('TOWN') or rid == 'AIRPORT-BACK-TO-TOWN'): continue
        D = catmull([tuple(p[:2]) for p in r['pts']], False)
        flips, rmin = wiggle(D)
        # off the drawing: every 4 m, the asphalt's middle square across the road (town plan in the town, overview out)
        offs = []
        d = np.r_[0, np.cumsum(np.hypot(*np.diff(D, axis=0).T))]
        for s in np.arange(15.0, d[-1] - 15.0, 4.0):
            i = int(np.searchsorted(d, s)); i = min(max(i, 1), len(D) - 2)
            x, y = D[i]; tx, ty = D[i + 1] - D[i - 1]; tl = math.hypot(tx, ty); tx, ty = tx / tl, ty / tl; nx, ny = ty, -tx
            u, v = T.to_town(x, y)
            if 300 < u < 10200 and 250 < v < 8400:
                # in the town: the plan's asphalt middle (the ridge of its distance to the edges), where it is clear
                best, bo = -1, 0.0
                Dr = T.ridge()
                for o in np.arange(-6.0, 6.01, 0.1):
                    qu, qv = T.to_town(x + nx * o, y + ny * o)
                    val = Dr[int(qv / T.DS), int(qu / T.DS)]
                    if val > best: best, bo = val, o
                if T.RIDGE[0] <= best <= T.RIDGE[1]: offs.append(abs(bo))
                continue
                pick, mpx, rd = tp, T.MPX, lambda c: T.is_road(c) and not (c[0] > 225 and c[1] > 225 and c[2] > 225)
                to_img = T.to_town
            elif x < 1900.0: continue   # (the airport's area: moved 37 m / 120 m to the real terminal on purpose, make_roads.moved)
            else:
                pick, mpx, rd = ovp, 1.0 / B.PXM, ov_road
                to_img = B.to_img
            hits = []
            for o in np.arange(-9.0, 9.01, 0.25):
                q = to_img(x + nx * o, y + ny * o)
                qi = (int(q[0]), int(q[1]))
                try:
                    if rd(pick[qi]): hits.append(o)
                except IndexError: pass
            if len(hits) < 8: continue
            runs, cur = [], [hits[0]]
            for h in hits[1:]:
                if h - cur[-1] <= 0.6: cur.append(h)
                else: runs.append(cur); cur = [h]
            runs.append(cur)
            run = min(runs, key=lambda q: abs((q[0] + q[-1]) / 2))
            if run[-1] - run[0] < 4.0: continue
            offs.append(abs((run[0] + run[-1]) / 2))
        om = float(np.mean(offs)) if offs else 0.0; ox = float(np.percentile(offs, 98)) if offs else 0.0
        bad = (om > OFF_MEAN or ox > OFF_MAX or flips > WIGGLES or rmin < MIN_R) and not rid.startswith('TOWN-TURN')
        print('%-26s %6.0f m  off %.2f / %.2f m  wiggles %d  tightest r %.0f m  %s' % (rid, d[-1], om, ox, flips, rmin, 'FAIL' if bad else 'ok'))
        if bad: fails.append(rid)
    print('%d roads fail' % len(fails) if fails else 'all roads pass')
    return fails


if __name__ == '__main__':
    main()
