# The jetways' CCTV cameras into the game (after Art/JetwayCamera/make_jetway_camera.py): /Game/JetwayCamera/SM_JwCamBullet
# and SM_JwCamDome (Nanite; the game places them as instanced copies on every jetway, riding with the part they're on).
# Materials: the housing a white painted plastic, the bracket bolts / trim the jetways' own dark metal, the lens and the
# dome a glossy smoked black.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_jetway_camera.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'JetwayCamera'); DEST = '/Game/JetwayCamera'
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools(); ML = unreal.MaterialEditingLibrary
def log(*a): unreal.log('ADP_CAM ' + ' '.join(str(x) for x in a))
for _f in glob.glob(os.path.join(ROOT, 'Content', 'JetwayCamera', '*.uasset')): os.chmod(_f, stat.S_IWRITE | stat.S_IREAD)
if not EAL.does_directory_exist(DEST): EAL.make_directory(DEST)

def plain(name, col, rough, metal=0.0, spec=0.5):
    p = DEST + '/' + name
    if EAL.does_asset_exist(p): EAL.delete_asset(p)
    m = AT.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_instanced_static_meshes', True)
    try: m.set_editor_property('used_with_nanite', True)
    except Exception: pass
    c = ML.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0); c.set_editor_property('constant', unreal.LinearColor(*col, 1.0))
    r = ML.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 200); r.set_editor_property('r', rough)
    mt = ML.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 300); mt.set_editor_property('r', metal)
    sp = ML.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 400); sp.set_editor_property('r', spec)
    ML.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    ML.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    ML.connect_material_property(mt, '', unreal.MaterialProperty.MP_METALLIC)
    ML.connect_material_property(sp, '', unreal.MaterialProperty.MP_SPECULAR)
    ML.recompile_material(m); EAL.save_loaded_asset(m); return p

body = plain('M_JwCamBody', (0.72, 0.72, 0.70), 0.42)
lens = plain('M_JwCamLens', (0.004, 0.004, 0.005), 0.06, 0.0, 0.9)
MAP = {'JwCamBody': body, 'JwCamDark': '/Game/Terminal/Materials/MI_T_jwDark', 'JwCamLens': lens}
for NAME in ('SM_JwCamBullet', 'SM_JwCamDome'):
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
        mi = unreal.load_asset(MAP.get(slot, body))
        if not mi: log('MISSING material for', slot)
        m.set_editor_property('material_interface', mi); mats.append(m)
    sm.set_editor_property('static_materials', mats)
    ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', True); sm.set_editor_property('nanite_settings', ns)
    EAL.save_loaded_asset(sm)
    log(NAME, 'slots', [(str(m.get_editor_property('material_slot_name')), m.get_editor_property('material_interface').get_name() if m.get_editor_property('material_interface') else None) for m in mats], 'bounds', sm.get_bounds().box_extent)
log('DONE')
