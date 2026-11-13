# The town's car dealership (fictional brand VELDRA): a two-storey glass showroom under a floating fascia clad in
# dark bronze panels with wavy grooves, a sand-coloured service wing, and a full interior: showroom floor with cars,
# reception, customer lounge with a coffee bar, toilets, glass sales offices, service reception, staff-only areas and
# the workshop. Real size in metres; the front faces south (-Y), X runs east.
import sys, math; sys.path.insert(0, '/home/claude/bld')
import bpy
from lib import *

reset()
view = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'ext'
samples = int(sys.argv[sys.argv.index('--') + 2]) if '--' in sys.argv and len(sys.argv) > sys.argv.index('--') + 2 else 128

# ── materials ──
fascia = wavy_panel('fascia', (0.074, 0.056, 0.056), (0.02, 0.016, 0.016), cell=1.6, amp=0.18, rough=0.45, metal=0.2)  # bronze-brown from the photos
fascia2 = wavy_panel('fascia_dark', (0.019, 0.027, 0.069), (0.006, 0.008, 0.02), cell=1.4, amp=0.16, rough=0.4, metal=0.2)  # the navy variant
soffit = mat('soffit', (0.02, 0.018, 0.017), 0.45, 0.0, coat=0.2)
glass = mat('glass', (0.92, 0.96, 0.97), 0.0, glass=True)
alu = mat('alu', (0.62, 0.64, 0.66), 0.28, 1.0)
alu_dark = mat('alu_dark', (0.12, 0.12, 0.13), 0.35, 0.9)
chrome = mat('chrome', (0.9, 0.91, 0.93), 0.06, 1.0)
plinth = img_mat('plinth', 'concrete_col.png', 'concrete_h.png', 1.5, 0.7, 0.15)
beige = img_mat('beige', 'beige_col.png', None, 3.0, 0.75)
white = mat('white_wall', (0.86, 0.86, 0.85), 0.7)
ceiling = mat('ceiling', (0.9, 0.9, 0.9), 0.8)
floor = mat('floor_tile', (0.62, 0.62, 0.63), 0.12, bump=0.05, bump_scale=40, var=0.05)
epoxy = mat('epoxy', (0.33, 0.35, 0.37), 0.25)
asph = img_mat('asphalt', 'asphalt_col.png', 'asphalt_h.png', 3.0, 0.88, 0.25, box=False)
conc = img_mat('concrete', 'concrete_col.png', 'concrete_h.png', 2.0, 0.8, 0.2, box=False)
gravel = img_mat('gravel', 'gravel_col.png', 'gravel_h.png', 1.6, 0.85, 0.8, box=False)
lawn = img_mat('lawn', 'lawn_col.png', 'lawn_h.png', 2.5, 0.9, 0.4, box=False)
paintw = mat('paint_line', (0.85, 0.85, 0.82), 0.6)
painty = mat('paint_yellow', (0.85, 0.65, 0.1), 0.6)
wood = mat('oak', (0.45, 0.3, 0.17), 0.45, bump=0.1, bump_scale=6, var=0.2)
desk_w = mat('desk_white', (0.92, 0.92, 0.9), 0.25, coat=0.5)
fabric = mat('fabric', (0.16, 0.18, 0.21), 0.9, bump=0.2, bump_scale=80)
fabric2 = mat('fabric_warm', (0.45, 0.33, 0.24), 0.9, bump=0.2, bump_scale=80)
black = mat('black', (0.02, 0.02, 0.02), 0.4)
screen = mat('screen', (0.05, 0.1, 0.2), 0.1, emit=(0.35, 0.55, 0.9), emit_str=2.0)
leaf = mat('leaf', (0.08, 0.2, 0.05), 0.7, bump=0.6, bump_scale=15, var=0.3)
porcelain = mat('porcelain', (0.95, 0.95, 0.94), 0.1, coat=0.6)
tile_wc = mat('tile_wc', (0.78, 0.8, 0.8), 0.3)
red = mat('red', (0.75, 0.05, 0.04), 0.4)
lift_y = mat('lift_yellow', (0.9, 0.62, 0.05), 0.4, 0.2)
downl = mat('downlight', (1, 1, 1), 0.2, emit=(1, 0.93, 0.82), emit_str=25)
strip = mat('strip_light', (1, 1, 1), 0.2, emit=(0.9, 0.95, 1.0), emit_str=12)
glowsign = mat('sign_glow', (1, 1, 1), 0.2, emit=(1, 1, 1), emit_str=4)
sign_g = mat('sign_green', (0.05, 0.5, 0.2), 0.3, emit=(0.1, 0.9, 0.35), emit_str=1.5)

