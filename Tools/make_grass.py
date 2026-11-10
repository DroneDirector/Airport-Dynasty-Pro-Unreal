# The grass round the airport: our own blade clumps, made here (no outside assets). Each blade a thin, tapering strip,
# bent over a little with its own lean and twist; many blades per clump, as close together as real turf.
#  - Art/Grass/SM_GrassNear.obj: 34 blades, 4 segments each (close to you: every blade's curve shows)
#  - Art/Grass/SM_GrassFar.obj:  16 blades, 2 segments each (further out, where the detail can't be seen)
#  - Art/Grass/SM_GrassTuft.obj: 22 single-segment blades over a wider clump (the whole airfield, out to the horizon)
# Units are centimetres, Y up (as OBJ files are read); UV v runs 0 at the root to 1 at the tip (the material's colour
# gradient and the wind use it); u across the blade.
#   python Tools/make_grass.py
import math, random
from pathlib import Path

D = Path(__file__).resolve().parent.parent / 'Art' / 'Grass'
D.mkdir(parents=True, exist_ok=True)

def clump(path, blades, segs, radius, seed):
    rnd = random.Random(seed)
    V, T, F = [], [], []
    for b in range(blades):
        # where the blade grows (spread over a disc), how tall and wide, which way it faces and leans
        a, r = rnd.uniform(0, 2 * math.pi), radius * math.sqrt(rnd.random())
        x0, z0 = r * math.cos(a), r * math.sin(a)
        h = rnd.uniform(7, 19)             # 7-19 cm tall (kept turf)
        w = rnd.uniform(0.45, 0.8)         # 4.5-8 mm wide at the root
        face = rnd.uniform(0, math.pi)
        lean = rnd.uniform(0.15, 0.75)     # how far it bends over (the tip's sideways reach, as part of its height)
        ld = a + rnd.uniform(-0.6, 0.6)    # leaning outwards from the clump's middle, roughly
        twist = rnd.uniform(-0.5, 0.5)
        base = len(V)
        for s in range(segs + 1):
            t = s / segs
            # the bend: sideways reach grows with the square of the height (a natural arc), the top drops a little
            off = lean * h * t * t
            y = h * t * (1 - 0.25 * lean * t * t)
            cx, cz = x0 + off * math.cos(ld), z0 + off * math.sin(ld)
            half = w * (1 - t) ** 0.8 + 0.02  # narrowing to a point
            ang = face + twist * t
            dx, dz = math.cos(ang) * half, math.sin(ang) * half
            V.append((cx - dx, y, cz - dz)); T.append((0.0, t))
            V.append((cx + dx, y, cz + dz)); T.append((1.0, t))
        for s in range(segs):
            i = base + s * 2
            F.append((i, i + 1, i + 3)); F.append((i, i + 3, i + 2))
    with open(path, 'w') as f:
        f.write('# grass clump, Airport Dynasty Pro\n')
        # written so the blade stands up along Unreal's Z once imported (the importer reads them as x, -y, z)
        for v in V: f.write('v %.3f %.3f %.3f\n' % (v[0], -v[2], v[1]))
        for t in T: f.write('vt %.4f %.4f\n' % t)
        for a, b, c in F: f.write('f %d/%d %d/%d %d/%d\n' % (a + 1, a + 1, b + 1, b + 1, c + 1, c + 1))
    print(path.name, len(V), 'vertices', len(F), 'triangles')

clump(D / 'SM_GrassNear.obj', 34, 4, 13.0, 11)
clump(D / 'SM_GrassFar.obj', 16, 2, 16.0, 23)
# the tufts that carry the grass across the whole airfield, far out (single-segment blades, a wider clump)
clump(D / 'SM_GrassTuft.obj', 22, 1, 24.0, 37)
