"""Step 2 of the cab window outlines (system Python, numpy + cv2 + scipy): the window colour masks from
trace_cab_windows_probe.py closed/opened, contour traced, smoothed, counter-clockwise -> cab_windows.json
(side: x, z on the door plane; windscreen: a = y, b = metres up the raked face). Usage: trace_cab_windows.py grid.json out.json"""
import json, numpy as np, cv2, sys
G = json.load(open(sys.argv[1])); OUT = sys.argv[2]
def mask(rows, win):
    return np.array([[c in win for c in r] for r in rows], np.uint8)
def trace(M, ax, seed, k_close, k_open, sig, n=96, clip=None):
    A, B = np.array(ax[0]), np.array(ax[1]); h = 0.01
    M = cv2.morphologyEx(M, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k_close, k_close)))
    M = cv2.morphologyEx(M, cv2.MORPH_OPEN, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k_open, k_open)))
    if clip is not None: M &= clip
    nl, lab = cv2.connectedComponents(M, connectivity=4)
    j, i = int(round((seed[1] - B[0]) / h)), int(round((seed[0] - A[0]) / h)); M = (lab == lab[j, i]).astype(np.uint8)
    cs, _ = cv2.findContours(M, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE); c = max(cs, key=len)[:, 0, :].astype(float)
    # periodic gaussian smoothing of the contour (sig in cells), then resample by arc length
    m = len(c); t = np.arange(-3 * sig, 3 * sig + 1); w = np.exp(-t ** 2 / (2 * sig * sig)); w /= w.sum()
    c = np.stack([np.convolve(np.r_[c[-len(t):, d], c[:, d], c[:len(t), d]], w, 'same')[len(t):len(t) + m] for d in (0, 1)], 1)
    L = np.r_[0, np.cumsum(np.linalg.norm(np.diff(np.r_[c, c[:1]], axis=0), axis=1))]
    s = np.linspace(0, L[-1], n, endpoint=False); c = np.stack([np.interp(s, L, np.r_[c[:, d], c[:1, d]]) for d in (0, 1)], 1)
    p = np.stack([A[0] + (c[:, 0] + 0.5) * h, B[0] + (c[:, 1] + 0.5) * h], 1)
    if (np.roll(p[:, 0], -1) - p[:, 0]) @ (np.roll(p[:, 1], -1) + p[:, 1]) > 0: p = p[::-1]   # counter-clockwise
    return M, p
xs, zs = G['side_ax']; X, Z = np.meshgrid(xs, zs)
side_win = set('-o#g x')
ML, MR = mask(G['L'], side_win), mask(G['R'], side_win)
box = ((X > 1.80) & (X < 3.62) & (Z > 1.96) & (Z < 2.79)).astype(np.uint8)
MS = (((ML.astype(float) + MR) / 2) >= 0.5).astype(np.uint8)   # both sides agree (the cab is symmetric): the cleaner, shared outline
Ms, ps = trace(MS, G['side_ax'], (3.0, 2.5), 7, 9, 8, 120, box)
As, Bs = G['ws_ax']; AA, BB = np.meshgrid(As, Bs)
MW = mask(G['WS'], set('-#g x')); MW = ((MW.astype(float) + MW[:, ::-1]) / 2 >= 0.5).astype(np.uint8)
boxw = ((np.abs(AA) < 1.24) & (BB > -0.10) & (BB < 0.80)).astype(np.uint8)
Mw, pw = trace(MW, G['ws_ax'], (0.0, 0.3), 9, 11, 6, 140, boxw)
def edge_fit(p, sel, upper):
    # a long straight-ish edge made clean: even quadratic fitted to its outer envelope (Meshy's texture line wobbles)
    a, b = p[sel, 0], p[sel, 1]; k = np.polyfit(a * a, b, 1)
    for _ in range(4):
        r = b - np.polyval(k, a * a); keep = r > -0.01 if upper else r < 0.01; k = np.polyfit(a[keep] ** 2, b[keep], 1)
    q = p.copy(); w = np.clip((1.12 - np.abs(p[:, 0])) / 0.12, 0, 1) * sel; q[:, 1] = p[:, 1] * (1 - w) + np.polyval(k, p[:, 0] ** 2) * w; return q
pw = edge_fit(pw, pw[:, 1] > 0.45, True); pw = edge_fit(pw, pw[:, 1] < 0.15, False)
json.dump({'note': 'Window outlines traced from the Meshy cab texture (Art/Lorry/trace_cab_windows.py): side (x, z) on the door skin, both sides agree; windscreen (y, metres up the raked face).',
           'side': np.round(ps, 4).tolist(), 'windscreen': np.round(pw, 4).tolist()}, open(OUT, 'w'), indent=0)
# overlay pictures for checking
for nm, M0, M1, p, ax in (('side', MS, Ms, ps, G['side_ax']), ('ws', MW, Mw, pw, G['ws_ax'])):
    A, B = np.array(ax[0]), np.array(ax[1]); im = np.zeros(M0.shape + (3,), np.uint8); im[M0 > 0] = (90, 90, 90); im[M1 > 0, 1] = 200
    im = cv2.resize(im, None, fx=4, fy=4, interpolation=cv2.INTER_NEAREST)
    q = ((p - [A[0], B[0]]) / 0.01 * 4).astype(np.int32); cv2.polylines(im, [q], True, (0, 0, 255), 1)
    cv2.imwrite(OUT.replace('.json', '_%s.png' % nm), im[::-1])
print('side', len(ps), ps.min(0).round(3), ps.max(0).round(3)); print('ws', len(pw), pw.min(0).round(3), pw.max(0).round(3))
