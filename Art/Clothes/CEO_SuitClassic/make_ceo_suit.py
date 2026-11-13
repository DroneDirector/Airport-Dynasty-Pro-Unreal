# The CEO's classic suit (owner, 2026-11-13: the clothes line, CEO outfits first): tailored on the player's own body
# (Art/Clothes/_bodies/player_body.fbx, exported from the player built in a garment that hides no skin:
# /Game/People/Built/MH_900_player_TestShirt; the plain build has no torso or hips, Epic's tee and shorts cut them away), one garment in five pieces,
# each in its fabric from the library (Art/Fabrics): white shirt with a stand collar (cotton), straight-legged trousers
# with a waistband (wool suiting), a single-breasted jacket open at the front with lapels and a hem below the seat
# (wool suiting), a tie (satin), shoes with soles (leather). Every piece starts as the body's own surface in its region
# (so it fits and its skin weights carry over), is smoothed into cloth, given ease (how far it stands off the skin) and
# thickness, and kept outside the body and outside the piece under it.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Clothes/CEO_SuitClassic/make_ceo_suit.py
#   -> CEO_SuitClassic.fbx (for Tools/make_outfit.py CEO_SuitClassic), CEO_SuitClassic.blend, preview.png
import bpy, bmesh, math
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from pathlib import Path

HERE = Path(__file__).resolve().parent
NAME = HERE.name
BODYFBX = HERE.parent / '_bodies' / 'player_body.fbx'

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
bpy.ops.import_scene.fbx(filepath=str(BODYFBX))
src = [o for o in sc.objects if o.type == 'MESH' and o.name.endswith('_LOD0')][0]   # the full body (built without the tee and shorts, which hide the skin under them)
M = src.matrix_world.copy()
GI = {g.index: g.name for g in src.vertex_groups}

# the body in world space (metres), as a mesh and a BVH, for fitting
bb = bmesh.new(); bb.from_mesh(src.data); bb.transform(M)
# the head mesh (its neck reaches down over the collarbones, over the body's): the cloth keeps out of it too
_before = set(sc.objects)
bpy.ops.import_scene.fbx(filepath=str(BODYFBX.parent / 'player_face.fbx'))
_face = [o for o in sc.objects if o not in _before and o.type == 'MESH' and o.name.endswith('_LOD0')]
if _face:
    fb = bmesh.new(); fb.from_mesh(_face[0].data); fb.transform(_face[0].matrix_world)
    tmp = bpy.data.meshes.new('fb'); fb.to_mesh(tmp); fb.free(); bb.from_mesh(tmp)
for o in [o for o in sc.objects if o not in _before]: bpy.data.objects.remove(o, do_unlink=True)
bb.normal_update()
BODY_BVH = BVHTree.FromBMesh(bb)
bb.verts.ensure_lookup_table()
BODY_N = [v.normal.copy() for v in bb.verts]
log = lambda *a: print('SUIT', *a)

# every bone, helpers and correctives too, belongs to the region of its nearest main ancestor
ARM = [o for o in sc.objects if o.type == 'ARMATURE'][0]
MAIN = [('pelvis', 'hips'), ('spine_', 'torso'), ('clavicle', 'upperarm'), ('upperarm', 'upperarm'), ('lowerarm', 'lowerarm'),
        ('hand_', 'hand'), ('neck_', 'neck'), ('head', 'head'), ('thigh', 'thigh'), ('calf', 'calf'), ('foot', 'foot'), ('ball', 'toes')]
def main_of(name):
    for p, r in MAIN:
        if name.startswith(p) and not any(k in name for k in ('corrective', '_bck', '_fwd', '_in_', '_out_')):
            return r
    return None
REGION = {}
for b in ARM.data.bones:
    x = b; r = None
    while x is not None and r is None:
        r = main_of(x.name); x = x.parent
    REGION[b.name] = r or 'root'
def weights(i):
    w = {}
    for e in src.data.vertices[i].groups:
        r = REGION.get(GI[e.group], 'root'); w[r] = w.get(r, 0.0) + e.weight
    return w
def wsum(w, regions):
    return sum(w.get(r, 0.0) for r in regions)
W = [weights(i) for i in range(len(src.data.vertices))]
CO = [M @ v.co for v in src.data.vertices]
FRONT = -1.0   # the body faces -y here

# ---------------------------------------------------------------- helpers
def cut(name, keep):
    """The body's faces whose corners are all kept, as a new object in world space (UVs and all)."""
    bm = bmesh.new(); bm.from_mesh(src.data); bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if not all(keep(v.index) for v in f.verts)], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.transform(M)
    # the body mesh is split along its texture seams (round the waist, down the inner legs): weld them, or smoothing
    # opens them into cracks
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0012)
    bmesh.ops.dissolve_degenerate(bm, dist=0.0008, edges=bm.edges)   # squashed triangles from the weld (they pinch the cloth: a dark dot)
    bmesh.ops.holes_fill(bm, edges=bm.edges, sides=6)   # (only pin-pricks: more capped the trouser hems shut)   # tiny holes in the body mesh would show as dark pin-pricks in the cloth
    # small islands (stray bits) go
    bm.verts.ensure_lookup_table(); seen = set(); islands = []
    for v in bm.verts:
        if v in seen: continue
        st = [v]; isl = []; seen.add(v)
        while st:
            x = st.pop(); isl.append(x)
            for e in x.link_edges:
                o = e.other_vert(x)
                if o not in seen: seen.add(o); st.append(o)
        islands.append(isl)
    big = max(len(i) for i in islands)
    bmesh.ops.delete(bm, geom=[v for i in islands if len(i) < big * 0.05 for v in i], context='VERTS')
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o)
    return o

