# A320 opening parts, the CLEAN method (owner 2026-11-27, pilot of 3 parts + the main wheels; the owner approves the look
# before the rest is done). No more cutting the Meshy triangles into parts. Per part:
#  1. the moving part is a NEW mesh: a quad grid over its outline (rounded corners), every vertex shot onto his Meshy outer
#     surface (so closed it lies exactly on it), with a real thickness, rolled edges and an inner skin; his texture
#     (colour, ORM, normal) is baked from his surface onto it, so closed it looks like his model.
#  2. his body is cut ONCE along a smooth level-set curve (the outline + half the seam) and only the faces inside go: the
#     hole edge is a clean continuous line on his surface. Behind it: a frame (lip under his skin edge, wall going in) and
#     the bay (hold, engine, flap cove).
#  3. closed check: every outer vertex and face centre of the part against his original surface, in mm.
#  4. main wheels: clean tyres (his size and place: lathe fitted to his tyre), 4 grooves, size text, rims with bolts and hub
#     cap; carbon brake stack (on the leg, does not roll).
# Pilot parts: FanCowl_Eng1_Out, FlapInner_R, CargoDoorFwd. Not imported, Data/a320-panels.json not changed.
#   ADP_STAGE=before: only the BEFORE pictures of his model. Default: build, bake, check, save A320-clean.blend, AFTER pictures.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/A320/make_a320_clean.py
import bpy, bmesh, os, json, math
import numpy as np
from mathutils import Vector, Matrix
import mathutils.kdtree
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
PIC = os.path.join(ROOT, 'docs', 'Pictures', 'a320-panels')
STAGE = os.environ.get('ADP_STAGE', 'after')
ONLY = os.environ.get('ADP_ONLY', '')
_exp = bpy.ops.export_scene.gltf
# the gear script must not overwrite A320.glb here (assigning bpy.ops.export_scene.gltf does not stick: its call is swapped out)
_parts = open(os.path.join(HERE, 'make_a320_parts.py')).read()
assert _parts.count('bpy.ops.export_scene.gltf(') == 1
exec(compile(_parts.replace('bpy.ops.export_scene.gltf(', "(lambda **k: print('A320.glb NOT written (clean script)'))("), 'make_a320_parts.py', 'exec'))
o = bpy.data.objects['A320']
assert (o.matrix_world - Matrix()).median_scale if False else True
MW = o.matrix_world.copy()
REPORT = {}
def V(*a): return Vector(a)

# his untouched surface: kept for shooting, baking and measuring
src = o.copy(); src.data = o.data.copy(); src.name = 'MeshySrc'; bpy.context.scene.collection.objects.link(src)
bm0 = bmesh.new(); bm0.from_mesh(src.data); bm0.transform(MW)
BVH = BVHTree.FromBMesh(bm0)
def shoot(org, d, lo=0.0, hi=4.0, test=None):
    d = Vector(d).normalized(); p = Vector(org); t = 0.0
    while t < hi:
        hit = BVH.ray_cast(p, d, hi - t)
        if hit[0] is None: return None
        t += hit[3]
        if t >= lo and (test is None or test(hit[0])): return hit[0]
        p = hit[0] + d * 1e-4; t += 1e-4
    return None

# ------------------------------------------------------------------ materials
def pmat(name, col, metal=0.0, rough=0.5, img=None, tex=None, scale=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if tex:     # ambientCG set: colour, roughness, normal
        N = m.node_tree.nodes; L = m.node_tree.links
        uvm = N.new('ShaderNodeMapping'); uvm.inputs['Scale'].default_value = (scale, scale, scale); tc = N.new('ShaderNodeTexCoord')
        L.new(tc.outputs['UV'], uvm.inputs['Vector'])
        for kind, sock in (('Color', 'Base Color'), ('Roughness', 'Roughness'), ('NormalGL', None)):
            f = os.path.join(ROOT, tex + '_' + kind + '.jpg')
            if not os.path.exists(f): continue
            t = N.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(f); L.new(uvm.outputs['Vector'], t.inputs['Vector'])
            if kind != 'Color': t.image.colorspace_settings.name = 'Non-Color'
            if kind == 'NormalGL':
                nm = N.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.8
                L.new(t.outputs['Color'], nm.inputs['Color']); L.new(nm.outputs['Normal'], b.inputs['Normal'])
            elif kind == 'Color':
                mix = N.new('ShaderNodeMixRGB'); mix.blend_type = 'MULTIPLY'; mix.inputs['Fac'].default_value = 1.0
                mix.inputs['Color2'].default_value = (*col, 1); L.new(t.outputs['Color'], mix.inputs['Color1']); L.new(mix.outputs['Color'], b.inputs['Base Color'])
            else: L.new(t.outputs['Color'], b.inputs[sock])
    return m
M_INNER = pmat('A320C_InnerSkin', (0.62, 0.64, 0.62), 0.1, 0.55)     # inside of cowl / door: light grey primer
M_EDGE = pmat('A320C_PanelEdge', (0.40, 0.42, 0.42), 0.3, 0.5)       # panel sides
M_SEAL = pmat('A320C_Seal', (0.04, 0.04, 0.045), 0.0, 0.85)          # rubber seals and the frame lip in the seam
M_FRAME = pmat('A320C_Frame', (0.55, 0.57, 0.55), 0.2, 0.55)         # frames, walls
M_SILL = pmat('A320C_Sill', (0.70, 0.71, 0.72), 0.95, 0.3)           # door sill, rods, latches
M_LINER = pmat('A320C_HoldLiner', (0.70, 0.71, 0.70), 0.0, 0.8)      # cargo hold lining
M_FLOOR = pmat('A320C_HoldFloor', (0.30, 0.31, 0.32), 0.6, 0.5)      # hold floor, roller panels
M_CASE = pmat('A320C_FanCase', (0.52, 0.54, 0.56), 0.85, 0.4)        # fan case drum
M_AGB = pmat('A320C_Gearbox', (0.20, 0.21, 0.22), 0.6, 0.5)
M_PIPE = pmat('A320C_Pipe', (0.78, 0.76, 0.72), 1.0, 0.3)
M_HARN = pmat('A320C_Harness', (0.48, 0.34, 0.18), 0.0, 0.75)
M_TRACK = pmat('A320C_FlapTrack', (0.56, 0.58, 0.60), 0.6, 0.42)    # flap tracks, carriages, links: bare steel
M_BOOT = pmat('A320C_TrackBoot', (0.17, 0.17, 0.18), 0.0, 0.8)     # rubber boots, dark grey
M_COVE = pmat('A320C_Cove', (0.45, 0.47, 0.46), 0.2, 0.6)            # flap cove, ribs
MATS = {}

# ------------------------------------------------------------------ mesh helpers
class Mesh:
    def __init__(s): s.v = []; s.f = []; s.m = []; s.uv = []
    def add(s, p): s.v.append(Vector(p)); return len(s.v) - 1
    def face(s, idx, mat, uvs=None): s.f.append(idx); s.m.append(mat); s.uv.append(uvs or [(0, 0)] * len(idx))
    def grid(s, rows, mat, closed=False, uvrows=None, flip=False):     # rows: lists of vertex ids, same length
        n = len(rows[0])
        for i in range(len(rows) - 1):
            for j in range(n if closed else n - 1):
                j2 = (j + 1) % n; q = [rows[i][j], rows[i][j2], rows[i + 1][j2], rows[i + 1][j]]
                u = [uvrows[i][j], uvrows[i][j2], uvrows[i + 1][j2], uvrows[i + 1][j]] if uvrows else None
                if flip: q = q[::-1]; u = u[::-1] if u else None
                s.face(q, mat, u)
    def obj(s, name, merge=0.0008):
        me = bpy.data.meshes.new(name); me.from_pydata([tuple(p) for p in s.v], [], s.f)
        mats = []
        for m in s.m:
            if m not in mats: mats.append(m)
        for m in mats: me.materials.append(m)
        uvl = me.uv_layers.new(name='UVMap')
        for poly, mi, uv in zip(me.polygons, s.m, s.uv):
            poly.material_index = mats.index(mi)
            for k, li in enumerate(poly.loop_indices): uvl.data[li].uv = uv[k]
        ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
        bm = bmesh.new(); bm.from_mesh(me)
        bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=merge)
        bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-5)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(me); bm.free()
        for p in me.polygons: p.use_smooth = True
        me.use_auto_smooth = True; me.auto_smooth_angle = math.radians(50)
        return ob
def tube(M, pts, r, mat, seg=10, cap=True):    # a round tube along a polyline
    rows = []; n0 = None
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        a = t.cross(V(0, 0, 1) if abs(t.z) < 0.9 else V(1, 0, 0)).normalized(); b = t.cross(a)
        rows.append([M.add(p + (a * math.cos(2 * math.pi * k / seg) + b * math.sin(2 * math.pi * k / seg)) * r) for k in range(seg)])
    M.grid(rows, mat, closed=True)
    if cap:
        for row, fl in ((rows[0], True), (rows[-1], False)): M.face(row[::-1] if fl else row, mat)
def box(M, c, ax, size, mat):                   # box at c with axes ax (3 unit vectors), size (3)
    c = Vector(c); P = []
    for k in range(8):
        P.append(M.add(c + sum((ax[i] * (size[i] / 2) * (1 if (k >> i) & 1 else -1) for i in range(3)), Vector())))
    for q in ((0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)): M.face([P[i] for i in q], mat)

