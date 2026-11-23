# The car park's fittings into Unreal (Art/CarPark/make_carpark_props.py -> *.fbx): /Game/CarPark/<name>, Nanite on, their own
# shape as their collision, plain painted materials (MI_CP_*). Checks that no material slot is left empty.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_carpark_props.py" -unattended -nullrhi
import glob, os, stat, time
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR, LM = '/Game/CarPark', '/Game/Materials/LightMast/'
EAL = unreal.EditorAssetLibrary; MEL = unreal.MaterialEditingLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
for f in glob.glob(os.path.join(ROOT, 'Content', 'CarPark', '*.uasset')): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
if EAL.does_directory_exist(DIR): EAL.delete_directory(DIR)
plain = unreal.load_asset(LM + 'M_LM_Plain')


def mi(name, col, metal, rough):
    a = AT.create_asset('MI_' + name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(a, plain)
    MEL.set_material_instance_vector_parameter_value(a, 'BaseColor', unreal.LinearColor(*col, 1))
    MEL.set_material_instance_scalar_parameter_value(a, 'Metallic', metal); MEL.set_material_instance_scalar_parameter_value(a, 'Roughness', rough)
    EAL.save_loaded_asset(a, only_if_is_dirty=False); return a


S = {'CP_Yellow': mi('CP_Yellow', (.75, .42, .012), 0, .45), 'CP_Dark': mi('CP_Dark', (.03, .032, .036), 0, .5), 'CP_White': mi('CP_White', (.75, .75, .75), 0, .45), 'CP_Red': mi('CP_Red', (.5, .01, .01), 0, .45),
     'CP_Steel': mi('CP_Steel', (.46, .47, .48), .85, .42), 'CP_Screen': mi('CP_Screen', (.02, .09, .22), 0, .15), 'CP_Blue': mi('CP_Blue', (.004, .06, .34), 0, .4), 'CP_Concrete': unreal.load_asset(LM + 'MI_LM_Concrete')}
for f in sorted(glob.glob(os.path.join(ROOT, 'Art', 'CarPark', 'cp*.fbx'))):
    name = os.path.basename(f)[:-4]
    o = unreal.FbxImportUI(); o.set_editor_property('import_mesh', True); o.set_editor_property('import_as_skeletal', False)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH); o.set_editor_property('import_materials', False); o.set_editor_property('import_textures', False)
    sd = o.get_editor_property('static_mesh_import_data'); sd.set_editor_property('combine_meshes', True); sd.set_editor_property('generate_lightmap_u_vs', False); sd.set_editor_property('auto_generate_collision', False)
    sd.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t = unreal.AssetImportTask(); t.filename = f; t.destination_path = DIR; t.automated = True; t.replace_existing = True; t.save = True; t.options = o
    AT.import_asset_tasks([t])
    sm = unreal.load_asset(DIR + '/' + name); ms = []
    for m in sm.get_editor_property('static_materials'): m.set_editor_property('material_interface', S.get(str(m.get_editor_property('material_slot_name')))); ms.append(m)
    sm.set_editor_property('static_materials', ms)
    ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', True); sm.set_editor_property('nanite_settings', ns)
    bs = sm.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EAL.save_loaded_asset(sm, only_if_is_dirty=False)
    empty = [str(m.get_editor_property('material_slot_name')) for m in sm.get_editor_property('static_materials') if not m.get_editor_property('material_interface')]
    b = sm.get_bounding_box()
    unreal.log('ADP_CARPARK %s empty %s box %.0f %.0f %.0f .. %.0f %.0f %.0f' % (name, empty, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))
    if empty: unreal.log_error('ADP_CARPARK %s EMPTY SLOTS' % name)
unreal.SystemLibrary.quit_editor()
