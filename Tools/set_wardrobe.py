# The office wardrobe (Art/Wardrobe/make_wardrobe.py -> wardrobe.glb: WardrobeBody, WardrobeDoorL, WardrobeDoorR) into
# Unreal: /Game/Wardrobe, Nanite on, its own shape as the collision, and its materials: the walnut panels and the
# inside as plain wood colours on the light masts' plain material (a wood texture comes with the fabric work), the
# steel and chrome from the light masts' set.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_wardrobe.py" -unattended -nullrhi
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(ROOT, 'Art', 'Wardrobe')
MESH_DIR, MAT_DIR, LM = '/Game/Wardrobe', '/Game/Materials/Wardrobe', '/Game/Materials/LightMast'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_WARDROBE ' + m)

if eal.does_directory_exist(MESH_DIR): eal.delete_directory(MESH_DIR)
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters(); params.is_automated = True
manager.import_asset(MESH_DIR, unreal.InterchangeManager.create_source_data(os.path.join(ART, 'wardrobe.glb')), params)
manager.wait_until_all_tasks_done(False)
meshes = [a for a in (unreal.load_asset(p) for p in eal.list_assets(MESH_DIR, recursive=True)) if isinstance(a, unreal.StaticMesh)]
for a in meshes: log('mesh ' + a.get_path_name())
if not meshes: raise RuntimeError('no mesh imported')

PLAIN = unreal.load_asset(LM + '/M_LM_Plain')
def instance(name, col, rough, metal=0.0):
    path = MAT_DIR + '/' + name
    if eal.does_asset_exist(path): eal.delete_asset(path)
    mi = tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, PLAIN)
    mel.set_material_instance_vector_parameter_value(mi, 'BaseColor', unreal.LinearColor(*col, 1))
    mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
    mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', metal)
    eal.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi
SLOTS = {
    'WalnutDark': instance('MI_WD_WalnutDark', (0.09, 0.05, 0.028), 0.45),
    'Walnut': instance('MI_WD_Walnut', (0.17, 0.095, 0.05), 0.38),
    'Inside': instance('MI_WD_Inside', (0.42, 0.3, 0.18), 0.6),
    'Hanger': instance('MI_WD_Hanger', (0.3, 0.17, 0.08), 0.5),
    'Steel': unreal.load_asset(LM + '/M_LM_Galvanised'),
    'Chrome': unreal.load_asset(LM + '/MI_LM_Aluminium'),
}
for mesh in meshes:
    for i, sm in enumerate(mesh.get_editor_property('static_materials')):
        slot = str(sm.get_editor_property('material_slot_name'))
        key = next((k for k in SLOTS if k.lower() in slot.lower()), None)
        if key and SLOTS[key]: mesh.set_material(i, SLOTS[key])
        log('%s slot %s -> %s' % (mesh.get_name(), slot, key))
    ns = mesh.get_editor_property('nanite_settings'); ns.enabled = True; mesh.set_editor_property('nanite_settings', ns)
    bs = mesh.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh, only_if_is_dirty=False)
bad = []
for mesh in meshes:
    fresh = unreal.load_asset(mesh.get_path_name())
    for x in fresh.get_editor_property('static_materials'):
        if not x.get_editor_property('material_interface'): bad.append('%s/%s' % (fresh.get_name(), x.get_editor_property('material_slot_name')))
if bad: unreal.log_error('ADP_WARDROBE CHECK FAILED: empty material slots ' + ', '.join(bad))
else: log('CHECK passed: every material slot filled')
unreal.SystemLibrary.quit_editor()