def smooth(o, iters, factor=0.5, keep_border=True):
    bm = bmesh.new(); bm.from_mesh(o.data)
    vs = [v for v in bm.verts if not (keep_border and v.is_boundary)]
    for _ in range(iters):
        bmesh.ops.smooth_vert(bm, verts=vs, factor=factor, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.to_mesh(o.data); bm.free()

def offset(o, dist_fn):
    bm = bmesh.new(); bm.from_mesh(o.data); bm.normal_update()
    for v in bm.verts: v.co += v.normal * dist_fn(v.co)
    bm.to_mesh(o.data); bm.free()

def keep_out(o, under_bvh, gap, reach=0.035):
    """No point of the piece closer than gap to (or inside) the surface under it (only where that surface is near)."""
    bm = bmesh.new(); bm.from_mesh(o.data); n = 0
    for v in bm.verts:
        loc, nor, _, d = under_bvh.find_nearest(v.co, reach)
        if loc is None: continue
        g = gap(v.co) if callable(gap) else gap
        side = (v.co - loc).dot(nor)
        if side < g:
            v.co = loc + nor * g; n += 1
    bm.to_mesh(o.data); bm.free()
    return n

def clean_edges(o, iters=4):
    """Every open edge of a piece made clean: a loop that is nearly flat (a hem, a cuff) is laid in its own plane, then
    every loop is relaxed along itself, so no edge follows the body's triangles in steps."""
    import numpy as np
    bm = bmesh.new(); bm.from_mesh(o.data)
    for loop in boundary_loops(bm):
        if len(loop) < 6: continue
        P = np.array([v.co[:] for v in loop]); c = P.mean(0)
        u, sv, vt = np.linalg.svd(P - c); n = vt[2]
        spread = np.abs((P - c) @ n).max()
        for _ in range(iters):
            new = [(loop[i - 1].co + loop[(i + 1) % len(loop)].co) * 0.25 + loop[i].co * 0.5 for i in range(len(loop))]
            for v, q in zip(loop, new): v.co = q
    bm.to_mesh(o.data); bm.free()

def trim(o, co, no):
    """Cut the piece with a flat plane, as a tailor marks a hem, keeping the side the normal points to."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=co, plane_no=no, clear_inner=True)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(o.data); bm.free()
def bone(name): return ARM.matrix_world @ ARM.data.bones[name].head_local
def cuffs(o, back):
    """The sleeves cut square to the forearm, back from the wrist joint."""
    for s_ in ('l', 'r'):
        h = bone('hand_' + s_); e = bone('lowerarm_' + s_); d = (h - e).normalized()
        c = h - d * back
        # only this arm: a plane square to the forearm, applied to the points out along it
        bm = bmesh.new(); bm.from_mesh(o.data)
        far = [f for f in bm.faces if (f.calc_center_median() - c).dot(d) > 0 and (f.calc_center_median() - h).length < 0.25]
        bmesh.ops.delete(bm, geom=far, context='FACES')
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
        bm.to_mesh(o.data); bm.free()

NECK = ARM.matrix_world @ ARM.data.bones['neck_01'].head_local
def neck_surface(theta, z, start=0.30):
    """How far the skin (body or head) is from the neck's axis, in direction theta (0 = front), at height z."""
    d = Vector((math.sin(theta), -math.cos(theta), 0.0)); c = Vector((NECK.x, NECK.y, z))
    hit = BODY_BVH.ray_cast(c + d * start, -d, start)
    return (hit[0] - c).length if hit[0] is not None else 0.07
def neck_tube(name, thetas, rows, radius):
    """A tube round the neck: for each angle, rows of heights rows(theta) at radius(theta, z, surface)."""
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new('UVMap'); grid = []
    for th in thetas:
        col = []
        for z in rows(th):
            r = radius(th, z, neck_surface(th, z))
            col.append(bm.verts.new(Vector((NECK.x, NECK.y, z)) + Vector((math.sin(th), -math.cos(th), 0)) * r))
        grid.append(col)
    for j in range(len(grid) - 1):
        for k in range(len(grid[j]) - 1):
            f = bm.faces.new((grid[j][k], grid[j + 1][k], grid[j + 1][k + 1], grid[j][k + 1]))
            for lp, (jj, kk) in zip(f.loops, ((j, k), (j + 1, k), (j + 1, k + 1), (j, k + 1))):
                lp[uvl].uv = (jj * 0.01, kk * 0.01)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); return o
def join_into(target, others):
    bpy.ops.object.select_all(action='DESELECT')
    for o in others: o.select_set(True)
    target.select_set(True); bpy.context.view_layer.objects.active = target; bpy.ops.object.join()
# the collar's top: 1 cm higher at the back than at the front (tailoring references)
def collar_top(th): return NECK.z + 0.010 + 0.030 * (1 - math.cos(th)) / 2
def lerp(a, b, t): return a + (b - a) * max(0.0, min(1.0, t))

