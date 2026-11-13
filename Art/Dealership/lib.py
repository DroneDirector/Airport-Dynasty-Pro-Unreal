# Shared helpers for the town's public buildings: real-size modelling in metres (Z up, X east, Y north),
# procedural materials, sky and sun, camera, Cycles render and glTF export of the building only.
import bpy, bmesh, math, os
from mathutils import Vector

OUT = '/home/claude/bld/out'
os.makedirs(OUT, exist_ok=True)

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = 'METRIC'
    for n in ('Building', 'Site'):
        c = bpy.data.collections.new(n); sc.collection.children.link(c)
    return sc

def col(name):
    return bpy.data.collections[name]

_mats = {}
def mat(name, color=(0.8, 0.8, 0.8), rough=0.5, metal=0.0, bump=0.0, bump_scale=8.0, wave=None, emit=None, emit_str=0.0,
        glass=False, var=0.0, coat=0.0, alpha=None):
    if name in _mats: return _mats[name]
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1)
    b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    if coat: b.inputs['Coat Weight'].default_value = coat
    if glass:
        b.inputs['Transmission Weight'].default_value = 1.0
        b.inputs['IOR'].default_value = 1.45
    if emit:
        b.inputs['Emission Color'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = emit_str
    tc = nt.nodes.new('ShaderNodeTexCoord')
    if var:  # colour variation (stains, weathering)
        n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = 0.6; n.inputs['Detail'].default_value = 6
        nt.links.new(tc.outputs['Object'], n.inputs['Vector'])
        r = nt.nodes.new('ShaderNodeMapRange'); r.inputs['To Min'].default_value = 1 - var; r.inputs['To Max'].default_value = 1 + var * 0.4
        nt.links.new(n.outputs['Fac'], r.inputs['Value'])
        mix = nt.nodes.new('ShaderNodeVectorMath'); mix.operation = 'SCALE'
        rgb = nt.nodes.new('ShaderNodeRGB'); rgb.outputs[0].default_value = (*color, 1)
        nt.links.new(rgb.outputs[0], mix.inputs[0]); nt.links.new(r.outputs['Result'], mix.inputs['Scale'])
        nt.links.new(mix.outputs[0], b.inputs['Base Color'])
    if bump or wave:
        bp = nt.nodes.new('ShaderNodeBump'); bp.inputs['Strength'].default_value = bump or 0.6
        if wave:  # corrugated cladding: bands along the given axis
            w = nt.nodes.new('ShaderNodeTexWave'); w.wave_type = 'BANDS'; w.bands_direction = wave
            w.inputs['Scale'].default_value = 1.0
            mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / 0.076,) * 3
            nt.links.new(tc.outputs['Object'], mp.inputs['Vector']); nt.links.new(mp.outputs[0], w.inputs['Vector'])
            nt.links.new(w.outputs['Color'], bp.inputs['Height'])
        else:
            n2 = nt.nodes.new('ShaderNodeTexNoise'); n2.inputs['Scale'].default_value = bump_scale; n2.inputs['Detail'].default_value = 10
            nt.links.new(tc.outputs['Object'], n2.inputs['Vector']); nt.links.new(n2.outputs['Fac'], bp.inputs['Height'])
        nt.links.new(bp.outputs['Normal'], b.inputs['Normal'])
    _mats[name] = m
    return m

def _link(o, c):
    for cc in o.users_collection: cc.objects.unlink(o)
    col(c).objects.link(o)

def box(name, size, loc, m, bevel=0.0, rot=0.0, c='Building'):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    o = bpy.context.active_object; o.name = name; o.scale = size; o.rotation_euler[2] = rot
    bpy.ops.object.transform_apply(scale=True)
    if bevel:
        md = o.modifiers.new('bev', 'BEVEL'); md.width = bevel; md.segments = 3; md.limit_method = 'ANGLE'
    o.data.materials.append(m); _link(o, c)
    return o

def cyl(name, r, h, loc, m, verts=32, c='Building', rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=h, location=loc, rotation=rot)
    o = bpy.context.active_object; o.name = name; o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    _link(o, c); return o

def prism(name, pts, z0, z1, m, c='Building'):
    """Extrude a 2D outline (list of (x,y), counter-clockwise) from z0 to z1."""
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    vb = [bm.verts.new((x, y, z0)) for x, y in pts]; vt = [bm.verts.new((x, y, z1)) for x, y in pts]
    bm.faces.new(list(reversed(vb))); bm.faces.new(vt)
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n; bm.faces.new((vb[i], vb[j], vt[j], vt[i]))
    bm.normal_update(); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); col(c).objects.link(o); o.data.materials.append(m)
    return o

