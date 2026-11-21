# (the slab's underside 4 cm over a room's ceiling face, 5.40 over 5.36, is the terminal's own build everywhere: hidden, not
#  counted)
# Every room keeps exactly one floor and one ceiling (owner 2026-11-21, the terminal's room changes: "be very careful
# with ceilings and floors" - no holes, nothing floating where a room went, nothing lying on anything else).
# Samples a 1 m grid over the given areas and counts, at each point, the flat faces of the live terminal (its layout's
# pieces: every face of the STRUCTURE pieces from Saved/terminal-faces-all.json - Tools/dump_terminal_faces.py - and the
# other flat parts by their boxes) plus the annex pieces' own flat faces (Saved/annex-flat-*.json, written by their
# Blender scripts): a floor at the floor height, a ceiling at the ceiling height.
#   python Tools/check_floors.py            (the areas below; prints every point with 0 or 2+ of either)
import glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import terminal_where as TW

# (name, x0, x1, z0, z1, floor y, ceiling y or None for a room open to the roof)
AREAS = [
    ('escalator room', 0.3, 41.7, 0.3, 29.7, 0.01, None),
    ('airside corridor', 4.3, 49.7, 44.3, 51.7, 0.01, 5.36),
    ('way down to the escalators', 4.3, 15.7, 30.3, 43.7, 0.01, 5.36),
    ('service room', 16.3, 35.7, 30.3, 43.7, 0.01, 5.36),
    ('store (x 0..4)', 0.3, 3.7, 30.3, 51.7, 0.01, None),
    ("men's toilets", 36.3, 42.7, 30.3, 43.7, 0.01, 5.36),
    ("women's toilets", 43.3, 49.7, 30.3, 43.7, 0.01, 5.36),
    ('Grand Hall', 42.3, 113.7, 0.3, 29.7, 0.01, None),
    ('baggage block', 114.3, 155.7, 0.3, 39.7, 0.01, 5.36),
    ('cart lane', 114.3, 135.7, 40.3, 51.7, 0.01, 5.36),
    ('staff entrance', 136.3, 155.7, 40.3, 51.7, 0.01, 5.36),
    ('Floor 2 offices', 114.3, 155.7, 0.3, 51.7, 6.01, 11.32),
    ('Floor 2 landing (east strip)', 14.3, 15.7, 0.3, 21.7, 6.01, 11.32),
    ('Floor 2 landing', 0.3, 15.7, 22.3, 29.7, 6.01, 11.32),
    ('Floor 2 toilets + family room', 0.3, 13.7, 0.3, 21.7, 6.008, 11.36),
    ('ramp services 13', 255.7, 283.1, 68.3, 81.7, 0.01, 5.36),
    ('airside lobby (old stairwell closed)', 50.3, 89.7, 42.3, 51.7, 0.01, 5.36),     # (S-01 out, 2026-11-21)
    ('Floor 2 airside landing west', 56.3, 71.7, 42.3, 51.7, 6.01, 11.32),
]
HOLES = [(140.0, 143.0, 43.0, 51.0), (152.4, 155.8, 47.6, 51.4),   # the staff stair and the lift shaft (open on purpose)
         (118.9, 120.9, 17.8, 19.4)]                               # and the check-in collector's rise out of its tunnel


def inside(poly, x, z):
    # (a flat triangle or polygon, in the plan's x and z; its edges count as inside)
    n = len(poly); sgn = 0
    for i in range(n):
        (ax, az), (bx, bz) = poly[i], poly[(i + 1) % n]
        c = (bx - ax) * (z - az) - (bz - az) * (x - ax)
        if abs(c) < 1e-6: continue
        if sgn == 0: sgn = 1 if c > 0 else -1
        elif (c > 0) != (sgn > 0): return False
    return True


