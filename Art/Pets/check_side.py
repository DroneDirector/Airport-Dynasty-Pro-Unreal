# A pet model's side silhouette (blue) over the reference profile with its traced outline (red), and the front/back
# silhouettes over the front/back views: <dir>/Views/<tag>_vs_views.png. One model, checked from each side.
#   python Art/Pets/check_side.py <ply> <tag> <profile.png> <outline.npy> ground_px withers_px rear_px withers_cm [front.png front_outline.npy front_ground front_eartip profile_eartip] [back.png back_outline.npy back_ground back_eartip]
import os, sys
import numpy as np, cv2
a = sys.argv[1:]
ply, tag, prof, outl = a[0], a[1], a[2], a[3]; G, Wt, R, Hcm = (float(v) for v in a[4:8])
K = Hcm / (G - Wt)
b = open(ply, 'rb').read(); h = b[:b.index(b'end_header\n') + 11].decode()
nv = int(h.split('element vertex ')[1].split()[0]); nf = int(h.split('element face ')[1].split()[0]); off = len(h)
v = np.frombuffer(b, '<f4', nv * 3, off).reshape(-1, 3); f = np.frombuffer(b, dtype=[('n', 'u1'), ('i', '<i4', (3,))], count=nf, offset=off + nv * 12)['i']
def over(img, px, o, s=2):
    img = cv2.resize(img, None, fx=s, fy=s)
    sil = np.zeros(img.shape[:2], np.uint8); cv2.fillPoly(sil, list(np.round(px[f] * s).astype(np.int32)), 255)
    cs, _ = cv2.findContours(sil, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    out = img.copy(); cv2.drawContours(out, cs, -1, (255, 120, 0), 2)
    cv2.polylines(out, [np.round(o * s).astype(np.int32)], True, (0, 0, 255), 2); return out
outs = [over(cv2.imread(prof), np.c_[R - v[:, 0] / K, G - v[:, 2] / K], np.load(outl))]
i = 8
while len(a) >= i + 5:
    fimg, fo, fg, fe, pe = a[i], a[i + 1], float(a[i + 2]), float(a[i + 3]), float(a[i + 4])
    kf = K * (G - pe) / (fg - fe)                  # (cm per pixel in that view: ground-to-ear-tip matched)
    o = np.load(fo); cx = (o[:, 0].min() + o[:, 0].max()) / 2
    view = 'front' if 'front' in fimg else 'back'
    x = cx + (v[:, 1] / kf if view == 'front' else -v[:, 1] / kf)
    outs.append(over(cv2.imread(fimg), np.c_[x, fg - v[:, 2] / kf], o))
    i += 5
hh = 700; outs = [cv2.resize(o, (int(o.shape[1] * hh / o.shape[0]), hh)) for o in outs]
d = os.path.join(os.path.dirname(ply), 'Views'); os.makedirs(d, exist_ok=True)
cv2.imwrite(os.path.join(d, tag + '_vs_views.png'), cv2.hconcat(outs)); print('wrote', tag)
