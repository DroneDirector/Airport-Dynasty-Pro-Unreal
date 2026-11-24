# One command for a new in-game object, every step in the order that works (owner, 2026-11-14: "each time you figure out
# a solution ... put that in the script as well, so it is automatic"). Lessons built in:
#   - the Blender script builds Art/<Object>/<Object>.blend and its .glb (asserts no empty material slots)
#   - Tools/find_zfight.py before import; it stops here if any surfaces fight (fix the gaps in the Blender script)
#   - the import with --assets --shared (one mesh per glTF mesh, instanced, Nanite on)
#   - Tools/fix_babylon_materials.py: without it the materials are black or missing on the instanced parts the game draws
#   - Tools/glb_nodes.py AFTER the import: it reads the imported names (mesh or object naming) - made before, the game
#     looks for parts under names that don't exist and the model is invisible
#   - Data/babylon-<name>.json copied to Content/Data (the editor reads Content/Data first)
# Mirroring: the loader mirrors a Blender model's x (left and right swap) - lettering and one-sided layouts must be drawn
# mirrored in the Blender script (see Art/DoorPanel/make_doorpanel.py: u runs right to left).
# Placing it: ABabylonModel::Place gives the model's root a half turn on top of the actor's - a Blender model's face
# (+y) then looks along the actor's RIGHT vector; turn the actor so its right points where the face should look.
#   python Tools/import_model.py Art/DoorPanel/make_doorpanel.py Art/DoorPanel/doorpanel.glb DoorPanel
import shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BLENDER = r'C:/Program Files/Blender Foundation/Blender 3.3/blender.exe'
EDITOR = r'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
PROJECT = str(ROOT / 'AirportDynastyPro.uproject')


def run(args, what):
    print('==', what)
    r = subprocess.run(args, cwd=ROOT, capture_output=True, text=True)
    return r.stdout + r.stderr


def editor_py(script_and_args, what):
    out = run([EDITOR, PROJECT, '-ExecutePythonScript=' + script_and_args, '-unattended', '-nullrhi'], what)
    log = (ROOT / 'Saved' / 'Logs' / 'AirportDynastyPro.log').read_text(encoding='utf-8', errors='ignore')
    errs = [l for l in log.splitlines() if 'LogPython: Error' in l or 'Traceback' in l]
    if errs:
        sys.exit('\n'.join(errs[:10]))
    return log


def main(blend_script, glb, name):
    lower = name.lower()
    # 1. the model, from its Blender script ('-' : a finished .glb made elsewhere, e.g. the villa and the dealership from
    # the phone chat, used as it is)
    if blend_script != '-':
        out = run([BLENDER, '-b', '--python', blend_script], 'Blender build')
        if 'Traceback' in out or 'Error' in out.split('DONE')[-1:][0][:0]:
            sys.exit(out[-3000:])
    # 2. no fighting surfaces
    # (its pairs go to Data/<name>-zfight.json: the game parts them by a hair - a finished .glb is taken so, our own
    # Blender models must have none; the second argument was the bare name and wrote a stray file in the project root)
    zf = 'Data/%s-zfight.json' % lower
    out = run([sys.executable, 'Tools/find_zfight.py', glb, zf], 'flicker check')
    print(out.strip().splitlines()[-1])
    shutil.copy(ROOT / zf, ROOT / 'Content' / zf)
    if not out.strip().splitlines()[-1].strip().startswith('0 pairs fight') and blend_script != '-':   # ('50 pairs fight' contains '0 pairs fight')
        sys.exit('surfaces fight - open the gaps in the Blender script first:\n' + out[-2000:])
    # (the import names its folder after the file: a file named otherwise - VELDRA-dealership.glb - is imported from a
    # copy named as the model, or the game looks in /Game/Babylon/<Name>/<name> and finds nothing)
    if Path(glb).stem.lower() != lower:
        cp = ROOT / 'Saved' / 'Import' / (name + '.glb'); cp.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(ROOT / glb, cp); glb = str(cp.relative_to(ROOT))
    # 3. into Unreal, one mesh per glTF mesh
    # (forward slashes: the editor's command line reads backslashes as escapes - Art\Trees	rees.glb became "<tab>rees.glb")
    editor_py('%s %s %s --assets --shared' % ((ROOT / 'Tools/import_babylon_model.py').as_posix(), (ROOT / glb).as_posix(), name), 'import')
    # 4. its materials usable on instanced parts
    log = editor_py('%s %s' % ((ROOT / 'Tools/fix_babylon_materials.py').as_posix(), name), 'material fix-up')
    print([l for l in log.splitlines() if 'ADP_MATFIX' in l][-1:])
    # 4b. its colour pictures as colour pictures, never taken for normal maps (the jetway's poster, 2026-11-23)
    log = editor_py('%s %s %s' % ((ROOT / 'Tools/fix_babylon_textures.py').as_posix(), (ROOT / glb).as_posix(), name), 'pictures')
    print([l for l in log.splitlines() if 'ADP_TEXFIX' in l][-1:])
    # 4c. Nanite off for its see-through glass (Nanite draws no translucent material: the fuel truck's windows were logged
    #     every frame as an invalid material on a Nanite mesh, 2026-11-24); every other part stays Nanite
    log = editor_py('%s %s' % ((ROOT / 'Tools/fix_glass_nanite.py').as_posix(), name), 'glass not Nanite')
    print([l for l in log.splitlines() if 'ADP_GLASS' in l][-1:])
    # 5. the layout, from the names the import actually gave
    print(run([sys.executable, 'Tools/glb_nodes.py', glb, lower], 'layout from the imported names').strip())
    # 6. where the editor reads it first
    shutil.copy(ROOT / 'Data' / ('babylon-%s.json' % lower), ROOT / 'Content' / 'Data' / ('babylon-%s.json' % lower))
    print('== done: %s ready to place with ABabylonModel::Place("%s")' % (name, name))


if __name__ == '__main__':
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3])