def flats():
    out = []      # (x0, x1, z0, z1, y, source)
    allf = {}
    fp = os.path.join(TW.ROOT, 'Saved', 'terminal-faces-all.json')
    if os.path.exists(fp): allf = json.load(open(fp))
    L = json.load(open(TW.D('terminal-layout.json'))); P = json.load(open(TW.D('terminal-pieces.json')))
    boxes = TW.boxes()
    # (every STRUCTURE piece in the tree - also the cut ones, which have no bounds for terminal_where's boxes)
    N = L['nodes']; live = set(); st = list(L['roots'])
    while st:
        i = st.pop(); live.add(i); st.extend(N[i].get('children', []))
    have = {i for i, nm, b in boxes}
    boxes = boxes + [(i, N[i]['name'], None) for i in live if '/STRUCTURE-' in N[i]['name'] and 'mesh' in N[i] and i not in have]
    merged = [i for i, nm, b in boxes if '/STRUCTURE-' in nm]
    M = TW.piece_to_plan(merged)
    for i, nm, b in boxes:
        if i in M and M[i][0] in allf or (i in M and P['terminal_mesh_%d' % L['nodes'][i]['mesh']][0] in allf):
            piece = P['terminal_mesh_%d' % L['nodes'][i]['mesh']][0]; m = M[i][1]
            for f in allf.get(piece) or allf.get(M[i][0]):
                pts = [(m[0][0] * q[0] + m[0][1] * q[1] + m[0][2] * q[2] + m[0][3], m[1][0] * q[0] + m[1][1] * q[1] + m[1][2] * q[2] + m[1][3], m[2][0] * q[0] + m[2][1] * q[1] + m[2][2] * q[2] + m[2][3]) for q in f[:-1]]
                ys = [p[1] for p in pts]
                if max(ys) - min(ys) < 0.005:
                    xs = [p[0] for p in pts]; zs = [p[2] for p in pts]
                    if min(max(xs) - min(xs), max(zs) - min(zs)) < 0.3: continue     # (a wall's top or foot, a trim)
                    out.append((min(xs), max(xs), min(zs), max(zs), ys[0], nm[-60:], [(p[0], p[2]) for p in pts]))
        elif b is None: print('  (no faces for %s)' % nm)
        elif b[4] - b[1] < 0.02:
            out.append((b[0], b[3], b[2], b[5], (b[1] + b[4]) / 2, nm[-60:], None))
        elif '/STRUCTURE-' in nm:
            out.append((b[0], b[3], b[2], b[5], None, nm[-60:], None))     # merged, its faces not dumped
    for f in glob.glob(os.path.join(TW.ROOT, 'Saved', 'annex-flat-*.json')):
        for (pc, x0, x1, y, z0, z1, mat) in json.load(open(f)):
            if min(x1 - x0, z1 - z0) < 0.3: continue
            out.append((x0, x1, z0, z1, y, '%s:%s' % (pc, mat), None))
    return out


if __name__ == '__main__':
    F = flats(); bad = 0
    for (name, x0, x1, z0, z1, fy, cy) in AREAS:
        nx, nz = int(x1 - x0) + 1, int(z1 - z0) + 1
        nb = 0; unknown = set()
        for a in range(nx):
            for c in range(nz):
                x = min(x1, x0 + a); z = min(z1, z0 + c)
                if any(h[0] <= x <= h[1] and h[2] <= z <= h[3] for h in HOLES): continue
                fl, ce, seen = [], [], set()
                for (fx0, fx1, fz0, fz1, y, src, poly) in F:
                    if not (fx0 - 1e-3 <= x <= fx1 + 1e-3 and fz0 - 1e-3 <= z <= fz1 + 1e-3): continue
                    if poly and not inside(poly, x, z): continue
                    if y is None: unknown.add(src); continue
                    if poly and (src, round(y, 3)) in seen: continue      # (a quad's two triangles meet on its diagonal)
                    if poly: seen.add((src, round(y, 3)))
                    if abs(y - fy) < 0.008: fl.append(src)
                    if cy is not None and cy - 0.02 <= y <= cy + 0.02: ce.append(src)
                if len(fl) != 1 or (cy is not None and len(ce) != 1):
                    nb += 1
                    if nb <= 6: print('  %s (%.1f, %.1f): floors %s ceilings %s' % (name, x, z, fl, ce if cy is not None else '-'))
        print('%-28s %4d points, %d off%s' % (name, nx * nz, nb, ('  (merged, faces unknown: %s)' % sorted(unknown)[:4]) if unknown else ''))
        bad += nb
    print('CHECK_FLOORS %s' % ('OK' if not bad else '%d points off' % bad))
