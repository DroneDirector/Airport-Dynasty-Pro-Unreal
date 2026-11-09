# The update package from one PC release to the next: only the files that differ (by their SHA-256 fingerprint) and the
# list of files to remove, with update.json for the game's updater (Source/.../UI/Updater.cpp). Unreal's own patch pak
# (packaged with -generatepatch against the base release) holds only the content that changed, so the package stays
# small; the game's program comes along only when its code changed. The terrain is left out (its region packs update
# on their own).
#   python Tools/MakeUpdate.py --old F:/ADP-Packaged/0.12.1/Windows --new F:/ADP-Packaged/0.13.0/Windows --from 0.12.1 --to 0.13.0
import argparse
import hashlib
import json
import os
import zipfile
from pathlib import Path

SKIP = ('Saved/', 'Manifest_', 'AirportDynastyPro/Content/Terrain/')

def tree(root: Path):
    out = {}
    for p in root.rglob('*'):
        if not p.is_file(): continue
        rel = p.relative_to(root).as_posix()
        if any(rel.startswith(s) or s in rel for s in SKIP): continue
        h = hashlib.sha256()
        with open(p, 'rb') as f:
            for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
        out[rel] = (h.hexdigest(), p.stat().st_size)
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--old', required=True); ap.add_argument('--new', required=True)
    ap.add_argument('--from', dest='frm', required=True); ap.add_argument('--to', required=True)
    ap.add_argument('--out', default='F:/ADP-Packaged')
    a = ap.parse_args()
    old, new = tree(Path(a.old)), tree(Path(a.new))
    changed = [p for p, (h, _) in new.items() if old.get(p, (None,))[0] != h]
    deleted = [p for p in old if p not in new]
    zpath = Path(a.out) / f'AirportDynastyPro-Unreal-PC-Update-{a.frm}-to-{a.to}.zip'
    with zipfile.ZipFile(zpath, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        z.writestr('update.json', json.dumps({'from': a.frm, 'to': a.to, 'files': [{'path': p.replace('/', '\\'), 'sha256': new[p][0], 'size': new[p][1]} for p in changed], 'deleted': [p.replace('/', '\\') for p in deleted]}, indent=1))
        for p in changed: z.write(Path(a.new) / p, 'files/' + p)
    print('%s: %d changed, %d removed, %.1f MB' % (zpath.name, len(changed), len(deleted), zpath.stat().st_size / 1048576))

if __name__ == '__main__':
    main()
