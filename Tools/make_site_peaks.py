"""The mountains round a site for the holographic map's mountain-name layer (to-do 171; owner, 2026-11-20 night).
The official names, places and surveyed heights of the summits (swissTLM3D's named points: main and alpine summits)
within 25 km of the airport. Out: Data/Sites/<site>/peaks.json
([{name, x, y (plan m), elev m}]) - copied to Content/Data at release.
  python -I Tools/make_site_peaks.py --site chablais"""
import json, math, sys
from pathlib import Path
import numpy as np
import rasterio
from rasterio.windows import from_bounds

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'Tools'))
import make_land_corridor as lc
import road_ground as rg

DEM = 'F:/ADP-Terrain/chablais/chablais_dem_4326.vrt'
# name, approximate lat, lon (refined on the height model); the main summits seen from the Rhone plain at Chablais
PEAKS = [
    ('Dents du Midi', 46.161, 6.923), ('Grammont', 46.353, 6.819), ('Tour d\'Aï', 46.370, 6.985), ('Tour de Mayen', 46.380, 6.997),
    ('Rochers de Naye', 46.432, 6.976), ('Cornettes de Bise', 46.336, 6.789), ('Le Chamossaire', 46.348, 7.053),
    ('Dent de Morcles', 46.187, 7.065), ('Grand Muveran', 46.237, 7.127), ('Les Diablerets', 46.304, 7.190),
    ('Dent de Jaman', 46.444, 6.976), ('Pointe de Bellevue', 46.272, 6.880), ('Le Linleu', 46.308, 6.868),
    ('Tour de Don', 46.317, 6.854), ('Le Signal de Bougy', 46.462, 6.835),
]


def main():
    site = sys.argv[sys.argv.index('--site') + 1] if '--site' in sys.argv else 'chablais'
    lc.SITE = site
    F, H, O, E, N, U = lc.georef(); SD = lc.site_dir()
    # the official names: swissTLM3D's named points (main and alpine summits, with their surveyed height), within 25 km
    import shapefile
    from pyproj import Transformer
    to_ll = Transformer.from_crs(2056, 4326, always_xy=True); to_lv = Transformer.from_crs(4326, 2056, always_xy=True)
    ap = json.loads((SD / 'site.json').read_text(encoding='utf-8'))['airport']
    ce, cn = to_lv.transform(ap['lon'], ap['lat'])
    r = shapefile.Reader('F:/ADP-Terrain/chablais/tlm/TLM_NAMEN/swissTLM3D_TLM_NAME_PKT.shp')
    cand = []
    for sh, rec in zip(r.iterShapes(), r.iterRecords()):
        if rec['OBJEKTART'] not in ('Hauptgipfel', 'Alpiner Gipfel', 'Gipfel'): continue
        x, y = sh.points[0][:2]; d = math.hypot(x - ce, y - cn)
        if d > 25000 or not rec['NAME'] or (rec['HOEHE'] or 0) < 1400: continue
        cand.append((rec['OBJEKTART'] != 'Gipfel', float(rec['HOEHE']), rec['NAME'], x, y, d))
    # the main and alpine summits first, then the highest; at most 45 so the layer stays readable
    cand.sort(key=lambda c: (not c[0], -c[1]))
    # spread over the whole view: a summit only if none already chosen stands within 2.5 km (the massifs give one name
    # each, the near mountains - Grammont, Tour d'Aï, Rochers de Naye - get theirs)
    chosen = []
    for c in cand:
        if all(math.hypot(c[3] - o[3], c[4] - o[4]) > 2500 for o in chosen): chosen.append(c)
        if len(chosen) >= 40: break
    out = []
    for main_, elev, name, x, y, dist in chosen:
        name = name.split(' | ')[0]   # (the first of its official names in more languages)
        plon, plat = to_ll.transform(x, y)
        hh = elev + 50.0
        lor, lar = math.radians(plon), math.radians(plat)
        nn = rg.A_ / math.sqrt(1 - rg.E2 * math.sin(lar) ** 2)
        P = np.array([(nn + hh) * math.cos(lar) * math.cos(lor), (nn + hh) * math.cos(lar) * math.sin(lor), (nn * (1 - rg.E2) + hh) * math.sin(lar)]) - O
        e, n = P @ E, P @ N
        X = e * math.sin(H) + n * math.cos(H); Y = -e * math.cos(H) + n * math.sin(H)
        out.append({'name': name, 'x': round(float(X), 1), 'y': round(float(Y), 1), 'elev': round(elev), 'lat': round(plat, 5), 'lon': round(plon, 5), 'main': bool(main_)})
        print(f'{name:28s} {elev:6.0f} m  {dist / 1000:5.1f} km')
    p = SD / 'peaks.json'
    json.dump({'note': 'Tools/make_site_peaks.py: swissTLM3D named summits within 25 km, spread 2.5 km apart, plan metres', 'peaks': out}, open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    c = ROOT / 'Content' / 'Data' / 'Sites' / site / 'peaks.json'; c.parent.mkdir(parents=True, exist_ok=True); c.write_text(p.read_text(encoding='utf-8'), encoding='utf-8')
    print(len(out), 'peaks ->', p)


if __name__ == '__main__':
    main()