# rounded rectangle in the part's own 2-D frame (u, v metres): points of the outline moved by d (>0 outward), the same count
# for every d so rings can be joined into quads
def rr(u0, u1, v0, v1, rc, d, nside_u, nside_v, ncorner=8):
    uc, vc = (u0 + u1) / 2, (v0 + v1) / 2; hu, hv = (u1 - u0) / 2 + d, (v1 - v0) / 2 + d
    r = max(rc + d, 0.0); r = min(r, max(hu, 0), max(hv, 0)); hu = max(hu, 1e-4); hv = max(hv, 1e-4)
    pts = []
    def side(a, b, n, nrm):
        for k in range(n): pts.append((a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n, nrm))
    def corner(cx, cy, a0):
        for k in range(ncorner):
            a = math.radians(a0 + 90 * k / ncorner); pts.append((cx + r * math.cos(a), cy + r * math.sin(a), (math.cos(a), math.sin(a))))
    side((uc - hu + r, vc - hv), (uc + hu - r, vc - hv), nside_u, (0, -1)); corner(uc + hu - r, vc - hv + r, -90)
    side((uc + hu, vc - hv + r), (uc + hu, vc + hv - r), nside_v, (1, 0)); corner(uc + hu - r, vc + hv - r, 0)
    side((uc + hu - r, vc + hv), (uc - hu + r, vc + hv), nside_u, (0, 1)); corner(uc - hu + r, vc + hv - r, 90)
    side((uc - hu, vc + hv - r), (uc - hu, vc - hv + r), nside_v, (-1, 0)); corner(uc - hu + r, vc - hv + r, 180)
    return pts
def sd_rr(u, v, u0, u1, v0, v1, rc):            # signed distance to the rounded rectangle (numpy)
    uc, vc = (u0 + u1) / 2, (v0 + v1) / 2; hu, hv = (u1 - u0) / 2 - rc, (v1 - v0) / 2 - rc
    qx = np.abs(u - uc) - hu; qy = np.abs(v - vc) - hv
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - rc

# ------------------------------------------------------------------ the one clean cut through his body
def refine_bm(bm, sdf, band, L, reach=0.03, label=''):
    """his big triangles near the outline are split (flat, his shape unchanged) until no edge there is longer than L, so
    the cut follows the outline as a smooth continuous line"""
    n0 = len(bm.faces)
    for it in range(16):
        bm.verts.ensure_lookup_table()
        P = np.array([v.co[:] for v in bm.verts]); inb = band(P); sd = np.full(len(P), 1e3); sd[inb] = sdf(P[inb])
        sel = set(); cand = set()
        for i in np.nonzero(inb)[0]: cand.update(bm.verts[int(i)].link_faces)
        for f in cand:
            ids = [v.index for v in f.verts]
            if not all(inb[i] for i in ids): continue
            ls = [e.calc_length() for e in f.edges]; sz = max(ls)
            if sz <= L: continue
            s = [sd[i] for i in ids]
            if min(s) < sz + reach and max(s) > -(sz + reach):
                for e, l_ in zip(f.edges, ls):
                    if l_ > L: sel.add(e)
        if not sel: break
        bmesh.ops.subdivide_edges(bm, edges=list(sel), cuts=1, use_grid_fill=False)
        bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 3])
    REPORT.setdefault('refined', {})[label] = dict(passes=it, faces_added=len(bm.faces) - n0); print('REFINED', label, it, len(bm.faces) - n0, flush=True)

def cut_hole(sdf, band, label, refine=0.0, keep=None, cap=None, islands=None):
    """sdf(P Nx3) -> signed distance of the hole outline (<0 inside); band(P) -> bool, which of his vertices take part.
    Every edge crossing the outline is split exactly on it, the faces are split along it, the inside goes.
    refine: first split his faces near the outline to this edge length. keep: name -> the cut-out faces become that object.
    cap: (centre, normal) -> the open cut line is closed with a fan to centre on both sides (body faces +normal)."""
    bm = bmesh.new(); bm.from_mesh(o.data); bm.transform(MW)
    if refine: refine_bm(bm, sdf, band, refine, label=label)
    bm.verts.ensure_lookup_table()
    P = np.array([v.co[:] for v in bm.verts]); inb = band(P)
    sd = np.full(len(P), 1e3); sd[inb] = sdf(P[inb])
    for v in bm.verts: v.tag = False
    val = {v: sd[v.index] for v in bm.verts if inb[v.index]}
    for e in list(bm.edges):
        a, b = e.verts
        if a not in val or b not in val: continue
        sa, sb = val[a], val[b]
        if sa * sb >= 0 or abs(sa - sb) < 1e-12: continue
        t = sa / (sa - sb)
        ne, nv = bmesh.utils.edge_split(e, a, t)
        val[nv] = 0.0; nv.tag = True
    nsplit = 0
    for f in list(bm.faces):
        on = [v for v in f.verts if v.tag]
        if len(on) == 2 and not any(e for e in f.edges if on[0] in e.verts and on[1] in e.verts):
            try: bmesh.utils.face_split(f, on[0], on[1]); nsplit += 1
            except Exception: pass
    gone = []
    for f in bm.faces:
        vs = [val.get(v) for v in f.verts]
        if any(x is None for x in vs): continue
        if sum(vs) / len(vs) < -1e-6 and max(vs) <= 1e-6: gone.append(f)
    part = None
    if keep:
        bm.faces.ensure_lookup_table(); gi = set(f.index for f in gone)
        b2 = bm.copy(); b2.faces.ensure_lookup_table()
        bmesh.ops.delete(b2, geom=[f for f in b2.faces if f.index not in gi], context='FACES')
        me2 = bpy.data.meshes.new(keep)
        if cap: _cap(b2, cap[0], -cap[1])
        b2.to_mesh(me2); b2.free()
        for m in o.data.materials: me2.materials.append(m)
        part = bpy.data.objects.new(keep, me2); bpy.context.scene.collection.objects.link(part)
        for p_ in me2.polygons: p_.use_smooth = True
    bmesh.ops.delete(bm, geom=gone, context='FACES')
    if islands:       # bits of his mesh left loose by the cut (whole piece inside the box, small): they go
        lo, hi = Vector(islands[0]), Vector(islands[1]); seen = set(); drop = []
        def inbox(v): return all(lo[k] <= v.co[k] <= hi[k] for k in range(3))
        for f in bm.faces:
            if f in seen or not all(inbox(v) for v in f.verts): continue
            comp = [f]; seen.add(f); stack = [f]; ok = True
            while stack and ok:
                g = stack.pop()
                for v in g.verts:
                    if not inbox(v): ok = False; break
                    for h in v.link_faces:
                        if h not in seen: seen.add(h); stack.append(h); comp.append(h)
                if len(comp) > 3000: ok = False
            if ok: drop += comp
        bmesh.ops.delete(bm, geom=drop, context='FACES')
        REPORT.setdefault('loose_bits_removed', {})[label] = len(drop)
    if cap: _cap(bm, cap[0], cap[1])
    bm.transform(MW.inverted()); bm.to_mesh(o.data); bm.free()
    REPORT.setdefault('cuts', {})[label] = dict(faces_removed=len(gone), faces_split=nsplit)
    return part
def _cap(bm, c, nrm):     # close the open cut line (edges between cut vertices with one face) with a fan to c
    c = Vector(c); cv = bm.verts.new(c); nf = 0
    for e in list(bm.edges):
        a, b = e.verts
        if len(e.link_faces) != 1 or abs((a.co - c).dot(nrm)) > 3e-4 or abs((b.co - c).dot(nrm)) > 3e-4: continue
        if (a.co - c).length > 0.6: continue
        q = [a, b, cv]
        if (b.co - a.co).cross(c - a.co).dot(nrm) < 0: q = [b, a, cv]
        try: bm.faces.new(q); nf += 1
        except Exception: pass
    REPORT.setdefault('caps', []).append(nf)

# ------------------------------------------------------------------ a single-curvature panel (door, cowl): S(u,v) -> point
def panel(name, S, D, out_rect, nU, nV, t, mat_in, step=0.02, gap=0.002, rb=0.004):
    """S(u, v) -> (point on his surface, inward unit vector). out_rect = (u0,u1,v0,v1,rc): the panel line (seam centre).
    The part: outer skin from the rolled edge inward, side, inner skin at depth t. Returns (part Mesh, outer face count, ring fn)."""
    u0, u1, v0, v1, rc = out_rect
    M = Mesh(); su = max(u1 - u0, v1 - v0)
    def ring(d, depth):
        ids = []; uvs = []
        for (u, v, n) in rr(u0, u1, v0, v1, rc, d, nU, nV):
            p, w = S(u, v); ids.append(M.add(p + w * depth)); uvs.append(((u - u0) / su, (v - v0) / su))
        return ids, uvs
    half = min(u1 - u0, v1 - v0) / 2
    insets = [gap + rb * (1 - math.sin(math.radians(a))) for a in (0, 30, 60, 90)]
    depths = [rb * (1 - math.cos(math.radians(a))) for a in (0, 30, 60, 90)]
    # outer skin: from the inner end of the rolled edge to the middle
    k = gap + rb; outer = []
    while k < half: outer.append(k); k += step
    outer.append(half - 1e-4)
    R = [ring(-d, 0.0) for d in outer]
    M.grid([r[0] for r in R][::-1], 'SKIN', uvrows=[r[1] for r in R][::-1])
    nskin = len(M.f)
    # rolled edge (outer -> side), side, rolled edge into the inner skin
    edge_rows = [ring(-(gap + rb * (1 - math.sin(math.radians(a)))), rb * (1 - math.cos(math.radians(a)))) for a in (0, 30, 60, 90)]
    rows = [R[0]] + edge_rows[1:]
    M.grid([r[0] for r in rows], 'SKIN', uvrows=[r[1] for r in rows])
    side = [ring(-gap, depth) for depth in (rb, t - rb)]
    M.grid([rows[-1][0], side[1][0]], M_EDGE)
    inner_edge = [ring(-(gap + rb * (1 - math.cos(math.radians(a)))), t - rb + rb * math.sin(math.radians(a))) for a in (30, 60, 90)]
    rows2 = [side[1]] + inner_edge
    M.grid([r[0] for r in rows2], M_EDGE)
    RI = [ring(-d, t) for d in outer]
    M.grid([rows2[-1][0]] + [r[0] for r in RI], mat_in)
    return M
