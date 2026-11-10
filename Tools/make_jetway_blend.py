# The jetway as a Blender file for the owner (Art/Jetway/Jetway.blend): jetway 1 taken from the Babylon model's own
# export (F:/ADP-Export/terminal.glb, the same geometry the game imports), everything else removed; the four jetways
# are the same model. Run again whenever the terminal is exported again, so the file stays up to date.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_jetway_blend.py
import bpy, os
SRC = 'F:/ADP-Export/terminal.glb'
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
