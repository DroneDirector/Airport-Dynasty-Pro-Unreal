# The operations room's monitor in three kinds, chosen in the game by the player's own screen (owner, 2026-11-11): a
# 27" 16:9 widescreen, a 34" 21:9 ultrawide (1900R curve) and a 49" 32:9 super-ultrawide (1000R curve). High detail
# (Nanite in the game): a separate screen surface (its own material, MonitorScreen<kind>, UVs over the whole picture), a
# gloss-black bezel with a chin, a matte back housing with vents, the ports panel, a VESA plate, an aluminium stand
# with a cable hole and a base. Each stands with its base on the desk at z = 0, the screen's middle over x = 0,
# facing -y. Blender 3.3:
#   blender -b --python Art/Monitor/make_monitors.py
# writes Art/Monitor/Monitor.blend (all three) and Art/Monitor/monitor169.glb, monitor219.glb, monitor329.glb.
import bpy, bmesh, math, os
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene


def mat(name, color, metal=0.0, rough=0.5, emit=None, strength=0.0):
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1)
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    if emit:
        b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    return m


def screen_mat(key):
    """The picture's material (one per monitor, MonitorScreen<key>): a picture for its colour and its light, dark until
    the game puts its own there (the operations screen, drawn for this monitor's proportions)."""
    m = bpy.data.materials.new('MonitorScreen' + key); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Roughness'].default_value = 0.06
    img = bpy.data.images.new('MonitorScreenOff' + key, 16, 16); img.pixels = [0.012, 0.014, 0.018, 1.0] * 256; img.pack()
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = img
    nt.links.new(t.outputs['Color'], b.inputs['Base Color']); nt.links.new(t.outputs['Color'], b.inputs['Emission'])
    b.inputs['Emission Strength'].default_value = 1.0
    return m


M = {
    'bezel': mat('MonitorBezel', (0.012, 0.012, 0.013), 0.0, 0.22),
    'housing': mat('MonitorHousing', (0.045, 0.046, 0.05), 0.0, 0.55),
    'alu': mat('MonitorAluminium', (0.62, 0.63, 0.65), 1.0, 0.3),
    'dark': mat('MonitorPorts', (0.008, 0.008, 0.009), 0.0, 0.7),
    'led': mat('MonitorLed', (0.6, 0.85, 1.0), 0.0, 0.3, (0.6, 0.85, 1.0), 2.0),
    'rubber': mat('MonitorRubber', (0.02, 0.02, 0.02), 0.0, 0.9),
}


def link(ob, coll):
    coll.objects.link(ob)
    return ob


def box(name, coll, size, at, material, xcuts=0, bevel=0.0, seg=3):
    """A box (w, d, h) centred at `at`, cut `xcuts` times across its width (so it can bend), its edges rounded."""
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    if xcuts:
        edges = [e for e in bm.edges if abs((e.verts[0].co - e.verts[1].co).normalized().x) > 0.99]
        bmesh.ops.subdivide_edges(bm, edges=edges, cuts=xcuts, use_grid_fill=True)
    bm.to_mesh(me); bm.free()
    ob = link(bpy.data.objects.new(name, me), coll); ob.location = at
    ob.data.materials.append(material)
    if bevel:
        bv = ob.modifiers.new('bevel', 'BEVEL'); bv.width = bevel; bv.segments = seg; bv.limit_method = 'ANGLE'
        ob.data.use_auto_smooth = True; ob.data.auto_smooth_angle = math.radians(40)
    return ob


def cyl(name, coll, r, depth, at, material, rot=(0, 0, 0), verts=48, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=at, rotation=rot)
    ob = bpy.context.active_object; ob.name = name
    for c in ob.users_collection: c.objects.unlink(ob)
    coll.objects.link(ob); ob.data.materials.append(material)
    if bevel:
        bv = ob.modifiers.new('bevel', 'BEVEL'); bv.width = bevel; bv.segments = 3; bv.limit_method = 'ANGLE'
    bpy.ops.object.shade_smooth(); ob.data.use_auto_smooth = True; ob.data.auto_smooth_angle = math.radians(35)
    return ob


