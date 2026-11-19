# The fire truck's siren and horn (owner, 2026-11-19: real recordings; Art/Sounds/FireTruck, CC0 from Freesound -
# SOURCES.md there): /Game/FireStation/Sounds/FtSiren (looping) and FtHorn (one blast). FireTruck.cpp plays them.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_firetruck_sounds.py" -unattended -nullrhi
import os, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
SD = '/Game/FireStation/Sounds'
for n, loop in (('FtSiren', True), ('FtHorn', False)):
    f = os.path.join(ROOT, 'Content', 'FireStation', 'Sounds', n + '.uasset')
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Sounds', 'FireTruck', n + '.wav'); t.destination_path = SD
    t.automated = True; t.replace_existing = True; t.save = True
    AT.import_asset_tasks([t])
    w = unreal.load_asset(SD + '/' + n)
    if w: w.set_editor_property('looping', loop); EAL.save_loaded_asset(w); unreal.log('ADP_FTS %s ok %.2f' % (n, w.get_editor_property('duration')))
    else: unreal.log('ADP_FTS %s MISSING' % n)
