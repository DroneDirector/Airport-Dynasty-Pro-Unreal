# The shared vehicle-building kit (owner 2026-11-21: "use what you learned on the tug as the standing rule for every
# vehicle"; the helpers and the automatic checks of Tools/make_pushback_tug.py, moved into one module when the 2nd
# vehicle - the ground power unit, Tools/make_gpu.py - started). Runs inside Blender (bpy).
# Axes: x forward, y left, z up; metres. Every part keeps its mesh in world coordinates (origin 0,0,0) like the tug.
import bpy, bmesh, math, os, json, subprocess
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

ALL = []          # every mesh part
IKS = []          # every IK / pivot empty

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    ALL.clear(); IKS.clear()

# ---------------------------------------------------------------- materials (plain first; photo() swaps in the CC0 photo set)
def pmat(name, col, metal=0.0, rough=0.5, emit=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if emit: b.inputs['Emission'].default_value = (*col, 1); b.inputs['Emission Strength'].default_value = emit
    return m

# ---------------------------------------------------------------- geometry helpers
def finish(o, m, smooth=40):
    o.data.materials.clear(); o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(smooth)
    ALL.append(o); return o
def link(name, bm):
    name = name.replace('.', 'p')        # (a '.' in a part's name: Unreal reads '.38' as a copy number and loses the part)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(o); return o
def apply_mods(o):
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    for md in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=md.name)
    o.select_set(False)
def ss(e0, e1, t):
    t = min(max((t - e0) / (e1 - e0), 0.0), 1.0); return t * t * (3 - 2 * t)
def box(name, c, s, m, bevel=0.0, segs=2, cuts=1, sub=0, deform=None, smooth=40):
    """A block (centre c, size s); bevel = edge chamfer/round (segs 1 = a flat chamfer, as cut steel)."""
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    if cuts > 1: bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts - 1, use_grid_fill=True)
    for v in bm.verts: v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
    o = link(name, bm)
    if bevel > 0:
        md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = segs; md.limit_method = 'ANGLE'; md.angle_limit = math.radians(30)
    if sub: md = o.modifiers.new('s', 'SUBSURF'); md.levels = sub; md.render_levels = sub
    apply_mods(o)
    if deform:
        for v in o.data.vertices:
            u, vv, w = ((v.co.x - c[0]) / (s[0] / 2), (v.co.y - c[1]) / (s[1] / 2), (v.co.z - c[2]) / (s[2] / 2))
            v.co = deform(Vector(v.co), u, vv, w)
    return finish(o, m, smooth)
def turn(o, centre, axis, deg):
    """Turns a part's mesh round an axis ('x','y','z') through centre (for parts set at an angle: louvres, grille slats)."""
    c = Vector(centre); o.data.transform(Matrix.Translation(c) @ Matrix.Rotation(math.radians(deg), 4, axis.upper()) @ Matrix.Translation(-c)); o.data.update(); return o
def cyl(name, c, r, depth, axis, m, verts=32, bevel=0.0, r2=None):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r, radius2=r if r2 is None else r2, depth=depth)
    rot = {'x': Matrix.Rotation(math.pi / 2, 4, 'Y'), 'y': Matrix.Rotation(math.pi / 2, 4, 'X'), 'z': Matrix.Identity(4)}[axis]
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(c)) @ rot, verts=bm.verts)
    o = link(name, bm)
    if bevel: md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = 2; md.limit_method = 'ANGLE'; apply_mods(o)
    return finish(o, m, 35)
