# Escape distances on the terminal's floors (VKF 16-15, 2.3: in a room the straight line, round the walls between rooms):
# the real walls (Data/holomap-walls.json, traced from the game), the walkable floors, the way out round the walls from every
# point to the nearest exit, on a 0.4 m grid with 32 directions (worst error about +1.3 % on the safe side).
import json, math, os
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import dijkstra
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
WALLS = json.load(open(os.path.join(ROOT, 'Data', 'holomap-walls.json')))['floors']
PLAN = json.load(open(os.path.join(ROOT, 'Data', 'terminal-plan.json')))
S = 0.4
DIRS = [(1, 0), (1, 1), (2, 1), (1, 2), (3, 1), (1, 3), (3, 2), (2, 3)]


class Field:
    def __init__(self, x0, x1, z0, z1):
        self.x0, self.z0 = x0, z0; self.nx, self.nz = int(math.ceil((x1 - x0) / S)), int(math.ceil((z1 - z0) / S))
        self.free = np.zeros((self.nx, self.nz), bool)

    def ix(self, x): return int(math.floor((x - self.x0) / S))
    def iz(self, z): return int(math.floor((z - self.z0) / S))

    def fill(self, b, v=True):
        a, c = max(0, self.ix(b[0] + 0.01)), max(0, self.iz(b[1] + 0.01)); e, f = self.ix(b[2] - 0.01) + 1, self.iz(b[3] - 0.01) + 1
        self.free[a:max(a, e), c:max(c, f)] = v

    def line(self, xa, za, xb, zb, v=False):
        n = int(math.hypot(xb - xa, zb - za) / (S * 0.4)) + 2
        for k in range(n + 1):
            i, j = self.ix(xa + (xb - xa) * k / n), self.iz(za + (zb - za) * k / n)
            if 0 <= i < self.nx and 0 <= j < self.nz: self.free[i, j] = v

    def door(self, x, z, along, w):
        """an opening of w metres carved through a wall (0.9 m deep each side of its line)"""
        if along == 'x': self.fill((x - w / 2, z - 0.65, x + w / 2, z + 0.65))
        else: self.fill((x - 0.65, z - w / 2, x + 0.65, z + w / 2))

    def solve(self, exits):
        """exits: [(x, z, metres still to go from there)] -> the way out's length from every free cell (metres)"""
        N = self.nx * self.nz; idx = np.arange(N).reshape(self.nx, self.nz); R, C, W = [], [], []
        F = self.free
        for dx, dz in DIRS:
            for sx, sz in ((1, 1), (1, -1)) if dx and dz else ((1, 1),):
                for ddx, ddz in {(dx * sx, dz * sz)} | ({(0, 1)} if (dx, dz) == (1, 0) else set()):
                    a = slice(max(0, -ddx), self.nx - max(0, ddx)); b = slice(max(0, -ddz), self.nz - max(0, ddz))
                    ok = F[a, b].copy(); m = max(abs(ddx), abs(ddz)) * 2
                    for k in range(1, m + 1):
                        for r in ({round(ddx * k / m - 1e-6), round(ddx * k / m + 1e-6)}):
                            for q in ({round(ddz * k / m - 1e-6), round(ddz * k / m + 1e-6)}):
                                ok &= F[a.start + r:a.stop + r, b.start + q:b.stop + q]
                    i = idx[a, b][ok]; j = idx[a.start + ddx:a.stop + ddx, b.start + ddz:b.stop + ddz][ok]
                    R.append(i); C.append(j); W.append(np.full(len(i), math.hypot(ddx, ddz) * S))
        # one source cell tied to every exit
        src = N; er, ec, ew = [], [], []
        for x, z, off in exits:
            i, j = self.ix(x), self.iz(z); best = None
            for di in range(-3, 4):
                for dj in range(-3, 4):
                    if 0 <= i + di < self.nx and 0 <= j + dj < self.nz and F[i + di, j + dj]:
                        d = math.hypot(di, dj) * S
                        if best is None or d < best[0]: best = (d, idx[i + di, j + dj])
            if best is None: print('exit in a wall', x, z); continue
            er.append(src); ec.append(best[1]); ew.append(off + best[0] + 1e-6)
        M = coo_matrix((np.concatenate(W + [ew]), (np.concatenate(R + [er]), np.concatenate(C + [ec]))), shape=(N + 1, N + 1)).tocsr()
        D = dijkstra(M, directed=False, indices=src)[:N].reshape(self.nx, self.nz)
        D[~F] = np.nan; return D

    def worst(self, D, b):
        a, c, e, f = self.ix(b[0] + 0.5), self.iz(b[1] + 0.5), self.ix(b[2] - 0.5) + 1, self.iz(b[3] - 0.5) + 1
        q = D[a:e, c:f]
        if not np.isfinite(q).any(): return None
        w = np.nanmax(np.where(np.isinf(q), np.nan, q)); k = np.unravel_index(np.nanargmax(np.where(np.isinf(q), np.nan, q)), q.shape)
        return float(w), self.x0 + (a + k[0] + .5) * S, self.z0 + (c + k[1] + .5) * S

    def share(self, D, b, lim=35.0):
        q = D[self.ix(b[0]):self.ix(b[2]), self.iz(b[1]):self.iz(b[3])]; n = np.isfinite(q).sum()
        return 100.0 * (q[np.isfinite(q)] > lim).sum() / max(1, n)


