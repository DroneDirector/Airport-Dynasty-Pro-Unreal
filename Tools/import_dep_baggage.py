# The departure baggage system's game parts into the game (owner 2026-11-21; after
# Art/BaggageCarousel/make_dep_baggage_parts.py, which also checks them for fighting surfaces): /Game/Baggage/Dep/SM_*
# (every slot the terminal finish of the same name; Nanite on all but the parts moved every frame as instances - the
# diverter paddles). The arrivals' parts (/Game/Baggage, Tools/import_baggage.py) are reused for the plates, flaps,
# bags, screens and sounds.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_dep_baggage.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'BaggageCarousel'); DEST = '/Game/Baggage/Dep'
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
def log(*a): unreal.log('ADP_DEPBAG ' + ' '.join(str(x) for x in a))
# (LFS checkouts are read-only: an import over a read-only .uasset silently keeps the old asset)
for f in glob.glob(os.path.join(ROOT, 'Content', 'Baggage', 'Dep', '*.uasset')): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
MOVING = ('SM_DiverterArm',)
NAMES = ('SM_DepConv', 'SM_DepGuard', 'SM_DepCorner', 'SM_DepLeg', 'SM_WeighBelt', 'SM_CTScanner', 'SM_DiverterPost',
         'SM_MakeupBody', 'SM_ChuteHood', 'SM_LinkTube', 'SM_TubeMouth', 'SM_LinkPost', 'SM_LinkYoke', 'SM_SearchTable',
         'SM_EndStop') + MOVING
for name in NAMES:
    path = DEST + '/' + name
    if EAL.does_asset_exist(path): EAL.delete_asset(path)
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', True); o.set_editor_property('import_as_skeletal', False)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property('import_materials', False); o.set_editor_property('import_textures', False)
    sd = o.get_editor_property('static_mesh_import_data')
    sd.set_editor_property('combine_meshes', True); sd.set_editor_property('generate_lightmap_u_vs', False); sd.set_editor_property('auto_generate_collision', False)
    t = unreal.AssetImportTask(); t.filename = os.path.join(SRC, name + '.fbx'); t.destination_path = DEST
    t.automated = True; t.replace_existing = True; t.save = True; t.options = o
    AT.import_asset_tasks([t])
    sm = unreal.load_asset(path)
    if not sm: log('MISSING', name); continue
    mats = []
    for m in sm.get_editor_property('static_materials'):
        slot = str(m.get_editor_property('material_slot_name'))
        mi = unreal.load_asset('/Game/Terminal/Materials/' + slot)
        if not mi: log('no material', slot, 'on', name); mi = unreal.load_asset('/Game/Terminal/Materials/MI_T_steel')
        m.set_editor_property('material_interface', mi); mats.append(m)
    sm.set_editor_property('static_materials', mats)
    ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', name not in MOVING); sm.set_editor_property('nanite_settings', ns)
    bs = sm.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EAL.save_loaded_asset(sm); log(name, 'in', sm.get_bounds().box_extent, len(mats), 'slots')
log('done')
unreal.SystemLibrary.quit_editor()