def frame(S, out_rect, nU, nV, depthfn, gap=0.002, lip=0.018, seal_mat=M_SEAL, wall_mat=M_FRAME, sill_mat=None, flange=0.0):
    """The body's side of the seam: a lip 4 mm under his skin edge (only seen in the seam), then the wall going in to
    depthfn(u, v). Returns the Mesh and the last ring (for the bay)."""
    u0, u1, v0, v1, rc = out_rect; M = Mesh()
    def ring(d, depth):
        out = []
        for (u, v, n) in rr(u0, u1, v0, v1, rc, d, nU, nV):
            p, w = S(u, v); dd = depthfn(u, v) if depth is None else depth
            out.append(M.add(p + w * dd))
        return out
    rows = [ring(gap + lip, 0.004), ring(gap, 0.004), ring(gap, 0.012)]
    M.grid(rows[:2], seal_mat, closed=True); M.grid(rows[1:], seal_mat, closed=True)
    last = ring(gap, None)
    if sill_mat:      # the bottom side of the opening is the metal sill
        nb = [n for (u, v, n) in rr(u0, u1, v0, v1, rc, gap, nU, nV)]; k = len(nb)
        for j in range(k):
            j2 = (j + 1) % k; mt = sill_mat if (nb[j][1] < -0.5 and nb[j2][1] < -0.5) else wall_mat
            M.face([rows[2][j], rows[2][j2], last[j2], last[j]], mt)
    else: M.grid([rows[2], last], wall_mat, closed=True)
    if flange:        # the wall turns out over the hold lining (covers the lining's cut, no slit)
        fl = ring(gap + flange, None); M.grid([last, fl], wall_mat, closed=True)
    return M

SKIPS = {}      # part name -> [(world point, 1 hidden inside a canoe | 2 over a hole in his mesh)]: not on his surface
def measure(ob, label, mat_name):   # closed check: his original surface vs the part's outer skin
    me = ob.data; mi = [i for i, m in enumerate(me.materials) if m and m.name == mat_name]
    sk = SKIPS.get(label) or []; kd = None; kinds = {}
    if sk:
        kd = mathutils.kdtree.KDTree(len(sk))
        for k, (q, _) in enumerate(sk): kd.insert(q, k)
        kd.balance()
    def skipped(i):
        if kd is None: return 0
        q, k, d = kd.find(ob.matrix_world @ me.vertices[i].co)
        return sk[k][1] if d < 0.002 else 0
    SV = {}
    vs = set(); cs = []
    for p in me.polygons:
        if p.material_index not in mi: continue
        f = [SV.setdefault(i, skipped(i)) for i in p.vertices]
        vs.update(i for i, k in zip(p.vertices, f) if not k)
        if not all(f): cs.append(ob.matrix_world @ p.center)
    for i, k in SV.items():
        if k: kinds[k] = kinds.get(k, 0) + 1
    if kd is not None: REPORT.setdefault('left_out_of_check', {})[label] = dict(hidden_in_canoe=kinds.get(1, 0), over_his_hole_or_bump=kinds.get(2, 0))
    vl = list(vs); dv = [BVH.find_nearest(ob.matrix_world @ me.vertices[i].co)[3] for i in vl]
    worst = sorted(range(len(dv)), key=lambda i: -dv[i])[:6]
    REPORT.setdefault('worst', {})[label] = [[round(x, 3) for x in (ob.matrix_world @ me.vertices[vl[i]].co)] + [round(dv[i] * 1000)] for i in worst]
    REPORT.setdefault('over_10mm', {})[label] = sum(1 for d in dv if d > 0.01)
    dc = [BVH.find_nearest(c)[3] for c in cs]
    REPORT.setdefault('closed_check_mm', {})[label] = dict(vertices=len(dv), vert_max=round(max(dv) * 1000, 2), vert_mean=round(sum(dv) / len(dv) * 1000, 3),
        face_centres=len(dc), centre_max=round(max(dc) * 1000, 2), centre_mean=round(sum(dc) / len(dc) * 1000, 3),
        centre_p99=round(float(np.percentile(dc, 99)) * 1000, 2))

def skin_mat(name):
    m = bpy.data.materials.new('A320C_%s_Skin' % name); m.use_nodes = True; N = m.node_tree.nodes; L = m.node_tree.links
    b = N['Principled BSDF']; ims = []
    for k, cs in (('Color', 'sRGB'), ('ORM', 'Non-Color'), ('Normal', 'Non-Color')):
        im = bpy.data.images.new('A320C_%s_%s' % (name, k), 2048, 2048, alpha=False, float_buffer=False)
        im.colorspace_settings.name = cs; t = N.new('ShaderNodeTexImage'); t.image = im; t.name = k; ims.append(im)
    L.new(N['Color'].outputs['Color'], b.inputs['Base Color'])
    sep = N.new('ShaderNodeSeparateColor'); L.new(N['ORM'].outputs['Color'], sep.inputs['Color'])
    L.new(sep.outputs['Green'], b.inputs['Roughness']); L.new(sep.outputs['Blue'], b.inputs['Metallic'])
    nm = N.new('ShaderNodeNormalMap'); L.new(N['Normal'].outputs['Color'], nm.inputs['Color']); L.new(nm.outputs['Normal'], b.inputs['Normal'])
    return m, ims

def bake(ob, m, ims):
    sc = bpy.context.scene; sc.render.engine = 'CYCLES'; sc.cycles.samples = 1; sc.cycles.device = 'CPU'
    dummy = bpy.data.images.get('A320C_dummy') or bpy.data.images.new('A320C_dummy', 16, 16)
    extra = []
    for mm in ob.data.materials:
        if mm == m: continue
        t = mm.node_tree.nodes.new('ShaderNodeTexImage'); t.image = dummy; mm.node_tree.nodes.active = t; extra.append((mm, t))
    sm = src.data.materials[0]; N = sm.node_tree.nodes; L = sm.node_tree.links; b = N['Principled BSDF']
    tex = {n.image.name: n for n in N if n.type == 'TEX_IMAGE'}
    bpy.ops.object.select_all(action='DESELECT'); src.hide_render = False; src.select_set(True); ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    b.inputs['Emission Strength'].default_value = 1.0
    for k, im in zip(('Color', 'ORM', 'Normal'), ims):
        m.node_tree.nodes.active = m.node_tree.nodes[k]
        if k != 'Normal':
            ln = L.new(tex['Image_0' if k == 'Color' else 'Image_1'].outputs['Color'], b.inputs['Emission'])
            bpy.ops.object.bake(type='EMIT', use_selected_to_active=True, cage_extrusion=0.02, max_ray_distance=0.05, margin=8)
            L.remove(ln)
        else:
            bpy.ops.object.bake(type='NORMAL', normal_space='TANGENT', use_selected_to_active=True, cage_extrusion=0.02, max_ray_distance=0.05, margin=8)
        im.filepath_raw = os.path.join(HERE, 'Textures', im.name + '.png'); im.file_format = 'PNG'; im.save()
    b.inputs['Emission Strength'].default_value = 0.0
    b.inputs['Emission'].default_value = (0, 0, 0, 1)
    for mm, t in extra: mm.node_tree.nodes.remove(t)

SKINS = {}
def finish(M, name, pivot):
    m, ims = skin_mat(name); SKINS[name] = (m, ims)
    M.m = [m if x == 'SKIN' else x for x in M.m]
    ob = M.obj(name)
    me = ob.data; me.transform(Matrix.Translation(-Vector(pivot))); ob.location = pivot
    return ob

PARTS = {}      # name -> (object, open transform about its pivot)
STATIC = []

# ================================================================== the fan cowl (engine 1, outboard half)
ECX, ECZ, ER = 5.50, 1.945, 1.33
def cowl_S(s):
    def S(u, v):   # u: arc length from the top (outboard +) on radius ER, v: y
        f = u / ER; d = V(s * math.sin(f), 0, math.cos(f)); c = V(s * ECX, v, ECZ)
        p = shoot(c + d * 1.75, -d, test=lambda h: 1.15 < (h - V(s * ECX, h.y, ECZ)).length < 1.62)
        if p is None: p = c + d * ER
        return p, -d
    return S