def walls(F, floor, skip=None):
    for a, b, c, d, g in WALLS[str(floor)]:
        if not (skip and skip(a, b, c, d)): F.line(a, b, c, d)


# ---- today's gates: the ten open stairs down (head at z 69.5, 2.8 m wide), by the gate's west wall
GX = {9: -233.7, 11: -153.15, 1: -72.6, 2: 7.95, 3: 88.5, 4: 169.05, 13: 255.4, 14: 335.2, 15: 407.5, 16: 479.8}
HEAD = {g: (x + 29.7, 69.3) for g, x in GX.items()}


def floor2_today():
    F = Field(-246, 556, -2, 84)
    for b in ((0, 0, 16, 30), (6, 30, 16, 52), (14, 30, 56, 52), (56, 30, 114, 52), (114, 0, 156, 52), (-241.1, 52, 551.2, 68),
              (-241.1, 8, -80, 52), (-80, 49.76, -68.2, 52.6)): F.fill(b)
    for g, x in GX.items(): F.fill((x, 67.6, x + 68, 82))
    walls(F, 2)
    for x in GX.values():                                                   # the stair's hole, its head open on the south
        F.fill((x + 28.3, 69.5, x + 31.1, 80.5), False); F.line(x + 28.2, 69.6, x + 28.2, 80.6); F.line(x + 31.2, 69.6, x + 31.2, 80.6)
    F.fill((140, 43, 143, 51), False)
    for d in PLAN['doors']:
        if d['f'] == 2 and d['type'] in ('swing', 'slide') and 'WC' not in d['id']: F.door(d['x'], d['z'], d['along'], max(1.2, d['w']))
    F.fill((13.4, 1.8, 14.6, 3.2)); F.fill((13.4, 6.8, 14.6, 8.2))             # (the toilets' doors to the landing)
    F.fill((-71.2, 49.4, -68.4, 52), False)
    return F


EXITS2 = [(x, z, 0.0) for x, z in HEAD.values()] + [(141.5, 42.7, 0.0)]


# ---- what sheet 9 proposes -------------------------------------------------------------------------------------------
GAPS = [(-165.7, -153.15), (-85.15, -72.6), (-4.6, 7.95), (75.95, 88.5), (156.5, 169.05), (237.05, 255.4), (323.4, 335.2), (403.2, 407.5), (475.5, 479.8)]
# E-1 .. E-9: closed stairs in the open gaps between the gate blocks (z 68-82, outdoors, down to the apron); in the gap of the
# new swing lobby (237-255) the stair stands at the lobby's back, z 82-88 beyond it: its door from the lobby (until the lobby
# is built, as the others)
GAPST = [(i + 1, (a + b) / 2, (4.2 if b - a < 6 else 5.0)) for i, (a, b) in enumerate(GAPS)]
ENDST = [(10, -243.6, 60.0), (11, 553.8, 60.0)]                             # beyond the walkway's two ends
# the back doors and the doors through the walkway's wall straight to the gate stairs' heads
BACK = [(18.2, 52), (26.6, 52), (35.0, 52), (43.4, 52), (51.8, 52), (-200, 52), (-182, 52), (-160, 52), (-112, 52)]
# the south side (closed outdoor stairs against the facade, z 4-8 west / z -3..0 at the landing), x of their door
SOUTHST = [(12, -235.0, 'tf'), (13, -211.0, 'tf'), (14, -175.7, 'tf'), (15, -150.7, 'tf'), (16, -112.0, 'hall'), (17, -88.0, 'hall'), (18, 15.0, 'landing')]
OFFST = [(19, 116.3, 0.6), (20, 155.4, 10.5)]


