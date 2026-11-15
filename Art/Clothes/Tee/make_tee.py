# The crew-neck t-shirt (owner, 2026-11-13; reference Art/Clothes/_references/Tee: a plain cotton crew-neck from front,
# side and back): tailored on the player's body with the suit's tools (Art/Clothes/CEO_SuitClassic/make_ceo_suit.py's
# helpers, so every fix learnt there carries over): a relaxed body falling straight from the chest to just below the
# belt, short sleeves square to the arm half-way to the elbow, a 2 cm ribbed crew collar round the neck; single-layer
# cloth (two-sided fabric). The front print is its own material slot (Print_<name>) laid flat over the chest with its
# own UVs, so any print can be swapped in later without touching the tee.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Clothes/Tee/make_tee.py
import bpy, bmesh, math
from mathutils import Vector
from pathlib import Path

HERE = Path(__file__).resolve().parent
SUIT = HERE.parent / 'CEO_SuitClassic' / 'make_ceo_suit.py'
_src = SUIT.read_text(encoding='utf-8')
_g = {'__file__': str(HERE / 'make_tee.py'), '__name__': 'tee'}
exec(_src[:_src.index('# ---------------------------------------------------------------- the shirt')], _g)   # body, fitting tools
globals().update({k: v for k, v in _g.items() if not k.startswith('__')})
NAME = 'Tee'
log = lambda *a: print('TEE', *a)

# ---------------------------------------------------------------- the body of the tee
HEM = 0.90                     # just below the belt (the reference hangs over the waistband)
def tee_keep(i):
    w = W[i]
    legs_ok = CO[i].z > 0.97 or wsum(w, ('thigh', 'calf', 'foot', 'toes')) < 0.3
    neck_ok = CO[i].z < NECK.z - 0.035 or wsum(w, ('head',)) < 0.3          # the tee's body reaches up the neck's base (a crew neck)
    return CO[i].z > HEM - 0.05 and wsum(w, ('hand', 'lowerarm')) < 0.2 and legs_ok and neck_ok and CO[i].z < NECK.z - 0.035
tee = cut('Tee', tee_keep)
trim(tee, Vector((0, 0, HEM)), Vector((0, 0, 1)))
# the sleeves: cut square to the upper arm, 45 % of the way from the shoulder to the elbow
for s_ in ('l', 'r'):
    sh = bone('upperarm_' + s_); el = bone('lowerarm_' + s_); d = (el - sh).normalized()
    c = sh + (el - sh) * 0.45
    bm = bmesh.new(); bm.from_mesh(tee.data)
    geom = [f for f in bm.faces if (f.calc_center_median() - sh).dot(d) > 0.02 and (f.calc_center_median() - sh).length < 0.40]
    bmesh.ops.bisect_plane(bm, geom=list({v for f in geom for v in f.verts}) + list({e for f in geom for e in f.edges}) + geom, plane_co=c, plane_no=-d, clear_outer=False, clear_inner=False)
    side_ = 1 if sh.x > 0 else -1   # only the arm (out past the armpit), never the torso below it
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if (f.calc_center_median() - c).dot(d) > 0 and f.calc_center_median().x * side_ > abs(sh.x) * 0.95 and (f.calc_center_median() - sh).length < 0.40], context='FACES')
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bm.to_mesh(tee.data); bm.free()
# relaxed: the suit shirt's proven steps (smooth, offset, smooth, out of the skin), with more ease
smooth(tee, 14, 0.45)
offset(tee, lambda c: 0.014)
smooth(tee, 8, 0.35)
log('tee off the skin at', keep_out(tee, BODY_BVH, 0.010), 'points')
clean_edges(tee)
# the crew collar: a 2 cm ribbed band round the base of the neck, from the tee's own neckline
def highest(loops): return max(loops, key=lambda l: sum(v.co.z for v in l) / len(l))
def rib(frac):
    def move(c):
        d = Vector((c.x - NECK.x, c.y - NECK.y, 0.0)); a_ = math.atan2(d.x, -d.y)
        z = c.z + 0.016 * frac                                  # a 1.6 cm ribbed band standing up from the neckline
        r = lerp(d.length, neck_surface(a_, z) + 0.006, frac)   # snug round the neck
        return Vector((NECK.x + math.sin(a_) * r, NECK.y - math.cos(a_) * r, z))
    return move
extrude_loop(tee, highest, rib(0.5)); extrude_loop(tee, highest, rib(1.0))
subdivide(tee, 1)
log('tee out of the skin after smoothing at', keep_out(tee, BODY_BVH, 0.009), 'points')
dress(tee, material('Fabric_cotton_jersey', (0.93, 0.93, 0.91), 0.8))

# ---------------------------------------------------------------- the shorts (to-do 49, owner 2026-11-15: "his shorts are
# invisible" - the tee outfit held only the tee, so the hips looked see-through and cast no shadow). Casual sweat shorts
# in charcoal fleece, as the trousers of the suit (its proven steps) but cut 6 cm above the knee: a 1.2 cm waistband
# under the tee's hem, legs falling straight and a little wide, a plain front; the tee kept 9 mm over them.
SHORTS_TOP = 1.00; SHORTS_HEM = bone('calf_l').z + 0.06            # (the knee joint + 6 cm)
def shorts_keep(i):
    w = W[i]; z = CO[i].z
    return wsum(w, ('upperarm', 'lowerarm', 'hand')) < 0.1 and SHORTS_HEM - 0.06 < z < SHORTS_TOP + 0.06