W, D, H = 42.0, 26.0, 9.0          # showroom: width (x), depth (y), glass height
FZ0, FZ1 = 9.6, 13.2               # fascia band
# ── showroom shell ──
box('slab', (W + 0.4, D + 0.4, 0.4), (W / 2, D / 2, 0.2), plinth)
box('floor', (W - 0.2, D - 0.2, 0.02), (W / 2, D / 2, 0.41), floor)
box('ceiling', (W - 0.3, D - 0.3, 0.3), (W / 2, D / 2, H + 0.15), ceiling)
box('roof', (W + 2.4, D + 2.4, 0.4), (W / 2, D / 2, FZ1 - 0.2), alu_dark)
# fascia: a floating box overhanging the glass by 1.2 m, and far out to the west over the entrance corner
OV, WEST = 1.2, 5.0
box('fascia_s', (W + OV + WEST, 0.4, FZ1 - FZ0), ((W + OV - WEST) / 2, -OV, (FZ0 + FZ1) / 2), fascia)
box('fascia_n', (W + OV + WEST, 0.4, FZ1 - FZ0), ((W + OV - WEST) / 2, D + OV, (FZ0 + FZ1) / 2), fascia)
box('fascia_w', (0.4, D + 2 * OV, FZ1 - FZ0), (-WEST, D / 2, (FZ0 + FZ1) / 2), fascia)
box('fascia_e', (0.4, D + 2 * OV, FZ1 - FZ0), (W + OV, D / 2, (FZ0 + FZ1) / 2), fascia)
box('fascia_soffit', (W + OV + WEST, D + 2 * OV, 0.12), ((W + OV - WEST) / 2, D / 2, FZ0 + 0.06), soffit)
# dark glossy band between the glass and the fascia, with downlights in the soffit
box('band_s', (W, 0.25, FZ0 - H), (W / 2, 0.0, (H + FZ0) / 2), soffit)
box('band_w', (0.25, D, FZ0 - H), (0.0, D / 2, (H + FZ0) / 2), soffit)
for x in range(-4, int(W + 1), 3):
    cyl('dl_out', 0.09, 0.02, (x + 0.5, -0.6, FZ0 - 0.01), downl, 16)
for y in range(2, int(D), 3):
    cyl('dl_out', 0.09, 0.02, (-2.5, y, FZ0 - 0.01), downl, 16)
# glass curtain wall: front (y=0) and west (x=0), mullions every 1.5 m, a transom at 4.6 m, a white sill
def curtain(axis, length, fixed, z0=0.4, z1=H, step=1.5, skip=()):
    for i in range(int(length / step)):
        a = i * step + step / 2
        if any(s0 <= a <= s1 for s0, s1 in skip): continue
        if axis == 'x': box('glass', (step - 0.04, 0.02, z1 - z0), (a, fixed, (z0 + z1) / 2), glass)
        else: box('glass', (0.02, step - 0.04, z1 - z0), (fixed, a, (z0 + z1) / 2), glass)
    for i in range(int(length / step) + 1):
        a = i * step
        if axis == 'x': box('mullion', (0.06, 0.16, z1 - z0), (a, fixed, (z0 + z1) / 2), alu)
        else: box('mullion', (0.16, 0.06, z1 - z0), (fixed, a, (z0 + z1) / 2), alu)
    for z in (z0, 4.6, z1):
        if axis == 'x': box('transom', (length, 0.18, 0.1), (length / 2, fixed, z), alu)
        else: box('transom', (0.18, length, 0.1), (fixed, length / 2, z), alu)
curtain('x', W, 0.0, skip=[(4.5, 7.5)])
curtain('y', D, 0.0)
# entrance: a dark metal portal standing out of the glass, double glass doors
PX = 6.0
box('portal_top', (5.0, 1.6, 0.5), (PX, -0.6, 4.35), alu_dark, bevel=0.02)
box('portal_l', (0.5, 1.6, 4.2), (PX - 2.25, -0.6, 2.4), alu_dark, bevel=0.02)
box('portal_r', (0.5, 1.6, 4.2), (PX + 2.25, -0.6, 2.4), alu_dark, bevel=0.02)
for dx in (-1.0, 0.0, 1.0):
    box('door_glass', (0.96, 0.03, 3.2), (PX + dx * 1.0 if dx else PX, 0.0, 2.0), glass)
