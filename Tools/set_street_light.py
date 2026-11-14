# The road street light (Art/StreetLight/make_streetlight.py -> streetlight.glb) into Unreal: the mesh to
# /Game/StreetLight (Nanite on, its own shape as the collision), on the light mast's own materials
# (/Game/Materials/LightMast: the galvanised steel, the LED face whose glow the game sets at night, the plain parts) -
# the same steel and the same lamp picture as the apron's masts. Checks no material slot is left empty.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_street_light.py" -unattended
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(ROOT, 'Art', 'StreetLight')
MESH_DIR, MAT = '/Game/StreetLight', '/Game/Materials/LightMast/'
eal = unreal.EditorAssetLibrary

def log(m): unreal.log('ADP_STREETLIGHT ' + m)

# a fresh import every time (the model is rebuilt from its script)
if eal.does_directory_exist(MESH_DIR): eal.delete_directory(MESH_DIR)
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters(); params.is_automated = True
manager.import_asset(MESH_DIR, unreal.InterchangeManager.create_source_data(os.path.join(ART, 'streetlight.glb')), params)
manager.wait_until_all_tasks_done(False)
meshes = [unreal.load_asset(p) for p in eal.list_assets(MESH_DIR, recursive=True)]
meshes = [m for m in meshes if isinstance(m, unreal.StaticMesh)]
if not meshes: raise RuntimeError('no mesh imported')

SLOTS = {'Galvanised': 'M_LM_Galvanised', 'LEDFace': 'M_LM_LEDFace', 'DarkMetal': 'MI_LM_DarkMetal', 'Concrete': 'MI_LM_Concrete',
         'Bolt': 'MI_LM_Bolt', 'Aluminium': 'MI_LM_Aluminium'}
for mesh in meshes:
    for i, sm in enumerate(mesh.get_editor_property('static_materials')):
        slot = str(sm.get_editor_property('material_slot_name'))
        key = next((k for k in SLOTS if k.lower() in slot.lower()), None)
        if key: mesh.set_material(i, unreal.load_asset(MAT + SLOTS[key]))
        log('%s slot %s -> %s' % (mesh.get_name(), slot, key))
    ns = mesh.get_editor_property('nanite_settings'); ns.enabled = True; mesh.set_editor_property('nanite_settings', ns)
    bs = mesh.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    log('mesh ready: %s' % mesh.get_path_name())
bad = []
for mesh in meshes:
    fresh = unreal.load_asset(mesh.get_path_name())
    for x in fresh.get_editor_property('static_materials'):
        if not x.get_editor_property('material_interface'): bad.append('%s/%s' % (fresh.get_name(), x.get_editor_property('material_slot_name')))
if bad: unreal.log_error('ADP_STREETLIGHT CHECK FAILED: empty material slots ' + ', '.join(bad))
else: log('CHECK passed: every material slot filled')
unreal.SystemLibrary.quit_editor()
