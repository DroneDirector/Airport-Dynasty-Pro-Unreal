# Terminal part 2a (owner approved 2026-11-23, blueprint v2 sheets 2, 3a, 3b, 4; pictures D3c, D4, D4c, D5): the non-Schengen
# Floor 2 concourse's rooms and the passport wall, in the terminal plan's metres and its own materials (numbers: conc_geom.py).
#   east_c_walkwall           the gate walkway's south wall x 146..242 put back round its two new openings (the old faces go:
#                             Data/terminal-cuts.json "touch" on F2-WALK/STRUCTURE-plaster / -white)
#   east_c_solid_*            partitions (4.2 m), room ceilings, floors, portals, fronts, the passport wall's frame, markings
#   east_c_glass_w / _e       the passport wall across the walkway (x 240), the closed transfer lane's glass
#   east_c_signs_*            every sign: dark bands, letters as glowing shapes (no pictures, sharp at any distance)
#   east_c_door, _escdoor, _shutter, _tape, _post, _booth, _egate, _leaf_glass    instanced pieces. The booths and e-gates are plain
#                             counters that only hold the places (TO-DO 212 brings the real ones); their closed glass leaves keep
#                             passport control shut until part 5; the doors stay closed until the rooms are fitted out (part 3 / 212)
# Left free: the escape-stair boxes x 246 + 30 k, z 7..21.5 (sheet 9c) and stair 21 at x 551 (z 38..40 is a passage).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalEast/make_concourse.py
#   then Tools/import_east.py east_c_..., Tools/terminal_edit.py (Tools/terminal_concourse_edit.py), Tools/terminal_cut.py 10381 10382
import bpy, bmesh, json, math, os, sys, glob
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); sys.path.insert(0, HERE)
from conc_geom import *
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(ROOT, 'Art', 'TerminalAnnex', 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
W, T = 'MI_T_white', 0.2
GL, MU, PL, MARBLE, OAK, CEDAR, BASALT = 'MI_T_glassOuter', 'MI_T_mullion', 'MI_T_plaster', 'M_Marble', 'MI_T_oak', 'MI_T_cedar', 'MI_T_basalt'
LED, OFF, BLK, STEEL, GREEN, AMBER, RED = 'MI_T_l_FFFFFF', 'MI_T_offwhite', 'MI_T_black', 'MI_T_steel', 'MI_T_l_20FF40', 'MI_T_l_FFC870', 'MI_T_l_FF2020'
S = Piece('east_c_solid'); GW = Piece('east_c_glass_w'); GE = Piece('east_c_glass_e'); SG = Piece('east_c_signs'); WP = Piece('east_c_walkwall')
def G(x): return GW if x < 300 else GE
ZN = 51.885                                            # the lining of the walkway's wall

# ---- letters ----
FONT = bpy.data.fonts.load('C:/Windows/Fonts/arialbd.ttf')
def text(P, s, cx, cy, cz, n, size, m=LED, depth=0.012, maxw=None):
    """A line of letters, its middle at plan (cx, cy, cz) on a face looking along n ('+x', '-x', '+z', '-z')."""
    cu = bpy.data.curves.new('t', 'FONT'); cu.body = s; cu.font = FONT; cu.size = size; cu.extrude = depth / 2; cu.resolution_u = 3
    cu.align_x = 'CENTER'; cu.align_y = 'CENTER'
    o = bpy.data.objects.new('t', cu); bpy.context.scene.collection.objects.link(o); bpy.context.view_layer.update()
    e = o.evaluated_get(bpy.context.evaluated_depsgraph_get()); me = e.to_mesh()
    xs = [v.co.x for v in me.vertices]; ys = [v.co.y for v in me.vertices]
    w = max(xs) - min(xs); mid = (max(xs) + min(xs)) / 2; my = (max(ys) + min(ys)) / 2; k = 1.0 if not maxw or w <= maxw else maxw / w
    R = {'-z': (1, 0), '+z': (-1, 0), '-x': (0, -1), '+x': (0, 1)}[n]; N_ = {'-z': (0, -1), '+z': (0, 1), '-x': (-1, 0), '+x': (1, 0)}[n]
    def pl(v):
        u, q, d = (v.co.x - mid) * k, (v.co.y - my) * k, v.co.z + depth / 2 + 0.0005
        return (cx + R[0] * u + N_[0] * d, cy + q, cz + R[1] * u + N_[1] * d)
    for p in me.polygons:
        f = P.face([pl(me.vertices[i]) for i in p.vertices], m); nn = p.normal
        wd = (R[0] * nn.x + N_[0] * nn.z, nn.y, R[1] * nn.x + N_[1] * nn.z); P.want.append((f, (-wd[0], -wd[2], wd[1])))
    e.to_mesh_clear(); bpy.data.objects.remove(o); bpy.data.curves.remove(cu)
def band(n, at, u0, u1, y0, y1, s, size, m=LED, d=0.08):
    """A dark sign band on a face (plane z = at for '+z'/'-z', x = at for '+x'/'-x') from u0 to u1, its letters."""
    c = (u0 + u1) / 2; sg = 1 if n[0] == '+' else -1; a, b = sorted((at, at + sg * d))
    if n[1] == 'z': SG.box(u0, u1, y0, y1, a, b, BLK); text(SG, s, c, (y0 + y1) / 2, at + sg * d, n, size, m, maxw=(u1 - u0) - 0.3)
    else: SG.box(a, b, y0, y1, u0, u1, BLK); text(SG, s, at + sg * d, (y0 + y1) / 2, c, n, size, m, maxw=(u1 - u0) - 0.3)

# ---- walls with their steel skirting ----
def spans(y0, y1, u, v, hl):
    sp = [(y0, y1)]
    for h in hl:
        if h[0] <= u + 1e-6 and v <= h[1] + 1e-6: sp = [s for a, b in sp for s in ((a, min(b, h[2])), (max(a, h[3]), b)) if s[1] - s[0] > 1e-4]
    return sp
def wall(ax, at, u0, u1, hl=(), top=TOP, m=PL):
    cu = sorted({u0, u1} | {min(max(v, u0), u1) for h in hl for v in h[:2]}); o = 'x' if ax == 'z' else 'z'
    for u, v in zip(cu, cu[1:]):
        sp = spans(F2, top, u, v, hl)
        e = ('-y', '+y') + tuple('-+'[i] + o for i, q in enumerate((u, v)) if abs(q - (u0, u1)[i]) < 1e-6 or any(abs(q - h[j]) < 1e-6 for h in hl for j in (0, 1)))
        for a, b in sp:
            if ax == 'z': S.box(u, v, a, b, at - T / 2, at + T / 2, m, sides=e + ('-z', '+z'))
            else: S.box(at - T / 2, at + T / 2, a, b, u, v, m, sides=e + ('-x', '+x'))
        if sp and sp[0][0] < F2 + 0.01:
            for s in '-+':
                a = at - T / 2 - 0.012 if s == '-' else at + T / 2; b = a + 0.012
                if ax == 'z': S.box(u, v, F2, F2 + 0.1, a, b, STEEL, sides=('+y', s + 'z', '-x', '+x'))
                else: S.box(a, b, F2, F2 + 0.1, u, v, STEEL, sides=('+y', s + 'x', '-z', '+z'))
def wz(z, a, b, hl=()): wall('z', z, a, b, hl)
def wx(x, a, b, hl=()): wall('x', x, a, b, hl)
def esc(x): return (x - DW, x + DW, F2, HD)
def dr(u): return (u - 0.62, u + 0.62, F2, F2 + 2.3)
def lid(x0, x1, z0, z1, yb=9.6):                       # an enclosed room's ceiling and its closed top, just under the walls' tops
    S.box(x0, x1, yb, yb + 0.02, z0, z1, OFF, sides=('-y',)); S.box(x0, x1, yb + 0.3, TOP - 0.02, z0, z1, W, sides=('+y',))
def floor(x0, x1, z0, z1, m, h=0.008): S.box(x0, x1, F2, F2 + h, z0, z1, m, sides=('+y', '-x', '+x', '-z', '+z'))

# ======================================================================================================== the walkway's wall x 146..242
holes_w = [(a, b, 6.0, h) for a, b, h in holes() if b < 242.0]
cu = sorted({146.0, 242.0} | {v for h in holes_w for v in h[:2]})
for a, b in zip(cu, cu[1:]):
    h = next((q[3] for q in holes_w if q[0] <= a and b <= q[1]), 6.0); dn = ('-y',) if h > 6 else ()
    WP.box(a, b, h, 12.0, 51.92, 52.0, W, sides=('-z',) + dn); WP.box(a, b, h, 11.4, 52.03, 52.15, PL, sides=('+z',) + dn)
for a, b, _, h in holes_w:                             # (the jambs, behind the portals' oak)
    WP.box(a, a, 6.0, h, 51.92, 52.15, PL, sides=('+x',)); WP.box(b, b, 6.0, h, 51.92, 52.15, PL, sides=('-x',))
# ======================================================================================================== the openings onto the walkway (z 52)
def zf(x): return 52.15 if x < 242 else 52.1           # the wall's face on the walkway
def portal(a, b, h, w=0.08):
    z0, z1 = 51.85, zf((a + b) / 2) + 0.03; a, b, h = a + 0.003, b - 0.003, h - 0.003      # (3 mm inside the wall's own reveals)
    S.box(a, a + w, F2, h, z0, z1, OAK); S.box(b - w, b, F2, h, z0, z1, OAK); S.box(a + w, b - w, h - w, h, z0, z1, OAK, sides=('-y', '-z', '+z'))
    S.box(a + w, b - w, h - w - 0.012, h - w, 51.99, 52.03, LED, sides=('-y', '-z', '+z'))
    S.box(a + w, b - w, 5.99, F2 + 0.006, 51.88, 52.14, STEEL, sides=('+y', '-z', '+z'))
for a, b in BIG: portal(a, b, HB)
for a, b in LANES: portal(a, b, HL)
for x in ESC_N: S.box(x - DW, x + DW, 5.99, F2 + 0.006, 51.897, 52.103, STEEL, sides=('+y', '-z', '+z'))
for (a, b), s in zip(BIG, ('PASSPORT CONTROL  ·  GATES 13-16', 'TAX FREE', 'FOOD COURT  ·  CAFÉ  ·  BAR', 'SEATING  ·  KIDS  ·  WORK DESKS')):
    c = (a + b) / 2; w = min(b - a, 14.0); band('+z', zf(c), c - w / 2, c + w / 2, 10.3, 11.05, s, 0.46, AMBER if s[0] in 'PT' else LED)
for (a, b), s in zip(LANES, ('NO ENTRY', 'TOILETS', 'TOILETS', 'LOUNGE', 'TOILETS')):
    c = (a + b) / 2; band('+z', zf(c), c - 1.25, c + 1.25, 9.0, 9.5, s, 0.27, RED if s[0] == 'N' else LED)
# ======================================================================================================== the passport wall across the walkway (x 240)
XW, Z0, Z1 = 240.0, 52.15, 67.85
# its two automatic doors (D-F2-ENTR-PASSWALL-*; middles, half widths): south the staff's, north the escape way, as drawn. Until
# passport control works (part 5) the passengers of gates 13-16 walk through both: then they lock (the drop of "-ENTR-" in the name)
D = [(55.2, 2.66), (64.8, 2.66)]
HEAD = F2 + 3.16
ed = [Z0 + 0.05, D[0][0] - D[0][1], D[0][0] + D[0][1], D[1][0] - D[1][1], D[1][0] + D[1][1], Z1 - 0.05]
ms = [(Z0 + 0.04, F2), (Z1 - 0.04, F2)] + [(z, F2) for z in ed[1:5]] + [(D[0][0], HEAD + 0.08), (D[1][0], HEAD + 0.08), (60.0, F2)]
for z, y in ms: S.box(XW - 0.07, XW + 0.07, y, 11.4, z - 0.04, z + 0.04, MU, sides=('-x', '+x', '-z', '+z'))
for y0, y1, za, zb in ((11.32, 11.4, Z0, Z1), (F2, F2 + 0.06, Z0, ed[1]), (F2, F2 + 0.06, ed[2], ed[3]), (F2, F2 + 0.06, ed[4], Z1), (HEAD, HEAD + 0.08, Z0, Z1)):
    S.box(XW - 0.066, XW + 0.066, y0, y1, za, zb, MU)
BY = (9.6, 10.35)
for za, zb in zip(ed, ed[1:]):
    op = (za, zb) in ((ed[1], ed[2]), (ed[3], ed[4]))
    for y0, y1 in (([] if op else [(F2 + 0.06, HEAD)]) + [(HEAD + 0.08, 11.32)]): GW.box(XW - 0.015, XW + 0.015, y0, y1, za, zb, GL, sides=('-x', '+x'))
# up to the vault's foot (over it its gable: a copy of the walkway's own end gable, Tools/terminal_concourse_edit.py)
for z in (55.0, 65.0): S.box(XW - 0.066, XW + 0.066, 11.4, 12.17, z - 0.04, z + 0.04, MU)
GW.box(XW - 0.015, XW + 0.015, 11.4, 12.09, 55.04, 64.96, GL, sides=('-x', '+x')); S.box(XW - 0.066, XW + 0.066, 12.09, 12.17, 55.04, 64.96, MU)
SG.box(XW - 0.12, XW + 0.12, BY[0], BY[1], 54.0, 66.0, BLK)
text(SG, 'PASSPORT CONTROL  ·  GATES 13-16   →', XW - 0.12, sum(BY) / 2, 60.0, '-x', 0.5, AMBER); text(SG, 'GATES 1-12  ·  ARRIVALS', XW + 0.12, sum(BY) / 2, 60.0, '+x', 0.46, LED)
for z, s, m in ((55.2, 'STAFF', LED), (64.8, 'EMERGENCY EXIT', GREEN)):
    for sd, n in ((-1, '-x'), (1, '+x')):
        a, b = sorted((XW + sd * 0.075, XW + sd * 0.105)); SG.box(a, b, 9.27, 9.53, z - 1.1, z + 1.1, BLK); text(SG, s, XW + sd * 0.105, 9.4, z, n, 0.15, m)
for za, zb in ((ed[0], ed[1]), (ed[2], ed[3]), (ed[4], ed[5])):                                         # two rows of white dots on the glass (the rule for glass walls)
    for y in (F2 + 0.95, F2 + 1.5):
        z = za + 0.12
        while z < zb - 0.1:
            for a, b in ((XW - 0.019, XW - 0.0155), (XW + 0.0155, XW + 0.019)): S.box(a, b, y, y + 0.05, z, z + 0.05, W, sides=('-x', '+x'))
            z += 0.2
# ======================================================================================================== the closed transfer lane (x 228..231; z 20..24)
def mul(ax, at, q, y0, y1):
    if ax == 'z': S.box(q - 0.04, q + 0.04, y0, y1, at - 0.07, at + 0.07, MU)
    else: S.box(at - 0.07, at + 0.07, y0, y1, q - 0.04, q + 0.04, MU)
def gwall(ax, at, u0, u1, hl=(), top=TOP, step=2.0):
    cu = sorted({u0, u1} | {v for h in hl for v in h[:2]}); done = set()
    for u, v in zip(cu, cu[1:]):
        op = any(h[0] <= u and v <= h[1] for h in hl); n = 1 if op else max(1, round((v - u) / step))
        for i in range(n + 1):
            q = round(min(max(u + (v - u) * i / n, u0 + 0.04), u1 - 0.04), 3)
            if q not in done: done.add(q); mul(ax, at, q, F2, top)
        for a, b in [(top - 0.08, top - 0.004)] + ([(HD, HD + 0.06)] if op else [(F2, F2 + 0.06), (F2 + 3.16, F2 + 3.24)]):
            e0, e1 = u + (0.006 if u == u0 else 0.0), v - (0.006 if v == u1 else 0.0)
            if ax == 'z': S.box(e0, e1, a, b, at - 0.066, at + 0.066, MU)
            else: S.box(at - 0.066, at + 0.066, a, b, e0, e1, MU)
        P = G(u if ax == 'z' else at); y0 = HD + 0.06 if op else F2 + 0.06
        if ax == 'z': P.box(u, v, y0, top - 0.08, at - 0.015, at + 0.015, GL, sides=('-z', '+z'))
        else: P.box(at - 0.015, at + 0.015, y0, top - 0.08, u, v, GL, sides=('-x', '+x'))
gwall('x', 231.0, 24.08, 51.86)
gwall('z', 24.0, 230.93, 255.9, [esc(234.5), esc(244.0), esc(252.0)])
gwall('x', 310.5, 20.2, 23.8)
GW.box(228.46, 230.54, F2 + 0.02, HL - 0.1, 51.985, 52.015, GL, sides=('-z', '+z'))                    # its way out onto the walkway: closed glass until part 5
for x in (228.42, 229.5, 230.58): S.box(x - 0.04, x + 0.04, F2 + 0.006, HL - 0.092, 51.95, 52.05, MU)
S.box(228.46, 230.54, F2 + 1.0, F2 + 1.06, 51.94, 52.06, STEEL)
band('-z', 51.85, 228.3, 230.7, 9.0, 9.5, 'TO ALL GATES 1-12', 0.22)
band('+x', 228.07, 34.5, 39.5, 8.6, 9.1, 'TRANSFERS  ·  GATES 1-12', 0.26)
# ======================================================================================================== passport control OUT
band('-x', 248.0, 24.5, 36.9, 8.9, 9.6, 'PASSPORT CONTROL  ·  ALL PASSPORTS', 0.42, AMBER, d=0.16); band('-x', 248.0, 37.1, 51.4, 8.9, 9.6, 'E-GATES', 0.42, GREEN, d=0.16)
for z in (24.6, 37.0, 51.3): S.box(248.0, 248.12, F2, 9.6, z - 0.06, z + 0.06, STEEL)                  # the bands' posts
S.box(247.9, 248.14, 9.6, 9.63, 24.5, 51.4, STEEL)
for k in range(8):                                     # the kiosk places (to-do 212): dark stone inlays with a steel rim
    x, z = 234 + (k % 4) * 2.4, 27 + (k // 4) * 3.5
    floor(x - 0.1, x + 1.3, z - 0.1, z + 1.1, STEEL, 0.004); floor(x - 0.07, x + 1.27, z - 0.07, z + 1.07, 'MI_T_marbleDark', 0.008)
floor(247.1, 247.25, 24.6, 51.4, 'MI_T_yellow', 0.005)                                                 # the waiting line, the kiosk field
floor(233.2, 242.8, 26.2, 26.28, STEEL, 0.005); floor(233.2, 242.8, 32.4, 32.48, STEEL, 0.005)
band('+z', 24.08, 235.8, 242.2, 8.6, 9.1, 'REGISTER HERE  ·  EES', 0.3)
wx(256.0, 24.1, ZN, [(25.0, 36.6, F2, 9.0), (37.6, 50.6, F2, 9.0)])
for a, b in ((25.0, 36.6), (37.6, 50.6)):
    for z in (a, b - 0.06): S.box(255.86, 256.14, F2, 9.0, z, z + 0.06, OAK)
    S.box(255.86, 256.14, 8.94, 9.0, a + 0.06, b - 0.06, OAK, sides=('-y', '-x', '+x'))
floor(248.2, 255.88, 24.1, 51.86, 'MI_T_marbleDark', 0.006)
# ======================================================================================================== the rooms
# z 24: from the passport line's end to the pier's end, one long wall and its openings
GAPS = [(350.4, 353.6), (386.4, 389.6)]; SEAT = [(476.0, 486.0), (490.0, 502.0), (506.0, 516.0)]; SHOPF = (524.0, 548.0)
H24 = [esc(x) for x in ESC_S if x > 256] + [dr(308.9)] + [(a, b, F2, TOP + 1) for a, b in GAPS] + [(a, b, F2, 9.3) for a, b in SEAT] + [(SHOPF[0], SHOPF[1], F2, 9.2)]
wz(24.0, 255.9, 551.1, H24)
for a, b in SEAT:
    for x in (a, b - 0.08): S.box(x, x + 0.08, F2, 9.3, 23.86, 24.14, OAK)
    S.box(a + 0.08, b - 0.08, 9.22, 9.3, 23.86, 24.14, OAK, sides=('-y', '-z', '+z'))
for a, b in ((255.9, GAPS[0][0]), (GAPS[0][1], GAPS[1][0]), (GAPS[1][1], 551.1)):                      # the gallery's side: an oak band along the top, an LED line under it
    S.box(a, b, 9.4, TOP - 0.01, 23.82, 23.9, OAK, sides=('-z', '-y', '+y', '-x', '+x')); S.box(a, b, 9.36, 9.4, 23.86, 23.9, LED, sides=('-z', '-y', '-x', '+x'))
# duty free
wx(258.0, 24.1, ZN, [(28.5, 49.5, F2, 9.2)]); wx(290.0, 24.1, ZN); floor(258.112, 289.888, 24.112, 51.86, 'MI_T_marbleDark')
band('-x', 257.9, 29.0, 49.0, 9.3, 10.0, 'TAX FREE', 0.5, AMBER, d=0.05)
# shop 1, toilets 1
wx(292.0, 24.1, 37.9, [(26.0, 35.0, F2, 9.2)]); wx(305.0, 24.1, 37.9); wz(38.0, 291.9, 305.1); wz(40.0, 291.9, 305.1, [dr(295.5), dr(301.5)]); wx(292.0, 40.1, ZN); wx(305.0, 40.1, ZN)
floor(292.112, 304.888, 24.112, 37.888, 'M_WoodFloor'); floor(292.112, 304.888, 40.112, 51.86, 'MI_T_tile'); lid(292.1, 304.9, 24.1, 37.9); lid(292.1, 304.9, 40.1, ZN)
band('-x', 291.9, 27.0, 34.0, 9.3, 9.9, 'BOOKS & NEWS', 0.34, d=0.06)
for x, s in ((295.5, 'MEN'), (301.5, 'WOMEN')): band('-z', 39.9, x - 1.6, x + 1.6, 8.6, 9.1, s, 0.24, d=0.05)
# the transfer centre (stands open to the oak ceiling; its floor lid x 326..338 stays until the transfer stair comes, 2b)
wx(307.0, 24.1, ZN); wx(340.0, 24.1, ZN); band('-z', 23.82, 307.4, 310.3, 9.5, 10.1, 'TRANSFERS', 0.3, d=0.05)
floor(308.0, 339.0, 36.0, 36.1, 'MI_T_yellow', 0.005)
band('+x', 307.1, 41.0, 49.0, 8.8, 9.4, 'TRANSFER SECURITY', 0.34, d=0.05); band('+x', 307.1, 27.0, 34.0, 8.8, 9.4, 'TRANSFER DESK', 0.34, d=0.05)
# food court: five kiosks along the gallery, closed (shutters down), two ways through to it
wx(342.0, 24.1, ZN); wx(398.0, 24.1, ZN); floor(342.112, 397.888, 28.4, 51.86, 'M_WoodFloor'); [floor(a, b, 23.86, 28.4, 'M_WoodFloor') for a, b in GAPS]
KI = [(342.1, 350.3, 'CAFÉ'), (353.7, 364.5, 'FONDUE'), (364.5, 375.5, 'PIZZA'), (375.5, 386.3, 'BURGERS'), (389.7, 397.9, 'BAR')]
SHUT = []                                              # (room, x, z, turn, width)
for a, b, s in KI:
    S.box(a, b, 9.2, TOP - 0.02, 24.1, 28.0, BLK, sides=('+z', '+y', '-y')); S.box(a, b, 9.16, 9.2, 27.93, 28.02, LED, sides=('-y', '+z', '+y'))
    text(SG, s, (a + b) / 2, 9.68, 28.0, '+z', 0.5, AMBER)
    for x in (a, b - 0.15): S.box(x, x + 0.15, F2, 9.16, 27.7, 27.99, OAK)
    S.box(a + 0.15, b - 0.15, F2, F2 + 1.05, 28.0, 28.3, BASALT, sides=('+z', '-x', '+x')); S.box(a + 0.1, b - 0.1, F2 + 1.05, F2 + 1.1, 27.95, 28.36, OAK)
    S.box(a + 0.15, b - 0.15, F2 + 0.99, F2 + 1.02, 28.3, 28.315, LED, sides=('+z', '-y', '+y'))
    n = max(1, round((b - a - 0.3) / 3)); w = (b - a - 0.3) / n
    for i in range(n): SHUT.append(['food', a + 0.15 + w * (i + 0.5), 27.82, 180, w])
for x, s in ((350.3, 1), (353.7, -1), (386.3, 1), (389.7, -1)):                                        # the kiosks' ends at the ways to the gallery
    a, b = sorted((x, x + s * 0.1)); S.box(a, b, F2, TOP, 23.9, 28.0, CEDAR)
for a, b in GAPS: S.box(a, b, 9.2, TOP, 27.86, 27.98, BLK); text(SG, 'WINDOW GALLERY', (a + b) / 2, 9.68, 27.98, '+z', 0.26, LED); S.box(a, b, 9.2, TOP, 23.9, 24.02, OAK)
# prayer and quiet room, toilets 2 (family, baby)
wx(400.0, 24.1, 36.9); wx(424.0, 24.1, 36.9); wz(37.0, 399.9, 424.1, [dr(412.0)]); wz(39.0, 399.9, 424.1, [dr(404.0), dr(412.0), dr(420.0)]); wx(400.0, 39.1, ZN); wx(424.0, 39.1, ZN)
floor(400.112, 423.888, 24.112, 36.888, 'MI_T_carpetGrey'); floor(400.112, 423.888, 39.112, 51.86, 'MI_T_tile'); lid(400.1, 423.9, 24.1, 36.9); lid(400.1, 423.9, 39.1, ZN)
band('+z', 37.1, 409.0, 415.0, 8.6, 9.1, 'PRAYER  ·  QUIET ROOM', 0.24, d=0.05)
for x, s in ((404, 'MEN'), (412, 'FAMILY  ·  BABY'), (420, 'WOMEN')): band('-z', 38.9, x - 1.6, x + 1.6, 8.6, 9.1, s, 0.24, d=0.05)
# the premium lounge
wx(426.0, 24.1, ZN); wx(470.0, 24.1, ZN, [dr(44.0)]); floor(426.112, 469.888, 24.112, 51.86, 'MI_T_carpetBlue'); lid(426.1, 469.9, 24.1, ZN)
band('+x', 470.1, 41.0, 47.0, 8.6, 9.1, 'PREMIUM LOUNGE', 0.3, AMBER, d=0.05)
# seating, kids, work desks
wx(472.0, 24.1, ZN); wx(520.0, 24.1, ZN); floor(472.112, 519.888, 24.112, 51.86, 'MI_T_carpetGrey')
# shops 2, toilets 3
wx(522.0, 24.1, 37.9); wz(38.0, 521.9, 551.1); wz(40.0, 521.9, 551.1, [dr(530.0), dr(538.0), dr(546.0)]); wx(522.0, 40.1, ZN)
floor(522.112, 551.1, 24.112, 37.888, 'M_WoodFloor'); floor(522.112, 551.1, 40.112, 51.86, 'MI_T_tile'); lid(522.1, 551.1, 24.1, 37.9); lid(522.1, 551.1, 40.1, ZN)
for x in (530.0, 536.0, 542.0): S.box(x - 0.1, x + 0.1, F2 + 0.008, 9.6, 24.12, 37.888, PL, sides=('-x', '+x', '-z'))
for k, s in enumerate(('SOUVENIRS', 'FASHION', 'ELECTRONICS', 'CHOCOLATE')): band('-z', 23.82, 524.3 + 6 * k, 529.7 + 6 * k, 9.5, 10.1, s, 0.36, d=0.05)
for x, s in ((530, 'MEN'), (538, 'FAMILY'), (546, 'WOMEN')): band('-z', 39.9, x - 1.6, x + 1.6, 8.6, 9.1, s, 0.24, d=0.05)
# ---- the shutters ----
def row_z(tag, x0, x1, z):
    n = max(1, round((x1 - x0) / 3)); w = (x1 - x0) / n
    for i in range(n): SHUT.append([tag, x0 + w * (i + 0.5), z, 0, w])
row_z('df', 262.08, 285.92, 52.0); row_z('shops2', 524.0, 548.0, 24.0)
S.box(262.08, 285.92, 9.2, HB - 0.08, 51.94, 52.06, BLK, sides=('-z', '+z', '-y'))
for i in range(7): SHUT.append(['df', 258.0, 30.0 + 3 * i, 90, 3.0])
for i in range(3): SHUT.append(['shop1', 292.0, 27.5 + 3 * i, 90, 3.0])
# ======================================================================================================== the instanced pieces (round x 0, z 0; y from the floor)
def shutter():
    P = Piece('east_c_shutter'); h = 9.195 - F2
    for x in (-1.5, 1.44): P.box(x, x + 0.06, 0.0, h, -0.07, 0.07, BLK)
    P.box(-1.44, 1.44, h - 0.2, h, -0.08, 0.08, BLK, sides=('-y', '-z', '+z')); P.box(-1.44, 1.44, 0.0, 0.06, -0.03, 0.03, BLK, sides=('+y', '-z', '+z'))
    y = 0.06
    while y < h - 0.21:
        e = min(y + 0.085, h - 0.2); P.box(-1.44, 1.44, y, e, -0.018, 0.018, STEEL, sides=('-y', '+y', '-z', '+z'))
        P.box(-1.44, 1.44, e, min(y + 0.1, h - 0.2), -0.008, 0.008, 'MI_T_dark', sides=('-z', '+z')); y += 0.1
    return P
def door1():
    P = Piece('east_c_door')
    for x in (-0.62, 0.55): P.box(x, x + 0.07, 0, 2.3, -0.13, 0.13, OAK)
    P.box(-0.55, 0.55, 2.23, 2.3, -0.13, 0.13, OAK, sides=('-y', '-z', '+z')); P.box(-0.55, 0.55, 0.01, 2.23, -0.025, 0.025, CEDAR, sides=('-z', '+z'))
    for s in (-1, 1): P.box(0.40, 0.43, 0.95, 1.25, *sorted((s * 0.025, s * 0.07)), STEEL); P.box(-0.5, 0.5, 0.02, 0.22, *sorted((s * 0.025, s * 0.029)), STEEL)
    return P
def door2():
    P = Piece('east_c_escdoor'); h = HD - F2 - 0.012
    for x in (-0.997, 0.917): P.box(x, x + 0.08, 0, h, -0.17, 0.17, W)
    P.box(-0.917, 0.917, h - 0.08, h, -0.17, 0.17, W, sides=('-y', '-z', '+z'))
    for a, b in ((-0.92, -0.004), (0.004, 0.92)):
        P.box(a, b, 0.012, h - 0.08, -0.03, 0.03, OFF)
        for s in (-1, 1):
            P.box(a + 0.1, b - 0.1, 1.0, 1.06, *sorted((s * 0.08, s * 0.11)), STEEL)
            for x in (a + 0.14, b - 0.18): P.box(x, x + 0.04, 1.0, 1.06, *sorted((s * 0.03, s * 0.08)), STEEL, sides=('-x', '+x', '-y', '+y'))
    for s, n in ((-1, '-z'), (1, '+z')):
        P.box(-0.5, 0.5, h - 0.02, h + 0.22, *sorted((s * 0.17, s * 0.21)), BLK); text(P, 'EMERGENCY EXIT', 0, h + 0.1, s * 0.21, n, 0.09, GREEN)
    return P
def post(P): P.box(-0.15, 0.15, 0, 0.02, -0.15, 0.15, STEEL); P.box(-0.025, 0.025, 0.02, 0.96, -0.025, 0.025, STEEL); P.box(-0.035, 0.035, 0.96, 1.0, -0.035, 0.035, BLK)
def booth():                                           # TO-DO 212: only holds the passport booth's place (6 x 2.2 m)
    P = Piece('east_c_booth')
    for a, b, c, d in ((-3, 3, -1.1, -0.9), (-3, -2.8, -0.9, 0.9), (2.8, 3, -0.9, 0.9), (-3, 3, 0.9, 1.1)): P.box(a, b, 0, 1.1, c, d, BASALT)
    for a, b, c, d in ((-3.03, 3.03, -1.13, -0.7), (-3.03, -2.6, -0.7, 0.7), (2.6, 3.03, -0.7, 0.7), (-3.03, 3.03, 0.7, 1.13)): P.box(a, b, 1.1, 1.15, c, d, OAK)
    return P
def egate():                                           # TO-DO 212: only holds the e-gate's place (4.8 x 0.7 m)
    P = Piece('east_c_egate'); P.box(-2.4, 2.4, 0, 1.0, -0.35, 0.35, STEEL); P.box(-2.42, 2.42, 1.0, 1.04, -0.37, 0.37, BLK); P.box(-2.3, 2.3, 1.04, 1.055, -0.03, 0.03, GREEN)
    return P
def leaf():                                            # a closed glass leaf between two e-gates / booths (the way stays shut until part 5); 1 m, scaled
    P = Piece('east_c_leaf_glass'); P.box(-0.012, 0.012, 0.12, 1.5, -0.5, 0.5, GL, sides=('-x', '+x')); return P
TP = Piece('east_c_tape'); post(TP); TP.box(0.035, 1.665, 0.88, 0.94, -0.004, 0.004, 'MI_T_navy', sides=('-y', '+y', '-z', '+z'))
PO = Piece('east_c_post'); post(PO)
PIECES = [S, GW, GE, SG, WP, shutter(), door1(), door2(), TP, PO, booth(), egate(), leaf()]
# ======================================================================================================== where they go
PL_ = []                                               # [name, asset, x, y, z, turn (degrees about the height), scale]
def put(nm, a, x, y, z, r=0, s=None): PL_.append([nm, a, round(x, 4), round(y, 4), round(z, 4), r, s])
for k, (t, x, z, r, w) in enumerate(SHUT): put('SHUTTER-%03d' % (k + 1), 'east_c_shutter', x, F2, z, r, [round(w / 3.0, 5), 1, 1])
for k, x in enumerate(ESC_N): put('ESC-N-%02d' % (k + 1), 'east_c_escdoor', x, F2 + 0.006, 52.0)
for k, x in enumerate(ESC_S): put('ESC-S-%02d' % (k + 1), 'east_c_escdoor', x, F2, 24.0)
for k, (x, z, r) in enumerate([(308.9, 24.0, 0), (295.5, 40.0, 0), (301.5, 40.0, 0), (412.0, 37.0, 0), (404.0, 39.0, 0), (412.0, 39.0, 0), (420.0, 39.0, 0), (470.0, 44.0, 90), (530.0, 40.0, 0), (538.0, 40.0, 0), (546.0, 40.0, 0)]):
    put('DOOR-%02d' % (k + 1), 'east_c_door', x, F2, z, r)
for k in range(4): put('BOOTH-212-%d' % (k + 1), 'east_c_booth', 252.0, F2 + 0.006, 26.1 + 2.9 * k)
for k in range(8): put('EGATE-212-%d' % (k + 1), 'east_c_egate', 252.0, F2 + 0.006, 37.95 + 1.6 * k)
LF = [(24.07, 25.0)] + [(27.2 + 2.9 * k, 27.9 + 2.9 * k) for k in range(3)] + [(35.9, 37.6)] + [(38.3 + 1.6 * k, 39.2 + 1.6 * k) for k in range(7)] + [(49.5, 51.87)]
for k, (a, b) in enumerate(LF): put('LEAF-GLASS-%02d' % (k + 1), 'east_c_leaf_glass', 252.0, F2 + 0.006, (a + b) / 2, 0, [1, 1, round(b - a - 0.02, 3)])
for j, z in enumerate((27.55, 30.45, 33.35, 36.75, 41.2, 46.0, 50.8)):
    for i in range(2): put('TAPE-%02d' % (2 * j + i + 1), 'east_c_tape', 243.3 + 1.7 * i, F2, z)
    put('POST-%02d' % (j + 1), 'east_c_post', 246.7, F2, z)
LAMPS = {'F2-CONC-WC1': [[x, z] for x in (295.5, 301.5) for z in (43.0, 49.0)], 'F2-CONC-WC2': [[x, z] for x in (404, 412, 420) for z in (42.5, 48.5)],
         'F2-CONC-WC3': [[x, z] for x in (527, 536.5, 546) for z in (43, 49)], 'F2-CONC-SHOP1': [[298.5, 28], [298.5, 34]], 'F2-CONC-SHOPS2': [[x, 31] for x in (526, 533, 539, 546.5)],
         'F2-CONC-PRAYER': [[x, 30.5] for x in (404, 412, 420)], 'F2-CONC-LOUNGE': [[x, z] for x in (431, 441.5, 454.5, 465) for z in (29, 38, 47)]}
LIDS = [(292, 305, 24, 52), (400, 424, 24, 52), (426, 470, 24, 52), (522, 552, 24, 52)]
# ======================================================================================================== out: the big solid in pieces along the building (Lumen), as the shell
BINX = [290.0, 341.0, 399.0, 471.0]
def split(o):
    bm = bmesh.new(); bm.from_mesh(o.data)
    if o.name == 'east_c_solid':
        for x in BINX: bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(-x, 0, 0), plane_no=(1, 0, 0), dist=1e-5)
    grp = {}; bm.faces.ensure_lookup_table(); bm.faces.index_update()
    for f in bm.faces: grp.setdefault(sum(-f.calc_center_median().x > b for b in BINX) + 1, []).append(f.index)
    out = []
    for n, idx in sorted(grp.items()):
        c = bm.copy(); c.faces.ensure_lookup_table(); c.faces.index_update(); keep = set(idx)
        bmesh.ops.delete(c, geom=[f for f in c.faces if f.index not in keep], context='FACES')
        used = sorted({f.material_index for f in c.faces}); mp = {u: k for k, u in enumerate(used)}
        for f in c.faces: f.material_index = mp[f.material_index]
        me = bpy.data.meshes.new('%s_%d' % (o.name, n)); c.to_mesh(me); c.free()
        for u in used: me.materials.append(o.data.materials[u])
        q = bpy.data.objects.new(me.name, me); bpy.context.scene.collection.objects.link(q); out.append(q)
    bm.free(); bpy.data.objects.remove(o); return out
for f_ in glob.glob(os.path.join(HERE, 'east_c_*.fbx')): os.remove(f_)
BIGP = []
for P_ in PIECES:
    o = P_.finish(); objs = split(o) if o.name in ('east_c_solid', 'east_c_signs') else [o]
    if len(objs) > 1 or o.name in ('east_c_glass_w', 'east_c_glass_e', 'east_c_walkwall'): BIGP += [q.name for q in objs]
    for o in objs:
        assert o.data.materials and all(m for m in o.data.materials), 'empty material slot ' + o.name
        print('CONC', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
        for x in bpy.context.selected_objects: x.select_set(False)
        o.select_set(True); bpy.context.view_layer.objects.active = o
        bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
json.dump({'big': BIGP, 'place': PL_, 'lamps': LAMPS, 'lids': LIDS, 'doors': [['D-F2-ENTR-PASSWALL-S', XW, 55.2], ['D-F2-ENTR-PASSWALL-N', XW, 64.8]]}, open(os.path.join(HERE, 'conc_place.json'), 'w'), indent=0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'Concourse.blend'))
print('CONC saved', len(PL_), 'places')