def smooth_where(o, where, iters, factor=0.5):
    """Smooth only the points in a region (a junction, a crease), never the open edges."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    vs = [v for v in bm.verts if not v.is_boundary and where(v.co)]
    for _ in range(iters): bmesh.ops.smooth_vert(bm, verts=vs, factor=factor, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    bm.to_mesh(o.data); bm.free()

def bvh_of(o):
    bm = bmesh.new(); bm.from_mesh(o.data); bm.normal_update(); t = BVHTree.FromBMesh(bm); bm.free(); return t

def subdivide(o, levels=1):
    m = o.modifiers.new('Sub', 'SUBSURF'); m.levels = levels; m.render_levels = levels; m.uv_smooth = 'PRESERVE_CORNERS'
    bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier='Sub')

def thicken(o, t):
    m = o.modifiers.new('T', 'SOLIDIFY'); m.thickness = t; m.offset = -1.0; m.use_rim = True; m.use_even_offset = False   # (even offset spikes at sharp corners)
    bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier='T')

def boundary_loops(bm):
    edges = {e for e in bm.edges if e.is_boundary}; loops = []
    while edges:
        e = edges.pop(); loop = [e.verts[0], e.verts[1]]
        while True:
            nxt = [x for x in loop[-1].link_edges if x in edges]
            if not nxt: break
            edges.discard(nxt[0]); v = nxt[0].other_vert(loop[-1])
            if v == loop[0]: break
            loop.append(v)
        loops.append(loop)
    return loops

def extrude_loop(o, pick, move, only=lambda c: True):
    """Extrude the boundary loop chosen by pick(loop) and place each new vertex with move(co)."""
    bm = bmesh.new(); bm.from_mesh(o.data)
    loops = boundary_loops(bm)
    if not loops: bm.free(); return
    for l in loops: log(' loop', o.name, len(l), 'z', round(min(v.co.z for v in l), 3), round(max(v.co.z for v in l), 3), 'x', round(min(v.co.x for v in l), 3), round(max(v.co.x for v in l), 3))
    loop = pick(loops)
    lv = set(loop)
    es = [e for e in bm.edges if e.is_boundary and e.verts[0] in lv and e.verts[1] in lv and all(only(v.co) for v in e.verts)]
    r = bmesh.ops.extrude_edge_only(bm, edges=es)
    log('extruded', len(es), 'edges of a loop of', len(loop), 'on', o.name)
    for v in [g for g in r['geom'] if isinstance(g, bmesh.types.BMVert)]: v.co = move(v.co)
    bm.normal_update(); bm.to_mesh(o.data); bm.free()

MATS = {}
def material(name, rgb, rough):
    if name in MATS: return MATS[name]
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']; b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Roughness'].default_value = rough
    MATS[name] = m; return m
def metre_uvs(o):
    """A fresh UV layout (smart projection), scaled so one UV unit is one metre of cloth: the fabric's weave then
    has the same size everywhere on the garment, and its material tiles it per metre."""
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
    if not o.data.uv_layers: o.data.uv_layers.new(name='UVMap')
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.003, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode='OBJECT')
    me = o.data; uv = me.uv_layers.active.data; a3 = a2 = 0.0
    for poly in me.polygons:
        a3 += poly.area
        pts = [uv[li].uv for li in poly.loop_indices]
        a2 += abs(sum(pts[k].x * pts[(k + 1) % len(pts)].y - pts[(k + 1) % len(pts)].x * pts[k].y for k in range(len(pts)))) / 2
    k = math.sqrt(a3 / max(a2, 1e-9))
    for d in uv: d.uv = d.uv * k
def dress(o, mat):
    metre_uvs(o)
    o.data.materials.clear(); o.data.materials.append(mat)
    bpy.context.view_layer.objects.active = o; bpy.ops.object.shade_smooth()

TORSO = ('torso', 'upperarm', 'lowerarm', 'hips', 'neck')
NOT_SHIRT = ('hand', 'thigh', 'head', 'calf', 'foot', 'toes')
LEGS = ('hips', 'thigh', 'calf')
FEET = ('foot', 'toes')

# ---------------------------------------------------------------- the shirt
WAIST = 1.04     # the trousers' waistband (the player's hip joints are at 0.95 m, the neck at 1.55 m, the navel about 1.07 m)
def upper_body(i, bottom):
    # the torso and arms from this height up: not the hands, not the legs, not the head
    w = W[i]
    # (the belly carries a third of its weight on the thigh bones: the legs test only counts below the hip joints)
    legs_ok = CO[i].z > 0.97 or wsum(w, ('thigh', 'calf', 'foot', 'toes')) < 0.3
    return CO[i].z > bottom and wsum(w, ('hand',)) < 0.1 and legs_ok and wsum(w, ('head',)) < 0.3
def shirt_keep(i):
    return upper_body(i, WAIST - 0.12)
shirt = cut('Shirt', shirt_keep)
trim(shirt, Vector((0, 0, WAIST - 0.07)), Vector((0, 0, 1)))
cuffs(shirt, 0.012)
smooth(shirt, 14, 0.45)
offset(shirt, lambda c: 0.008)
smooth(shirt, 8, 0.35)
log('shirt pushed out at', keep_out(shirt, BODY_BVH, 0.005), 'points')
def highest(loops): return max(loops, key=lambda l: sum(v.co.z for v in l) / len(l))
clean_edges(shirt)
# the collar band, from below the shirt's neckline (a yoke over the shoulders' slope) up round the neck, closed at the
# front under the tie knot; and the fold-down collar over it, its spread points meeting either side of the knot
TH = [2 * math.pi * j / 64 for j in range(65)]
def band_foot(th):
    # the yoke reaches down over the chest only in the jacket's front opening; at the sides and back it stays near the
    # neck (posed, its edge poked through the jacket over the shoulders)
    front = max(0.0, math.cos(th)) ** 3
    return lerp(NECK.z - 0.07, NECK.z - 0.10, (1 - math.cos(th)) / 2) if False else lerp(NECK.z - 0.10, NECK.z - 0.07, (1 - math.cos(th)) / 2)   # (as before: the shards were not the yoke)
band = neck_tube('CollarBand', TH, lambda th: [band_foot(th) + k * (collar_top(th) - band_foot(th)) / 9 for k in range(10)],
                 lambda th, z, surf: surf + 0.010)   # room for the throat when the head tips forward
def leaf_rows(th):
    t = collar_top(th) + 0.002; a = abs(math.atan2(math.sin(th), math.cos(th)))
    depth = 0.034 + 0.036 * max(0.0, 1.0 - (a - 0.27) / 0.34)    # the spread points: longest at the inner edge by the knot, a straight edge back up
    return [t - depth * k / 6 for k in range(7)]
LEAF_TH = [0.27 + (2 * math.pi - 0.54) * j / 60 for j in range(61)]   # the points beside the tie's knot, not over it
leaf = neck_tube('CollarLeaf', LEAF_TH, leaf_rows, lambda th, z, surf: max(neck_surface(th, collar_top(th)) + 0.015 + 0.03 * (collar_top(th) - z), surf + 0.014))   # lying on the shirt
join_into(shirt, [band, leaf])
subdivide(shirt, 1)
log('shirt out of the skin after smoothing at', keep_out(shirt, BODY_BVH, 0.006), 'points')
pass   # no inner shell: posed, a doubled layer folded through the outer one (dark triangles); the material is two-sided
dress(shirt, material('Fabric_cotton_shirt', (0.92, 0.93, 0.95), 0.6))
SHIRT_BVH = bvh_of(shirt)

# ---------------------------------------------------------------- the trousers
def trousers_keep(i):
    w = W[i]; z = CO[i].z
    return wsum(w, ('upperarm', 'lowerarm', 'hand')) < 0.1 and wsum(w, FEET) < 0.6 and 0.0 < z < WAIST + 0.06
tr = cut('Trousers', trousers_keep)
trim(tr, Vector((0, 0, WAIST + 0.02)), Vector((0, 0, -1)))
trim(tr, Vector((0, 0, 0.035)), Vector((0, 0, 1)))
smooth(tr, 8, 0.45)
offset(tr, lambda c: 0.012)
# straight legs: below the seat each leg is pushed out to at least a tailored radius round its own centre at that
# height (6.6 cm at the hem, 9.5 cm at the top of the thigh), so the cloth falls straight instead of hugging the calf
bm = bmesh.new(); bm.from_mesh(tr.data)
CROTCH = 0.86
bins = {}
for v in bm.verts:
    if v.co.z < CROTCH:
        k = (v.co.x > 0, int(v.co.z / 0.02)); bins.setdefault(k, []).append(v.co.copy())
cent = {k: sum(p, Vector()) / len(p) for k, p in bins.items()}
for v in bm.verts:
    if v.co.z >= CROTCH: continue
    k = (v.co.x > 0, int(v.co.z / 0.02)); c = cent.get(k)
    if c is None: continue
    t = max(0.0, min(1.0, (v.co.z - 0.04) / (CROTCH - 0.04)))
    rmin = 0.072 + (0.10 - 0.072) * t
    d = v.co - c; d.z = 0
    # only the outside of each leg is let out; the inside seam stays (the legs would collide)
    out = abs(v.co.x) > abs(c.x) - 0.01 and v.co.z < CROTCH - 0.08
    if d.length < rmin and d.length > 1e-5 and out: v.co = c + d.normalized() * rmin + Vector((0, 0, v.co.z - c.z))
bm.to_mesh(tr.data); bm.free()
smooth(tr, 4, 0.3)
smooth_where(tr, lambda c: 0.74 < c.z < 0.98 and abs(c.x) < 0.10 and c.y < -0.02, 16, 0.5)   # a plain front, no anatomy
smooth_where(tr, lambda c: 0.60 < c.z < 0.80 and abs(c.x) < 0.07, 20, 0.5)   # the crotch and inner thighs: one smooth fall of cloth
log('trousers pushed out at', keep_out(tr, BODY_BVH, 0.008), 'points')
# the waistband: the top loop up 1 cm
def top_loop(loops): return highest(loops)
clean_edges(tr)
# the hems fall straight down over the shoes to 4 cm off the ground (the ankle's skin, cut away with the feet, showed
# between hem and shoe)
for _ in range(2):
    bm = bmesh.new(); bm.from_mesh(tr.data)
    for loop in [l for l in boundary_loops(bm) if sum(v.co.z for v in l) / len(l) < 0.25]:
        pass
        lv = set(loop); es = [e for e in bm.edges if e.is_boundary and e.verts[0] in lv and e.verts[1] in lv]
        r = bmesh.ops.extrude_edge_only(bm, edges=es)
        nv = [g for g in r['geom'] if isinstance(g, bmesh.types.BMVert)]
        cz = sum(v.co.z for v in loop) / len(loop)
        for v in nv: v.co.z = cz - (cz - 0.04) / 2
    bm.normal_update(); bm.to_mesh(tr.data); bm.free()
extrude_loop(tr, top_loop, lambda c: c + Vector((0, 0, 0.012)))
subdivide(tr, 1)
log('trousers out of the skin at', keep_out(tr, BODY_BVH, 0.009), 'points, over the shirt at', keep_out(tr, SHIRT_BVH, 0.004), 'points')
pass   # (as the shirt)
dress(tr, material('Fabric_wool_suiting', (0.035, 0.045, 0.075), 0.75))
TROUSERS_BVH = bvh_of(tr)

# ---------------------------------------------------------------- the jacket
def jacket_keep(i):
    w = W[i]; z = CO[i].z; c = CO[i]
    w_ = W[i]
    neck_ok = CO[i].z > NECK.z - 0.12 and wsum(w_, ('hand', 'thigh', 'calf', 'foot', 'toes')) < 0.1   # up to the neck seam
    if not (upper_body(i, WAIST - 0.10) or neck_ok): return False
    if 'lowerarm_l' in w or 'lowerarm_r' in w:
        pass
    # the sleeve ends 1.5 cm short of the shirt cuff
    if wsum(w, ('lowerarm',)) > 0.5 and wsum(w, ('hand',)) > 0.0: return False
    return z > WAIST - 0.06
jk = cut('Jacket', jacket_keep)
trim(jk, Vector((0, 0, WAIST - 0.04)), Vector((0, 0, 1)))
cuffs(jk, 0.035)
smooth(jk, 34, 0.5)        # a jacket's canvas front hides the chest's muscles
offset(jk, lambda c: 0.022)
smooth(jk, 8, 0.35)
log('jacket off the body at', keep_out(jk, BODY_BVH, 0.016), 'points')
# the skirt: the bottom loop dropped to 0.78 m, falling straight from the hips (its ring let out to the seat's width)
def lowest(loops):
    # the hem: of the loops round the body's middle (the cuffs are out at the sides), the lowest
    mids = [l for l in loops if min(v.co.x for v in l) < 0 < max(v.co.x for v in l)] or loops
    return min(mids, key=lambda l: sum(v.co.z for v in l) / len(l))
def skirt_move(c):
    hip_c = Vector((0.0, -0.03, c.z)); d = c - hip_c; d.z = 0
    r = max(d.length, 0.0) + 0.015
    return hip_c + d.normalized() * r + Vector((0, 0, 0.80 - c.z))
# the skirt (from the waist to below the seat), grown down from the jacket's own hem (a clean loop, cut by a plane), so
# jacket and skirt are one piece of cloth: row by row each point falls straight from the widest part of the hips under
# it (2 cm clear), down to 0.80 m; the fronts then curve away below the button (a cut-away)
def body_radius(theta, z, start=0.30):
    d = Vector((math.sin(theta), -math.cos(theta), 0.0)); c = Vector((0.0, -0.03, z))
    hit = BODY_BVH.ray_cast(c + d * start, -d, start)
    return (hit[0] - c).length if hit[0] is not None else None
HIP = {}
def hip_r(th):
    k = round(th, 2)
    if k not in HIP:
        rs = [body_radius(th, z) for z in (0.80, 0.84, 0.88, 0.92, 0.96, 1.0)]
        HIP[k] = max([r for r in rs if r] or [0.17])
    return HIP[k]
bm = bmesh.new(); bm.from_mesh(jk.data)
_loops = boundary_loops(bm)
_hem = set(min(_loops, key=lambda l: sum(v.co.z for v in l) / len(l)))   # the hem: the lowest loop
hem = [e for e in bm.edges if e.is_boundary and e.verts[0] in _hem and e.verts[1] in _hem]
for v in _hem: v.co.z = min(v.co.z, WAIST - 0.03)
log('hem edges', len(hem))
ROWS = [0.975, 0.95, 0.925, 0.90, 0.875, 0.85, 0.825, 0.80]
for z in ROWS:
    r = bmesh.ops.extrude_edge_only(bm, edges=hem)
    for v in [g for g in r['geom'] if isinstance(g, bmesh.types.BMVert)]:
        c = Vector((0.0, -0.03, 0.0)); d = Vector((v.co.x, v.co.y + 0.03, 0.0)); th = math.atan2(d.x, -d.y)
        rad = max(d.length, hip_r(th) + 0.02)
        v.co = Vector((math.sin(th) * rad, -0.03 - math.cos(th) * rad, z))
    hem = [g for g in r['geom'] if isinstance(g, bmesh.types.BMEdge) and all(abs(v.co.z - z) < 1e-6 for v in g.verts)]
def cutaway(c):
    a = abs(math.atan2(c.x, -(c.y + 0.03)))
    return c.z < WAIST - 0.03 and a < math.radians(3 + 34 * max(0.0, min(1.0, (WAIST - 0.03 - c.z) / (WAIST - 0.03 - 0.80))) ** 2.6)   # opens mostly near the hem
bmesh.ops.delete(bm, geom=[f for f in bm.faces if cutaway(f.calc_center_median())], context='FACES')
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.normal_update(); bm.to_mesh(jk.data); bm.free()
smooth(jk, 3, 0.3)
smooth_where(jk, lambda c: 0.92 < c.z < 1.08, 12, 0.5)   # one smooth line from chest to skirt
# the jacket's collar (owner: flat at the back, no ledge, no crumples): the jacket's own top edge (the body's neck
# seam, a clean ring) rises in two rows to 1.2 cm under the shirt collar's top, closing in to 1.5 cm off the neck;
# the front opening below then cuts through it, leaving the notch beside the lapels
def top_ring(loops): return max(loops, key=lambda l: sum(v.co.z for v in l) / len(l))
def collar_row(frac):
    def move(c):
        d = Vector((c.x - NECK.x, c.y - NECK.y, 0.0)); a_ = math.atan2(d.x, -d.y)
        z = lerp(c.z, collar_top(a_) - 0.012, frac)
        r = lerp(d.length, neck_surface(a_, z) + 0.015, frac)
        return Vector((NECK.x + math.sin(a_) * r, NECK.y - math.cos(a_) * r, z))
    return move
extrude_loop(jk, top_ring, collar_row(0.5))
extrude_loop(jk, top_ring, collar_row(1.0))
# the front opening, cut after the hem is made: a V from the button (1.12 m) up to the collar, a narrow parting below
bm = bmesh.new(); bm.from_mesh(jk.data)
def opening(c):
    if c.y > -0.05: return False
    if c.z > 1.07: return abs(c.x) < 0.010 + (c.z - 1.07) * 0.21   # two-button: the button at the navel
    return False
bmesh.ops.delete(bm, geom=[f for f in bm.faces if opening(f.calc_center_median())], context='FACES')
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(jk.data); bm.free()
clean_edges(jk, 5)   # (more rounds the V's point into a U)
subdivide(jk, 1)
log('jacket out of the skin at', keep_out(jk, BODY_BVH, 0.018, 0.05), 'points')
# a soft last pass: the canvas front lies smooth over the chest instead of showing its muscles
smooth(jk, 14, 0.45); keep_out(jk, BODY_BVH, 0.008, 0.05)
NEAR_COLLAR = lambda c: c.z > NECK.z - 0.16 and Vector((c.x - NECK.x, c.y - NECK.y, 0)).length < 0.26
smooth_where(jk, NEAR_COLLAR, 16, 0.5)   # the shoulder tops round the collar: no lumps
keep_out(jk, BODY_BVH, lambda c: 0.004 if NEAR_COLLAR(c) else 0.010, 0.05)   # (bridging the shoulder muscles there)
smooth_where(jk, lambda c: c.y > 0.0 and abs(c.x) < 0.06 and 1.05 < c.z < 1.45, 10, 0.5)   # no crease down the spine (owner)
log('jacket over the shirt at', keep_out(jk, SHIRT_BVH, 0.006), 'points')
# the last soft pass over the waist and the skirt's top, then kept clear of what is under it
WAISTBAND = lambda c: 0.86 < c.z < 1.12
smooth_where(jk, WAISTBAND, 16, 0.5)
keep_out(jk, BODY_BVH, lambda c: 0.021 if WAISTBAND(c) else 0.008, 0.06)
smooth_where(jk, WAISTBAND, 4, 0.3)
# ---- the jacket's details, from the references (a single-breasted two-button jacket):
GORGE = NECK.z - 0.11                                     # where the lapel meets the collar (the notch)
def v_half(z): return 0.010 + (z - 1.07) * 0.21
def lapel_outer(z): return lerp(0.012, v_half(GORGE) + 0.078, (z - 1.07) / (GORGE - 1.07))   # straight from the button to the notch
# the lapels (owner: straight, even edges; no layer cutting through the jacket when posed): each lapel is its own clean
# strip of cloth, laid in rows exactly along the straight line from the button to the notch, shaped onto the jacket
# and 4 mm proud of it; the jacket's own cloth under it is removed (so there's no doubled layer), the lapel tapering to
# nothing at the button
JK0 = bvh_of(jk)
def x_line(z): return lerp(0.012, lapel_outer(GORGE), (z - 1.09) / (GORGE - 1.09))
bm = bmesh.new(); bm.from_mesh(jk.data)
def under_lapel(c):
    if c.y > -0.02 or c.z < 1.095 or c.z > GORGE - 0.004: return False
    return abs(c.x) < x_line(c.z) - 0.011 and abs(c.x) > 0.0   # (well inside the strip: the cloth removed never peeks out past its edge)
bmesh.ops.delete(bm, geom=[f for f in bm.faces if all(under_lapel(v.co) for v in f.verts)], context='FACES')   # (whole faces under the strip only: no hole may reach past its edge)
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
# the jacket left under the strip's edge sinks away from it (3 mm deeper, over the hidden band): posed in the game it
# poked up through the strip; with 7 mm between them it can't
bm.normal_update()
for v in bm.verts:
    c = v.co
    if c.y > -0.02 or c.z < 1.095 or c.z > GORGE - 0.002: continue
    d_in = x_line(c.z) - abs(c.x)
    if d_in > 0.0015: v.co -= v.normal * 0.003 * min(1.0, (d_in - 0.0015) / 0.003)
bm.to_mesh(jk.data); bm.free()
strips = []
for side in (1, -1):
    sb = bmesh.new(); R, C = 30, 10; grid = []
    for r in range(R + 1):
        z = lerp(1.09, GORGE, r / R)
        x0 = min(v_half(z) + 0.004, x_line(z) - 0.001); x1 = x_line(z)   # (4 mm in: every ray lands on the jacket)
        lift = 0.004 * max(0.0, min(1.0, (z - 1.09) / 0.12))
        row = []
        for c in range(C + 1):
            x = side * lerp(x0, x1, c / C)
            hit = JK0.ray_cast(Vector((x, -0.6, z)), Vector((0, 1, 0)))
            p_ = (hit[0] + hit[1] * lift) if (hit[0] is not None and hit[0].y < -0.02) else Vector((x, -0.12, z))
            row.append(sb.verts.new(p_))
        grid.append(row)
    for r in range(R):
        for c in range(C):
            q = (grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c])
            sb.faces.new(q if side > 0 else q[::-1])
    bmesh.ops.recalc_face_normals(sb, faces=sb.faces)
    me_ = bpy.data.meshes.new('Lapel'); sb.to_mesh(me_); sb.free()
    o_ = bpy.data.objects.new('Lapel', me_); sc.collection.objects.link(o_); o_.data.uv_layers.new(name='UVMap')
    strips.append(o_)
join_into(jk, strips)
bm = bmesh.new(); bm.from_mesh(jk.data)
bm.to_mesh(jk.data); bm.free()
bm = bmesh.new(); bm.from_mesh(jk.data)
bm.to_mesh(jk.data); bm.free()
# the pockets: a welt on the left breast, flaps at the hips; and the two buttons (references: two-button jacket)
JK_BVH = bvh_of(jk)
def side_patch(name, th, z, w, h, lift, n=8):
    """A patch (a pocket flap) on the jacket round the body: centred at angle th from the front (0) at height z, w wide
    along the surface, h tall; each point shot onto the jacket from outside, square to the body's axis."""
    pb = bmesh.new(); grid = []
    r0 = hip_r(th) + 0.02
    for i in range(n + 1):
        a = th + (i / n - 0.5) * w / r0
        d = Vector((math.sin(a), -math.cos(a), 0.0))
        row = []
        for j in range(3):
            pz = z - h / 2 + h * j / 2
            c = Vector((0.0, -0.03, pz))
            hit = JK_BVH.ray_cast(c + d * 0.5, -d, 0.5)
            if hit[0] is None: pb.free(); return None
            row.append(pb.verts.new(hit[0] + hit[1] * lift))
        grid.append(row)
    for i in range(n):
        for j in range(2): pb.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    me = bpy.data.meshes.new(name); pb.to_mesh(me); pb.free()
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); return o
def patch(name, x, z, w, h, lift, n=6):
    pb = bmesh.new(); grid = []
    for i in range(n + 1):
        row = []
        for j in range(3):
            px = x - w / 2 + w * i / n; pz = z - h / 2 + h * j / 2
            hit = JK_BVH.ray_cast(Vector((px, -0.6, pz)), Vector((0, 1, 0)))
            if hit[0] is None or hit[0].y > -0.02: pb.free(); return None   # only on the jacket's front (a ray through the opening would land on the back)
            row.append(pb.verts.new(hit[0] + hit[1] * lift))
        grid.append(row)
    for i in range(n):
        for j in range(2): pb.faces.new((grid[i][j], grid[i + 1][j], grid[i + 1][j + 1], grid[i][j + 1]))
    me = bpy.data.meshes.new(name); pb.to_mesh(me); pb.free()
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); return o
details = [o for o in (patch('ChestWelt', 0.12, 1.30, 0.10, 0.022, 0.0025),
           side_patch('FlapL', math.radians(55), 0.95, 0.15, 0.05, 0.0025), side_patch('FlapR', -math.radians(55), 0.95, 0.15, 0.05, 0.0025)) if o]   # on the sides of the hips (owner)
