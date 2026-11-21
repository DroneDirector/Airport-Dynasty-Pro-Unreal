# The pushback tug's sounds (owner 2026-11-21; Art/Sounds/PushbackTug, CC0 from Freesound - SOURCES.md there; cut by
# Tools/make_tug_sounds.py) -> /Game/PushbackTug/Sounds. Tugs.cpp plays them (the idle and the beeper loop).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_tug_sounds.py" -unattended -nullrhi
import os, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
SD = '/Game/PushbackTug/Sounds'
LOOP = {'TugIdle', 'TugReverseBeep'}
for n in ('TugStart', 'TugIdle', 'TugStop', 'TugReverseBeep', 'TugDoorOpen', 'TugDoorClose', 'TugHitchClunk', 'TugHorn'):
    f = os.path.join(ROOT, 'Content', 'PushbackTug', 'Sounds', n + '.uasset')
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Sounds', 'PushbackTug', n + '.wav'); t.destination_path = SD
    t.automated = True; t.replace_existing = True; t.save = True
    AT.import_asset_tasks([t])
    w = unreal.load_asset(SD + '/' + n)
    if w: w.set_editor_property('looping', n in LOOP); EAL.save_loaded_asset(w); unreal.log('ADP_TUGS %s ok %.2f' % (n, w.get_editor_property('duration')))
    else: unreal.log('ADP_TUGS %s MISSING' % n)
