# The lorry's cab, modelled (called by make_lorry.py after lorry_details.build; game metres: x forward, y left, z up).
# The owner saw the Meshy cab crumpled and torn (door, side window, wheel arch, wavy panels, smeared paint), so the whole
# cab shell above the chassis is cut out of the Meshy 'Tractor' and built again from one side outline:
#   outer body (rolled edges) - 9 cm wall - inner trim; door apertures with a 5 mm shut gap, door edge 9 cm; window openings
#   with closed reveals, rubber seals and see-through glass; lamp pockets, grille recess, bumper, mirror brackets, a black
#   arch liner with rolled edge, fenders over the drive tandem and the trailer axles.
# Every existing part keeps its name and pivot (Door_L/R, Door_*_Glass, Glass_Windscreen, Mirror_*, lamps, steps, grab
# handles ...); their meshes are swapped where the owner named them. The placeholder CabLining becomes the full interior
# (CabInterior + moving parts SteeringWheel, Needle_Speed/RPM, GearLever, ParkBrake, Pedal_Accel/Brake, SunVisor_L/R).
# Left-hand drive (driver on +y).
import bpy, bmesh, math
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
import forklift_details as FD

Part, material, W = FD.Part, FD.material, FD.W
XB, XF, HY, WALL = 0.88, 3.86, 1.37, 0.09          # cab back, front face, half width (door skin), wall depth
WC, RA = (2.326, 0.60), 0.78                        # front wheel centre (x, z), arch radius
ZB = 1.38                                           # cab bottom; floor inside = ZB + WALL
WIN = dict(a0=1.47, a1=2.52, b0=2.20, b1=3.12)      # door window opening (x, z)
DOOR = dict(a0=1.37, a1=2.62, b0=1.39, b1=3.24)     # door outline (hinge at the front edge, pivot kept)
WS = dict(a0=-1.10, a1=1.10, b0=0.07, b1=0.88)      # windscreen opening (y, metres up the raked face)
P0 = Vector((XF, 0, 1.95)); SL = Vector((-0.12, 0, 0.95)).normalized(); NO = Vector((SL.z, 0, -SL.x))   # windscreen frame


def outer_pts():
    p = [(XB, ZB), (WC[0], ZB)]
    p += [(WC[0] + RA * math.cos(a), WC[1] + RA * math.sin(a)) for a in np.linspace(math.pi / 2, 0, 13)[1:]]
    return p + [(WC[0] + RA, 0.42), (XF - 0.06, 0.42), (XF, 0.48), (XF, 1.95), (XF - 0.12, 2.90), (XF - 0.20, 3.12), (XF - 0.38, 3.32),
                (2.6, 3.55), (1.5, 3.80), (1.05, 3.90), (XB, 3.86)]


def inner_pts(w=WALL):
    return [(XB + w, ZB + w), (XF - w, ZB + w), (XF - w, 1.95), (XF - 0.12 - w, 2.88), (XF - 0.20 - 0.8 * w, 3.12 - w),
            (XF - 0.38 - 0.5 * w, 3.32 - w), (2.6, 3.55 - w), (1.5, 3.80 - w), (1.05, 3.90 - w), (XB + w, 3.86 - w)]


def rrect(a0, a1, b0, b1, r, n=6):
    out = []
    for (ca, cb, t0) in ((a1 - r, b0 + r, -90), (a1 - r, b1 - r, 0), (a0 + r, b1 - r, 90), (a0 + r, b0 + r, 180)):
        for k in range(n + 1):
            t = math.radians(t0 + 90 * k / n); out.append((ca + r * math.cos(t), cb + r * math.sin(t)))
    return out


