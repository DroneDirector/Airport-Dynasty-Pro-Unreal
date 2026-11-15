# Draws the body outline (red) and the hand-traced parts lying over the body (ears, cyan) on the photo, whole and with an
# enlarged, brightened head (owner: every photo, every dog, every cat, ears included).
#   python Art/Pets/check_parts.py <photo> <name> <head x0 y0 x1 y1>
import json, os, sys
import numpy as np, cv2
from scipy.interpolate import splprep, splev
HERE = os.path.dirname(os.path.abspath(__file__))
photo, name = sys.argv[1], sys.argv[2]; hx0, hy0, hx1, hy1 = (int(v) for v in sys.argv[3:7])
pet = name.split('_')[0].capitalize()
img = cv2.imread(photo); H, W = img.shape[:2]
body = np.load(os.path.join(HERE, pet, name + '_outline.npy'))
p = np.array(json.load(open(os.path.join(HERE, pet, 'ears.json')))[name], float)
tck, _ = splprep([p[:, 0], p[:, 1]], s=2, per=True); ear = np.column_stack(splev(np.linspace(0, 1, 300), tck))
np.save(os.path.join(HERE, pet, name + '_ear_outline.npy'), ear)
s = max(1, int(round(1600 / W)))
chk = cv2.resize(img, (W * s, H * s), interpolation=cv2.INTER_CUBIC)
for c, col in ((body, (0, 0, 255)), (ear, (255, 255, 0))):
    cv2.polylines(chk, [np.round(c * s).astype(np.int32)], True, col, max(2, s, W // 600), cv2.LINE_AA)
cv2.imwrite(os.path.join(HERE, pet, name + '_outline_check.png'), chk)
z = max(2, int(1000 / (hx1 - hx0)))
hd = cv2.resize(cv2.convertScaleAbs(img[hy0:hy1, hx0:hx1], alpha=2.0, beta=10), None, fx=z, fy=z, interpolation=cv2.INTER_CUBIC)
for c, col in ((body, (0, 0, 255)), (ear, (255, 255, 0))):
    cv2.polylines(hd, [np.round((c - [hx0, hy0]) * z).astype(np.int32)], True, col, 3, cv2.LINE_AA)
cv2.imwrite(os.path.join(HERE, pet, name + '_head_check.png'), hd)
print('checked', name)