sh_ = cut('Shorts', shorts_keep)
bm = bmesh.new(); bm.from_mesh(sh_.data); bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005); bm.to_mesh(sh_.data); bm.free()   # (the body's seams are doubled)
trim(sh_, Vector((0, 0, SHORTS_TOP)), Vector((0, 0, -1)))
trim(sh_, Vector((0, 0, SHORTS_HEM)), Vector((0, 0, 1)))
smooth(sh_, 8, 0.45)
offset(sh_, lambda c: 0.018)                                         # (looser than trousers: sweat shorts)
bm = bmesh.new(); bm.from_mesh(sh_.data)
CROTCH = 0.86; bins = {}
for v in bm.verts:
    if v.co.z < CROTCH:
        k = (v.co.x > 0, int(v.co.z / 0.02)); bins.setdefault(k, []).append(v.co.copy())
cent = {k: sum(p, Vector()) / len(p) for k, p in bins.items()}
for v in bm.verts:
    if v.co.z >= CROTCH: continue
    k = (v.co.x > 0, int(v.co.z / 0.02)); c = cent.get(k)
    if c is None: continue
    t = max(0.0, min(1.0, (v.co.z - SHORTS_HEM) / (CROTCH - SHORTS_HEM)))
    rmin = 0.098 + (0.104 - 0.098) * t                                 # (a little wide: the leg falls straight to the hem)
    d = v.co - c; d.z = 0
    out = abs(v.co.x) > abs(c.x) - 0.01 and v.co.z < CROTCH - 0.06
    if d.length < rmin and d.length > 1e-5 and out: v.co = c + d.normalized() * rmin + Vector((0, 0, v.co.z - c.z))
bm.to_mesh(sh_.data); bm.free()
smooth(sh_, 4, 0.3)
# cloth spans hollows, it does not follow them in (the first previews showed the anatomy at the front and a seat tight
# as briefs): every 1 cm slice of the shorts is stretched over its own convex outline - one outline above the crotch,
# one per leg below it - each point moved out from the slice's middle onto that outline
from mathutils import geometry as _geo
def span_hollows(o, crotch, top, legs=True):
    bm = bmesh.new(); bm.from_mesh(o.data); groups = {}
    for v in bm.verts:
        if v.co.z > top or (not legs and v.co.z < crotch): continue
        k = (int(v.co.z / 0.01), 0 if v.co.z >= crotch else (1 if v.co.x > 0 else -1)); groups.setdefault(k, []).append(v)
    moved = 0
    for vs in groups.values():
        if len(vs) < 8: continue
        pts = [(v.co.x, v.co.y) for v in vs]; hull = [pts[i_] for i_ in _geo.convex_hull_2d(pts)]
        cx = sum(p_[0] for p_ in hull) / len(hull); cy = sum(p_[1] for p_ in hull) / len(hull)
        for v in vs:
            dx, dy = v.co.x - cx, v.co.y - cy; L = math.hypot(dx, dy)
            if L < 1e-5: continue
            best = None
            for a_ in range(len(hull)):                                 # (where the ray from the middle leaves the outline)
                (x1, y1), (x2, y2) = hull[a_], hull[(a_ + 1) % len(hull)]
                den = dx * (y2 - y1) - dy * (x2 - x1)
                if abs(den) < 1e-12: continue
                t_ = ((x1 - cx) * (y2 - y1) - (y1 - cy) * (x2 - x1)) / den; u_ = ((x1 - cx) * dy - (y1 - cy) * dx) / den
                if t_ > 0 and -1e-6 <= u_ <= 1 + 1e-6: best = t_ if best is None else min(best, t_)
            if best is not None and best > 1.0: v.co.x, v.co.y = cx + dx * best, cy + dy * best; moved += 1
    bm.to_mesh(o.data); bm.free(); return moved