def F_side(s): return Matrix(((1, 0, 0, 0), (0, 0, s, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
F_WS = Matrix(((0, SL.x, NO.x, P0.x), (1, 0, 0, 0), (0, SL.z, NO.z, P0.z), (0, 0, 0, 1)))


def obj(name, bm, mats=()):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    for m in mats: me.materials.append(m)
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o); return o


def prism_bm(pts, c0, c1, F, bm=None):
    bm = bm or bmesh.new()
    A = [bm.verts.new(F @ Vector((a, b, c0))) for a, b in pts]; B = [bm.verts.new(F @ Vector((a, b, c1))) for a, b in pts]
    bm.faces.new(A[::-1]); bm.faces.new(B)
    for i in range(len(pts)):
        j = (i + 1) % len(pts); bm.faces.new((A[i], A[j], B[j], B[i]))
    return bm


def ring_bm(outer, inner, c0, c1, F, bev=0.0):
    """closed tube of a rounded-rect frame (seal): outer/inner loops equal length"""
    bm = bmesh.new(); L = []
    for loop, c in ((outer, c1), (inner, c1), (inner, c0), (outer, c0)):
        L.append([bm.verts.new(F @ Vector((a, b, c))) for a, b in loop])
    n = len(outer)
    for k in range(4):
        P, Q = L[k], L[(k + 1) % 4]
        for i in range(n):
            j = (i + 1) % n; bm.faces.new((P[i], P[j], Q[j], Q[i]))
    if bev: bmesh.ops.bevel(bm, geom=[e for e in bm.edges if e.calc_face_angle(0) > 1.0], offset=bev, segments=2, profile=0.5, affect='EDGES', clamp_overlap=True)
    return bm


def solid(pts, hy, rk, rs):
    """the side outline extruded across +-hy; kinks rounded rk, every edge round the side faces rolled rs"""
    bm = bmesh.new(); n = len(pts)
    A = [bm.verts.new((x, hy, z)) for x, z in pts]; B = [bm.verts.new((x, -hy, z)) for x, z in pts]
    bm.faces.new(A); bm.faces.new(B[::-1])
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((A[i], A[j], B[j], B[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    kink = []
    for i in range(n):
        p0, p1, p2 = (np.array(pts[k]) for k in (i - 1, i, (i + 1) % n))
        d1, d2 = p1 - p0, p2 - p1
        if math.degrees(math.acos(np.clip(d1 @ d2 / np.linalg.norm(d1) / np.linalg.norm(d2), -1, 1))) > 20:
            kink.append(bm.edges.get((A[i], B[i])))
    bmesh.ops.bevel(bm, geom=kink, offset=rk, segments=3, profile=0.5, affect='EDGES', clamp_overlap=True)
    side = {e for f in bm.faces if abs(f.normal.y) > 0.99 for e in f.edges}
    bmesh.ops.bevel(bm, geom=list(side), offset=rs, segments=4, profile=0.5, affect='EDGES', clamp_overlap=True)
    bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4], quad_method='BEAUTY', ngon_method='EAR_CLIP')
    return bm


def apply_bool(o, cutter, op):
    m = o.modifiers.new('b', 'BOOLEAN'); m.operation = op; m.solver = 'EXACT'; m.object = cutter
    dg = bpy.context.evaluated_depsgraph_get(); me = bpy.data.meshes.new_from_object(o.evaluated_get(dg))
    o.modifiers.clear(); old = o.data; o.data = me; bpy.data.meshes.remove(old)


def tree_of(o):
    bm = bmesh.new(); bm.from_mesh(o.data); bmesh.ops.transform(bm, matrix=o.matrix_world, verts=bm.verts)
    t = BVHTree.FromBMesh(bm); bm.free(); return t


def join_into(target, parts):
    for o in bpy.context.scene.objects: o.select_set(False)
    for o in parts: o.select_set(True)
    target.select_set(True); bpy.context.view_layer.objects.active = target; bpy.ops.object.join()


def swap_mesh(o, src):
    """o keeps its name, pivot and matrix; its mesh becomes src's (world coordinates)"""
    me = src.data; me.transform(o.matrix_world.inverted()); old = o.data; o.data = me; me.name = o.name
    bpy.data.objects.remove(src, do_unlink=True); bpy.data.meshes.remove(old)


def sweep(P, path, prof, up, mi, caps=True):
    """profile (u along up, v along tangent x up) swept along the 3D path"""
    t = bmesh.new(); rings = []; path = [Vector(p) for p in path]; up = Vector(up)
    for i, p in enumerate(path):
        tg = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized(); bn = tg.cross(up).normalized(); u = bn.cross(tg)
        rings.append([t.verts.new(p + u * a + bn * b) for a, b in prof])
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(len(prof)):
            j = (i + 1) % len(prof); t.faces.new((r0[i], r0[j], r1[j], r1[i]))
    if caps: t.faces.new(rings[0][::-1]); t.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(t, faces=t.faces); P._merge(t, mi)


def circle(c, r, n=10): return [(c[0] + r * math.cos(2 * math.pi * k / n), c[1] + r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def fender(P, axles, zt, yi, yo, mi_plate, mi_lip, end=60):
    """mudguard over one or more axles: plate 6 mm, rolled outer lip; arcs down over the first/last tyre"""
    R = zt - 0.60; ax = sorted(axles, reverse=True); path = []
    path += [(ax[0] + R * math.cos(math.radians(a)), 0, 0.60 + R * math.sin(math.radians(a))) for a in np.linspace(90 - end, 90, 9)]
    path += [(ax[-1] + R * math.cos(math.radians(a)), 0, 0.60 + R * math.sin(math.radians(a))) for a in np.linspace(90, 90 + end, 9)]
    s = 1 if yo > 0 else -1
    for yy, pr, mi in ((0, [(yi, 0), (yo, 0), (yo, 0.006), (yi, 0.006)], mi_plate),):
        sweep(P, [(x, 0, z) for x, _, z in path], [(a, b) for a, b in pr], (0, 1, 0), mi)
    sweep(P, [(x, 0, z) for x, _, z in path], circle((yo, 0.0), 0.011), (0, 1, 0), mi_lip)


def build(sc, body, trailer, PARTS):
    INFO, IKN, mats = [], {}, {}
    def mat(name, col, rough, metal=0.0, emit=None):
        if name not in mats: mats[name] = material(name, col, rough, metal, emit)
        return mats[name]
    paint = mat('M_Lorry_CabPaint', (0.035, 0.19, 0.52), 0.28)
    p = paint.node_tree.nodes['Principled BSDF']; p.inputs['Clearcoat'].default_value = 1.0; p.inputs['Clearcoat Roughness'].default_value = 0.05
    trim = mat('M_Lorry_Trim', (0.10, 0.10, 0.105), 0.65); head = mat('M_Lorry_Headliner', (0.42, 0.41, 0.39), 0.9)
    blk = bpy.data.materials['M_Lorry_BlackPlastic']; rub = bpy.data.materials['M_Lorry_Rubber']; chrome = bpy.data.materials['M_Lorry_Chrome']
    seal = mat('M_Lorry_Seal', (0.008, 0.008, 0.009), 0.55); glass = bpy.data.materials['M_Lorry_Glass']
    fab = mat('M_Lorry_SeatFabric', (0.045, 0.047, 0.055), 0.95); dash = mat('M_Lorry_Dash', (0.025, 0.025, 0.028), 0.55)
    dial = mat('M_Lorry_DialFace', (0.01, 0.01, 0.012), 0.3); white = mat('M_Lorry_DialMarks', (0.9, 0.9, 0.88), 0.4, 0.0, (0.9, 0.9, 0.85))
    needle = mat('M_Lorry_Needle', (0.9, 0.12, 0.02), 0.4, 0.0, (1.0, 0.2, 0.02)); screen = mat('M_Lorry_Screen', (0.004, 0.006, 0.01), 0.08, 0.0, (0.02, 0.05, 0.09))
    knob = mat('M_Lorry_ParkKnob', (0.75, 0.45, 0.02), 0.4); matm = mat('M_Lorry_FloorMat', (0.02, 0.02, 0.022), 0.85); bed = mat('M_Lorry_Mattress', (0.09, 0.08, 0.07), 0.9)
    # tyres: black rubber, little sheen (they rendered grey); glass: see-through (transmission), still alpha for the game
    r = rub.node_tree.nodes['Principled BSDF']; r.inputs['Base Color'].default_value = (0.010, 0.010, 0.011, 1); r.inputs['Roughness'].default_value = 0.78; r.inputs['Specular'].default_value = 0.25
    g = glass.node_tree.nodes['Principled BSDF']; g.inputs['Transmission'].default_value = 1.0; g.inputs['IOR'].default_value = 1.52
    g.inputs['Roughness'].default_value = 0.0; g.inputs['Base Color'].default_value = (0.80, 0.86, 0.84, 1)
    glass.use_backface_culling = False; glass.shadow_method = 'HASHED'

    # ------------------------------------------------ 1. the Meshy cab shell out (chassis, tanks, steps' Meshy remains kept)
    n = FD.delete_faces(body, lambda v: (v[:, 0] > 0.80) & (v[:, 2] > 1.30), True)
    n += FD.delete_faces(body, lambda v: (v[:, 0] > 2.75) & (v[:, 2] > 0.30) & ~((np.abs(v[:, 1]) < 0.55) & (v[:, 2] < 1.0) & (v[:, 0] < 3.3)))
    n += FD.delete_faces(body, lambda v: (v[:, 0] > 1.55) & (v[:, 0] < 2.80) & (np.abs(v[:, 1]) > 0.85) & (v[:, 2] > 0.9))
    INFO.append('Cab: %d Meshy faces cut out (cab shell, front, arches)' % n)

    # ------------------------------------------------ 2. shell = outer - inner - openings
    O = obj('_O', solid(outer_pts(), HY, 0.06, 0.085)); I = obj('_I', solid(inner_pts(), HY - WALL, 0.03, 0.03))
    tO, tI = tree_of(O), tree_of(I)
    shell = obj('_Shell', solid(outer_pts(), HY, 0.06, 0.085)); apply_bool(shell, I, 'DIFFERENCE')
    door_o = {s: rrect(DOOR['a0'], DOOR['a1'], DOOR['b0'], DOOR['b1'], 0.06) for s in (1, -1)}
    cut = []
    for s in (1, -1):
        c = obj('_c', prism_bm(rrect(DOOR['a0'] - 0.005, DOOR['a1'] + 0.005, 1.30, DOOR['b1'] + 0.005, 0.065), HY - WALL - 0.02, HY + 0.3, F_side(s))); cut.append(c)
    cut.append(obj('_c', prism_bm(rrect(WS['a0'], WS['a1'], WS['b0'], WS['b1'], 0.09), -0.25, 0.10, F_WS)))
    lamps = [PARTS[k] for k in ('Light_Head_L', 'Light_Head_R', 'Light_Indicator_FL', 'Light_Indicator_FR') if k in PARTS]
    for L in lamps:
        v = W(L); lo, hi = v.min(0) - 0.008, v.max(0) + 0.008; s = 1 if lo[1] > 0 else -1
        if 'Indicator' in L.name: lo[1], hi[1] = (lo[1], HY + 0.2) if s > 0 else (-HY - 0.2, hi[1])
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
        for q in bm.verts: q.co = Vector(((lo[0] + XF + 0.3) / 2 + q.co.x * (XF + 0.3 - lo[0]), (lo[1] + hi[1]) / 2 + q.co.y * (hi[1] - lo[1]), (lo[2] + hi[2]) / 2 + q.co.z * (hi[2] - lo[2])))
        cut.append(obj('_c', bm))
    cut.append(obj('_c', prism_bm(rrect(-0.72, 0.72, 1.52, 1.92, 0.03), XF - 0.03, XF + 0.3, Matrix(((0, 0, 1, 0), (1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 0, 1))))))
    for c in cut: apply_bool(shell, c, 'DIFFERENCE')
    # doors: the same wall, inside the door outline, minus the window
    doors = {}
    for s in (1, -1):
        d = obj('_D', solid(outer_pts(), HY, 0.06, 0.085)); apply_bool(d, I, 'DIFFERENCE')
        box = obj('_c', prism_bm(door_o[s], HY - WALL - 0.005, HY + 0.3, F_side(s))); apply_bool(d, box, 'INTERSECT'); cut.append(box)
        w = obj('_c', prism_bm(rrect(WIN['a0'], WIN['a1'], WIN['b0'], WIN['b1'], 0.05), HY - 0.3, HY + 0.3, F_side(s))); apply_bool(d, w, 'DIFFERENCE'); cut.append(w)
        doors[s] = d

    def paint_faces(o, door):
        o.data.materials.clear()
        for m in (paint, trim, head, blk, seal): o.data.materials.append(m)
        M = o.matrix_world; idx = []
        for f in o.data.polygons:
            c = M @ f.center; nrm = (M.to_3x3() @ f.normal).normalized()
            dO = (tO.find_nearest(c)[3] or 1); dI = (tI.find_nearest(c)[3] or 1)
            if dO < 0.003: k = 3 if nrm.z < -0.6 or (c.z < 1.40 and abs(c.y) < HY - 0.05) else 0
            elif dI < 0.003: k = 2 if nrm.z < -0.5 and c.z > 2.8 else 3 if nrm.z > 0.7 and c.z < 1.5 else 1
            elif door or (DOOR['a0'] - 0.02 < c.x < DOOR['a1'] + 0.02 and abs(c.y) > HY - WALL - 0.03 and c.z > 1.38):
                inwin = WIN['a0'] - 0.01 < c.x < WIN['a1'] + 0.01 and WIN['b0'] - 0.01 < c.z < WIN['b1'] + 0.01
                k = 4 if inwin else 0
            else: k = 4 if c.z > 1.98 and c.x > 3.5 else 3
            idx.append(k)
        o.data.polygons.foreach_set('material_index', idx)
        o.data.polygons.foreach_set('use_smooth', [True] * len(idx)); o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(40)
    paint_faces(shell, False)
    for s in (1, -1): paint_faces(doors[s], True)

    # ------------------------------------------------ 3. seals + glass (windscreen, door windows)
    E = Part('_Ext', [blk, seal, chrome, paint, trim])
    def seal_glass(F, a0, a1, b0, b1, r, sign=1):
        E._merge(ring_bm(rrect(a0 - 0.006, a1 + 0.006, b0 - 0.006, b1 + 0.006, r + 0.006), rrect(a0 + 0.022, a1 - 0.022, b0 + 0.022, b1 - 0.022, max(r - 0.022, 0.01)),
                         -0.042, -0.004, F, 0.004), 1)
        G = Part('_G', [glass]); G._merge(prism_bm(rrect(a0 + 0.012, a1 - 0.012, b0 + 0.012, b1 - 0.012, max(r - 0.012, 0.01)), -0.026, -0.020, F), 0); return G.done(sc)
    gw = seal_glass(F_WS, WS['a0'], WS['a1'], WS['b0'], WS['b1'], 0.09)
    PARTS['Glass_Windscreen'].matrix_world = Matrix.Identity(4); swap_mesh(PARTS['Glass_Windscreen'], gw)
    for s, side in ((1, 'L'), (-1, 'R')):
        D = Part('_DS', [seal, chrome, blk, trim, dash])
        D._merge(ring_bm(rrect(WIN['a0'] - 0.006, WIN['a1'] + 0.006, WIN['b0'] - 0.006, WIN['b1'] + 0.006, 0.056), rrect(WIN['a0'] + 0.02, WIN['a1'] - 0.02, WIN['b0'] + 0.02, WIN['b1'] - 0.02, 0.03),
                         HY - 0.040, HY - 0.003, F_side(s), 0.004), 0)
        G = Part('_G', [glass]); G._merge(prism_bm(rrect(WIN['a0'] + 0.012, WIN['a1'] - 0.012, WIN['b0'] + 0.012, WIN['b1'] - 0.012, 0.04), HY - 0.026, HY - 0.020, F_side(s)), 0)
        swap_mesh(PARTS['Door_%s_Glass' % side], G.done(sc))
        # outside handle: dark recess plate + grip bar on two posts (at the IK point)
        D.box((1.40, s * 1.366, 2.185), (1.56, s * 1.374, 2.255), 2, 0.003)
        D.box((1.42, s * 1.385, 2.205), (1.54, s * 1.397, 2.235), 1, 0.005)
        for xx in (1.43, 1.53): D.box((xx - 0.008, s * 1.372, 2.212), (xx + 0.008, s * 1.387, 2.228), 1, 0.002)
        # door card: armrest, window switch, pull handle, speaker, pocket, inner release
        yi = HY - WALL
        D.box((1.55, s * (yi - 0.075), 2.00), (2.30, s * (yi + 0.004), 2.075), 3, 0.018)
        D.box((2.08, s * (yi - 0.06), 2.075), (2.20, s * (yi - 0.015), 2.088), 4, 0.004)        # window switch block
        for xx in (2.10, 2.15): D.box((xx, s * (yi - 0.05), 2.088), (xx + 0.03, s * (yi - 0.025), 2.096), 2, 0.002)
        D._merge(sweep_tmp([(1.62, s * (yi + 0.004), 2.16), (1.64, s * (yi - 0.045), 2.17), (1.80, s * (yi - 0.045), 2.17), (1.82, s * (yi + 0.004), 2.16)], 0.013), 3)
        D.cyl((2.27, s * (yi + 0.004), 1.68), (2.27, s * (yi - 0.012), 1.68), 0.095, 3, 32, 0.004)
        D.cyl((2.27, s * (yi - 0.010), 1.68), (2.27, s * (yi - 0.016), 1.68), 0.08, 2, 32, 0.0)
        for rr in (0.025, 0.045, 0.065): D.torus((0, 0, 0), rr, 0.003, 4, 32, 6, M=Matrix.Translation((2.27, s * (yi - 0.016), 1.68)) @ Matrix.Rotation(math.pi / 2, 4, 'X'))
        D.box((1.58, s * (yi - 0.06), 1.52), (2.10, s * (yi + 0.004), 1.78), 3, 0.015)
        D.box((1.47, s * (yi - 0.02), 2.30), (1.53, s * (yi + 0.003), 2.40), 4, 0.008); D.box((1.475, s * (yi - 0.035), 2.335), (1.525, s * (yi - 0.015), 2.365), 1, 0.006)
        dd = D.done(sc); join_into(doors[s], [dd])
        swap_mesh(PARTS['Door_' + side], doors[s]); PARTS['Door_' + side].data.name = 'Door_' + side
        IKN['DoorInnerHandle_' + side] = (1.50, s * (yi - 0.03), 2.35)
        # door shut, sealed: ring A frames the aperture, sunk 5 mm into the wall lining (no slit behind it) and closes
        # under the door sill; ring B is the lip 2 mm inside the closed door, overlapping its edge 3 cm and filling the
        # floor slot along the sill up to 1 cm above the floor - the 5 mm shut gap shows rubber, never light
        E._merge(ring_bm(rrect(DOOR['a0'] - 0.025, DOOR['a1'] + 0.025, ZB - 0.01, DOOR['b1'] + 0.025, 0.085),
                         rrect(DOOR['a0'] - 0.005, DOOR['a1'] + 0.005, ZB + 0.005, DOOR['b1'] + 0.005, 0.065),
                         HY - WALL - 0.03, HY - WALL + 0.005, F_side(s), 0.003), 1)
        E._merge(ring_bm(rrect(DOOR['a0'] - 0.008, DOOR['a1'] + 0.008, ZB, DOOR['b1'] + 0.008, 0.068),
                         rrect(DOOR['a0'] + 0.03, DOOR['a1'] - 0.03, ZB + WALL + 0.01, DOOR['b1'] - 0.03, 0.03),
                         HY - WALL - 0.028, HY - WALL - 0.002, F_side(s), 0.002), 1)
        # mirror bracket on the A-pillar up to the kept mirror pivot (hinge pin through the mirror arms)
        mp = PARTS['Mirror_' + side].matrix_world.translation
        for zz in (mp.z + 0.30, mp.z - 0.15):
            E.cyl((mp.x, s * (HY - 0.01), zz), (mp.x, mp.y - s * 0.022, zz), 0.016, 0, 14)
            E.box((mp.x - 0.05, s * (HY - 0.01), zz - 0.05), (mp.x + 0.05, s * (HY + 0.008), zz + 0.05), 0, 0.006)
            E.cyl((mp.x, mp.y, zz - 0.045), (mp.x, mp.y, zz - 0.018), 0.02, 0, 14)
        E.cyl((mp.x, mp.y, mp.z - 0.20), (mp.x, mp.y, mp.z + 0.36), 0.006, 2, 10)
    # grille slats in the recess, bumper, arch flare round the front arch, rear mudguard quarter behind the front tyre
    for k in range(6):
        z = 1.545 + k * 0.065; E.box((XF - 0.032, -0.70, z), (XF - 0.008, 0.70, z + 0.028), 0, 0.006)
    E.box((XF - 0.03, -1.20, 0.45), (XF + 0.09, 1.20, 0.84), 3, 0.05); E.box((XF + 0.06, -1.0, 0.53), (XF + 0.095, 1.0, 0.60), 0, 0.01)
    for s in (1, -1):
        arc = [(WC[0] + (RA + 0.004) * math.cos(a), 0, WC[1] + (RA + 0.004) * math.sin(a)) for a in np.linspace(0, math.pi / 2, 16)]
        sweep(E, arc, circle((s * (HY - 0.012), 0), 0.014, 10), (0, 1, 0), 0)
        rq = [(WC[0] + 0.70 * math.cos(a), 0, WC[1] + 0.70 * math.sin(a)) for a in np.linspace(math.pi / 2, math.radians(148), 10)]
        sweep(E, rq, [(s * 0.92, 0), (s * 1.30, 0), (s * 1.30, 0.006), (s * 0.92, 0.006)], (0, 1, 0), 0)
        sweep(E, rq, circle((s * 1.30, 0.003), 0.011), (0, 1, 0), 0)
        E.cyl((WC[0] - 0.25, s * 0.55, 1.05), (WC[0] - 0.25, s * 0.93, 1.29), 0.015, 0, 10)
    # fenders over the drive tandem (tractor) - top kept under the trailer's swing
    xs = [PARTS['Wheel_%sL' % a].matrix_world.translation.x for a in ('D1', 'D2')]
    for s in (1, -1):
        fender(E, xs, 1.240, s * 0.86, s * 1.29, 0, 0)
        for xx in xs: E.cyl((xx, s * 0.45, 1.00), (xx, s * 0.87, 1.235), 0.015, 0, 10)
    ext = E.done(sc); join_into(body, [shell, ext])
    # trailer fenders: under the box floor (ray up), over the three axles
    tt = tree_of(trailer); xs = [PARTS['Wheel_%sL' % a].matrix_world.translation.x for a in ('T1', 'T2', 'T3')]
    zs = [h[0].z for x in (xs[0], xs[2]) for yy in (0.9, 1.25, -0.9, -1.25) for h in [tt.ray_cast(Vector((x, yy, 1.21)), Vector((0, 0, 1)), 1.0)] if h[0] is not None]
    zt = min(1.27, (min(zs) - 0.012) if zs else 1.27)
    if zt > 1.225:
        T = Part('_TF', [blk])
        for s in (1, -1):
            fender(T, xs, zt, s * 0.86, s * 1.27, 0, 0)
            for xx in xs: T.cyl((xx, s * 0.95, zt + 0.004), (xx, s * 0.95, zt + 0.004 + max(0.005, (min(zs) if zs else zt) - zt - 0.004)), 0.012, 0, 8)
        join_into(trailer, [T.done(sc)])
        INFO.append('Trailer fenders top z %.3f (box underside %.3f)' % (zt, min(zs) if zs else -1))
    else: INFO.append('Trailer fenders skipped: box underside %.3f too low' % min(zs))

    # ------------------------------------------------ 4. interior (static) + moving controls
    if 'CabLining' in PARTS: bpy.data.objects.remove(PARTS.pop('CabLining'), do_unlink=True)
    zf = ZB + WALL; yd = 0.62
    C = Part('CabInterior', [trim, dash, fab, blk, chrome, matm, screen, bed, white, dial, head])
    C.box((3.36, -1.27, zf - 0.01), (3.78, 1.27, 1.90), 1, 0.05)                       # dash body against the front wall
    C.box((3.24, -1.27, 1.86), (3.78, 1.27, 1.99), 1, 0.045)                          # rounded dash top pad
    C.box((3.16, 0.28, 1.96), (3.40, 0.96, 2.22), 1, 0.05)                            # instrument binnacle (hood)
    for yy, nm in ((0.46, 'Speed'), (0.78, 'RPM')):
        C.cyl((3.168, yy, 2.08), (3.18, yy, 2.08), 0.085, 9, 40, 0.0)
        C.torus((0, 0, 0), 0.087, 0.006, 4, 40, 8, M=Matrix.Translation((3.165, yy, 2.08)) @ Matrix.Rotation(math.pi / 2, 4, 'Y'))
        for k in range(11):                                                             # tick marks
            a = math.radians(225 - 27 * k); C.box((3.163, yy + 0.07 * math.cos(a) - 0.002, 2.08 + 0.07 * math.sin(a) - 0.002), (3.167, yy + 0.07 * math.cos(a) + 0.002, 2.08 + 0.07 * math.sin(a) + 0.002), 8, 0.0)
        N = Part('Needle_' + nm, [needle, chrome]); N.box((3.158, yy - 0.003, 2.08), (3.162, yy + 0.003, 2.145), 0, 0.001); N.cyl((3.157, yy, 2.08), (3.163, yy, 2.08), 0.009, 1, 16, 0.001)
        PARTS['Needle_' + nm] = N.done(sc, (3.16, yy, 2.08)); INFO.append('Needle_%s pivot 3.160 %.3f 2.080, turns about x (0 = straight up, -135..+135 deg)' % (nm, yy))
    C.box((3.30, -0.15, 1.94), (3.34, 0.22, 2.08), 6, 0.006)                           # centre screen
    for k in range(6): C.box((3.335, -0.13 + k * 0.06, 1.89), (3.355, -0.09 + k * 0.06, 1.915), 3, 0.004)   # switch row
    for yy in (-1.05, -0.55, 1.12):                                                     # vents
        C.box((3.235, yy - 0.09, 1.905), (3.25, yy + 0.09, 1.975), 3, 0.006)
        for k in range(4): C.box((3.228, yy - 0.085, 1.912 + k * 0.016), (3.236, yy + 0.085, 1.918 + k * 0.016), 4, 0.001)
    C.box((3.20, -1.05, 1.60), (3.37, -0.35, 1.84), 1, 0.03)                           # glovebox bulge, passenger
    # steering column + shroud (wheel tilted 30 deg from horizontal)
    nW = Vector((-0.5, 0, 0.866)); cW = Vector((3.00, yd, 2.02))
    C.cyl(cW - nW * 0.07, cW - nW * 0.62, 0.035, 3, 16); C.cyl(cW - nW * 0.12, cW - nW * 0.40, 0.065, 3, 20, 0.01, r2=0.08)
    S = Part('SteeringWheel', [blk, chrome, dash]); Mw = Matrix.Translation(cW) @ nW.to_track_quat('Z', 'X').to_matrix().to_4x4()
    S.torus((0, 0, 0), 0.24, 0.019, 0, 56, 12, M=Mw); S.cyl((0, 0, -0.04), (0, 0, 0.02), 0.07, 2, 28, 0.01, M=Mw)
    for a in (0, 180, 270):
        ra = math.radians(a); S._merge(sweep_tmp([(0.06 * math.cos(ra), 0.06 * math.sin(ra), -0.005), (0.23 * math.cos(ra), 0.23 * math.sin(ra), 0.0)], 0.014), 0, Mw)
    for k, (bx, by) in enumerate(((0.10, 0.03), (0.10, -0.01), (-0.10, 0.03), (-0.10, -0.01))):
        S.box((bx - 0.012, by - 0.009, 0.004), (bx + 0.012, by + 0.009, 0.012), 1 if k % 2 else 2, 0.002, M=Mw)
    S.cyl((0, 0, 0.018), (0, 0, 0.024), 0.035, 1, 24, 0.002, M=Mw)
    PARTS['SteeringWheel'] = S.done(sc, cW, nW.to_track_quat('Z', 'X').to_matrix()); INFO.append('SteeringWheel centre %.3f %.3f %.3f, turns about its local z (column axis %s)' % (*cW, tuple(round(q, 3) for q in nW)))
    for s_, nm in ((1, 'L'), (-1, 'R')): IKN['Hand_Wheel_' + nm] = tuple(cW + Mw.to_3x3() @ Vector((0, s_ * 0.24, 0)))
    # pedals hanging from the dash
    for nm, yy, w in (('Brake', yd + 0.08, 0.09), ('Accel', yd - 0.13, 0.06)):
        Pd = Part('Pedal_' + nm, [blk, chrome]); top = (3.40, yy, 1.86); pad = (3.33, yy, 1.62)
        Pd.cyl(top, pad, 0.012, 1, 10); Pd.box((pad[0] - 0.012, yy - w / 2, pad[2] - 0.09), (pad[0] + 0.012, yy + w / 2, pad[2] + 0.04), 0, 0.006)
        PARTS['Pedal_' + nm] = Pd.done(sc, top); IKN['Pedal_' + nm] = (pad[0] - 0.015, yy, pad[2] - 0.02); INFO.append('Pedal_%s pivot %.3f %.3f %.3f about y' % (nm, *top))
    # seats (driver +y, passenger -y): suspended base, cushion with bolsters, raked back with bolsters, headrest, armrest
    for yc, nm in ((yd, 'Driver'), (-yd, 'Passenger')):
        C.box((2.32, yc - 0.20, zf - 0.005), (2.68, yc + 0.20, zf + 0.30), 3, 0.02)
        C.box((2.20, yc - 0.27, zf + 0.30), (2.76, yc + 0.27, zf + 0.42), 2, 0.04)
        for y0_, y1_ in ((0.20, 0.27), (-0.27, -0.20)): C.box((2.24, yc + y0_, zf + 0.36), (2.74, yc + y1_, zf + 0.50), 2, 0.03)
        Mb = Matrix.Translation((2.26, yc, zf + 0.42)) @ Matrix.Rotation(math.radians(-14), 4, 'Y')
        C.box((-0.14, -0.27, 0.0), (0.0, 0.27, 0.80), 2, 0.04, M=Mb)
        for y0_, y1_ in ((0.20, 0.27), (-0.27, -0.20)): C.box((-0.12, y0_, 0.05), (0.06, y1_, 0.62), 2, 0.035, M=Mb)
        C.box((-0.12, -0.15, 0.84), (0.0, 0.15, 1.04), 2, 0.04, M=Mb); for_ = [C.cyl((-0.06, yy, 0.79), (-0.06, yy, 0.85), 0.008, 4, 8, 0.0, M=Mb) for yy in (-0.08, 0.08)]
        C.box((2.30, yc - 0.33 if yc > 0 else yc + 0.27, zf + 0.58), (2.62, yc - 0.27 if yc > 0 else yc + 0.33, zf + 0.65), 3, 0.02)
        IKN['Seat_' + nm] = (2.47, yc, zf + 0.42)
        C.box((2.80, yc - 0.33, zf - 0.004), (3.32, yc + 0.33, zf + 0.008), 5, 0.004)        # floor mat with ribs
        for k in range(7): C.box((2.84 + k * 0.065, yc - 0.30, zf + 0.008), (2.86 + k * 0.065, yc + 0.30, zf + 0.013), 5, 0.002)
    # console between the seats with gear selector; park brake knob on the dash
    C.box((2.62, 0.17, zf - 0.005), (3.30, 0.33, zf + 0.33), 1, 0.03)
    Gl = Part('GearLever', [blk, chrome]); gb = (3.05, 0.25, zf + 0.33)
    Gl.cyl(gb, (3.03, 0.25, zf + 0.50), 0.010, 1, 12); Gl.ball((3.03, 0.25, zf + 0.53), 0.032, 0, 16, sz=1.3)
    PARTS['GearLever'] = Gl.done(sc, gb); IKN['GearLever'] = (3.03, 0.25, zf + 0.54); INFO.append('GearLever pivot %.3f %.3f %.3f about y' % gb)
    C.box((3.02, 0.20, zf + 0.33), (3.08, 0.30, zf + 0.338), 3, 0.003)
    Pb = Part('ParkBrake', [knob, blk]); pb = (3.26, 0.14, 1.93)
    Pb.cyl(pb, (3.22, 0.14, 1.93), 0.008, 1, 10); Pb.box((3.19, 0.12, 1.915), (3.22, 0.16, 1.95), 0, 0.008)
    PARTS['ParkBrake'] = Pb.done(sc, pb); IKN['ParkBrake'] = (3.20, 0.14, 1.933); INFO.append('ParkBrake pivot %.3f %.3f %.3f, pulls along -x' % pb)
    # bunk behind the seats (storage base + mattress), back wall trim panel, headliner console, sun visors
    C.box((XB + WALL - 0.01, -1.27, zf - 0.005), (1.72, 1.27, zf + 0.46), 0, 0.03)
    C.box((XB + WALL + 0.01, -1.25, zf + 0.46), (1.70, 1.25, zf + 0.60), 7, 0.05)
    for yy in (-0.6, 0.6): C.box((1.715, yy - 0.25, zf + 0.12), (1.725, yy + 0.25, zf + 0.30), 3, 0.004)   # locker lids
    C.box((3.20, -1.0, 3.00), (3.50, 1.0, 3.14), 10, 0.04)                             # overhead shelf front edge
    for s, side in ((1, 'L'), (-1, 'R')):
        V_ = Part('SunVisor_' + side, [head, blk]); hz = 2.93
        V_.box((3.36, s * 0.18 if s > 0 else -0.98, hz - 0.02), (3.60, 0.98 if s > 0 else -0.18, hz), 0, 0.008)
        V_.cyl((3.60, s * 0.20, hz - 0.01), (3.60, s * 0.96, hz - 0.01), 0.008, 1, 10)
        PARTS['SunVisor_' + side] = V_.done(sc, (3.60, s * 0.58, hz - 0.01)); INFO.append('SunVisor_%s hinge (y axis) 3.600 %.3f %.3f, folds down' % (side, s * 0.58, hz - 0.01))
    IKN['Bunk'] = (1.30, 0.0, zf + 0.60)
    PARTS['CabInterior'] = C.done(sc)
    for o in (O, I, *cut): bpy.data.objects.remove(o, do_unlink=True)
    # sidewall detail on every tyre is in lorry_details.tyre (rim protector, two raised rings)
    IKS = []
    for k, p in IKN.items():
        e = bpy.data.objects.new('IK_' + k, None); e.empty_display_type = 'ARROWS'; e.empty_display_size = 0.08
        e.location = Vector(p); sc.collection.objects.link(e); IKS.append(e); INFO.append('IK_%s %.3f %.3f %.3f' % (k, *p))
    return INFO, IKS


def sweep_tmp(path, r, n=12):
    """round bar along a 3D polyline (handles, spokes) as a bmesh"""
    t = bmesh.new(); rings = []; path = [Vector(p) for p in path]
    for i, p in enumerate(path):
        tg = (path[min(i + 1, len(path) - 1)] - path[max(i - 1, 0)]).normalized()
        a = tg.cross(Vector((0, 0, 1)) if abs(tg.z) < 0.9 else Vector((1, 0, 0))).normalized(); b = tg.cross(a)
        rings.append([t.verts.new(p + (a * math.cos(2 * math.pi * k / n) + b * math.sin(2 * math.pi * k / n)) * r) for k in range(n)])
    for r0, r1 in zip(rings, rings[1:]):
        for i in range(n):
            j = (i + 1) % n; t.faces.new((r0[i], r0[j], r1[j], r1[i]))
    t.faces.new(rings[0][::-1]); t.faces.new(rings[-1]); bmesh.ops.recalc_face_normals(t, faces=t.faces); return t
