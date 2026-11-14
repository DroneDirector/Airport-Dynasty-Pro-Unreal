# The self check-in kiosk (owner, 2026-11-14; modelled by the cloud chat from his reference photos: Art/Kiosk, built by
# Tools/kiosk_model.py) into Unreal: the mesh to /Game/Terminal/Props/Kiosk (Nanite on, its own shape as the collision),
# its materials on the terminal's own masters (Tools/set_terminal_materials.py): the body in white powder coat (the
# clear-coat master), the handle dark grey, black plastic, steel, the green light glowing; the screen and the black glass
# round it a mirror-smooth clear coat over black - the hall, its lights and the people passing reflect in it as in the
# terminal's windows (Lumen), the "Touch to start" picture glowing under it. Checks no material slot is left empty.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_kiosk.py" -unattended
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(ROOT, 'Art', 'Kiosk')
DIR, MASTERS = '/Game/Terminal/Props/Kiosk', '/Game/Terminal/Masters/'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_KIOSK ' + m)

# (run again: the materials are updated in place and the mesh re-imported over itself - deleting the folder first left
# the slots empty on a second run, the deleted materials still held in memory)
MESH_DIR = DIR + '/SM_CheckinKiosk'
# (the repo's Git LFS 'lockable' makes committed .uasset files read-only on disk: every save then fails quietly and the
# game keeps the old kiosk - 2026-11-14. Made writable first.)
import stat
for d, _, fs in os.walk(os.path.join(ROOT, 'Content', 'Terminal', 'Props', 'Kiosk')):
    for f in fs: os.chmod(os.path.join(d, f), stat.S_IWRITE | stat.S_IREAD)
if eal.does_directory_exist(MESH_DIR): eal.delete_directory(MESH_DIR)
def tex(file, name):
    t = unreal.AssetImportTask(); t.filename = os.path.join(ART, file); t.destination_path = DIR; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    tools.import_asset_tasks([t]); a = unreal.load_asset(DIR + '/' + name)
    a.set_editor_property('srgb', True); eal.save_loaded_asset(a, only_if_is_dirty=False); return a
T_UI = tex('T_Kiosk_ScreenUI.png', 'T_Kiosk_ScreenUI')
T_LABEL = tex('T_Kiosk_Label.png', 'T_Kiosk_Label')
T_BAY = tex('T_Kiosk_Bay.png', 'T_Kiosk_Bay')       # (the photos, colour noise cleaned: Art/Kiosk/clean_textures.py)
T_VENTS = tex('T_Kiosk_Vents.png', 'T_Kiosk_Vents')

def mi(name, master, bc=(1, 1, 1), r=0.5, m=0.0, cc=None, ccr=0.1, ec=None, eb=1.0, base=None, emi=None):
    a = unreal.load_asset(DIR + '/' + name) if eal.does_asset_exist(DIR + '/' + name) else tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(a, unreal.load_asset(MASTERS + master))
    mel.set_material_instance_vector_parameter_value(a, 'BC', unreal.LinearColor(*bc, 1))
    mel.set_material_instance_scalar_parameter_value(a, 'R', r)
    mel.set_material_instance_scalar_parameter_value(a, 'M', m)
    if cc is not None:
        mel.set_material_instance_scalar_parameter_value(a, 'CC', cc); mel.set_material_instance_scalar_parameter_value(a, 'CCR', ccr)
    if ec is not None:
        mel.set_material_instance_vector_parameter_value(a, 'EC', unreal.LinearColor(*ec, 1)); mel.set_material_instance_scalar_parameter_value(a, 'EB', eb)
    if base: mel.set_material_instance_texture_parameter_value(a, 'Base', base)
    if emi: mel.set_material_instance_texture_parameter_value(a, 'Emi', emi)
    mel.update_material_instance(a); eal.save_loaded_asset(a, only_if_is_dirty=False); return a