def arc_pts(cx, cy, r, a0, a1, n=48):
    return [(cx + r * math.cos(a0 + (a1 - a0) * i / n), cy + r * math.sin(a0 + (a1 - a0) * i / n)) for i in range(n + 1)]

def ring(name, cx, cy, r0, r1, a0, a1, z0, z1, m, n=64, c='Building'):
    """A curved wall/band: the area between radius r0 and r1 from angle a0 to a1, extruded z0..z1."""
    pts = arc_pts(cx, cy, r1, a0, a1, n) + list(reversed(arc_pts(cx, cy, r0, a0, a1, n)))
    return prism(name, pts, z0, z1, m, c)

def text(name, s, loc, size, m, rot=(math.pi / 2, 0, 0), depth=0.04, align='CENTER', c='Building', bold=True):
    cu = bpy.data.curves.new(name, 'FONT'); cu.body = s; cu.size = size; cu.extrude = depth; cu.align_x = align; cu.align_y = 'CENTER'
    o = bpy.data.objects.new(name, cu); o.location = loc; o.rotation_euler = rot; col(c).objects.link(o); o.data.materials.append(m)
    return o

def area_light(name, loc, size, power, color=(1, 0.95, 0.85), c='Building', rot=(0, 0, 0)):
    ld = bpy.data.lights.new(name, 'AREA'); ld.size = size; ld.energy = power; ld.color = color
    o = bpy.data.objects.new(name, ld); o.location = loc; o.rotation_euler = rot; col(c).objects.link(o); return o

def car(name, loc, rot, colr, c='Site'):
    """A simple modern hatchback/SUV block-out at real size (4.4 x 1.85 x 1.6 m)."""
    body = mat('car_' + name, colr, 0.25, 0.6, coat=1.0)
    glass = mat('carglass', (0.02, 0.03, 0.04), 0.05, 0.2, coat=1.0)
    tyre = mat('tyre', (0.02, 0.02, 0.02), 0.8)
    parts = []
    parts.append(box(name + '_b', (4.4, 1.85, 0.75), (0, 0, 0.62), body, bevel=0.18, c=c))
    parts.append(box(name + '_c', (2.6, 1.7, 0.62), (-0.25, 0, 1.25), glass, bevel=0.2, c=c))
    for sx in (-1.35, 1.35):
        for sy in (-0.85, 0.85):
            parts.append(cyl(name + '_w', 0.36, 0.26, (sx, sy, 0.36), tyre, 24, c=c, rot=(math.pi / 2, 0, 0)))
    e = bpy.data.objects.new(name, None); col(c).objects.link(e)
    for p in parts: p.parent = e
    e.location = loc; e.rotation_euler[2] = rot
    return e

def ground(size=260, asphalt_rect=None):
    g = mat('grass', (0.17, 0.2, 0.11), 0.95, bump=0.4, bump_scale=3, var=0.3)
    box('ground', (size, size, 0.2), (0, 0, -0.1), g, c='Site')

def sky(sun_elev=14, sun_rot=215, strength=1.0):
    w = bpy.data.worlds.new('W'); bpy.context.scene.world = w; w.use_nodes = True
    nt = w.node_tree; bg = nt.nodes['Background']
    st = nt.nodes.new('ShaderNodeTexSky')
    for t in ('MULTIPLE_SCATTERING', 'NISHITA', 'SINGLE_SCATTERING', 'HOSEK_WILKIE'):
        try: st.sky_type = t; break
        except Exception: pass
    try:
        st.sun_elevation = math.radians(sun_elev); st.sun_rotation = math.radians(sun_rot); st.air_density = 1.2; st.dust_density = 2.0
    except Exception: pass
    nt.links.new(st.outputs[0], bg.inputs[0]); bg.inputs[1].default_value = 0.35 * strength
    ld = bpy.data.lights.new('sun', 'SUN'); ld.energy = 3.2 * strength; ld.angle = math.radians(1.2); ld.color = (1, 0.93, 0.82)
    o = bpy.data.objects.new('sun', ld); bpy.context.scene.collection.objects.link(o)
    o.rotation_euler = (math.radians(90 - sun_elev), 0, math.radians(sun_rot + 180 - 90))

