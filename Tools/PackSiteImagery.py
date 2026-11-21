"""A site's aerial photo tiles (Tools/make_site_imagery.py) in ~1 GB download packs (to-do 164; owner 2026-11-20: the
Swiss photos shipped as packs like the terrain, never streamed). Each pack is a zip (stored: the tiles are JPEG already)
whose paths start Sites/<site>/imagery/..., so the game's pack installer (TerrainPacks.cpp) puts them where the globe
looks (ADPTerrain::Dir()/Sites/<site>/imagery). The coarse levels come first (pack 000: tiles.json and levels 0-14 - the
whole area at once, sharper packs follow). manifest-<site>.json lists every pack with its size and SHA-256, so the
downloader resumes and an update fetches only changed packs.
  python -I Tools/PackSiteImagery.py --site chablais --src F:/ADP-Terrain/chablais/imagery --out F:/ADP-Terrain/packs-chablais"""
import argparse, hashlib, json, os, time, zipfile
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument('--site', default='chablais'); ap.add_argument('--src', default='F:/ADP-Terrain/chablais/imagery')
ap.add_argument('--out', default='F:/ADP-Terrain/packs-chablais'); ap.add_argument('--gb', type=float, default=1.0)
a = ap.parse_args()
S, O = Path(a.src), Path(a.out); O.mkdir(parents=True, exist_ok=True)
LIMIT = int(a.gb * 1024 ** 3)
prefix = 'Sites/%s/imagery/' % a.site
t0 = time.time()
# every file, coarse levels first (level folders are numbers), then by x / y folder order
files = []
for root, dirs, fs in os.walk(S):
    for f in fs:
        p = Path(root) / f; rel = p.relative_to(S).as_posix()
        lvl = int(rel.split('/')[0]) if rel.split('/')[0].isdigit() else -1
        files.append((lvl, rel, p))
files.sort(key=lambda x: (x[0], x[1]))
print(len(files), 'files', flush=True)
packs = []; n = 0; z = None; size = 0; name = ''
def close():
    global z
    if not z: return
    z.close(); h = hashlib.sha256()
    with open(O / name, 'rb') as fh:
        for chunk in iter(lambda: fh.read(1 << 24), b''): h.update(chunk)
    packs.append({'name': name, 'size': (O / name).stat().st_size, 'sha256': h.hexdigest()})
    print('%s  %.2f GB  (%.0f s)' % (name, packs[-1]['size'] / 1e9, time.time() - t0), flush=True); z = None
for lvl, rel, p in files:
    fsz = p.stat().st_size
    # (a new pack at the limit, and always after the coarse levels: the first pack shows the whole area at once)
    if z is None or size + fsz > LIMIT or (lvl == 15 and name.endswith('-000.zip') and size > 0):
        close(); name = '%s-imagery-%03d.zip' % (a.site, n); n += 1; size = 0
        if (O / name).exists(): (O / name).unlink()
        z = zipfile.ZipFile(O / name, 'w', zipfile.ZIP_STORED)
    z.write(p, prefix + rel); size += fsz
close()
json.dump({'site': a.site, 'kind': 'imagery', 'packs': packs, 'note': 'Tools/PackSiteImagery.py'}, open(O / ('manifest-%s.json' % a.site), 'w'), indent=1)
print('done: %d packs, %.1f GB' % (len(packs), sum(p['size'] for p in packs) / 1e9), flush=True)
