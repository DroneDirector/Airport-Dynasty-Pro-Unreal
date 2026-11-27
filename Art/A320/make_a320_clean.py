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
M_PAINT = pmat('A320C_CanoePaint', (0.80, 0.81, 0.83), 0.0, 0.45)   # canoe bulkheads: his paint
M_FLAPPAINT = pmat('A320C_FlapPaint', (0.80, 0.81, 0.83), 0.0, 0.45)   # flap lower skin where it is not on his surface
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

def cut_hole(sdf, band, label, refine=0.0, keep=None, cap=None, islands=None, inside=None):
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
        REPORT.setdefault('kept_loose_removed', {})[keep] = _keep_main(b2)
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
            if ok and not (len(islands) > 2 and islands[2](comp)): drop += comp
        bmesh.ops.delete(bm, geom=drop, context='FACES')
        REPORT.setdefault('loose_bits_removed', {})[label] = len(drop)
    if inside:        # slivers of his left in the part's place (faces partly outside the band): they go
        lo, hi = Vector(inside[0]), Vector(inside[1])
        cand = [f_ for f_ in bm.faces if all(lo[k] < f_.verts[0].co[k] < hi[k] for k in range(3))]
        ins = inside[2](np.array([f_.calc_center_median()[:] for f_ in cand])) if cand else []
        sl = [f_ for f_, k in zip(cand, ins) if k]
        bmesh.ops.delete(bm, geom=sl, context='FACES'); REPORT.setdefault('slivers_removed', {})[label] = len(sl)
    if cap: _cap(bm, cap[0], cap[1])
    bm.transform(MW.inverted()); bm.to_mesh(o.data); bm.free()
    REPORT.setdefault('cuts', {})[label] = dict(faces_removed=len(gone), faces_split=nsplit)
    return part
def _cap(bm, c, nrm):     # close the open cut line (edges between cut vertices with one face) with a clean flat fill
    c = Vector(c); sel = []
    for e in list(bm.edges):
        a, b = e.verts
        if len(e.link_faces) != 1 or abs((a.co - c).dot(nrm)) > 3e-4 or abs((b.co - c).dot(nrm)) > 3e-4: continue
        if (a.co - c).length > 0.6: continue
        sel.append(e)
    deg = {}
    for e in sel:
        for v in e.verts: deg[v] = deg.get(v, 0) + 1
    ends = [v for v, k in deg.items() if k == 1]
    while len(ends) >= 2:      # the cut line is open where it meets his skin: closed straight across before the fill
        a = ends.pop(0); b = min(ends, key=lambda q: (q.co - a.co).length); ends.remove(b)
        try: sel.append(bm.edges.new((a, b)))
        except Exception: pass
    # a constrained fill of the cut outline (no fan to a centre: a fan makes long dark spikes where the outline is not convex)
    r = bmesh.ops.triangle_fill(bm, use_beauty=True, use_dissolve=False, edges=sel, normal=Vector(nrm)) if sel else {'geom': []}
    nf = 0
    for f in r['geom']:
        if not isinstance(f, bmesh.types.BMFace): continue
        f.normal_update()
        if f.normal.dot(nrm) < 0: f.normal_flip()
        f.smooth = False; nf += 1
    REPORT.setdefault('caps', []).append(nf)
def _keep_main(bm, frac=0.02):   # loose bits of a kept part (smaller than frac of its faces) go: one solid piece
    bm.faces.ensure_lookup_table(); seen = set(); comps = []
    for f in bm.faces:
        if f in seen: continue
        st = [f]; seen.add(f); comp = []
        while st:
            g = st.pop(); comp.append(g)
            for v in g.verts:
                for h in v.link_faces:
                    if h not in seen: seen.add(h); st.append(h)
        comps.append(comp)
    big = max(len(c_) for c_ in comps) if comps else 0
    drop = [g for c_ in comps if len(c_) < frac * big for g in c_]
    if drop: bmesh.ops.delete(bm, geom=drop, context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    return len(comps) - 1

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
    if kd is not None: REPORT.setdefault('left_out_of_check', {})[label] = dict(hidden_in_canoe=kinds.get(1, 0), over_his_hole_or_bump=kinds.get(2, 0), against_flap_underside=kinds.get(3, 0))
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

def clean_marks(im, r=10, k=0.84):
    """round 4 (owner 11-27: smudged dark marks on the flap's upper skin): the scan's dark creases and bake marks are painted
    over with the paint round them - every pixel much darker than its neighbourhood takes the neighbourhood's colour"""
    w, h = im.size; A = np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4); ok = (A[..., 3] > 0.5).astype(np.float32)
    def blur(X):
        for ax in (0, 1):
            c = np.cumsum(np.pad(X, [(r + 1, r) if a_ == ax else (0, 0) for a_ in range(X.ndim)], mode='edge'), axis=ax)
            X = (np.take(c, range(2 * r + 1, c.shape[ax]), axis=ax) - np.take(c, range(0, c.shape[ax] - 2 * r - 1), axis=ax)) / (2 * r + 1)
        return X
    n_ = 0
    for _ in range(2):
        lum = A[..., :3] @ np.array([0.2126, 0.7152, 0.0722], np.float32)
        wsum = np.maximum(blur(ok), 1e-4); bl = blur(lum * ok) / wsum
        bad = (ok > 0) & (lum < bl * k)
        good = ok * (~bad); gs = np.maximum(blur(good), 1e-4)
        for c_ in range(3):
            m_ = blur(A[..., c_] * good) / gs; A[..., c_] = np.where(bad, m_, A[..., c_])
        n_ += int(bad.sum())
    im.pixels[:] = A.ravel(); REPORT.setdefault('flap_marks_cleaned_px', []).append(n_)
def bake(ob, m, ims):
    sc = bpy.context.scene; sc.render.engine = 'CYCLES'; sc.cycles.samples = 1; sc.cycles.device = 'CPU'
    dummy = bpy.data.images.get('A320C_dummy') or bpy.data.images.new('A320C_dummy', 16, 16)
    extra = []
    for mm in ob.data.materials:
        if mm == m or mm.get('share'): continue
        t = mm.node_tree.nodes.new('ShaderNodeTexImage'); t.image = dummy; mm.node_tree.nodes.active = t; extra.append((mm, t))
    sm = src.data.materials[0]; N = sm.node_tree.nodes; L = sm.node_tree.links; b = N['Principled BSDF']
    tex = {n.image.name: n for n in N if n.type == 'TEX_IMAGE'}
    bpy.ops.object.select_all(action='DESELECT'); src.hide_render = False; src.select_set(True); ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    b.inputs['Emission Strength'].default_value = 1.0
    for k, im in zip(('Color', 'ORM', 'Normal'), ims):
        m.node_tree.nodes.active = m.node_tree.nodes[k]
        for mm in ob.data.materials:
            if mm.get('share'): mm.node_tree.nodes.active = mm.node_tree.nodes[k]
        if k != 'Normal':
            ln = L.new(tex['Image_0' if k == 'Color' else 'Image_1'].outputs['Color'], b.inputs['Emission'])
            bpy.ops.object.bake(type='EMIT', use_selected_to_active=True, cage_extrusion=0.02, max_ray_distance=0.05, margin=8)
            L.remove(ln)
            if k == 'Color' and ob.name.startswith('FlapInner'): clean_marks(im)
        else:
            bpy.ops.object.bake(type='NORMAL', normal_space='TANGENT', use_selected_to_active=True, cage_extrusion=0.02, max_ray_distance=0.05, margin=8)
        im.filepath_raw = os.path.join(HERE, 'Textures', im.name + '.png'); im.file_format = 'PNG'; im.save()
    b.inputs['Emission Strength'].default_value = 0.0
    b.inputs['Emission'].default_value = (0, 0, 0, 1)
    for mm, t in extra: mm.node_tree.nodes.remove(t)

