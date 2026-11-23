# The gates' curved windows (owner approved 2026-11-23; docs/Blueprints/Gate-Arc, GA-1..GA-8): one 3 m deep true circle per gate
# room, in the terminal's own materials, the same models for every gate (instanced; Nanite but the glass), in the gate's own
# frame (lx = plan x - the room's x0; y, z the plan's): gatearc_geom.py has every number and the list of gates.
#   gatearc_floor_j / _f    the bay's floor: the room's wood floor (by world position: the planks run on), the 0.60 m white band, the white soffit 5.40 (one sheet, under the jetway
#                           corridor too: the same white - owner's correction); _j with the corridor's slot, _f without
#   gatearc_roof            ceiling 11.36, the 0.60 m white band, the roof 12.00
#   gatearc_frame_j / _f    20 radial mullions, sill, head, the two end posts; _j the steel collar, its rubber joint and the
#                           free-standing door wall at z 82 (the glass entrance)
#   gatearc_glass_j / _f    19 curved panes, the flat 2.16 m in front of the lift; _j the entrance's two panes
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/GateArc/make_gate_arc.py
#   then Tools/import_gatearc.py (in Unreal), Tools/terminal_cut.py (the old door panels), Tools/terminal_edit.py
import bpy, bmesh, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE)); sys.path.insert(0, HERE)
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(ROOT, 'Art', 'TerminalAnnex', 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])     # (Piece: one set of rules for every new part)
from gatearc_geom import *
WHITE, MU, CARP, OFFW, PL, GL = 'MI_T_white2', 'MI_T_mullion', 'M_WoodFloor', 'MI_T_offwhite', 'MI_T_plaster', 'MI_T_glassOuter'
STEEL, RUB = 'MI_T_jwTruss', 'MI_T_jwRubber'


def put(P, pts, m, n, uv=None):
    """one face (plan points) that must face the plan direction n; uv: its own texture places"""
    q = [p for i, p in enumerate(pts) if math.dist(p, pts[i - 1]) > 1e-5]
    if len(q) < 3: return
    f = P.face(q, m); P.want.append((f, (-n[0], -n[2], n[1])))
    if uv:
        for l, (x, y, z) in zip(f.loops, q): l[P.uv].uv = uv(x, y, z)


def steps(t0, t1, extra=()):
    """where a curve is cut: the same places for every part (the slab, the bands, the plates meet point on point)"""
    g = [-L / 2 + L * i / 164 for i in range(165)]
    return sorted({round(t, 6) for t in [t0, t1] + [t for t in g if t0 + 0.02 < t < t1 - 0.02] + [t for t in extra if t0 < t < t1]})


def ring(P, t0, t1, o0, o1, h0, h1, m, ts=None, sides='io+-ab', uvm=0.0):
    """a curved bar along the arc, t0..t1, between the offsets o0 and o1, the heights h0 and h1 (sides: inner, outer, +top,
    -bottom, a/b its ends); uvm: texture metres along / up (the glass)"""
    ts = ts or (steps(t0, t1) if abs(t1 - t0) > 0.3 else [t0, t1])
    q = lambda t, o, h: (pt(t, o)[0], h, pt(t, o)[1])
    for a, b in zip(ts, ts[1:]):
        w = (a + b) / 2 / R; n = (math.sin(w), 0, math.cos(w)); k = (-n[0], 0, -n[2]); uv = None
        if uvm:
            def uv(x, y, z, a=a, b=b): return ((a if math.dist((x, z), pt(a)) < math.dist((x, z), pt(b)) else b) / uvm, y / uvm)
        if 'o' in sides: put(P, [q(a, o1, h0), q(b, o1, h0), q(b, o1, h1), q(a, o1, h1)], m, n, uv)
        if 'i' in sides: put(P, [q(a, o0, h0), q(b, o0, h0), q(b, o0, h1), q(a, o0, h1)], m, k, uv)
        if '+' in sides: put(P, [q(a, o0, h1), q(b, o0, h1), q(b, o1, h1), q(a, o1, h1)], m, (0, 1, 0))
        if '-' in sides: put(P, [q(a, o0, h0), q(b, o0, h0), q(b, o1, h0), q(a, o1, h0)], m, (0, -1, 0))
    for s, t, sg in (('a', ts[0], -1), ('b', ts[-1], 1)):
        if s in sides:
            w = t / R; put(P, [q(t, o0, h0), q(t, o1, h0), q(t, o1, h1), q(t, o0, h1)], m, (sg * math.cos(w), 0, -sg * math.sin(w)))


