# Shared modelling kit for the airport express coach (Tools/make_coach.py and its parts): rounded blocks, tubes, lathed
# parts, flat cast profiles, booleans, our own generated textures and textured materials, real-size box UVs.
import bpy, bmesh, math, os
import numpy as np
from mathutils import Vector, Matrix

ART = None
ALL = []
rng = np.random.default_rng(11)
N = 512
yy, xx = np.mgrid[0:N, 0:N] / N

def blur(a, n=1):
    for _ in range(n): a = (a + np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 5
    return a
def save_img(name, rgb, nonc=False):
    h, w = rgb.shape[:2]
    im = bpy.data.images.new(name, w, h, alpha=False)
    px = np.ones((h, w, 4), np.float32); px[..., :3] = np.clip(rgb, 0, 1)
    im.pixels = px.ravel(); im.filepath_raw = os.path.join(ART, name + '.png'); im.file_format = 'PNG'; im.save()
    if nonc: im.colorspace_settings.name = 'Non-Color'
    return im
def normal_from(hgt, k):
    gy, gx = np.gradient(hgt)
    n = np.dstack((-gx * k, -gy * k, np.ones_like(hgt))); n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n * 0.5 + 0.5
def tex(name, base, grain=0.06, k=2.0, streak=False, pattern=None):
    g = blur(rng.random((N, N)), 1)
    if streak: g = 0.6 * blur(np.repeat(rng.random((N, 1)), N, 1), 1) + 0.4 * g
    h = g if pattern is None else g * 0.3 + pattern
    col = np.array(base)[None, None, :] * (1 - grain + 2 * grain * g[..., None])
    if pattern is not None: col = col * (0.75 + 0.25 * pattern[..., None])
    return save_img(name, col), save_img(name + '_N', normal_from(h, k), True)

def mat(name, t, metal, rough, emit=None, strength=0.0, alpha=None, coat=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    ti = nt.nodes.new('ShaderNodeTexImage'); ti.image = t[0]; nt.links.new(ti.outputs['Color'], b.inputs['Base Color'])
    tn = nt.nodes.new('ShaderNodeTexImage'); tn.image = t[1]
    nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.5
    nt.links.new(tn.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], b.inputs['Normal'])
    b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    b.inputs['Clearcoat'].default_value = coat
    if emit: b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    if alpha is not None:
        b.inputs['Alpha'].default_value = alpha; b.inputs['Transmission'].default_value = 0.0
        m.blend_method = 'BLEND'; m.shadow_method = 'HASHED'; m.use_backface_culling = False
    return m

def finish(o, m, smooth=60):
    o.data.materials.clear(); o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(smooth)
    ALL.append(o); return o
def link(name, bm):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(o); return o
def apply_mods(o):
    for x in bpy.context.selected_objects: x.select_set(False)
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    for md in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=md.name)
    o.select_set(False)
def ss(e0, e1, t):
    t = min(max((t - e0) / (e1 - e0), 0.0), 1.0); return t * t * (3 - 2 * t)

def rblock(name, c, s, r, m, cuts=2, segs=3, sub=0, deform=None, smooth=60):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    if cuts: bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    for v in bm.verts: v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
    o = link(name, bm)
    if r > 0:
        md = o.modifiers.new('b', 'BEVEL'); md.width = r; md.segments = segs; md.limit_method = 'ANGLE'; md.angle_limit = math.radians(30)
    if sub: md = o.modifiers.new('s', 'SUBSURF'); md.levels = sub; md.render_levels = sub
    apply_mods(o)
    if deform:
        for v in o.data.vertices:
            u, vv, w = ((v.co.x - c[0]) / (s[0] / 2), (v.co.y - c[1]) / (s[1] / 2), (v.co.z - c[2]) / (s[2] / 2))
            v.co = deform(Vector(v.co), u, vv, w)
    return finish(o, m, smooth)
def box(name, x0, x1, y0, y1, z0, z1, m=None, r=0.0, segs=2):
    c = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2); s = (abs(x1 - x0), abs(y1 - y0), abs(z1 - z0))
    o = rblock(name, c, s, r, m or bpy.data.materials[0], cuts=0, segs=segs)
    if m is None: ALL.remove(o)
    return o

def to_mesh(o, m, smooth=60):
    for x in bpy.context.selected_objects: x.select_set(False)
    bpy.context.view_layer.objects.active = o
    o.select_set(True); bpy.ops.object.convert(target='MESH'); o = bpy.context.view_layer.objects.active; o.select_set(False)
    return finish(o, m, smooth)
def tube(name, pts, r, m, res=10, bez=True, caps=True, cyclic=False):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = r; cu.bevel_resolution = 3
    cu.use_fill_caps = caps; cu.resolution_u = res
    if bez:
        sp = cu.splines.new('BEZIER'); sp.bezier_points.add(len(pts) - 1)
        for p, q in zip(sp.bezier_points, pts): p.co = q; p.handle_left_type = p.handle_right_type = 'AUTO'
    else:
        sp = cu.splines.new('POLY'); sp.points.add(len(pts) - 1)
        for p, q in zip(sp.points, pts): p.co = (*q, 1)
    sp.use_cyclic_u = cyclic
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o)
    return to_mesh(o, m)