def build_cowl():
    s = 1; S = cowl_S(s); en = 'Eng1'; name = 'FanCowl_Eng1_Out'
    rect = (math.radians(27) * ER, math.radians(179.5) * ER, -8.52, -7.22, 0.04)     # front/rear on his texture's panel lines
    nU, nV = 170, 64
    M = panel(name, S, None, rect, nU, nV, 0.045, M_INNER, step=0.022)
    # inner skin ribs (frames) and the 3 latch hooks on the bottom edge, 2 hold-open rod fittings
    for v in (-7.45, -7.87, -8.30):
        pts = [S(u, v)[0] + S(u, v)[1] * 0.045 for u in np.linspace(rect[0] + 0.06, rect[1] - 0.06, 40)]
        w = [S(u, v)[1] for u in np.linspace(rect[0] + 0.06, rect[1] - 0.06, 40)]
        rows = [[M.add(p + ww * dz + V(0, dy, 0)) for p, ww in zip(pts, w)] for dy, dz in ((-0.012, 0), (-0.012, 0.035), (0.012, 0.035), (0.012, 0))]
        M.grid(rows, M_INNER)
    for v in (-7.5, -7.87, -8.25):
        p, w = S(rect[1] - 0.05, v); box(M, p + w * 0.07, (V(0, 1, 0), w.cross(V(0, 1, 0)).normalized(), w), (0.07, 0.06, 0.05), M_SILL)
    for v in (-7.35, -7.75, -8.05, -8.4):
        p, w = S(rect[0] + 0.05, v); box(M, p + w * 0.06, (V(0, 1, 0), w.cross(V(0, 1, 0)).normalized(), w), (0.025, 0.09, 0.04), M_SILL)
    ob = finish(M, name, V(s * ECX, -7.87, ECZ + ER * 1.0) + V(s * 0.38, 0, -0.03))
    # hinge on the pylon: the top edge line, axis along y; opens 50 deg up and out
    hp = S(rect[0], -7.87)[0]
    ob.data.transform(Matrix.Translation(ob.location - hp)); ob.location = hp
    PARTS[name] = (ob, ('rot', hp, V(0, -1, 0) * s, 50.0))
    HF = Mesh()      # 4 hinge fittings: a lug on the cowl, a bracket on the pylon frame, the pin through both
    for v in (-7.35, -7.75, -8.05, -8.4):
        p, w = S(rect[0], v); t_ = w.cross(V(0, 1, 0)).normalized()
        q = V(hp.x, v, hp.z) + w * 0.03
        box(HF, q - t_ * 0.03, (V(0, 1, 0), t_, w), (0.03, 0.07, 0.05), M_SILL)
        tube(HF, [q - V(0, 0.03, 0), q + V(0, 0.03, 0)], 0.008, M_BOLT if 'M_BOLT' in globals() else M_SILL, seg=8)
    STATIC.append(HF.obj('FanCowl_Eng1_Out_Hinges'))
    # the body's side: frame and walls down to the fan case
    def depth(u, v):
        p, w = S(u, v); return (p - V(s * ECX, p.y, ECZ)).length - 1.055
    F = frame(S, rect, nU, nV, depth, wall_mat=M_FRAME)
    # the fan case drum and what sits on it (CFM56-5B: gearbox and accessories low on the case)
    c = V(s * ECX, 0, ECZ); rows = []
    for y in np.linspace(-8.75, -7.0, 12):
        rows.append([F.add(c + V(1.065 * math.sin(a) * s, y, 1.065 * math.cos(a))) for a in np.linspace(0, 2 * math.pi, 96, endpoint=False)])
    F.grid(rows, M_CASE, closed=True)
    for y in (-8.7, -7.5, -7.05):       # stiffening flanges
        tube(F, [c + V(1.07 * math.sin(a) * s, y, 1.07 * math.cos(a)) for a in np.linspace(0, 2 * math.pi, 97)], 0.012, M_CASE, seg=8, cap=False)
    for a, y, sz, m in ((150, -7.9, (0.55, 0.42, 0.16), M_AGB), (115, -8.15, (0.3, 0.28, 0.2), M_AGB), (100, -7.45, (0.22, 0.30, 0.18), M_SILL), (65, -7.7, (0.26, 0.2, 0.12), M_AGB)):
        u = V(math.sin(math.radians(a)) * s, 0, math.cos(math.radians(a))); t_ = u.cross(V(0, 1, 0)).normalized()
        box(F, c + u * (1.065 + sz[2] / 2) + V(0, y, 0), (t_, V(0, 1, 0), u), sz, m)
    for a0, a1, y, r in ((40, 170, -7.62, 1.12), (50, 160, -8.05, 1.13), (30, 140, -8.45, 1.11)):
        tube(F, [c + V(r * math.sin(math.radians(a)) * s, y, r * math.cos(math.radians(a))) for a in np.linspace(a0, a1, 40)], 0.018, M_PIPE, seg=8)
    tube(F, [c + V(1.10 * math.sin(math.radians(a)) * s, -7.28, 1.10 * math.cos(math.radians(a))) for a in np.linspace(35, 175, 40)], 0.022, M_HARN, seg=8)
    STATIC.append(F.obj('FanCowl_Eng1_Out_Bay'))
    # hold-open rods (shown open): from the case to the cowl's lower inner edge
    R = Mesh(); M_rot = Matrix.Translation(hp) @ Matrix.Rotation(math.radians(50.0), 4, V(0, -1, 0) * s) @ Matrix.Translation(-hp)
    for v in (-7.55, -8.2):
        a = c + V(1.08 * math.sin(math.radians(85)) * s, v, 1.08 * math.cos(math.radians(85)))
        p, w = S(rect[1] - 0.35, v); b_ = M_rot @ (p + w * 0.05)
        tube(R, [a, b_], 0.016, M_SILL, seg=10)
        u_ = (a - c); u_.y = 0; u_.normalize(); box(R, a - u_ * 0.01, (V(0, 1, 0), u_.cross(V(0, 1, 0)), u_), (0.06, 0.06, 0.04), M_SILL)   # clevis on the case
        bw = (M_rot.to_3x3() @ w).normalized(); box(R, b_ + bw * 0.005, (V(0, 1, 0), bw.cross(V(0, 1, 0)).normalized(), bw), (0.06, 0.06, 0.03), M_SILL)  # fitting on the cowl
    rods = R.obj('FanCowl_Eng1_Out_HoldOpenRods'); rods['open_only'] = 1
    # his body: the hole, a smooth curve 2 mm outside the panel line
    def sdf(P):
        dx = P[:, 0] - s * ECX; dz = P[:, 2] - ECZ
        u = np.arctan2(dx * s, dz) * ER
        return sd_rr(u, P[:, 1], *rect) - 0.002
    def band(P):
        r = np.hypot(P[:, 0] - s * ECX, P[:, 2] - ECZ)
        return (r > 1.10) & (r < 1.75) & (P[:, 1] > -8.9) & (P[:, 1] < -6.9) & (P[:, 0] * s > ECX - 1.0)
    cut_hole(sdf, band, name)
    return ob

# ================================================================== forward cargo door (right side = Blender -x)
FZC, FR = 4.235, 2.047        # his fuselage here (fitted to his skin at y -2 .. -3.8): axis height, radius
def door_S(u, v):             # u: y, v: arc length on the fuselage from the bottom, up the right side (-x)
    th = v / FR; d = V(-math.sin(th), 0, -math.cos(th)); c = V(0, u, FZC)
    p = shoot(c + d * (FR + 0.6), -d, test=lambda h: (h - V(0, h.y, FZC)).length > FR - 0.25)
    return (p if p else c + d * FR), -d
def arc_of(z): return FR * math.acos((FZC - z) / FR)
def build_door():
    name = 'CargoDoorFwd'; v0 = arc_of(2.55); rect = (-3.72, -1.9, v0, v0 + 1.30, 0.14)   # A320 layout (Airbus AC 2-3): 1.82 x 1.24 m clear
    nU, nV = 90, 64; T = 0.07
    M = panel(name, door_S, None, rect, nU, nV, T, M_INNER, step=0.02)
    def inner(u, v, h=0.0): p, w = door_S(u, v); return p + w * (T + h)
    def axes(u, v): w = door_S(u, v)[1]; return (V(0, 1, 0), w.cross(V(0, 1, 0)).normalized(), w)
    for u in (-3.35, -2.81, -2.27):       # inner frames, following the curved inner skin
        rows = [[M.add(inner(u + du, v, h)) for v in np.linspace(rect[2] + 0.06, rect[3] - 0.06, 30)] for du, h in ((-0.015, 0), (-0.015, 0.045), (0.015, 0.045), (0.015, 0))]
        M.grid(rows, M_INNER)
    for v in (rect[2] + 0.4, rect[3] - 0.4):
        rows = [[M.add(inner(u, v + dv, h)) for u in np.linspace(rect[0] + 0.06, rect[1] - 0.06, 30)] for dv, h in ((-0.015, 0), (-0.015, 0.035), (0.015, 0.035), (0.015, 0))]
        M.grid(rows, M_INNER)
    box(M, inner(-2.81, rect[3] - 0.15, 0.03), axes(-2.81, rect[3] - 0.15), (0.12, 0.08, 0.06), M_SILL)          # actuator fitting
    for u in (-3.5, -2.1): box(M, inner(u, rect[2] + 0.07, 0.02), axes(u, rect[2] + 0.07), (0.08, 0.05, 0.05), M_SILL)   # latch hooks
    sr = [inner(u, v, 0.005) for (u, v, n) in rr(*rect[:4], rect[4], -0.016, 60, 44)]
    tube(M, sr + sr[:1], 0.006, M_SEAL, seg=8, cap=False)     # rubber seal round the door's inner face
    pv, wv = door_S(-3.45, rect[2] + 0.22); box(M, pv + wv * 0.006, axes(-3.45, rect[2] + 0.22), (0.16, 0.10, 0.004), M_SEAL)   # small vent door, flush
    top, wt = door_S(-2.81, rect[3]); hp = top + wt * 0.015
    for u in (-3.5, -3.0, -2.6, -2.1): box(M, door_S(u, rect[3] - 0.03)[0] + wt * 0.025, axes(u, rect[3] - 0.03), (0.06, 0.05, 0.03), M_SILL)   # hinge lugs
    ob = finish(M, name, hp)
    PARTS[name] = (ob, ('rot', hp, V(0, 1, 0), 100.0))       # up and out to just past horizontal
    HG = Mesh(); tube(HG, [V(hp.x, -3.65, hp.z), V(hp.x, -1.97, hp.z)], 0.012, M_SILL, seg=10)    # hinge pin along the top
    for u in (-3.25, -2.81, -2.35): box(HG, hp + wt * 0.02 + V(0, u + 2.81, 0), axes(u, rect[3]), (0.06, 0.06, 0.05), M_SILL)
    STATIC.append(HG.obj('CargoDoorFwd_Hinge'))
    RL = FR - 0.15; zf, zc = 2.42, 3.85; ya, yb = -4.6, -1.0; xfar = 1.3
    def to_lining(u, v):          # wall depth: from his skin to 5 mm in front of the hold lining
        p, w = door_S(u, v); return math.hypot(p.x, p.z - FZC) - (RL - 0.005)
    F = frame(door_S, rect, nU, nV, to_lining, sill_mat=M_SILL, flange=0.10)      # seal lip, wall, sill, flange on the lining
    tha = math.acos((FZC - zf) / RL); thb = math.acos((FZC - zc) / RL)
    def lp(y, th): return V(-RL * math.sin(th), y, FZC - RL * math.cos(th))
    ths = np.linspace(tha, thb, 40); ys = np.linspace(ya, yb, 90)
    sdv = lambda y, th: sd_rr(np.array([y]), np.array([th * FR]), *rect)[0]
    G = [[F.add(lp(y, th)) for th in ths] for y in ys]
    for i in range(len(ys) - 1):
        for j in range(len(ths) - 1):
            if min(sdv(ys[a], ths[b]) for a in (i, i + 1) for b in (j, j + 1)) < 0.03: continue
            F.face([G[i][j], G[i + 1][j], G[i + 1][j + 1], G[i][j + 1]], M_LINER)
    fl = [F.add(V(x, y, zf)) for x, y in ((lp(0, tha).x, ya), (xfar, ya), (xfar, yb), (lp(0, tha).x, yb))]; F.face(fl, M_FLOOR)
    ce = [F.add(V(x, y, zc)) for x, y in ((lp(0, thb).x, ya), (xfar, ya), (xfar, yb), (lp(0, thb).x, yb))]; F.face(ce[::-1], M_LINER)
    F.face([F.add(V(xfar, y, z)) for y, z in ((ya, zf), (ya, zc), (yb, zc), (yb, zf))], M_LINER)
    for y in (ya, yb):
        F.face([F.add(lp(y, th)) for th in ths] + [F.add(V(xfar, y, zc)), F.add(V(xfar, y, zf))], M_LINER)
    for y in np.arange(ya + 0.27, yb, 0.53):     # frames round the lining, clear of the opening
        if abs(y + 2.81) < 1.0: continue
        tube(F, [lp(y, th) for th in ths], 0.025, M_FRAME, seg=6)
    box(F, V(lp(0, tha).x + 0.25, -2.81, zf + 0.01), (V(0, 1, 0), V(1, 0, 0), V(0, 0, 1)), (1.9, 0.5, 0.02), M_SILL)      # sill plate
    STATIC.append(F.obj('CargoDoorFwd_Bay'))
    Rm = Matrix.Translation(hp) @ Matrix.Rotation(math.radians(100.0), 4, V(0, 1, 0)) @ Matrix.Translation(-hp)
    A = Mesh(); a = V(lp(0, thb).x + 0.35, -2.8085, zc - 0.04); b_ = Rm @ inner(-2.8085, rect[3] - 0.15, 0.06)
    tube(A, [a, a + (b_ - a) * 0.55], 0.035, M_SILL, seg=12); tube(A, [a + (b_ - a) * 0.5, b_], 0.02, M_PIPE, seg=10)
    box(A, V(a.x, a.y, zc - 0.02), (V(1, 0, 0), V(0, 1, 0), V(0, 0, 1)), (0.12, 0.1, 0.04), M_SILL)
    dr = (b_ - a).normalized()
    for e in (a, b_):     # clevis at both ends: two lugs and a pin across (along y)
        for sy in (-0.032, 0.032): box(A, e + V(0, sy, 0), (dr, V(0, 1, 0), dr.cross(V(0, 1, 0)).normalized()), (0.07, 0.012, 0.05), M_SILL)
        tube(A, [e + V(0, -0.045, 0), e + V(0, 0.045, 0)], 0.009, M_PIPE, seg=10)
    act = A.obj('CargoDoorFwd_Actuator'); act['open_only'] = 1
    def sdf(P):
        th = np.arctan2(-P[:, 0], FZC - P[:, 2]); return sd_rr(P[:, 1], th * FR, *rect) - 0.002
    def band(P):
        r = np.hypot(P[:, 0], P[:, 2] - FZC)
        return (P[:, 0] < -0.3) & (P[:, 1] > -5.5) & (P[:, 1] < -0.2) & (r > FR - 0.35) & (r < FR + 0.35) & (P[:, 2] < FZC + 0.3)
    cut_hole(sdf, band, name, refine=0.01)
    return ob