box('door_frame', (4.0, 0.12, 0.12), (PX, 0.0, 3.62), alu)
for dx in (-2.0, -1.0, 0.0, 1.0, 2.0): box('door_jamb', (0.07, 0.12, 3.2), (PX + dx, 0.0, 2.0), alu)
for dx in (-0.35, 0.35): box('handle', (0.03, 0.06, 1.2), (PX + dx, -0.08, 1.5), chrome)
box('glass_above_door', (4.0, 0.02, H - 4.6), (PX, 0.0, (4.6 + H) / 2), glass)
box('glass_door_side', (1.0, 0.02, 4.2), (PX - 2.25 + 0.0, 0.0, 2.5), glass)
box('step', (6.0, 2.4, 0.4), (PX, -1.2, 0.2), plinth)
text('door_hours', 'Mán–fös 9–18 · Lau 11–16', (PX + 0.6, -0.04, 1.75), 0.06, glowsign, depth=0.002, align='LEFT')
# back wall (sand panels) and the brand wall inside above the rooms
box('back_wall', (W, 0.3, H + 0.6), (W / 2, D - 0.15, (H + 0.6) / 2 + 0.4), beige)

# ── the brand: chrome letters and an oval badge (fictional) ──
def badge(x, y, z, s, face_y=True):
    t = bpy.data.objects
    bpy.ops.mesh.primitive_torus_add(major_radius=0.62 * s, minor_radius=0.07 * s, location=(x, y, z), rotation=(math.pi / 2, 0, 0) if face_y else (math.pi / 2, 0, math.pi / 2))
    o = bpy.context.active_object; o.scale = (1.0, 0.62, 1.0) if face_y else (0.62, 1.0, 1.0)
    o.name = 'badge_ring'; o.data.materials.append(chrome); [setattr(p, 'use_smooth', True) for p in o.data.polygons]
    for cc in o.users_collection: cc.objects.unlink(o)
    col('Building').objects.link(o)
    text('badge_v', 'V', (x, y - (0.05 if face_y else 0), z), 0.95 * s, chrome, rot=(math.pi / 2, 0, 0) if face_y else (math.pi / 2, 0, math.pi / 2), depth=0.08 * s)
badge(12.5, -OV - 0.25, (FZ0 + FZ1) / 2 + 0.05, 1.5)
text('brand_s', 'VELDRA', (21.5, -OV - 0.28, (FZ0 + FZ1) / 2), 1.55, chrome, depth=0.18)
text('dealer_w', 'Sandur Bílar', (-WEST - 0.28, 2.5, (FZ0 + FZ1) / 2), 0.7, chrome, rot=(math.pi / 2, 0, -math.pi / 2), depth=0.08)
badge(-WEST - 0.25, 9.5, (FZ0 + FZ1) / 2, 1.2, face_y=False)

# ── service wing (sand panels, a dark wavy band, sectional doors) ──
SX0, SX1, SY0, SY1, SH = W, 66.0, 4.0, 30.0, 7.0
box('svc_body', (SX1 - SX0, SY1 - SY0, SH), ((SX0 + SX1) / 2, (SY0 + SY1) / 2, SH / 2), beige)
box('svc_roof', (SX1 - SX0 + 0.3, SY1 - SY0 + 0.3, 0.3), ((SX0 + SX1) / 2, (SY0 + SY1) / 2, SH + 0.15), alu_dark)
box('svc_band', (17.0, 0.25, 2.4), (57.0, SY0 - 0.13, 5.0), fascia2)
for x in range(int(SX0) + 3, int(SX1), 3): box('joint', (0.02, 0.02, SH), (x, SY0 - 0.01, SH / 2), mat('joint', (0.5, 0.42, 0.32), 0.8))
for z in (2.3, 4.6): box('joint_h', (SX1 - SX0, 0.02, 0.02), ((SX0 + SX1) / 2, SY0 - 0.01, z), mat('joint', (0.5, 0.42, 0.32), 0.8))
box('svc_win_glass', (4.0, 0.03, 2.0), (50.0, SY0 - 0.02, 2.0), glass); box('svc_win_frame', (4.2, 0.08, 2.2), (50.0, SY0 + 0.01, 2.0), alu)
box('svc_door', (1.1, 0.04, 2.3), (59.0, SY0 - 0.02, 1.15), glass); box('svc_door_frame', (1.3, 0.08, 2.45), (59.0, SY0 + 0.01, 1.2), alu)
text('svc_door_sign', 'ÞJÓNUSTA · SERVICE', (59.0, SY0 - 0.05, 2.75), 0.16, chrome, depth=0.01)
for i, y in enumerate((9.0, 15.0, 21.0)):
    box('sect_door', (0.12, 3.6, 4.2), (SX1 + 0.02, y, 2.1), mat('sect', (0.8, 0.8, 0.8), 0.4, 0.6, wave='Z'))
    box('sect_frame', (0.2, 3.9, 0.15), (SX1 + 0.05, y, 4.3), alu_dark)
    text('bay_no', str(i + 1), (SX1 + 0.12, y, 5.0), 0.5, paintw, rot=(math.pi / 2, 0, math.pi / 2), depth=0.01)