def lathe(name, prof, c, m, segs=72, smooth=50):
    """A part turned round the y axis through c (a tyre, a rim): prof = a closed outline of (radius, y-offset) points."""
    bm = bmesh.new(); rings = []
    for k in range(segs):
        a = 2 * math.pi * k / segs
        rings.append([bm.verts.new((c[0] + r * math.cos(a), c[1] + y, c[2] + r * math.sin(a))) for r, y in prof])
    n = len(prof)
    for k in range(segs):
        A, B = rings[k], rings[(k + 1) % segs]
        for i in range(n): bm.faces.new((A[i], A[(i + 1) % n], B[(i + 1) % n], B[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(link(name, bm), m, smooth)
def torus(name, c, R, r, axis, m, segs=48, rsegs=16):
    """A ring (a towing eye, a grommet): R = the ring's centre-line radius, r = its bar radius."""
    bm = bmesh.new(); rings = []
    rot = {'x': Matrix.Rotation(math.pi / 2, 3, 'Y'), 'y': Matrix.Rotation(math.pi / 2, 3, 'X'), 'z': Matrix.Identity(3)}[axis]
    for k in range(segs):
        a = 2 * math.pi * k / segs; ring = []
        for j in range(rsegs):
            b = 2 * math.pi * j / rsegs; rr = R + r * math.cos(b)
            ring.append(bm.verts.new(Vector(c) + rot @ Vector((rr * math.cos(a), rr * math.sin(a), r * math.sin(b)))))
        rings.append(ring)
    for k in range(segs):
        A, B = rings[k], rings[(k + 1) % segs]
        for j in range(rsegs): bm.faces.new((A[j], A[(j + 1) % rsegs], B[(j + 1) % rsegs], B[j]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(link(name, bm), m, 60)
def tube(name, pts, r, m, res=8, bez=True, bres=3):
    name = name.replace('.', 'p')
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = r; cu.bevel_resolution = bres; cu.use_fill_caps = True; cu.resolution_u = res
    if bez:
        sp = cu.splines.new('BEZIER'); sp.bezier_points.add(len(pts) - 1)
        for p, q in zip(sp.bezier_points, pts): p.co = q; p.handle_left_type = p.handle_right_type = 'AUTO'
    else:
        sp = cu.splines.new('POLY'); sp.points.add(len(pts) - 1)
        for p, q in zip(sp.points, pts): p.co = (*q, 1)
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o)
    bpy.context.view_layer.objects.active = o
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.ops.object.convert(target='MESH'); o = bpy.context.view_layer.objects.active; o.select_set(False)
    return finish(o, m, 50)
def prism(name, pts_xz, y0, y1, m, bevel=0.0, segs=1):
    """A side outline (x, z) pushed across y0..y1 - the way plate steel is cut and welded (or a moulded canopy)."""
    bm = bmesh.new()
    a = [bm.verts.new((x, y0, z)) for x, z in pts_xz]; b = [bm.verts.new((x, y1, z)) for x, z in pts_xz]
    bm.faces.new(a[::-1]); bm.faces.new(b)
    n = len(pts_xz)
    for i in range(n): bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = link(name, bm)
    if bevel: md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = segs; md.limit_method = 'ANGLE'; md.angle_limit = math.radians(30); apply_mods(o)
    return finish(o, m, 30)
def cut(target, cutter):
    md = target.modifiers.new('cut', 'BOOLEAN'); md.operation = 'DIFFERENCE'; md.object = cutter; md.solver = 'EXACT'
    apply_mods(target); ALL.remove(cutter); bpy.data.objects.remove(cutter, do_unlink=True)
def ik(name, loc, rot=(0, 0, 0), prefix='IK_'):
    e = bpy.data.objects.new(prefix + name, None); e.empty_display_type = 'ARROWS'; e.empty_display_size = 0.08
    e.location = loc; e.rotation_euler = rot; bpy.context.collection.objects.link(e); IKS.append(e); return e
def lamp(name, face_pt, normal, w, h, m, m_housing, m_rim, depth=0.06):
    """Every lamp is a housing SET INTO the face (half its depth inside), a rim, the lens recessed in it.
    face_pt: a point on the face; normal: 'x+', 'x-', 'y+', 'y-'; w across, h up (m)."""
    ax, sg = normal[0], (1 if normal[1] == '+' else -1)
    c = Vector(face_pt)
    if ax == 'x':
        box(name + '_Housing', (c.x, c.y, c.z), (depth, w + 0.03, h + 0.03), m_housing, bevel=0.008)
        rim = box(name + '_Rim', (c.x + sg * depth / 2, c.y, c.z), (0.008, w + 0.03, h + 0.03), m_rim, bevel=0.004)
        cut(rim, box(name + '_Recess', (c.x + sg * depth / 2, c.y, c.z), (0.03, w, h), m_housing))
        box(name + '_Lens', (c.x + sg * depth / 2, c.y, c.z), (0.004, w - 0.002, h - 0.002), m)   # (2 mm into the housing, 2 mm inside the rim, 1 mm clear of the recess walls: no flicker)
    else:
        box(name + '_Housing', (c.x, c.y, c.z), (w + 0.03, depth, h + 0.03), m_housing, bevel=0.008)
        rim = box(name + '_Rim', (c.x, c.y + sg * depth / 2, c.z), (w + 0.03, 0.008, h + 0.03), m_rim, bevel=0.004)
        cut(rim, box(name + '_Recess', (c.x, c.y + sg * depth / 2, c.z), (w, 0.03, h), m_housing))
        box(name + '_Lens', (c.x, c.y + sg * depth / 2, c.z), (w - 0.002, 0.004, h - 0.002), m)

# ---------------------------------------------------------------- the automatic checks (owner 2026-11-21)
def bbox(o):
    cs = [o.matrix_world @ Vector(c) for c in o.bound_box]
    return Vector((min(c.x for c in cs), min(c.y for c in cs), min(c.z for c in cs))), Vector((max(c.x for c in cs), max(c.y for c in cs), max(c.z for c in cs)))
def touch(a, b, tol=0.002): return all(a[0][i] - tol <= b[1][i] and b[0][i] - tol <= a[1][i] for i in range(3))
def diag(b): return (b[1] - b[0]).length
def bvh(o):
    me = o.data; mw = o.matrix_world
    return BVHTree.FromPolygons([mw @ v.co for v in me.vertices], [tuple(p.vertices) for p in me.polygons])
def checks(tag, ground_ok=('Tyre',), overhang_skip=(), overhang_skip_prefix=()):
    """Box float, lamp-lens clashes, overhang past a face's edge (15 mm), and the real MESH contact test (BVH overlap or
    1 mm): every part, however small, must cut into or touch another part. Returns the three lists."""
    BB = {o.name: bbox(o) for o in ALL}
    floating = [n for n, b in BB.items() if not n.startswith(ground_ok) and not any(touch(b, b2) for m, b2 in BB.items() if m != n)]
    clash = []
    for n, b in BB.items():
        if n.endswith('_Lens'):
            own = n[:-5]
            for m, b2 in BB.items():
                if m.startswith(own) or m == n: continue
                if touch(b, b2, -0.001) and (b2[1] - b2[0]).length < 0.2: clash.append((n, m))
    trees = {o.name: bvh(o) for o in ALL}
    def mesh_touch(a, b):
        if trees[a].overlap(trees[b]): return True
        oa = bpy.data.objects[a]; mw = oa.matrix_world; vs = list(oa.data.vertices)
        for v in vs[:: max(1, len(vs) // 400)]:
            hit = trees[b].find_nearest(mw @ v.co)
            if hit[0] is not None and hit[3] < 0.001: return True
        return False
    overhang = []
    for n, b in BB.items():
        if n in overhang_skip or n.startswith(overhang_skip_prefix): continue
        if diag(b) > 0.35 or n.endswith(('_Lens', '_Rim')): continue
        hosts = [(m, b2) for m, b2 in BB.items() if m != n and diag(b2) > 0.4 and touch(b, b2) and mesh_touch(n, m)]   # (a host is a part it really touches, not a box neighbour)
        for m, b2 in hosts:
            pen = [min(b[1][i], b2[1][i]) - max(b[0][i], b2[0][i]) for i in range(3)]
            nrm = min(range(3), key=lambda i: pen[i])
            if all(b[0][i] >= b2[0][i] + 0.015 and b[1][i] <= b2[1][i] - 0.015 for i in range(3) if i != nrm): break
        else:
            if hosts: overhang.append(n)
    loose = []
    for n, b in BB.items():
        if n.startswith(ground_ok): continue
        near = [m for m, b2 in BB.items() if m != n and touch(b, b2, 0.003)]
        if not any(mesh_touch(n, m) for m in near): loose.append(n)
    print('%s CHECK: %d floating %s | %d lamp clashes %s | %d overhang %s | %d mesh-loose %s' % (
        tag, len(floating), floating[:12], len(clash), clash[:12], len(overhang), overhang[:12], len(loose), loose[:12]))
    return floating, clash, overhang, loose

# ---------------------------------------------------------------- CC0 photo materials (ambientCG, 2K JPG, 1 UV unit = 1 m)
def uv_cube():
    for o in ALL:
        bpy.context.view_layer.objects.active = o
        for x in bpy.context.selected_objects: x.select_set(False)
        o.select_set(True); bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.cube_project(cube_size=1.0, correct_aspect=True, scale_to_bounds=False); bpy.ops.object.mode_set(mode='OBJECT'); o.select_set(False)
def photo(m, tex_dir, mid, tile=1.0, tint=None, metal=None, rough_mul=1.0, normal=0.8):
    m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial'); b = nt.nodes.new('ShaderNodeBsdfPrincipled'); nt.links.new(b.outputs[0], out.inputs[0])
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / tile, 1 / tile, 1)
    nt.links.new(tc.outputs['UV'], mp.inputs['Vector'])
    def img(suffix, noncolor):
        f = os.path.join(tex_dir, mid, '%s_2K-JPG_%s.jpg' % (mid, suffix))
        if not os.path.exists(f): return None
        n = nt.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(f, check_existing=True)
        if noncolor: n.image.colorspace_settings.name = 'Non-Color'
        nt.links.new(mp.outputs[0], n.inputs[0]); return n
    col = img('Color', False); ao = img('AmbientOcclusion', True)
    assert col is not None, 'missing texture ' + mid
    c_out = col.outputs[0]
    if ao:
        mx = nt.nodes.new('ShaderNodeMixRGB'); mx.blend_type = 'MULTIPLY'; mx.inputs[0].default_value = 0.7
        nt.links.new(c_out, mx.inputs[1]); nt.links.new(ao.outputs[0], mx.inputs[2]); c_out = mx.outputs[0]
    if tint:
        mt = nt.nodes.new('ShaderNodeMixRGB'); mt.blend_type = 'MULTIPLY'; mt.inputs[0].default_value = 1.0
        mt.inputs[2].default_value = (*tint, 1); nt.links.new(c_out, mt.inputs[1]); c_out = mt.outputs[0]
    nt.links.new(c_out, b.inputs['Base Color'])
    r = img('Roughness', True)
    if r:
        if rough_mul != 1.0:
            mm = nt.nodes.new('ShaderNodeMath'); mm.operation = 'MULTIPLY'; mm.inputs[1].default_value = rough_mul
            nt.links.new(r.outputs[0], mm.inputs[0]); nt.links.new(mm.outputs[0], b.inputs['Roughness'])
        else: nt.links.new(r.outputs[0], b.inputs['Roughness'])
    mtl = img('Metalness', True)
    if metal is not None: b.inputs['Metallic'].default_value = metal
    elif mtl: nt.links.new(mtl.outputs[0], b.inputs['Metallic'])
    nrm = img('NormalGL', True)
    if nrm:
        nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = normal
        nt.links.new(nrm.outputs[0], nm.inputs['Color']); nt.links.new(nm.outputs[0], b.inputs['Normal'])
    return m

# ---------------------------------------------------------------- save, export, flicker fix-up to 0 pairs
def save_export(root, blend, glb, zf, tag, collection):
    col = bpy.data.collections.new(collection); bpy.context.scene.collection.children.link(col)
    for o in ALL + IKS:
        for c in list(o.users_collection): c.objects.unlink(o)
        col.objects.link(o)
    for o in ALL: assert len(o.data.materials) and all(o.data.materials), 'empty material slot on ' + o.name   # (material slot rule)
    def export():
        for o in bpy.context.scene.objects: o.select_set(o in ALL or o in IKS)
        bpy.ops.export_scene.gltf(filepath=glb, export_format='GLB', use_selection=True, export_apply=True)
    OB = {o.name: o for o in ALL}; last = '?'
    for it in range(8):
        export()
        r = subprocess.run([r'C:/Python311/python.exe', '-I', os.path.join(root, 'Tools', 'find_zfight.py'), glb, zf], capture_output=True, text=True)
        last = (r.stdout.strip().splitlines() or ['?'])[-1]; print('%s FLICKER pass %d: %s' % (tag, it, last))
        if last.strip().startswith('0 pairs fight'): break                 # (not '50 pairs fight': read the number)
        try: pairs = json.load(open(zf))
        except Exception: break
        items = pairs if isinstance(pairs, list) else pairs.get('pairs', [])
        if it == 0: print('%s FLICKER first pass pairs: %s' % (tag, [(p.get('a'), p.get('b')) for p in items if isinstance(p, dict)]))
        done = set()
        for pr in items:
            a, b = (pr[0], pr[1]) if isinstance(pr, (list, tuple)) else (pr.get('a'), pr.get('b'))
            if a not in OB or b not in OB: continue
            ba, bb = bbox(OB[a]), bbox(OB[b])
            small, big = (a, b) if (ba[1] - ba[0]).length <= (bb[1] - bb[0]).length else (b, a)
            if small in done: continue
            done.add(small); o = OB[small]; bs, bg = bbox(o), bbox(OB[big])
            c = (bs[0] + bs[1]) / 2; sz = bs[1] - bs[0]
            k = Vector(tuple(max(0.2, (sz[i] - 0.003) / sz[i]) if sz[i] > 0.006 else 1.0 for i in range(3)))
            pen = [min(bs[1][i], bg[1][i]) - max(bs[0][i], bg[0][i]) for i in range(3)]
            nrm = min(range(3), key=lambda i: pen[i]); push = Vector((0, 0, 0))
            push[nrm] = 0.003 if (bg[0][nrm] + bg[1][nrm]) / 2 > c[nrm] else -0.003
            o.data.transform(Matrix.Translation(c + push) @ Matrix.Diagonal((*k, 1)) @ Matrix.Translation(-c))
            o.data.update()
    bpy.ops.wm.save_as_mainfile(filepath=blend)
    return last

# ---------------------------------------------------------------- the wireframe views (Freestyle on the real mesh)
def wire_setup():
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.taa_render_samples = 16
    w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.035, 0.08, 0.16, 1); sc.world = w
    sc.view_settings.view_transform = 'Standard'; sc.view_settings.look = 'None'
    sc.render.use_freestyle = True; sc.render.line_thickness_mode = 'ABSOLUTE'; sc.render.line_thickness = 0.8
    fs = bpy.context.view_layer.freestyle_settings; fs.crease_angle = math.radians(179.5)
    ls = fs.linesets[0] if len(fs.linesets) else fs.linesets.new('All')
    ls.select_by_visibility = True; ls.visibility = 'VISIBLE'; ls.select_by_edge_types = True
    ls.select_crease = True; ls.select_silhouette = True; ls.select_border = True
    if ls.linestyle is None: ls.linestyle = bpy.data.linestyles.new('Wire')
    ls.linestyle.color = (0.82, 0.9, 1.0); ls.linestyle.thickness = 0.8
    flat = bpy.data.materials.new('WireBlue'); flat.use_nodes = True
    flat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.10, 0.25, 0.55, 1)
    flat.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.8
    bpy.context.view_layer.material_override = flat
    for nm, loc, e in (('Key', (4, 5, 7), 4.0), ('Fill', (-5, -4, 4), 1.5)):
        ld = bpy.data.lights.new(nm, 'SUN'); ld.energy = e; lo = bpy.data.objects.new(nm, ld); sc.collection.objects.link(lo)
        lo.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    cam_d = bpy.data.cameras.new('Cam'); cam = bpy.data.objects.new('Cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
    def shoot(path, loc, at, lens=50, ortho=0, res=(2400, 1500), hide=()):
        for o in ALL: o.hide_render = o.name in hide
        cam.location = loc; cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        cam_d.type = 'ORTHO' if ortho else 'PERSP'; cam_d.lens = lens; cam_d.ortho_scale = ortho or 1
        sc.render.resolution_x, sc.render.resolution_y = res
        sc.render.filepath = path; bpy.ops.render.render(write_still=True); print('view', os.path.basename(path))
        for o in ALL: o.hide_render = False
    return shoot
