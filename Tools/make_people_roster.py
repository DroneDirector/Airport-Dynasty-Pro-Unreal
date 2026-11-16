# The built MetaHumans' roster for the game (owner, 2026-11-16: talking to people - who is a man or a woman, how old,
# what they do): Content/People/Built/<MH_nnn_role> -> Data/people-roster.json [{name, i, role, female, age, teen}],
# worked out by the very same spec as Tools/make_metahumans.py (run without Unreal: its 'unreal' import is stubbed).
#   python Tools/make_people_roster.py
import os, sys, json, types, re
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
class _Any:
    def __getattr__(self, k): return _Any()
    def __call__(self, *a, **k): return ROOT + '/Saved'
    def __fspath__(self): return ROOT + '/Saved'
sys.modules['unreal'] = _Any()
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import make_metahumans as mm
out = []
for name in sorted(os.listdir(os.path.join(ROOT, 'Content', 'People', 'Built'))):
    m = re.match(r'^MH_(\d+)_([a-z]+)$', name)
    if not m: continue
    p = mm.person(int(m.group(1)))
    out.append({'name': name, 'i': p['i'], 'role': p['role'], 'female': bool(p['female']), 'age': int(p['age']), 'teen': bool(p.get('teen', False))})
for d in ('Data', os.path.join('Content', 'Data')):
    json.dump(out, open(os.path.join(ROOT, d, 'people-roster.json'), 'w'), indent=1)
print('roster:', len(out), 'people;', sum(o['female'] for o in out), 'women;', {r: sum(1 for o in out if o['role'] == r) for r in sorted(set(o['role'] for o in out))})