join_into(jk, details)
smooth_where(jk, lambda c: c.y > 0.0 and c.z > NECK.z - 0.20 and abs(c.x) < 0.14, 10, 0.5)   # the back below the collar: no crumples (owner)
pass   # (as the shirt: single-layer cloth, two-sided material)
dress(jk, material('Fabric_wool_suiting', (0.035, 0.045, 0.075), 0.75))
buttons = []
for z in (1.07, 0.97):
    hit = JK_BVH.ray_cast(Vector((-0.012, -0.6, z)), Vector((0, 1, 0)))
    if hit[0] is None: continue
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.0095, depth=0.003, location=hit[0] - Vector((0, 0.004, 0)), rotation=(math.radians(90), 0, 0))
    buttons.append(bpy.context.object)
if buttons:
    bpy.ops.object.select_all(action='DESELECT')
    for b_ in buttons: b_.select_set(True)
    bpy.context.view_layer.objects.active = buttons[0]; bpy.ops.object.join(); btn = bpy.context.object; btn.name = 'Buttons'
    dress(btn, material('Fabric_leather', (0.02, 0.018, 0.016), 0.35))
    join_into(jk, [btn])

# ---------------------------------------------------------------- the tie (reference: a four-in-hand knot, Wikimedia
# Commons "Four-In-Hand tie knot"): the knot an inverted, rounded wedge from the top of the collar band, 3.8 cm wide
# narrowing to 1.6 cm over 4.6 cm, leaning a little, a soft diagonal fold across it, standing about 1 cm off the shirt;
# the blade comes out under it with a small dimple, widens to 8 cm and runs on under the jacket's button
KTOP = collar_top(0.0) + 0.002; KH = 0.046
def shirt_y(x, z, fallback=-0.15):
    hit = SHIRT_BVH.ray_cast(Vector((x, -0.5, z)), Vector((0, 1, 0)))
    return hit[0].y if hit[0] is not None else fallback
