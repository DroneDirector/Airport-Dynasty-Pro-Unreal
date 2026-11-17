# The TAX FREE (16 gates, owner 2026-11-17; his own plan and renders: docs/TaxFree/TaxFree-Floor2-Plan.png,
# TaxFree-Render-*.png): Floor 2 of the new west end, x -241.1..-80, z 8..52 (161 x 44 m), a walk-through shop - in from
# the gate walkway at its east end, one light-stone promenade (5 m wide, polished marble as in his render) curving past
# everything, out to gates 9-12 at its west end. Zones east to west: BEAUTY 43 m, LIQUOR 34, AURORA PLAZA 22, ICELAND
# MARKET 26, LUXURY 36; stock rooms along the south wall. Oak floors (the walkway's own matte oak: lit, never mirroring -
# owner 2026-11-17), white marble in beauty and round the plaza, a dark walnut slat ceiling with LED lines, the aurora
# ceiling over the plaza. Floor 1 under it empty for now. The shell, the floors, the ceiling and every one-off piece are
# tf_shell_*; everything that repeats is its own small model placed many times (instanced, Nanite): shelf islands, wall
# bays, pods, kiosks, seats, posts, pendants, boutiques, towers, tables, pay points, plinths, stools
# (Art/TerminalAnnex/taxfree_place.json, read by Tools/terminal_16gates.py). Goods in made-up colours only, no brands.
# Plan metres (x east, y up, z north); his plan's pixels map 10.91 px per metre.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_taxfree.py
import bpy, bmesh, os, json, math, random
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(HERE, 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
X0, X1, Z0, Z1 = -241.1, -80.0, 8.0, 52.0
F2, CEIL, ROOF = 6.0, 11.4, 12.0
def PX(px): return X0 + (px - 113) / 10.91
def PZ(py): return Z1 - (py - 330) / 10.91
ZSTOCK = 11.7                                         # the stock rooms' wall
PIECES = {}
def piece(name):
    if name not in PIECES: PIECES[name] = Piece(name)
    return PIECES[name]
PLACE = []
def place(asset, x, z, rot=0.0, y=F2, s=None):
    PLACE.append({'asset': asset,  # rot: degrees about up; the model's front (+z) then faces plan (-sin rot, cos rot)
                  'x': round(x, 3), 'y': round(y, 3), 'z': round(z, 3), 'rot': rot, **({'s': s} if s else {})})

# ---------------- helpers on a Piece ----------------
def cyl(P, cx, cz, r, y0, y1, m, n=20, top=True, bottom=False, mtop=None):
    pts = [(cx + r * math.cos(2 * math.pi * i / n), cz + r * math.sin(2 * math.pi * i / n)) for i in range(n)]
    for i in range(n):
        (ax, az), (bx, bz) = pts[i], pts[(i + 1) % n]
        mx, mz = (ax + bx) / 2 - cx, (az + bz) / 2 - cz
        P.want.append((P.face([(ax, y0, az), (bx, y0, bz), (bx, y1, bz), (ax, y1, az)], m), (-mx, -mz, 0)))
    if top: P.want.append((P.face([(x, y1, z) for x, z in reversed(pts)], mtop or m), (0, 0, 1)))
    if bottom: P.want.append((P.face([(x, y0, z) for x, z in pts], m), (0, 0, -1)))
def ring(P, cx, cz, r0, r1, y0, y1, m, n=128, mtop=None, a0=0.0, a1=2 * math.pi):
    full = abs(a1 - a0 - 2 * math.pi) < 1e-6
    k = n if full else n
    ang = [a0 + (a1 - a0) * i / n for i in range(n + 1)]
    for i in range(n):
        A, B = ang[i], ang[i + 1]
        o = [(cx + r1 * math.cos(A), cz + r1 * math.sin(A)), (cx + r1 * math.cos(B), cz + r1 * math.sin(B))]
        ii = [(cx + r0 * math.cos(A), cz + r0 * math.sin(A)), (cx + r0 * math.cos(B), cz + r0 * math.sin(B))]
        dx, dz = math.cos((A + B) / 2), math.sin((A + B) / 2)
        P.want.append((P.face([(o[0][0], y0, o[0][1]), (o[1][0], y0, o[1][1]), (o[1][0], y1, o[1][1]), (o[0][0], y1, o[0][1])], m), (-dx, -dz, 0)))
        P.want.append((P.face([(ii[0][0], y0, ii[0][1]), (ii[0][0], y1, ii[0][1]), (ii[1][0], y1, ii[1][1]), (ii[1][0], y0, ii[1][1])], m), (dx, dz, 0)))
        P.want.append((P.face([(o[0][0], y1, o[0][1]), (o[1][0], y1, o[1][1]), (ii[1][0], y1, ii[1][1]), (ii[0][0], y1, ii[0][1])], mtop or m), (0, 0, 1)))
def products(P, x0, x1, y, z0, z1, pal, rnd, hmax=0.32):
    """A shelf's goods: boxes and bottles of made-up colours, side by side."""
    x = x0 + 0.03
    while x < x1 - 0.08:
        w = rnd.uniform(0.07, 0.22); h = rnd.uniform(0.12, hmax); d = min(z1 - z0 - 0.04, rnd.uniform(0.12, 0.3))
        m = rnd.choice(pal)
        if rnd.random() < 0.3 and 'bottle' in pal[0] + pal[-1] or m in ('MI_T_tf_bottle', 'MI_T_tf_wine', 'MI_T_tf_amber'):
            cyl(P, x + w / 2, (z0 + z1) / 2, min(w, d) / 2, y, y + h * 1.2, m, n=8)
        else:
            P.box(x, min(x1, x + w), y, y + h, z0 + 0.02, z0 + 0.02 + d, m)
        x += w + rnd.uniform(0.01, 0.04)

PAL = {'beauty': ['MI_T_tf_pink', 'MI_T_tf_rose', 'MI_T_tf_cream', 'MI_T_tf_lilac', 'MI_T_white', 'MI_T_tf_plum', 'MI_T_black'],
       'liquor': ['MI_T_tf_amber', 'MI_T_tf_bottle', 'MI_T_tf_wine', 'MI_T_tf_amber', 'MI_T_tf_cream', 'MI_T_black'],
       'sweets': ['MI_T_tf_red', 'MI_T_tf_yellow', 'MI_T_tf_sky', 'MI_T_tf_choc', 'MI_T_tf_pink', 'MI_T_white'],
       'market': ['MI_T_tf_wool', 'MI_T_tf_slate', 'MI_T_tf_sky', 'MI_T_tf_choc', 'MI_T_white', 'MI_T_tf_red'],
       'tech': ['MI_T_black', 'MI_T_tf_slate', 'MI_T_tf_navyp', 'MI_T_white', 'MI_T_tf_sky'],
       'lux': ['MI_T_tf_navyp', 'MI_T_gold', 'MI_T_black', 'MI_T_tf_cream', 'MI_T_tf_amber', 'MI_T_tf_slate']}

# ================= the instanced models (each made round its own origin, front toward +z) =================
for pal in ('beauty', 'liquor', 'sweets', 'tech'):
    P = piece('tf_gondola_' + pal); rnd = random.Random(hash(pal) & 0xffff)
    L, Dp, H = 7.0, 1.3, 1.75
    P.box(-L / 2, L / 2, 0.0, 0.12, -Dp / 2, Dp / 2, 'MI_T_black')
    P.box(-0.03 - 0.0, 0.03, 0.12, H, -0.05, 0.05, 'MI_T_white')                       # (the spine, run along x below)
    P.box(-L / 2 + 0.12, L / 2 - 0.12, 0.12, H, -0.04, 0.04, 'MI_T_white')
    for sx in (-1, 1):
        P.box(sx * L / 2 - (0.12 if sx > 0 else 0), sx * L / 2 + (0 if sx > 0 else 0.12), 0.0, H + 0.05, -Dp / 2, Dp / 2, 'MI_T_oak')
        for gy in (0.5, 1.0): P.box(sx * L / 2 - 0.13 if sx > 0 else sx * L / 2 - 0.01, sx * L / 2 + 0.01 if sx > 0 else sx * L / 2 + 0.13, gy, gy + 0.03, -Dp / 2 - 0.005, Dp / 2 + 0.005, 'MI_T_black')
    for side in (-1, 1):
        for sy in (0.12, 0.55, 0.98, 1.41):
            za, zb = (0.04, Dp / 2) if side > 0 else (-Dp / 2, -0.04)
            P.box(-L / 2 + 0.12, L / 2 - 0.12, sy, sy + 0.025, za, zb, 'MI_T_white')
            if sy > 0.2: P.box(-L / 2 + 0.12, L / 2 - 0.12, sy - 0.012, sy, (zb - 0.02) if side > 0 else za, (zb) if side > 0 else za + 0.02, 'MI_T_l_FFFFFF')
            products(P, -L / 2 + 0.14, L / 2 - 0.14, sy + 0.025, za, zb, PAL[pal], rnd, hmax=0.36)
for pal in ('beauty', 'liquor', 'market', 'lux'):
    P = piece('tf_wallbay_' + pal); rnd = random.Random(hash(pal + 'w') & 0xffff)
    W, Dp, H = 3.9, 0.45, 2.6
    P.box(-W / 2, W / 2, 0.0, H, 0.0, 0.03, 'MI_T_l_FFF4D8')                                  # the lit back
    P.box(-W / 2, -W / 2 + 0.06, 0.0, H, 0.0, Dp, 'MI_T_oak'); P.box(W / 2 - 0.06, W / 2, 0.0, H, 0.0, Dp, 'MI_T_oak')
    P.box(-W / 2, W / 2, H, H + 0.08, 0.0, Dp, 'MI_T_oak'); P.box(-W / 2, W / 2, 0.0, 0.15, 0.0, Dp, 'MI_T_black')
    for sy in (0.15, 0.62, 1.09, 1.56, 2.03):
        P.box(-W / 2 + 0.06, W / 2 - 0.06, sy, sy + 0.02, 0.03, Dp, 'MI_T_glass' if sy > 0.2 else 'MI_T_black')
        products(P, -W / 2 + 0.08, W / 2 - 0.08, sy + 0.02, 0.05, Dp - 0.02, PAL[pal], rnd, hmax=0.34)
P = piece('tf_pod'); rnd = random.Random(7)
ring(P, 0, 0, 1.05, 1.4, 0.0, 0.95, 'MI_T_lacquer', mtop='MI_T_white')
ring(P, 0, 0, 1.0, 1.45, 0.95, 1.0, 'MI_T_brass', mtop='MI_T_white')
cyl(P, 0, 0, 0.28, 0.0, 2.3, 'MI_T_white'); ring(P, 0, 0, 0.28, 0.32, 1.9, 2.0, 'MI_T_l_FFFFFF', n=24)
for i in range(18):
    a = 2 * math.pi * i / 18; r = 1.22
    cyl(P, r * math.cos(a), r * math.sin(a), 0.035, 1.0, 1.0 + rnd.uniform(0.08, 0.18), rnd.choice(PAL['beauty']), n=8)
P = piece('tf_kiosk')
P.box(-0.34, 0.34, 0.0, 0.05, -0.29, 0.29, 'MI_T_brass'); P.box(-0.3, 0.3, 0.05, 1.6, -0.25, 0.25, 'MI_T_black')
P.box(-0.24, 0.24, 1.0, 1.42, 0.25, 0.262, 'MI_T_l_FFFFFF'); P.box(-0.26, 0.26, 0.82, 0.86, 0.25, 0.5, 'MI_T_black')
P = piece('tf_seat')
P.box(-1.2, 1.2, 0.0, 0.06, -0.3, 0.3, 'MI_T_brass'); P.box(-1.18, 1.18, 0.06, 0.46, -0.28, 0.28, 'MI_T_tf_velvet')
P = piece('tf_post')
cyl(P, 0, 0, 0.16, 0.0, 0.03, 'MI_T_brass', n=16); cyl(P, 0, 0, 0.03, 0.03, 0.98, 'MI_T_brass', n=10); cyl(P, 0, 0, 0.045, 0.98, 1.02, 'MI_T_brass', n=10)
P = piece('tf_pendant')                                         # hung from the ceiling: the cone's rim at y 0
n = 12
for i in range(n):
    a, b = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
    P.face([(0.22 * math.cos(a), 0.0, 0.22 * math.sin(a)), (0.22 * math.cos(b), 0.0, 0.22 * math.sin(b)), (0.04 * math.cos(b), 0.36, 0.04 * math.sin(b)), (0.04 * math.cos(a), 0.36, 0.04 * math.sin(a))], 'MI_T_gold')
P.face([(0.2 * math.cos(2 * math.pi * i / n), 0.02, 0.2 * math.sin(2 * math.pi * i / n)) for i in range(n)], 'MI_T_l_FFC870')
P.box(-0.006, 0.006, 0.36, 3.0, -0.006, 0.006, 'MI_T_black')
P = piece('tf_boutique'); rnd = random.Random(11)                # 8 x 8 m, its back on the stock wall (z 0), its glass front at z 8
P.box(-4.0, -3.85, 0.0, 4.2, 0.0, 8.0, 'MI_T_tf_walnut'); P.box(3.85, 4.0, 0.0, 4.2, 0.0, 8.0, 'MI_T_tf_walnut')
P.box(-4.0, 4.0, 3.6, 4.2, 7.85, 8.0, 'MI_T_black'); P.box(-4.0, 4.0, 3.66, 3.7, 8.0, 8.02, 'MI_T_gold')
P.box(-3.85, 3.85, 0.0, 0.01, 0.0, 7.85, 'MI_T_tf_stone')
G2 = piece('tf_boutique_glass')
for a_, b_ in ((-3.85, -0.9), (0.9, 3.85)):
    G2.box(a_, b_, 0.01, 3.6, 7.9, 7.93, 'MI_T_glass'); P.box(a_, a_ + 0.05, 0.01, 3.6, 7.88, 7.95, 'MI_T_black')
P.box(-0.95, -0.9, 0.01, 3.6, 7.88, 7.95, 'MI_T_black'); P.box(0.9, 0.95, 0.01, 3.6, 7.88, 7.95, 'MI_T_black')
for sy in (0.4, 0.9, 1.4, 1.9):
    P.box(-3.6, 3.6, sy, sy + 0.03, 0.0, 0.4, 'MI_T_gold'); products(P, -3.5, 3.5, sy + 0.03, 0.02, 0.38, PAL['lux'], rnd, 0.22)
P.box(-3.6, 3.6, 0.0, 2.4, 0.0, 0.02, 'MI_T_black')
P.box(-1.4, 1.4, 0.0, 0.9, 3.4, 4.4, 'MI_T_black'); G2.box(-1.4, 1.4, 0.9, 1.25, 3.4, 4.4, 'MI_T_glass'); P.box(-1.42, 1.42, 1.25, 1.27, 3.38, 4.42, 'MI_T_brass')
products(P, -1.3, 1.3, 0.9, 3.5, 4.3, PAL['lux'], rnd, 0.14)
P = piece('tf_chocotower'); rnd = random.Random(13)
for k in range(9): cyl(P, 0, 0, 1.0 - k * 0.07, k * 0.3, k * 0.3 + 0.28, rnd.choice(['MI_T_tf_choc', 'MI_T_tf_red', 'MI_T_tf_yellow', 'MI_T_gold']), n=24)
ring(P, 0, 0, 0.3, 0.5, 2.7, 2.8, 'MI_T_gold', n=24)
P = piece('tf_table'); rnd = random.Random(17)
P.box(-1.5, 1.5, 0.78, 0.85, -0.7, 0.7, 'MI_T_oak'); P.box(-1.4, 1.4, 0.0, 0.78, -0.6, 0.6, 'MI_T_black')
products(P, -1.4, 1.4, 0.85, -0.6, 0.6, PAL['market'], rnd, 0.2)
P = piece('tf_paypoint')
P.box(-1.2, 1.2, 0.0, 1.0, -0.4, 0.4, 'MI_T_oak'); P.box(-1.25, 1.25, 1.0, 1.05, -0.45, 0.45, 'MI_T_marbleDark')
P.box(-0.25, 0.25, 1.05, 1.4, -0.1, -0.06, 'MI_T_black'); P.box(-0.22, 0.22, 1.08, 1.37, -0.06, -0.055, 'MI_T_l_FFFFFF')
P = piece('tf_plinth'); rnd = random.Random(19)
P.box(-1.0, 1.0, 0.0, 0.6, -0.4, 0.4, 'MI_T_white'); P.box(-1.0, 1.0, 0.6, 0.62, -0.4, 0.4, 'MI_T_gold')
for i in range(5): cyl(P, -0.7 + i * 0.35, 0, 0.07, 0.62, 0.62 + rnd.uniform(0.25, 0.45), rnd.choice(PAL['lux'] + PAL['liquor']), n=10)
P = piece('tf_stool')
cyl(P, 0, 0, 0.2, 0.0, 0.02, 'MI_T_brass', n=12); cyl(P, 0, 0, 0.025, 0.02, 0.72, 'MI_T_brass', n=8); cyl(P, 0, 0, 0.19, 0.72, 0.8, 'MI_T_tf_velvet', n=14)

# ================= the shell and the one-off pieces =================
S = piece('tf_shell_solid'); G = piece('tf_shell_glass'); FL = piece('tf_floor'); AU = piece('tf_aurora'); rnd = random.Random(23)
# ---- Floor 1 (empty for now): floor, walls (cedar outside), its ceiling, the slab ----
S.box(X0, X1, -0.3, 0.0, Z0, Z1 - 0.1, 'MI_T_concrete')
S.box(X0, X1, 5.4, 5.42, Z0, Z1 - 0.1, 'MI_T_offwhite', sides=('-y',))
S.box(X0, X1, 5.42, F2 - 0.01, Z0, Z1 - 0.1, 'MI_T_concrete', sides=('-x', '+x', '-z'))
for (a, b, c, d) in ((X0 - 0.2, X1, Z0 - 0.2, Z0), (X0 - 0.2, X0, Z0, Z1)):
    S.box(a, b, 0.0, ROOF - 0.4, c, d, 'MI_T_cedar')
S.box(X0, X1, 0.0, F2, Z0, Z0 + 0.06, 'MI_T_concrete', sides=('+z',)); S.box(X0, X0 + 0.06, 0.0, F2, Z0, Z1, 'MI_T_concrete', sides=('+x',))
# ---- Floor 2: the oak floor (the walkway's own matte wood), the walls inside, the dark plenum, the roof ----
FL.box(X0, X1, F2 - 0.01, F2, Z0, Z1 - 0.1, 'M_WoodFloor', sides=('+y',))     # the walkway's own oak
S.box(X0, X1, F2, CEIL, Z0, Z0 + 0.06, 'MI_T_plaster', sides=('+z',)); S.box(X0, X0 + 0.06, F2, CEIL, Z0, Z1, 'MI_T_plaster', sides=('+x',))
S.box(X1 - 0.36, X1 - 0.3, F2, CEIL, Z0, Z1 - 0.1, 'MI_T_plaster', sides=('-x',))      # (over the arrivals wing's cedar)
S.box(X0, X1, CEIL, CEIL + 0.02, Z0, Z1, 'MI_T_dark', sides=('-y',))
S.box(X0 - 0.4, X1, ROOF - 0.4, ROOF, Z0 - 0.4, Z1, 'MI_T_roofZinc')
# ---- the polished stone promenade, 5 m wide, his curve from IN (east) to OUT (west) ----
CP = [(1708, 332), (1708, 420), (1660, 480), (1560, 530), (1400, 560), (1250, 560), (1100, 520), (1020, 470), (930, 432),
      (840, 438), (760, 445), (640, 445), (540, 450), (470, 450), (380, 445), (320, 470), (270, 440), (262, 390), (265, 332)]   # (both ends at the shop's doorway: the walkway keeps its own floor - owner 2026-11-17)
CP = [(PX(a), PZ(b)) for a, b in CP]
def cr(p0, p1, p2, p3, t):
    return tuple(0.5 * ((2 * p1[i]) + (-p0[i] + p2[i]) * t + (2 * p0[i] - 5 * p1[i] + 4 * p2[i] - p3[i]) * t * t + (-p0[i] + 3 * p1[i] - 3 * p2[i] + p3[i]) * t ** 3) for i in (0, 1))
# smooth as the roads outside: the curve sampled every 25 cm, its direction smoothed, so both edges run without a kink;
# the polished marble 4.8 m wide with a 10 cm neon line set into the floor on each side, flush with the stone (one
# level, side by side, never overlapping: no flicker)
dense = []
ext = [CP[0]] + CP + [CP[-1]]
for k in range(1, len(ext) - 2):
    for j in range(40): dense.append(cr(ext[k - 1], ext[k], ext[k + 1], ext[k + 2], j / 40.0))
dense.append(CP[-1])
path = [dense[0]]; acc = 0.0
for p_ in dense[1:]:
    acc += math.hypot(p_[0] - path[-1][0], p_[1] - path[-1][1]) if False else 0.0
    if math.hypot(p_[0] - path[-1][0], p_[1] - path[-1][1]) >= 0.25: path.append(p_)
if path[-1] != dense[-1]: path.append(dense[-1])
tang = []
for i in range(len(path)):
    a_ = path[max(0, i - 4)]; b_ = path[min(len(path) - 1, i + 4)]
    tx, tz = b_[0] - a_[0], b_[1] - a_[1]; L = math.hypot(tx, tz) or 1; tang.append((tx / L, tz / L))
def offs(i, d):
    tx, tz = tang[i]; return (path[i][0] - tz * d, path[i][1] + tx * d)
YP = F2 + 0.012
def band(d0, d1, m):
    for i in range(len(path) - 1):
        a0, a1, b0, b1 = offs(i, d0), offs(i + 1, d0), offs(i, d1), offs(i + 1, d1)
        FL.want.append((FL.face([(a0[0], YP, a0[1]), (a1[0], YP, a1[1]), (b1[0], YP, b1[1]), (b0[0], YP, b0[1])], m), (0, 0, 1)))
band(-2.4, 2.4, 'M_Marble')
band(2.4, 2.5, 'MI_T_neonPink'); band(-2.5, -2.4, 'MI_T_neonPink')
# (only the promenade is marble - owner 2026-11-17: the rest the gate walkway's oak)
PCX, PCZ = PX(910), PZ(549)
# ---- the stock rooms (south): their wall with doors, plaster both sides ----
doors = [(-232.0, -230.6), (-196.0, -194.6), (-150.0, -148.6), (-110.0, -108.6)]
for a, b in zip([X0 + 0.06] + [d[1] for d in doors], [d[0] for d in doors] + [X1 - 0.36]):
    S.box(a, b, F2, CEIL, ZSTOCK - 0.1, ZSTOCK + 0.1, 'MI_T_plaster')
for a, b in doors: S.box(a, b, F2 + 2.3, CEIL, ZSTOCK - 0.1, ZSTOCK + 0.1, 'MI_T_plaster'); S.box(a, b, F2, F2 + 2.3, ZSTOCK - 0.02, ZSTOCK + 0.02, 'MI_T_tf_slate')
for xa in (-205.1, -157.1, -123.1): S.box(xa - 0.1, xa + 0.1, F2, CEIL, Z0 + 0.06, ZSTOCK - 0.1, 'MI_T_plaster')
# ---- the ceiling: dark walnut slats across the shop, LED lines along it; the aurora's round opening over the plaza ----
x = X0 + 0.15
while x < X1 - 0.4:
    if abs(x - PCX) < 9.6:
        dz = math.sqrt(max(0.0, 9.6 ** 2 - (x - PCX) ** 2))
        for a, b in ((Z0 + 0.06, PCZ - dz), (PCZ + dz, Z1 - 0.1)):
            if b - a > 0.2: S.box(x, x + 0.09, 10.55, 10.75, a, b, 'MI_T_tf_walnut')
    else:
        S.box(x, x + 0.09, 10.55, 10.75, Z0 + 0.06, Z1 - 0.1, 'MI_T_tf_walnut')
    x += 0.32
# (the slats end on the coffer's circle; its rim a smooth ring)
ring(S, PCX, PCZ, 9.55, 9.65, 10.55, 11.3, 'MI_T_tf_walnut', n=160)
for zl in (14.5, 19.5, 24.5, 29.5, 34.5, 39.5, 44.5, 49.5):          # LED lines every 5 m (his render: bright lines)
    for a, b in ((X0 + 0.3, PCX - 10.0), (PCX + 10.0, X1 - 0.6)):
        S.box(a, b, 10.5, 10.55, zl - 0.04, zl + 0.04, 'MI_T_l_FFFFFF')
# the aurora: a round dark coffer, its glowing ribbons swirling (teal and lilac) as in his render
S.want.append((S.face([(PCX + 9.6 * math.cos(2 * math.pi * i / 160), 11.3, PCZ + 9.6 * math.sin(2 * math.pi * i / 160)) for i in range(160)], 'MI_T_dark'), (0, 0, -1)))
# flat glowing bands swirling overhead (seen from below as in his render), each rising and falling along its sweep
for k, (r, y, m, a0, sweep) in enumerate([(3.0, 10.9, 'MI_T_auroraTeal', 0.3, 4.6), (4.4, 10.7, 'MI_T_auroraLilac', 2.1, 4.0), (5.8, 10.6, 'MI_T_auroraTeal', 3.6, 4.8),
                                           (7.2, 10.45, 'MI_T_auroraTeal', 0.9, 3.6), (8.4, 10.35, 'MI_T_auroraLilac', 4.4, 3.2), (5.1, 10.95, 'MI_T_auroraTeal', 5.2, 2.6)]):
    nn = 160                                   # smooth: 160 steps along each ribbon
    for i in range(nn):
        A = a0 + sweep * i / nn; B = a0 + sweep * (i + 1) / nn
        ya = y - 0.35 * math.sin(i / nn * math.pi); yb = y - 0.35 * math.sin((i + 1) / nn * math.pi)
        wa = 0.25 + 0.3 * math.sin(i / nn * math.pi); wb = 0.25 + 0.3 * math.sin((i + 1) / nn * math.pi)
        q = [(PCX + (r - wa) * math.cos(A), ya, PCZ + (r - wa) * math.sin(A)), (PCX + (r + wa) * math.cos(A), ya, PCZ + (r + wa) * math.sin(A)),
             (PCX + (r + wb) * math.cos(B), yb, PCZ + (r + wb) * math.sin(B)), (PCX + (r - wb) * math.cos(B), yb, PCZ + (r - wb) * math.sin(B))]
        AU.want.append((AU.face(q, m), (0, 0, -1))); AU.want.append((AU.face(list(reversed(q)), m), (0, 0, 1)))   # seen from below and above
# ---- AURORA PLAZA: the champagne and caviar bar round basalt columns in a pool ----
ring(S, PCX, PCZ, 4.4, 5.2, F2, F2 + 1.05, 'MI_T_tf_stone', mtop='MI_T_white')
ring(S, PCX, PCZ, 4.35, 5.3, F2 + 1.05, F2 + 1.1, 'MI_T_marbleDark')
ring(S, PCX, PCZ, 5.18, 5.22, F2 + 0.08, F2 + 0.12, 'MI_T_l_FFFFFF')
ring(S, PCX, PCZ, 3.2, 3.5, F2, F2 + 0.6, 'MI_T_basalt', mtop='MI_T_basalt')
S.want.append((S.face([(PCX + 3.2 * math.cos(2 * math.pi * i / 128), F2 + 0.5, PCZ + 3.2 * math.sin(2 * math.pi * i / 128)) for i in reversed(range(128))], 'MI_T_glassBlue'), (0, 0, 1)))
for i in range(26):
    a = rnd.uniform(0, 2 * math.pi); r = math.sqrt(rnd.random()) * 2.4; h = rnd.uniform(1.4, 5.0) * (1.25 - r / 3.2)
    cyl(S, PCX + r * math.cos(a), PCZ + r * math.sin(a), rnd.uniform(0.32, 0.5), F2 + 0.3, F2 + 0.6 + h, 'MI_T_basalt', n=6)
for i in range(14):
    a = 2 * math.pi * (i + 0.5) / 14
    place('tf_stool', PCX + 5.75 * math.cos(a), PCZ + 5.75 * math.sin(a))
for (px, py, rot) in ((803, 593, 90.0), (1017, 593, 90.0), (955, 656, 0.0)):
    place('tf_seat', PX(px), PZ(py), rot)
# ---- LIQUOR: the wine cellar (a glass room), the whisky and gin tasting bar with its back bar ----
wx0, wx1, wz0, wz1 = PX(1041), PX(1128), PZ(433), PZ(348)
S.box(wx0, wx1, F2, F2 + 0.01, wz0, wz1, 'MI_T_tf_slate', sides=('+y',))
for (a, b, c, d) in ((wx0, wx1, wz0 - 0.03, wz0), (wx0 - 0.03, wx0, wz0, wz1), (wx1, wx1 + 0.03, wz0, wz1)):
    G.box(a, b, F2, F2 + 3.4, c, d, 'MI_T_glass')
S.box(wx0, wx1, F2 + 3.4, F2 + 3.5, wz0 - 0.05, wz1, 'MI_T_black')
for xa in (wx0, wx1): S.box(xa - 0.04, xa + 0.04, F2, F2 + 3.4, wz0 - 0.04, wz0 + 0.04, 'MI_T_black')
S.box((wx0 + wx1) / 2 - 0.8, (wx0 + wx1) / 2 + 0.8, F2, F2 + 3.4, wz0 - 0.05, wz0 + 0.02, 'MI_T_black', sides=('-z',)) if False else None
for zz in (wz1 - 0.5, ):
    for row in range(8):
        S.box(wx0 + 0.3, wx1 - 0.3, F2 + 0.3 + row * 0.36, F2 + 0.33 + row * 0.36, zz - 0.4, zz, 'MI_T_tf_walnut')
        products(S, wx0 + 0.35, wx1 - 0.35, F2 + 0.33 + row * 0.36, zz - 0.38, zz - 0.02, ['MI_T_tf_wine', 'MI_T_tf_bottle', 'MI_T_tf_wine'], rnd, 0.3)
bx0, bx1, bz0, bz1 = PX(1183), PX(1335), PZ(428), PZ(397)
S.box(bx0, bx1, F2, F2 + 1.08, bz0, bz1, 'MI_T_tf_walnut'); S.box(bx0 - 0.05, bx1 + 0.05, F2 + 1.08, F2 + 1.13, bz0 - 0.05, bz1 + 0.05, 'MI_T_marbleDark')
S.box(bx0, bx1, F2 + 0.06, F2 + 0.09, bz0 - 0.01, bz0, 'MI_T_l_FFC870')
kx0, kx1, kz = PX(1194), PX(1324), PZ(364)
S.box(kx0, kx1, F2, F2 + 3.0, kz, kz + 0.45, 'MI_T_tf_walnut'); S.box(kx0 + 0.1, kx1 - 0.1, F2 + 1.1, F2 + 2.9, kz - 0.01, kz, 'MI_T_l_FFC870')
for sy in (1.2, 1.7, 2.2): S.box(kx0 + 0.1, kx1 - 0.1, F2 + sy, F2 + sy + 0.02, kz - 0.3, kz, 'MI_T_glass'); products(S, kx0 + 0.15, kx1 - 0.15, F2 + sy + 0.02, kz - 0.28, kz - 0.02, PAL['liquor'], rnd, 0.32)
for i in range(6): place('tf_stool', bx0 + 1.2 + i * (bx1 - bx0 - 2.4) / 5, bz0 - 0.7)
# ---- ICELAND MARKET: the tobacco room (glass) ----
tx0, tx1, tz0, tz1 = PX(512), PX(588), ZSTOCK + 0.1, PZ(697)
for (a, b, c, d) in ((tx0, tx1, tz1, tz1 + 0.03), (tx0 - 0.03, tx0, tz0, tz1), (tx1, tx1 + 0.03, tz0, tz1)):
    G.box(a, b, F2, F2 + 3.2, c, d, 'MI_T_glass')
S.box(tx0, tx1, F2 + 3.2, F2 + 3.3, tz0, tz1 + 0.05, 'MI_T_black')
for row in range(5):
    S.box(tx0 + 0.3, tx1 - 0.3, F2 + 0.4 + row * 0.45, F2 + 0.43 + row * 0.45, tz0, tz0 + 0.4, 'MI_T_tf_walnut')
    products(S, tx0 + 0.35, tx1 - 0.35, F2 + 0.43 + row * 0.45, tz0 + 0.02, tz0 + 0.38, ['MI_T_tf_slate', 'MI_T_white', 'MI_T_tf_navyp', 'MI_T_tf_red'], rnd, 0.2)
# ---- BEAUTY: the fragrance discovery bar ----
fx0, fx1, fz0, fz1 = PX(1423), PX(1553), PZ(422), PZ(391)
S.box(fx0, fx1, F2, F2 + 1.0, fz0, fz1, 'MI_T_lacquer'); S.box(fx0, fx1, F2 + 1.0, F2 + 1.04, fz0, fz1, 'MI_T_l_FFF4D8')
for i in range(24): cyl(S, fx0 + 0.4 + i * (fx1 - fx0 - 0.8) / 23, (fz0 + fz1) / 2, 0.05, F2 + 1.04, F2 + 1.04 + rnd.uniform(0.1, 0.2), rnd.choice(PAL['beauty']), n=8)
# ---- LUXURY & PAY LOUNGE: the long counter with its 12 tills, the wood wall behind it, queue rails ----
cx0, cx1, cz0, cz1 = PX(147), PX(408), PZ(620), PZ(598)
S.box(cx0, cx1, F2, F2 + 1.0, cz0, cz1, 'MI_T_tf_walnut'); S.box(cx0 - 0.05, cx1 + 0.05, F2 + 1.0, F2 + 1.05, cz0 - 0.05, cz1 + 0.05, 'MI_T_marbleDark')
S.box(cx0, cx1, F2 + 0.06, F2 + 0.09, cz1, cz1 + 0.01, 'MI_T_l_FFFFFF')
S.box(cx0, cx1, F2, CEIL, cz0 - 2.2, cz0 - 2.0, 'MI_T_tf_walnut')                       # the wood wall behind the tills
for i in range(12):
    xx = cx0 + 1.0 + i * (cx1 - cx0 - 2.0) / 11
    S.box(xx - 0.22, xx + 0.22, F2 + 1.05, F2 + 1.4, (cz0 + cz1) / 2 - 0.05, (cz0 + cz1) / 2, 'MI_T_black')
    S.box(xx - 0.2, xx + 0.2, F2 + 1.08, F2 + 1.38, (cz0 + cz1) / 2, (cz0 + cz1) / 2 + 0.005, 'MI_T_l_FFFFFF')
    place('tf_pendant', xx, (cz0 + cz1) / 2, y=F2 + 3.2)
for i in range(13):
    place('tf_post', cx0 + 0.2 + i * (cx1 - cx0 - 0.4) / 12, PZ(588))
S.box(cx0 + 0.2, cx1 - 0.2, F2 + 0.9, F2 + 0.95, PZ(588) - 0.015, PZ(588) + 0.015, 'MI_T_tf_bottle')
for i in range(8): place('tf_kiosk', PX(170) + i * (PX(375) - PX(170)) / 7, PZ(560))
so = {k: v.finish() for k, v in PIECES.items()} if False else None

# ================= the rest of the placed models (his plan's numbers) =================
for x, z in ((PX(435), PZ(362)), (PX(435), PZ(400))): place('tf_gondola_tech', x, z)            # 18 electronics, travel
place('tf_table', PX(440), PZ(531)); place('tf_table', PX(457), PZ(566))                          # 19 travel essentials, 22 gift wrapping
for px in (168, 261, 354, 449): place('tf_boutique', PX(px), ZSTOCK + 0.1)                        # 17 boutiques
place('tf_gondola_sweets', PX(597), PZ(373)); place('tf_gondola_sweets', PX(688), PZ(373))       # 15 sweets
for px in (582, 702): place('tf_table', PX(px), PZ(563))                                          # 14 market tables
place('tf_paypoint', PX(538), PZ(604))                                                            # 16 pay point
for px in (647, 735): place('tf_chocotower', PX(px), PZ(681))                                     # 13 chocolate towers
for px in (1095, 1204, 1313):
    for py in (673, 717): place('tf_gondola_liquor', PX(px), PZ(py))                             # 6 liquor islands
place('tf_paypoint', PX(1362), PZ(625))                                                           # 9
for px in (1461, 1559, 1657, 1755): place('tf_pod', PX(px), PZ(691)); place('tf_gondola_beauty', PX(px), PZ(619))   # 1, 2
for px in (1440, 1495, 1548): place('tf_table', PX(px), PZ(360), s=[0.45, 1.0, 0.6])               # 4 display tables
place('tf_paypoint', PX(1793), PZ(461), 90.0)                                                     # 5
# 24 the backlit wall bays: along the stock wall (facing north) and up the east end wall (facing west)
def bays(xa, xb, pal):
    n = int((xb - xa) // 4.0)
    for i in range(n): place('tf_wallbay_' + pal, xa + 2.0 + i * 4.0, ZSTOCK + 0.1)
bays(PX(588) + 0.3, -179.6, 'market'); bays(-156.8, -150.4, 'liquor'); bays(-148.4, -123.4, 'liquor'); bays(-122.8, -110.4, 'beauty'); bays(-108.4, -84.4, 'beauty')
for i in range(7): place('tf_wallbay_beauty', X1 - 0.36, 15.0 + i * 4.0, 90.0)       # facing west
place('tf_wallbay_lux', X0 + 0.06, 40.0, -90.0); place('tf_wallbay_lux', X0 + 0.06, 44.0, -90.0); place('tf_wallbay_lux', X0 + 0.06, 36.0, -90.0)   # facing east
# 25 window displays behind the glass shop front (facing the walkway)
for px in (446, 544, 642, 740, 838, 936, 1034, 1132, 1230, 1328, 1426, 1524):
    place('tf_plinth', PX(px), Z1 - 1.2)        # facing the walkway through the glass

# ---------------- out: one FBX per piece, the places for the layout ----------------
for name, P in PIECES.items():
    o = P.finish()
    print('TF', name, len(o.data.polygons), 'faces', len(o.data.materials), 'materials')
    for xo in bpy.context.selected_objects: xo.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
json.dump({'pieces': sorted(PIECES), 'place': PLACE}, open(os.path.join(HERE, 'taxfree_place.json'), 'w'), indent=1)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'TaxFree.blend'))
print('TF done', len(PLACE), 'placed')
