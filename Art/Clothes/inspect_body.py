# Prints the player's body as Blender sees it (size, bones), to tailor garments against.
import bpy, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(HERE / '_bodies' / 'player_body.fbx'))
for o in bpy.context.scene.objects:
    print('BODY', o.name, o.type, tuple(round(d, 3) for d in o.dimensions), o.scale[:], len(o.data.vertices) if o.type == 'MESH' else '')
    if o.type == 'ARMATURE':
        print('BONES', len(o.data.bones), [b.name for b in o.data.bones][:60])
    if o.type == 'MESH':
        import mathutils
        ws = [o.matrix_world @ v.co for v in o.data.vertices]
        print('ZRANGE', min(w.z for w in ws), max(w.z for w in ws), 'Y', min(w.y for w in ws), max(w.y for w in ws), 'mats', [m.name for m in o.data.materials], 'vgroups', len(o.vertex_groups))