# ================================================================== inner flap, right wing (Blender +x): Fowler
def zU(a): return 3.33 + 0.09 * a
def wing_section(x, yF, yTE=-14.45):
    """his wing's section at x: top and bottom z at each y from yF back to the trailing edge"""
    top = []; bot = []
    for y in np.arange(yF, yTE - 0.12, -0.01):
        t = shoot(V(x, y, zU(x) + 1.5), V(0, 0, -1), test=lambda h: abs(h.z - zU(x)) < 0.6)
        if t is None: break
        # the lower skin: the first face under his top skin, facing down (a canoe floor, a hole or an inside face are sorted
        # out over all stations afterwards in flap_bottoms)
        h = BVH.ray_cast(V(x, y, t.z - 0.004), V(0, 0, -1), 0.75)
        top.append((y, t.z)); bot.append((y, h[0].z if h[0] is not None and h[1].z < 0 else float('nan')))
    return top, bot
def flap_bottoms(secs, CAN):
    """his lower skin under the flap, station by station. Where the first face under his top skin is not his lower skin
    (a canoe or fairing floor far below, his gear-leg hole: nothing, an inside face) the flap's lower skin is filled
    across the span from his real lower skin either side. Returns per station a flag per point: 0 his skin, 1 hidden
    inside a canoe / fairing (his surface there is the canoe), 2 not on his surface: over his gear-leg hole or his small bumps (filled smooth, seen closed)."""
    X = np.array([s[0] for s in secs]); NN = np.array([len(s[1]) for s in secs])
    for i, (x, top, bot) in enumerate(secs):   # a canoe tail goes on past his trailing edge: there the station ends with its neighbours
        nb = [k for k in range(len(secs)) if abs(X[k] - x) < 0.35]; m = int(np.median(NN[nb]))
        if NN[i] > m + 3: del top[m:], bot[m:]
    n = max(len(s[1]) for s in secs)
    BZ = np.full((len(secs), n), np.nan); TZ = np.full((len(secs), n), np.nan)
    for i, (x, top, bot) in enumerate(secs):
        TZ[i, :len(top)] = [p[1] for p in top]; BZ[i, :len(bot)] = [p[1] for p in bot]
    TH = TZ - BZ; good = np.isfinite(TH) & (TH > -0.002) & (TH < 0.7); bad = np.zeros_like(good)
    for j in range(n):
        g = good[:, j]
        if g.sum() < 8: continue
        d = BZ[g, j]; xg = X[g]; s_ = np.nanmedian(np.diff(d) / np.maximum(np.diff(xg), 1e-6))
        D = BZ[:, j] - s_ * X
        for i in range(len(X)):
            if not g[i]: continue
            nb = g & (np.abs(X - X[i]) < 0.6)
            if abs(D[i] - np.median(D[nb])) > 0.05: bad[i, j] = True
    ok = good & ~bad
    for _ in range(4):        # the flanks of a canoe: next to a bad point and off the line of the good ones beyond it
        grow = np.zeros_like(ok)
        for j in range(n):
            for i in range(len(X)):
                if not ok[i, j]: continue
                for k in (i - 1, i + 1):
                    if 0 <= k < len(X) and not ok[k, j] and np.isfinite(TZ[k, j]):
                        far = [m for m in range(i + (i - k), i + 6 * (i - k), i - k) if 0 <= m < len(X) and ok[m, j]]
                        if len(far) >= 3:
                            c = np.polyfit(X[far], BZ[far, j], 1)
                            if abs(BZ[i, j] - np.polyval(c, X[i])) > 0.008: grow[i, j] = True
        if not grow.any(): break
        ok &= ~grow
    FLAG = []
    for i, (x, top, bot) in enumerate(secs):
        fl = []
        for j in range(len(bot)):
            if ok[i, j]: fl.append(0); continue
            col = ok[:, j]
            if col.sum() < 2: fl.append(2); continue
            z = float(np.interp(x, X[col], BZ[col, j])); raw = BZ[i, j]
            bot[j] = (bot[j][0], z); fl.append(1 if any(c0 - 0.06 < x < c1 + 0.06 for c0, c1 in CAN) else 2)
        FLAG.append(np.array(fl))
        if i % 4 == 0: REPORT.setdefault('flap_flag_map', []).append([round(x, 2), len(fl), ''.join('.ch'[f] for f in fl[::4])])
    REPORT['flap_bottom_points'] = dict(his_skin=int(sum((f == 0).sum() for f in FLAG)), in_canoe=int(sum((f == 1).sum() for f in FLAG)),
                                        over_hole=int(sum((f == 2).sum() for f in FLAG)))
    return FLAG