kb = bmesh.new(); kgrid = []; KR, KC = 14, 13
for r in range(KR + 1):
    t = r / KR; z = KTOP - KH * t
    half = lerp(0.020, 0.011, t ** 1.4); xc = 0.003 * t                        # narrowing, leaning a little
    stand = lerp(0.012, 0.008, t)
    row = []
    for c in range(KC + 1):
        u = c / KC * 2 - 1                                                       # -1 .. 1 across the knot
        x = xc + u * half
        bulge = 0.004 * (1 - u * u)                                              # rounded across
        fold = 0.0012 * math.exp(-((u - (1.6 * t - 0.8)) / 0.18) ** 2)          # the wrap's soft diagonal fold
        wrap = 0.004 * (abs(u) ** 4)                                             # the sides wrap back toward the shirt
        y = shirt_y(x, z) - (stand + bulge - fold - wrap)
        row.append(kb.verts.new((x, y, z)))
    kgrid.append(row)
for r in range(KR):
    for c in range(KC): kb.faces.new((kgrid[r][c], kgrid[r][c + 1], kgrid[r + 1][c + 1], kgrid[r + 1][c]))
bmesh.ops.recalc_face_normals(kb, faces=kb.faces)
kme = bpy.data.meshes.new('Knot'); kb.to_mesh(kme); kb.free()
knot = bpy.data.objects.new('Knot', kme); sc.collection.objects.link(knot); knot.data.uv_layers.new(name='UVMap')
subdivide(knot, 1)