box('svc_glass_link', (0.3, 14.0, 0.3), (SX0, 11.0, H + 0.45), alu_dark)

# ── showroom interior: ceiling downlights, the room block along the back, reception, cars ──
for x in range(3, int(W), 3):
    for y in range(3, 18, 3):
        cyl('dl', 0.13, 0.03, (x, y, H - 0.01), downl, 20)
ROOM_Y, ROOM_H = 17.5, 3.5
box('rooms_roof', (W - 0.4, D - ROOM_Y - 0.3, 0.25), (W / 2, (ROOM_Y + D - 0.3) / 2, ROOM_H + 0.4 + 0.12), white)
box('rooms_fascia', (W - 0.4, 0.15, 0.6), (W / 2, ROOM_Y, ROOM_H + 0.4 + 0.2), wood)
for x in range(2, int(W) - 1, 2): cyl('dl_room_edge', 0.06, 0.02, (x, ROOM_Y + 0.3, ROOM_H + 0.39), downl, 12)
# brand wall above the rooms
box('brand_wall', (24.0, 0.2, 4.0), (21.0, D - 0.35, 6.7), fascia2)
badge(14.5, D - 0.5, 6.7, 1.3); text('brand_in', 'VELDRA', (23.5, D - 0.5, 6.6), 1.3, chrome, depth=0.12)
box('brand_light', (24.0, 0.25, 0.08), (21.0, D - 0.6, 8.75), strip)
# 1) customer lounge (public room), open to the showroom
LX0, LX1 = 0.3, 11.0
box('lounge_wall', (0.15, D - ROOM_Y, ROOM_H), (LX1, (ROOM_Y + D) / 2, ROOM_H / 2 + 0.4), white)
box('lounge_rug', (6.0, 4.0, 0.01), (5.0, 21.5, 0.43), fabric2)
for i, (x, y, r) in enumerate(((3.0, 23.6, 0), (6.8, 23.6, 0), (2.0, 20.5, math.pi / 2))):
    box('sofa_seat', (3.2, 0.9, 0.45), (x, y, 0.65), fabric, bevel=0.06, rot=r)
    box('sofa_back', (3.2, 0.25, 0.55), (x + (0 if r == 0 else -0.35), y + (0.35 if r == 0 else 0), 1.1), fabric, bevel=0.06, rot=r)