def plate(P, h, off, m, up, z0, slot=None):
    """a flat sheet between the straight line z0 (a number, or a function of lx) and the arc, with a slot (lx a..b) left out"""
    z0f = z0 if callable(z0) else (lambda x: z0)
    ends = [sg * R * math.asin(math.sqrt((R + off) ** 2 - (z0f(H + sg * H) - CZ) ** 2) / (R + off)) for sg in (-1, 1)]     # (where the curve leaves z0)
    ts = steps(-L / 2, L / 2, [tat(s, off) for s in (slot or ())] + ends)
    if slot: ts = [t for t in ts if not any(0 < abs(t - tat(s, off)) < 0.03 for s in slot)]
    for a, b in zip(ts, ts[1:]):
        (xa, za), (xb, zb) = pt(a, off), pt(b, off); y0 = z0f((xa + xb) / 2)
        if slot and slot[0] < (xa + xb) / 2 < slot[1]: continue
        if min(za, zb) < y0 - 1e-4: continue
        put(P, [(xa, h, y0), (xb, h, y0), (xb, h, max(y0, zb)), (xa, h, max(y0, za))], m, (0, 1 if up else -1, 0))


def box(P, x0, x1, y0, y1, z0, z1, m, sides='all'): P.box(x0, x1, y0, y1, z0, z1, m, sides)


PIECES = []


def piece(n):
    p = Piece(n); PIECES.append(p); return p


S1 = JC + 0.045                                      # the slab's slot: 4.5 cm each side of the corridor's fascia
S2 = 1.612                                           # the floor runs on to the corridor's base panel (1.61), over its floor slab's edge
V4 = ('-x', '+x', '-z', '+z')
for J in (1, 0):
    k = 'j' if J else 'f'
    # ---------------------------------------------------------------- the floor
    P = piece('gatearc_floor_' + k)
    plate(P, F2 + 0.008, 0.0, CARP, True, ZE, slot=(JX - S2, JX + S2) if J else None)
    # the soffit: one white sheet from the wall below (its cladding z 82.08 as far as lx 28, the ground gate's window from there)
    plate(P, SOFFIT, E1, WHITE, False, lambda x: 82.05 if x < 28.0 else 81.93)
    if J:
        T0, T1 = tat(JX - S1), tat(JX + S1)
        ring(P, -L / 2, T0, 0.0, E1, SOFFIT, F2 - 0.004, WHITE, sides='oa+')
        ring(P, T1, L / 2, 0.0, E1, SOFFIT, F2 - 0.004, WHITE, sides='ob+')
        ring(P, T0, T1, 0.0, E1, SOFFIT, 5.53, WHITE, sides='oi+')              # (under the corridor: the same band, lower)
        for s in (-1, 1):                                                       # the slot's sides, and a thin rubber bead where the floor meets the corridor
            x = JX + s * S1; z1 = zat(x) + E1
            put(P, [(x, SOFFIT, 82.05), (x, SOFFIT, z1), (x, F2 - 0.004, z1), (x, F2 - 0.004, 82.05)], WHITE, (-s, 0, 0))
            c = JX + s * 1.6135; box(P, c - 0.0085, c + 0.0085, 5.99, 6.024, 82.16, zat(c) - 0.11, RUB)
    else:
        ring(P, -L / 2, L / 2, 0.0, E1, SOFFIT, F2 - 0.004, WHITE, sides='oab+')
    # (the flat end in front of the lift: the white edge runs on straight to the view bay's wall, over the raw slab edge and the open 23 cm under it)
    box(P, X1 + 0.01, XR - 0.01, SOFFIT - 0.003, F2 - 0.004, 81.94, 82.012, WHITE, ('-y', '+y', '+z', '+x'))
    # (its back, inside the floor: the shape closed - its distance field, its shadow)
    put(P, [(0.0, SOFFIT, 82.02), (X1, SOFFIT, 82.02), (X1, 5.98, 82.02), (0.0, 5.98, 82.02)], WHITE, (0, 0, -1))
    # ---------------------------------------------------------------- the frame
    P = piece('gatearc_frame_' + k)
    for i, t in enumerate(MULL):
        if i in (0, NBAY): continue                                             # (the ends: the posts)
        y0 = HEAD[1] - 0.006 if J and i in (12, 13, 14) else F2
        ring(P, t - 0.04, t + 0.04, -0.07, 0.07, y0, GTOP, MU, ts=[t - 0.04, t + 0.04], sides='ioab')
    for a, b in (((-L / 2 + 0.05, TJL), (TJR, L / 2 - 0.05)) if J else ((-L / 2 + 0.05, L / 2 - 0.05),)):
        ring(P, a, b, -0.05, 0.05, F2, F2 + 0.06, MU, sides='io+')
    ring(P, -L / 2 + 0.05, L / 2 - 0.05, -0.05, 0.05, GTOP - 0.044, GTOP, MU, sides='io-')
    for x in (0.0, X1): box(P, x - 0.07, x + 0.07, F2, GTOP, ZG - 0.09, ZG + 0.085, MU, V4)
    if J:
        # the collar: 150 mm of steel, the face of his own gate trim, set in the curve
        for t in (TJL, TJR): ring(P, t - 0.075, t + 0.075, -0.16, 0.16, SOFFIT - 0.006, HEAD[0], STEEL, ts=[t - 0.075, t + 0.075])
        ring(P, TJL - 0.075, TJR + 0.075, -0.16, 0.16, HEAD[0], HEAD[1], STEEL)
        # the rubber joint round the corridor (it stands on its own columns: the bay never loads it)
        for s in (-1, 1):
            xa, xb = sorted((JX + s * 1.578, JX + s * 1.745)); zc = zat(JX + s * 1.66)
            box(P, xa, xb, 5.53, HEAD[0] - 0.005, zc - 0.10, zc + 0.10, RUB, V4)
        ring(P, tat(JX - JC - 0.02), tat(JX + JC + 0.02), -0.08, 0.08, 8.92, HEAD[0] + 0.01, RUB, sides='io-')
        ring(P, tat(JX - S1), tat(JX + S1), 0.005, E1 - 0.005, 5.53, 5.60, RUB, sides='io')
        # the door wall at z 82: today's glass cut to the corridor's width, standing free in the bay - the glass entrance (the door itself untouched)
        for x in (JX - 0.90, JX + 0.90): box(P, x - 0.04, x + 0.04, F2, HEAD[0], 81.845, 81.975, MU, V4)
        box(P, JX - 0.86, JX + 0.86, 8.385, HEAD[0], 81.85, 81.97, PL, ('-z', '+z', '-y'))
        box(P, JX - 1.855, JX + 1.855, HEAD[0] - 0.005, HEAD[1] + 0.004, 81.84, 82.03, STEEL)
        for a, b in ((-1.855, -JC), (JC, 1.855)): box(P, JX + a, JX + b, 5.99, HEAD[0] - 0.005, 81.84, 82.03, STEEL, V4)
    # ---------------------------------------------------------------- the glass
    P = piece('gatearc_glass_' + k); g, lo, hi = 0.035, F2 + 0.05, GTOP - 0.03

    def pane(a, b, y0, y1): ring(P, a, b, -0.015, 0.015, y0, y1, GL, ts=[a + (b - a) * i / 14 for i in range(15)], sides='io', uvm=3.0)

    def flat(x0, x1, y0, y1):
        for z, n in ((ZG - 0.015, -1), (ZG + 0.015, 1)):
            put(P, [(x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z)], GL, (0, 0, n), lambda x, y, z: (x / 3.0, y / 3.0))

    for i in range(NBAY):
        a, b = MULL[i] + g, MULL[i + 1] - g
        if J and i in (12, 13, 14): pane(a, b, HEAD[1] - 0.01, hi)
        else: pane(a, b, lo, hi)
        if J and i == 14: pane(TJR + 0.07, b, lo, HEAD[1] - 0.01)
    flat(X1 + 0.06, XR - 0.035, F2, GTOP)
    if J:
        for a, b in ((-JC, -0.93), (0.93, JC)): flat(JX + a, JX + b, F2, HEAD[0])
