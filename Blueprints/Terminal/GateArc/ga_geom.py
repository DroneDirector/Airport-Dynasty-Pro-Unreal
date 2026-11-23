# Gate arc (gate 9 as the example): the numbers every drawing and the Blender scene use, all measured on the game's own
# model (terminal-layout.json / jetways.glb / holomap-walls.json).  Plan metres: x along the building, z out to the apron.
import math
X0, X1 = -233.70, -201.86      # the outer corner's mullion line .. the lift cladding's west face
XR = -199.70                   # the wall to the view bay (the gate room's other side)
ZG = 81.91                     # the flat glass's middle line today (mullions z 81.84-81.98)
ZE = 82.00                     # the slab's and the roof's edge today
F2, SLAB_B, CEIL, GTOP, ROOF = 6.0, 5.63, 11.36, 11.40, 12.00
CH, MID = X1 - X0, (X0 + X1) / 2
DEPTH = 3.0
NBAY = 19                      # panes: 20 radial mullions, as many as the flat window has today
# the jetway J-9 (never moves)
JX = -211.70                   # its axis
JCL, JCR = -213.41, -209.99    # the fixed corridor's outer faces (fascia)
JTOP, JBOT = 9.10, 5.70        # its roof's top and its floor slab's underside at the gate
JSL = 0.0577                   # its slope: 5.77 % down towards the rotunda
ROT = (-211.70, 95.84); ROTR = 1.92
COLZ = 85.85                   # its first columns' faces towards the building (x -213.3..-210.1)
LIFT = (-201.86, 79.54, -199.74, 81.66); STAIR = (-205.40, 69.5, -202.60, 80.5)


class Arc:
    def __init__(s, d=DEPTH, x0=X0, x1=X1, zg=ZG, n=NBAY):
        s.d, s.x0, s.x1, s.zg, s.n = d, x0, x1, zg, n
        s.h = (x1 - x0) / 2; s.mid = (x0 + x1) / 2
        s.R = (s.h * s.h + d * d) / (2 * d); s.a = math.asin(s.h / s.R)
        s.L = 2 * s.R * s.a; s.cz = zg + d - s.R; s.p = s.L / n
        s.area = s.R ** 2 * (s.a - math.sin(s.a) * math.cos(s.a))

    def pt(s, t, off=0.0):      # t: metres along the arc from its middle; off: outward offset
        a = t / s.R; r = s.R + off
        return (s.mid + r * math.sin(a), s.cz + r * math.cos(a))

    def z(s, x, off=0.0):
        r = s.R + off; o = x - s.mid
        return s.cz + math.sqrt(max(0.0, r * r - o * o))

    def t(s, x, off=0.0): return s.R * math.asin((x - s.mid) / (s.R + off))

    def mull(s): return [-s.L / 2 + s.p * i for i in range(s.n + 1)]


A = Arc()
TJL = A.mull()[12]                                  # the portal's left jamb IS the 13th mullion
TJR = A.t(JX + (JX - A.pt(TJL)[0]))                 # the right jamb: as far the other side of the corridor
PORT_HEAD = (9.10, 9.25)
# ---- the seats along the curve (owner, 2026-11-23: room to walk in front, 4 out of the middle, a way to the glass)
PITCH, SOFF = 0.62, -2.05                           # the seat's middle 2.05 m in from the glass (the first drawing: 1.75)
KNEE, BACK, FOOT = 0.43, 0.41, 0.453                # from the seat's middle: a seated person's knees (0.65 from the backrest), the backrest, the rear feet
SP = PITCH * A.R / (A.R + SOFF)                     # the pitch measured on the glass's arc
SEC_E = [2.75 - SP * k for k in range(12)][::-1]    # the east section's 12 seats (t of their middles)
SEC_W = [SEC_E[0] - 5 * SP - SP * k for k in range(12)][::-1]   # 4 seats left out, then the west section's 12
GAP_T = (SEC_E[0] + SEC_W[-1]) / 2
ROWZ = (71.4, 74.4, 77.4); ROW0 = -230.61           # today's three rows: 37 seats each
ROWS_NOW = [[ROW0 + PITCH * k for k in range(37)]]
ROWS_OPT = [[ROW0 + PITCH * k for k in range(-2, 10)], [ROW0 + PITCH * k for k in range(14, 39)]]   # the option: the same 37, 4 moved to the ends


if __name__ == '__main__':
    for d in (2, 3, 4):
        q = Arc(d); print(d, 'R %.3f  half angle %.2f deg  arc %.3f  pane %.4f  area %.2f  z at corridor L/axis/R %.2f %.2f %.2f  to columns %.2f' % (
            q.R, math.degrees(q.a), q.L, q.p, q.area, q.z(JCL), q.z(JX), q.z(JCR), COLZ - q.z(JCL)))
    print('jambs', A.pt(TJL), A.pt(TJR), TJL, TJR, 'pane bow mm', A.p ** 2 / 8 / A.R * 1000, 'next mullion', A.mull()[15] - TJR)