def screen_plane(name, coll, w, h, at, material, xcuts):
    """The picture: a thin plane facing -y, its UVs over the whole picture (u across, v up)."""
    me = bpy.data.meshes.new(name); bm = bmesh.new(); uv = bm.loops.layers.uv.new('UVMap')
    n = xcuts + 1; rows = []
    for i in range(n + 1):
        x = -w / 2 + w * i / n
        rows.append((bm.verts.new((x, 0, -h / 2)), bm.verts.new((x, 0, h / 2))))
    for i in range(n):
        (a0, a1), (b0, b1) = rows[i], rows[i + 1]
        f = bm.faces.new((a0, b0, b1, a1))   # winding: normal towards -y
        for lp in f.loops:
            lp[uv].uv = ((lp.vert.co.x + w / 2) / w, (lp.vert.co.z + h / 2) / h)
    bm.normal_update()
    bm.to_mesh(me); bm.free()
    ob = link(bpy.data.objects.new(name, me), coll); ob.location = at; ob.data.materials.append(material)
    return ob


def monitor(key, diag_in, aspect, radius, centre_h, base_v, offset_x):
    coll = bpy.data.collections.new('Monitor' + key); sc.collection.children.link(coll)
    d = diag_in * 0.0254; H = d / math.sqrt(1 + aspect * aspect); W = H * aspect   # the picture, metres
    side, top, chin = 0.0065, 0.0065, 0.019
    BW, BH = W + 2 * side, H + top + chin
    cuts = 0 if radius is None else 120
    zc = centre_h; zb = zc - H / 2 - chin + BH / 2      # the bezel's middle (the chin below the picture)
    parts = []
    # the bezel: a thin slab, rounded, its front at y = -0.005
    parts.append(box(key + '-Bezel', coll, (BW, 0.010, BH), (0, 0, zb), M['bezel'], cuts, 0.0018))
    # the picture, a fraction of a millimetre proud of the bezel
    parts.append(screen_plane(key + '-Screen', coll, W, H, (0, -0.0053, zc), screen_mat(key), cuts))
    # the back housing: a rounded shell behind the panel, its vents near the top, the ports panel low down
    hw, hh, hd = BW * 0.82, BH * 0.74, 0.034
    hz = zb - BH * 0.04
    parts.append(box(key + '-Housing', coll, (hw, hd, hh), (0, 0.005 + hd / 2, hz), M['housing'], cuts, 0.011, 5))
    for i in range(int(hw / 0.012)):
        x = -hw / 2 + 0.03 + i * 0.012
        if x > hw / 2 - 0.03: break
        parts.append(box(key + '-Vent%02d' % i, coll, (0.0045, 0.002, 0.045), (x, 0.005 + hd + 0.0005, hz + hh / 2 - 0.05), M['dark'], 0, 0.0008, 2))
    # (on the flat one every vent is the same: one mesh, the rest copies of it, so the game draws them as instances)
    if radius is None:
        vents = [o for o in parts if '-Vent' in o.name]
        me = bpy.data.meshes.new_from_object(vents[0].evaluated_get(bpy.context.evaluated_depsgraph_get()))   # its rounding made
        for o in vents:
            old = o.data; o.modifiers.clear(); o.data = me
            if old.users == 0: bpy.data.meshes.remove(old)
    pz = hz - hh / 2 + 0.045
    parts.append(box(key + '-PortsPanel', coll, (0.20, 0.004, 0.035), (hw * 0.22, 0.005 + hd + 0.001, pz), M['dark'], 0, 0.002))
    for j, (pw, ph) in enumerate([(0.016, 0.007), (0.016, 0.007), (0.018, 0.006), (0.010, 0.004), (0.010, 0.004), (0.022, 0.012)]):
        parts.append(box(key + '-Port%d' % j, coll, (pw, 0.003, ph), (hw * 0.22 - 0.08 + j * 0.03, 0.005 + hd + 0.003, pz), M['rubber'], 0, 0.0008, 2))
    # the power light and the joystick under the chin, right
    parts.append(cyl(key + '-Led', coll, 0.0012, 0.002, (BW / 2 - 0.05, -0.0052, zb - BH / 2 + chin / 2), M['led'], (math.pi / 2, 0, 0), 16))
    parts.append(cyl(key + '-Joystick', coll, 0.004, 0.006, (BW / 2 - 0.08, 0.004, zb - BH / 2 - 0.002), M['dark'], (0, 0, 0), 24))
    # the curve: every part of the panel (its roundings made first) bent onto circles round one centre in front of the
    # screen, so it all stays together and curves towards you (1900R or 1000R); x along the arc, depth kept
    if radius is not None:
        dg = bpy.context.evaluated_depsgraph_get()
        for ob in parts:
            me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg)); ob.modifiers.clear(); ob.data = me
            mw = ob.matrix_world.copy(); inv = mw.inverted()
            for v in me.vertices:
                p = mw @ v.co; th = p.x / radius; r = radius + p.y
                v.co = inv @ Vector((r * math.sin(th), -radius + r * math.cos(th), p.z))
            me.update()
    # the VESA plate and the stand: a column behind the panel's middle, a hinge, a cable hole, the base
    back = 0.005 + hd
    parts.append(box(key + '-Vesa', coll, (0.11, 0.008, 0.11), (0, back + 0.004 + (0 if radius is None else 0.002), hz), M['alu'], 0, 0.003))
    hinge_z = hz
    cyl(key + '-Hinge', coll, 0.014, 0.08, (0, back + 0.022, hinge_z), M['alu'], (0, math.pi / 2, 0), 48, 0.002)
    col_h = hinge_z + 0.01
    box(key + '-Column', coll, (0.065, 0.028, col_h), (0, back + 0.045, col_h / 2 + 0.01), M['alu'], 0, 0.006, 4)
    cyl(key + '-CableHole', coll, 0.016, 0.03, (0, back + 0.045, 0.12), M['dark'], (math.pi / 2, 0, 0), 40)
    if not base_v:
        box(key + '-Base', coll, (0.26, 0.19, 0.012), (0, back + 0.02, 0.006), M['alu'], 0, 0.005, 4)
        box(key + '-BaseFoot', coll, (0.24, 0.17, 0.002), (0, back + 0.02, 0.001), M['rubber'], 0, 0.001)
    else:
        for s in (-1, 1):
            leg = box(key + '-BaseLeg%s' % ('L' if s < 0 else 'R'), coll, (0.05, 0.30, 0.014), (0, 0, 0.007), M['alu'], 0, 0.006, 4)
            leg.rotation_euler = (0, 0, s * math.radians(32)); leg.location = (s * 0.075, back + 0.045 - 0.12, 0.007)
        box(key + '-BaseHub', coll, (0.09, 0.07, 0.016), (0, back + 0.045, 0.008), M['alu'], 0, 0.006, 4)
    for ob in coll.objects:
        ob.location.x += offset_x
    return coll


