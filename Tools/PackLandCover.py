# The land cover tiles (Tools/MakeLandCover.py) as two more packs next to the terrain's (Tools/PackTerrain.py), added
# to the same manifest.json, so the game's downloader fetches them with the terrain (and an installed game only them).
# They unpack to <terrain folder>/landcover/<world|region>/... Prints a line every 500 MB packed.
#   python Tools/PackLandCover.py [--tiles F:/ADP-Terrain/landcover] [--packs F:/ADP-Terrain/packs-v2]
import argparse, hashlib, json, time, zipfile
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument('--tiles', default='F:/ADP-Terrain/landcover')
ap.add_argument('--packs', default='F:/ADP-Terrain/packs-v2')
a = ap.parse_args()
T, P = Path(a.tiles), Path(a.packs)
STEP = 500 * 1024 * 1024; done = 0; mark = STEP; t0 = time.time()
entries = []
for kind, name, region in (('world', 'landcover-world.zip', None), ('region', 'landcover-north-atlantic.zip', [-28.125, 56.25, -5.625, 67.5])):
    path = P / name; n = 0
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_STORED) as z:
        for f in sorted((T / kind).rglob('*.jpg')):
            rel = f.relative_to(T).as_posix()
            z.write(f, 'landcover/' + rel); n += 1
            done += f.stat().st_size
            while done >= mark:
                print('%5.2f GB packed  (%.0f s)  pack %s  file %s' % (done / 1e9, time.time() - t0, name, rel), flush=True); mark += STEP
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b''): h.update(chunk)
    e = {'name': name, 'size': path.stat().st_size, 'sha256': h.hexdigest(), 'tiles': n}
    if region: e['region'] = region
    entries.append(e)
    print(name, n, 'tiles', '%.2f GB' % (e['size'] / 1e9), flush=True)
m = json.loads((P / 'manifest.json').read_text())
names = {e['name'] for e in entries}
m['packs'] = [p for p in m['packs'] if p['name'] not in names] + entries
m['total'] = sum(p['size'] for p in m['packs'])
(P / 'manifest.json').write_text(json.dumps(m, indent=1))
print('done: manifest %d packs, %.2f GB' % (len(m['packs']), m['total'] / 1e9), flush=True)
