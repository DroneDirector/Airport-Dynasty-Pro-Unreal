# The baggage claim's game parts and sounds into the game (to-do 72; after Art/BaggageCarousel/make_baggage_parts.py and
# Art/Sounds/BaggageClaim/make_claim_sounds.py): /Game/Baggage/SM_* (every slot the terminal finish of the same name;
# Nanite on all but the plates, flaps, bags and button - moved every frame as instances, small), and the belt sounds
# /Game/Baggage/Sounds/BeltStart (one shot), BeltRun (looping), BeltClack.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_baggage.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'BaggageCarousel'); DEST = '/Game/Baggage'
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
def log(*a): unreal.log('ADP_BAG ' + ' '.join(str(x) for x in a))
for f in glob.glob(os.path.join(ROOT, 'Content', 'Baggage', '**', '*.uasset'), recursive=True): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
MOVING = ('SM_BeltPlate', 'SM_Flap', 'SM_BeltButton', 'SM_Bag0', 'SM_Bag1', 'SM_Bag2', 'SM_Bag3')
NAMES = ('SM_BeltBody', 'SM_BeltIncline', 'SM_ButtonPlate', 'SM_CrewBox', 'SM_InputConv', 'SM_WallFrame', 'SM_BeltScreen') + MOVING
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
        mi = unreal.load_asset('/Game/Terminal/Materials/' + slot) or unreal.load_asset('/Game/Terminal/Materials/MI_T_steel')
        m.set_editor_property('material_interface', mi); mats.append(m)
    sm.set_editor_property('static_materials', mats)
    ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', name not in MOVING); sm.set_editor_property('nanite_settings', ns)
    bs = sm.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EAL.save_loaded_asset(sm); log(name, 'in', sm.get_bounds().box_extent)
SD = DEST + '/Sounds'
for n, loop in (('BeltStart', False), ('BeltRun', True), ('BeltClack', False)):
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Sounds', 'BaggageClaim', n + '.wav'); t.destination_path = SD
    t.automated = True; t.replace_existing = True; t.save = True
    AT.import_asset_tasks([t]); w = unreal.load_asset(SD + '/' + n)
    if w: w.set_editor_property('looping', loop); EAL.save_loaded_asset(w); log('sound', n, 'loop' if loop else 'once')
log('done')
unreal.SystemLibrary.quit_editor()
