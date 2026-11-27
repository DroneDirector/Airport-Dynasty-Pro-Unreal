# Chablais bus station - the structure's dimensions, in metres (docs/Bus-Station-Plan.md).
# Shared by Tools/make_busstation_blueprint.py (the sheet) and Art/BusStation/make_busstation.py (the model), so the
# drawing and the model can never drift apart.
# Origin: the platform's west end, on its centreline, ground z = 0. +x east along the platform, +y to the terminal side.
import math

# ---- island platform
PL_LEN = 150.0          # x 0 .. 150
PL_WID = 8.0            # y -4 .. +4
PL_HALF = PL_WID / 2
DECK_Z = 0.18           # deck (and kerb) top above the carriageway
KERB_H = 0.18           # kerb upstand at both long edges
KERB_W = 0.30           # kerb stone width
KERB_DEEP = 0.35        # how deep the kerb stone goes
PAVE_T = 0.04           # the deck's wearing surface (granite paving) on the slab
TACT_OFF = 0.50         # tactile strip: its outer edge this far in from the kerb face
TACT_W = 0.40           # tactile strip width
TACT_T = 0.022          # tactile strip thickness (corduroy ribs on top)
DRN_OFF = 1.60          # slot drain centreline, from the platform edge
DRN_W = 0.30            # channel width
DRN_D = 0.32            # channel depth
DRN_SLOT = 0.025        # the slot left open between the two top plates

# ---- the vault's arches
# The vault is NOT centred on the platform: it leans towards the terminal and shelters the walk to the doors.
# Platform y -4 .. +4 (world y -18.35 .. -10.35). Column lines: south y -3.0 (1 m in from the bus-side kerb,
# clear of the bay apron), north y +13.0 (on the pavement, 1.35 m clear of the terminal wall).
SPAN = 16.0             # column to column
COL_S_Y = -3.0
COL_N_Y = 13.0
COL_Y = {-1: COL_S_Y, 1: COL_N_Y}
CANT_S = 3.5            # the roof cantilevers this far past the south columns, over the bay noses
CANT_N = 1.0            # and this far past the north columns
EAVE_S_Y = COL_S_Y - CANT_S                          # -6.5
EAVE_N_Y = COL_N_Y + CANT_N                          # 14.0
YC = (EAVE_S_Y + EAVE_N_Y) / 2                       # the arc's centre line, y 3.75
EAVE_HALF = (EAVE_N_Y - EAVE_S_Y) / 2                # 10.25
HALF = SPAN / 2         # kept for the sheets: half the column spacing
CROWN = 10.70           # to the crown, above ground (10.52 m over the deck)
PED_Z = 0.45            # concrete pedestal top above ground
PLATE_T = 0.045         # steel base plate on it
FOOT_Z = PED_Z + PLATE_T                             # the column stands on the base plate, z 0.495
EAVE_Z = 5.68                                        # the glazing stops here: 5.50 m clear over the deck
RISE = CROWN - EAVE_Z
R = (RISE * RISE + EAVE_HALF * EAVE_HALF) / (2 * RISE)
CZ = CROWN - R
TE = math.acos((EAVE_Z - CZ) / R)                    # t of the eaves, from the crown
T0 = TE                                              # the arch ends at the eaves (the cantilever tips)
T_COL = {-1: math.asin((COL_S_Y - YC) / R), 1: math.asin((COL_N_Y - YC) / R)}
COL_OD = 0.46           # white tubular steel column
COL_R = COL_OD / 2
COL_HEAD = 0.56         # square head plate under the arch
BAY = 7.5               # arches every 7.5 m
N_BAY = int(round(PL_LEN / BAY))                     # 20 bays
N_ARCH = N_BAY + 1                                   # 21 arches, x = 0 .. 150
ARCH_OD = 0.42          # white tubular steel arch
ARCH_R = ARCH_OD / 2

# ---- the eaves (gutter beam) and the purlins between them
EAVE_Y = EAVE_N_Y                                    # the north eaves, y 14.0
EAVE_Y_S = {-1: EAVE_S_Y, 1: EAVE_N_Y}
EAVE_W = 0.30           # eaves beam, square box section
PUR_OD = 0.14           # purlin tube
PUR_R = PUR_OD / 2
RP = R + ARCH_R + PUR_R - 0.04                       # purlin axis radius: 40 mm into the arch tube
N_ROW = 14                                           # glazing rows between the two eaves beams
PUR_K = list(range(1, N_ROW))                         # 13 purlins; k = 0 and 14 are the eaves beams
CLEAT_T = 0.016         # welded saddle cleat at every purlin/arch joint

# ---- glazing
GL_COLS = 3             # glazing bars split every bay into three panes (2.5 m)
GL_BAR_W = 0.08         # secondary glazing bar, across the bay
GL_BAR_D = 0.12         # its radial depth; the pane sits in it
PANE_T = 0.024          # laminated glass

# ---- cable bracing, in the roof plane just inside the arches
BRACE_BAYS = [0, 4, 9, 14, 19]
CABLE_OD = 0.032
CABLE_DR = -0.45        # cable chord radius offset from R (inside the arches, clear of the purlins)
LUG_T = 0.018

# ---- gutter at the arch feet
GUT_W = 0.40
GUT_D = 0.22
GUT_T = 0.012
GUT_DY = 0.59           # gutter axis, outboard of the eaves beam
GUT_DZ = -0.30          # and below it
BRK_T = 0.014           # gutter bracket plate
DOWN_OD = 0.11          # downpipe
DOWN_EVERY = 2          # one at every second arch

# ---- feet and foundations
FOOT_PAD = 1.30         # square footing pad, seen above ground
FOOT_PAD_Z = 0.12       # how far it stands above ground
PED_W = 0.80            # concrete pedestal on top of it
PLATE_W = 0.62          # steel base plate
BOLT_OD = 0.042
RAFT_T = 0.45           # the foundation raft the pads, pedestals and the platform slab are all cast on
RAFT_TOP = -0.02        # just under the ground, never seen


def arch_pt(t, dr=0.0):
    """A point on an arch's axis, t radians from the crown (+t to +y); dr widens the radius."""
    return YC + (R + dr) * math.sin(t), CZ + (R + dr) * math.cos(t)


def row_t(k):
    """The t of glazing row line k (0 = south eaves, N_ROW = north eaves)."""
    return -TE + k * (2 * TE / N_ROW)