bm = bmesh.new()
rows = 40; top, bot = KTOP - KH + 0.006, 1.08   # the blade from under the knot to under the jacket's button
last_y = -0.15
COLS_T = 9
for r in range(rows + 1):
    z = top - (top - bot) * r / rows; dz = top - z; to_tip = z - bot
    half = lerp(0.009, 0.040, dz / 0.11)
    if to_tip < 0.04: half *= max(0.02, to_tip / 0.04)
    stand = 0.0025 + 0.004 * max(0.0, 1.0 - dz / 0.03)      # out from under the knot, then close on the shirt
    for c in range(COLS_T):
        u = c / (COLS_T - 1) * 2 - 1; x = u * half
        dimple = 0.002 * math.exp(-(u / 0.25) ** 2) * max(0.0, 1.0 - dz / 0.035)   # the dimple under the knot
        y = shirt_y(x, z, last_y) - stand + dimple
        last_y = y
        bm.verts.new((x, y, z))
bm.verts.ensure_lookup_table()
for r in range(rows):
    for c in range(COLS_T - 1):
        a_ = r * COLS_T + c
        bm.faces.new((bm.verts[a_], bm.verts[a_ + 1], bm.verts[a_ + 1 + COLS_T], bm.verts[a_ + COLS_T]))
me = bpy.data.meshes.new('Tie'); bm.to_mesh(me); bm.free()
tie = bpy.data.objects.new('Tie', me); sc.collection.objects.link(tie)
tie.data.uv_layers.new(name='UVMap')
smooth(tie, 6, 0.5)
thicken(tie, 0.0015)
thicken(knot, 0.0015)
join_into(tie, [knot])
dress(tie, material('Fabric_satin', (0.32, 0.03, 0.06), 0.35))