monitor('169', 27, 16 / 9, None, 0.42, False, -1.6)
monitor('219', 34, 21 / 9, 1.9, 0.40, True, 0.0)
monitor('329', 49, 32 / 9, 1.0, 0.36, True, 2.2)

# every mesh has its material (no empty slots)
for ob in bpy.data.objects:
    if ob.type == 'MESH':
        assert len(ob.data.materials) and all(m is not None for m in ob.data.materials), ob.name

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'Monitor.blend'))

# each one on its own, back at x = 0, as glTF for the game (the curve and roundings applied)
for key, off in (('169', -1.6), ('219', 0.0), ('329', 2.2)):
    coll = bpy.data.collections['Monitor' + key]
    bpy.ops.object.select_all(action='DESELECT')
    for ob in coll.objects:
        ob.location.x -= off
        if ob.type == 'MESH':
            ob.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'monitor%s.glb' % key), use_selection=True, export_apply=True, export_format='GLB')
    for ob in coll.objects:
        ob.location.x += off
# the meshes' own names out (Unreal then names them <file>_mesh_<n>, placed by number as the Babylon models are)
import runpy, sys as _sys
_tool = os.path.join(HERE, '..', '..', 'Tools', 'glb_strip_mesh_names.py')
_sys.argv = [_tool] + [os.path.join(HERE, 'monitor%s.glb' % k) for k in ('169', '219', '329')]
runpy.run_path(_tool, run_name='__main__')
print('MONITORS DONE')