def floor2_after():
    F = floor2_today(); ex = list(EXITS2)
    for g, (x, z) in HEAD.items(): F.door(x, 68, 'x', 2.4)
    for a, b in GAPS:
        ex.append(((a + b) / 2, 67.2, 0.0)); ex += [(a - 0.9, 75, 0.0), (b + 0.9, 75, 0.0)]
    ex += [(-232.8, 75, 0.0), (546.9, 75, 0.0), (-240.2, 60, 0.0), (550.4, 60, 0.0)]
    for x, z in BACK: F.door(x, z, 'x', 1.8)
    for n, x, k in SOUTHST: ex.append((x, {'tf': 12.6, 'hall': 8.8, 'landing': 0.6}[k], 0.0))
    F.door(150, 34.5, 'x', 1.4); F.door(-119.15, 32, 'z', 1.8)
    for n, x, z in OFFST: ex.append((x, z, 0.0))
    return F, ex


# ---- the ground floor today: check-in hall, security, airside lobby, corridor, escalator room, lounge -----------------
def floor1(after=False):
    F = Field(-6, 162, -6, 56)
    for b in ((0, 0, 114, 52), (44, -4, 56, 0.5), (67, -4, 83, 0.5), (95, -4, 107, 0.5)): F.fill(b)
    F.fill((0, 30, 16, 44), True)
    walls(F, 1, skip=lambda a, b, c, d: -6 < b < -1 and -6 < d < -1 and abs(a - c) < 2.2)      # (not the revolving doors' wings)
    F.fill((68.0, 46.4, 70.1, 48.6), False)                                  # the lift
    for d in PLAN['doors']:
        if d['f'] == 1 and d['x'] < 113 and d['z'] < 50 and d['type'] in ('swing', 'slide', 'open', 'oneway', 'secure', 'revolving'): F.door(d['x'], d['z'], d['along'], max(1.2, d['w'] * 0.9))
    F.line(42, 0.2, 42, 29.9)
    ex = [(50, -3.8, 0), (75, -3.8, 0), (101, -3.8, 0)]
    if after:
        for x in (17.5, 28.0, 39.0): F.door(x, 0, 'x', 1.8); ex.append((x, 0.6, 0.0))          # A: escalator room to the forecourt
        for x in (62.0, 88.0, 110.5): F.door(x, 0, 'x', 1.8); ex.append((x, 0.6, 0.0))   # B: the hall's glass front
        F.door(35.0, 44, 'x', 1.8); ex.append((35.0, 44.6, 0.0))                          # C: the protected passage (service room, escalator room)
        F.door(102, 30, 'x', 1.8)                                                         # D: the lounge to the hall
        F.door(88, 30, 'x', 1.8)                                                          # E: security's east end to the hall
        F.door(55, 30, 'x', 1.8)
        F.door(53, 42, 'x', 1.8); F.door(87, 42, 'x', 1.8)                                # F: the lobby to security, against the way in
    return F, ex


