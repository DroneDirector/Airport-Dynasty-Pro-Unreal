# The arrivals wing (to-do 72, owner 2026-11-16; design: docs/Baggage-Claim-Concept.md, renders docs/Pictures/BaggageClaim,
# blueprints docs/Baggage-Claim-*Blueprint.png): the museum hall's place and the free ground west of it, flush with the
# end of the gates' pier - x -80..0, z -4..52 - in the terminal plan's metres and the terminal's own materials (slots
# named after /Game/Terminal/Materials; MI_T_basalt and MI_T_cedar made by Tools/make_claim_materials.py). The shell:
#   the claim hall (z 16..52) and the arrivals hall (z -4..8) under oak slats at 5.4 m with LED lines; customs between
#   (z 8..16: the green channel x -44..-36 straight through, the red channel x -54..-48; toilets west, its office east);
#   the dark basalt wall along the east side (x 0) with the store's door; glass fronts to 5.4 m on the kerb side (z -4,
#   the openings for the two sliding doors the layout places) and the west end (x -80); above them zinc cladding to the
#   12 m roof; the escalators' void (x -80..-68.2, z 34.5..52) open up to Floor 2's ceiling (11.4) with its cedar wall
#   on the west, its Floor 2 landing (x -76.1..-68.2 from z 49.76) at the gate walkway's new doorway; the floor with each
#   belt's feed-shaft opening (the belts themselves come in their own models).
# Blender axes for the game's import: (x, y, z) = (-plan x, -plan z, plan y).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_arrivals_wing.py
import bpy, bmesh, json, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
# the west end's own builders (Piece, wall_x, wall_z, slab, glass_front_z) - one set of rules for every new part
src = open(os.path.join(HERE, 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
S = Piece('annex_arrivals_solid'); G = Piece('annex_arrivals_glass')
W, T = 'MI_T_white', 0.2
exec(src[src.index('def wall_x('):src.index('\n', src.index("        S.box(x0, x1, ty - 0.04, ty + 0.04, z - 0.075, z + 0.075, 'MI_T_mullion')")) + 1])

X0, X1, ZK, Z16, ZN = -80.0, 0.0, -4.0, 16.0, 52.0       # the wing's west end, east side, kerb front, claim hall's south wall, north wall
CEIL, F2, ROOF = 5.4, 6.0, 12.0
VOID = (-80.0, -68.2, 34.5, 52.0)                          # the escalators' void (x0, x1, z0, z1)
LAND = 37.46 + 12.3                                        # the Floor 2 landing's edge (the escalators' foot + their top flat)
BELTS = [-60.8, -48.15, -35.5, -22.85, -10.2]; BCZ = 31.0  # the belts' middles (their shafts at x -0.45..+1.4, z +5.4..+6.6)

# ---- the floor: terrazzo, with each belt's feed-shaft opening ----
holes = [(cx - 0.45, cx + 1.4, BCZ + 5.4, BCZ + 6.6) for cx in BELTS]
xs = sorted({X0, X1} | {h[0] for h in holes} | {h[1] for h in holes}); zs = sorted({ZK, ZN} | {h[2] for h in holes} | {h[3] for h in holes})
for xa, xb in zip(xs, xs[1:]):
    for za, zb in zip(zs, zs[1:]):
        if any(h[0] <= xa and xb <= h[1] and h[2] <= za and zb <= h[3] for h in holes): continue
        slab(xa, xb, za, zb, 0.01, top='MI_T_terrazzo', under='MI_T_concrete', thick=0.3)

# ---- the east side (x 0): the basalt wall, full height, with the store's door (x 0..6, z 30..52 behind it) ----
wall_x(-0.1, ZK, ZN, 0.01, ROOF, m_in='MI_T_basalt', holes=[(40.0, 41.4, 0.0, 2.3)])
# ---- the kerb front (z -4): glass to 5.4 m, mullions every 2.5 m, the two door openings (x -46..-41, -39..-34) ----
DOORS = [(-46.0, -41.0), (-39.0, -34.0)]
spans = [(X0, DOORS[0][0]), (DOORS[0][1], DOORS[1][0]), (DOORS[1][1], X1)]
for a_, b_ in spans:
    G.box(a_, b_, 0.01, CEIL, ZK - 0.012, ZK + 0.012, 'MI_T_glass')
    n = max(1, round((b_ - a_) / 2.5)); st = (b_ - a_) / n
    for i in range(n + 1): S.box(a_ + i * st - 0.04, a_ + i * st + 0.04, 0.01, CEIL, ZK - 0.075, ZK + 0.075, 'MI_T_mullion')
for a_, b_ in DOORS:
    G.box(a_, b_, 3.2, CEIL, ZK - 0.012, ZK + 0.012, 'MI_T_glass')          # glass over the doors
    S.box(a_, b_, 3.14, 3.2, ZK - 0.075, ZK + 0.075, 'MI_T_mullion')
S.box(X0, X1, 0.01, 0.06, ZK - 0.075, ZK + 0.075, 'MI_T_mullion'); S.box(X0, X1, CEIL - 0.04, CEIL + 0.04, ZK - 0.075, ZK + 0.075, 'MI_T_mullion')
# ---- the west end (x -80): glass to 5.4 m ----
G.box(X0 - 0.012, X0 + 0.012, 0.01, CEIL, ZK, ZN, 'MI_T_glass')
for i in range(int((ZN - ZK) / 2.5) + 1):
    z = min(ZN, ZK + i * 2.5); S.box(X0 - 0.075, X0 + 0.075, 0.01, CEIL, z - 0.04, z + 0.04, 'MI_T_mullion')
S.box(X0 - 0.075, X0 + 0.075, CEIL - 0.04, CEIL + 0.04, ZK, ZN, 'MI_T_mullion')
# ---- above the glass: cedar cladding to the roof (outside), the void's cedar wall inside ----
#   (cedar planks outside too, owner 2026-11-17: the dark zinc looked heavy in the wave roof's shade)
S.box(X0 - 0.2, X1, CEIL, ROOF - 0.4, ZK - 0.2, ZK, 'MI_T_cedar')
S.box(X0 - 0.2, X0, CEIL, ROOF - 0.4, ZK, ZN, 'MI_T_cedar', sides=('-x', '+y', '-y', '-z', '+z'))
S.box(X0, X0 + 0.06, CEIL - 0.15, 11.4, VOID[2], ZN, 'MI_T_cedar', sides=('+x',))
# (owner, 2026-11-18: the landing opens NORTH, straight ahead off the escalators onto the gate walkway - the walkway's wall
#  over the void is taken out x -80..-68.2 (Data/terminal-cuts.json "touch", rebuilt round it by make_walk_patch.py); the
#  cedar wall on the west stays closed)
# ---- the roof (12 m) and its edge ----
S.box(X0 - 0.4, X1, ROOF - 0.4, ROOF, ZK - 0.4, ZN, 'MI_T_roofZinc')   # (its top at 12.0: under the wave roof's lowest point, 12.3)
S.box(X0 - 0.42, X1, ROOF - 0.45, ROOF + 0.05, ZK - 0.42, ZK - 0.38, 'MI_T_black'); S.box(X0 - 0.42, X0 - 0.38, ROOF - 0.45, ROOF + 0.05, ZK - 0.4, ZN, 'MI_T_black')

# ---- customs (z 8..16) and the claim hall's south wall (z 16) ----
wall_z(Z16, X0, X1, 0.01, CEIL, m='MI_T_plaster', holes=[(-44.0, -36.0, 0.0, 3.0)])
wall_z(8.0, X0, X1, 0.01, CEIL, m='MI_T_plaster', holes=[(-44.0, -36.0, 0.0, 3.0), (-54.0, -48.0, 0.0, 3.0), (-70.6, -69.4, 0.0, 2.3), (-12.6, -11.4, 0.0, 2.3)])
wall_x(-56.0, 8.0, Z16, 0.01, CEIL, m_in='MI_T_plaster'); wall_x(-24.0, 8.0, Z16, 0.01, CEIL, m_in='MI_T_plaster')
S.box(-44.0, -36.0, 0.01, 0.014, 8.1, 15.9, 'MI_T_l_20FF40')       # the green channel's floor strip
S.box(-54.0, -48.0, 0.01, 0.014, 8.1, 15.9, 'MI_T_l_FF2020')       # the red channel's

# ---- the ceilings at 5.4: oak slats (6 x 12 cm, every 12 cm) over a dark plenum, LED lines every 3 m; none over the void ----
def slats(x0, x1, z0, z1):
    z = z0 + 0.05
    while z + 0.06 <= z1:
        S.box(x0, x1, CEIL - 0.12, CEIL, z, z + 0.06, 'MI_T_oak', sides=('-y', '-z', '+z')); z += 0.12
    S.box(x0, x1, CEIL + 0.3, CEIL + 0.34, z0, z1, 'MI_T_dark', sides=('-y',))
for (x0, x1, z0, z1) in ((X0, X1, ZK, VOID[2]), (VOID[1], X1, VOID[2], ZN)): slats(x0, x1, z0, z1)
for z in [ZK + 3.0 + 3.0 * k for k in range(18)]:
    x0 = X0 if z < VOID[2] else VOID[1]
    S.box(x0, X1, CEIL - 0.13, CEIL - 0.11, z - 0.02, z + 0.02, 'MI_T_l_FFFFFF')
# ---- the void: its east and south walls up to Floor 2's ceiling, the ceiling, the Floor 2 landing ----
S.box(VOID[1], VOID[1] + 0.3, CEIL - 0.15, 11.4, VOID[2], ZN, 'MI_T_white')
S.box(X0, VOID[1] + 0.3, CEIL - 0.15, 11.4, VOID[2] - 0.3, VOID[2], 'MI_T_white')
S.box(X0, VOID[1] + 0.3, 11.4, 11.55, VOID[2], ZN, 'MI_T_offwhite', sides=('-y',))
# (owner 2026-11-18: the Floor 2 landing round the escalators' tops in the gate walkway's own oak, not white terrazzo -
#  the escalators' own plates and markings unchanged)
slab(-76.1, -71.2, LAND, ZN, F2 + 0.01, top='M_WoodFloor', under='MI_T_offwhite', thick=0.6)   # (the lift's shaft x -71.2..-68.4 through it)
slab(-68.4, VOID[1], LAND, ZN, F2 + 0.01, top='M_WoodFloor', under='MI_T_offwhite', thick=0.6)
slab(-71.2, -68.4, LAND, 49.4, F2 + 0.01, top='M_WoodFloor', under='MI_T_offwhite', thick=0.6)
slab(X0, -76.1, ZN - 0.1, ZN, F2 + 0.01, top='M_WoodFloor', under='MI_T_offwhite', thick=0.6)
# (owner 2026-11-18, to-do 113: the wood ran over the escalators' top ends - their floor plates, combs and newels reach
#  z 51.90 and are x -79.60..-76.38 across (Escalators.cpp: the pair at x -78.8, 1.62 apart, each 1.60 wide); the wood now
#  stops at their sides and only runs on past their ends)
slab(X0, -79.62, LAND, ZN - 0.1, F2 + 0.01, top='M_WoodFloor', under='MI_T_offwhite', thick=0.6)
slab(-76.36, -76.1, LAND, ZN - 0.1, F2 + 0.01, top='M_WoodFloor', under='MI_T_offwhite', thick=0.6)

# ---- the stair beside the escalators (x -76.1..-74.1, 30 steps of 20 x 38 cm up to the landing): terrazzo with oak
#      treads and black non-slip nosings; its open side and the landing's edge one glass balustrade under one handrail ----
SX0, SX1 = -76.1, -74.1; B0 = LAND - 11.4
# (owner 2026-11-18: the stair's steps as the escalators' - cleated metal, not wood. The visible steps are the game's
#  SM_StairStep copies (Escalators.cpp AddFixedStair, Art/Escalator/make_escalator.py); here only the solid under them,
#  set 12 mm back from each riser and 25 mm under each tread so it never shows through the cleats)
for k in range(30):
    z0 = B0 + k * 0.38; yt = 0.2 * (k + 1)
    S.box(SX0, SX1, 0.01, yt - 0.025, z0 + 0.012, z0 + 0.38, 'MI_T_terrazzo')
S.box(SX1, SX1 + 0.02, 0.01, 6.0, B0, LAND, 'MI_T_marbleDark', sides=('+x',))       # (its side, under the steps)
def tube(pts, r, m):
    # a round rail along a polyline (8 sides), for the handrails
    import math as _m
    for (a, b) in zip(pts, pts[1:]):
        d = [b[i] - a[i] for i in range(3)]; L = _m.sqrt(sum(c * c for c in d)) or 1.0; t = [c / L for c in d]
        up = (0.0, 1.0, 0.0) if abs(t[1]) < 0.9 else (1.0, 0.0, 0.0)
        u = [up[1] * t[2] - up[2] * t[1], up[2] * t[0] - up[0] * t[2], up[0] * t[1] - up[1] * t[0]]; ul = _m.sqrt(sum(c * c for c in u)); u = [c / ul for c in u]
        v = [t[1] * u[2] - t[2] * u[1], t[2] * u[0] - t[0] * u[2], t[0] * u[1] - t[1] * u[0]]
        ring = lambda c, k: tuple(c[i] + r * (_m.cos(k * _m.pi / 4) * u[i] + _m.sin(k * _m.pi / 4) * v[i]) for i in range(3))
        for k in range(8):
            f = S.face([ring(a, k), ring(b, k), ring(b, k + 1), ring(a, k + 1)], m)
            # (each face out of the rail, set by hand: Blender's guess on loose faces flips some in, and the rail looked
            #  broken - owner 2026-11-18; plan -> Blender axes: (x, y, z) -> (-x, -z, y))
            c_, s_ = _m.cos((k + 0.5) * _m.pi / 4), _m.sin((k + 0.5) * _m.pi / 4)
            o_ = [c_ * u[i] + s_ * v[i] for i in range(3)]
            S.want.append((f, (-o_[0], -o_[2], o_[1])))
GX = SX1 + 0.05; HR = 1.0
def zn(z): return 0.2 + (z - B0) * (0.2 / 0.38)
# (owner 2026-11-18, to-do 114: the guard looked broken at the top - the sloped glass and the landing's glass met in a
#  sharp corner with a kinked rail. Now as the escalators' guards: one glass ribbon under one handrail, its slope easing
#  into the level with no kink and turning the corner onto the landing's edge in a round bend; the glass runs up into
#  the rail)
import math as _m
def smin(a, b, k):
    h = max(k - abs(a - b), 0.0) / k; return min(a, b) - h * h * k / 4
def smax(a, b, k): return -smin(-a, -b, k)
zk0 = B0 - 0.2
def rail_y(z): return HR + smax(zn(zk0), smin(F2, zn(z), 0.18), 0.18)
def glass_lo(z): return smax(0.08, smin(F2 + 0.03, zn(z) - 0.14, 0.18), 0.18)   # (at the top: into the clamp shoe)
RC, ZE = 0.15, LAND + 0.04                     # the corner's radius, the landing glass's line
path = []                                      # (x, z, the side the glass faces (nx, nz))
z = B0 - 0.6
while z < ZE - RC - 1e-6: path.append((GX, z)); z += 0.05
for i in range(13):
    a = (_m.pi / 2) * i / 12
    path.append((GX + RC * (1 - _m.cos(a)), ZE - RC + RC * _m.sin(a)))
x = GX + RC
while x < -71.05 - 1e-6: x += 0.10; path.append((min(x, -71.05), ZE))
def at(i):
    x_, z_ = path[i]; zz = min(z_, ZE - RC)                 # (the bend and the landing: level, as at the slope's top)
    return x_, z_, rail_y(zz), glass_lo(zz)
pts = [at(i) for i in range(len(path))] + [(-71.0, ZE, rail_y(ZE - RC), glass_lo(ZE - RC))]
# (owner 2026-11-18, second look: the rail showed seams and the glass broke at the corner - each little piece had its own
#  ends. Now each is ONE continuous sweep: at every point one cross-section, set square to the path's mean direction there,
#  shared by the pieces on both sides, so nothing gaps or overlaps round the bend)
def side_dir(i):
    """the plan direction the guard faces at point i (horizontal, square to the path's mean direction there)"""
    a, b = pts[max(0, i - 1)], pts[min(len(pts) - 1, i + 1)]
    dx, dz = b[0] - a[0], b[1] - a[1]; L = _m.hypot(dx, dz) or 1.0
    return dz / L, -dx / L
def sweep(P, sec, m, hard=True):
    """one continuous skin between consecutive cross-sections (sec(i) -> closed loop of (plan point, outward plan
    direction)); the vertices are shared along the path and the faces smooth-shaded, so the bend reads as one smooth
    surface (owner 2026-11-18: "make sure it's smooth"). hard: the loop's corners stay sharp (the glass's faces and its
    edges, the shoe's sides); else smooth all round (the round rail)"""
    rings = [sec(i) for i in range(len(pts))]
    n = len(rings[0]); bm = P.bm
    B = lambda q: (-q[0], -q[2], q[1])                               # plan -> Blender axes
    if hard:   # each side of the loop its own two columns of vertices
        cols = [[(bm.verts.new(B(rings[i][k][0])), bm.verts.new(B(rings[i][(k + 1) % n][0]))) for i in range(len(pts))] for k in range(n)]
    else:
        ring_v = [[bm.verts.new(B(rings[i][k][0])) for k in range(n)] for i in range(len(pts))]
    L = 0.0
    for i in range(len(pts) - 1):
        d = _m.dist(rings[i][0][0], rings[i + 1][0][0])
        for k in range(n):
            k1 = (k + 1) % n
            if hard: vs = [cols[k][i][0], cols[k][i + 1][0], cols[k][i + 1][1], cols[k][i][1]]
            else: vs = [ring_v[i][k], ring_v[i + 1][k], ring_v[i + 1][k1], ring_v[i][k1]]
            f = bm.faces.new(vs); f.material_index = P.mi(m); f.smooth = True
            for l in f.loops:
                c = l.vert.co; l[P.uv].uv = (-c.x - c.y, c.z)              # (world-space, as the Piece's own faces)
            o_ = [(rings[i][k][1][c] + rings[i][k1][1][c]) * 0.5 for c in range(3)]
            P.want.append((f, B(o_)))
        L += d
# the glass: 16 mm thick, its edges closed, from the shoe up into the rail
def glass_sec(i):
    x_, z_, r_, l_ = pts[i]; nx, nz = side_dir(i); t = 0.008
    return [((x_ + nx * t, l_, z_ + nz * t), (nx, 0, nz)), ((x_ + nx * t, r_ - 0.02, z_ + nz * t), (0, 1, 0)),
            ((x_ - nx * t, r_ - 0.02, z_ - nz * t), (-nx, 0, -nz)), ((x_ - nx * t, l_, z_ - nz * t), (0, -1, 0))]
sweep(G, glass_sec, 'MI_T_escGlass')   # (the escalators' smoked glass, Tools/escalator_glass.py)
# the handrail: a round 70 mm rail, 16 sides
def rail_sec(i):
    x_, z_, r_, l_ = pts[i]; nx, nz = side_dir(i)
    a, b = pts[max(0, i - 1)], pts[min(len(pts) - 1, i + 1)]
    tx, ty, tz = b[0] - a[0], b[2] - a[2], b[1] - a[1]; L = _m.sqrt(tx * tx + ty * ty + tz * tz) or 1.0; tx, ty, tz = tx / L, ty / L, tz / L
    u = (nx, 0.0, nz)                                                  # across, level
    v = (u[1] * tz - u[2] * ty, u[2] * tx - u[0] * tz, u[0] * ty - u[1] * tx)   # up-ish, square to the rail
    if v[1] < 0: v = tuple(-c for c in v)
    out = []
    for k in range(16):
        c_, s_ = _m.cos(2 * _m.pi * k / 16), _m.sin(2 * _m.pi * k / 16)
        d = tuple(c_ * u[q] + s_ * v[q] for q in range(3))
        out.append(((x_ + 0.035 * d[0], r_ + 0.035 * d[1], z_ + 0.035 * d[2]), d))
    return out
sweep(S, rail_sec, 'MI_T_jwRubber', hard=False)
# the black clamp shoe under the glass wherever the guard stands level on the landing - round the bend too
lvl = [i for i in range(len(pts)) if pts[i][1] >= ZE - RC - 1e-6]
def shoe_sec(i):
    x_, z_, r_, l_ = pts[i]; nx, nz = side_dir(i); w = 0.04
    return [((x_ + nx * w, F2, z_ + nz * w), (nx, 0, nz)), ((x_ + nx * w, F2 + 0.1, z_ + nz * w), (0, 1, 0)),
            ((x_ - nx * w, F2 + 0.1, z_ - nz * w), (-nx, 0, -nz)), ((x_ - nx * w, F2, z_ - nz * w), (0, -1, 0))]
_all = pts
pts = [_all[i] for i in lvl]
sweep(S, shoe_sec, 'MI_T_black')
pts = _all
# ---- the lift's glass shaft (x -71.0..-68.6, z 49.6..51.9), from the claim hall up through the landing; its doors on the
#      west side at both floors (the car is the staff lift's, TerminalLift.h) ----
LX0, LX1, LZ0, LZ1 = -71.2, -68.4, 49.4, 52.0
for (a0, a1, b0, b1) in ((LX0, LX1, LZ0, LZ0 + 0.02), (LX1 - 0.02, LX1, LZ0, LZ1)):
    G.box(a0, a1, 0.01, 11.4, b0, b1, 'MI_T_glass')
for (zz0, zz1) in ((LZ0, 50.15), (51.35, LZ1)): G.box(LX0, LX0 + 0.02, 0.01, 11.4, zz0, zz1, 'MI_T_glass')
for y0 in (0.01, F2 + 0.01): G.box(LX0, LX0 + 0.02, y0 + 2.3, y0 + 5.99 if y0 < 1 else 11.4, 50.15, 51.35, 'MI_T_glass')
for x_ in (LX0, LX1):
    for z_ in (LZ0, LZ1): S.box(x_ - 0.04, x_ + 0.04, 0.01, 11.4, z_ - 0.04, z_ + 0.04, 'MI_T_liftBrushed')
# ---- customs: the green channel's sign, the red channel's desk; the arrivals hall's meeting rail, desks and cafe ----
S.box(-44.0, -36.0, 3.1, 3.9, 15.6, 15.7, 'MI_T_l_20FF40'); S.box(-54.0, -48.0, 3.1, 3.9, 15.6, 15.7, 'MI_T_l_FF2020')
S.box(-54.5, -52.5, 0.01, 0.95, 11.0, 13.5, 'MI_T_steel'); S.box(-54.6, -52.4, 0.95, 1.0, 10.9, 13.6, 'MI_T_marbleDark')
for (a0, a1) in ((-62.0, -46.0), (-34.0, -18.0)):
    S.box(a0, a1, 0.95, 1.0, 3.6, 3.7, 'MI_T_steel')
    x_ = a0
    while x_ <= a1 + 0.01: S.box(x_ - 0.03, x_ + 0.03, 0.01, 0.95, 3.62, 3.68, 'MI_T_steel'); x_ += 2.0
for xc in (-76.0, -70.0, -64.0):
    S.box(xc - 2.4, xc + 2.4, 0.01, 1.05, 4.2, 5.0, 'MI_T_oak'); S.box(xc - 2.5, xc + 2.5, 1.05, 1.1, 4.15, 5.05, 'MI_T_marbleDark')
    S.box(xc - 2.5, xc + 2.5, 0.01, 3.2, 7.4, 7.7, 'MI_T_basalt')
S.box(-14.0, -2.0, 0.01, 1.05, 4.0, 5.0, 'MI_T_oak'); S.box(-14.1, -1.9, 1.05, 1.1, 3.95, 5.05, 'MI_T_marbleDark')
# ---- seats: along the west glass clear of the escalators, along the basalt wall, and in the arrivals hall ----
for (xa, xb, zc) in [(-79.2, -78.6, 18.0 + k * 3.2) for k in range(3)] + [(-0.9, -0.3, 20.0 + k * 4.0) for k in range(3)]:
    S.box(xa, xb, 0.42, 0.48, zc, zc + 2.2, 'MI_T_oak'); S.box(xa + 0.2, xb - 0.2, 0.01, 0.42, zc + 0.5, zc + 1.7, 'MI_T_black')
so = S.finish(); go = G.finish()
for o in (so, go):
    print('ANNEX', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'ArrivalsWing.blend'))
print('ANNEX saved')
