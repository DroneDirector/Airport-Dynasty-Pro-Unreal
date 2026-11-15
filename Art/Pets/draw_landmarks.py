# Draws the hand-traced landmarks (Gsd/landmarks.json) on each view with the body outline: eyes green, ears cyan, nose
# and mouth magenta, tongue pink, legs orange (joints as dots), shoulder blade / upper arm blue, tail purple - for the
# owner to check before the model is built from them.
#   python Art/Pets/draw_landmarks.py <pet dir> <reference dir>
import json, os, sys
import numpy as np, cv2
D, R = sys.argv[1], sys.argv[2]
L = json.load(open(os.path.join(D, 'landmarks.json')))
COL = {'eye': (60, 200, 60), 'ear': (255, 255, 0), 'nose': (255, 0, 255), 'mouth': (255, 0, 255), 'tongue': (180, 120, 255),
       'leg': (0, 140, 255), 'shoulder': (255, 80, 0), 'arm': (255, 80, 0), 'tail': (200, 0, 140), 'brisket': (0, 200, 255),
       'stop': (0, 0, 0), 'muzzle': (255, 0, 255), 'hip': (0, 200, 255)}
def col(k):
    # (match on the name's first word: 'near'/'rear' contain 'ear' - legs came out in the ear colour)
    w = k.split('_')
    if 'leg' in w or 'paws' in w: return COL['leg']
    for n, c in COL.items():
        if n in w: return c
    return (0, 0, 0)
for view, items in L.items():
    if view.startswith('_'): continue
    img = cv2.imread(os.path.join(R, view + '.png')); H, W = img.shape[:2]
    s = max(1, int(round(1400 / max(W, H) * 1.6))) if max(W, H) < 1000 else 1
    out = cv2.resize(img, None, fx=s, fy=s, interpolation=cv2.INTER_CUBIC)
    o = np.load(os.path.join(D, view + '_outline.npy'))
    cv2.polylines(out, [np.round(o * s).astype(np.int32)], True, (0, 0, 255), max(1, s // 2 + 1), cv2.LINE_AA)
    t = max(2, s)
    def draw(k, v, c):
        a = np.array(v, float)
        if a.ndim == 1: cv2.circle(out, tuple(np.round(a * s).astype(int)), t * 2, c, -1, cv2.LINE_AA)
        elif a.ndim == 2:
            closed = k in ('eye', 'eye_left', 'eye_right', 'nose', 'ear', 'mouth', 'tongue', 'tail') and len(a) > 3
            cv2.polylines(out, [np.round(a * s).astype(np.int32)], closed, c, t, cv2.LINE_AA)
        else:
            for b in a: draw(k, b, c)
    for k, v in items.items():
        if isinstance(v, dict):
            for kk, vv in v.items():
                if isinstance(vv, (int, float)): continue
                draw(k + '_' + kk, vv, col(k))
        else: draw(k, v, col(k))
    cv2.imwrite(os.path.join(D, view + '_landmarks_check.png'), out); print(view, out.shape)