# -------------------------------------------------------------------- the roof
P = piece('gatearc_roof')
plate(P, CEIL, -0.05, OFFW, False, ZE)
plate(P, ROOF, -0.043, WHITE, True, ZE)
ring(P, -L / 2, L / 2, -0.044, E1, GTOP - 0.008, ROOF + 0.004, WHITE, sides='oabi+-')
put(P, [(0.0, CEIL, 82.02), (X1, CEIL, 82.02), (X1, ROOF - 0.01, 82.02), (0.0, ROOF - 0.01, 82.02)], WHITE, (0, 0, -1))

for P_ in PIECES:
    bmesh.ops.remove_doubles(P_.bm, verts=P_.bm.verts, dist=2e-5)
    P_.want = [(f, n) for f, n in P_.want if f.is_valid]
    o = P_.finish(); me = o.data
    # a true curve: smooth along it, sharp at every real edge (> 20 degrees, or where the material changes)
    bm = bmesh.new(); bm.from_mesh(me)
    for f in bm.faces: f.smooth = True
    for e in bm.edges:
        e.smooth = len(e.link_faces) == 2 and e.calc_face_angle(3.0) < math.radians(20) and e.link_faces[0].material_index == e.link_faces[1].material_index
    bmesh.ops.triangulate(bm, faces=bm.faces); bm.to_mesh(me); bm.free()
    assert me.materials and all(m for m in me.materials), 'empty material slot ' + o.name
    print('GATEARC', o.name, len(me.polygons), 'triangles', [m.name for m in me.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='OFF', path_mode='STRIP')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'GateArc.blend'))