def build_flap():
    name = 'FlapInner_R'; a0, a1, yF = 2.35, 6.15, -12.6; g = 0.002   # a1: inboard of his wing kink at 6.2 (the trailing edge steps aft there)
    # the canoes (flap-track fairings) under this flap: his mesh 0.4 - 1.2 m under the top skin, seen from below
    can = []
    for x in np.arange(a0 + 0.1, a1 - 0.1, 0.015):
        # aft of the trailing edge only the canoe tail is there: both rays must hit it
        if all(shoot(V(x, yy, 1.0), V(0, 0, 1), hi=3.0, test=lambda q, x=x: 2.3 < q.z < zU(x) - 0.2) is not None
               for yy in (-14.1, -14.3)): can.append(x)
    CAN = []
    for x in can:
        if CAN and x - CAN[-1][1] < 0.05: CAN[-1][1] = x
        else: CAN.append([x, x])
    CAN = [(c0 - 0.02, c1 + 0.02) for c0, c1 in CAN if c1 - c0 > 0.05]
    REPORT['flap_canoes'] = [[round(c0, 3), round(c1, 3)] for c0, c1 in CAN]
    xs = np.linspace(a0 + g, a1 - g, 130); NL = 60
    loops = []; secs = []
    for x in xs:
        top, bot = wing_section(x, yF - g)
        if len(top) < 20: continue
        secs.append((x, top, bot))
    FLAG = flap_bottoms(secs, CAN)
    REPORT['flap_sections'] = dict(n=len(secs), first=[secs[0][0], secs[0][1][0], secs[0][2][0], len(secs[0][1])] if secs else None)
    # one closed loop per station: top from front to the trailing edge, bottom back to the front, nose inside the cove
    M = Mesh(); rows = []; uvr = []; STN = []
    SK = SKIPS.setdefault(name, [])
    for (x, top, bot), fl in zip(secs, FLAG):
        ty = np.array([p[0] for p in top]); tz = np.array([p[1] for p in top]); bz = np.array([p[1] for p in bot])
        th = tz - bz; ite = len(th) - 1
        for i in range(len(th)):
            if th[i] < 0.006: ite = i; break
        ty, tz, bz = ty[:ite + 1], tz[:ite + 1], bz[:ite + 1]
        yte = ty[-1]; mid = (tz[-1] + bz[-1]) / 2; STN.append((x, ty, tz, bz))
        h = (tz[0] - bz[0]) / 2; zm = (tz[0] + bz[0]) / 2; b = min(0.10, 0.6 * h); hn = h - 0.04
        ys = np.linspace(ty[0], yte, NL)
        T = [(y, float(np.interp(-y, -ty, tz))) for y in ys]
        B = [(y, float(np.interp(-y, -ty, bz))) for y in ys[::-1]]
        # his trailing edge is blunt in places: the flap ends with its own top and bottom points there (a narrow TE face)
        fy = fl[:ite + 1]
        for y, z in B + [(ty[0] - 0.004, bz[0])]:      # lower skin points not on his surface: left out of the check, counted apart
            k_ = int(np.argmin(np.abs(ty - y))); kk = fy[max(k_ - 1, 0):k_ + 2].max()
            if kk: SK.append((V(x, y, z), int(kk)))
        nose = [(ty[0] + b * math.cos(a), zm + hn * math.sin(a)) for a in np.linspace(-math.pi / 2, math.pi / 2, 13)]
        # rolled edges where the skins meet the nose (4 mm)
        loop = T + B[1:] + [(ty[0] - 0.004, bz[0] + 0.004 * 0), (ty[0] + 0.004, bz[0] + 0.012)] + nose + [(ty[0] + 0.004, tz[0] - 0.012), (ty[0] - 0.004, tz[0])]
        rows.append([M.add(V(x, y, z)) for y, z in loop])
        uvr.append([((x - a0) / 3.9, k / len(loop) * 2.9 / 3.9) for k in range(len(loop))])
        loops.append(loop)
    nT = NL; nB = NL - 1
    # faces: skin (top + bottom), the rest edge material
    n = len(rows[0])
    for i in range(len(rows) - 1):
        for j in range(n):
            j2 = (j + 1) % n; q = [rows[i][j], rows[i][j2], rows[i + 1][j2], rows[i + 1][j]]
            u = [uvr[i][j], uvr[i][j2], uvr[i + 1][j2], uvr[i + 1][j]]
            M.face(q[::-1], 'SKIN' if j < nT + nB - 1 else M_EDGE, u[::-1])
    for row, fl in ((rows[0], False), (rows[-1], True)):      # end ribs
        c = M.add(sum((M.v[k] for k in row), Vector()) / len(row))
        for j in range(n): M.face([row[j], row[(j + 1) % n], c][::-1] if fl else [row[j], row[(j + 1) % n], c], M_EDGE)
    # Fowler: aft and down on curved tracks in the canoes. 30 deg = landing: the nose goes 0.42 m aft and 0.14 m down; that motion is
    # one turn about a point under the wing (hp), so the tracks are arcs about hp and the carriage stays on them all the way.
    SX = np.array([q[0] for q in STN])
    def stn(x): return STN[int(np.argmin(np.abs(SX - x)))]
    xm = (a0 + a1) / 2; _, ty_, tz_, bz_ = stn(xm); h_ = (tz_[0] - bz_[0]) / 2
    LE = V(xm, ty_[0] + min(0.10, 0.6 * h_), (tz_[0] + bz_[0]) / 2); OPEN = 30.0; D = (-0.42, -0.14)
    c_, s_ = math.cos(math.radians(OPEN)), math.sin(math.radians(OPEN)); det = (c_ - 1) ** 2 + s_ ** 2
    ry = ((c_ - 1) * D[0] + s_ * D[1]) / det; rz = (-s_ * D[0] + (c_ - 1) * D[1]) / det
    hp = V(xm, LE.y - ry, LE.z - rz)
    def turn(p, deg):
        return Matrix.Translation(hp) @ Matrix.Rotation(math.radians(deg), 4, V(1, 0, 0)) @ Matrix.Translation(-hp) @ p
    def refz(x, y):           # his lower skin (filled over canoes and holes) at (x, y); far up aft of the trailing edge
        _, ty_, tz_, bz_ = stn(x)
        return 10.0 if y < ty_[-1] - 0.005 else float(np.interp(-y, -ty_, bz_))
    def in_canoe(q):          # inside his canoe: under his lower skin with room for the rail, his canoe below and either side
        if q.z > refz(q.x, q.y) - 0.04 and q.y > stn(q.x)[1][-1]: return False
        d = BVH.ray_cast(q, V(0, 0, -1), 0.9)
        if d[0] is None or q.z - d[0].z < 0.035: return False
        for sx in (1, -1):
            h = BVH.ray_cast(q, V(sx, 0, 0), 0.3)
            if h[0] is None or abs(h[0].x - q.x) < 0.024: return False
        u = BVH.ray_cast(q, V(0, 0, 1), 0.9)
        return u[0] is not None and u[0].z - q.z > 0.035
    TR = Mesh(); rail_out = 0; CARS = []
    for c0, c1 in CAN:
        cx = (c0 + c1) / 2
        # the carriage's place: the rail is the arc about hp through it; it must lie inside his canoe over the whole travel
        best = None
        for k0 in (-24.0, -18.0, -12.0):
            for cy in np.arange(yF - 0.02, yF - 0.32, -0.03):
                zr = refz(cx, cy)
                for cz in np.arange(zr - 0.08, zr - 0.6, -0.025):
                    c_ = V(cx, cy, cz); out = sum(1 for a_ in np.linspace(k0, OPEN + 6.0, 25) if not in_canoe(turn(c_, a_)))
                    key = (out, -k0 * 0.0 + (k0 != -24.0) * 0.5, zr - cz)
                    if best is None or key < best[0]: best = (key, c_, k0)
        car, k0 = best[1], best[2]; zb0 = refz(cx, car.y); CARS.append((cx, car, k0, best[0][0]))
        # link: plate from the flap's lower skin down to the carriage; carriage: side plates round the rail, rollers above/below
        box(M, V(cx, car.y, (zb0 + 0.012 + car.z + 0.045) / 2), (V(1, 0, 0), V(0, 1, 0), V(0, 0, 1)), (0.025, 0.12, zb0 + 0.012 - car.z - 0.045), M_TRACK)
        for sx in (-0.032, 0.032): box(M, car + V(sx, 0, 0), (V(1, 0, 0), V(0, 1, 0), V(0, 0, 1)), (0.012, 0.16, 0.12), M_TRACK)
        box(M, car + V(0, 0, 0.051), (V(1, 0, 0), V(0, 1, 0), V(0, 0, 1)), (0.076, 0.16, 0.018), M_TRACK)
        for dy in (-0.05, 0.05):
            for dz in (-0.0455, 0.0455): tube(M, [car + V(-0.026, dy, dz), car + V(0.026, dy, dz)], 0.015, M_PIPE, seg=12)
        # the rail: 40 x 60 mm, an arc about hp through the carriage, from the fixed canoe (bracket on his lower skin) to past full travel
        ang = np.linspace(k0, OPEN + 6.0, 48); rows = []
        for a_ in ang:
            q = turn(car, a_); tg = (turn(car, a_ + 0.5) - turn(car, a_ - 0.5)).normalized(); nz = V(1, 0, 0).cross(tg).normalized()
            rows.append([TR.add(q + V(sx, 0, 0) + nz * sz) for sx, sz in ((-0.02, -0.03), (0.02, -0.03), (0.02, 0.03), (-0.02, 0.03))])
            if not in_canoe(q): rail_out += 1
        TR.grid(rows, M_TRACK, closed=True); TR.face(rows[0][::-1], M_TRACK); TR.face(rows[-1], M_TRACK)
        st = turn(car, k0); top_ = shoot(st, V(0, 0, 1), hi=0.6)
        zt = top_.z if top_ else zb0
        box(TR, V(cx, st.y, (st.z + zt) / 2 + 0.005), (V(1, 0, 0), V(0, 1, 0), V(0, 0, 1)), (0.10, 0.12, zt - st.z + 0.01), M_TRACK)   # track bracket
        # rubber boots where the rail goes through the canoe's bulkhead at yF: one on the fixed side, one on the fairing's side
        ys_ = [turn(car, a_).y for a_ in ang]; kk = next((j for j in range(len(ang) - 1) if (ys_[j] - yF) * (ys_[j + 1] - yF) <= 0), None)
        if kk is not None:
            t_ = (ys_[kk] - yF) / (ys_[kk] - ys_[kk + 1]); a_ = ang[kk] + (ang[kk + 1] - ang[kk]) * t_; q = turn(car, a_)
            tg = (turn(car, a_ + 0.5) - turn(car, a_ - 0.5)).normalized(); hz = 0.03 / max(abs(tg.y), 0.3) + 0.009; hx = 0.029
            for MM, dy in ((TR, 0.008), (M, -0.008)):
                ring_ = [V(cx + hx * math.cos(t) / max(abs(math.cos(t)), abs(math.sin(t))) , yF + dy, q.z + hz * math.sin(t) / max(abs(math.cos(t)), abs(math.sin(t)))) for t in np.linspace(0, 2 * math.pi, 33)[:-1]]
                tube(MM, ring_ + ring_[:1], 0.008, M_BOOT, seg=8, cap=False)
        REPORT.setdefault('flap_boot', []).append(kk is not None)
    REPORT['flap_carriages'] = [[round(cx, 3), [round(c, 3) for c in car], k0, o_] for cx, car, k0, o_ in CARS]
    REPORT['flap_rail_points_outside_canoe'] = rail_out
    ob = finish(M, name, hp)
    PARTS[name] = (ob, ('rot', hp, V(1, 0, 0), OPEN))
    # the cove: shroud lip under the top skin, ceiling, front wall, floor, end ribs; lips under his bottom skin
    F = Mesh(); crow = []
    for x, top, bot in [min(secs, key=lambda q, x=x: abs(q[0] - x)) for x in np.linspace(a0 - 0.008, a1 + 0.008, 60)]:
        zt, zb = top[0][1], bot[0][1]; h = (zt - zb) / 2; b = min(0.10, 0.6 * h); yW = yF + b + 0.03
        prof = [(yF + 0.004 + 0.02, zt - 0.004), (yF + 0.004, zt - 0.004), (yF + 0.004, zt - 0.03), (yW, zt - 0.03), (yW, zb + 0.03), (yF + 0.004, zb + 0.03),
                (yF + 0.004, zb + 0.004), (yF + 0.004 + 0.02, zb + 0.004)]
        crow.append([F.add(V(x, y, z)) for y, z in prof])
    F.grid(crow, M_COVE)
    for row in (crow[0], crow[-1]): F.face(row[2:6], M_COVE)
    for x, row in ((a0 - 0.008, crow[0]), (a1 + 0.008, crow[-1])):        # end ribs of the cove behind the flap ends, from the front wall to the trailing edge
        _, top, bot = min(secs, key=lambda q: abs(q[0] - x))
        if len(top) < 3: continue
        ids = [F.add(V(x, y, z - 0.004)) for y, z in top] + [F.add(V(x, y, z + 0.004)) for y, z in bot[::-1]]
        ids += [row[k] for k in (6, 5, 4, 3, 2, 1)] + ids[:1]     # joined to the cove's end row: one closed piece
        c = F.add(sum((F.v[k] for k in ids), Vector()) / len(ids))
        for j in range(len(ids) - 1): F.face([ids[j], ids[j + 1], c], M_COVE)
    STATIC.append(F.obj('FlapInner_R_Cove'))
    # the aft canoes (behind the flap's front line) go down with the flap, cut cleanly at yF and closed with a bulkhead each side
    for i, (c0, c1) in enumerate(CAN):
        cx = (c0 + c1) / 2; zb0 = float(stn(cx)[3][0])
        def fband(P, c0=c0, c1=c1):
            return (P[:, 0] > c0 - 0.08) & (P[:, 0] < c1 + 0.08) & (P[:, 2] > 1.9) & (P[:, 2] < 3.6) & (P[:, 1] < yF + 0.3) & (P[:, 1] > -15.5)
        def fsdf(P):      # <0: his canoe aft of yF, more than 4 mm under his lower skin
            rz = np.array([refz(p[0], p[1]) for p in P]) if len(P) else np.zeros(0)
            return np.maximum(P[:, 1] - yF, P[:, 2] - (rz - 0.004))
        fo = cut_hole(fsdf, fband, name + '_Fairing%d' % i, keep=name + '_Fairing%d' % i,
                      cap=(V(cx, yF, zb0 - 0.06), V(0, -1, 0)))
        if fo is not None:
            PARTS[fo.name] = (fo, ('rot', hp, V(1, 0, 0), OPEN))
    STATIC.append(TR.obj('FlapInner_R_Tracks'))
    # his wing in the flap: everything between his top and lower skin of the flap's stations, from yF to the trailing edge
    def sdf(P):
        qx = np.abs(P[:, 0] - (a0 + a1) / 2) - (a1 - a0) / 2; qy = P[:, 1] - yF
        return np.maximum(qx, qy) - 0.002
    def band(P):
        ok = (P[:, 0] > a0 - 0.3) & (P[:, 0] < a1 + 0.3) & (P[:, 1] < yF + 0.3) & (P[:, 1] > -14.5)
        J = np.nonzero(ok)[0]; Q = P[J]; idx = np.clip(np.searchsorted(SX, Q[:, 0]), 0, len(SX) - 1); r_ = np.zeros(len(J), bool)
        order = np.argsort(idx, kind='stable'); ks, starts = np.unique(idx[order], return_index=True); ends = list(starts[1:]) + [len(order)]
        for k, a_, b_ in zip(ks, starts, ends):
            m = order[a_:b_]; _, ty_, tz_, bz_ = STN[k]; y = Q[m, 1]
            t = np.interp(-y, -ty_, tz_); b = np.interp(-y, -ty_, bz_)
            r_[m] = (Q[m, 2] < t + 0.03) & (Q[m, 2] > b - 0.03) & (y > ty_[-1] - 0.015)
        ok[J] = r_
        return ok
    cut_hole(sdf, band, name, refine=0.025, islands=((a0 - 0.45, -15.0, 2.3), (a1 + 0.3, yF + 0.45, 4.2)))
    return ob

