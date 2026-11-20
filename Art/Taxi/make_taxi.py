# Builds the airport taxi (a modern four-door saloon) in Blender from the owner's blueprint sheet, 2026-11-19.
# Sizes: 4.70 m long, 1.80 m wide (2.00 m over the mirrors), 1.45 m high (1.61 m with the roof sign), 2.85 m between
# the wheels, 225/45 R17 tyres. Units: metres, the car's front points to +X (Unreal's forward), centred on the origin,
# the ground at Z 0.
# Run: python3 make_taxi.py   (Blender as a Python module, bpy 5.3)  -> Taxi.blend and Taxi.glb next to this file.

# #region ── Setup ──
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector, Matrix, geometry
from shapely.geometry import Polygon, Point
from mathutils.bvhtree import BVHTree
from scipy.interpolate import PchipInterpolator

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_BLEND = os.path.join(HERE, 'Taxi.blend')
OUT_GLB = os.path.join(HERE, 'Taxi.glb')

bpy.ops.wm.read_factory_settings(use_empty=True)
SC = bpy.context.scene
SC.unit_settings.system = 'METRIC'
COL = bpy.data.collections.new('Taxi'); SC.collection.children.link(COL)
HELP = bpy.data.collections.new('Helpers'); SC.collection.children.link(HELP)

L = 4.70
XF = L / 2
AX_F, AX_R = 0.83, 3.68          # axle positions from the nose (2.85 m apart)
WR, WY = 0.333, 0.775            # wheel radius, wheel centre distance from the middle


def X(u):                        # distance from the nose -> Blender X
    return XF - u


def T(pairs):
    xs = np.array([p[0] for p in pairs], float); ys = np.array([p[1] for p in pairs], float)
    f = PchipInterpolator(xs, ys)
    return lambda u: float(f(np.clip(u, xs[0], xs[-1])))
# #endregion