# (only above the crotch, where the shorts are one piece of cloth: stretched per leg below it, each leg's inside was
#  pushed into the other - 1694 crossing triangle pairs at 0.70-0.81 m, dark patches on the seat in the game)
log('shorts spanned over hollows at', span_hollows(sh_, 0.80, 0.97, legs=False), 'points')
# the stretched part relaxed so the cloth lies flat again: moving the cleft's points onto the outline folded triangles
# over each other (dark shards on the seat in the game, 2026-11-15); then every face turned the same way out
smooth_where(sh_, lambda c: 0.78 < c.z < 0.97, 12, 0.5)
bm = bmesh.new(); bm.from_mesh(sh_.data)
bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=0.0004)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
bm.to_mesh(sh_.data); bm.free()
smooth(sh_, 3, 0.3)
log('shorts pushed out at', keep_out(sh_, BODY_BVH, 0.008), 'points')
clean_edges(sh_)
extrude_loop(sh_, highest, lambda c: c + Vector((0, 0, 0.012)))      # the waistband
subdivide(sh_, 1)
log('shorts out of the skin at', keep_out(sh_, BODY_BVH, 0.009), 'points')
# a wall between the legs: below the crotch the inside of each leg stays on its own side of the middle (moved 1.8 cm
# off the skin, where the thighs are under 3.6 cm apart each leg's inside went into the other: 1700-1800 crossing
# triangle pairs at 0.70-0.81 m, dark patches on the seat in the game). Which leg a point is on: the way it faces.
for _ in range(3):
    bm = bmesh.new(); bm.from_mesh(sh_.data); bm.normal_update()
    for v in bm.verts:
        if v.co.z < 0.88 and abs(v.co.x) < 0.05:
            if v.normal.x < -0.25: v.co.x = max(v.co.x, 0.004)
            elif v.normal.x > 0.25: v.co.x = min(v.co.x, -0.004)
    bm.to_mesh(sh_.data); bm.free()
    smooth_where(sh_, lambda c: c.z < 0.88 and abs(c.x) < 0.05, 2, 0.3)
dress(sh_, material('Fabric_fleece', (0.16, 0.17, 0.19), 0.85))
SHORTS_BVH = bvh_of(sh_)
log('tee kept over the shorts at', keep_out(tee, SHORTS_BVH, 0.009), 'points')
join_into(tee, [sh_])

# ---------------------------------------------------------------- the print: its own slot, its own UVs, over the chest
PW, PH, PZ = 0.30, 0.30, 1.30          # 30 x 30 cm, centred on the chest (a typical front print)
pm = material('Print_northern_skies', (1, 1, 1), 0.8)
tee.data.materials.append(pm); pi = len(tee.data.materials) - 1
bm = bmesh.new(); bm.from_mesh(tee.data)
uvl = bm.loops.layers.uv.active
for f in bm.faces:
    c = f.calc_center_median()
    if c.y < -0.03 and abs(c.x) < PW / 2 + 0.02 and abs(c.z - PZ) < PH / 2 + 0.02:
        f.material_index = pi
        for lp in f.loops:
            lp[uvl].uv = ((lp.vert.co.x + PW / 2) / PW, (lp.vert.co.z - (PZ - PH / 2)) / PH)   # flat front projection
bm.to_mesh(tee.data); bm.free()
log('print faces', sum(1 for p in tee.data.polygons if p.material_index == pi))

# ---------------------------------------------------------------- export, a look
log('Tee', len(tee.data.vertices), 'verts', tuple(round(d, 3) for d in tee.dimensions))
bpy.ops.object.select_all(action='DESELECT'); tee.select_set(True); bpy.context.view_layer.objects.active = tee
bpy.ops.export_scene.fbx(filepath=str(HERE / (NAME + '.fbx')), use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', add_leaf_bones=False)
import json
json.dump({m.name: list(m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value)[:3] for m in tee.data.materials}, open(HERE / (NAME + '.json'), 'w'), indent=1)
# preview: the print on in Blender too
img = bpy.data.images.load(str(HERE / 'print_northern_skies.png'))
nt = pm.node_tree; tx = nt.nodes.new('ShaderNodeTexImage'); tx.image = img; tx.extension = 'CLIP'
mixn = nt.nodes.new('ShaderNodeMixRGB'); mixn.inputs[1].default_value = (0.93, 0.93, 0.91, 1)
nt.links.new(tx.outputs['Alpha'], mixn.inputs[0]); nt.links.new(tx.outputs['Color'], mixn.inputs[2])
nt.links.new(mixn.outputs[0], nt.nodes['Principled BSDF'].inputs['Base Color'])
src.data.materials.clear(); src.data.materials.append(material('Skin', (0.55, 0.38, 0.30), 0.5))
for o in list(sc.objects):
    if o.type == 'MESH' and o not in (tee, src): bpy.data.objects.remove(o, do_unlink=True)
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50; tgt = Vector((0, -0.05, 0.98))
for nm, lv, e in (('Key', (-2, -3, 3), 900), ('Fill', (2.5, -2, 1.5), 350), ('Rim', (0, 3, 2.5), 500)):
    ld = bpy.data.lights.new(nm, 'AREA'); ld.energy = e; ld.size = 2
    lo = bpy.data.objects.new(nm, ld); lo.location = lv; lo.rotation_euler = (Vector((0, 0, 1)) - Vector(lv)).to_track_quat('-Z', 'Y').to_euler(); sc.collection.objects.link(lo)
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.4
sc.render.engine = 'BLENDER_EEVEE'; sc.render.resolution_x = 900; sc.render.resolution_y = 1000
sc.view_settings.view_transform = 'Standard'   # (the print's colours as drawn, not softened)
for nm, off in (('preview_shorts_front', Vector((-0.8, -3.0, 0.15))), ('preview_shorts_back', Vector((1.0, 2.8, 0.15)))):   # (new names: never overwrite a picture he has seen)
    cam.location = tgt + off; cam.rotation_euler = (tgt - cam.location).to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = str(HERE / (nm + '.png')); bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(HERE / (NAME + '.blend')))
log('DONE')