# ================================================================== main wheels: clean tyres and rims in his place and size
TEXR = 'Art/PushbackTug/Textures/Rubber004/Rubber004_2K-JPG'
M_TYRE = pmat('A320C_Tyre', (0.55, 0.55, 0.55), 0.0, 0.85, tex=TEXR, scale=4.0)
M_RIM = pmat('A320C_Rim', (0.72, 0.73, 0.75), 0.9, 0.32)
M_BOLT = pmat('A320C_Bolt', (0.55, 0.56, 0.58), 1.0, 0.35)
M_BRAKE = pmat('A320C_CarbonBrake', (0.07, 0.07, 0.075), 0.2, 0.7)
M_TEXT = pmat('A320C_TyreText', (0.10, 0.10, 0.10), 0.0, 0.6)
def lathe(M, prof, c, ax, mat, seg=144, uvs=True):     # prof: (axial, radius)
    a = ax.cross(V(0, 0, 1)).normalized() if abs(ax.z) < 0.9 else V(1, 0, 0); b = ax.cross(a)
    rows = []; uv = []; L = 0.0
    for i, (x, r) in enumerate(prof):
        if i: L += math.hypot(x - prof[i - 1][0], r - prof[i - 1][1])
        rows.append([M.add(c + ax * x + (a * math.cos(2 * math.pi * k / seg) + b * math.sin(2 * math.pi * k / seg)) * r) for k in range(seg)])
        uv.append([(k / seg * 3.0, L) for k in range(seg)])
    for i in range(len(rows) - 1):
        for k in range(seg):
            k2 = (k + 1) % seg; M.face([rows[i][k], rows[i][k2], rows[i + 1][k2], rows[i + 1][k]], mat, [uv[i][k], uv[i][k2] if k2 else (3.0, uv[i][0][1]), uv[i + 1][k2] if k2 else (3.0, uv[i + 1][0][1]), uv[i + 1][k]])
    return a, b
def text_on_side(M, s_txt, c, ax, side, r_mid, x_at, size=0.032, a0=0.0):
    cu = bpy.data.curves.new('t', 'FONT'); cu.body = s_txt; cu.size = size; cu.align_x = 'CENTER'; cu.align_y = 'CENTER'; cu.extrude = 0.0
    to = bpy.data.objects.new('t', cu); bpy.context.scene.collection.objects.link(to)
    dg = bpy.context.evaluated_depsgraph_get(); me = to.evaluated_get(dg).to_mesh()
    a = ax.cross(V(0, 0, 1)).normalized(); b = ax.cross(a); base = len(M.v)
    for vv in me.vertices:
        ang = a0 + side * vv.co.x / r_mid; r = r_mid + vv.co.y
        M.add(c + ax * (x_at(r) + side * 0.0015) + (a * math.cos(ang) + b * math.sin(ang)) * r)
    for p in me.polygons: M.face([base + i for i in (list(p.vertices) if side > 0 else list(p.vertices)[::-1])], M_TEXT)
    to.to_mesh_clear(); bpy.data.objects.remove(to); bpy.data.curves.remove(cu)
