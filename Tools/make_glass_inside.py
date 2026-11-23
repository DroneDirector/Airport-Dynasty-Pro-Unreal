# The terminal's inside, seen from above, as a picture the window glass reads (owner, 2026-11-23: more reflection on the glass
# from outside than from inside): red = inside on floor 1, green = inside on floor 2, 20 cm a pixel, plan x -250..569.2,
# z -8..94.4 (row 0 = z -8). The glass looks 40 cm to your side of the pane (Tools/set_building_glass.py): outside or inside.
# From Data/terminal-plan.json's rooms, the parts the plan has no rooms for, and the curved gates' arcs (Art/GateArc).
# Run again when the building changes, then Tools/set_building_glass.py.   python Tools/make_glass_inside.py
import json, os, sys
import numpy as np
from PIL import Image
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'Art', 'GateArc'))
import gatearc_geom as G
X0, Z0, P, W, H = -250.0, -8.0, 0.2, 4096, 512
X, Z = np.meshgrid(X0 + (np.arange(W) + .5) * P, Z0 + (np.arange(H) + .5) * P)
m = {1: np.zeros((H, W), bool), 2: np.zeros((H, W), bool)}


def box(f, x0, z0, x1, z1): m[f] |= (X > x0) & (X < x1) & (Z > z0) & (Z < z1)


for r in json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))['rooms']:
    gate = r['id'].startswith('F2-GATE-')
    box(r['f'], r['x0'], r['z0'], r['x1'], min(r['z1'], 82.0) if gate else r['z1'])
    if r['f'] == 1 and r.get('kind') == 'hall': box(2, r['x0'], r['z0'], r['x1'], r['z1'])
    if gate and int(r['id'].split('-')[2]) in G.GATES:
        arc = np.sqrt(np.maximum(0, G.R ** 2 - (np.clip(X - r['x0'], 0, G.X1) - G.H) ** 2)) + G.CZ
        m[2] |= (X > r['x0']) & (X < r['x0'] + G.X1) & (Z > 68) & (Z < arc)
# (the main building between its front and the long walkway: every room, also the ones the plan does not name)
for f in (1, 2): box(f, -80, 0, 551.2, 68); box(f, -241.1, 8, -80, 68)
img = np.zeros((H, W, 3), np.uint8); img[..., 0] = m[1] * 255; img[..., 1] = m[2] * 255
out = os.path.join(ROOT, 'Art', 'Terminal', 'Glass', 'T_GlassInside.png'); os.makedirs(os.path.dirname(out), exist_ok=True)
Image.fromarray(img).save(out, optimize=True)
print('glass inside: %d bytes, floor 1 %.0f m2, floor 2 %.0f m2' % (os.path.getsize(out), m[1].sum() * P * P, m[2].sum() * P * P))
