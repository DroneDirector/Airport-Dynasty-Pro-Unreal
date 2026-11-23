# The gates' curved windows: the numbers (owner approved 2026-11-23, docs/Blueprints/Gate-Arc; the same as
# Blueprints/Terminal/GateArc/ga_geom.py, but measured from the gate's own outer corner: lx = plan x - the room's x0, so one set
# of models and one seat plan serve every gate).  Plan metres: x along the building, z out to the apron, y up.
# Read by make_gate_arc.py (the models), Tools/terminal_gatearc_edit.py (the layout), Tools/terminal_gatearc_plan.py (the plan),
# Tools/check_gate_arc.py (the checks).
import math
GATES = (1, 2, 3, 4, 9, 11, 13, 14, 15, 16)          # every Floor 2 gate window; a new gate: add its number here, nothing else
X1, XR = 31.84, 34.0                                 # the arc ends at the lift's face; the wall to the view bay
ZG, ZE = 81.91, 82.0                                 # the flat glass's old middle line; the old edge of slab and roof
F2, SOFFIT, CEIL, GTOP, ROOF = 6.0, 5.40, 11.36, 11.40, 12.00
DEPTH, NBAY, E1 = 3.0, 19, 0.09                      # 19 panes; the slab's and the roof's edge 9 cm outside the glass
JX, JC = 22.0, 1.71                                  # the jetway corridor: its axis, its fascia's half width
H = X1 / 2; R = (H * H + DEPTH * DEPTH) / (2 * DEPTH); HALF = math.asin(H / R); L = 2 * R * HALF; CZ = ZG + DEPTH - R; BAY = L / NBAY


def pt(t, off=0.0):                                  # t: metres along the arc from its middle; off: outward
    a = t / R; r = R + off; return (H + r * math.sin(a), CZ + r * math.cos(a))


def zat(x, off=0.0):
    r = R + off; o = x - H; return CZ + math.sqrt(max(0.0, r * r - o * o))


def tat(x, off=0.0): return R * math.asin((x - H) / (R + off))


MULL = [-L / 2 + BAY * i for i in range(NBAY + 1)]
TJL = MULL[12]; TJR = tat(JX + (JX - pt(TJL)[0]))    # the portal's jambs: the 13th mullion, and as far the other side of the corridor
HEAD = (9.10, 9.25)
# the seats (knees 1.55 m from the mullions; 12 + 12 with the 2.42 m way to the glass; the rows' aisle in line with it)
PITCH, SOFF = 0.62, -2.05
SP = PITCH * R / (R + SOFF)                          # the pitch measured on the glass's own arc
_E = [2.75 - SP * k for k in range(12)][::-1]
SEC_E = _E; SEC_W = [_E[0] - 5 * SP - SP * k for k in range(12)][::-1]
ROWZ = (71.4, 74.4, 77.4); ROW0 = 3.09               # the first of today's 37 seats
ROWS = ((-2, 12), (14, 25))                          # (first seat, how many): 12, the 2.42 m aisle (4 seats), 25 - 37 as today
LAMPS = [t for t in (-12.8 + 3.2 * k for k in range(9)) if not (abs(pt(t, -1.35)[0] - JX) < 2.2 or zat(pt(t)[0]) - ZE < 1.8)]
LAMP_OFF, LAMP_Y = -1.35, 11.33


def has_corridor(layout_jetways, x0):
    """Does a jetway's corridor stand at this gate's own door (Data/babylon-jetways.json)?"""
    import re
    return next((n['name'] for n in layout_jetways['nodes'] if re.match(r'J-\d+[A-Z]?$', n['name']) and abs(-n['translation'][0] - (x0 + JX)) < 0.05), None)
