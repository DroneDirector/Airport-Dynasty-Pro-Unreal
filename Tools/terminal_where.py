# Where every piece of the terminal is, in the terminal plan's own metres (x east along the building, z north to the
# airside, y up - the same as Data/terminal-plan.json): each layout node with a mesh (Data/terminal-layout.json), its
# piece and offset (terminal-pieces.json), the piece's size (terminal-piece-bounds.json, Tools/terminal_piece_bounds.py).
#   python Tools/terminal_where.py x0 x1 z0 z1 [y0 y1]     -> the pieces whose box lies in (or crosses) that box
#   (as a module: boxes() -> [(node index, path, (x0, y0, z0, x1, y1, z1))])
import json, math, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = lambda n: os.path.join(ROOT, 'Data', n)


def qmul(a, b):
    ax, ay, az, aw = a; bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qrot(q, v):
    x, y, z, w = q; vx, vy, vz = v
    tx, ty, tz = 2 * (y * vz - z * vy), 2 * (z * vx - x * vz), 2 * (x * vy - y * vx)      # t = 2 q x v
    return (vx + w * tx + (y * tz - z * ty), vy + w * ty + (z * tx - x * tz), vz + w * tz + (x * ty - y * tx))


def boxes():
    L = json.load(open(D('terminal-layout.json'))); N = L['nodes']
    P = json.load(open(D('terminal-pieces.json'))); B = json.load(open(D('terminal-piece-bounds.json')))
    out = []
    def walk(i, T, path):
        n = N[i]
        t = n.get('translation', [0, 0, 0]); r = n.get('rotation', [0, 0, 0, 1]); s = n.get('scale', [1, 1, 1])
        PT, PR, PS = T
        # world = parent(T R S) * local(T R S), kept as composed affine via point mapping
        def apply(v):
            v = (v[0] * s[0], v[1] * s[1], v[2] * s[2]); v = qrot(r, v); v = (v[0] + t[0], v[1] + t[1], v[2] + t[2])
            return PT(v)
        nm = path + '/' + n['name']
        if 'mesh' in n:
            k = 'terminal_mesh_%d' % n['mesh']
            if k in P and P[k][0] in B:
                bb = B[P[k][0]]; off = P[k][1:]
                lo = [bb[0] + off[0], bb[1] + off[1], bb[2] + off[2]]; hi = [bb[3] + off[0], bb[4] + off[1], bb[5] + off[2]]
                pts = []
                for cx in (lo[0], hi[0]):
                    for cy in (lo[1], hi[1]):
                        for cz in (lo[2], hi[2]):
                            g = (cx / 100, cz / 100, cy / 100)          # the mesh's Unreal axes back to the glTF node's
                            w = apply(g); pts.append((-w[0], w[1], w[2]))   # glTF -> the plan: x east = -x
                xs, ys, zs = zip(*pts)
                out.append((i, nm, (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))))
        for c in n.get('children', []): walk(c, (apply, None, None), nm)
    for r in L['roots']: walk(r, (lambda v: v, None, None), '')
    return out


if __name__ == '__main__':
    a = [float(x) for x in sys.argv[1:]]
    x0, x1, z0, z1 = a[:4]; y0, y1 = (a[4], a[5]) if len(a) > 5 else (-100, 100)
    for i, nm, (bx0, by0, bz0, bx1, by1, bz1) in boxes():
        if bx1 >= x0 and bx0 <= x1 and bz1 >= z0 and bz0 <= z1 and by1 >= y0 and by0 <= y1:
            print('%6d  x %7.2f..%7.2f  y %6.2f..%6.2f  z %6.2f..%6.2f  %s' % (i, bx0, bx1, by0, by1, bz0, bz1, nm[-70:]))


def mat_trs(t, r, s):
    x, y, z, w = r
    R = [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
         [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
         [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]]
    return [[R[i][0] * s[0], R[i][1] * s[1], R[i][2] * s[2], t[i]] for i in range(3)]


def mat_mul(A, B):
    return [[sum(A[i][k] * B[k][j] for k in range(3)) + (A[i][3] if j == 3 else 0) for j in range(4)] for i in range(3)]


def piece_to_plan(indices):
    """For each node index: (piece name, 3x4 matrix taking the piece's own Unreal cm to the plan's metres)."""
    L = json.load(open(D('terminal-layout.json'))); N = L['nodes']
    P = json.load(open(D('terminal-pieces.json')))
    parent = {}
    for i, n in enumerate(N):
        for c in n.get('children', []): parent[c] = i
    out = {}
    for i in indices:
        M = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0]]
        k = i
        chain = []
        while k is not None: chain.append(k); k = parent.get(k)
        for k in reversed(chain):
            n = N[k]; M = mat_mul(M, mat_trs(n.get('translation', [0, 0, 0]), n.get('rotation', [0, 0, 0, 1]), n.get('scale', [1, 1, 1])))
        piece, ox, oy, oz = P['terminal_mesh_%d' % N[i]['mesh']]
        # piece cm (Unreal axes) + offset -> the glTF node's metres (x, z, y swapped back) -> world glTF -> plan (x east = -x)
        G = [[0.01, 0, 0, ox * 0.01], [0, 0, 0.01, oz * 0.01], [0, 0.01, 0, oy * 0.01]]
        W = mat_mul(M, G)
        out[i] = (piece, [[-W[0][j] for j in range(4)], W[1], W[2]])
    return out
