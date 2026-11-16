# The jetways' warning beacons into the game (after Art/JetwayBeacon/make_jetway_beacon.py): /Game/JetwayBeacon/
# SM_JwBeacon and SM_JwLightBar (the yellow bar over the cab's doorway) (Nanite, its own shape as its collision - a real stand you bump into), its pole and plate the jetways' own
# aluminium, its housing and bolts their dark metal, its lens MI_JwBeaconLens: the fire trucks' lens (M_TruckLens: lit
# per copy from the game, LED dots and a bright core) in amber.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_jetway_beacon.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'JetwayBeacon'); DEST = '/Game/JetwayBeacon'
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
def log(*a): unreal.log('ADP_BCN ' + ' '.join(str(x) for x in a))
for _f in glob.glob(os.path.join(ROOT, 'Content', 'JetwayBeacon', '*.uasset')): os.chmod(_f, stat.S_IWRITE | stat.S_IREAD)
if not EAL.does_directory_exist(DEST): EAL.make_directory(DEST)
# the amber lens
lp = DEST + '/MI_JwBeaconLens'
if EAL.does_asset_exist(lp): EAL.delete_asset(lp)
lens = AT.create_asset('MI_JwBeaconLens', DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
unreal.MaterialEditingLibrary.set_material_instance_parent(lens, unreal.load_asset('/Game/Truck/M_TruckLens'))
unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(lens, 'BC', unreal.LinearColor(1.0, 0.28, 0.0, 1.0))
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(lens, 'Glow', 45.0)   # (orange right through: driven harder it blooms to white in the middle - owner)   # (the colour really popping: owner)
EAL.save_loaded_asset(lens)
# the roof lamps' red lens (owner: the lights on top red, the others yellow)
rp = DEST + '/MI_JwBeaconLensRed'
if EAL.does_asset_exist(rp): EAL.delete_asset(rp)
red = AT.create_asset('MI_JwBeaconLensRed', DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
unreal.MaterialEditingLibrary.set_material_instance_parent(red, unreal.load_asset('/Game/Truck/M_TruckLens'))
unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(red, 'BC', unreal.LinearColor(1.0, 0.0, 0.0, 1.0))
unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(red, 'Glow', 8.0)   # (red stays red only this bright: driven harder the camera's colour curve turns it orange)
EAL.save_loaded_asset(red)
MAP = {'JwBcnSteel': '/Game/Terminal/Materials/MI_T_jwAluB', 'JwBcnDark': '/Game/Terminal/Materials/MI_T_jwDark', 'JwBcnLens': lp}
def lens(name, col, glow):
    pth = DEST + '/' + name
    if EAL.does_asset_exist(pth): EAL.delete_asset(pth)
    mi = AT.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, unreal.load_asset('/Game/Truck/M_TruckLens'))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, 'BC', unreal.LinearColor(*col))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, 'Glow', glow)
    EAL.save_loaded_asset(mi); return pth
# the floodlights' white lens, the crew signal's green (and the red above)
white = lens('MI_JwLensWhite', (1.0, 0.97, 0.9, 1.0), 30.0)
green = lens('MI_JwLensGreen', (0.05, 1.0, 0.2, 1.0), 10.0)
for NAME in ('SM_JwBeacon', 'SM_JwLightBar', 'SM_JwFlood', 'SM_JwLamp'):
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
  bs = sm.get_editor_property('body_setup')
  if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  EAL.save_loaded_asset(sm)
  log(NAME, 'slots', [(str(m.get_editor_property('material_slot_name')), m.get_editor_property('material_interface').get_name() if m.get_editor_property('material_interface') else None) for m in mats], 'bounds', sm.get_bounds().box_extent)
# the roof lamps' own mesh with the red lens built in (owner: the lamps on top red - a lens swap in the game didn't take)
rm = DEST + '/SM_JwBeaconRed'
if EAL.does_asset_exist(rm): EAL.delete_asset(rm)
smr = EAL.duplicate_asset(DEST + '/SM_JwBeacon', rm)
mats = []
for m in smr.get_editor_property('static_materials'):
    if str(m.get_editor_property('material_slot_name')) == 'JwBcnLens': m.set_editor_property('material_interface', unreal.load_asset(rp))
    mats.append(m)
smr.set_editor_property('static_materials', mats); EAL.save_loaded_asset(smr)
log('SM_JwBeaconRed', [(str(m.get_editor_property('material_slot_name')), m.get_editor_property('material_interface').get_name()) for m in mats])
# coloured copies: the floodlight with its white lens, the signal lamp red and green
def variant(src, dst, lens_path):
    if EAL.does_asset_exist(DEST + '/' + dst): EAL.delete_asset(DEST + '/' + dst)
    sm2 = EAL.duplicate_asset(DEST + '/' + src, DEST + '/' + dst); ms = []
    for m in sm2.get_editor_property('static_materials'):
        if str(m.get_editor_property('material_slot_name')) == 'JwBcnLens': m.set_editor_property('material_interface', unreal.load_asset(lens_path))
        ms.append(m)
    sm2.set_editor_property('static_materials', ms); EAL.save_loaded_asset(sm2); log(dst, 'made')
variant('SM_JwFlood', 'SM_JwFloodWhite', white)
variant('SM_JwLamp', 'SM_JwLampRed', rp)
variant('SM_JwLamp', 'SM_JwLampGreen', green)
# the jetway's sounds (Art/Sounds/Jetway, CC0 from Freesound - SOURCES.md there): looping
SD = DEST + '/Sounds'
for n in ('JwBeeper', 'JwMotor', 'JwTyres', 'JwLift'):
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Sounds', 'Jetway', n + '.wav'); t.destination_path = SD
    t.automated = True; t.replace_existing = True; t.save = True
    AT.import_asset_tasks([t])
    w = unreal.load_asset(SD + '/' + n)
    if w: w.set_editor_property('looping', True); EAL.save_loaded_asset(w); log('sound', n, 'ok', w.get_editor_property('duration'))
    else: log('sound', n, 'MISSING')
log('done')