# ---- the new part (v2_common.py), Floor 2 and ground: as drawn (no doors are dimensioned on the sheets: every room gets a 3 m
#      opening in the middle of its south side, the drawn openings to the walkway, side doors to the toilets) and as proposed
def concourse(after=False):
    import v2_common as V
    F = floor2_today(); F.fill((228, 20, 551.2, 52.6)); R = V.F
    for k in ('dutyfree', 'wc1', 'shop1', 'transfer', 'food', 'prayer', 'wc2', 'lounge', 'seating', 'wc3', 'shops2', 'queue', 'line'):
        x0, z0, x1, z1 = R[k]
        for l in ((x0, z0, x1, z0), (x1, z0, x1, z1), (x0, z0, x0, z1)): F.line(*l)
    for a, b in V.OPEN_F2 + ((228.3, 230.7),): F.fill((a, 51.2, b, 52.8))
    F.line(240, 52.4, 240, 67.6)
    for k in ('dutyfree', 'shop1', 'transfer', 'food', 'prayer', 'lounge', 'seating', 'shops2'): F.door((R[k][0] + R[k][2]) / 2, R[k][1], 'x', 3)
    for k in ('wc1', 'wc2', 'wc3'): F.door(R[k][0], 46, 'z', 1.6)
    F.door(248, 40, 'z', 6); F.door(256, 40, 'z', 6); F.door(258, 45, 'z', 6); F.door(290, 45, 'z', 6)
    ex = list(EXITS2) + [(x, 22.4 if after else 20.4, 0.0) for x in V.STAIRS]
    if after:
        for g, (x, z) in HEAD.items(): F.door(x, 68, 'x', 2.4)
        for a, b in GAPS[5:]: ex += [((a + b) / 2, 67.2, 0.0), (a - 0.9, 75, 0.0), (b + 0.9, 75, 0.0)]
        ex += [(550.4, 60, 0.0), (546.9, 75, 0.0), (550.4, 38.6, 0.0)]
        for x in CDOORS_N: F.door(x, 52, 'x', 1.8)
        for x in CDOORS_S: F.door(x, 24, 'x', 1.8)
    return F, ex


CDOORS_N = (298.5, 318.0, 331.0, 412.0, 436.0, 460.0, 536.0)                 # to the walkway
CDOORS_S = (234.5, 244.0, 252.0, 266.0, 282.0, 314.0, 333.0, 352.0, 388.0, 436.0, 460.0, 480.0, 512.0)   # to the gallery / strip


def ground_east(after=False):
    import v2_common as V
    F = Field(150, 556, -6, 56); F.fill((156, 0, 551.2, 52)); G = V.G
    for k in ('customs', 'customs_office', 'claim', 'pc_line', 'pc_office', 'pc_hall', 'crew', 'canteen', 'ground_off', 'lockers', 'staffsec', 'police', 'coach', 'staff_lane', 'wc_hall', 'family', 'trolley', 'bagserv', 'df_east', 'wc_claim', 'lf_east', 'os_east', 'second_line', 'transfer_up'):
        x0, z0, x1, z1 = G[k]
        for l in ((x0, z0, x1, z0), (x1, z0, x1, z1), (x0, z0, x0, z1), (x0, z1, x1, z1)): F.line(*l)
    for x in V.STAIRS: F.fill((x - 1.3, 7, x + 1.3, 21.5 if after else 20), False)
    F.door(236, 28, 'z', 5); F.door(236, 36, 'z', 4); F.door(228, 28, 'z', 5); F.door(228, 36, 'z', 4); F.door(312, 36, 'z', 24); F.door(324, 36, 'z', 24)
    for x, z in ((169, 40), (184, 40), (200, 40), (219, 40), (232, 40), (268, 19), (291, 19), (255, 16), (305, 42), (364, 40), (332, 40)): F.door(x, z, 'x', 2)
    F.door(388, 6, 'x', 2); F.door(420, 6, 'x', 2); F.door(453, 6, 'x', 2); F.door(388, 26, 'x', 2); F.door(420, 26, 'x', 2); F.door(453, 26, 'x', 2); F.door(470, 3, 'z', 4)
    F.door(318, 20, 'x', 1.6); F.door(318, 6, 'x', 1.6)
    F.door(228, 3, 'z', 5)
    ex = [(192, 0.4, 0.0), (295, 0.4, 0.0), (401, 0.4, 0.0), (510, 0.4, 0.0)]
    if after:
        for x in range(170, 551, 20): F.door(x, 0, 'x', 1.8); ex.append((float(x), 0.4, 0.0))
        for x in (376, 400, 408, 432, 440, 466): F.door(x, 26, 'x', 1.4)      # the north staff rooms' second doors
        for x in V.STAIRS:
            F.fill((x - 1.6, 21.5, x + 1.6, 22.3)); ex.append((x, 22.0, 0.0))
    return F, ex
