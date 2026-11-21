# A vehicle's sounds (Art/Sounds/<Vehicle>/*.wav, CC0 - SOURCES.md there) -> /Game/<Vehicle>/Sounds; the ones named *Idle*,
# *Loop* or *Beep* loop. Every vehicle uses this (the tug's own was Tools/import_tug_sounds.py).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_vehicle_sounds.py GPU" -unattended -nullrhi
import os, sys, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
V = sys.argv[1] if len(sys.argv) > 1 else 'GPU'
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
SRC = os.path.join(ROOT, 'Art', 'Sounds', V); SD = '/Game/%s/Sounds' % V
for f in sorted(os.listdir(SRC)):
    if not f.lower().endswith('.wav'): continue
    n = f[:-4]
    ua = os.path.join(ROOT, 'Content', V, 'Sounds', n + '.uasset')
    if os.path.exists(ua): os.chmod(ua, stat.S_IWRITE | stat.S_IREAD)   # (LFS read-only: else the import keeps the old one)
    t = unreal.AssetImportTask(); t.filename = os.path.join(SRC, f); t.destination_path = SD
    t.automated = True; t.replace_existing = True; t.save = True
    AT.import_asset_tasks([t])
    w = unreal.load_asset(SD + '/' + n)
    if w:
        w.set_editor_property('looping', any(k in n for k in ('Idle', 'Loop', 'Beep')))
        EAL.save_loaded_asset(w); unreal.log('ADP_SOUNDS %s ok %.2f' % (n, w.get_editor_property('duration')))
    else: unreal.log('ADP_SOUNDS %s MISSING' % n)