# ---------------------------------------------------------------- the shoes
# the shoes: a dress shoe's last measured from each foot (its length, its width and height along it), a smooth upper
# from the heel round the toe, the top at the instep and ankle (the trousers cover it), and a sole 1.2 cm thick a little
# wider than the upper
def shoe(side):
    pts = [CO[i] for i in range(len(CO)) if CO[i].z < 0.13 and CO[i].x * side > 0 and wsum(W[i], ('foot', 'toes')) > 0.3]
    ys = [p.y for p in pts]; y_heel, y_toe = max(ys) + 0.010, min(ys) - 0.016   # front is -y; the toe 1.6 cm past the toes
    cx = sum(p.x for p in pts) / len(pts)
    N, R = 26, 18
    bm = bmesh.new(); rings = []
    for k in range(N + 1):
        t = k / N; y = y_heel + (y_toe - y_heel) * t
        band = [p for p in pts if abs(p.y - y) < 0.012] or pts
        x0, x1 = min(p.x for p in band), max(p.x for p in band); zt = max(p.z for p in band)
        hw = (x1 - x0) / 2 + 0.007; xc = (x0 + x1) / 2
        # an oxford's line: heel counter 6.8 cm, rising to the lacing's throat (7.5 cm), the vamp falling to a 4 cm toe
        top = 0.085 if t < 0.30 else lerp(0.085, 0.080, (t - 0.30) / 0.15) if t < 0.45 else lerp(0.080, 0.045, (t - 0.45) / 0.40)   # up round the ankle
        top = max(top, min(zt + 0.008, 0.09))
        # the toe rounds off over its last 18 %, the heel over its first 8 %
        if t > 0.80: f = math.sqrt(max(0.0, 1 - ((t - 0.80) / 0.20) ** 2)); hw *= max(0.08, f); top = 0.016 + (top - 0.016) * max(0.12, f)   # a round toe closing smoothly (an ellipse in plan, no flat end)
        if t < 0.08: f = math.sqrt(max(0.0, 1 - ((0.08 - t) / 0.08) ** 2)); hw *= max(0.35, f)
        ring = []
        for j in range(R):
            a = 2 * math.pi * j / R
            base = 0.026 if t < 0.20 else lerp(0.026, 0.012, (t - 0.20) / 0.14)   # the heel, then the shank down to the sole
            cap = 0.9985 if 0.78 < t < 0.80 else 1.0                                 # the toe cap's seam, a fine groove
            x = xc + hw * cap * math.cos(a); z = base + (top - base) * max(0.0, math.sin(a)) ** 0.7 * cap if math.sin(a) > 0 else base
            ring.append(bm.verts.new((x, y, z)))
        rings.append(ring)
    for k in range(N):
        for j in range(R):
            bm.faces.new((rings[k][j], rings[k][(j + 1) % R], rings[k + 1][(j + 1) % R], rings[k + 1][j]))
    for ring in (rings[0], rings[-1]):
        c = bm.verts.new(sum((v.co for v in ring), Vector()) / R)
        for j in range(R): bm.faces.new((ring[j], ring[(j + 1) % R], c))
    RING = [(min(v.co.x for v in r), max(v.co.x for v in r), r[0].co.y) for r in rings]   # (kept: the bmesh is freed below)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new('Shoe'); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new('Shoe', me); sc.collection.objects.link(o)
    subdivide(o, 2)                                  # the upper smooth (the sole stays crisp, made apart)
    # the sole: the upper's outline 3 mm wider (its width smoothed along the foot, so the edge is an even curve), a strip
    # of quads from heel to toe, flat underneath, its top following the heel block and the shank (owner: even lines)
    xl = [r[0] - 0.003 for r in RING]; xr = [r[1] + 0.003 for r in RING]
    for _ in range(6):
        xl = [xl[0]] + [(xl[i - 1] + 2 * xl[i] + xl[i + 1]) / 4 for i in range(1, len(xl) - 1)] + [xl[-1]]
        xr = [xr[0]] + [(xr[i - 1] + 2 * xr[i] + xr[i + 1]) / 4 for i in range(1, len(xr) - 1)] + [xr[-1]]
    sb = bmesh.new(); M_ = 6; grid_lo = []; grid_hi = []
    for k, r in enumerate(RING):
        t = k / N; y = r[2]
        ztop = 0.026 if t < 0.20 else lerp(0.026, 0.012, (t - 0.20) / 0.14)
        grid_lo.append([sb.verts.new((lerp(xl[k], xr[k], m / M_), y, 0.0)) for m in range(M_ + 1)])
        grid_hi.append([sb.verts.new((lerp(xl[k], xr[k], m / M_), y, ztop)) for m in range(M_ + 1)])
    for k in range(N):
        for m in range(M_):
            sb.faces.new((grid_lo[k][m + 1], grid_lo[k][m], grid_lo[k + 1][m], grid_lo[k + 1][m + 1]))
            sb.faces.new((grid_hi[k][m], grid_hi[k][m + 1], grid_hi[k + 1][m + 1], grid_hi[k + 1][m]))
        for m in (0, M_):
            sb.faces.new((grid_lo[k][m], grid_hi[k][m], grid_hi[k + 1][m], grid_lo[k + 1][m]))
    for k in (0, N):
        for m in range(M_): sb.faces.new((grid_lo[k][m], grid_lo[k][m + 1], grid_hi[k][m + 1], grid_hi[k][m]))
    bmesh.ops.recalc_face_normals(sb, faces=sb.faces)
    sme = bpy.data.meshes.new('Sole'); sb.to_mesh(sme); sb.free()
    so = bpy.data.objects.new('Sole', sme); sc.collection.objects.link(so)
    join_into(o, [so])
    # the laces: five across the instep, over the lacing's throat
    laces = []
    for k in range(5):
        t = 0.47 + 0.035 * k; y = y_heel + (y_toe - y_heel) * t
        band = [p for p in pts if abs(p.y - y) < 0.012] or pts
        xc = (min(p.x for p in band) + max(p.x for p in band)) / 2
        hit = BODY_BVH.ray_cast(Vector((xc, y, 0.3)), Vector((0, 0, -1)))
        ztop = (hit[0].z if hit[0] is not None else 0.06) + 0.012
        bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.0022, depth=0.034 - 0.004 * k, location=(xc, y, ztop), rotation=(0, math.radians(90), 0))
        laces.append(bpy.context.object)
    join_into(o, laces)
    return o
