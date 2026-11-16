# The taxiway lights into the game (to-do 40e, after Art/AirfieldLights/make_airfield_lights.py): /Game/AirfieldLights/
# SM_TwyEdge (the blue elevated edge light) and SM_TwyCentre (the green inset centreline light), Nanite, drawn as
# instances by AirfieldLights.cpp. Their metal the jetways' own; their lenses the fire trucks' lens (M_TruckLens, lit
# per copy) in ICAO aviation blue and green.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_airfield_lights.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'AirfieldLights'); DEST = '/Game/AirfieldLights'
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools(); MEL = unreal.MaterialEditingLibrary
def log(*a): unreal.log('ADP_TWYL ' + ' '.join(str(x) for x in a))
for _f in glob.glob(os.path.join(ROOT, 'Content', 'AirfieldLights', '*.uasset')): os.chmod(_f, stat.S_IWRITE | stat.S_IREAD)
if not EAL.does_directory_exist(DEST): EAL.make_directory(DEST)
def lens(name, col, glow):
    pth = DEST + '/' + name
    if EAL.does_asset_exist(pth): EAL.delete_asset(pth)
    mi = AT.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, unreal.load_asset('/Game/Truck/M_TruckLens'))
    MEL.set_material_instance_vector_parameter_value(mi, 'BC', unreal.LinearColor(*col))
    MEL.set_material_instance_scalar_parameter_value(mi, 'Glow', glow)
    EAL.save_loaded_asset(mi); return pth
blue = lens('MI_TwyLensBlue', (0.02, 0.12, 1.0, 1.0), 90.0)
green = lens('MI_TwyLensGreen', (0.05, 1.0, 0.25, 1.0), 60.0)
for NAME, lp in (('SM_TwyEdge', blue), ('SM_TwyCentre', green)):
    MAP = {'JwBcnSteel': '/Game/Terminal/Materials/MI_T_jwAluB', 'JwBcnDark': '/Game/Terminal/Materials/MI_T_jwDark', 'JwBcnLens': lp}
    path = DEST + '/' + NAME
    if EAL.does_asset_exist(path): EAL.delete_asset(path)
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', True); o.set_editor_property('import_as_skeletal', False)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property('import_materials', False); o.set_editor_property('import_textures', False)
    sd = o.get_editor_property('static_mesh_import_data')
    sd.set_editor_property('combine_meshes', True); sd.set_editor_property('generate_lightmap_u_vs', False)
    sd.set_editor_property('auto_generate_collision', False)
    sd.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t = unreal.AssetImportTask(); t.filename = os.path.join(SRC, NAME + '.fbx'); t.destination_path = DEST
    t.automated = True; t.replace_existing = True; t.save = True; t.options = o
    AT.import_asset_tasks([t])
    sm = unreal.load_asset(path)
    mats = []
    for m in sm.get_editor_property('static_materials'):
        slot = str(m.get_editor_property('material_slot_name'))
        mi = unreal.load_asset(MAP.get(slot, MAP['JwBcnSteel']))
        if not mi: log('MISSING material for', slot)
        m.set_editor_property('material_interface', mi); mats.append(m)
    sm.set_editor_property('static_materials', mats)
    ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', True); sm.set_editor_property('nanite_settings', ns)
    EAL.save_loaded_asset(sm)
    log(NAME, 'slots', [(str(m.get_editor_property('material_slot_name')), m.get_editor_property('material_interface').get_name() if m.get_editor_property('material_interface') else None) for m in mats], 'bounds', sm.get_bounds().box_extent)
log('done')
unreal.SystemLibrary.quit_editor()