SKINS = {}
def finish(M, name, pivot):
    m, ims = skin_mat(name); SKINS[name] = (m, ims)
    if 'CANOE' in M.m: m2 = M_PAINT.copy(); m2.name = 'A320C_%s_CanoeSkin' % name     # clean paint like the fixed canoe (owner 11-27: no scan texture), own check
    M.m = [m if x == 'SKIN' else m2 if x == 'CANOE' else x for x in M.m]
    ob = M.obj(name)
    me = ob.data; me.transform(Matrix.Translation(-Vector(pivot))); ob.location = pivot
    bpy.context.view_layer.update()      # matrix_world right away (the check and the open pose read it)
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
            M.face(q[::-1], 'SKIN' if j < nT + nB - 1 else M_PAINT, u[::-1])     # the nose (hidden closed, seen open): painted like the skin
    for row, fl in ((rows[0], False), (rows[-1], True)):      # end ribs: top joined straight down to bottom (a fan to one centre made dark wedges)
        def ef(q): M.face(q[::-1] if fl else q, M_FLAPPAINT)   # end ribs painted like the skin (owner 11-27: no dark wedges)
        for j in range(NL - 2): ef([row[j], row[j + 1], row[2 * NL - 3 - j], row[2 * NL - 2 - j]])
        ef([row[NL - 2], row[NL - 1], row[NL]])
        fr_ = row[2 * NL - 2:] + [row[0]]; c = M.add(sum((M.v[k] for k in fr_), Vector()) / len(fr_))
        for j in range(len(fr_) - 1): ef([fr_[j], fr_[j + 1], c])
        ef([fr_[-1], fr_[0], c])
    # lower-skin faces not on his surface (over the canoe / his hole): the bake finds no skin of his there (dark smears), so
    # they get the clean paint instead; closed they are inside the canoe or under the flap
    if SK:
        kd_ = mathutils.kdtree.KDTree(len(SK))
        for k_, (q_, _) in enumerate(SK): kd_.insert(q_, k_)
        kd_.balance(); npaint = 0
        for fi, f_ in enumerate(M.f):
            if M.m[fi] == 'SKIN' and any(kd_.find(M.v[k_])[2] < 0.002 for k_ in f_): M.m[fi] = M_FLAPPAINT; npaint += 1
        REPORT['flap_lower_faces_painted'] = npaint
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
    TR = Mesh(); FX = Mesh(); rail_out = 0; CARS = []; EX = V(1, 0, 0)
    def prism(MM, prof, ey, ez, c, x0, x1, mat):      # a 2-D outline [(a, b)] in the plane (ey, ez) at c, solid from x0 to x1 along x
        r0 = [MM.add(c + EX * x0 + ey * a + ez * b) for a, b in prof]; r1 = [MM.add(c + EX * x1 + ey * a + ez * b) for a, b in prof]
        n = len(prof)
        for j in range(n): MM.face([r0[j], r0[(j + 1) % n], r1[(j + 1) % n], r1[j]], mat)
        MM.face(r0[::-1], mat); MM.face(r1, mat)
    def rrect(u0, u1, v0, v1, r, n=6): return [(p[0], p[1]) for p in rr(u0, u1, v0, v1, r, 0.0, 1, 1, n)]
    def arc(cy, cz, r, a0_, a1_, n): return [(cy + r * math.cos(math.radians(a)), cz + r * math.sin(math.radians(a))) for a in np.linspace(a0_, a1_, n)]
    # I-section rail (flanges 44 x 7 mm, web 10 mm, 60 mm high) in the plane (x, normal of the arc)
    PI_ = [(-0.022, -0.03), (0.022, -0.03), (0.022, -0.023), (0.005, -0.023), (0.005, 0.023), (0.022, 0.023), (0.022, 0.03),
           (-0.022, 0.03), (-0.022, 0.023), (-0.005, 0.023), (-0.005, -0.023), (-0.022, -0.023)]
    Ycut = -13.10           # his canoe is cut here: ahead of it fixed (his, with the gear door), behind it the new modelled rear canoe moves with the flap
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
        tg = (turn(car, 0.5) - turn(car, -0.5)).normalized(); nz = EX.cross(tg).normalized(); up = nz if nz.z > 0 else -nz
        # carriage (moves with the flap): 2 shaped side plates, 4 rollers on the rail flanges with axles and nuts, top bridge,
        # clevis lugs and pin; the link (tapered plate with an eye) from the pin up into the flap's lower skin
        for sx in (-1, 1):
            prism(M, rrect(-0.085, 0.085, -0.068, 0.068, 0.022), tg, up, car, sx * 0.028, sx * 0.036, M_TRACK)
            prism(M, [(-0.035, 0.074), (0.035, 0.074)] + arc(0, 0.105, 0.035, 0, 180, 13), tg, up, car, sx * 0.011, sx * 0.021, M_TRACK)
            tube(M, [car + up * 0.105 + EX * sx * 0.021, car + up * 0.105 + EX * sx * 0.026], 0.013, M_SILL, seg=14)
        prism(M, rrect(-0.085, 0.085, 0.0675, 0.077, 0.004), tg, up, car, -0.036, 0.036, M_TRACK)
        tube(M, [car + up * 0.105 - EX * 0.027, car + up * 0.105 + EX * 0.027], 0.009, M_SILL, seg=14)
        for dt in (-0.05, 0.05):
            for sn in (-1, 1):
                pc = car + tg * dt + up * sn * (0.03 + 0.0145)
                tube(M, [pc - EX * 0.018, pc + EX * 0.018], 0.014, M_TRACK, seg=20)
                tube(M, [pc - EX * 0.040, pc + EX * 0.040], 0.005, M_SILL, seg=10)
                for sx in (-1, 1): tube(M, [pc + EX * sx * 0.036, pc + EX * sx * 0.043], 0.008, M_SILL, seg=6)
        pin = car + up * 0.105; ztop_l = zb0 + 0.006 - pin.z
        prism(M, arc(0, 0, 0.028, 180, 360, 13) + [(0.055, ztop_l), (-0.055, ztop_l)], V(0, 1, 0), V(0, 0, 1), pin, -0.009, 0.009, M_TRACK)
        # the rail: I-section, an arc about hp through the carriage, from the fixed canoe (bracket on his lower skin) to past full travel
        ang = np.linspace(k0, OPEN + 6.0, 48); rows = []
        for a_ in ang:
            q = turn(car, a_); tg_ = (turn(car, a_ + 0.5) - turn(car, a_ - 0.5)).normalized(); n_ = EX.cross(tg_).normalized()
            n_ = n_ if n_.dot(up) > 0 else -n_
            rows.append([TR.add(q + EX * a + n_ * b) for a, b in PI_])
            if not in_canoe(q): rail_out += 1
        TR.grid(rows, M_TRACK, closed=True); TR.face(rows[0][::-1], M_TRACK); TR.face(rows[-1], M_TRACK)
        st = turn(car, k0); top_ = shoot(st, V(0, 0, 1), hi=0.6)
        zt = top_.z if top_ else zb0
        prism(TR, [(-0.06, st.z - 0.03 - st.z), (0.06, st.z - 0.03 - st.z), (0.04, zt + 0.006 - st.z), (-0.04, zt + 0.006 - st.z)], V(0, 1, 0), V(0, 0, 1),
              V(cx, st.y, st.z), -0.03, 0.03, M_TRACK)       # forward bracket up to his lower skin
        en = turn(car, ang[-1])
        # ---- the canoe, modelled anew over its whole length under the flap (his scanned canoe is torn where it would be cut):
        # his canoe shot all round from outside, smoothed, lofted as one clean body, split at Ycut: the front half is fixed
        # (on the wing, holds the rail), the rear half hangs on the flap and turns with it. Both ends closed, a 3.5 mm gap.
        def hit_ok(q, cx=cx, zmax=3.45): return abs(q.x - cx) < 0.2 and q.z < zmax
        ytip = Ycut
        for y in np.arange(-14.3, -15.3, -0.004):
            if any(shoot(V(cx + dx, y, 1.6), V(0, 0, 1), hi=1.85, test=hit_ok) is not None for dx in (-0.04, 0.0, 0.04)): ytip = y
            else: break
        TEy = float(stn(cx)[1][-1]); NA = 48; yA = -11.80     # round 4: the new body reaches his canoe's nose (no seam to his scanned front)
        def lowskin(y):          # his wing's lower skin beside the canoe (ahead of the flap)
            zs = [h.z for h in (shoot(V(cx + dx, y, 1.6), V(0, 0, 1), hi=2.0, test=lambda q: q.z > 2.5) for dx in (-0.26, 0.26)) if h is not None]
            return float(np.mean(zs)) if zs else refz(cx, yF - 0.01)
        YS = np.r_[np.linspace(yA, yF + 0.002, 24), np.linspace(yF - 0.004, Ycut + 0.0035, 22), np.linspace(Ycut, ytip, 60)]
        NS = len(YS); NF = 46                 # stations 0..NF-1 fixed, NF.. moving
        ZT = []; ZB = []; ZTOP = []; ZB2 = []
        for y in YS:
            if y > yF: ls = lowskin(y); t_ = ls - 0.004; tt = ls + 0.02     # ahead of the flap: the walls go on up into his wing
            elif y > TEy + 0.004: t_ = refz(cx, y) - 0.003; tt = t_      # under the flap: a flat top 3 mm under its lower skin
            else:
                h = shoot(V(cx, y, 3.8), V(0, 0, -1), hi=1.6, test=hit_ok); t_ = h.z if h else np.nan; tt = t_
            h = shoot(V(cx, y, 1.6), V(0, 0, 1), hi=1.85, test=hit_ok); ZT.append(t_); ZB.append(h.z if h else np.nan); ZTOP.append(tt)
            h = shoot(V(cx + 0.10, y, 1.6), V(0, 0, 1), hi=1.85, test=hit_ok); ZB2.append(h.z if h else np.nan)
        # his gear-leg door (a thin plate, x cx-0.11..cx+0.06) hangs from the canoe's front half down to z 1.9: the bottom line there
        # comes from the clean side (cx+0.10) plus the offset measured behind the plate
        ZB = np.array(ZB, float); ZB2 = np.array(ZB2, float); YP = Ycut - 0.12
        aft = (YS < YP - 0.03) & (YS > YP - 0.40) & np.isfinite(ZB) & np.isfinite(ZB2)
        d0 = float(np.median(ZB[aft] - ZB2[aft])) if aft.any() else 0.0
        fr = YS > YP; ZB[fr] = ZB2[fr] + d0
        def fillsm(a, k=5):
            a = np.array(a, float); ok_ = np.isfinite(a); a = np.interp(np.arange(len(a)), np.nonzero(ok_)[0], a[ok_])
            p_ = np.pad(a, k // 2, mode='edge'); return np.convolve(p_, np.ones(k) / k, mode='valid')
        ZB = fillsm(fillsm(ZB, 9), 9); ZT = np.array(ZT, float)
        ZT[NF + 1:] = fillsm(ZT[NF + 1:])
        ZTOP = np.array(ZTOP, float); ZTOP = np.where(np.isfinite(ZTOP), ZTOP, ZT); ZC = (ZT + ZB) / 2
        R = np.full((NS, NA), np.nan); TH = 2 * np.pi * np.arange(NA) / NA
        for i, y in enumerate(YS):
            c = V(cx, y, ZC[i])
            for j, th in enumerate(TH):
                d = V(math.cos(th), 0, math.sin(th))
                h = shoot(c + d * 0.45, -d, hi=0.45, test=lambda q, zz=ZT[i], zb=ZB[i], c=c, d=d: hit_ok(q) and q.z < zz + 0.002 and q.z > zb - 0.012 and (q - c).dot(d) > 0)
                if h is None: continue
                r = (h - c).length
                if d.z > 1e-3: r = min(r, (ZT[i] - ZC[i]) / d.z)
                R[i, j] = r
        for i in range(NS):
            ok_ = np.isfinite(R[i])
            if ok_.sum() >= NA // 3: R[i] = np.interp(TH, np.r_[TH[ok_] - 2 * np.pi, TH[ok_], TH[ok_] + 2 * np.pi], np.r_[R[i, ok_], R[i, ok_], R[i, ok_]])
            else: R[i] = R[i - 1] * 0.7 if i else 0.05
        R0 = R[0].copy()
        R = np.array([np.median(np.pad(R, ((2, 2), (0, 0)), mode='edge')[i:i + 5], axis=0) for i in range(NS)])
        for _ in range(3):
            R = (np.roll(R, 1, 1) + R + np.roll(R, -1, 1)) / 3
            Rp = np.pad(R, ((1, 1), (0, 0)), mode='edge'); R = (Rp[:-2] + Rp[1:-1] + Rp[2:]) / 3
        # a clean lofted body (owner 11-27: no scan lumps): each ring kept to its first 4 harmonics, the harmonics smoothed
        # along the length; then lifted where his canoe would stick out more than 8 mm (his inside it is cut away)
        Rraw = R.copy(); FT = np.fft.rfft(R, axis=1); FT[:, 5:] = 0; R = np.fft.irfft(FT, n=NA, axis=1)
        for _ in range(4):
            Rp = np.pad(R, ((3, 3), (0, 0)), mode='edge'); R = sum(Rp[k:k + NS] for k in range(7)) / 7
        lift = np.maximum((Rraw - R).max(axis=1) - 0.008, 0.0)
        lp = np.pad(lift, 3, mode='edge'); lift = np.max([lp[k:k + NS] for k in range(7)], axis=0)
        lp = np.pad(lift, 3, mode='edge'); lift = sum(lp[k:k + NS] for k in range(7)) / 7
        R = R + lift[:, None]
        REPORT.setdefault('flap_canoe_smooth_mm', []).append(dict(lift_max=round(float(lift.max()) * 1000, 1), dev_max=round(float(np.abs(R - Rraw).max()) * 1000, 1)))
        R = R + 0.003; R[0] = np.maximum(R[0], R0 + 0.002)       # 3 mm over his canoe (it stays inside); his cut ring at yA under the new front
        # round 4 (owner 11-27: the fixed front read as a flat-sided box): every ring of the fixed front, blended into the moving
        # canoe, is one kind of clean section - an elliptic keel, sides bowed out a little, the full top width into the wing /
        # under the flap; his canoe in the body's column goes (csdf), so the section need not follow his boxy corners
        CT = np.cos(TH)[None, :]; ST = np.sin(TH)[None, :]
        WA = fillsm(np.abs(R * CT).max(axis=1), 7); WT = fillsm(np.where(ST > 0.35, np.abs(R * CT), 0).max(axis=1), 7)
        WT = np.minimum(WT, WA); ZBn = fillsm(ZC - R[:, 3 * NA // 4], 7); ZTn = ZC + R[:, NA // 4]
        def polar(poly, zc):      # radius of the closed outline poly [(dx, z)] seen from (0, zc) along every TH
            P_ = np.array(poly); A_ = P_; B_ = np.roll(P_, -1, axis=0); out = np.zeros(NA)
            for j, th in enumerate(TH):
                d = np.array([math.cos(th), math.sin(th)]); best = 0.0
                for (ax, az), (bx, bz) in zip(A_, B_):
                    e = np.array([bx - ax, bz - az]); m_ = np.array([[d[0], -e[0]], [d[1], -e[1]]])
                    if abs(np.linalg.det(m_)) < 1e-12: continue
                    t_, u_ = np.linalg.solve(m_, np.array([ax, az - zc]))
                    if t_ > 0 and -1e-9 <= u_ <= 1 + 1e-9: best = max(best, t_)
                out[j] = best
            return out
        Rn = R.copy()
        for i, y in enumerate(YS):
            Wm = WA[i] + 0.004; Wt = min(WT[i] + 0.003, Wm); zb, zt = ZBn[i], max(ZTn[i], ZBn[i] + 0.05); H = zt - zb
            if y < yF: Wt = min(Wt, 0.80 * Wm)      # under the flap the sides tuck in toward the top: a bowed, rounded side, never a flat wall
            hb = 0.55 * H; free = y <= TEy + 0.004; rt = min(0.8 * Wt, 0.35 * H) if free else 0.0
            right = [(Wm * math.sin(a_), zb + hb * (1 - math.cos(a_))) for a_ in np.linspace(0, math.pi / 2, 14)]
            for t_ in np.linspace(0, 1, 11)[1:]:
                right.append((Wt + (Wm - Wt) * math.cos(math.pi / 2 * t_), zb + hb + t_ * (zt - rt - zb - hb)))
            if free: right += [(Wt - rt + rt * math.cos(a_), zt - rt + rt * math.sin(a_)) for a_ in np.linspace(0, math.pi / 2, 8)[1:]]
            else: right.append((0.0, zt))
            poly = right + [(-x_, z_) for x_, z_ in right[::-1][1:-1]]
            Rn[i] = polar(poly, ZC[i])
        wb = np.clip((NF + 14 - np.arange(NS)) / 12.0, 0.0, 1.0)[:, None]
        R = wb * Rn + (1 - wb) * R
        # the nose: over its first 0.16 m the section rises into his wing (a round blend, no open front)
        yn = yA - 0.16
        for i in range(NS):
            if YS[i] <= yn: break
            sN = max(math.sqrt(max(1 - ((YS[i] - yn) / (yA - yn)) ** 2, 0.0)), 0.04)
            for j, th in enumerate(TH):
                if math.sin(th) < 0: R[i, j] = (ZC[i] - (ZT[i] - (ZT[i] - (ZC[i] + R[i, j] * math.sin(th))) * sN)) / -math.sin(th) if math.sin(th) < -0.05 else R[i, j]
        REPORT.setdefault('flap_canoe_front', []).append(dict(zb=[round(float(ZC[i] - R[i, 3 * NA // 4]), 3) for i in range(0, NF, 4)], zt=[round(float(ZT[i]), 3) for i in range(0, NF, 4)],
                                                              w=[round(float(WA[i]), 3) for i in range(0, NF, 4)]))
        # the tail closes in a smooth round point over its last 0.3 m (no pinch on the last ring)
        yt0 = ytip + 0.30; i0 = max(i for i in range(NS) if YS[i] >= yt0); Rt = R[i0].copy()
        for i in range(i0 + 1, NS):
            t_ = min((yt0 - YS[i]) / max(yt0 - ytip, 1e-3), 1.0); R[i] = np.minimum(R[i], Rt * max(math.sqrt(max(1 - t_ ** 2, 0.0)), 0.03))
        PTS = []; RF = np.zeros_like(R); TOPF = []
        for i, y in enumerate(YS):
            row = []; tf = []
            for j, th in enumerate(TH):
                z = ZC[i] + R[i, j] * math.sin(th)
                if y > TEy + 0.004: z = ZT[i] - 0.018 * float(np.logaddexp(0.0, -(z - ZT[i]) / 0.018))     # a flat top meets the sides in a rounded edge
                top = z >= ZT[i] - (0.0015 if y > TEy + 0.004 else 0.0005)
                p = V(cx + R[i, j] * math.cos(th), y, ZTOP[i] if top else z); row.append(p); tf.append(top)
                RF[i, j] = math.hypot(p.x - cx, min(z, ZT[i]) - ZC[i])
            PTS.append(row); TOPF.append(tf)
            if NF <= i and y > TEy + 0.004:     # its top against the flap's lower skin (3 mm under it): no surface of his there to match
                SKIPS.setdefault(name + '_Canoe', []).extend((p.copy(), 3) for p, t in zip(row, tf) if t)
                SKIPS[name + '_Canoe'].extend((p.copy(), 2) for p, t in zip(row, tf) if not t and p.z > ZC[i] + 0.02 and BVH.find_nearest(p)[3] > 0.01)
        # the moving rear canoe (his texture baked on it)
        crow = [[M.add(p) for p in row] for row in PTS[NF:]]
        for i in range(len(crow) - 1):
            for j in range(NA):
                j2 = (j + 1) % NA; u0_, u1_ = 0.015 + 0.97 * i / (len(crow) - 1), 0.015 + 0.97 * (i + 1) / (len(crow) - 1); v0_, v1_ = 0.765 + 0.22 * j / NA, 0.765 + 0.22 * (j + 1) / NA
                M.face([crow[i][j], crow[i][j2], crow[i + 1][j2], crow[i + 1][j]], 'CANOE', [(u0_, v0_), (u0_, v1_), (u1_, v1_), (u1_, v0_)])
        def endcap(MM, row, flip, mat):          # a flat closed end: a ring 6 mm in, then a fan (smooth outline, no spikes)
            cen = sum((MM.v[k] for k in row), Vector()) / len(row)
            inner = [MM.add(MM.v[k] + (cen - MM.v[k]).normalized() * min(0.006, (cen - MM.v[k]).length * 0.3)) for k in row]
            MM.grid([row, inner], mat, closed=True, flip=not flip); cc = MM.add(cen)
            for j in range(len(row)): MM.face([inner[j], inner[(j + 1) % len(row)], cc][::-1 if flip else 1], mat)
        endcap(M, crow[0], False, M_PAINT); endcap(M, crow[-1], True, M_PAINT)
        lowest = min(p.z for row in PTS for p in row)
        zfl = float(np.interp(-en.y, -YS, [min(p.z for p in row) for row in PTS]))   # aft support post from the rail's end down onto the new canoe's floor
        prism(TR, [(-0.04, zfl + 0.001 - en.z), (0.04, zfl + 0.001 - en.z), (0.025, -0.02), (-0.025, -0.02)], V(0, 1, 0), V(0, 0, 1), en, -0.015, 0.015, M_TRACK)
        REPORT.setdefault('flap_canoe', []).append(dict(cx=round(cx, 3), y=[round(yA, 3), round(Ycut, 3), round(ytip, 3)], lowest_z=round(lowest, 3), hp_z=round(hp.z, 3),
            front_clear_mm=round(min((Ycut + 0.0035 - turn(p, a_).y) * 1000 if turn(p, a_).z > ZB[NF] - 0.01 else 99 for p in PTS[NF] for a_ in np.linspace(0, OPEN, 31)), 1),
            rail_min_y=round(min(turn(car, a_).y for a_ in np.linspace(k0, OPEN + 6.0, 25)) - 0.09, 3)))
        # the fixed front canoe: open top under the flap (a deck with the link's slot closes it), closed aft end
        frow = [[FX.add(p) for p in row] for row in PTS[:NF]]
        for i in range(NF - 1):
            under = YS[i + 1] < yF and YS[i] < yF
            for j in range(NA):
                j2 = (j + 1) % NA
                if under and TOPF[i][j] and TOPF[i][j2] and TOPF[i + 1][j] and TOPF[i + 1][j2]: continue
                FX.face([frow[i][j], frow[i][j2], frow[i + 1][j2], frow[i + 1][j]], M_PAINT)
        endcap(FX, frow[-1], True, M_PAINT)
        def topw(i, sgn):         # the half-width of the open top at station i (its outermost top point on that side)
            xs = [PTS[i][j].x - cx for j in range(NA) if TOPF[i][j] and (PTS[i][j].x - cx) * sgn >= 0]
            return max(abs(v) for v in xs) if xs else 0.03
        IU = [i for i in range(NF) if YS[i] < yF]; YD = YS[IU]; ZD = ZT[IU]
        WL = [topw(i, -1) for i in IU]; WR = [topw(i, 1) for i in IU]
        y_se = min(turn(V(cx, car.y + sg * 0.07, refz(cx, car.y) - 0.01), OPEN).y for sg in (-1, 1)) - 0.03
        y_se = max(y_se, YD[-1] + 0.03); REPORT.setdefault('flap_slot_end', []).append(round(y_se, 3))
        def strip(xa, xb, ya, yb):      # deck plate 6 mm, its top flush with the canoe's top edge
            ys_ = sorted(set([ya, yb] + [y for y in YD if ya - 1e-6 <= y <= yb + 1e-6]), reverse=True); rows_ = []
            for y in ys_:
                z = float(np.interp(-y, -YD, ZD)); a_, b_ = xa(y), xb(y)
                rows_.append([FX.add(V(a_, y, z)), FX.add(V(b_, y, z)), FX.add(V(b_, y, z - 0.006)), FX.add(V(a_, y, z - 0.006))])
            FX.grid(rows_, M_PAINT, closed=True); FX.face(rows_[0][::-1], M_PAINT); FX.face(rows_[-1], M_PAINT)
        wl = lambda y: cx - float(np.interp(-y, -YD, WL)); wr = lambda y: cx + float(np.interp(-y, -YD, WR))
        strip(wl, lambda y: cx - 0.03, YD[-1], YD[0]); strip(lambda y: cx + 0.03, wr, YD[-1], YD[0])
        strip(lambda y: cx - 0.03, lambda y: cx + 0.03, YD[-1], y_se)
        # his canoe goes from yA to its tip (the new one takes its place): all of it inside the new body, never his wing above
        def csdf(P, cx=cx, YS=YS, ZC=ZC, ZT=ZT, RF=RF):
            y = P[:, 1]; t = np.interp(-y, -YS, np.arange(NS)); i = np.clip(np.floor(t).astype(int), 0, NS - 2); w = np.clip(t - i, 0, 1)
            zc_ = ZC[i] * (1 - w) + ZC[i + 1] * w; dx = P[:, 0] - cx; dz = P[:, 2] - zc_
            a = np.mod(np.arctan2(dz, dx), 2 * np.pi) / (2 * np.pi) * NA; j = np.floor(a).astype(int) % NA; v = a - np.floor(a); j2 = (j + 1) % NA
            rf = (RF[i, j] * (1 - v) + RF[i, j2] * v) * (1 - w) + (RF[i + 1, j] * (1 - v) + RF[i + 1, j2] * v) * w
            zt_ = ZT[i] * (1 - w) + ZT[i + 1] * w; zb_ = zc_ - (RF[i, 3 * NA // 4] * (1 - w) + RF[i + 1, 3 * NA // 4] * w)
            hw = np.max(RF, axis=1); hw_ = hw[i] * (1 - w) + hw[i + 1] * w
            col = np.maximum(np.abs(dx) - (hw_ + 0.01), (zb_ - 0.015) - P[:, 2])
            cap = np.where(y > TEy + 0.004, P[:, 2] - zt_ - 0.001, -1.0)
            plate = (dx > -0.13) & (dx < 0.08) & (y > YP - 0.12) & (dz < 0)       # his gear-leg door: cut right on the new canoe's skin, it joins there
            col = np.where(plate, np.maximum(np.abs(dx) - (hw_ + 0.01), zb_ - P[:, 2]), col)
            return np.maximum.reduce([np.minimum(np.hypot(dx, dz) - (rf + np.where(plate, 0.002, 0.012)), col), cap, y - yA, (YS[-1] - 0.03) - y])
        def cband(P, cx=cx, y1=YS[-1]):
            return (np.abs(P[:, 0] - cx) < 0.35) & (P[:, 1] < yA + 0.05) & (P[:, 1] > y1 - 0.1) & (P[:, 2] > 1.9) & (P[:, 2] < 3.6)
        cut_hole(csdf, lambda P: cband(P) & (P[:, 1] > yA - 0.06) & (np.abs(P[:, 0] - cx) < 0.25), name + '_CanoeFront%d' % len(CARS), refine=0.012)
        cut_hole(csdf, cband, name + '_Canoe%d' % len(CARS), islands=((cx - 0.3, YS[-1] - 0.1, 1.9), (cx + 0.3, yA - 0.002, 3.45)))
        # his gear-leg door (a ~5 cm plate under the fixed canoe, x cx-0.13..cx-0.02) ends aft in a torn scan edge near y -13.03:
        # trimmed straight at Yd, the cut closed flat and finished with a rolled half-round edge (owner 11-27, like the door cut)
        Yd = -12.985
        def zbot(y, YS=YS, ZC=ZC, RF=RF): return float(np.interp(-y, -YS, ZC - RF[:, 3 * NA // 4]))
        dband = lambda P, cx=cx: ((P[:, 0] > cx - 0.20) & (P[:, 0] < cx + 0.06) & (P[:, 1] < Yd + 0.06) & (P[:, 1] > -13.7) & (P[:, 2] > 1.6)
                                  & (P[:, 2] < np.array([zbot(y) for y in P[:, 1]]) + 0.004))
        cut_hole(lambda P: P[:, 1] - Yd, dband, name + '_GearDoorTrim%d' % len(CARS), cap=((cx - 0.07, Yd, 2.2), V(0, -1, 0)),
                 islands=((cx - 0.2, -13.7, 1.6), (cx + 0.06, Yd + 0.001, zbot(Yd) + 0.01)))
        # round 4 (owner 11-27: his door still showed the scan paint, dark creases and a ragged edge): his gear-leg door goes as
        # a whole and a NEW clean solid door takes its place - his own faces measured and fitted smooth (same shape), a straight
        # trimmed bottom and ends, half-round rolled edges, clean paint like the cowl; its top runs up into the new canoe
        GD = Mesh(); yG0, yG1 = Yd, -11.93; ZO = []; ZI = []
        for y in np.linspace(yG0 + 0.03, yG1 - 0.03, 15):
            for z in np.linspace(1.95, 2.50, 12):
                ho = shoot(V(cx + 0.45, y, z), V(-1, 0, 0), hi=0.42, test=lambda q: cx - 0.2 < q.x < cx + 0.12)
                hi_ = shoot(V(cx - 0.26, y, z), V(1, 0, 0), hi=0.42, test=lambda q: cx - 0.2 < q.x < cx + 0.12)
                if ho is not None and hi_ is not None and 0.03 < ho.x - hi_.x < 0.2: ZO.append((y, z, ho.x)); ZI.append((y, z, hi_.x))
        bots = [h.z for h in (shoot(V(cx - 0.04, y, 1.5), V(0, 0, 1), hi=0.6, test=lambda q: cx - 0.2 < q.x < cx + 0.12) for y in np.linspace(yG0 + 0.05, yG1 - 0.15, 12)) if h is not None]
        if len(ZO) > 12:
            def fit(S):
                A_ = np.array([[1, y, z, z * z] for y, z, _ in S]); b_ = np.array([q[2] for q in S]); c_ = np.linalg.lstsq(A_, b_, rcond=None)[0]
                for _ in range(2):      # drop the scan's lumps (> 1 cm off) and fit again
                    ok_ = np.abs(A_ @ c_ - b_) < 0.01
                    if ok_.sum() > 8: c_ = np.linalg.lstsq(A_[ok_], b_[ok_], rcond=None)[0]
                return lambda y, z, c_=c_: float(c_[0] + c_[1] * y + c_[2] * z + c_[3] * z * z)
            fo, fi = fit(ZO), fit(ZI); zB = float(np.median(bots)) if bots else 1.88
            def sect(y, s=1.0):           # closed outline at y: bottom half-round, outer face up, across inside the canoe, inner face down
                zt_ = zbot(y) + 0.05; h0 = (fo(y, zB + 0.06) - fi(y, zB + 0.06)) / 2; zc0 = zB + h0
                out = []
                for a_ in np.linspace(-math.pi / 2, 0, 7):      # the rolled bottom edge (outer half)
                    xm_ = (fo(y, zc0) + fi(y, zc0)) / 2; out.append(V(xm_ + h0 * s * math.cos(a_), y, zc0 + h0 * math.sin(a_)))
                for z in np.linspace(zc0, zt_, 10)[1:]:
                    xm_ = (fo(y, z) + fi(y, z)) / 2; hh = (fo(y, z) - fi(y, z)) / 2; out.append(V(xm_ + hh * s, y, z))
                for z in np.linspace(zt_, zc0, 10)[:-1]:
                    xm_ = (fo(y, z) + fi(y, z)) / 2; hh = (fo(y, z) - fi(y, z)) / 2; out.append(V(xm_ - hh * s, y, z))
                for a_ in np.linspace(math.pi, 3 * math.pi / 2, 7)[:-1]:
                    xm_ = (fo(y, zc0) + fi(y, zc0)) / 2; out.append(V(xm_ + h0 * s * math.cos(a_), y, zc0 + h0 * math.sin(a_)))
                return out
            re_ = 0.03; ys_ = []
            for a_ in np.linspace(0.18, math.pi / 2, 6): ys_.append((yG0 - re_ * (1 - math.cos(a_)), math.sin(a_)))
            for y in np.linspace(yG0 - re_, yG1 + re_, 14)[1:-1]: ys_.append((float(y), 1.0))
            for a_ in np.linspace(math.pi / 2, 0.18, 6): ys_.append((yG1 + re_ * (1 - math.cos(a_)), math.sin(a_)))
            rows_ = [[GD.add(p) for p in sect(y, s)] for y, s in ys_]
            GD.grid(rows_, M_PAINT, closed=True)
            for row, fl in ((rows_[0], True), (rows_[-1], False)):
                cc = GD.add(sum((GD.v[k] for k in row), Vector()) / len(row))
                for j in range(len(row)): GD.face(([row[j], row[(j + 1) % len(row)], cc])[::-1 if fl else 1], M_PAINT)
            # his door goes (all of it, below the new canoe's skin), the new one stands in its place
            gband = lambda P, cx=cx: ((P[:, 0] > cx - 0.24) & (P[:, 0] < cx + 0.20) & (P[:, 1] < -11.60) & (P[:, 1] > yG0 - 0.08) & (P[:, 2] > 1.55)
                                      & (P[:, 2] < np.minimum(np.array([zbot(y) for y in P[:, 1]]) + 0.03, np.where(P[:, 1] > yG1 + 0.04, 2.55, 2.92))))
            cut_hole(lambda P: np.full(len(P), -1.0), gband, name + '_GearDoorOld%d' % len(CARS))
            STATIC.append(GD.obj('FlapInner_R_GearDoorFairing'))
            REPORT.setdefault('flap_gear_door', []).append(dict(y=[round(yG0, 3), round(yG1, 3)], z_bottom=round(zB, 3), n_fit=len(ZO),
                                                                 t=[round(fo(y, 2.2) - fi(y, 2.2), 3) for y in (yG0, (yG0 + yG1) / 2, yG1)]))
    REPORT['flap_carriages'] = [[round(cx, 3), [round(c, 3) for c in car], k0, o_] for cx, car, k0, o_ in CARS]
    REPORT['flap_rail_points_outside_canoe'] = rail_out
    ob = finish(M, name, hp)
    PARTS[name] = (ob, ('rot', hp, V(1, 0, 0), OPEN))
    # the cove: shroud lip under the top skin, ceiling, front wall, floor, end ribs; lips under his bottom skin. His skins at the
    # front line are noisy: the lip follows a smooth line along the span (moved under / over his skin so it never pokes through)
    F = Mesh(); crow = []; FS = {}; FE = {}
    def fillet(pr, r, nseg=4):   # the profile's inner corners rounded (r), so the cove reads as one bent sheet, not planks
        out = [pr[0]]; FS[0] = FE[0] = 0
        for k in range(1, len(pr) - 1):
            a, b, c = np.array(pr[k - 1]), np.array(pr[k]), np.array(pr[k + 1]); rk = r[k - 1] if isinstance(r, (list, tuple)) else r
            u = (a - b) / np.linalg.norm(a - b); w = (c - b) / np.linalg.norm(c - b); FS[k] = len(out)
            p0 = b + u * rk; p1 = b + w * rk
            for t in np.linspace(0, 1, (nseg if rk < 0.02 else 3 * nseg) + 1):     # quadratic Bezier from p0 over the corner to p1
                q = (1 - t) ** 2 * p0 + 2 * (1 - t) * t * b + t ** 2 * p1; out.append((float(q[0]), float(q[1])))
            FE[k] = len(out) - 1
        FS[len(pr) - 1] = FE[len(pr) - 1] = len(out); out.append(pr[-1]); return out
    CS = [min(secs, key=lambda q, x=x: abs(q[0] - x)) for x in np.linspace(a0 - 0.012, a1 + 0.012, 60)]; XL = np.linspace(a0 - 0.012, a1 + 0.012, 60)   # 4 mm clear of the flap's end ribs (no z-fight closed)
    XC = np.array([q[0] for q in CS]); ZTr = np.array([q[1][0][1] for q in CS]); ZBr = np.array([q[2][0][1] for q in CS])
    ft = np.polyval(np.polyfit(XC, ZTr, 2), XC); fb = np.polyval(np.polyfit(XC, ZBr, 2), XC)
    ZTs = np.minimum(ft - min(max((ft - ZTr).max(), 0.0), 0.008), ZTr); ZBs = np.maximum(fb + min(max((ZBr - fb).max(), 0.0), 0.008), ZBr)
    REPORT['flap_cove_lip_dev_mm'] = [round(float(np.abs(ZTs - ft).max()) * 1000, 1), round(float(np.abs(ZBs - fb).max()) * 1000, 1)]
    for x, (_, top, bot), zt, zb in zip(XL, CS, ZTs, ZBs):     # its own x (a station's x would sit on the flap's end ribs)
        h = (zt - zb) / 2; b = min(0.10, 0.6 * h); yW = yF + b + 0.03
        prof = [(yF + 0.004 + 0.02, zt - 0.004), (yF + 0.004, zt - 0.004), (yF + 0.004, zt - 0.03), (yW, zt - 0.03), (yW, zb + 0.03), (yF + 0.004, zb + 0.03),
                (yF + 0.004, zb + 0.004), (yF + 0.004 + 0.02, zb + 0.004)]
        # round 4 (owner 11-27: flat angled planks): the cove's back is one big round - ceiling, wall and floor a single
        # curved shroud sheet; its lips folded over small rounds (no sharp plank edges)
        rB = 0.98 * min(b + 0.012, (zt - zb - 0.06) / 2)
        crow.append([F.add(V(x, y, z)) for y, z in fillet(prof, [0.008, 0.012, rB, rB, 0.012, 0.008])])
    F.grid(crow, M_COVE)
    for row in (crow[0], crow[-1]): F.face(row[FS[2]:FE[5] + 1], M_COVE)
    for x, row in ((a0 - 0.012, crow[0]), (a1 + 0.012, crow[-1])):        # end ribs of the cove behind the flap ends, from the front wall to the trailing edge
        _, top, bot = min(secs, key=lambda q: abs(q[0] - x))
        if len(top) < 3: continue
        nn = min(len(top), len(bot)); pr = [(top[k][0], top[k][1] - 0.004, bot[k][1] + 0.004) for k in range(nn)]
        pr = [q for q in pr if math.isfinite(q[2]) and q[1] - q[2] > 0.004]
        it = [F.add(V(x, y, z)) for y, z, _ in pr]; ib = [F.add(V(x, y, z)) for y, _, z in pr]
        fl_ = x > (a0 + a1) / 2      # faces look into the hole
        for j in range(len(pr) - 1):
            q = [it[j], it[j + 1], ib[j + 1], ib[j]]; F.face(q if fl_ else q[::-1], M_COVE)
        q = [it[0]] + row[FS[1]:FE[2] + 1] + row[FS[5]:FE[6] + 1] + [ib[0]]; F.face(q[::-1] if fl_ else q, M_COVE)   # joined to the cove's end row
    STATIC.append(F.obj('FlapInner_R_Cove'))
    STATIC.append(TR.obj('FlapInner_R_Tracks'))
    STATIC.append(FX.obj('FlapInner_R_CanoeFixed'))
    CANX = [((c0 + c1) / 2 - 0.2, (c0 + c1) / 2 + 0.2) for c0, c1 in CAN]
    def skins(Q):             # his top and lower skin (lower filled over canoes) at the points Q, by the flap's stations
        idx = np.clip(np.searchsorted(SX, Q[:, 0]), 0, len(SX) - 1); t = np.zeros(len(Q)); b = np.zeros(len(Q)); y0_ = np.zeros(len(Q)); y1_ = np.zeros(len(Q))
        order = np.argsort(idx, kind='stable'); ks, starts = np.unique(idx[order], return_index=True); ends = list(starts[1:]) + [len(order)]
        for k, a_, b_ in zip(ks, starts, ends):
            m = order[a_:b_]; _, ty_, tz_, bz_ = STN[k]; y = Q[m, 1]
            t[m] = np.interp(-y, -ty_, tz_); b[m] = np.interp(-y, -ty_, bz_); y0_[m] = ty_[0]; y1_[m] = ty_[-1]
        return t, b, y0_, y1_
    def in_canx(Q):
        r_ = np.zeros(len(Q), bool)
        for lo_, hi_ in CANX: r_ |= (Q[:, 0] > lo_) & (Q[:, 0] < hi_) & (Q[:, 1] > Ycut - 0.01)
        return r_
    # his wing in the flap: everything between his top and lower skin of the flap's stations, from yF to the trailing edge
    def sdf(P):               # the flap's place; over the fixed canoe his walls stay up to 4 mm under his lower skin (a clean level cut)
        qx = np.abs(P[:, 0] - (a0 + a1) / 2) - (a1 - a0) / 2; qy = P[:, 1] - yF
        r_ = np.maximum(qx, qy) - 0.002; m = in_canx(P)
        if m.any(): r_[m] = np.maximum(r_[m], (skins(P[m])[1] - 0.004) - P[m, 2])
        return r_
    def band(P):
        ok = (P[:, 0] > a0 - 0.3) & (P[:, 0] < a1 + 0.3) & (P[:, 1] < yF + 0.9) & (P[:, 1] > -14.5)
        J = np.nonzero(ok)[0]; Q = P[J]; t, b, y0_, y1_ = skins(Q); y = Q[:, 1]
        fw = np.maximum(y - y0_, 0.0) * 0.35        # ahead of the front line his skins keep rising / falling
        dn = np.where(in_canx(Q), 0.08, 0.03)
        ok[J] = (Q[:, 2] < t + 0.03 + fw) & (Q[:, 2] > b - dn - fw) & (y > y1_ - 0.015)
        return ok
    def spare(comp):          # his fixed canoe's pieces (under his lower skin, ahead of the cut) stay
        c = comp[0].calc_center_median(); Q = np.array([c[:]])
        return bool(in_canx(Q)[0]) and c.z < skins(Q)[1][0] - 0.004
    def inside(P):            # in the flap's place: between his skins (lower one filled over canoes / holes), aft of the front line
        r_ = np.zeros(len(P), bool)
        for i_, q in enumerate(P):
            if not (a0 + 0.002 < q[0] < a1 - 0.002 and q[1] < yF - 0.002): continue
            _, ty_, tz_, bz_ = stn(q[0])
            if q[1] < ty_[-1] - 0.01: continue
            r_[i_] = float(np.interp(-q[1], -ty_, bz_)) + 0.003 < q[2] < float(np.interp(-q[1], -ty_, tz_)) + 0.03
        return r_
    cut_hole(sdf, band, name, islands=((a0 - 0.02, -15.0, 2.3), (a1 + 0.02, yF + 0.01, 4.2), spare),   # only bits in the hole: a wider box took his own skin patches ahead of it (notches)
            
             inside=((a0 - 0.05, -15.0, 2.3), (a1 + 0.05, yF + 0.05, 4.2), inside))
    # his pylon tail fairing reaches under the flap's front (the flap would swing through it): it ends square at the flap's
    # front line, closed with a flat cap
    PYX = (4.75, 6.45)
    # round 4 (owner 11-27: a square flat cap): his tail fairing (x ~5.38-5.72, under the wing skin) is cut on the old line
    # with fine faces near it, then its last 4 cm drawn in on a quarter circle toward its own centre line - a rounded tail
    # end like the real fairing, the flat cap shrinking into a small end; the wing skin above (z > 3.18) is not touched
    psdf = lambda P: P[:, 1] - (yF - 0.003)
    pband = lambda P: (P[:, 0] > PYX[0]) & (P[:, 0] < PYX[1]) & (P[:, 1] > -13.3) & (P[:, 1] < yF + 0.4) & (P[:, 2] > 2.3) & (P[:, 2] < 3.6)
    cut_hole(psdf, lambda P: (P[:, 0] > 5.25) & (P[:, 0] < 5.85) & (P[:, 1] > -12.75) & (P[:, 1] < yF + 0.06) & (P[:, 2] > 2.6) & (P[:, 2] < 3.2),
             name + '_PylonTailFine', refine=0.011)      # fine faces only on the tail's last few cm
    cut_hole(psdf, pband, name + '_PylonTail', cap=((5.6, yF - 0.003, 3.0), V(0, 1, 0)),
             islands=((PYX[0], -13.4, 2.3), (PYX[1], yF - 0.002, 3.6)))
    Dp = 0.04; MWi = MW.inverted(); nmv = 0
    tl = [MW @ v.co for v in o.data.vertices]
    near = [q for q in tl if 5.25 < q.x < 5.85 and abs(q.y - (yF - 0.003)) < 3e-4 and 2.6 < q.z < 3.18]
    if near:
        cxp = (min(q.x for q in near) + max(q.x for q in near)) / 2; ztp = 3.18
        for v, q in zip(o.data.vertices, tl):
            if not (5.25 < q.x < 5.85 and yF - 0.004 < q.y < yF - 0.003 + Dp and 2.6 < q.z < 3.2): continue
            s_ = (q.y - (yF - 0.003)) / Dp; k = max(math.sqrt(max(1 - (1 - s_) ** 2, 0.0)), 0.30)
            w = min(max((ztp - q.z) / 0.08, 0.0), 1.0); k = 1 - (1 - k) * w
            v.co = MWi @ V(cxp + (q.x - cxp) * k, q.y, ztp - (ztp - q.z) * k); nmv += 1
        o.data.update()
    REPORT['flap_pylon_tail_round'] = dict(moved=nmv, cx=round(cxp, 3) if near else None)
    # his stray bits hanging under the flap's lower skin (seen once it opens): out of the fixed canoe and the fairing they go
    bm = bmesh.new(); bm.from_mesh(o.data); bm.transform(MW); und = []
    for f_ in bm.faces:
        q = f_.calc_center_median()
        if not (a0 + 0.3 < q.x < PYX[0] and q.y < yF - 0.004) or any(abs(q.x - c_[0]) < 0.2 for c_ in CARS): continue
        _, ty_, tz_, bz_ = stn(q.x)
        if q.y < ty_[-1] - 0.01: continue
        zb_ = float(np.interp(-q.y, -ty_, bz_))
        if zb_ - 0.3 < q.z < zb_ + 0.003: und.append(f_)
    bmesh.ops.delete(bm, geom=und, context='FACES'); REPORT['flap_under_bits_removed'] = len(und)
    bm.transform(MW.inverted()); bm.to_mesh(o.data); bm.free()
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
                 flap_edge=((7.4, -15.9, 2.15), (5.4, -13.3, 3.0), 35))
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
if os.environ.get('ADP_NOWHEELS') != '1':
    for sd in ('_L', '_R'): build_wheels(sd)
for nm, ob in built.items():
    measure(ob, ob.name, 'A320C_%s_Skin' % ob.name)
    if 'A320C_%s_CanoeSkin' % ob.name in ob.data.materials: measure(ob, ob.name + '_Canoe', 'A320C_%s_CanoeSkin' % ob.name)
    if os.environ.get('ADP_NOBAKE') != '1': bake(ob, *SKINS[ob.name]); print('BAKED', nm, flush=True)
REPORT['note'] = 'closed check: distance of each part outer skin (incl. its 4 mm rolled edge) to his ORIGINAL surface, before the cut, in mm'
json.dump(REPORT, open(os.path.join(HERE, 'a320-clean-check.json'), 'w'), indent=1)
bpy.data.objects.remove(src)
for ob in bpy.data.objects:
    if ob.get('open_only'): ob.hide_render = True
if os.environ.get('ADP_POST'):      # a look at the result (own renders to a scratch folder), nothing saved
    exec(open(os.environ['ADP_POST']).read()); raise SystemExit
if os.environ.get('ADP_NOSAVE') != '1': bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'A320-clean.blend'))
if os.environ.get('ADP_GLB'):
    bpy.ops.object.select_all(action='DESELECT')
    for ob in bpy.data.objects:
        if ob.type == 'MESH' and any(ob.name.startswith(k) for k in os.environ.get('ADP_GLB_PARTS', 'CargoDoorFwd,FlapInner_R').split(',')): ob.select_set(True)
    _exp(filepath=os.environ['ADP_GLB'], export_format='GLB', use_selection=True, export_apply=True)
if os.environ.get('ADP_RENDER', '1') == '1':
    cam = setup_render(); RO = os.environ.get('ADP_RENDER_ONLY', '')      # e.g. 'flap': only those parts' pictures are rewritten
    want = lambda v: not RO or v.split('_')[0] in RO.split(',')
    for v in [x for x in VIEWS if (x == 'wheels' or x in built) and want(x)]: shot(cam, v, os.path.join(PIC, 'A320-clean-%s-AFTER%s.png' % (v, '-closed' if v != 'wheels' else '')))
    for nm, (ob, (_, p, axis, deg)) in PARTS.items():
        ob.matrix_world = Matrix.Translation(p) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-p) @ ob.matrix_world
    for ob in bpy.data.objects:
        if ob.get('open_only'): ob.hide_render = False
    for v in [x for x in VIEWS if x in built and want(x)]: shot(cam, v, os.path.join(PIC, 'A320-clean-%s-AFTER-open.png' % v))
    VIEWS.update(OPENVIEWS)
    for v in [x for x in OPENVIEWS if x.split('_')[0] in built and want(x)]: shot(cam, v, os.path.join(PIC, 'A320-clean-%s-AFTER-open.png' % v))
print('DONE', json.dumps(REPORT)[:1500])
