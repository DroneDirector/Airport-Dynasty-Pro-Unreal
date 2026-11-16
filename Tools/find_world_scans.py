# Every place where a frame-by-frame update (Tick) walks through all objects of a kind (TActorIterator, TObjectIterator,
# GetAllActorsOfClass, GetActorOfClass), checked against Data/world-scans-checked.json (owner, 2026-11-16: catch a
# heavy scan early - ASeats::Tick gathering every building's seats every 15 s was the game's stutter). A new one stops
# the release until it is looked at: it needs ADP_PERF_SCOPE (PerfLog.h) and to run only when something changed or on
# a slow timer, then its name goes into the list.
#   python Tools/find_world_scans.py          (exit 1 if there is a new one)
#   python Tools/find_world_scans.py --accept (after checking: write the current list)
import re, glob, json, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIST = os.path.join(ROOT, 'Data', 'world-scans-checked.json')
found = {}
for f in sorted(glob.glob(os.path.join(ROOT, 'Source', 'AirportDynastyPro', 'Private', '**', '*.cpp'), recursive=True)):
    s = open(f, encoding='utf-8', errors='ignore').read()
    for m in re.finditer(r'\n(?:void|bool) ([A-Z][A-Za-z0-9]+)::(Tick|TickComponent)\([^)]*\)\s*\n\{', s):
        i = j = m.end(); depth = 1
        while depth and j < len(s):
            depth += {'{': 1, '}': -1}.get(s[j], 0); j += 1
        body = s[i:j]
        hits = sorted(set(re.findall(r'(TActorIterator<\w+>|TObjectIterator<\w+>|GetAllActorsOfClass|GetActorOfClass)', body)))
        if hits:
            found['%s::%s' % (m.group(1), m.group(2))] = {'file': os.path.relpath(f, ROOT).replace(os.sep, '/'), 'scans': hits, 'guarded': 'ADP_PERF_SCOPE' in body}
if '--accept' in sys.argv:
    json.dump(found, open(LIST, 'w'), indent=1); print('accepted %d' % len(found)); sys.exit(0)
known = json.load(open(LIST)) if os.path.exists(LIST) else {}
bad = []
for k, v in found.items():
    if not v['guarded']: bad.append('%s (%s) walks %s with no time guard (ADP_PERF_SCOPE)' % (k, v['file'], ', '.join(v['scans'])))
    elif k not in known or set(v['scans']) - set(known[k]['scans']): bad.append('%s (%s) walks %s: new - check it, then --accept' % (k, v['file'], ', '.join(v['scans'])))
if bad: print('\n'.join(bad)); sys.exit(1)
print('all %d frame-by-frame scans checked and guarded' % len(found))
