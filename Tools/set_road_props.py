# The roads' small fittings into Unreal (owner, 2026-11-14): the gully grating (Art/Drain/make_drain.py -> drain.glb) and
# the round manhole cover (Art/Manhole/make_manhole.py -> manhole.glb), each to /Game/RoadProps/<name> (Nanite on, its own
# shape as the collision), on cast iron with a little rust in its colour, the dark sump, the light mast's concrete.
# Checks no material slot is left empty. The game lays them along the roads (RoadNetwork.cpp "props").
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_road_props.py" -unattended
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DIR, LM = '/Game/RoadProps', '/Game/Materials/LightMast/'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_ROADPROPS ' + m)

if eal.does_directory_exist(DIR): eal.delete_directory(DIR)
plain = unreal.load_asset(LM + 'M_LM_Plain')
def mi(name, col, metal, rough):
    a = tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(a, plain)
    mel.set_material_instance_vector_parameter_value(a, 'BaseColor', unreal.LinearColor(*col, 1))
    mel.set_material_instance_scalar_parameter_value(a, 'Metallic', metal)
    mel.set_material_instance_scalar_parameter_value(a, 'Roughness', rough)
    eal.save_loaded_asset(a, only_if_is_dirty=False); return a
SLOTS = {'CastIron': mi('MI_RP_CastIron', (0.115, 0.098, 0.082), 0.55, 0.62),   # (worn cast iron: a rusty brown in the dark grey)
         'Sump': mi('MI_RP_Sump', (0.012, 0.011, 0.010), 0.0, 0.95),
         'Concrete': unreal.load_asset(LM + 'MI_LM_Concrete')}
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters(); params.is_automated = True
bad = []
for name, glb in (('Drain', 'Art/Drain/drain.glb'), ('Manhole', 'Art/Manhole/manhole.glb')):
    d = DIR + '/' + name
    manager.import_asset(d, unreal.InterchangeManager.create_source_data(os.path.join(ROOT, glb)), params)
    manager.wait_until_all_tasks_done(False)
    for p in eal.list_assets(d, recursive=True):
        mesh = unreal.load_asset(p)
        if not isinstance(mesh, unreal.StaticMesh): continue
        for i, s in enumerate(mesh.get_editor_property('static_materials')):
            slot = str(s.get_editor_property('material_slot_name'))
            key = next((k for k in SLOTS if k.lower() in slot.lower()), None)
            if key: mesh.set_material(i, SLOTS[key])
        ns = mesh.get_editor_property('nanite_settings'); ns.enabled = True; mesh.set_editor_property('nanite_settings', ns)
        bs = mesh.get_editor_property('body_setup')
        if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        eal.save_loaded_asset(mesh, only_if_is_dirty=False)
        fresh = unreal.load_asset(mesh.get_path_name())
        bad += ['%s/%s' % (fresh.get_name(), s.get_editor_property('material_slot_name')) for s in fresh.get_editor_property('static_materials') if not s.get_editor_property('material_interface')]
        log('mesh ready: ' + mesh.get_path_name())
if bad: unreal.log_error('ADP_ROADPROPS CHECK FAILED: empty material slots ' + ', '.join(bad))
else: log('CHECK passed: every material slot filled')
unreal.SystemLibrary.quit_editor()
