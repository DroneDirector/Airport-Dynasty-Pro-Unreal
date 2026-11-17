# The jetway as a Blender file for the owner (Art/Jetway/Jetway.blend): jetway 1 taken from the jetways' own export
# (F:/ADP-Export/jetways.glb, 2026-11-13: the extended jetways the game imports as its "Jetways" model and ships in
# 0.17.21; the older terminal.glb copy was the short jetway from before the extension), everything else removed.
# From 2026-11-17 the jetway is changed in Blender (owner: not in Babylon any more).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_jetway_blend.py
import bpy, os
SRC = 'F:/ADP-Export/jetways.glb'
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Art', 'Jetway', 'Jetway.blend')
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)
root = next(o for o in bpy.data.objects if o.name == 'J-1')
keep = {root} | set(root.children_recursive)
for o in list(bpy.data.objects):
    if o not in keep: bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.outliner.orphans_purge(do_recursive=True)
# stood at the origin, so it opens centred
w = root.matrix_world.translation.copy()
root.location -= w
os.makedirs(os.path.dirname(OUT), exist_ok=True)
for img in bpy.data.images:
    if img.packed_file is None and img.source == 'FILE' and img.has_data: img.pack()
bpy.ops.wm.save_as_mainfile(filepath=OUT, compress=True)
print('SAVED', OUT, len(keep), 'objects')