def camera(loc, target, lens=28):
    cd = bpy.data.cameras.new('cam'); cd.lens = lens
    o = bpy.data.objects.new('cam', cd); bpy.context.scene.collection.objects.link(o); o.location = loc
    d = Vector(target) - Vector(loc); o.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    bpy.context.scene.camera = o; return o

def render(path, res=(1920, 1080), samples=96):
    sc = bpy.context.scene; sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'
    sc.cycles.samples = samples; sc.cycles.use_denoising = True
    try: sc.cycles.denoiser = 'OPENIMAGEDENOISE'
    except Exception: pass
    sc.cycles.max_bounces = 6; sc.cycles.glossy_bounces = 3; sc.cycles.transmission_bounces = 6
    sc.render.resolution_x, sc.render.resolution_y = res; sc.render.resolution_percentage = 100
    try: sc.view_settings.view_transform = 'AgX'; sc.view_settings.look = 'AgX - Medium High Contrast'
    except Exception: pass
    sc.view_settings.exposure = -1.6
    sc.render.filepath = path; sc.render.image_settings.file_format = 'PNG'
    bpy.ops.render.render(write_still=True)

def export(name):
    for o in bpy.context.scene.objects: o.select_set(False)
    for o in col('Building').all_objects: o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=f'{OUT}/{name}.glb', use_selection=True, export_apply=True)
    bpy.ops.wm.save_as_mainfile(filepath=f'{OUT}/{name}.blend')

def wavy_panel(name, color=(0.24, 0.2, 0.18), groove=(0.06, 0.05, 0.045), cell=1.5, amp=0.16, rough=0.38, metal=0.35):
    """Cladding with a lattice of wavy grooves (two families of sine-warped diagonals), as on modern dealership fascias."""
    if name in _mats: return _mats[name]
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    tc = nt.nodes.new('ShaderNodeTexCoord'); sep = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(tc.outputs['Object'], sep.inputs[0])
    def M(op, a, bb=None, v=None):
        n = nt.nodes.new('ShaderNodeMath'); n.operation = op
        for i, x in enumerate((a, bb)):
            if x is None: continue
            if isinstance(x, (int, float)): n.inputs[i].default_value = x
            else: nt.links.new(x, n.inputs[i])
        return n.outputs[0]
    h = M('ADD', sep.outputs[0], sep.outputs[1]); z = sep.outputs[2]
    k = 2 * math.pi / (cell * 2.0)
    def fam(sign):
        a = M('ADD', h, M('MULTIPLY', z, sign))            # h ± z
        bb = M('SUBTRACT', h, M('MULTIPLY', z, sign))       # h ∓ z (the warp runs along the other diagonal)
        s = M('ADD', M('DIVIDE', a, cell), M('MULTIPLY', M('SINE', M('MULTIPLY', bb, k)), amp))
        f = M('SUBTRACT', M('FRACT', s), 0.5)
        return M('ABSOLUTE', f)
    d = M('MAXIMUM', fam(1.0), fam(-1.0))                    # 0.5 on a groove line
    line = M('GREATER_THAN', d, 0.485)
    mix = nt.nodes.new('ShaderNodeMix'); mix.data_type = 'RGBA'
    mix.inputs[6].default_value = (*color, 1); mix.inputs[7].default_value = (*groove, 1); nt.links.new(line, mix.inputs[0])
    nt.links.new(mix.outputs[2], b.inputs['Base Color'])
    bp = nt.nodes.new('ShaderNodeBump'); bp.inputs['Strength'].default_value = 0.5; bp.inputs['Distance'].default_value = 0.01
    nt.links.new(M('MULTIPLY', line, -1.0), bp.inputs['Height']); nt.links.new(bp.outputs['Normal'], b.inputs['Normal'])
    _mats[name] = m; return m

