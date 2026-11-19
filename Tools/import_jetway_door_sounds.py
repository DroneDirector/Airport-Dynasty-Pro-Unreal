# The jetway service door's sounds (owner, 2026-11-19; Art/Sounds/Jetway, CC0 from Freesound - SOURCES.md there):
# /Game/JetwayBeacon/Sounds/JwSvcOpen and JwSvcClose, not looping (JetwayRigs.cpp plays them at the door).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_jetway_door_sounds.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
SD = '/Game/JetwayBeacon/Sounds'
for n in ('JwSvcOpen', 'JwSvcClose'):
    f = os.path.join(ROOT, 'Content', 'JetwayBeacon', 'Sounds', n + '.uasset')
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Sounds', 'Jetway', n + '.wav'); t.destination_path = SD
    t.automated = True; t.replace_existing = True; t.save = True
    AT.import_asset_tasks([t])
    w = unreal.load_asset(SD + '/' + n)
    if w: w.set_editor_property('looping', False); EAL.save_loaded_asset(w); unreal.log('ADP_JWDS %s ok %.2f' % (n, w.get_editor_property('duration')))
    else: unreal.log('ADP_JWDS %s MISSING' % n)
