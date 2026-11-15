# Measures a pet's traced outlines (owner, 2026-11-15: "measure the widths and where it starts to form, start to bend,
# and where the ears connect"): on the front and back views the width at every height, on the profile the depth at
# every length - in cm, at the profile's scale - and the bend points of every outline (where the contour starts to
# curve: the strongest curvature along it, after smoothing out the fur). Writes <dir>/measurements.json and a chart
# per view (<view>_measure_check.png): width bars with cm values, bend points numbered.
#   python Art/Pets/measure_views.py <pet dir> <reference dir> <profile view> <cm per profile px> <view:ground_px:eartip_px> ...
import json, os, sys
import numpy as np, cv2
from scipy.ndimage import gaussian_filter1d
D, R, prof, kp = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
views = [v.split(':') for v in sys.argv[5:]]
pm = np.load(os.path.join(D, prof + '_mask.npy'))
pys = np.nonzero(pm.any(1))[0]; p_ground, p_top = pys.max(), pys.min()
out = {'_': 'cm at the profile scale (%.4f cm per profile pixel); bends are (x, y) in each view\'s own pixels' % kp}


def bends(o, n=24, sigma=6):
    c = np.c_[gaussian_filter1d(o[:, 0], sigma, mode='wrap'), gaussian_filter1d(o[:, 1], sigma, mode='wrap')]
    d1 = np.gradient(c, axis=0); d2 = np.gradient(d1, axis=0)
    k = (d1[:, 0] * d2[:, 1] - d1[:, 1] * d2[:, 0]) / np.maximum((d1 ** 2).sum(1) ** 1.5, 1e-6)
    a = np.abs(k); idx = []
    for i in np.argsort(-a):
        if all(min(abs(i - j), len(a) - abs(i - j)) > len(a) / (n * 1.6) for j in idx): idx.append(int(i))
        if len(idx) >= n: break
    return [(o[i].round(1).tolist(), round(float(k[i]), 4)) for i in sorted(idx)]


def chart(view, m, o, k, rows=True):
    img = cv2.imread(os.path.join(R, view + '.png')); s = max(1, int(round(1500 / max(img.shape[:2]))))
    img = cv2.resize(img, None, fx=s, fy=s, interpolation=cv2.INTER_CUBIC)
    cv2.polylines(img, [np.round(o * s).astype(np.int32)], True, (0, 0, 255), 2, cv2.LINE_AA)
    table = []
    ys, xs = np.nonzero(m)
    if rows:
        step = max(4, int((ys.max() - ys.min()) / 34))
        for y in range(ys.min() + step // 2, ys.max(), step):
            r = np.nonzero(m[y])[0]
            if not len(r): continue
            segs = np.split(r, np.nonzero(np.diff(r) > 1)[0] + 1)
            row = []
            for sg in segs:
                w = (sg[-1] - sg[0] + 1) * k; row.append([int(sg[0]), int(sg[-1]), round(w, 1)])
                cv2.line(img, (sg[0] * s, y * s), (sg[-1] * s, y * s), (0, 170, 0), 1, cv2.LINE_AA)
                cv2.putText(img, '%.1f' % w, (sg[-1] * s + 3, y * s + 4), 0, 0.38, (0, 110, 0), 1, cv2.LINE_AA)
            table.append({'y_px': int(y), 'height_cm': round((ys.max() - y) * k, 1), 'runs': row})
    else:
        step = max(4, int((xs.max() - xs.min()) / 40))
        for x in range(xs.min() + step // 2, xs.max(), step):
            c = np.nonzero(m[:, x])[0]
            if not len(c): continue
            segs = np.split(c, np.nonzero(np.diff(c) > 1)[0] + 1)
            col = []
            for sg in segs:
                d = (sg[-1] - sg[0] + 1) * k; col.append([int(sg[0]), int(sg[-1]), round(d, 1)])
                cv2.line(img, (x * s, sg[0] * s), (x * s, sg[-1] * s), (0, 170, 0), 1, cv2.LINE_AA)
            cv2.putText(img, '%.0f' % (sum(c_[2] for c_ in col)), (x * s - 6, (c.min()) * s - 4), 0, 0.33, (0, 110, 0), 1, cv2.LINE_AA)
            table.append({'x_px': int(x), 'from_nose_cm': round((x - xs.min()) * k, 1), 'runs': col})
    b = bends(o)
    for i, (p, kk) in enumerate(b):
        q = (int(p[0] * s), int(p[1] * s)); cv2.circle(img, q, 5, (255, 0, 200), -1, cv2.LINE_AA)
        cv2.putText(img, str(i + 1), (q[0] + 6, q[1] - 4), 0, 0.5, (160, 0, 140), 2, cv2.LINE_AA)
    cv2.imwrite(os.path.join(D, view + '_measure_check.png'), img)
    return table, b


t, b = chart(prof, pm, np.load(os.path.join(D, prof + '_outline.npy')), kp, rows=False)
out[prof] = {'cm_per_px': kp, 'depths': t, 'bends': b}
for view, g, e in views:
    m = np.load(os.path.join(D, view + '_mask.npy')); k = kp * (p_ground - p_top) / (float(g) - float(e))
    t, b = chart(view, m, np.load(os.path.join(D, view + '_outline.npy')), k, rows=True)
    out[view] = {'cm_per_px': round(k, 5), 'widths': t, 'bends': b}
json.dump(out, open(os.path.join(D, 'measurements.json'), 'w'), indent=0)
print('measured', [prof] + [v[0] for v in views])
