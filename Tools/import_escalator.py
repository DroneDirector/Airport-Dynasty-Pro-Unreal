# The escalators into the game (after Art/Escalator/make_escalator.py): /Game/Escalator/SM_EscStep (Nanite, the game
# moves its copies round the loop), SM_EscBody (the still parts; not Nanite - it carries the balustrade glass),
# SM_EscRamp (the invisible walking surface, its own shape as its collision). Every slot gets the terminal's own
# material so it matches the building: brushed metal as the lifts', the terminal's glass, the jetways' aluminium
# and rubber, the terminal's yellow and steel, an LED line.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_escalator.py" -unattended -nullrhi
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'Escalator')
DEST = '/Game/Escalator'
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
TM = '/Game/Terminal/Materials/'
MAP = {'M_EscStainless': 'MI_T_liftBrushed', 'M_EscStepMetal': 'MI_T_jwAluB', 'M_EscYellow': 'MI_T_yellow',
       'M_EscFloorPlate': 'MI_T_steel', 'M_EscGlass': 'MI_T_glass', 'M_EscHandrail': 'MI_T_jwRubber',
       'M_EscTruss': 'MI_T_steel', 'M_EscLight': 'MI_T_J-1_LED'}
def log(*a): unreal.log('ADP_ESC ' + ' '.join(str(x) for x in a))
# (Git LFS leaves committed .uasset files read-only: Unreal then can't replace them and the import fails silently -
#  the game kept an old copy of the new parts; the flag is cleared first)
import glob, stat
for _f in glob.glob(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Content', 'Escalator', '*.uasset')):
    os.chmod(_f, stat.S_IWRITE | stat.S_IREAD)


for name in ('SM_EscStep', 'SM_EscBody', 'SM_EscRamp', 'SM_EscBodyShort', 'SM_EscRampShort'):   # (the Short ones: the arrivals escalators, make_escalator_short.py)
    path = '%s/%s' % (DEST, name)
    if EAL.does_asset_exist(path): EAL.delete_asset(path)
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', True); o.set_editor_property('import_as_skeletal', False)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property('import_materials', False); o.set_editor_property('import_textures', False)
    sd = o.get_editor_property('static_mesh_import_data')
    sd.set_editor_property('combine_meshes', True); sd.set_editor_property('generate_lightmap_u_vs', False)
    sd.set_editor_property('auto_generate_collision', False)
    sd.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t = unreal.AssetImportTask()
    t.filename = os.path.join(SRC, name + '.fbx'); t.destination_path = DEST; t.automated = True; t.replace_existing = True; t.save = True
    t.options = o
    AT.import_asset_tasks([t])
    sm = unreal.load_asset(path)
    mats = []
    for m in sm.get_editor_property('static_materials'):
        slot = str(m.get_editor_property('material_slot_name'))
        mi = unreal.load_asset(TM + MAP.get(slot, 'MI_T_steel'))
        m.set_editor_property('material_interface', mi); mats.append(m)
    sm.set_editor_property('static_materials', mats)
    ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', name == 'SM_EscStep')
    sm.set_editor_property('nanite_settings', ns)
    bs = sm.get_editor_property('body_setup')
    if bs:
        bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if name != 'SM_EscStep' else unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    EAL.save_loaded_asset(sm)
    log(name, 'slots', [str(m.get_editor_property('material_slot_name')) for m in mats], 'bounds', sm.get_bounds().box_extent)
log('done')
