# Terminal part 2b (owner approved 2026-11-23, blueprint v2 sheets 2c, 2d, 3a, 3c, 5b, 9c): gate 13's swing lobby, the ways down at
# gates 13-16 and the closed arrivals corridor: the numbers, one place for make_arrivals.py (the models), Art/TerminalAnnex/
# make_pier_extension.py (the pier's walls), Tools/terminal_arrivals_edit.py (the layout), Tools/terminal_east_plan.py (the plan).
# Plan metres; u = plan x - the gate room's west wall; y from the ground. Source/.../SwingGate.cpp has the same escalator, stair and
# lift numbers.
GATE = {13: 255.4, 14: 335.2, 15: 407.5, 16: 479.8}
JW = {13: (54.0, 59.0, 66.0), 14: (54.0, 64.0), 15: (54.0, 64.0), 16: (54.0, 64.0)}        # the jetways' middles (u)
PARTY = {13: (71.3, 74.4), 14: (69.5, 73.5), 15: (69.5, 73.5), 16: (69.5, 73.5)}            # the opening gate room / view bay (z), head 3.2 m
F2 = 6.01
# ---- the core (u) ----
TOP = 50.9                                             # the escalators' top combs; bottom 13.757 m west
BOT = TOP - 13.7569
EZ = (80.81, 79.19)                                    # the two escalators' middles
ST0, ST1, STZ = 39.2, 50.6, (76.44, 78.36)             # the stair: foot, top, its sides
HX0, HX1, HZ = 36.9, 51.6, (76.44, 78.375, 81.64)      # the hole in Floor 2 (stair strip to ST1, escalators to HX1)
LU, LZ = 50.0, 75.3                                    # the lift's middle; its doors face east
CW, CE = 36.86, 52.4                                   # the core's west glass wall; the doors' wall to the vestibule
VS, NS = 76.4, 74.2                                    # the vestibule's and the core's south glass; the lift nook's
ARR_Z = 79.15                                          # the arrivals doors' middle (z; 5 m clear), between core and vestibule
GATE_U = 60.5                                          # the gate side doors' middle (u; 5 m clear), in the vestibule's south glass
LW, LE = 34.2, 55.2                                    # the ground lobby's walls
OUT_U = 43.0                                           # the one-way doors to the corridor (u; 3 m clear), in the pier's wall z 68
STAIR_DOOR = 30.0                                      # the gate stair hall's own door onto the apron (u; 1.8 m clear)
TURN = (29.3, 40.3, 68.3, 71.1)                        # gate 13's stair turned along the walkway wall (u0, u1, z0, z1): foot west
# ---- the corridor (plan x, z) ----
CX0, CX1, CZ0, CZ1, CH = 294.0, 526.0, 63.4, 67.9, 4.0
KX0, KX1 = 340.0, 346.0                                # the cross passage to the passport hall
PC_X, PC_HALF = 343.0, 2.66                            # its one-way doors (z 52; 5 m clear)
# ---- the swing lobby (plan x, z) ----
SX0, SX1, SZ0, SZ1 = 237.13, 255.32, 68.1, 82.0
DC = 238.5                                             # doors C (2.4 m clear), in the walkway's wall (z 68)
DF = 73.5                                              # doors F (6 m clear), in gate 13's west wall (x 255.4)
DB = ((267.4, 263.4, 271.4), (306.4, 301.4, 311.4))    # doors B (6 m clear): middle, the opening
def f1_z68(): return [(a + OUT_U - 1.66, a + OUT_U + 1.66, 0.0, 3.16) for a in GATE.values()]
def f1_z52(): return [(PC_X - PC_HALF, PC_X + PC_HALF, 0.0, 3.16)]
