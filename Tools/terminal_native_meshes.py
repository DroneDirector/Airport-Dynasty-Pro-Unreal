# The true-Unreal terminal (to-do 42), part 2b: the terminal's own native piece set. Every distinct piece found by
# Tools/terminal_dedup.py (Data/terminal-pieces.json) is copied once into /Game/Terminal/Meshes under its own name,
# geometry untouched, Nanite and collision as imported; ABabylonModel then draws every copy of a piece as one
# instanced mesh (the copies' offsets kept). The old /Game/Babylon/Terminal set is left as it is (backup) until the
# native terminal is approved.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/terminal_native_meshes.py" -unattended -nullrhi
import unreal, json, os
eal = unreal.EditorAssetLibrary
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = '/Game/Babylon/Terminal/terminal/StaticMeshes'
DST = '/Game/Terminal/Meshes'
def log(m): unreal.log('ADP_NATIVE ' + m)
pieces = json.load(open(os.path.join(ROOT, 'Data', 'terminal-pieces.json')))
canon = sorted(set(v[0] for v in pieces.values()))
if not eal.does_directory_exist(DST): eal.make_directory(DST)
done = made = 0
for c in canon:
    dst = DST + '/' + c
    if not eal.does_asset_exist(dst):
        if eal.duplicate_asset(SRC + '/' + c, dst): made += 1
        else: log('could not copy ' + c); continue
    done += 1
    if done % 200 == 0:
        eal.save_directory(DST, only_if_is_dirty=True, recursive=False)
        log('%d / %d pieces' % (done, len(canon)))
eal.save_directory(DST, only_if_is_dirty=True, recursive=False)
log('ready: %d pieces in %s (%d new)' % (done, DST, made))
unreal.SystemLibrary.quit_editor()