COAT, SURF = 'M_TSurface_Coat', 'M_TSurface'
SLOTS = {
    'WhiteCoat': mi('MI_Kiosk_WhiteCoat', COAT, (0.80, 0.81, 0.81), r=0.35, cc=0.6, ccr=0.18),                 # powder coat
    'BasePlate': mi('MI_Kiosk_BasePlate', SURF, (0.07, 0.075, 0.08), r=0.5, m=0.5),
    'Vent': mi('MI_Kiosk_Vent', SURF, (0.03, 0.03, 0.035), r=0.7),
    'Steel': mi('MI_Kiosk_Steel', SURF, (0.7, 0.7, 0.72), r=0.3, m=1.0),
    'BlackPlastic': mi('MI_Kiosk_BlackPlastic', SURF, (0.018, 0.018, 0.02), r=0.55),
    'Keys': mi('MI_Kiosk_Keys', SURF, (0.32, 0.33, 0.35), r=0.5),
    'ScannerGlass': mi('MI_Kiosk_ScannerGlass', COAT, (0.05, 0.12, 0.10), r=0.1, cc=1.0, ccr=0.03),
    # the status lamp on top of the handle (connected): bright enough that Lumen lights the handle and the kiosk's top green with it
    'GreenLight': mi('MI_Kiosk_GreenLight', SURF, (0.1, 0.9, 0.45), r=0.3, ec=(0.15, 1.0, 0.45), eb=12.0),   # (owner: the floor only faintly lit by it)
    'SmallLCD': mi('MI_Kiosk_SmallLCD', COAT, (0.05, 0.06, 0.06), r=0.1, cc=1.0, ccr=0.03, ec=(0.30, 0.36, 0.35), eb=1.5),
    'Label': mi('MI_Kiosk_Label', SURF, (1, 1, 1), r=0.5, base=T_LABEL),
    # the screen and its bezel: black glass, a mirror-smooth coat (reflections like the windows'), the picture glowing under
    'BezelGlass': mi('MI_Kiosk_BezelGlass', COAT, (0.006, 0.006, 0.008), r=0.05, cc=1.0, ccr=0.02),
    'Screen': mi('MI_Kiosk_Screen', COAT, (0.0, 0.0, 0.0), r=0.02, cc=1.0, ccr=0.0, ec=(1, 1, 1), eb=2.2, emi=T_UI),
    # the device bay (printer, passport scanner, payment terminal) and the back's vent panel: the owner's photos as textures
    'Bay': mi('MI_Kiosk_Bay', SURF, (1, 1, 1), r=0.45, base=T_BAY),
    'VentPanel': mi('MI_Kiosk_VentPanel', SURF, (1, 1, 1), r=0.4, base=T_VENTS),
    'HandleGrey': mi('MI_Kiosk_HandleGrey', SURF, (0.16, 0.17, 0.18), r=0.45),
}
log('materials ready')

manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters(); params.is_automated = True
manager.import_asset(DIR, unreal.InterchangeManager.create_source_data(os.path.join(ART, 'SM_CheckinKiosk.glb')), params)
manager.wait_until_all_tasks_done(False)
meshes = [a for a in (unreal.load_asset(p) for p in eal.list_assets(DIR, recursive=True)) if isinstance(a, unreal.StaticMesh)]
if not meshes: raise RuntimeError('no mesh imported')
bad = []
for mesh in meshes:
    for i, s in enumerate(mesh.get_editor_property('static_materials')):
        slot = str(s.get_editor_property('material_slot_name'))
        key = next((k for k in sorted(SLOTS, key=len, reverse=True) if k.lower() in slot.lower()), None)
        if key: mesh.set_material(i, SLOTS[key])
        log('%s slot %s -> %s' % (mesh.get_name(), slot, key))
    ns = mesh.get_editor_property('nanite_settings'); ns.enabled = True; mesh.set_editor_property('nanite_settings', ns)
    bs = mesh.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    fresh = unreal.load_asset(mesh.get_path_name())
    bad += ['%s/%s' % (fresh.get_name(), s.get_editor_property('material_slot_name')) for s in fresh.get_editor_property('static_materials') if not s.get_editor_property('material_interface')]
    log('mesh ready: %s, bounds %s' % (mesh.get_path_name(), mesh.get_bounding_box()))
on_disk = os.path.getmtime(os.path.join(ROOT, 'Content', 'Terminal', 'Props', 'Kiosk', 'SM_CheckinKiosk', 'StaticMeshes', 'SM_CheckinKiosk.uasset'))
if on_disk < os.path.getmtime(os.path.join(ART, 'SM_CheckinKiosk.glb')): bad.append('the mesh file on disk is older than the model (not saved)')
if bad: unreal.log_error('ADP_KIOSK CHECK FAILED: empty material slots ' + ', '.join(bad))
else: log('CHECK passed: every material slot filled')
unreal.SystemLibrary.quit_editor()