box('coffee_table', (1.6, 0.9, 0.06), (5.0, 21.6, 0.85), wood, bevel=0.01); box('ct_leg', (1.2, 0.5, 0.4), (5.0, 21.6, 0.62), black)
box('coffee_bar', (3.5, 0.7, 1.05), (8.8, 18.6, 0.93), desk_w, bevel=0.02); box('bar_top', (3.6, 0.8, 0.05), (8.8, 18.6, 1.48), wood)
box('coffee_machine', (0.5, 0.45, 0.55), (8.0, 18.7, 1.78), black, bevel=0.03); box('cups', (0.6, 0.3, 0.12), (9.4, 18.7, 1.57), porcelain)
box('lounge_tv', (2.2, 0.06, 1.25), (5.0, D - 0.33, 2.2), screen)
for x in (1.0, 10.3): cyl('plant_pot', 0.3, 0.6, (x, 18.3, 0.7), black); cyl('plant', 0.45, 1.3, (x, 18.3, 1.6), leaf, 12)
text('lounge_sign', 'Biðstofa · Lounge', (5.5, ROOM_Y - 0.08, ROOM_H + 0.68), 0.25, chrome, depth=0.01)
# 2) toilets: a tiled block with two doors and signs
WX0, WX1 = 11.0, 17.0
box('wc_front', (WX1 - WX0, 0.15, ROOM_H), ((WX0 + WX1) / 2, ROOM_Y, ROOM_H / 2 + 0.4), white)
box('wc_mid', (0.12, D - ROOM_Y, ROOM_H), (14.0, (ROOM_Y + D) / 2, ROOM_H / 2 + 0.4), tile_wc)
box('wc_side', (0.15, D - ROOM_Y, ROOM_H), (WX1, (ROOM_Y + D) / 2, ROOM_H / 2 + 0.4), white)
for x, lab in ((12.5, 'WC  ♀'), (15.5, 'WC  ♂')):
    box('wc_door', (0.95, 0.06, 2.15), (x, ROOM_Y - 0.06, 1.48), wood, bevel=0.01)
    box('wc_handle', (0.15, 0.04, 0.03), (x + 0.35, ROOM_Y - 0.12, 1.45), chrome)
    text('wc_sign', lab, (x, ROOM_Y - 0.1, 2.85), 0.2, chrome, depth=0.01)
    box('wc_floor', (2.8, D - ROOM_Y - 0.4, 0.01), (x, (ROOM_Y + D) / 2, 0.43), tile_wc)
    cyl('toilet', 0.22, 0.42, (x, D - 1.0, 0.63), porcelain, 24); box('cistern', (0.45, 0.2, 0.45), (x, D - 0.6, 0.95), porcelain, bevel=0.03)
    box('sink', (0.55, 0.42, 0.14), (x - 0.9, ROOM_Y + 2.0, 1.25), porcelain, bevel=0.04); box('mirror', (0.6, 0.02, 0.8), (x - 0.9, ROOM_Y + 2.0, 1.9), chrome, rot=0)
# 3) two glass sales offices
for k, (ox0, ox1) in enumerate(((17.0, 23.0), (23.0, 29.0))):
    box('office_glass', (ox1 - ox0 - 1.1, 0.02, ROOM_H - 0.1), ((ox0 + ox1) / 2 + 0.5, ROOM_Y, ROOM_H / 2 + 0.4), glass)
    box('office_door', (0.95, 0.03, 2.2), (ox0 + 0.6, ROOM_Y, 1.5), glass); box('office_door_bar', (0.03, 0.06, 1.0), (ox0 + 0.95, ROOM_Y - 0.05, 1.5), chrome)
    box('office_wall', (0.12, D - ROOM_Y, ROOM_H), (ox1, (ROOM_Y + D) / 2, ROOM_H / 2 + 0.4), white)
    box('office_frost', (ox1 - ox0 - 1.1, 0.025, 0.25), ((ox0 + ox1) / 2 + 0.5, ROOM_Y - 0.01, 1.6), mat('frost', (0.9, 0.9, 0.9), 0.5))
    cx = (ox0 + ox1) / 2
    box('desk', (2.2, 0.9, 0.05), (cx, 21.2, 1.15), desk_w, bevel=0.01); box('desk_leg', (2.0, 0.6, 0.7), (cx, 21.4, 0.78), alu)
    box('monitor', (0.65, 0.04, 0.4), (cx, 21.5, 1.55), screen); box('monitor_foot', (0.2, 0.15, 0.2), (cx, 21.55, 1.27), black)
    for dx in (-0.55, 0.55): box('guest_chair', (0.5, 0.5, 0.5), (cx + dx, 20.2, 0.68), fabric2, bevel=0.06)
    box('office_chair', (0.6, 0.6, 0.55), (cx, 22.3, 0.7), black, bevel=0.06)
    text('office_sign', f'Sala {k + 1}', (cx + 1.5, ROOM_Y - 0.03, 2.7), 0.18, chrome, depth=0.005)
    area_light('office_light', (cx, 22.0, ROOM_H + 0.3), 2.0, 60, rot=(0, 0, 0))
