# The TAX FREE's neon sign (owner, 2026-11-17: "a glowing tax free sign that has a neon glow to it - only text, 3D text on
# the wall which says TAX FREE, then a tax free logo in the same kind of style"): 3D letters, rounded, glowing teal
# (the aurora's MI_T_l_40FFD0), with the logo beside them in the same neon tube - a shopping bag with its handle and a
# small aircraft on it. Made in the plan's metres round its own origin, facing the walkway (plan +z), its back at z 0;
# Tools/terminal_16gates.py hangs it on the wall over the shop's way in and its way out.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_taxfree_sign.py
import bpy, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
mat = bpy.data.materials.new('MI_T_neonPink')          # (the aurora's teal, glowing as neon: Tools/make_neon_materials_tf.py)
# ---- the letters: Blender text, bold, extruded 4 cm with a round edge ----
bpy.ops.object.text_add(location=(0, 0, 0))
t = bpy.context.object; t.data.body = 'TAX FREE'
for f in ('C:/Windows/Fonts/arialbd.ttf', 'C:/Windows/Fonts/segoeuib.ttf'):
    if os.path.exists(f): t.data.font = bpy.data.fonts.load(f); break
t.data.size = 0.82; t.data.extrude = 0.025; t.data.bevel_depth = 0.012; t.data.bevel_resolution = 2
t.data.align_x = 'LEFT'; t.data.align_y = 'CENTER'
bpy.ops.object.convert(target='MESH')
t.location.x = -1.15                                                   # (the logo left of it, from the walkway)
# ---- the logo: a neon tube - the bag's outline and handle, the small aircraft on it ----
def tube(pts, closed, r=0.022, name='logo'):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = r; cu.bevel_resolution = 3
    sp = cu.splines.new('POLY'); sp.points.add(len(pts) - 1)
    for p, (x, y) in zip(sp.points, pts): p.co = (x, y, 0.0, 1.0)
    sp.use_cyclic_u = closed
    o = bpy.data.objects.new(name, cu); bpy.context.scene.collection.objects.link(o); return o
bx0, bx1, by0, by1 = -2.15, -1.35, -0.45, 0.3                        # the bag (Blender x, y before standing it up)
rr = 0.08
bag = []
for cx, cy, a0 in ((bx1 - rr, by0 + rr, -90), (bx1 - rr, by1 - rr, 0), (bx0 + rr, by1 - rr, 90), (bx0 + rr, by0 + rr, 180)):
    for k in range(7): a = math.radians(a0 + 90 * k / 6); bag.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
objs = [tube(bag, True)]
hx, hy = (bx0 + bx1) / 2, by1
objs.append(tube([(hx + 0.2 * math.cos(math.radians(a)), hy + 0.24 * math.sin(math.radians(a))) for a in range(0, 181, 12)], False))
# the aircraft: fuselage, wings, tail, seen from above, nose up, on the bag
ax, ay = hx, -0.08
objs.append(tube([(ax, ay - 0.28), (ax, ay + 0.26)], False, 0.018))
objs.append(tube([(ax - 0.24, ay - 0.02), (ax, ay + 0.06), (ax + 0.24, ay - 0.02)], False, 0.018))
objs.append(tube([(ax - 0.1, ay - 0.27), (ax, ay - 0.22), (ax + 0.1, ay - 0.27)], False, 0.016))
for o in objs:
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.convert(target='MESH')
# ---- one mesh, the neon material, stood up on the wall facing the walkway ----
bpy.ops.object.select_all(action='DESELECT')
for o in bpy.context.scene.objects: o.select_set(True)
bpy.context.view_layer.objects.active = t
bpy.ops.object.join()
sign = bpy.context.object; sign.name = 'tf_neon_sign'
sign.data.materials.clear(); sign.data.materials.append(mat)
# Blender axes for the game: (x, y, z) = (-plan x, -plan z, plan y): the letters' face (+z) turned to -y (the walkway),
# read left to right from the walkway
sign.rotation_euler = (math.radians(90), 0, 0)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
mn = min(v.co.y for v in sign.data.vertices)
for v in sign.data.vertices: v.co.y -= (max(v.co.y for v in sign.data.vertices) if False else 0.0)
print('SIGN', len(sign.data.polygons), 'faces', 'depth y %.3f..%.3f' % (min(v.co.y for v in sign.data.vertices), max(v.co.y for v in sign.data.vertices)),
      'x %.2f..%.2f' % (min(v.co.x for v in sign.data.vertices), max(v.co.x for v in sign.data.vertices)))
bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, 'tf_neon_sign.fbx'), use_selection=True, object_types={'MESH'},
                         apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'TaxFreeSign.blend'))
