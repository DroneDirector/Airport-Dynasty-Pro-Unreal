# A finished pet (make_pet -> rig_pet -> anim_pet) out to Unreal: Models/<Kind>/<Name>/SK_<Name>.fbx with its mesh,
# skeleton and every move, for Tools/import_pet.py. The armature object is called "Armature" in the file so Unreal
# doesn't add it as an extra root bone above 'root'.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Pets/export_pet.py -- <Dogs|Cats> <Name>
import bpy, os, sys
a = sys.argv[sys.argv.index('--') + 1:]
KIND, NAME = a[0], a[1]
HERE = os.path.dirname(os.path.abspath(__file__))
D = os.path.join(HERE, 'Models', KIND, NAME)
bpy.ops.wm.open_mainfile(filepath=os.path.join(D, NAME + '.blend'))
rig = [o for o in bpy.data.objects if o.type == 'ARMATURE'][0]
mesh = [o for o in bpy.data.objects if o.type == 'MESH'][0]
rig.name = 'Armature'; mesh.name = 'SK_' + NAME
# every material slot filled (the project's rule), and the moves in the file
assert all(s.material for s in mesh.material_slots), 'an empty material slot'
rig.animation_data.action = None
for act in bpy.data.actions: act.use_fake_user = True
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); mesh.select_set(True)
bpy.context.view_layer.objects.active = rig
out = os.path.join(D, 'SK_%s.fbx' % NAME)
bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False,
                         apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', colors_type='SRGB',
                         bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
                         bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0, path_mode='STRIP')
print('EXPORT', out, [x.name for x in bpy.data.actions])