# 4) staff-only corridor (to the back office, the parts store and the workshop)
box('staff_wall', (36.0 - 29.0, 0.15, ROOM_H), (32.5, ROOM_Y, ROOM_H / 2 + 0.4), white)
box('staff_door', (1.0, 0.06, 2.15), (31.0, ROOM_Y - 0.06, 1.48), mat('staff_door', (0.3, 0.32, 0.35), 0.4, 0.3), bevel=0.01)
box('staff_handle', (0.15, 0.04, 0.03), (31.35, ROOM_Y - 0.12, 1.45), chrome)
box('staff_sign_plate', (0.9, 0.02, 0.32), (31.0, ROOM_Y - 0.1, 2.85), red)
text('staff_sign', 'STARFSFÓLK · STAFF ONLY', (31.0, ROOM_Y - 0.12, 2.85), 0.075, glowsign, depth=0.004)
box('keypad', (0.1, 0.03, 0.15), (31.7, ROOM_Y - 0.1, 1.5), black)
text('exit_sign', 'NEYÐARÚTGANGUR', (34.5, ROOM_Y - 0.1, 3.3), 0.12, sign_g, depth=0.004)
# 5) service reception
box('svc_desk', (5.0, 0.9, 1.1), (39.0, 16.4, 0.95), desk_w, bevel=0.02); box('svc_desk_top', (5.2, 1.1, 0.05), (39.0, 16.4, 1.52), wood)
for dx in (-1.4, 1.2): box('svc_screen', (0.55, 0.04, 0.36), (39.0 + dx, 16.75, 1.78), screen)
text('svc_sign', 'Þjónustumóttaka · Service', (39.0, ROOM_Y - 0.08, ROOM_H + 0.68), 0.24, chrome, depth=0.01)
box('svc_wall', (36.0 - 36.0 + 0.15, D - ROOM_Y, ROOM_H), (36.0, (ROOM_Y + D) / 2, ROOM_H / 2 + 0.4), white)
box('workshop_door_glass', (0.04, 3.0, 3.0), (W - 0.05, 20.5, 1.9), glass)
# main reception desk: curved white front, oak top, the brand behind on the room fascia
import bmesh as _bm
ring('reception', 20.0, 18.5, 3.6, 4.1, math.radians(200), math.radians(340), 0.42, 1.12, desk_w, 48)
ring('reception_top', 20.0, 18.5, 3.5, 4.25, math.radians(198), math.radians(342), 1.12, 1.17, wood, 48)
box('reception_screen', (0.6, 0.04, 0.38), (20.0, 15.0, 1.42), screen, rot=0.0)
box('reception_screen2', (0.6, 0.04, 0.38), (18.4, 15.4, 1.42), screen, rot=-0.4)
text('reception_sign', 'Móttaka · Reception', (20.0, 14.35, 0.85), 0.16, chrome, depth=0.01)
# showroom cars: one on a turntable by the entrance corner, the rest angled across the floor
cyl('turntable', 3.2, 0.12, (11.0, 7.0, 0.48), mat('turntable', (0.18, 0.18, 0.2), 0.15, 0.6, coat=1.0), 64)
cyl('turntable_light', 3.25, 0.03, (11.0, 7.0, 0.43), strip, 64)
car2('car_hero', (11.0, 7.0, 0.54), math.radians(25), (0.55, 0.02, 0.03), c='Building')
# the turntable turns the hero car round and round: one full turn every 30 seconds, looping forever
ring('turntable_trim', 11.0, 7.0, 3.05, 3.2, 0, 2 * math.pi, 0.43, 0.56, chrome, 96)
for k in range(8):
    a = k * math.pi / 4; box('turntable_inlay', (2.6, 0.03, 0.005), (11.0 + 1.5 * math.cos(a), 7.0 + 1.5 * math.sin(a), 0.545), chrome, rot=a)
pivot = bpy.data.objects.new('Turntable_Pivot', None); col('Building').objects.link(pivot); pivot.location = (11.0, 7.0, 0.0)
bpy.context.view_layer.update()
for nm in ('turntable', 'car_hero') + tuple(o.name for o in bpy.data.objects if o.name.startswith('turntable_inlay')):
    o = bpy.data.objects[nm]; mw = o.matrix_world.copy(); o.parent = pivot; o.matrix_world = mw
sc_ = bpy.context.scene; sc_.render.fps = 24; sc_.frame_start, sc_.frame_end = 1, 720
pivot.rotation_euler = (0, 0, 0); pivot.keyframe_insert('rotation_euler', index=2, frame=1)
pivot.rotation_euler = (0, 0, 2 * math.pi); pivot.keyframe_insert('rotation_euler', index=2, frame=721)
try:
    fc = pivot.animation_data.action.fcurves[0]
except Exception:
    fc = None
    for lay in pivot.animation_data.action.layers:
        for st in lay.strips:
            for cb in st.channelbags: fc = cb.fcurves[0]
if fc:
    for kp in fc.keyframe_points: kp.interpolation = 'LINEAR'
    fc.modifiers.new('CYCLES')
