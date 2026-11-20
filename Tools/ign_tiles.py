"""The French side of a map area from IGN's open Geoplateforme (Licence Ouverte / Etalab 2.0, source: IGN):
BD ORTHO 20 cm photos (500 m JPEG tiles, 2500 px) and RGE ALTI 1 m heights (1 km float32 BIL tiles, 1000 px), in Lambert-93.
Usage: python -I Tools/ign_tiles.py <x0> <y0> <x1> <y1> <outdir>    (Lambert-93 metres; resumes)"""
import os, sys, urllib.request, concurrent.futures as cf

WMS = ('https://data.geopf.fr/wms-r/wms?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&LAYERS=%s&STYLES=&CRS=EPSG:2154'
       '&BBOX=%d,%d,%d,%d&WIDTH=%d&HEIGHT=%d&FORMAT=%s')


def get(url, path, minsize):
    if os.path.exists(path) and os.path.getsize(path) >= minsize:
        return 0
    for _ in range(5):
        try:
            data = urllib.request.urlopen(url, timeout=120).read()
            if len(data) >= minsize and not data[:5].startswith(b'<?xml'):
                open(path, 'wb').write(data); return len(data)
        except Exception as e:
            print('retry', path, e, flush=True)
    raise RuntimeError('failed ' + url)


if __name__ == '__main__':
    x0, y0, x1, y1 = (int(v) for v in sys.argv[1:5]); out = sys.argv[5]
    os.makedirs(out + '/photo-20cm', exist_ok=True); os.makedirs(out + '/alti-1m', exist_ok=True)
    jobs = []
    for x in range(x0, x1, 1000):
        for y in range(y0, y1, 1000):
            jobs.append((WMS % ('ELEVATION.ELEVATIONGRIDCOVERAGE.HIGHRES', x, y, x + 1000, y + 1000, 1000, 1000, 'image/x-bil;bits=32'),
                         '%s/alti-1m/%d_%d.bil' % (out, x, y), 4000000))
            for dx in (0, 500):
                for dy in (0, 500):
                    jobs.append((WMS % ('ORTHOIMAGERY.ORTHOPHOTOS', x + dx, y + dy, x + dx + 500, y + dy + 500, 2500, 2500, 'image/jpeg'),
                                 '%s/photo-20cm/%d_%d.jpg' % (out, x + dx, y + dy), 1000))
    n = 0
    with cf.ThreadPoolExecutor(6) as ex:
        for i, s in enumerate(ex.map(lambda j: get(*j), jobs)):
            n += s
            if i % 200 == 0: print('%d / %d, %.2f GB' % (i + 1, len(jobs), n / 1e9), flush=True)
    print('done', len(jobs), flush=True)