# #region ── Materials ──
def mat(name, rgb, metal=0.0, rough=0.4, alpha=1.0, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get('Principled BSDF')
    b.inputs['Base Color'].default_value = (*rgb, 1)
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    if alpha < 1:
        b.inputs['Alpha'].default_value = alpha
        try: m.surface_render_method = 'BLENDED'
        except Exception: pass
    if emit:
        b.inputs['Emission Color'].default_value = (*rgb, 1)
        b.inputs['Emission Strength'].default_value = emit
    m.diffuse_color = (*rgb, alpha)
    return m

M = dict(
    Paint=mat('M_Taxi_Paint', (0.075, 0.16, 0.30), 0.3, 0.32),
    Glass=mat('M_Taxi_Glass', (0.10, 0.20, 0.32), 0.0, 0.05, alpha=0.55),
    Trim=mat('M_Taxi_TrimBlack', (0.012, 0.018, 0.03), 0.0, 0.5),
    Chrome=mat('M_Taxi_Chrome', (0.70, 0.75, 0.80), 1.0, 0.15),
    Under=mat('M_Taxi_Underbody', (0.01, 0.012, 0.018), 0.0, 0.8),
    Grille=mat('M_Taxi_GrilleBlack', (0.02, 0.03, 0.045), 0.2, 0.4),
    Head=mat('M_Taxi_HeadlightGlass', (0.55, 0.62, 0.70), 0.6, 0.12),
    Lens=mat('M_Taxi_Lens', (0.80, 0.86, 0.92), 0.2, 0.05, emit=0.4),
    Tail=mat('M_Taxi_Taillight', (0.42, 0.03, 0.04), 0.1, 0.15),
    TailLED=mat('M_Taxi_TaillightLED', (0.9, 0.08, 0.08), 0.0, 0.2, emit=1.5),
    Plate=mat('M_Taxi_Plate', (0.92, 0.92, 0.88), 0.0, 0.4),
    Ink=mat('M_Taxi_PlateText', (0.02, 0.02, 0.03), 0.0, 0.5),
    Tyre=mat('M_Taxi_Tyre', (0.025, 0.025, 0.028), 0.0, 0.85),
    Rim=mat('M_Taxi_Rim', (0.60, 0.64, 0.68), 1.0, 0.25),
    Brake=mat('M_Taxi_Brake', (0.30, 0.31, 0.33), 1.0, 0.45),
    Sign=mat('M_Taxi_RoofSign', (0.96, 0.78, 0.15), 0.0, 0.35),
    Seat=mat('M_Taxi_Seat', (0.05, 0.05, 0.06), 0.0, 0.7),
    Dash=mat('M_Taxi_Dash', (0.03, 0.035, 0.045), 0.0, 0.6),
)
# #endregion


# #region ── Body outline tables (measured off the owner's side, front, rear and top views) ──
zb = T([(0, 0.30), (0.15, 0.22), (0.5, 0.18), (4.3, 0.18), (4.6, 0.24), (4.7, 0.32)])             # underside
ws = T([(0, 0.60), (0.05, 0.72), (0.15, 0.82), (0.35, 0.875), (0.7, 0.895), (1.0, 0.90), (3.8, 0.90),
        (4.3, 0.89), (4.55, 0.86), (4.65, 0.80), (4.70, 0.70)])                                    # half width
zlow = T([(0, 0.42), (0.2, 0.46), (1.36, 0.51), (3.35, 0.60), (4.5, 0.58), (4.7, 0.55)])           # lower crease
zsh = T([(0, 0.52), (0.10, 0.70), (0.3, 0.76), (1.0, 0.80), (2.5, 0.86), (4.0, 0.92), (4.5, 0.94),
         (4.65, 0.92), (4.7, 0.80)])                                                               # shoulder line
ztop = T([(0, 0.60), (0.04, 0.70), (0.10, 0.79), (0.2, 0.84), (0.5, 0.90), (1.0, 0.97), (1.33, 1.03),
          (1.5, 1.12), (1.7, 1.24), (1.9, 1.35), (2.05, 1.41), (2.25, 1.435), (2.53, 1.45), (2.9, 1.445),
          (3.2, 1.42), (3.35, 1.385), (3.55, 1.32), (3.8, 1.24), (4.0, 1.18), (4.12, 1.14), (4.3, 1.125),
          (4.5, 1.115), (4.6, 1.11), (4.66, 1.07), (4.70, 0.96)])                                  # centre line
z5 = T([(0, 0.55), (0.10, 0.75), (0.3, 0.81), (1.0, 0.90), (1.33, 1.00), (1.5, 1.015), (2.0, 1.03),
        (2.6, 1.045), (3.3, 1.07), (3.62, 1.085), (4.12, 1.095), (4.5, 1.08), (4.66, 1.02), (4.70, 0.90)])  # belt
w5 = T([(0, 0.56), (0.1, 0.70), (0.3, 0.80), (1.0, 0.86), (1.33, 0.86), (2.0, 0.85), (3.62, 0.85),
        (4.12, 0.85), (4.5, 0.83), (4.66, 0.76), (4.70, 0.68)])
w7 = T([(0, 0.50), (0.1, 0.64), (0.3, 0.74), (1.0, 0.80), (1.33, 0.78), (1.7, 0.70), (2.05, 0.62),
        (2.5, 0.60), (3.2, 0.60), (3.6, 0.66), (4.12, 0.75), (4.4, 0.77), (4.66, 0.70), (4.70, 0.62)])  # top edge
dz7 = T([(0, 0.02), (1.0, 0.03), (1.33, 0.02), (2.05, 0.025), (2.4, 0.035), (3.2, 0.03), (3.5, 0.02),
         (4.12, 0.02), (4.7, 0.02)])
Z6T = T([(1.42, 1.013), (1.6, 1.095), (1.8, 1.195), (2.0, 1.30), (2.15, 1.355), (2.3, 1.38), (2.5, 1.395), (3.0, 1.39),
         (3.2, 1.37), (3.35, 1.33), (3.5, 1.20), (3.62, 1.091)])                                   # window top


def z7f(u): return ztop(u) - dz7(u)


def z6f(u):
    a = z5(u) + 0.006
    v = a if (u <= 1.42 or u >= 3.62) else max(a, Z6T(u))
    return max(a, min(v, z7f(u) - 0.012))


def section(u):
    """Half cross-section, bottom middle to top middle: 10 (half width, height) points."""
    s = ws(u); b = zb(u); Z5 = z5(u); Z6 = z6f(u); W5 = w5(u)
    W6 = W5 - (Z6 - Z5) * 0.62
    W7 = min(w7(u), W6 - 0.01)
    return [(0.0, b), (s - 0.10, b), (s - 0.025, b + 0.07), (s, zlow(u)), (s - 0.012, zsh(u)),
            (W5, Z5), (W6, Z6), (W7, z7f(u)), (W7 * 0.5, ztop(u) - 0.006), (0.0, ztop(u))]


STATIONS = [0, 0.02, 0.05, 0.10, 0.16, 0.24, 0.34, 0.46, 0.60, 0.75, 0.90, 1.05, 1.20, 1.33, 1.42, 1.52, 1.62,
            1.72, 1.82, 1.93, 2.05, 2.18, 2.32, 2.40, 2.48, 2.62, 2.78, 2.95, 3.10, 3.20, 3.27, 3.33, 3.42, 3.52, 3.62,
            3.72, 3.84, 3.97, 4.12, 4.25, 4.38, 4.50, 4.58, 4.64, 4.68, 4.70]
WINDOWS = [(1.62, 2.48), (2.62, 3.27), (3.33, 3.62)]
PILLARS = [(1.42, 1.62), (2.48, 2.62), (3.27, 3.33)]
A_PILLAR = (1.33, 2.32)
WINDSCREEN, BACKLIGHT = (1.33, 2.05), (3.20, 4.12)
# #endregion


# #region ── Body shell (lofted cage, smoothed with Subdivision Surface) ──
def build_body():
    bm = bmesh.new()
    loops = []
    for u in STATIONS:
        sec = section(u)
        idx = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3, 2, 1]
        sgn = [1] * 10 + [-1] * 8
        loops.append([bm.verts.new((X(u), sgn[k] * sec[idx[k]][0], sec[idx[k]][1])) for k in range(18)])
    bm.verts.ensure_lookup_table()
    row = lambda k: k if k <= 9 else 18 - k
    order = ['Paint', 'Glass', 'Trim', 'Under']
    for i in range(len(STATIONS) - 1):
        um = (STATIONS[i] + STATIONS[i + 1]) / 2
        gap = z6f(um) - z5(um)
        for k in range(18):
            k2 = (k + 1) % 18
            f = bm.faces.new((loops[i][k], loops[i][k2], loops[i + 1][k2], loops[i + 1][k]))
            lo = min(row(k), row(k2))
            m = 'Paint'
            if lo == 0: m = 'Under'
            elif lo == 5 and gap > 0.02:
                if any(a <= um <= b for a, b in WINDOWS): m = 'Glass'
                elif any(a <= um <= b for a, b in PILLARS): m = 'Trim'
            elif lo == 6 and A_PILLAR[0] <= um <= A_PILLAR[1]:
                m = 'Trim'
            elif lo in (7, 8) and (WINDSCREEN[0] <= um <= WINDSCREEN[1] or BACKLIGHT[0] <= um <= BACKLIGHT[1]):
                m = 'Glass'
            f.material_index = order.index(m)
    bm.faces.new(loops[0]); bm.faces.new(list(reversed(loops[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    # creases: the shoulder and lower character lines, and crisp front and rear faces
    cr = bm.edges.layers.float.get('crease_edge') or bm.edges.layers.float.new('crease_edge')
    def crease(k, u0, u1, v):
        for i in range(len(STATIONS) - 1):
            if u0 <= STATIONS[i] and STATIONS[i + 1] <= u1:
                for kk in (k, 18 - k):
                    e = bm.edges.get((loops[i][kk], loops[i + 1][kk]))
                    if e: e[cr] = v
    crease(4, 1.0, 4.25, 0.55)
    crease(3, 1.33, 3.42, 0.35)
    for lp, v in ((loops[0], 0.25), (loops[-1], 0.55)):
        for k in range(18):
            e = bm.edges.get((lp[k], lp[(k + 1) % 18]))
            if e: e[cr] = v
    me = bpy.data.meshes.new('SM_Taxi_Body')
    bm.to_mesh(me); bm.free()
    for n in order: me.materials.append(M[n])
    me.shade_smooth()
    ob = bpy.data.objects.new('Body', me); COL.objects.link(ob)
    sub = ob.modifiers.new('Smooth', 'SUBSURF'); sub.levels = 3; sub.render_levels = 3
    # wheel arches cut out, with wheelhouse walls left by the cut
    cut = cylinders('ArchCutters', [((X(a), s * 0.95, 0.355), 0.39, 0.74) for a in (AX_F, AX_R) for s in (1, -1)])
    cut.data.materials.append(M['Under'])
    b = ob.modifiers.new('WheelArches', 'BOOLEAN'); b.operation = 'DIFFERENCE'; b.solver = 'EXACT'
    b.object = cut; b.material_mode = 'TRANSFER'
    cut.hide_render = True; cut.hide_viewport = True
    return ob


def cylinders(name, specs, seg=64):
    """Closed cylinders along Y: (centre, radius, length)."""
    bm = bmesh.new()
    for (cx, cy, cz), r, ln in specs:
        ring = []
        for side in (-1, 1):
            ring.append([bm.verts.new((cx + r * math.cos(2 * math.pi * i / seg), cy + side * ln / 2,
                                       cz + r * math.sin(2 * math.pi * i / seg))) for i in range(seg)])
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new((ring[0][i], ring[0][j], ring[1][j], ring[1][i]))
        bm.faces.new(ring[0]); bm.faces.new(list(reversed(ring[1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); HELP.objects.link(ob)
    return ob
# #endregion


# #region ── Helpers: evaluated meshes, cutting prisms, conforming patches ──
def evaluated(ob):
    dg = bpy.context.evaluated_depsgraph_get(); dg.update()
    return bpy.data.meshes.new_from_object(ob.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)


def p3(axis, a, b, d):
    return {'X': (d, a, b), 'Y': (a, d, b), 'Z': (a, b, d)}[axis]


def prism(polys):
    """One object of closed prisms. polys: [(points [(a, b)], axis, lo, hi)]; for axis Y, a is the distance
    from the nose (u), for X and Z, a/b are the across/up or along/across coordinates."""
    bm = bmesh.new()
    for pts, axis, lo, hi in polys:
        if axis in ('Y', 'Z'): pts = [(X(a), b) for a, b in pts] if axis == 'Y' else [(X(a), b) for a, b in pts]
        n = len(pts)
        v0 = [bm.verts.new(p3(axis, a, b, lo)) for a, b in pts]
        v1 = [bm.verts.new(p3(axis, a, b, hi)) for a, b in pts]
        bm.faces.new(v0); bm.faces.new(list(reversed(v1)))
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((v0[i], v0[j], v1[j], v1[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new('Prism'); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new('Prism', me); HELP.objects.link(ob)
    return ob


def ribbon(line, w):
    """A thin strip round a polyline [(a, b)] -> polygon points."""
    p = [Vector((a, b)) for a, b in line]
    left, right = [], []
    for i, q in enumerate(p):
        t = (p[min(i + 1, len(p) - 1)] - p[max(i - 1, 0)]).normalized()
        nrm = Vector((-t.y, t.x)) * (w / 2)
        left.append(tuple(q + nrm)); right.append(tuple(q - nrm))
    return left + right[::-1]


def circle(ca, cb, r, n=28):
    return [(ca + r * math.cos(2 * math.pi * i / n), cb + r * math.sin(2 * math.pi * i / n)) for i in range(n)]


def mirror_a(pts):                       # mirror across the middle (front/rear outlines)
    return [(-a, b) for a, b in reversed(pts)]


FRONT = lambda u0=0.7: (X(u0), X(-0.3))          # x range for prisms seen from the front
REAR = lambda u0=4.0: (X(5.0), X(u0))            # x range for prisms seen from the rear
SIDE_P, SIDE_N = (0.30, 1.40), (-1.40, -0.30)    # y ranges for prisms seen from the side
TOP = (0.90, 1.60)                               # z range for prisms seen from above
BODY_EVAL = None
PARTS = {}


AXI = {'X': 0, 'Y': 1, 'Z': 2}
OTHER = {0: (1, 2), 1: (0, 2), 2: (0, 1)}
BODY_TRIS = None


def body_tris():
    """The smoothed body as triangles: positions, smooth normals, triangle corner indices, plane normals."""
    me = BODY_EVAL
    me.calc_loop_triangles()
    P = np.array([v.co[:] for v in me.vertices]); N = np.array([v.normal[:] for v in me.vertices])
    tris = np.array([t.vertices[:] for t in me.loop_triangles])
    fn = np.cross(P[tris[:, 1]] - P[tris[:, 0]], P[tris[:, 2]] - P[tris[:, 0]])
    fn /= np.maximum(np.linalg.norm(fn, axis=1)[:, None], 1e-12)
    return P, N, tris, fn


def clip_pieces(pieces, group):
    """pieces: [(tri id, [3D points])] lying on body triangles. Keep the parts inside any prism of the group."""
    P, N, tris, fn = BODY_TRIS
    preps = []
    for pts, axis, lo, hi in group:
        if axis in ('Y', 'Z'): pts = [(X(a), b) for a, b in pts]
        poly = Polygon(pts)
        if not poly.is_valid: poly = poly.buffer(0)
        preps.append((AXI[axis], poly, min(lo, hi), max(lo, hi)))
    out = []
    for tid, pts in pieces:
        n = fn[tid]; c = float(np.dot(n, P[tris[tid][0]]))
        for k, poly, lo, hi in preps:
            i, j = OTHER[k]
            cen = np.mean(pts, axis=0)
            if not (lo <= cen[k] <= hi): continue
            if abs(n[k]) < 1e-4: continue
            p2 = Polygon([(p[i], p[j]) for p in pts])
            if p2.area < 1e-10: continue
            if not p2.intersects(poly): continue
            inter = p2.intersection(poly)
            geoms = getattr(inter, 'geoms', [inter])
            for g in geoms:
                if g.geom_type != 'Polygon' or g.area < 1e-10: continue
                ring = list(g.exterior.coords)[:-1]
                q = []
                for a2, b2 in ring:
                    v = [0.0, 0.0, 0.0]; v[i] = a2; v[j] = b2
                    v[k] = (c - n[i] * a2 - n[j] * b2) / n[k]
                    q.append(v)
                out.append((tid, np.array(q)))
    return out


def patch(name, mname, t0, t1, cutter_groups):
    """A part lying on the body surface between t0 and t1 metres out from it, trimmed to the outlines.
    cutter_groups: list of prism lists; the part is the body skin inside every group."""
    P, N, tris, fn = BODY_TRIS
    # rough pre-selection by the first group's outline box
    keep = np.zeros(len(tris), bool)
    for pts, axis, lo, hi in cutter_groups[0]:
        k = AXI[axis]; i, j = OTHER[k]
        pa = [X(a) if axis in ('Y', 'Z') else a for a, b in pts]; pb = [b for a, b in pts]
        cen = P[tris].mean(axis=1)
        mn = P[tris].min(axis=1); mx = P[tris].max(axis=1)
        keep |= ((mx[:, i] >= min(pa)) & (mn[:, i] <= max(pa)) & (mx[:, j] >= min(pb)) & (mn[:, j] <= max(pb))
                 & (cen[:, k] >= min(lo, hi)) & (cen[:, k] <= max(lo, hi)))
    pieces = [(int(t), P[tris[t]]) for t in np.nonzero(keep)[0]]
    for g in cutter_groups:
        pieces = clip_pieces(pieces, g)
    bm = bmesh.new()
    for tid, pts in pieces:
        a, b, c = (Vector(P[x]) for x in tris[tid]); na, nb, nc = (Vector(N[x]) for x in tris[tid])
        vs = []
        for p in pts:
            p = Vector(p)
            nrm = geometry.barycentric_transform(p, a, b, c, na, nb, nc).normalized()
            vs.append(bm.verts.new(p + nrm * t1))
        if len(vs) >= 3:
            try: bm.faces.new(vs)
            except ValueError: pass
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=2e-5)
    me = bpy.data.meshes.new('SM_Taxi_' + name); bm.to_mesh(me); bm.free()
    me.materials.append(M[mname])
    if name in PARTS:
        old = PARTS[name]
        bm = bmesh.new(); bm.from_mesh(old.data); bm.from_mesh(me); bm.to_mesh(old.data); bm.free()
        bpy.data.meshes.remove(me)
        return old
    o = bpy.data.objects.new(name, me); COL.objects.link(o); PARTS[name] = o
    sd = o.modifiers.new('Thickness', 'SOLIDIFY'); sd.thickness = max(t1 - t0, 0.002); sd.offset = -1.0
    sd.use_even_offset = True
    print('  part', name, len(me.polygons), flush=True)
    return o


def mirrored_side(pts):
    return [(pts, 'Y', *SIDE_P), (pts, 'Y', *SIDE_N)]


def mirrored_front(pts, rng):
    return [(pts, 'X', *rng), (mirror_a(pts), 'X', *rng)]
# #endregion


# #region ── Small solid parts (rounded boxes, revolved parts, text) ──
def rbox(name, centre, size, mname, bevel=0.02, rot=(0, 0, 0), parent=None):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    me = bpy.data.meshes.new('SM_Taxi_' + name); bm.to_mesh(me); bm.free()
    me.materials.append(M[mname]); me.shade_smooth()
    ob = bpy.data.objects.new(name, me); COL.objects.link(ob)
    ob.location = centre; ob.rotation_euler = rot
    if bevel:
        b = ob.modifiers.new('Round', 'BEVEL'); b.width = bevel; b.segments = 4; b.limit_method = 'NONE'
    return ob


def lathe(name, prof, mname, seg=64):
    """Revolve [(radius, y)] round the Y axis."""
    bm = bmesh.new(); rings = []
    for r, y in prof:
        rings.append([bm.verts.new((r * math.cos(2 * math.pi * i / seg), y, r * math.sin(2 * math.pi * i / seg)))
                      for i in range(seg)])
    for a in range(len(rings) - 1):
        for i in range(seg):
            j = (i + 1) % seg
            bm.faces.new((rings[a][i], rings[a][j], rings[a + 1][j], rings[a + 1][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new('SM_Taxi_' + name); bm.to_mesh(me); bm.free()
    me.materials.append(M[mname]); me.shade_smooth()
    ob = bpy.data.objects.new(name, me); COL.objects.link(ob)
    return ob


def text(name, body, size, loc, facing, mname='Ink'):
    cu = bpy.data.curves.new(name, 'FONT'); cu.body = body; cu.size = size
    cu.align_x = 'CENTER'; cu.align_y = 'CENTER'; cu.extrude = 0.0015
    ob = bpy.data.objects.new(name, cu); COL.objects.link(ob)
    ob.location = loc
    ob.rotation_euler = {'+X': (math.pi / 2, 0, math.pi / 2), '-X': (math.pi / 2, 0, -math.pi / 2)}[facing]
    cu.materials.append(M[mname])
    me = evaluated(ob); bpy.data.objects.remove(ob); bpy.data.curves.remove(cu)
    o = bpy.data.objects.new(name, me); COL.objects.link(o)
    o.location = loc; o.rotation_euler = {'+X': (math.pi / 2, 0, math.pi / 2), '-X': (math.pi / 2, 0, -math.pi / 2)}[facing]
    return o
# #endregion


# #region ── Build: body ──
print('body', flush=True)
BODY = build_body()
BODY_EVAL = evaluated(BODY)
BODY_TRIS = body_tris()
BVH = BVHTree.FromPolygons([v.co.copy() for v in BODY_EVAL.vertices], [list(p.vertices) for p in BODY_EVAL.polygons])


def hit(origin, direction):
    loc, nrm, i, d = BVH.ray_cast(Vector(origin), Vector(direction).normalized())
    return loc, nrm
# #endregion


# #region ── Build: front (headlights, grille, plate, intakes, fog lights) ──
print('front', flush=True)
HL_F = [(0.95, 0.85), (0.47, 0.775), (0.455, 0.70), (0.47, 0.605), (0.95, 0.645)]
HL_S = [(-0.3, 0.58), (0.10, 0.615), (0.47, 0.73), (0.54, 0.80), (0.50, 0.835), (0.14, 0.835), (-0.3, 0.84)]
patch('Headlights', 'Head', 0.002, 0.008, [mirrored_front(HL_F, FRONT(0.7)), mirrored_side(HL_S)])
for yc in (0.60, 0.76):
    patch('HeadlightRings', 'Chrome', 0.006, 0.013, [mirrored_front(circle(yc, 0.715, 0.047), FRONT(0.45))])
    patch('HeadlightLenses', 'Lens', 0.010, 0.016, [mirrored_front(circle(yc, 0.715, 0.032), FRONT(0.45))])
patch('DaytimeLights', 'Lens', 0.007, 0.012, [mirrored_front(ribbon([(0.48, 0.627), (0.70, 0.64), (0.93, 0.66)], 0.014), FRONT(0.7)),
                                            mirrored_side(HL_S)])
GR = [(0.40, 0.765), (0.43, 0.70), (0.40, 0.535), (0.33, 0.515), (-0.33, 0.515), (-0.40, 0.535), (-0.43, 0.70), (-0.40, 0.765)]
GR_IN = [(0.383, 0.745), (0.41, 0.70), (0.383, 0.552), (0.322, 0.535), (-0.322, 0.535), (-0.383, 0.552), (-0.41, 0.70), (-0.383, 0.745)]
patch('GrilleSurround', 'Chrome', 0.002, 0.009, [[(GR, 'X', *FRONT(0.3))]])
patch('Grille', 'Grille', 0.001, 0.005, [[(GR_IN, 'X', *FRONT(0.3))]])
for i in range(6):
    z = 0.566 + i * 0.032
    patch('GrilleBars', 'Chrome', 0.003, 0.010, [[([(0.42, z), (-0.42, z), (-0.42, z + 0.014), (0.42, z + 0.014)], 'X', *FRONT(0.3))],
                                               [(GR_IN, 'X', *FRONT(0.3))]])
patch('FrontPlate', 'Plate', 0.010, 0.022, [[([(0.25, 0.37), (0.25, 0.48), (-0.25, 0.48), (-0.25, 0.37)], 'X', *FRONT(0.3))]])
patch('LowerIntake', 'Grille', 0.001, 0.005, [[([(0.50, 0.40), (0.56, 0.33), (0.52, 0.25), (-0.52, 0.25), (-0.56, 0.33), (-0.50, 0.40)], 'X', *FRONT(0.3))]])
patch('FogPods', 'Grille', 0.001, 0.006, [mirrored_front([(0.60, 0.42), (0.80, 0.44), (0.83, 0.30), (0.62, 0.265)], FRONT(0.3))])
patch('FogLights', 'Lens', 0.005, 0.011, [mirrored_front(circle(0.715, 0.355, 0.042), FRONT(0.3))])
patch('Gaps', 'Trim', 0.0005, 0.003, [mirrored_front(ribbon([(0.95, 0.60), (0.47, 0.588)], 0.006), FRONT(0.7))])
# #endregion


# #region ── Build: sides (door gaps, chrome window line, markers, handles, mirrors, fuel flap) ──
print('sides', flush=True)
ARC = [(AX_R + 0.47 * math.cos(math.radians(a)), 0.36 + 0.47 * math.sin(math.radians(a))) for a in range(100, 176, 8)]
for line in ([(1.40, 1.005), (1.385, 0.80), (1.37, 0.50), (1.37, 0.262)],
             [(2.535, 1.04), (2.52, 0.60), (2.505, 0.262)],
             [(3.585, 1.085), (3.585, 0.84)] + ARC + [(3.205, 0.262)],
             [(1.37, 0.262), (3.21, 0.262)],
             [(-0.25, 0.598), (0.40, 0.60)],
             [(4.08, 0.56), (4.80, 0.56)]):
    patch('Gaps', 'Trim', 0.0005, 0.003, [mirrored_side(ribbon(line, 0.006))])
BELT = [(u, z5(u) - 0.004) for u in np.linspace(1.42, 3.62, 45)]
patch('WindowChrome', 'Chrome', 0.001, 0.006, [mirrored_side(ribbon(BELT, 0.014))])
TOPL = [(u, z6f(u) + 0.007) for u in np.linspace(1.50, 3.60, 60)]
patch('WindowChrome', 'Chrome', 0.001, 0.005, [[(ribbon(TOPL, 0.010), 'Y', 0.58, 1.40), (ribbon(TOPL, 0.010), 'Y', -1.40, -0.58)]])
patch('FenderTrim', 'Chrome', 0.001, 0.006, [mirrored_side(ribbon([(1.06, 0.806), (1.30, 0.800)], 0.018))])
patch('SideMarkers', 'Lens', 0.001, 0.006, [mirrored_side(circle(1.27, 0.69, 0.022, 20))])
patch('FuelFlap', 'Paint', 0.0005, 0.0025, [[(ribbon([(3.73, 0.90), (3.89, 0.90), (3.895, 1.005), (3.725, 1.005), (3.73, 0.90)], 0.006), 'Y', *SIDE_P)]])
for u in (2.33, 3.32):
    for s in (1, -1):
        loc, n = hit((X(u), s * 2.0, 0.905), (0, -s, 0))
        rbox('DoorHandles', (loc.x, loc.y + s * 0.013, loc.z), (0.17, 0.028, 0.036), 'Chrome', bevel=0.012)
        patch('Gaps', 'Trim', 0.0005, 0.003, [[(ribbon([(u - 0.10, 0.884), (u + 0.10, 0.884)], 0.005), 'Y', *(SIDE_P if s > 0 else SIDE_N))]])
for s in (1, -1):
    rbox('Mirrors', (X(1.66), s * 1.00, 1.075), (0.12, 0.21, 0.125), 'Paint', bevel=0.045)
    rbox('Mirrors', (X(1.70), s * 1.00, 1.075), (0.06, 0.19, 0.105), 'Trim', bevel=0.03)
    rbox('MirrorArms', (X(1.60), s * 0.90, 1.035), (0.07, 0.10, 0.03), 'Trim', bevel=0.01)
    rbox('MirrorIndicators', (X(1.62), s * 1.03, 1.03), (0.05, 0.12, 0.012), 'Lens', bevel=0.005)
# #endregion


# #region ── Build: rear (tail lights, boot lid, plate, bumper, diffuser, exhausts, reflectors) ──
print('rear', flush=True)
TL_R = [(0.98, 0.97), (0.36, 0.935), (0.355, 0.80), (0.98, 0.775)]
TL_S = [(4.19, 0.955), (4.56, 1.005), (4.95, 1.01), (4.95, 0.815), (4.38, 0.83)]
TLM = [(-a, b) for a, b in TL_R]          # tail lights are given as the car's right side when seen from behind
TL_S2 = [(4.19, 0.955), (4.56, 1.005), (4.61, 1.007), (4.61, 0.818), (4.38, 0.83)]
patch('Taillights', 'Tail', 0.002, 0.009, [mirrored_front(TL_R, (X(5.0), X(4.61)))])     # the back face
patch('Taillights', 'Tail', 0.002, 0.009, [mirrored_side(TL_S2)])                        # the wrap round the side
patch('TaillightLEDs', 'TailLED', 0.008, 0.012, [mirrored_front(ribbon([(0.40, 0.88), (0.70, 0.888), (0.90, 0.90)], 0.012), (X(5.0), X(4.61)))])
patch('TaillightLEDs', 'TailLED', 0.008, 0.012, [mirrored_side(ribbon([(4.26, 0.94), (4.45, 0.955), (4.61, 0.96)], 0.012))])
for line in ([(0.72, 1.12), (0.70, 0.98)], [(0.70, 0.775), (0.68, 0.66), (-0.68, 0.66), (-0.70, 0.775)],
             [(-0.72, 1.12), (-0.70, 0.98)], [(0.98, 0.56), (-0.98, 0.56)]):
    patch('Gaps', 'Trim', 0.0005, 0.003, [[(ribbon(line, 0.006), 'X', *REAR(4.2))]])
for s in (1, -1):
    patch('Gaps', 'Trim', 0.0005, 0.003, [[(ribbon([(4.13, s * 0.72), (4.75, s * 0.72)], 0.006), 'Z', *TOP)]])
patch('RearPlate', 'Plate', 0.006, 0.016, [[([(0.24, 0.70), (0.24, 0.82), (-0.24, 0.82), (-0.24, 0.70)], 'X', *REAR(4.3))]])
patch('Diffuser', 'Grille', 0.001, 0.005, [[([(0.80, 0.40), (0.86, 0.30), (0.78, 0.21), (-0.78, 0.21), (-0.86, 0.30), (-0.80, 0.40)], 'X', *REAR(4.3))]])
EX = [(0.46, 0.35), (0.69, 0.35), (0.71, 0.262), (0.48, 0.262)]
EXI = [(0.478, 0.338), (0.675, 0.338), (0.69, 0.275), (0.495, 0.275)]
patch('Exhausts', 'Chrome', 0.004, 0.011, [mirrored_front(EX, REAR(4.3))])
patch('ExhaustInners', 'Under', 0.008, 0.014, [mirrored_front(EXI, REAR(4.3))])
patch('Reflectors', 'Tail', 0.004, 0.009, [mirrored_front([(0.64, 0.43), (0.80, 0.43), (0.80, 0.405), (0.64, 0.405)], REAR(4.3))])
for yy in (0.22, 0.52):
    patch('ParkingSensors', 'Paint', 0.0005, 0.0035, [mirrored_front(circle(yy, 0.47, 0.014, 16), REAR(4.3))])
    patch('ParkingSensors', 'Paint', 0.0005, 0.0035, [mirrored_front(circle(yy, 0.47, 0.014, 16), FRONT(0.3))])
patch('BrakeLight', 'TailLED', 0.001, 0.006, [[([(4.555, -0.22), (4.60, -0.22), (4.60, 0.22), (4.555, 0.22)], 'Z', *TOP)]])
# #endregion


# #region ── Build: plate lettering ──
def plate_x(z, front):
    part = PARTS['FrontPlate' if front else 'RearPlate']
    dg = bpy.context.evaluated_depsgraph_get(); dg.update()
    tree = BVHTree.FromObject(part, dg)
    loc, n, i, d = tree.ray_cast(Vector((X(-1.0) if front else X(5.7), 0.0, z)), Vector((-1, 0, 0) if front else (1, 0, 0)))
    return loc.x
xp = plate_x(0.425, True) + 0.001
PL = [text('PlateText', 'TAXI', 0.085, (xp, 0, 0.425), '+X')]
xr = plate_x(0.76, False) - 0.001
PL += [text('PlateText', 'TAXI', 0.060, (xr, 0, 0.78), '-X'), text('PlateText', '1024', 0.036, (xr, 0, 0.725), '-X')]
# #endregion


# #region ── Build: roof sign and aerial ──
print('roof', flush=True)
loc, n = hit((X(2.58), 0, 3.0), (0, 0, -1))
zr = loc.z
bm = bmesh.new()
pts = [(-0.11, -0.25), (0.11, -0.25), (0.11, 0.25), (-0.11, 0.25)]
bot = [bm.verts.new((X(2.58) + a, b, zr + 0.025)) for a, b in pts]
top = [bm.verts.new((X(2.58) + a * 0.55, b * 0.88, zr + 0.175)) for a, b in pts]
bm.faces.new(list(reversed(bot))); bm.faces.new(top)
for i in range(4):
    j = (i + 1) % 4; bm.faces.new((bot[i], bot[j], top[j], top[i]))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
me = bpy.data.meshes.new('SM_Taxi_RoofSign'); bm.to_mesh(me); bm.free(); me.materials.append(M['Sign']); me.shade_smooth()
sign = bpy.data.objects.new('RoofSign', me); COL.objects.link(sign)
bv = sign.modifiers.new('Round', 'BEVEL'); bv.width = 0.02; bv.segments = 4; bv.limit_method = 'NONE'
rbox('RoofSignBase', (X(2.58), 0, zr + 0.015), (0.26, 0.54, 0.03), 'Trim', bevel=0.01)
# sloped faces: lettering laid on the front and back slopes
tilt = math.atan2(0.11 * 0.45, 0.15)
for side, rz in ((1, math.pi / 2), (-1, -math.pi / 2)):
    o = text('RoofSignText', 'TAXI', 0.085, (X(2.58) + side * 0.088, 0, zr + 0.10), '+X' if side > 0 else '-X')
    o.rotation_euler = (math.pi / 2 - tilt, 0, rz)
za = hit((X(3.42), 0, 3.0), (0, 0, -1))[0].z
rbox('Aerial', (X(3.42), 0, za + 0.01), (0.10, 0.04, 0.025), 'Trim', bevel=0.008)
ant = rbox('AerialRod', (X(3.48), 0, za + 0.12), (0.006, 0.006, 0.26), 'Trim', bevel=0.002, rot=(0, -math.radians(28), 0))
# #endregion


# #region ── Build: wheels (225/45 R17 tyres, five twin-spoke rims, brake discs) ──
print('wheels', flush=True)
TYRE = [(0.216, -0.100), (0.26, -0.112), (0.30, -0.112), (0.322, -0.105), (0.331, -0.08), (0.333, 0.0),
        (0.331, 0.08), (0.322, 0.105), (0.30, 0.112), (0.26, 0.112), (0.216, 0.100)]
RIM = [(0.205, -0.10), (0.212, -0.10), (0.212, 0.085), (0.222, 0.095), (0.218, 0.104), (0.196, 0.100),
       (0.190, 0.080), (0.190, -0.09)]
HUB = [(0.0, 0.098), (0.045, 0.098), (0.062, 0.090), (0.075, 0.075), (0.075, 0.05), (0.0, 0.05)]


def wheel(name, u, s):
    parts = []
    t = lathe(name + '_Tyre', TYRE, 'Tyre'); parts.append(t)
    r = lathe(name + '_Rim', RIM, 'Rim'); parts.append(r)
    h = lathe(name + '_Hub', HUB, 'Rim'); parts.append(h)
    for k in range(5):
        for d in (-7.5, 7.5):
            a = math.radians(k * 72 + d + 90)
            sp = rbox(name + '_Spoke', (0.125 * math.cos(a), 0.083, 0.125 * math.sin(a)), (0.135, 0.022, 0.020), 'Rim',
                      bevel=0.006, rot=(0, -a, 0))
            parts.append(sp)
    disc = lathe(name + '_Disc', [(0.06, -0.02), (0.165, -0.02), (0.165, 0.006), (0.06, 0.006)], 'Brake'); parts.append(disc)
    cal = rbox(name + '_Caliper', (0.0, 0.0, 0.0), (0.09, 0.055, 0.06), 'Trim', bevel=0.012); parts.append(cal)
    cal.location = (-0.14 * math.cos(math.radians(30)), 0.0, 0.14 * math.sin(math.radians(30)))
    root = bpy.data.objects.new(name, None); COL.objects.link(root)
    root.location = (X(u), s * WY, WR)
    root.rotation_euler = (0, 0, 0 if s > 0 else math.pi)
    for p in parts: p.parent = root
    return root

WHEELS = [wheel(n, u, s) for n, u, s in (('Wheel_FL', AX_F, 1), ('Wheel_FR', AX_F, -1), ('Wheel_RL', AX_R, 1), ('Wheel_RR', AX_R, -1))]
# #endregion


# #region ── Build: cabin (floor, dashboard, steering wheel, seats for MetaHumans) ──
print('cabin', flush=True)
rbox('Floor', (X(2.55), 0, 0.27), (2.3, 1.56, 0.04), 'Dash', bevel=0.01)
rbox('Dashboard', (X(1.62), 0, 0.88), (0.42, 1.58, 0.22), 'Dash', bevel=0.06)
rbox('CentreConsole', (X(2.05), 0, 0.42), (0.75, 0.22, 0.24), 'Dash', bevel=0.04)
sw = lathe('SteeringWheel', [(0.17, -0.014), (0.185, 0.0), (0.17, 0.014), (0.155, 0.0), (0.17, -0.014)], 'Dash', 48)
sw.location = (X(1.95), 0.38, 0.98); sw.rotation_euler = (math.radians(24), 0, math.pi / 2)
for yy in (0.38, -0.38):
    rbox('FrontSeats', (X(2.36), yy, 0.47), (0.52, 0.52, 0.14), 'Seat', bevel=0.05)
    rbox('FrontSeats', (X(2.66), yy, 0.86), (0.13, 0.50, 0.62), 'Seat', bevel=0.05, rot=(0, math.radians(-14), 0))
    rbox('FrontSeats', (X(2.74), yy, 1.24), (0.10, 0.26, 0.17), 'Seat', bevel=0.04, rot=(0, math.radians(-14), 0))
rbox('RearSeat', (X(3.25), 0, 0.46), (0.52, 1.36, 0.15), 'Seat', bevel=0.05)
rbox('RearSeat', (X(3.56), 0, 0.82), (0.13, 1.36, 0.56), 'Seat', bevel=0.05, rot=(0, math.radians(-20), 0))
rbox('ParcelShelf', (X(3.88), 0, 1.10), (0.35, 1.40, 0.025), 'Dash', bevel=0.005)
# #endregion


# #region ── Save, and export glTF for Unreal ──
for o in HELP.objects: o.hide_render = True; o.hide_viewport = True
bpy.ops.wm.save_as_mainfile(filepath=OUT_BLEND)
bpy.ops.export_scene.gltf(filepath=OUT_GLB, export_format='GLB', use_selection=False, export_apply=True,
                          use_visible=True)
print('saved', OUT_BLEND, flush=True)
# #endregion