sc_.frame_set(1)
for i, (x, y, r, colr, kind) in enumerate([(21.0, 6.5, -20, (0.85, 0.86, 0.88), 'suv'), (29.0, 8.0, -30, (0.03, 0.05, 0.12), 'sedan'),
                                          (38.0, 10.5, -40, (0.5, 0.52, 0.55), 'suv'), (25.0, 12.8, 160, (0.9, 0.9, 0.9), 'sedan'),
                                          (33.5, 13.0, 155, (0.12, 0.2, 0.32), 'suv')]):
    car2(f'car_in{i}', (x, y, 0.42), math.radians(r), colr, c='Building', kind=kind)
    box('price_stand', (0.04, 0.4, 1.1), (x + 2.6 * math.cos(math.radians(r) + 1.2), y + 2.6 * math.sin(math.radians(r) + 1.2), 0.97), alu)
# interior fill light (soft, like the ceiling fixtures together)
for x in (8, 20, 32):
    area_light('show_fill', (x, 9.0, H - 0.25), 8.0, 1600, color=(1, 0.95, 0.88))
area_light('lounge_fill', (5.5, 21.5, ROOM_H + 0.3), 4.0, 220); area_light('wc_fill', (14.0, 21.5, ROOM_H + 0.3), 3.0, 80)
area_light('rec_fill', (20, 17.0, ROOM_H + 0.5), 3.0, 120)

# ── workshop (staff only) in the service wing ──
box('ws_floor', (SX1 - SX0 - 0.4, SY1 - SY0 - 0.4, 0.02), ((SX0 + SX1) / 2, (SY0 + SY1) / 2, 0.02), epoxy)
for i, y in enumerate((9.0, 15.0, 21.0)):
    for dx in (-1.6, 1.6):
        box('lift_post', (0.4, 0.5, 3.6), (60.0 + dx, y, 1.8), lift_y)
    box('lift_beam', (3.6, 0.3, 0.2), (60.0, y, 3.7), lift_y)
    for z in range(1, 3): cyl('ws_strip', 0.04, 18.0, (54.0, 4.8 + i * 0.01 + y - y, 6.6), strip, 8, rot=(0, math.pi / 2, 0)) if False else None
car2('car_lift', (60.0, 15.0, 1.6), math.pi / 2, (0.1, 0.25, 0.15), c='Building', kind='suv')
for x in (45.0, 48.0): box('tool_chest', (1.6, 0.6, 1.1), (x, SY1 - 0.8, 0.57), red, bevel=0.02)
for y in (8.0, 14.0, 20.0, 26.0): box('ws_light', (12.0, 0.2, 0.06), (55.0, y, SH - 0.2), strip)
area_light('ws_fill', (55.0, 17.0, SH - 0.4), 10.0, 900, color=(0.95, 0.98, 1.0))
# staff room behind the service wing's front window
box('staff_room_wall', (12.0, 0.15, 3.2), (50.0, 8.5, 1.6), white)
box('staff_table', (2.0, 1.0, 0.05), (50.0, 6.0, 0.75), wood); [box('staff_chair', (0.45, 0.45, 0.45), (49.2 + 0.8 * k, 5.2, 0.45), fabric2, bevel=0.05) for k in range(3)]
area_light('staffroom_light', (50.0, 6.2, 3.0), 2.5, 120)

# ── the site: forecourt, lot, planting, lamps ──
box('asphalt', (140, 70, 0.1), (25, -24, -0.05), asph, c='Site')
box('lawn', (3000, 3000, 0.08), (25, 30, -0.1), lawn, c='Site')
box('front_walk', (W + 8, 3.0, 0.15), (W / 2 - 2, -2.8, 0.075), conc, c='Site')
box('bed', (W + 6, 2.0, 0.12), (W / 2 - 1, -5.3, 0.06), gravel, c='Site')
box('kerb', (W + 6, 0.2, 0.18), (W / 2 - 1, -6.4, 0.09), plinth, c='Site')
box('west_bed', (6.0, D + 6, 0.12), (-8.5, D / 2, 0.06), gravel, c='Site')
for x in range(-2, int(W + 2), 3): bpy.ops.mesh.primitive_ico_sphere_add(radius=0.38, subdivisions=2, location=(x + 0.7, -5.3, 0.38)); o = bpy.context.active_object; o.scale = (1, 1, 0.8); o.data.materials.append(leaf); [cc.objects.unlink(o) for cc in o.users_collection]; col('Site').objects.link(o)
for y in range(0, int(D), 3): bpy.ops.mesh.primitive_ico_sphere_add(radius=0.42, subdivisions=2, location=(-8.0, y + 1, 0.42)); o = bpy.context.active_object; o.data.materials.append(leaf); [cc.objects.unlink(o) for cc in o.users_collection]; col('Site').objects.link(o)
for x in (-3.0, 10.0, 22.0, 34.0):
    cyl('bollard', 0.1, 1.0, (x, -4.2, 0.5), alu_dark, 16, c='Site'); cyl('bollard_light', 0.1, 0.12, (x, -4.2, 0.9), glowsign, 16, c='Site')
