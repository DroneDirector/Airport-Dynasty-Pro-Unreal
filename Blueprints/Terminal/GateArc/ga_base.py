# Gate arc, step 1: the real terminal's west part (x < 60, the airside half) out of a COPY of F:/ADP-Export/tc_exist.blend
# (the game's own layout, meshes and jetways; Blueprints/Terminal/Changes/Detailed/build_scene.py) -> F:/ADP-Export/ga_base.blend
#   blender -b --python ga_base.py          (F:/ADP-Export/ga_exist.blend = the copy)
import bpy
from mathutils import Vector
SRC = 'F:/ADP-Export/ga_exist.blend'
bpy.ops.wm.open_mainfile(filepath=SRC)
keep = []
for o in bpy.data.objects:
    if o.type != 'MESH':
        if o.name.startswith('A320@STAND-') and o.matrix_world.translation.x + 200 < 60: keep.append(o.name)
        continue
    b = [o.matrix_world @ Vector(c) for c in o.bound_box]
    x0 = min(v.x for v in b) + 200; z0, z1 = min(v.y for v in b), max(v.y for v in b); h1 = max(v.z for v in b); zc = (z0 + z1) / 2
    if o.name in ('Green', 'Red') or x0 > 60: continue
    if o.get('grp') == 'SITE' and (o.name.startswith(('SITE/ROAD', 'SITE/CARPARK', 'SITE/GRASS', 'SITE/KERB', 'SITE/SIDE', 'SITE/APRON')) or zc > 83 or -100 < zc < -20): continue
    if z1 > 51 or z1 - z0 > 20 or h1 > 11.5: keep.append(o.name)
K = set(keep)
bpy.ops.wm.read_factory_settings(use_empty=True)
with bpy.data.libraries.load(SRC, link=False) as (a, b): b.objects = [n for n in a.objects if n in K]
c = bpy.data.collections.new('EXIST'); bpy.context.scene.collection.children.link(c)
for o in bpy.data.objects:                                # (the parents too: their frames place the jetways)
    if not o.users_collection or o.name in K: c.objects.link(o) if o.name not in c.objects else None
bpy.ops.wm.save_as_mainfile(filepath='F:/ADP-Export/ga_base.blend', compress=False)
