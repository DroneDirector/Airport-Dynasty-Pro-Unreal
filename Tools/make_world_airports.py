# Every airport in the world for the simulation (docs/AI-Simulation-Plan.md part 2): OurAirports' public-domain airport
# and runway lists (Art/Reference/OurAirports-<date>/) trimmed to what the game uses and written compact to
# Data/world-airports.json - shipped inside the game, read by Sim/ADPWorldData (never downloaded while playing).
#   python Tools/make_world_airports.py [Art/Reference/OurAirports-2026-11-20]
# Kept: every open airport, seaplane base and heliport. Per airport: ident, ICAO, IATA, name, town, country, kind
# (L large, M medium, S small, H heliport, W seaplane base), lat, lon, elevation (ft), scheduled service (0/1).
# Per runway (open ones): both end names, length and width (ft), surface (P paved, G grass, R gravel/dirt, W water,
# O other), lit (0/1), both ends' lat/lon where known, heading of the low end (deg true).
import csv, json, sys, glob
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(sorted(glob.glob(str(ROOT / 'Art' / 'Reference' / 'OurAirports-*')))[-1])
KIND = {'large_airport': 'L', 'medium_airport': 'M', 'small_airport': 'S', 'heliport': 'H', 'seaplane_base': 'W'}

def num(v, nd=None):
    try:
        f = float(v)
        return round(f, nd) if nd is not None else f
    except (TypeError, ValueError):
        return None

def surface(s):
    s = (s or '').upper()
    if any(k in s for k in ('ASP', 'CON', 'BIT', 'PEM', 'PAV', 'TAR', 'MAC', 'ASPH')): return 'P'
    if any(k in s for k in ('GRS', 'GRASS', 'TURF')): return 'G'
    if any(k in s for k in ('GRV', 'GRAV', 'DIRT', 'SAN', 'CLA', 'SOIL', 'COR', 'LAT', 'EARTH')): return 'R'
    if 'WAT' in s: return 'W'
    return 'O'

airports, index = [], {}
with open(SRC / 'airports.csv', encoding='utf-8') as f:
    for r in csv.DictReader(f):
        k = KIND.get(r['type'])
        if not k: continue   # (closed and balloonports out)
        lat, lon = num(r['latitude_deg'], 5), num(r['longitude_deg'], 5)
        if lat is None or lon is None: continue
        index[r['ident']] = len(airports)
        airports.append([r['ident'], r['icao_code'] or '', r['iata_code'] or '', r['name'], r['municipality'] or '', r['iso_country'],
                         k, lat, lon, int(num(r['elevation_ft']) or 0), 1 if r['scheduled_service'] == 'yes' else 0, []])
nrw = 0
with open(SRC / 'runways.csv', encoding='utf-8') as f:
    for r in csv.DictReader(f):
        i = index.get(r['airport_ident'])
        if i is None or r['closed'] == '1': continue
        airports[i][11].append([r['le_ident'], r['he_ident'], int(num(r['length_ft']) or 0), int(num(r['width_ft']) or 0), surface(r['surface']),
                                1 if r['lighted'] == '1' else 0, num(r['le_latitude_deg'], 5), num(r['le_longitude_deg'], 5),
                                num(r['he_latitude_deg'], 5), num(r['he_longitude_deg'], 5), num(r['le_heading_degT'], 1)])
        nrw += 1
out = ROOT / 'Data' / 'world-airports.json'
doc = {'source': 'OurAirports (public domain) - ' + SRC.name + ', trimmed by Tools/make_world_airports.py',
       'airport': ['ident', 'icao', 'iata', 'name', 'town', 'country', 'kind', 'lat', 'lon', 'elevFt', 'scheduled', 'runways'],
       'runway': ['le', 'he', 'lengthFt', 'widthFt', 'surface', 'lit', 'leLat', 'leLon', 'heLat', 'heLon', 'leHdg'],
       'airports': airports}
out.write_text(json.dumps(doc, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
kinds = {}
for a in airports: kinds[a[6]] = kinds.get(a[6], 0) + 1
print('%s: %d airports (%s), %d runways, %.1f MB' % (out, len(airports), ', '.join('%s %d' % kv for kv in sorted(kinds.items())), nrw, out.stat().st_size / 1e6))