def build_wheels(sd):
    w = bpy.data.objects['MainWheels' + sd]; leg = bpy.data.objects['MainLeg' + sd]
    W = w.matrix_world; P = np.array([(W @ v.co)[:] for v in w.data.vertices])
    axle = {'_L': V(-3.8784, -12.8034, 0.6463), '_R': V(3.8587, -12.8025, 0.6475)}[sd]
    r = np.hypot(P[:, 1] - axle.y, P[:, 2] - axle.z); R = float(np.percentile(r, 99.7))
    LW = REPORT.get('wheels', {}).get('_L') if sd == '_R' else None      # _R: the same wheels as _L, mirrored about its own axle
    AXL = V(-3.8784, -12.8034, 0.6463)
    if LW: R = LW[0]['tyre_radius']
    tyre = P[r > 0.55]; mid = axle.x
    groups = [tyre[tyre[:, 0] < mid], tyre[tyre[:, 0] >= mid]]
    M = Mesh(); B = Mesh(); ax = V(1, 0, 0); info = []
    k = R / 0.585          # his tyre is 1.1 x the real 46x17R20 (R 0.585 m): rim sizes scaled the same, his size kept
    rb, rf = 0.254 * k, 0.29 * k
    for gi, G in enumerate(groups):
        lo, hi = float(np.percentile(G[:, 0], 0.5)), float(np.percentile(G[:, 0], 99.5)); xc = (lo + hi) / 2; hw = (hi - lo) / 2
        if LW: Lg = LW[1 - gi]; xc = axle.x - (Lg['centre'][0] - AXL.x); hw = Lg['half_width']
        face = 1.0 if xc > axle.x else -1.0         # the outer face: away from the leg
        c = V(xc, axle.y, axle.z)
        # tyre section: a squarish superellipse (his width and radius), flat tread with 4 straight grooves
        prof = []; rc = (R + rf) / 2; hr = (R - rf) / 2
        for t in np.linspace(-math.pi / 2, math.pi / 2, 40):     # inner side -> outer side over the top
            ct, st = math.cos(t), math.sin(t)
            x = hw * np.sign(st) * abs(st) ** (2 / 3.2); rr_ = rc + hr * abs(ct) ** (2 / 3.2)
            prof.append([x, rr_])
        prof = [(-hw * 0.82, rb), (-hw * 0.9, rf - 0.01)] + [(x, rr_) for x, rr_ in prof] + [(hw * 0.9, rf - 0.01), (hw * 0.82, rb)]
        out = []
        grooves = [g * hw * 0.62 for g in (-0.75, -0.25, 0.25, 0.75)]
        for x, rr_ in prof:
            for gx in grooves:
                if abs(x - gx) < 0.011 and rr_ > R - 0.03: rr_ -= 0.011
            out.append((x, rr_))
        # extra rows at the groove walls so they are crisp
        fine = []
        for i in range(len(out) - 1):
            fine.append(out[i]); x0, x1 = out[i][0], out[i + 1][0]
            for gx in grooves:
                for e in (gx - 0.011, gx + 0.011):
                    if x0 < e < x1 and out[i][1] > R - 0.04:
                        rr_ = float(np.interp(e, [x0, x1], [out[i][1], out[i + 1][1]]))
                        fine += [(e - 0.0005, rr_ + (0.011 if abs(e - 0.0005 - gx) > 0.011 else 0)), (e + 0.0005, rr_ + (0.011 if abs(e + 0.0005 - gx) > 0.011 else 0))]
        fine.append(out[-1]); fine.sort(key=lambda q: q[0])
        lathe(M, fine, c, ax, M_TYRE, seg=160)
        # size text on both sidewalls, plain
        def x_at(rr_, side):
            pts = [(abs(x), q) for x, q in fine if (x > 0) == (side > 0) and q > rf]
            pts.sort(key=lambda q: q[1]); return side * float(np.interp(rr_, [q[1] for q in pts], [q[0] for q in pts]))
        rm = rf + (R - rf) * 0.42
        for side in (1, -1):
            for a0 in (0.6, 0.6 + math.pi):
                text_on_side(M, '46x17R20   30 PR   TUBELESS', c, ax, side, rm, lambda rr_, s_=side: x_at(rr_, s_), a0=a0)
        # rim: flanges, barrel, the outer half's face with 16 bolts and the hub cap; the leg side open (brake inside)
        fx = hw * 0.82
        rim = [(-fx - 0.01, rf), (-fx, rf + 0.004), (-fx + 0.012, rf - 0.012), (-fx + 0.02, rb), (fx - 0.02, rb), (fx - 0.012, rf - 0.012), (fx, rf + 0.004), (fx + 0.01, rf),
               (fx + 0.004, rb - 0.02)]
        rim = [(x * face, q) for x, q in rim]
        if face < 0: rim = rim[::-1]
        lathe(M, rim, c, ax, M_RIM, seg=96)
        fz = face * (fx - 0.06)
        disc = [(fz, rb - 0.015), (fz + face * 0.01, 0.20 * k), (fz + face * 0.005, 0.16 * k), (fz - face * 0.01, 0.13 * k), (fz, 0.085 * k), (fz + face * 0.03, 0.075 * k),
                (fz + face * 0.05, 0.05 * k), (fz + face * 0.058, 0.0)]
        if face < 0: disc = disc[::-1]
        lathe(M, disc, c, ax, M_RIM, seg=96)
        a_ = ax.cross(V(0, 0, 1)).normalized(); b_ = ax.cross(a_)
        for i in range(16):
            an = 2 * math.pi * i / 16; p = c + ax * (fz + face * 0.004) + (a_ * math.cos(an) + b_ * math.sin(an)) * (0.175 * k)
            hexa = [(face * 0.0, 0.014), (face * 0.022, 0.014), (face * 0.026, 0.0)]
            if face < 0: hexa = hexa[::-1]
            lathe(M, [(x, q) for x, q in hexa], p, ax, M_BOLT, seg=6)
        # carbon brake stack (does not roll: on the leg) inside the rim on the leg side, 7 piston housings
        bz0 = -face * (fx - 0.03); bz1 = -face * 0.05
        lathe(B, [(bz0, 0.0), (bz0, rb - 0.03), (bz1, rb - 0.03), (bz1, 0.0)], c, ax, M_BRAKE, seg=64)
        for i in range(7):
            an = 2 * math.pi * i / 7; p = c + ax * (bz0 - face * 0.0) + (a_ * math.cos(an) + b_ * math.sin(an)) * (0.17 * k)
            tube(B, [p, p - ax * face * 0.05], 0.03, M_BOLT, seg=12)
        info.append(dict(centre=[round(xc, 4), round(axle.y, 4), round(axle.z, 4)], tyre_radius=round(R, 4), half_width=round(hw, 4), outer_face=face))
    # his wheel mesh is replaced by the clean one in the same place (object, name and pivot kept)
    xs_ = [q['centre'][0] for q in info]; tube(B, [V(min(xs_) - 0.3, axle.y, axle.z), V(max(xs_) + 0.3, axle.y, axle.z)], 0.07, M_BOLT, seg=24)   # the axle
    inv = W.inverted(); ob_new = M.obj('MW_tmp')
    ob_new.data.transform(inv); w.data = ob_new.data; bpy.data.objects.remove(ob_new)
    w.data.name = 'MainWheels' + sd
    bo = B.obj('MainBrakes' + sd)        # joined into the leg (moves with it, does not roll)
    bo.data.transform(leg.matrix_world.inverted())
    bpy.ops.object.select_all(action='DESELECT'); bo.matrix_world = leg.matrix_world; bo.select_set(True); leg.select_set(True)
    bpy.context.view_layer.objects.active = leg; bpy.ops.object.join()
    REPORT.setdefault('wheels', {})[sd] = info

# ================================================================== pictures
def setup_render():
    sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'; sc.render.resolution_x, sc.render.resolution_y = 3840, 2160
    sc.eevee.taa_render_samples = 48; sc.eevee.use_gtao = True; sc.eevee.use_ssr = True; sc.eevee.use_soft_shadows = True
    sc.eevee.shadow_cube_size = '2048'; sc.eevee.shadow_cascade_size = '4096'
    w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.42, 0.5, 0.62, 1)
    w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.9
    sun = bpy.data.lights.new('sun', 'SUN'); sun.energy = 3.5; sun.angle = math.radians(2); so = bpy.data.objects.new('sun', sun)
    so.rotation_euler = (math.radians(50), math.radians(10), math.radians(140)); sc.collection.objects.link(so)
    gm = pmat('ground', (0.33, 0.33, 0.32), 0.0, 0.9); bpy.ops.mesh.primitive_plane_add(size=300, location=(0, -10, 0.0)); bpy.context.object.data.materials.append(gm)
    cam = bpy.data.cameras.new('cam'); co_ = bpy.data.objects.new('cam', cam); sc.collection.objects.link(co_); sc.camera = co_
    sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'
    if 'MeshySrc' in bpy.data.objects: bpy.data.objects['MeshySrc'].hide_render = True
    return co_
VIEWS = dict(cowl=((10.2, -4.6, 3.1), (5.6, -7.9, 1.9), 30), flap=((7.6, -18.4, 6.4), (4.3, -13.2, 3.4), 32),
             door=((-7.4, -0.6, 2.9), (-2.0, -2.8, 3.0), 32), wheels=((-7.2, -10.3, 1.1), (-3.9, -12.8, 0.62), 40))
OPENVIEWS = dict(door_side=((-4.6, 1.2, 2.0), (-1.9, -2.8, 3.0), 30), door_edge=((-3.3, -1.5, 2.7), (-1.9, -2.9, 2.95), 45),
                 flap_under=((7.8, -16.6, 1.3), (4.3, -13.4, 3.0), 30), flap_side=((8.6, -13.3, 3.0), (4.3, -13.4, 3.1), 30),
                 flap_edge=((5.6, -15.2, 2.6), (4.3, -13.2, 3.0), 45))
def shot(cam, view, path):
    pos, tgt, lens = VIEWS[view]; cam.location = pos; cam.data.lens = lens; cam.data.clip_start = 0.05
    cam.rotation_euler = (Vector(tgt) - Vector(pos)).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.render.filepath = path; bpy.ops.render.render(write_still=True)

os.makedirs(PIC, exist_ok=True); os.makedirs(os.path.join(HERE, 'Textures'), exist_ok=True)
if STAGE == 'before':
    cam = setup_render()
    for v in (ONLY.split(',') if ONLY else VIEWS): shot(cam, v, os.path.join(PIC, 'A320-clean-%s-BEFORE.png' % v))
    raise SystemExit
if os.environ.get('ADP_PROBE'):
    exec(open(os.environ['ADP_PROBE']).read()); raise SystemExit
built = {}
for nm, fn in (('cowl', build_cowl), ('door', build_door), ('flap', build_flap)):
    if ONLY and nm not in ONLY.split(','): continue
    import time; _t = time.time(); built[nm] = fn(); print('BUILT', nm, round(time.time() - _t), REPORT.get('refined'), flush=True)
for sd in ('_L', '_R'): build_wheels(sd)
for nm, ob in built.items():
    measure(ob, ob.name, 'A320C_%s_Skin' % ob.name)
    if os.environ.get('ADP_NOBAKE') != '1': bake(ob, *SKINS[ob.name]); print('BAKED', nm, flush=True)
REPORT['note'] = 'closed check: distance of each part outer skin (incl. its 4 mm rolled edge) to his ORIGINAL surface, before the cut, in mm'
json.dump(REPORT, open(os.path.join(HERE, 'a320-clean-check.json'), 'w'), indent=1)
bpy.data.objects.remove(src)
for ob in bpy.data.objects:
    if ob.get('open_only'): ob.hide_render = True
if os.environ.get('ADP_NOSAVE') != '1': bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'A320-clean.blend'))
if os.environ.get('ADP_GLB'):
    bpy.ops.object.select_all(action='DESELECT')
    for ob in bpy.data.objects:
        if ob.type == 'MESH' and any(ob.name.startswith(k) for k in os.environ.get('ADP_GLB_PARTS', 'CargoDoorFwd,FlapInner_R').split(',')): ob.select_set(True)
    _exp(filepath=os.environ['ADP_GLB'], export_format='GLB', use_selection=True, export_apply=True)
if os.environ.get('ADP_RENDER', '1') == '1':
    cam = setup_render()
    for v in [x for x in VIEWS if x == 'wheels' or x in built]: shot(cam, v, os.path.join(PIC, 'A320-clean-%s-AFTER%s.png' % (v, '-closed' if v != 'wheels' else '')))
    for nm, (ob, (_, p, axis, deg)) in PARTS.items():
        ob.matrix_world = Matrix.Translation(p) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-p) @ ob.matrix_world
    for ob in bpy.data.objects:
        if ob.get('open_only'): ob.hide_render = False
    for v in [x for x in VIEWS if x in built]: shot(cam, v, os.path.join(PIC, 'A320-clean-%s-AFTER-open.png' % v))
    VIEWS.update(OPENVIEWS)
    for v in [x for x in OPENVIEWS if x.split('_')[0] in built]: shot(cam, v, os.path.join(PIC, 'A320-clean-%s-AFTER-open.png' % v))
print('DONE', json.dumps(REPORT)[:1500])
