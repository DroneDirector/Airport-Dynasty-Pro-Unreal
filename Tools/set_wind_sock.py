# The runway windsock (Art/WindSock/make_windsock.py -> windsock.glb) into Unreal: the mast and the swivelling frame to
# /Game/WindSock (the mast with Nanite, the thin frame without; their own shape as the collision), their materials from
# the light masts' set (galvanised steel, dark metal, concrete, the red obstruction light) plus the floodlights' lens
# (its glow set by the game at night, parameter "EmissiveStrength"), and the sock's fabric (two-sided, so the inside of
# the mouth shows; orange and white instances) for the sock the game draws (WindSocks.cpp).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_wind_sock.py" -unattended -nullrhi
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(ROOT, 'Art', 'WindSock')
MESH_DIR, MAT_DIR, LM = '/Game/WindSock', '/Game/Materials/WindSock', '/Game/Materials/LightMast'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def log(m): unreal.log('ADP_WINDSOCK ' + m)

# ---- the meshes (imported again each time: the model is the Blender file's) ----
if eal.does_directory_exist(MESH_DIR): eal.delete_directory(MESH_DIR)
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters(); params.is_automated = True
manager.import_asset(MESH_DIR, unreal.InterchangeManager.create_source_data(os.path.join(ART, 'windsock.glb')), params)
manager.wait_until_all_tasks_done(False)
meshes = [unreal.load_asset(p) for p in eal.list_assets(MESH_DIR, recursive=True)]
meshes = [a for a in meshes if isinstance(a, unreal.StaticMesh)]
for a in meshes: log('mesh ' + a.get_path_name())
if not meshes: raise RuntimeError('no mesh imported')

# ---- the materials ----
def new_material(name):
    path = MAT_DIR + '/' + name
    if eal.does_asset_exist(path): eal.delete_asset(path)
    m = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_nanite', True); m.set_editor_property('used_with_instanced_static_meshes', True)
    return m
def instance(name, parent, col, metal=None, rough=None, emit=None, strength=None):
    path = MAT_DIR + '/' + name
    if eal.does_asset_exist(path): eal.delete_asset(path)
    mi = tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    mel.set_material_instance_vector_parameter_value(mi, 'BaseColor', unreal.LinearColor(*col, 1))
    if metal is not None: mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', metal)
    if rough is not None: mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
    if emit:
        mel.set_material_instance_vector_parameter_value(mi, 'EmissiveColor', unreal.LinearColor(*emit, 1))
        mel.set_material_instance_scalar_parameter_value(mi, 'EmissiveStrength', strength)
    eal.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi

# the sock's fabric: two-sided nylon, matte, a little light comes through it (as a real sock glows against the sky)
m = new_material('M_WS_Fabric')
m.set_editor_property('two_sided', True)
col = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
col.set_editor_property('parameter_name', 'BaseColor'); col.set_editor_property('default_value', unreal.LinearColor(0.9, 0.25, 0.02, 1))
rough = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 200)
rough.set_editor_property('parameter_name', 'Roughness'); rough.set_editor_property('default_value', 0.8)
glow = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -700, 350)
glow.set_editor_property('parameter_name', 'Glow'); glow.set_editor_property('default_value', 0.0)
mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 350)
mel.connect_material_expressions(col, '', mul, 'A'); mel.connect_material_expressions(glow, '', mul, 'B')
mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
FABRIC = m
instance('MI_WS_SockOrange', FABRIC, (0.92, 0.22, 0.015))
instance('MI_WS_SockWhite', FABRIC, (0.80, 0.80, 0.78))

PLAIN = unreal.load_asset(LM + '/M_LM_Plain')
SLOTS = {
    'Galvanised': unreal.load_asset(LM + '/M_LM_Galvanised'),
    'DarkMetal': unreal.load_asset(LM + '/MI_LM_DarkMetal'),
    'Concrete': unreal.load_asset(LM + '/MI_LM_Concrete'),
    'ObstructionLight': unreal.load_asset(LM + '/MI_LM_ObstructionLight'),
    'LampLens': instance('MI_WS_LampLens', PLAIN, (0.85, 0.85, 0.8), 0.0, 0.15, (1.0, 0.93, 0.8), 0.0),
}
missing = [k for k, v in SLOTS.items() if not v]
if missing: unreal.log_error('ADP_WINDSOCK light mast materials missing: ' + ', '.join(missing))

for mesh in meshes:
    thin = 'frame' in mesh.get_name().lower()
    for i, sm in enumerate(mesh.get_editor_property('static_materials')):
        slot = str(sm.get_editor_property('material_slot_name'))
        key = next((k for k in SLOTS if k.lower() in slot.lower()), None)
        if key and SLOTS[key]: mesh.set_material(i, SLOTS[key])
        log('%s slot %s -> %s' % (mesh.get_name(), slot, key))
    ns = mesh.get_editor_property('nanite_settings'); ns.enabled = not thin; mesh.set_editor_property('nanite_settings', ns)
    bs = mesh.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    log('mesh ready: %s (Nanite %s)' % (mesh.get_path_name(), 'off' if thin else 'on'))

# CHECK (owner's rule): the saved meshes reloaded from disk; no material slot may be empty
bad = []
for mesh in meshes:
    fresh = unreal.load_asset(mesh.get_path_name())
    for x in fresh.get_editor_property('static_materials'):
        if not x.get_editor_property('material_interface'): bad.append('%s/%s' % (fresh.get_name(), x.get_editor_property('material_slot_name')))
if bad: unreal.log_error('ADP_WINDSOCK CHECK FAILED: empty material slots ' + ', '.join(bad))
else: log('CHECK passed: every material slot filled in the saved meshes')
unreal.SystemLibrary.quit_editor()
