"""Lists (and downloads) the newest swisstopo tiles for a lat/lon box: SWISSIMAGE 10 cm photos and swissALTI3D 0.5 m heights.
Open data, free use (source: swisstopo). Usage:
  python -I Tools/swiss_tiles.py list  <collection> <lon0> <lat0> <lon1> <lat1> <gsd>  -> prints count + total size
  python -I Tools/swiss_tiles.py fetch <collection> <lon0> <lat0> <lon1> <lat1> <gsd> <outdir>   (resumes; skips files already complete)
collection: ch.swisstopo.swissimage-dop10 (gsd 0.1 or 2) | ch.swisstopo.swissalti3d (gsd 0.5 or 2)
"""
import json, os, sys, urllib.request, concurrent.futures as cf

API = 'https://data.geo.admin.ch/api/stac/v0.9/collections/%s/items?bbox=%s&limit=100'


def items(coll, box):
    url = API % (coll, ','.join(str(v) for v in box)); out = []
    while url:
        d = json.load(urllib.request.urlopen(url, timeout=60))
        out += d['features']
        url = next((l['href'] for l in d.get('links', []) if l.get('rel') == 'next'), None)
    return out


def newest(coll, box, gsd):
    best = {}
    for it in items(coll, box):
        tile = it['id'].rsplit('_', 1)[1]          # e.g. 2558-1134
        year = it['properties'].get('datetime', '')[:4]
        for a in it['assets'].values():
            if float(a.get('eo:gsd') or a.get('gsd') or 0) == gsd and a['href'].endswith('.tif'):
                if tile not in best or year > best[tile][0]:
                    best[tile] = (year, a['href'])
    return best


def size(url):
    r = urllib.request.urlopen(urllib.request.Request(url, method='HEAD'), timeout=60)
    return int(r.headers['Content-Length'])


def fetch(url, outdir):
    p = os.path.join(outdir, url.rsplit('/', 1)[1])
    want = size(url)
    if os.path.exists(p) and os.path.getsize(p) == want:
        return 0
    for _ in range(4):
        try:
            urllib.request.urlretrieve(url, p + '.part')
            if os.path.getsize(p + '.part') == want:
                os.replace(p + '.part', p); return want
        except Exception as e:
            print('retry', url, e, flush=True)
    raise RuntimeError('failed ' + url)


if __name__ == '__main__':
    cmd, coll = sys.argv[1], sys.argv[2]
    box = [float(v) for v in sys.argv[3:7]]; gsd = float(sys.argv[7])
    tiles = newest(coll, box, gsd)
    urls = [u for _, u in tiles.values()]
    if cmd == 'list':
        sample = urls[:: max(1, len(urls) // 25)]
        avg = sum(size(u) for u in sample) / max(1, len(sample))
        print('%d tiles, about %.1f GB' % (len(urls), avg * len(urls) / 1e9))
    else:
        outdir = sys.argv[8]; os.makedirs(outdir, exist_ok=True)
        json.dump(tiles, open(os.path.join(outdir, 'tiles.json'), 'w'), indent=0)
        done = 0
        with cf.ThreadPoolExecutor(6) as ex:
            for i, n in enumerate(ex.map(lambda u: fetch(u, outdir), urls)):
                done += n
                if i % 25 == 0: print('%d / %d tiles, %.1f GB new' % (i + 1, len(urls), done / 1e9), flush=True)
        print('done', len(urls), flush=True)
