# Joint pieces for the hangar equipment, batch 2 (to-do 364 A), on top of hq_detail.py: triangular gussets welded into
# a corner, clevis pins with a head and a split pin, end caps on tube ends, foot plates with anchor bolts.
from hq_detail import *


def gusset(g, p, u, v, a, t, m, weld=True):
    """A triangular gusset plate in the corner at p, legs a long along u and v, t thick (welded both legs)."""
    p, u, v = Vector(p), Vector(u).normalized(), Vector(v).normalized(); n = u.cross(v).normalized() * (t / 2)
    o = bmesh.new()
    A = [o.verts.new(q) for q in (p - n, p + u * a - n, p + v * a - n)]
    Bv = [o.verts.new(q) for q in (p + n, p + u * a + n, p + v * a + n)]
    o.faces.new(A); o.faces.new(list(reversed(Bv)))
    for i in range(3): o.faces.new((A[i], A[(i + 1) % 3], Bv[(i + 1) % 3], Bv[i]))
    g._take(o, MATS[m])
    if weld:
        for s in (-1, 1):
            g.weld(p + n * s * 1.6 + (u + v) * 0.0025, p + u * (a - 0.006) + n * s * 1.6 + v * 0.0025, m, 0.003)
            g.weld(p + n * s * 1.6 + (u + v) * 0.0025, p + v * (a - 0.006) + n * s * 1.6 + u * 0.0025, m, 0.003)


def pin(g, c, axis, r, L, m='CR'):
    """A clevis pin centred at c along axis, L long: a head collar at one end, a split pin through the other."""
    c, z = Vector(c), Vector(axis).normalized()
    g.tube(c - z * L / 2, c + z * L / 2, r, m, 12, weld=False)
    g.lz([(r * 1.7, 0.0), (r * 1.7, r * 0.8), (r * 1.4, r * 1.1), (0.0, r * 1.1)], m, frame(c + z * L / 2 - z * 0.0004, z), 14)
    x = z.orthogonal().normalized()
    g.tube(c - z * (L / 2 - r * 0.7) - x * r * 1.9, c - z * (L / 2 - r * 0.7) + x * r * 1.9, max(0.0018, r * 0.18), 'ST', 6, weld=False)


def cap(g, p, n, r, m):
    """A pressed end cap on a round tube end at p (tube runs along -n): a domed plug a hair wider than the tube."""
    g.lz([(r + 0.0012, -0.006), (r + 0.0012, 0.002), (r * 0.8, 0.005), (0.0, 0.006)], m, frame(p, n), 16)


def foot(g, x, y, z, w, m, bolts=2, d=0.009):
    """A square foot plate (w x w, 8 mm) under a post at (x, y), its top at z, with anchor bolts into the floor."""
    g.box((x - w / 2, y - w / 2, z - 0.008), (x + w / 2, y + w / 2, z), m, 0.002)
    for k in range(bolts):
        a = math.pi / 4 + 2 * math.pi * k / bolts
        g.bolt((x + w * 0.36 * math.cos(a), y + w * 0.36 * math.sin(a), z), (0, 0, 1), d)
