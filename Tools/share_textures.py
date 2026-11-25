# Shares the copies of one and the same texture: every imported model under Content/Babylon brings its own (67 copies of PaintGSE,
# 54 of Metal027 ...), and at a busy stand they alone filled another gigabyte of graphics memory. Nothing gets worse: only textures
# whose imported picture is the same to the last bit and whose settings are the same become one.
# Run after an import, before packaging (plain Python; starts the editor without a window by itself):
#   python -I Tools/share_textures.py          counts them only
#   python -I Tools/share_textures.py check    fails if there are any (no editor; Tools/release_pc.sh does it)
#   python -I Tools/share_textures.py apply    shares them, and deletes the copies nothing uses any more
import os, sys, json, struct, subprocess, traceback
from pathlib import Path

try:
    import unreal
except ImportError:
    unreal = None

WORK = Path(os.environ['TEMP']) / 'adp-texdups.json'
OUT = Path(os.environ['TEMP']) / 'adp-texdups-done.json'
EDITOR = r'C:/Program Files/Epic Games/UE_5.7/Engine/Binaries/Win64/UnrealEditor-Cmd.exe'


def outside(apply):
    root = Path(__file__).resolve().parent.parent
    tag = struct.pack('<Q', 0xD1C43B2E80A5F697)   # (the package trailer: the hash of every bulk picture in the file)
    groups = {}
    n = 0
    for p in (root / 'Content' / 'Babylon').rglob('Textures/*.uasset'):
        b = p.read_bytes()
        n += 1
        i = b.find(tag)
        if i < 0 or struct.unpack_from('<i', b, i + 24)[0] != 1:
            continue
        groups.setdefault(b[i + 28:i + 48].hex(), []).append('/Game/Babylon/' + p.relative_to(root / 'Content' / 'Babylon').as_posix()[:-7])
    groups = [sorted(v) for v in groups.values() if len(v) > 1]
    print(n, 'textures,', sum(len(v) - 1 for v in groups), 'copies of', len(groups))
    if 'check' in sys.argv:
        sys.exit('textures there more than once: run python -I Tools/share_textures.py apply' if groups else 0)
    WORK.write_text(json.dumps({'apply': apply, 'groups': groups}))
    OUT.unlink(missing_ok=True)
    subprocess.run([EDITOR, str(root / 'AirportDynastyPro.uproject'), '-ExecutePythonScript=' + Path(__file__).resolve().as_posix(), '-unattended', '-nullrhi'],
                   cwd=root, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not OUT.exists():
        sys.exit('the editor part did not finish: Saved/Logs/AirportDynastyPro.log, ADP_SHARE')
    done = json.loads(OUT.read_text())
    print(json.dumps(done['say'], indent=1))
    if not apply:
        return
    gone = mb = 0
    for t in done['free']:
        f = root / 'Content' / (t[len('/Game/'):] + '.uasset')
        if f.exists():
            os.chmod(f, 0o666)
            mb += f.stat().st_size / 1e6
            f.unlink()
            gone += 1
    print('deleted', gone, 'copies,', round(mb), 'MB')


PROPS = ('srgb', 'compression_settings', 'lod_group', 'flip_green_channel', 'address_x', 'address_y', 'filter', 'mip_gen_settings', 'never_stream',
         'max_texture_size', 'virtual_texture_streaming', 'lod_bias', 'compression_no_alpha', 'power_of_two_mode', 'adjust_brightness', 'defer_compression')


def prop(t, q):
    try:
        return str(t.get_editor_property(q))
    except Exception:
        return '-'


def path_of(t):
    return t.get_path_name().split('.')[0] if t else ''


def inside():
    eal, mel = unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    reg.search_all_assets(True)
    job = json.loads(WORK.read_text())
    apply = job['apply']
    say = {'materials': 0, 'parameters': 0, 'other referencers': {}, 'formats': {}, 'left': [], 'failed': []}
    other = say['other referencers']
    opt = unreal.AssetRegistryDependencyOptions(True, True, True, True, True)
    to = {}
    for g in job['groups']:
        by = {}
        for p in g:
            t = unreal.load_asset(p)
            if t:
                by.setdefault(tuple(prop(t, q) for q in PROPS) + (type(t).__name__,), []).append(p)
        for key, ts in by.items():
            good = [p for p in ts if not p.startswith('/Game/Babylon/Terminal/')] or ts   # (never one of the terminal's: that folder isn't packaged)
            own = next((p for p in good if any(not str(r).startswith(('/Game/Babylon/', '/Game/Chunks/')) for r in (reg.get_referencers(p, opt) or []))), good[0])   # (the one other materials use)
            f = '%s %s' % (key[1], key[0])
            say['formats'][f] = say['formats'].get(f, 0) + len(ts) - 1
            for p in ts:
                if p != own:
                    to[p] = own
    users = {}
    for p in to:
        for r in (reg.get_referencers(p, opt) or []):
            users.setdefault(str(r), set()).add(p)
    say['copies'] = len(to)
    say['users'] = len(users)
    stuck = set()
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    for k, (u, uses) in enumerate(sorted(users.items())):
        a = unreal.load_asset(u)
        kind = type(a).__name__
        if isinstance(a, unreal.MaterialInstanceConstant):
            hits = [(v.parameter_info.name, path_of(v.parameter_value)) for v in a.get_editor_property('texture_parameter_values') if path_of(v.parameter_value) in to]
            if hits:
                say['materials'] += 1
                say['parameters'] += len(hits)
                if apply:
                    for name, p in hits:
                        mel.set_material_instance_texture_parameter_value(a, name, unreal.load_asset(to[p]))
                    mel.update_material_instance(a)
                    try:
                        os.chmod(content + u[len('/Game/'):] + '.uasset', 0o666)
                    except OSError:
                        pass
                    left = {path_of(v.parameter_value) for v in a.get_editor_property('texture_parameter_values')} & set(to)
                    if left or not eal.save_loaded_asset(a, False):
                        stuck |= uses
                        say['failed'].append(u)
            else:
                stuck |= uses
                other['instance without it as a parameter'] = other.get('instance without it as a parameter', 0) + 1
        else:
            other[kind] = other.get(kind, 0) + 1
            if 'SceneImport' not in u and kind != 'PrimaryAssetLabel':   # (the chunk labels are written anew: Tools/make_chunk_labels.py)
                stuck |= uses
                if len(say['left']) < 15:
                    say['left'].append(u)
        if k % 200 == 199:
            unreal.SystemLibrary.collect_garbage()
            unreal.log('ADP_SHARE %d of %d' % (k + 1, len(users)))
    say['copies still used'] = len(stuck)
    OUT.write_text(json.dumps({'say': say, 'free': sorted(set(to) - stuck)}))
    unreal.log('ADP_SHARE DONE')


if unreal is None:
    outside(len(sys.argv) > 1 and sys.argv[1] == 'apply')
else:
    try:
        inside()
    except Exception:
        unreal.log_error('ADP_SHARE FAILED ' + traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