for x in (24.0, 50.0, 70.0):
    cyl('pole', 0.08, 8.0, (x, -16.0, 4.0), alu_dark, 12, c='Site'); box('pole_head', (1.4, 0.4, 0.15), (x, -16.0, 8.0), alu_dark, c='Site')
# parking: two rows of stock and customer cars, white lines
for r, (y0, flip) in enumerate(((-11.5, 0), (-21.5, 1))):
    for k in range(-4, 22):
        x = -6.0 + k * 2.7
        box('bay_line', (0.12, 5.0, 0.01), (x, y0, 0.01), paintw, c='Site')
for k, (x, y, cl, kind) in enumerate([(-2.0, -11.5, (0.85, 0.85, 0.86), 'sedan'), (0.7, -11.5, (0.1, 0.1, 0.11), 'suv'), (6.1, -11.5, (0.55, 0.02, 0.03), 'sedan'),
                                      (11.5, -11.5, (0.45, 0.47, 0.5), 'suv'), (14.2, -11.5, (0.9, 0.9, 0.9), 'suv'), (22.3, -11.5, (0.04, 0.08, 0.2), 'sedan'),
                                      (27.7, -11.5, (0.75, 0.75, 0.77), 'sedan'), (33.1, -11.5, (0.15, 0.15, 0.16), 'suv'), (38.5, -11.5, (0.6, 0.1, 0.08), 'suv'),
                                      (3.4, -21.5, (0.8, 0.8, 0.82), 'suv'), (8.8, -21.5, (0.05, 0.05, 0.06), 'sedan'), (19.6, -21.5, (0.3, 0.35, 0.42), 'sedan'),
                                      (30.4, -21.5, (0.92, 0.92, 0.92), 'suv')]):
    car2(f'car_lot{k}', (x, y, 0.0), math.pi / 2 if y > -15 else -math.pi / 2, cl, kind=kind)
# pylon sign at the road (fictional brand)
PYX, PYY = -22.0, -4.0
box('pylon', (2.0, 0.6, 6.5), (PYX, PYY, 3.25), fascia2, c='Site')
badge(PYX, PYY - 0.35, 5.2, 0.9); text('pylon_brand', 'VELDRA', (PYX, PYY - 0.35, 3.6), 0.48, chrome, depth=0.04, c='Site')
text('pylon_sub', 'Sala · Þjónusta', (PYX, PYY - 0.35, 2.8), 0.22, chrome, depth=0.02, c='Site')

# ── light, camera ──
sky(sun_elev=24, sun_rot=210, strength=1.0)
sun = bpy.data.objects['sun']
az, el = math.radians(215), math.radians(24)
d = -Vector((math.cos(el) * math.sin(az), math.cos(el) * math.cos(az), math.sin(el)))
sun.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
if view == 'ext':
    camera((-15.0, -27.0, 4.2), (17.0, 5.0, 6.5), lens=22)
    out = f'{OUT}/dealer_exterior.png'
elif view == 'int':
    camera((35.0, 1.4, 1.7), (15.0, 18.0, 2.4), lens=18)
    out = f'{OUT}/dealer_interior.png'
else:
    camera((-15.0, -27.0, 4.2), (17.0, 5.0, 6.5), lens=22); out = f'{OUT}/dealer_check.png'
if view == 'checkint': camera((39.0, 1.6, 1.75), (14.0, 18.5, 3.0), lens=18); out = f'{OUT}/dealer_checkint.png'
if view == 'export':
    bpy.ops.file.pack_all(); bpy.ops.wm.save_as_mainfile(filepath=f'{OUT}/VELDRA-dealership.blend')
    for o in bpy.context.scene.objects: o.select_set(False)
    for o in col('Building').all_objects: o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=f'{OUT}/VELDRA-dealership.glb', use_selection=True, export_apply=True)
else:
    res = (1920, 1080) if view in ('ext', 'int') else (640, 360)
    render(out, res=res, samples=samples)
print('DONE', view)