def cyl(name, c, r, depth, axis, m, verts=24, bevel=0.0, r2=None):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r, radius2=r if r2 is None else r2, depth=depth)
    rot = {'x': Matrix.Rotation(math.pi / 2, 4, 'Y'), 'y': Matrix.Rotation(math.pi / 2, 4, 'X'), 'z': Matrix.Identity(4)}[axis]
    bmesh.ops.transform(bm, matrix=Matrix.Translation(c) @ rot, verts=bm.verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = link(name, bm)
    if bevel: md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = 2; md.limit_method = 'ANGLE'; apply_mods(o)
    return finish(o, m, 40)
def lathe(name, prof, m, segs=64, axis='y', c=(0, 0, 0), smooth=50):
    """A turned part: prof = [(r, a)] (radius, position along the axis), spun round the axis."""
    bm = bmesh.new(); rings = []
    for i in range(segs):
        t = 2 * math.pi * i / segs; ring = []
        for r, a in prof:
            p = {'y': Vector((r * math.cos(t), a, r * math.sin(t))), 'x': Vector((a, r * math.cos(t), r * math.sin(t))), 'z': Vector((r * math.cos(t), r * math.sin(t), a))}[axis]
            ring.append(bm.verts.new(p + Vector(c)))
        rings.append(ring)
    for i in range(segs):
        a, b = rings[i], rings[(i + 1) % segs]
        for j in range(len(prof) - 1): bm.faces.new((a[j], b[j], b[j + 1], a[j + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(link(name, bm), m, smooth)
def profile(name, outer, holes, thick, bevel, m, plane):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '2D'; cu.fill_mode = 'BOTH'
    cu.extrude = max(thick / 2 - bevel, 0.0001); cu.bevel_depth = bevel; cu.bevel_resolution = 3; cu.resolution_u = 10
    for loop in [outer] + holes:
        sp = cu.splines.new('BEZIER'); sp.bezier_points.add(len(loop) - 1); sp.use_cyclic_u = True
        for p, q in zip(sp.bezier_points, loop):
            p.co = (q[0], q[1], 0); p.handle_left_type = p.handle_right_type = ('VECTOR' if len(q) > 2 else 'AUTO')
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o)
    o = to_mesh(o, m, 35)
    for v in o.data.vertices: v.co = plane(v.co)
    return o
def text(name, body, c, size, m, rot, extrude=0.0006, align='CENTER'):
    cu = bpy.data.curves.new(name, 'FONT'); cu.body = body; cu.size = size; cu.extrude = extrude; cu.align_x = align; cu.align_y = 'CENTER'
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o)
    o = to_mesh(o, m, 30); o.data.transform(Matrix.Translation(c) @ rot); return o
def dup(o, name, m=None):
    n = o.copy(); n.data = o.data.copy(); n.name = name; n.modifiers.clear()
    bpy.context.collection.objects.link(n)
    if m: n.data.materials.clear(); n.data.materials.append(m)
    ALL.append(n); return n
def boolean(o, cutters, op='DIFFERENCE'):
    """o minus / intersected with the cutters (exact); the cutters are removed."""
    col = bpy.data.collections.new(o.name + '_cut')
    bpy.context.scene.collection.children.link(col)
    for c in cutters:
        for uc in c.users_collection: uc.objects.unlink(c)
        col.objects.link(c)
        if c in ALL: ALL.remove(c)
    md = o.modifiers.new('bo', 'BOOLEAN'); md.operation = op; md.solver = 'EXACT'
    if len(cutters) == 1 and op != 'DIFFERENCE': md.operand_type = 'OBJECT'; md.object = cutters[0]
    else: md.operand_type = 'COLLECTION'; md.collection = col
    md.use_self = False
    keep = o.data.materials[0] if o.data.materials else None
    apply_mods(o)
    # (the cutters' slots come along with the exact solver: the part keeps its own material only)
    o.data.materials.clear()
    if keep: o.data.materials.append(keep)
    for p in o.data.polygons: p.material_index = 0
    for c in cutters: bpy.data.objects.remove(c, do_unlink=True)
    bpy.data.collections.remove(col)
    return o
def solidify(o, t, offset=-1.0):
    md = o.modifiers.new('so', 'SOLIDIFY'); md.thickness = t; md.offset = offset; md.use_even_offset = True
    apply_mods(o); return o

def box_uv(objs, tile=0.25):
    for o in objs:
        if o.get('keep_uv'): continue
        me = o.data
        if not me.uv_layers: me.uv_layers.new(name='UVMap')
        uv = me.uv_layers.active.data
        for p in me.polygons:
            n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
            for li in p.loop_indices:
                co = me.vertices[me.loops[li].vertex_index].co
                a, b = [(co.y, co.z), (co.x, co.z), (co.x, co.y)][ax]
                uv[li].uv = (a / tile, b / tile)
def check_slots(objs):
    for o in objs:
        if not o.data.materials or any(s is None for s in o.data.materials): raise SystemExit('empty material slot on ' + o.name)
