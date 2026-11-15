# The joints picture for a pet's skeleton: its points seen from the side and from above with the bones drawn over,
# and a grid in percent of its length (the units of pet_skeletons.json), to check and fix the joint places.
#   python Art/Pets/draw_joints.py <Name> <points.npy>     -> Models/Views/<Name>_joints.png
import cv2, json, os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
name, pts = sys.argv[1], sys.argv[2]
c = np.load(pts); B = json.load(open(os.path.join(HERE, 'Models', 'Views', name + '_bones.json')))
L = B['L']; k = 1400 / L; lo = c.min(0)
panels = []
for (u, v) in [(1, 2), (1, 0)]:
    W = int((c[:, u].max() - lo[u]) * k) + 40; H = int((c[:, v].max() - lo[v]) * k) + 40
    im = np.full((H, W, 3), 255, np.uint8)
    for g in np.arange(0, max(W, H) / k, L / 20):
        x = int(20 + g * k); y = int(H - 20 - g * k)
        cv2.line(im, (x, 0), (x, H), (225, 225, 255), 1); cv2.line(im, (0, y), (W, y), (225, 225, 255), 1)
        cv2.putText(im, '%.0f' % (g / L * 100), (x + 2, 12), 0, 0.4, (0, 0, 200), 1)
        cv2.putText(im, '%.0f' % (g / L * 100), (2, y - 2), 0, 0.4, (0, 0, 200), 1)
    f = lambda p: (int(20 + (p[u] - lo[u]) * k), int(H - 20 - (p[v] - lo[v]) * k))
    px = (20 + (c[:, u] - lo[u]) * k).astype(int); py = (H - 20 - (c[:, v] - lo[v]) * k).astype(int)
    im[py.clip(0, H - 1), px.clip(0, W - 1)] = (150, 150, 150)
    for n, (h, t) in B['bones'].items():
        if n == 'root': continue
        col = (0, 160, 0) if n.endswith('_l') else (200, 0, 200) if n.endswith('_r') else (220, 90, 0)
        cv2.line(im, f(h), f(t), col, 3); cv2.circle(im, f(h), 6, (0, 0, 0), 2)
    panels.append(im)
w = max(p.shape[1] for p in panels)
panels = [cv2.copyMakeBorder(p, 0, 10, 0, w - p.shape[1], cv2.BORDER_CONSTANT, value=(255, 255, 255)) for p in panels]
cv2.imwrite(os.path.join(HERE, 'Models', 'Views', name + '_joints.png'), np.vstack(panels))
