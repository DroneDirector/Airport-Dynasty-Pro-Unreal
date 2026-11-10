# The shipped terrain (Tools/MakeTerrain.py's tiles) in region packs for the game's downloader and updater: a base pack
# (layer.json and levels 0-2: the whole globe, coarse) and one pack per level-3 tile of the geographic TMS layout
# (16 x 8 regions of 22.5 degrees), holding that region's tiles from level 3 down. The tiles are already gzipped, so
# the packs only store them. manifest.json lists every pack with its size, its SHA-256 and its region, so an update
# downloads only the packs that changed.
# Prints a line every 500 MB packed (with the pack and the file being worked on).
#   python Tools/PackTerrain.py [--tiles F:/ADP-Terrain/tiles] [--out F:/ADP-Terrain/packs] [--version 1]
import argparse, hashlib, json, os, sys, time, zipfile
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument('--tiles', default='F:/ADP-Terrain/tiles')
ap.add_argument('--out', default='F:/ADP-Terrain/packs')
ap.add_argument('--version', default='1')
a = ap.parse_args()
T, O = Path(a.tiles), Path(a.out)
O.mkdir(parents=True, exist_ok=True)

STEP = 500 * 1024 * 1024
done = 0; next_mark = STEP; t0 = time.time()
def note(pack, rel, size):
    global done, next_mark
    done += size
    if done >= next_mark:
        print('%5.1f GB packed  (%.0f s)  pack %s  file %s' % (done / 1e9, time.time() - t0, pack, rel), flush=True)
        next_mark += STEP

# the region of a tile: its level-3 ancestor
groups = {}
if __name__ != '__main__': levels = []
levels = sorted(int(p.name) for p in T.iterdir() if p.is_dir() and p.name.isdigit()) if __name__ == '__main__' else []
for z in levels:
    for xd in (T / str(z)).iterdir():
        x = int(xd.name)
        for f in xd.iterdir():
            if not f.name.endswith('.terrain'): continue
            y = int(f.name.split('.')[0])
            key = 'base' if z < 3 else '%02d-%02d' % (x >> (z - 3), y >> (z - 3))
            groups.setdefault(key, []).append((z, x, y, f))
if __name__ == '__main__': print('tiles found in', len(groups), 'packs:', sum(len(v) for v in groups.values()), flush=True)

def make_pack(job):
    """One pack (run in its own process): its tiles stored, its checksum taken."""
    key, files, tiles, out = job
    name = 'terrain-%s.zip' % key
    path = Path(out) / name
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_STORED) as z:
        if key == 'base': z.write(Path(tiles) / 'layer.json', 'layer.json')
        for (lz, x, y, f) in files: z.write(f, '%d/%d/%d.terrain' % (lz, x, y))
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b''): h.update(chunk)
    entry = {'name': name, 'size': path.stat().st_size, 'sha256': h.hexdigest(), 'tiles': len(files)}
    if key != 'base':
        rx, ry = map(int, key.split('-'))
        entry['region'] = [-180 + rx * 22.5, -90 + ry * 22.5, -180 + (rx + 1) * 22.5, -90 + (ry + 1) * 22.5]   # lon0, lat0, lon1, lat1
    last = files[-1]
    return entry, sum(f.stat().st_size for (_, _, _, f) in files), '%d/%d/%d.terrain' % last[:3]

if __name__ == '__main__':
    from concurrent.futures import ProcessPoolExecutor, as_completed
    packs = []
    jobs = [(k, sorted(groups[k]), str(T), str(O)) for k in sorted(groups, key=lambda k: (k != 'base', k))]
    with ProcessPoolExecutor(max_workers=8) as ex:
        for fut in as_completed([ex.submit(make_pack, j) for j in jobs]):
            entry, size, last = fut.result()
            packs.append(entry)
            note(entry['name'], last, size)
    packs.sort(key=lambda p: (p['name'] != 'terrain-base.zip', p['name']))
    manifest = {'version': a.version, 'total': sum(p['size'] for p in packs), 'packs': packs}
    (O / 'manifest.json').write_text(json.dumps(manifest, indent=1))
    print('done: %d packs, %.2f GB, %.0f s' % (len(packs), manifest['total'] / 1e9, time.time() - t0), flush=True)