shoes = [shoe(1), shoe(-1)]
bpy.ops.object.select_all(action='DESELECT')
for o in shoes: o.select_set(True)
bpy.context.view_layer.objects.active = shoes[0]; bpy.ops.object.join(); sh = bpy.context.object; sh.name = 'Shoes'
log('shoes off the foot at', keep_out(sh, BODY_BVH, 0.004), 'points')
dress(sh, material('Fabric_leather', (0.02, 0.018, 0.016), 0.35))

# ---------------------------------------------------------------- one object, exported in the body's space
pieces = [shirt, tr, jk, tie, sh]
def peek(objs, name, side=False):
    """A quick grey picture of some pieces (front or side) for checking: dbg_<name>.png."""
    hidden = [o for o in sc.objects if o.type == 'MESH' and o not in objs]
    for o in hidden: o.hide_render = True
    cam = bpy.data.objects.get('PeekCam') or bpy.data.objects.new('PeekCam', bpy.data.cameras.new('PeekCam'))
    if cam.name not in sc.collection.objects: sc.collection.objects.link(cam)
    sc.camera = cam; cam.data.type = 'ORTHO'; cam.data.ortho_scale = 1.3
    cam.location = (3, -0.03, 0.75) if side else (0, -3, 0.75); cam.rotation_euler = (1.5708, 0, 1.5708 if side else 0)
    sc.render.engine = 'BLENDER_WORKBENCH'; sc.render.resolution_x = 600; sc.render.resolution_y = 900
    sc.render.filepath = str(HERE / ('dbg_%s.png' % name)); bpy.ops.render.render(write_still=True)
    for o in hidden: o.hide_render = False
peek([tr], 'trousers'); peek([tr], 'trousers_side', True); peek([jk], 'jacket'); peek([shirt], 'shirt')
import os
ONLY = os.environ.get('SUIT_ONLY', '')   # for testing one piece in Unreal: SUIT_ONLY=Shirt,Tie (exported under NAME_<only>)
if ONLY:
    keep = [o for o in pieces if o.name in ONLY.split(',')]
    for o in pieces:
        if o not in keep: bpy.data.objects.remove(o, do_unlink=True)
    pieces = keep; NAME = NAME + 'Only'
    (HERE.parent / NAME).mkdir(exist_ok=True); HERE = HERE.parent / NAME
for o in pieces: log(o.name, len(o.data.vertices), 'verts', tuple(round(d, 3) for d in o.dimensions))
bpy.ops.object.select_all(action='DESELECT')
for o in pieces: o.select_set(True)
bpy.context.view_layer.objects.active = shirt
bpy.ops.object.join(); suit = bpy.context.object; suit.name = NAME
bpy.ops.object.select_all(action='DESELECT'); suit.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(HERE / (NAME + '.fbx')), use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', add_leaf_bones=False)
log('slots', [m.name for m in suit.data.materials])
# each slot's colour, for Unreal (Tools/make_outfit.py tints the fabric with it)
import json
json.dump({m.name: list(m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value)[:3] for m in suit.data.materials},
          open(HERE / (NAME + '.json'), 'w'), indent=1)

# a look at it on the body (front three-quarter), for checking by eye
for o in list(sc.objects):
    if o.type == 'MESH' and o not in (suit, src): bpy.data.objects.remove(o, do_unlink=True)
src.data.materials.clear(); src.data.materials.append(material('Skin', (0.55, 0.38, 0.30), 0.5))
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50; tgt = Vector((0, -0.1, 0.9))
cam.location = tgt + Vector((-1.4, -3.2, 0.35)); cam.rotation_euler = (tgt - cam.location).to_track_quat('-Z', 'Y').to_euler()
for nm, loc, e in (('Key', (-2, -3, 3), 900), ('Fill', (2.5, -2, 1.5), 350), ('Rim', (0, 3, 2.5), 500)):
    ld = bpy.data.lights.new(nm, 'AREA'); ld.energy = e; ld.size = 2
    lo = bpy.data.objects.new(nm, ld); lo.location = loc; lo.rotation_euler = (Vector((0, 0, 1)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); sc.collection.objects.link(lo)
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.4
sc.render.engine = 'BLENDER_EEVEE'; sc.render.resolution_x = 900; sc.render.resolution_y = 1200
sc.render.filepath = str(HERE / 'preview.png'); bpy.ops.render.render(write_still=True)
cam.location = tgt + Vector((1.6, 3.0, 0.3)); cam.rotation_euler = (tgt - cam.location).to_track_quat('-Z', 'Y').to_euler()
sc.render.filepath = str(HERE / 'preview_back.png'); bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / (NAME + '.blend')))
log('DONE')