def car2(name, loc, rot, colr, c='Site', length=4.6, width=1.88, kind='sedan'):
    """A shaped car: side profile extruded across the width with rounded edges, tinted glasshouse, wheels with rims."""
    body = mat('paint_' + name, colr, 0.18, 0.55, coat=1.0)
    glass = mat('carglass', (0.015, 0.02, 0.025), 0.04, 0.3, coat=1.0)
    tyre = mat('tyre', (0.02, 0.02, 0.02), 0.85); rim = mat('rim', (0.35, 0.36, 0.38), 0.3, 1.0)
    lamp = mat('lamp', (0.9, 0.9, 0.95), 0.1, 0.0, emit=(1, 1, 1), emit_str=2.0)
    tail = mat('tail', (0.5, 0.02, 0.02), 0.2, 0.0, emit=(1, 0.05, 0.03), emit_str=1.0)
    L = length; hgt = 1.48 if kind == 'sedan' else 1.68
    # side profile (x along the car, z up), front at +x
    prof = [(-L/2, 0.32), (L/2, 0.32), (L/2 + 0.02, 0.62), (L/2 - 0.18, 0.80), (L/2 - 1.2, 0.98), (0.55, hgt - 0.02), (-0.75, hgt), (-L/2 + 0.55, 1.08 if kind == 'sedan' else hgt - 0.08), (-L/2, 0.95), (-L/2 - 0.02, 0.55)]
    def extrude(nm, pts, w, mm, y0=0.0):
        me = bpy.data.meshes.new(nm); bm = bmesh.new()
        a = [bm.verts.new((x, -w/2 + y0, z)) for x, z in pts]; bv = [bm.verts.new((x, w/2 + y0, z)) for x, z in pts]
        bm.faces.new(a); bm.faces.new(list(reversed(bv)))
        for i in range(len(pts)):
            j = (i + 1) % len(pts); bm.faces.new((a[j], a[i], bv[i], bv[j]))
        bm.normal_update(); bm.to_mesh(me); bm.free()
        o = bpy.data.objects.new(nm, me); col(c).objects.link(o); o.data.materials.append(mm)
        md = o.modifiers.new('bev', 'BEVEL'); md.width = 0.12; md.segments = 4; md.limit_method = 'ANGLE'
        o.modifiers.new('ss', 'SUBSURF').levels = 1
        for p in me.polygons: p.use_smooth = True
        return o
    parts = [extrude(name + '_body', prof, width, body)]
    gp = [(L/2 - 1.22, 1.0), (0.5, hgt - 0.06), (-0.72, hgt - 0.04), (-L/2 + 0.6, 1.07 if kind == 'sedan' else hgt - 0.12), (-L/2 + 0.7, 0.99), (L/2 - 1.3, 0.99)]
    parts.append(extrude(name + '_glass', gp, width + 0.02, glass))
    for sx in (L/2 - 0.85, -L/2 + 0.85):
        for sy in (-width/2 + 0.12, width/2 - 0.12):
            parts.append(cyl(name + '_t', 0.34, 0.25, (sx, sy, 0.34), tyre, 32, c=c, rot=(math.pi/2, 0, 0)))
            parts.append(cyl(name + '_r', 0.21, 0.255, (sx, sy, 0.34), rim, 10, c=c, rot=(math.pi/2, 0, 0)))
    for sy in (-width/2 + 0.32, width/2 - 0.32):
        parts.append(box(name + '_hl', (0.05, 0.42, 0.07), (L/2 + 0.0, sy, 0.7), lamp, c=c))
        parts.append(box(name + '_tl', (0.05, 0.5, 0.06), (-L/2 - 0.01, sy, 0.86), tail, c=c))
    e = bpy.data.objects.new(name, None); col(c).objects.link(e)
    for p in parts: p.parent = e
    e.location = loc; e.rotation_euler[2] = rot
    return e

def img_mat(name, col_png, h_png=None, size=2.0, rough=0.8, bump=0.3, metal=0.0, box=True):
    """A material from a seamless texture (tex/), projected from the object's coordinates at `size` metres per tile."""
    if name in _mats: return _mats[name]
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / size,) * 3
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector'])
    def tex(png, non_color=False):
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(f'/home/claude/bld/tex/{png}', check_existing=True)
        if non_color: t.image.colorspace_settings.name = 'Non-Color'
        t.extension = 'REPEAT'
        if box: t.projection = 'BOX'; t.projection_blend = 0.25
        nt.links.new(mp.outputs[0], t.inputs['Vector']); return t
    nt.links.new(tex(col_png).outputs['Color'], b.inputs['Base Color'])
    if h_png:
        bp = nt.nodes.new('ShaderNodeBump'); bp.inputs['Strength'].default_value = bump
        nt.links.new(tex(h_png, True).outputs['Color'], bp.inputs['Height']); nt.links.new(bp.outputs['Normal'], b.inputs['Normal'])
    _mats[name] = m; return m
