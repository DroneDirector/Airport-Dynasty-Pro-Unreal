# The jetway console's flat HMI monitor (owner, 2026-11-18, to-do 115: "keep the console as it is, but change the
# monitor on it into a flat monitor on the console, adjust the 3D geometry"). Real jetway consoles carry a flat
# industrial touch panel (a panel PC) at the back of the sloped desk, tilted up to the operator, next to the joystick,
# the push buttons and the emergency stop. On each jetway's console (Tools/jetway_part5.py made it):
#  - the old screen hood (jwAluB#new above the desk) and the dark panel under it (jwDark#new) go, and the picture
#    plane that sat in it (jwp#...);
#  - a flat monitor: a 16:10 screen 36 x 22.5 cm (its own material, jwHmiScreen - the game puts the live HMI on it)
#    in a dark housing 42 x 30 x 4.5 cm with a slim bezel, tilted 15 degrees back, on a short post and a foot plate
#    bolted to the desk's raised back.
# Console frame (glTF metres): x across, y up, z away from the operator (he stands at -z). In place on the export:
#   python Tools/jetway_monitor.py [glb]      (then Tools/find_zfight.py and Tools/import_jetways.sh)
import json, struct, sys, math, os
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HERE, 'jetway_details.py'), encoding='utf-8').read()
exec(src[:src.index("stat = {'sills'")])          # (its reading, writing and geometry helpers, JROOTS)
p5 = open(os.path.join(HERE, 'jetway_part5.py'), encoding='utf-8').read()
exec(p5[p5.index('def frame_root'):p5.index('stat5 = ')])   # (frame_root, desc, drop_where)

if 'jwHmiScreen' not in MAT:
    g['materials'].append({'name': 'jwHmiScreen', 'pbrMetallicRoughness': {'baseColorFactor': [0.02, 0.03, 0.04, 1], 'metallicFactor': 0.0, 'roughnessFactor': 0.08},
                           'emissiveFactor': [0.04, 0.07, 0.09]})
    MAT['jwHmiScreen'] = len(g['materials']) - 1

T = math.radians(15.0)
C = np.array([0.0, 1.24, 0.20])                                   # the screen's middle
EX, EY, EZ = np.array([1.0, 0, 0]), np.array([0, math.cos(T), math.sin(T)]), np.array([0, -math.sin(T), math.cos(T)])
def at(u, v, w): return C + EX * u + EY * v + EZ * w             # u across, v up the screen, w back into the housing
def obox(geo, u0, u1, v0, v1, w0, w1):
    c = lambda i, j, k: at((u0, u1)[i], (v0, v1)[j], (w0, w1)[k])
    for (a, b2, cc, d, n) in ((c(0,0,0), c(1,0,0), c(1,1,0), c(0,1,0), -EZ), (c(1,0,1), c(0,0,1), c(0,1,1), c(1,1,1), EZ),
                             (c(0,0,1), c(0,0,0), c(0,1,0), c(0,1,1), -EX), (c(1,0,0), c(1,0,1), c(1,1,1), c(1,1,0), EX),
                             (c(0,1,0), c(1,1,0), c(1,1,1), c(0,1,1), EY), (c(0,0,1), c(1,0,1), c(1,0,0), c(0,0,0), -EY)):
        geo.quad(a, b2, cc, d, n)

stat = {'monitors': 0, 'hood_tris': 0, 'panel_tris': 0, 'pictures': 0}
for J in sorted(JROOTS):
    con = next((i for i, nd in enumerate(N) if nd.get('name') == f'{J}/CAB-CONSOLE'), None)
    if con is None: continue
    if any(N[c].get('name', '') == f'{J}/CAB-CONSOLE:jwHmiScreen#monitor' for c in N[con].get('children', [])): continue   # (done)
    for c in N[con].get('children', []):
        nm = N[c].get('name', '')
        if nm == f'{J}/CAB-CONSOLE:jwAluB#new':     # the hood: its roof, its sides
            stat['hood_tris'] += drop_where(c, con, lambda cc, a, b2, c2: (cc[:, 1] > 1.07) | ((np.minimum(np.minimum(a[:, 1], b2[:, 1]), c2[:, 1]) > 1.055) & (np.abs(cc[:, 0]) > 0.185) & (cc[:, 2] > 0.1)))
        if nm == f'{J}/CAB-CONSOLE:jwDark#new':     # the dark panel under the hood
            stat['panel_tris'] += drop_where(c, con, lambda cc, a, b2, c2: (np.minimum(np.minimum(a[:, 1], b2[:, 1]), c2[:, 1]) > 1.045) & (np.minimum(np.minimum(a[:, 2], b2[:, 2]), c2[:, 2]) > 0.22) & (np.abs(cc[:, 0]) < 0.192))
        if nm.startswith('jwp') and 'mesh' in N[c]: del N[c]['mesh']; stat['pictures'] += 1
    dark, alu, scr = Geo(), Geo(), Geo()
    SW, SH = 0.18, 0.1125                                         # half the screen's width and height
    obox(dark, -0.21, 0.21, -0.15, 0.15, 0.004, 0.045)            # the housing
    obox(dark, -0.21, 0.21, SH, 0.15, -0.003, 0.004)              # the bezel: top, bottom (deeper chin), sides
    obox(dark, -0.21, 0.21, -0.15, -SH, -0.003, 0.004)
    obox(dark, -0.21, -SW, -SH, SH, -0.003, 0.004); obox(dark, SW, 0.21, -SH, SH, -0.003, 0.004)
    obox(alu, -0.06, 0.06, -0.06, 0.06, 0.045, 0.055)             # the mounting plate on its back
    alu.box(-0.035, 0.035, 1.015, 1.215, 0.25, 0.275)             # the post up from the desk's raised back
    alu.box(-0.08, 0.08, 1.015, 1.023, 0.19, 0.295)               # its foot plate
    for (x, z) in ((-0.06, 0.205), (0.06, 0.205), (-0.06, 0.28), (0.06, 0.28)): dark.cylinder(np.array([x, 1.023, z]), np.array([0, 1.0, 0]), 0.007, 0.004, 10)   # its bolts
    # the screen: one quad facing the operator (-EZ), UVs left->right, top->bottom as he sees it
    a, b2, cc, d = at(SW, SH, 0.0), at(-SW, SH, 0.0), at(-SW, -SH, 0.0), at(SW, -SH, 0.0)
    scr.quad(a, b2, cc, d, -EZ, [(0, 0), (1, 0), (1, 1), (0, 1)])
    add_node(con, f'{J}/CAB-CONSOLE:jwDark#monitor', 'jwDark', dark)
    add_node(con, f'{J}/CAB-CONSOLE:jwAluB#monitor', 'jwAluB', alu)
    add_node(con, f'{J}/CAB-CONSOLE:jwHmiScreen#monitor', 'jwHmiScreen', scr)
    stat['monitors'] += 1

while len(BIN) % 4: BIN.append(0)
g['buffers'][0]['byteLength'] = len(BIN)
js = json.dumps(g, separators=(',', ':')).encode()
while len(js) % 4: js += b' '
out = bytearray(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(BIN)))
out += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(BIN), 0x004E4942) + BIN
open(PATH, 'wb').write(out)
print('MONITOR', stat)
