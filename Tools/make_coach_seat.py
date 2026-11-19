# The airport express coach's passenger seat pair (owner's blueprint + wireframe sheet, 2026-11-19:
# Art/Reference/From-Phone/2026-11-19/Coach-Seat-*.png), our own model made here in Blender, nothing bought or copied.
#   0.98 m pair (two 0.41 m seats, three armrests), 0.80 m pitch, cushion top 0.45 m with a waterfall front edge,
#   backrest top 1.22 m, headrest top 1.36 m (folding wings + linen band), armrests 0.65 m (0.40 long, aisle one lifts),
#   back reclined 15 deg, swept cast-aluminium aisle leg (0.375 foot, 0.338 to the tube axis, waist + open cut-out) on a
#   floor track (foot plate, 3 bolts), the wall side on the side-wall rail by a bracket; back shell with fold-down table
#   + latch + cup holder, magazine pocket, USB-C + USB-A, coat hook, ambient light strip, seat numbers, grab handle at
#   the aisle corner; lap-belt buckles; folding footrests for the passenger behind.
# Axes as the other seats (make_gate_seat.py): x across the pair (wall at -x, aisle at +x), -y the way a seated person
# faces, z up; metres; origin on the floor under the pair's middle, at the cushion's back edge line.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_coach_seat.py [-- --render]
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector, Matrix

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, 'Art', 'CoachSeat'); os.makedirs(ART, exist_ok=True)
SHOTS = os.path.join(ROOT, 'docs', 'Pictures', 'CoachSeat'); os.makedirs(SHOTS, exist_ok=True)
RENDER = '--render' in sys.argv or '--quick' in sys.argv
QUICK = '--quick' in sys.argv
bpy.ops.wm.read_factory_settings(use_empty=True)
rng = np.random.default_rng(7)

# ---------------------------------------------------------------- textures (our own, made here)
def blur(a, n=1):
    for _ in range(n): a = (a + np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 5
    return a
def save_img(name, rgb, nonc=False):
    h, w = rgb.shape[:2]
    im = bpy.data.images.new(name, w, h, alpha=False)
    px = np.ones((h, w, 4), np.float32); px[..., :3] = np.clip(rgb, 0, 1)
    im.pixels = px.ravel(); im.filepath_raw = os.path.join(ART, name + '.png'); im.file_format = 'PNG'; im.save()
    if nonc: im.colorspace_settings.name = 'Non-Color'
    return im
def normal_from(hgt, k):
    gy, gx = np.gradient(hgt)
    n = np.dstack((-gx * k, -gy * k, np.ones_like(hgt))); n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n * 0.5 + 0.5
N = 512
yy, xx = np.mgrid[0:N, 0:N] / N
def weave(f1, f2):
    return 0.5 + 0.25 * np.sin(xx * math.pi * 2 * f1) * np.sin(yy * math.pi * 2 * f2) + 0.25 * np.sin((xx + yy) * math.pi * 2 * f1 * 0.5)
def moquette(base, fleck, name, quilt=False):
    h = weave(96, 96) * 0.6 + blur(rng.random((N, N)), 2) * 0.4
    if quilt: h = h - 1.2 * np.exp(-((((yy * 5) % 1) - 0.5) / 0.035) ** 2)   # stitched grooves every 5 cm
    # a small woven figure (diamonds) as coach moquettes have
    d = (np.abs(((xx * 16) % 1) - 0.5) + np.abs(((yy * 16) % 1) - 0.5)) < 0.18
    col = np.array(base)[None, None, :] * (0.82 + 0.3 * h[..., None])
    col = np.where(d[..., None], col * 0.75 + np.array(fleck)[None, None, :] * 0.25, col)
    return save_img(name, col), save_img(name + '_N', normal_from(h * 1.0 + d * 0.4, 6.0), True)
T_MOQ = moquette((0.035, 0.09, 0.24), (0.25, 0.75, 0.80), 'T_Coach_Moquette')
T_MOQC = moquette((0.09, 0.22, 0.48), (0.35, 0.60, 0.90), 'T_Coach_MoquetteCentre', True)
def plain(name, base, grain, k=3.0, streak=False):
    g = blur(rng.random((N, N)), 1)
    if streak: g = 0.6 * blur(np.repeat(rng.random((N, 1)), N, 1), 1) + 0.4 * g
    col = np.array(base)[None, None, :] * (1 - grain + 2 * grain * g[..., None])
    return save_img(name, col), save_img(name + '_N', normal_from(g, k), True)
T_SHELL = plain('T_Coach_Shell', (0.045, 0.09, 0.19), 0.06, 2.0)
T_ALU = plain('T_Coach_CastAlu', (0.72, 0.76, 0.80), 0.07, 2.5)
T_STEEL = plain('T_Coach_Steel', (0.62, 0.64, 0.66), 0.08, 1.5, True)
T_RUB = plain('T_Coach_Rubber', (0.03, 0.03, 0.035), 0.10, 4.0)
T_BLACK = plain('T_Coach_BlackPlastic', (0.02, 0.022, 0.026), 0.05, 2.0)
T_LIN = moquette((0.42, 0.41, 0.38), (0.5, 0.5, 0.47), 'T_Coach_Linen')
def net():
    h = ((np.abs(((xx * 40) % 1) - 0.5) < 0.12) | (np.abs(((yy * 40) % 1) - 0.5) < 0.12)).astype(float)
    return save_img('T_Coach_Pocket', np.dstack([0.03 + 0.05 * h, 0.06 + 0.08 * h, 0.12 + 0.12 * h])), save_img('T_Coach_Pocket_N', normal_from(blur(h, 1), 4.0), True)
T_NET = net()
T_WHITE = plain('T_Coach_White', (0.92, 0.94, 0.96), 0.02, 0.5)
T_FLOOR = plain('T_Coach_FloorVinyl', (0.16, 0.17, 0.19), 0.12, 2.0)

def mat(name, tex, metal, rough, emit=None, strength=0.0, tile=0.25):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    ti = nt.nodes.new('ShaderNodeTexImage'); ti.image = tex[0]; nt.links.new(ti.outputs['Color'], b.inputs['Base Color'])
    tn = nt.nodes.new('ShaderNodeTexImage'); tn.image = tex[1]
    nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.6
    nt.links.new(tn.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], b.inputs['Normal'])
    b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if emit:
        b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    return m
M_MOQ = mat('Coach_Moquette', T_MOQ, 0, 0.92)
M_MOQC = mat('Coach_MoquetteCentre', T_MOQC, 0, 0.90)
M_SHELL = mat('Coach_Shell', T_SHELL, 0, 0.45)
M_ALU = mat('Coach_CastAlu', T_ALU, 1, 0.38)
M_STEEL = mat('Coach_Steel', T_STEEL, 1, 0.30)
M_RUB = mat('Coach_Rubber', T_RUB, 0, 0.85)
M_BLACK = mat('Coach_BlackPlastic', T_BLACK, 0, 0.50)
M_LIN = mat('Coach_Linen', T_LIN, 0, 0.95)
M_NET = mat('Coach_Pocket', T_NET, 0, 0.90)
M_LIGHT = mat('Coach_AmbientLight', T_WHITE, 0, 0.3, (0.25, 0.95, 0.85), 6.0)
M_LABEL = mat('Coach_Label', T_WHITE, 0, 0.4, (0.9, 0.95, 1.0), 0.6)

# ---------------------------------------------------------------- helpers
ALL = []
def finish(o, m, smooth=60):
    o.data.materials.clear(); o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(smooth)
    ALL.append(o); return o
def link(name, bm):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(o); return o
def apply_mods(o):
    bpy.context.view_layer.objects.active = o; o.select_set(True)
    for md in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=md.name)
    o.select_set(False)
def ss(e0, e1, t):
    t = min(max((t - e0) / (e1 - e0), 0.0), 1.0); return t * t * (3 - 2 * t)

def rblock(name, c, s, r, m, cuts=8, segs=4, sub=1, deform=None, smooth=60):
    """A rounded block (centre c, size s, edge radius r), quads throughout; deform(co, u, v, w) after (u, v, w in -1..1
    across the block as it was made)."""
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    for v in bm.verts: v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
    o = link(name, bm)
    if r > 0:
        md = o.modifiers.new('b', 'BEVEL'); md.width = r; md.segments = segs; md.limit_method = 'ANGLE'; md.angle_limit = math.radians(30)
    if sub: md = o.modifiers.new('s', 'SUBSURF'); md.levels = sub; md.render_levels = sub
    apply_mods(o)
    if deform:
        for v in o.data.vertices:
            u, vv, w = ((v.co.x - c[0]) / (s[0] / 2), (v.co.y - c[1]) / (s[1] / 2), (v.co.z - c[2]) / (s[2] / 2))
            v.co = deform(Vector(v.co), u, vv, w)
    return finish(o, m, smooth)

def tube(name, pts, r, m, res=10, bez=True, caps=True):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = r; cu.bevel_resolution = 3
    cu.use_fill_caps = caps; cu.resolution_u = res
    if bez:
        sp = cu.splines.new('BEZIER'); sp.bezier_points.add(len(pts) - 1)
        for p, q in zip(sp.bezier_points, pts): p.co = q; p.handle_left_type = p.handle_right_type = 'AUTO'
    else:
        sp = cu.splines.new('POLY'); sp.points.add(len(pts) - 1)
        for p, q in zip(sp.points, pts): p.co = (*q, 1)
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o)
    return to_mesh(o, m)
def to_mesh(o, m, smooth=60):
    bpy.context.view_layer.objects.active = o
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.ops.object.convert(target='MESH'); o = bpy.context.view_layer.objects.active; o.select_set(False)
    return finish(o, m, smooth)
def cyl(name, c, r, depth, axis, m, verts=24, bevel=0.0):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r, radius2=r, depth=depth)
    rot = {'x': Matrix.Rotation(math.pi / 2, 4, 'Y'), 'y': Matrix.Rotation(math.pi / 2, 4, 'X'), 'z': Matrix.Identity(4)}[axis]
    bmesh.ops.transform(bm, matrix=Matrix.Translation(c) @ rot, verts=bm.verts)
    o = link(name, bm)
    if bevel: md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = 2; md.limit_method = 'ANGLE'; apply_mods(o)
    return finish(o, m, 40)
def profile(name, outer, holes, thick, bevel, m, plane):
    """A flat cast part: an outline (with holes) in 2D, filled, thick, its edges rounded; plane(Vector2D) -> world."""
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '2D'; cu.fill_mode = 'BOTH'
    cu.extrude = thick / 2 - bevel; cu.bevel_depth = bevel; cu.bevel_resolution = 3; cu.resolution_u = 10
    for loop in [outer] + holes:
        sp = cu.splines.new('BEZIER'); sp.bezier_points.add(len(loop) - 1); sp.use_cyclic_u = True
        for p, q in zip(sp.bezier_points, loop):
            p.co = (q[0], q[1], 0); p.handle_left_type = p.handle_right_type = ('VECTOR' if len(q) > 2 else 'AUTO')
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o)
    o = to_mesh(o, m, 35)
    for v in o.data.vertices: v.co = plane(v.co)
    return o
def text(name, body, c, size, m, rot):
    cu = bpy.data.curves.new(name, 'FONT'); cu.body = body; cu.size = size; cu.extrude = 0.0004; cu.align_x = 'CENTER'; cu.align_y = 'CENTER'
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o)
    o = to_mesh(o, m, 30); o.data.transform(Matrix.Translation(c) @ rot); return o

# ---------------------------------------------------------------- the pair
SEATS = (-0.235, 0.235)          # the two seats' middles (wall seat A at -x, aisle seat B at +x)
CF, CB, CTOP = -0.50, -0.04, 0.45  # cushion front, back, top
TUBE_Z, TUBE_Y = 0.338, (-0.377, -0.127)
LEG_X = 0.452                    # the aisle leg (under the aisle armrest)
RECLINE = math.radians(15)

def cushion_deform(xc):
    def f(co, u, v, w):
        top = ss(-1.0, 1.0, w)                                    # 0 at the pan, 1 on top
        co.z += top * (0.007 * (1 - u * u) + 0.014 * ss(0.55, 0.95, abs(u)) - 0.006 * ss(0.3, 1.0, v))   # crown, bolsters, rear dip
        fr = ss(-0.55, -1.0, v)                                   # the waterfall front: rolls down over the edge
        co.z -= fr * fr * 0.022 * (0.4 + 0.6 * top)
        co.y -= fr * 0.006 * (1 - top)
        return co
    return f
for k, xc in enumerate(SEATS):
    sd = 'AB'[k]
    rblock(f'Cushion_{sd}', (xc, (CF + CB) / 2, 0.412), (0.41, CB - CF, 0.076), 0.028, M_MOQ, cuts=10, segs=4, sub=1, deform=cushion_deform(xc))
    rblock(f'CushionInset_{sd}', (xc, CF - 0.001, 0.396), (0.30, 0.008, 0.028), 0.006, M_SHELL, cuts=2, segs=3, sub=0)
    rblock(f'CushionPan_{sd}', (xc, (CF + CB) / 2 + 0.01, 0.366), (0.42, 0.44, 0.016), 0.006, M_SHELL, cuts=2, segs=2, sub=0)
    # lap belt: a buckle on the inner side, the tongue's anchor on the outer, at the cushion's back
    for s, mm in ((-1, M_STEEL), (1, M_BLACK)):
        x = xc + s * 0.175 * (1 if k else -1)
        rblock(f'Belt_{sd}{"ab"[s > 0]}', (x, CB - 0.02, 0.463), (0.028, 0.012, 0.05), 0.004, mm, cuts=1, segs=2, sub=0)
        rblock(f'BeltStrap_{sd}{"ab"[s > 0]}', (x, CB - 0.005, 0.44), (0.024, 0.004, 0.05), 0.001, M_BLACK, cuts=1, segs=1, sub=0)

# the frame: two cross tubes on the aisle leg and the wall bracket, longitudinal members under each seat end
for j, y in enumerate(TUBE_Y):
    cyl(f'CrossTube_{j}', Vector((-0.02, y, TUBE_Z)), 0.019, 1.03, 'x', M_STEEL, 28)
    for x in (-0.535, 0.495): cyl(f'TubeCap_{j}{x > 0}', Vector((x, y, TUBE_Z)), 0.0195, 0.006, 'x', M_BLACK, 28, 0.002)
for i, x in enumerate((-0.47, 0.0, 0.43)):
    rblock(f'SideMember_{i}', (x, -0.25, 0.352), (0.026, 0.33, 0.034), 0.005, M_STEEL, cuts=2, segs=2, sub=0)

# the aisle leg: the blueprint's profile (A), s forward from the foot's back, z up; thickest at the foot and the top
L = 0.00147
def px(x, y, sharp=False): return ((x - 698) * L, (1045 - y) * L) + ((1,) if sharp else ())
LEG_OUT = [px(672, 1045, 1), px(706, 1028), px(722, 990), px(735, 930), px(740, 872), px(724, 830), px(726, 806),
           px(750, 796), px(800, 808), px(860, 813), px(900, 800), px(935, 806), px(955, 822, 1), px(925, 848), px(882, 900),
           px(860, 958), px(872, 1006), px(915, 1027), px(980, 1045, 1)]
LEG_HOLE = [px(792, 850), px(876, 846), px(858, 884), px(818, 935), px(780, 988), px(764, 962), px(768, 905)]
S0 = 0.059 + (TUBE_Y[1])        # s of the rear boss -> world y
def leg_plane(co):
    s, z, t = co.x, co.y, co.z
    w = 1.0 - 0.38 * math.exp(-((z - 0.13) / 0.08) ** 2)          # the waist: 62 % as thick
    return Vector((LEG_X + t * w, TUBE_Y[1] - (s - 0.059), z))
profile('AisleLeg', LEG_OUT, [LEG_HOLE], 0.046, 0.009, M_ALU, leg_plane)
for j, y in enumerate(TUBE_Y):   # the bosses round the tubes
    cyl(f'LegBoss_{j}', Vector((LEG_X, y, TUBE_Z)), 0.03, 0.054, 'x', M_ALU, 32, 0.004)
    cyl(f'LegBossBolt_{j}', Vector((LEG_X + 0.029, y, TUBE_Z)), 0.011, 0.006, 'x', M_STEEL, 6, 0.001)

# floor track (an 0.80 m length, one pitch) and the leg's foot plate with 3 bolts
rblock('FloorTrack', (LEG_X, -0.25, 0.006), (0.064, 0.80, 0.012), 0.003, M_ALU, cuts=2, segs=2, sub=0)
rblock('FloorTrackSlot', (LEG_X, -0.25, 0.0121), (0.016, 0.80, 0.0004), 0.0, M_BLACK, cuts=1, segs=1, sub=0)
for i in range(16): cyl(f'TrackHole_{i}', Vector((LEG_X, -0.25 - 0.375 + i * 0.05, 0.0122)), 0.009, 0.0006, 'z', M_BLACK, 16)
foot_y0, foot_y1 = TUBE_Y[1] - (-0.03 - 0.059), TUBE_Y[1] - (0.41 - 0.059)
rblock('FootPlate', (LEG_X, (foot_y0 + foot_y1) / 2, 0.016), (0.07, abs(foot_y0 - foot_y1) + 0.01, 0.008), 0.003, M_ALU, cuts=2, segs=2, sub=0)
for i, t in enumerate((0.12, 0.5, 0.88)):
    yb = foot_y0 + (foot_y1 - foot_y0) * t
    cyl(f'FootBolt_{i}', Vector((LEG_X, yb, 0.0225)), 0.008, 0.006, 'z', M_STEEL, 6, 0.001)

# the wall side: the side-wall rail (along the coach) and the bracket from it to the tubes
cyl('WallRail', Vector((-0.565, -0.25, 0.30)), 0.021, 0.80, 'y', M_STEEL, 28)
rblock('WallRailFlat', (-0.585, -0.25, 0.30), (0.012, 0.80, 0.05), 0.003, M_ALU, cuts=2, segs=2, sub=0)
for j, y in enumerate(TUBE_Y):
    tube(f'WallBracket_{j}', [(-0.47, y, TUBE_Z), (-0.515, y, TUBE_Z - 0.003), (-0.545, y, 0.318), (-0.556, y, 0.30)], 0.015, M_ALU, res=10)
    cyl(f'WallClamp_{j}', Vector((-0.565, y, 0.30)), 0.029, 0.05, 'y', M_ALU, 32, 0.004)
    cyl(f'WallClampBolt_{j}', Vector((-0.565, y, 0.331)), 0.007, 0.008, 'z', M_STEEL, 6, 0.001)

# armrests: wall, middle, aisle (the aisle one lifts 80 deg on its pivot); pads 0.40 long, top 0.65
for i, x in enumerate((-0.468, 0.0, 0.468)):
    rblock(f'ArmSlot_{i}{0}', (x - 0.0285, -0.24, 0.624), (0.003, 0.28, 0.016), 0.003, M_BLACK, cuts=2, segs=2, sub=0)
    rblock(f'ArmSlot_{i}{1}', (x + 0.0285, -0.24, 0.624), (0.003, 0.28, 0.016), 0.003, M_BLACK, cuts=2, segs=2, sub=0)
    rblock(f'Armrest_{i}', (x, -0.235, 0.624), (0.056, 0.40, 0.056), 0.02, M_SHELL if i != 1 else M_SHELL, cuts=6, segs=4, sub=1,
           deform=lambda co, u, v, w: co + Vector((0, 0, 0.004 * (1 - v * v) * ss(-1, 1, w))))
    rblock(f'ArmPad_{i}', (x, -0.25, 0.651), (0.040, 0.30, 0.006), 0.003, M_RUB, cuts=4, segs=2, sub=1)
    cyl(f'ArmPivot_{i}', Vector((x, -0.035, 0.625)), 0.026, 0.058, 'x', M_BLACK, 32, 0.004)
    cyl(f'ArmPivotCap_{i}', Vector((x + 0.0295 * (1 if x >= 0 else -1), -0.035, 0.625)), 0.016, 0.003, 'x', M_STEEL, 32)
rblock('ArmTipLight', (0.468, -0.43, 0.6505), (0.026, 0.012, 0.002), 0.0008, M_LIGHT, cuts=1, segs=1, sub=0)
rblock('ArmLiftButton', (0.468 + 0.0265, -0.08, 0.628), (0.003, 0.018, 0.012), 0.001, M_BLACK, cuts=1, segs=1, sub=0)

# ---------------------------------------------------------------- the backs (made upright, then reclined)
BACK = []
def back_frame():
    # pivot at the cushion's back, 0.46 m up; local y = 0 the backrest's face, +y behind
    return Matrix.Translation((0, CB + 0.01, 0.46)) @ Matrix.Rotation(-RECLINE, 4, 'X')
for k, xc in enumerate(SEATS):
    sd = 'AB'[k]
    def bdef(co, u, v, w):
        front = ss(0.6, -1.0, v)
        co.y -= front * (0.032 * ss(0.55, 0.95, abs(u)) + 0.032 * math.exp(-((co.z - 0.20) / 0.13) ** 2))   # bolsters, lumbar
        co.y += front * (0.022 * ss(0.35, 1.0, w) + 0.006 * ss(0.9, 1.0, w))   # thinner towards the top (owner's side view), rolled back
        return co
    def bdef2(co, u, v, w, bdef=bdef):
        co = bdef(co, u, v, w)
        cz = ss(0.5, 0.38, abs(u)) * ss(0.12, 0.18, co.z) * ss(0.70, 0.64, co.z)
        co.y += ss(0.6, -1.0, v) * 0.004 * cz                 # the centre panel sits a little in
        return co
    br = rblock(f'Backrest_{sd}', (xc, 0.045, 0.41), (0.40, 0.09, 0.78), 0.03, M_MOQ, cuts=14, segs=4, sub=1, deform=bdef2)
    br.data.materials.append(M_MOQC)
    for p in br.data.polygons:
        c = p.center
        if p.normal.y < -0.5 and abs(c.x - xc) < 0.118 and 0.115 < c.z < 0.69: p.material_index = 1
    BACK.append(br)
    for s_ in (-1, 1):   # the dark insets on the bolsters' sides
        BACK.append(rblock(f'BolsterInset_{sd}{s_ > 0}', (xc + s_ * 0.2005, 0.045, 0.43), (0.004, 0.045, 0.40), 0.004, M_SHELL, cuts=2, segs=2, sub=0))
    # the back shell behind, a hair wider than the backrest
    def shdef(co, u, v, w):
        co.y += 0.012 * (1 - u * u) * ss(1.0, -0.2, v) * (1 - w * w)      # a gentle bulge on the back
        return co
    BACK.append(rblock(f'BackShell_{sd}', (xc, 0.103, 0.395), (0.418, 0.028, 0.82), 0.014, M_SHELL, cuts=10, segs=3, sub=1, deform=shdef))
    YB = 0.117 + 0.012      # the back's rear face (with its bulge) where things sit
    # fold-down table (closed, flush in its recess) + latch + cup holder
    BACK.append(rblock(f'TableRecess_{sd}', (xc, YB - 0.004, 0.53), (0.31, 0.004, 0.235), 0.01, M_BLACK, cuts=2, segs=2, sub=0))
    BACK.append(rblock(f'Table_{sd}', (xc, YB + 0.001, 0.53), (0.30, 0.008, 0.225), 0.008, M_SHELL, cuts=4, segs=3, sub=1))
    BACK.append(rblock(f'TableLatch_{sd}', (xc, YB + 0.006, 0.655), (0.05, 0.010, 0.016), 0.004, M_STEEL, cuts=1, segs=2, sub=0))
    for a in range(1):
        BACK.append(tube(f'CupRing_{sd}', [(xc + 0.09 + 0.032 * math.cos(t * math.pi / 8), YB + 0.006, 0.585 + 0.032 * math.sin(t * math.pi / 8)) for t in range(17)], 0.0025, M_STEEL, res=2, bez=False))
    # magazine pocket: a net panel bulging out, an elastic top band
    BACK.append(rblock(f'Pocket_{sd}', (xc, YB + 0.004, 0.235), (0.30, 0.018, 0.25), 0.012, M_NET, cuts=6, segs=3, sub=1,
                       deform=lambda co, u, v, w: co + Vector((0, 0.010 * (1 - u * u) * ss(1.0, -1.0, w) * ss(-0.5, 1.0, v), 0))))
    BACK.append(cyl(f'PocketBand_{sd}', Vector((xc, YB + 0.014, 0.358)), 0.006, 0.30, 'x', M_BLACK, 12))
    # USB-C + USB-A and the coat hook; the ambient light strip and the seat number
    BACK.append(rblock(f'UsbPlate_{sd}', (xc + 0.02, YB + 0.003, 0.705), (0.07, 0.006, 0.024), 0.004, M_BLACK, cuts=1, segs=2, sub=0))
    for j, (dx, w_) in enumerate(((0.0, 0.009), (0.025, 0.013))):
        BACK.append(rblock(f'UsbPort_{sd}{j}', (xc + 0.008 + dx, YB + 0.0062, 0.705), (w_, 0.001, 0.005), 0.0005, M_STEEL, cuts=1, segs=1, sub=0))
    BACK.append(tube(f'CoatHook_{sd}', [(xc - 0.10, YB, 0.72), (xc - 0.10, YB + 0.022, 0.718), (xc - 0.10, YB + 0.03, 0.70), (xc - 0.10, YB + 0.02, 0.688)], 0.005, M_STEEL, res=6))
    BACK.append(rblock(f'LightStrip_{sd}', (xc, YB - 0.0005, 0.765), (0.24, 0.006, 0.007), 0.002, M_LIGHT, cuts=2, segs=1, sub=0))
    BACK.append(text(f'SeatNo_{sd}', f'12 {sd}', Vector((xc, YB + 0.0005, 0.745)), 0.016, M_LABEL, Matrix.Rotation(-math.pi / 2, 4, 'X') @ Matrix.Rotation(math.pi, 4, 'Z')))
    # headrest: folding wings, a linen band across the top, on two chrome posts
    def hdef(co, u, v, w):
        wing = ss(0.42, 1.0, abs(u))
        co.y -= wing * 0.04; co.x -= math.copysign(wing * 0.012, u)
        co.y -= 0.008 * (1 - u * u) * ss(0.5, -1.0, v)
        return co
    BACK.append(rblock(f'Headrest_{sd}', (xc, 0.03, 0.88), (0.31, 0.10, 0.15), 0.035, M_MOQ, cuts=8, segs=4, sub=1, deform=hdef))
    for s_ in (-1, 1):
        BACK.append(cyl(f'HeadCap_{sd}{s_ > 0}', Vector((xc + s_ * 0.141, -0.004, 0.88)), 0.042, 0.006, 'x', M_SHELL, 32, 0.002))
    BACK.append(rblock(f'LinenBand_{sd}', (xc, -0.023, 0.918), (0.20, 0.006, 0.045), 0.003, M_LIN, cuts=6, segs=2, sub=1,
                       deform=lambda co, u, v, w: co + Vector((0, -0.004 * (1 - u * u), 0))))
    for j, dx in enumerate((-0.055, 0.055)):
        BACK.append(cyl(f'HeadPost_{sd}{j}', Vector((xc + dx, 0.05, 0.80)), 0.0065, 0.04, 'z', M_STEEL, 16))
        BACK.append(cyl(f'PostSleeve_{sd}{j}', Vector((xc + dx, 0.05, 0.795)), 0.011, 0.012, 'z', M_BLACK, 16, 0.002))
# the grab handle at the aisle corner (on the aisle seat's back)
BACK.append(tube('GrabHandle', [(0.205, 0.125, 0.80), (0.215, 0.165, 0.825), (0.215, 0.17, 0.79), (0.215, 0.165, 0.68), (0.208, 0.125, 0.665)], 0.0105, M_STEEL, res=8))
BACK.append(cyl('GrabFootTop', Vector((0.206, 0.125, 0.80)), 0.015, 0.012, 'y', M_BLACK, 16, 0.002))
BACK.append(cyl('GrabFootLow', Vector((0.208, 0.125, 0.665)), 0.015, 0.012, 'y', M_BLACK, 16, 0.002))
# the grab handle sits on the aisle corner: move it out to the aisle edge of seat B
for o in BACK[-3:]: o.data.transform(Matrix.Translation((0.235, 0, 0)))
M = back_frame()
for o in BACK: o.data.transform(M)

# folding footrests for the passenger behind: one U frame under each seat, hung from the rear tube
for k, xc in enumerate(SEATS):
    sd = 'AB'[k]
    for s in (-1, 1):
        tube(f'FootrestArm_{sd}{s > 0}', [(xc + s * 0.12, TUBE_Y[1], TUBE_Z - 0.02), (xc + s * 0.12, TUBE_Y[1] + 0.12, 0.24), (xc + s * 0.12, TUBE_Y[1] + 0.30, 0.15)], 0.009, M_STEEL, res=8)
        cyl(f'FootrestHinge_{sd}{s > 0}', Vector((xc + s * 0.12, TUBE_Y[1], TUBE_Z - 0.02)), 0.016, 0.02, 'x', M_BLACK, 16, 0.002)
    cyl(f'FootrestBar_{sd}', Vector((xc, TUBE_Y[1] + 0.30, 0.15)), 0.0095, 0.26, 'x', M_STEEL, 16)
    cyl(f'FootrestGrip_{sd}', Vector((xc, TUBE_Y[1] + 0.30, 0.15)), 0.016, 0.20, 'x', M_RUB, 20, 0.003)

# ---------------------------------------------------------------- UVs: box-mapped at real size (0.25 m a tile)
for o in ALL:
    me = o.data
    if not me.uv_layers: me.uv_layers.new(name='UVMap')
    uv = me.uv_layers.active.data
    for p in me.polygons:
        n = p.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for li in p.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            a, b = [(co.y, co.z), (co.x, co.z), (co.x, co.y)][ax]
            uv[li].uv = (a / 0.25, b / 0.25)
    if not o.data.materials or o.data.materials[0] is None: raise SystemExit('empty material slot on ' + o.name)

# one object for the game (Nanite, instanced along the coach), parts kept by name in the .blend's collection
col = bpy.data.collections.new('CoachSeatParts'); bpy.context.scene.collection.children.link(col)
for o in ALL:
    for c in o.users_collection: c.objects.unlink(o)
    col.objects.link(o)
tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in ALL)
print('COACHSEAT: %d parts, %d triangles' % (len(ALL), tris))
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ART, 'CoachSeat.blend'))
bpy.ops.export_scene.gltf(filepath=os.path.join(ART, 'coachseat.glb'), export_format='GLB', use_selection=False, export_apply=True)

# ---------------------------------------------------------------- previews
if RENDER:
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    ee = sc.eevee; ee.use_gtao = True; ee.gtao_distance = 0.3; ee.use_ssr = True; ee.use_soft_shadows = True; ee.shadow_cube_size = '2048'; ee.taa_render_samples = 96
    ee.use_bloom = True; ee.bloom_intensity = 0.02
    sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'
    w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.035, 0.05, 0.08, 1)
    w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.6; sc.world = w
    # the coach's floor and side wall for scale
    fl = rblock('Floor', (0, -0.25, -0.01), (3.0, 3.0, 0.02), 0, mat('Coach_Floor', T_FLOOR, 0, 0.6), cuts=1, segs=1, sub=0)
    wl = rblock('Wall', (-0.62, -0.25, 1.0), (0.06, 3.0, 2.0), 0, mat('Coach_Wall', plain('T_Coach_WallPanel', (0.55, 0.58, 0.62), 0.04), 0, 0.5), cuts=1, segs=1, sub=0)
    for o in (fl, wl):
        uv = o.data.uv_layers.new().data
    def light(name, loc, energy, size, col=(1, 0.97, 0.92)):
        L_ = bpy.data.lights.new(name, 'AREA'); L_.energy = energy; L_.size = size; L_.color = col
        o = bpy.data.objects.new(name, L_); sc.collection.objects.link(o); o.location = loc
        o.rotation_euler = (Vector((0, -0.2, 0.7)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    light('Key', (1.6, -2.0, 2.6), 450, 1.5); light('Fill', (-0.4, -2.5, 1.2), 120, 2.0, (0.85, 0.9, 1.0))
    light('Rim', (1.0, 2.0, 2.2), 300, 1.5); light('Top', (0, -0.3, 2.4), 150, 1.0)
    cam_d = bpy.data.cameras.new('Cam'); cam = bpy.data.objects.new('Cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
    def shoot(name, loc, at, lens=50, ortho=0, res=(3840, 2160), hide_wall=False):
        cam.location = loc; cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        cam_d.type = 'ORTHO' if ortho else 'PERSP'; cam_d.lens = lens; cam_d.ortho_scale = ortho or 1
        wl.hide_render = hide_wall
        if QUICK: res = (res[0] // 3, res[1] // 3); ee.taa_render_samples = 16
        sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = os.path.join(SHOTS, name + '.png'); bpy.ops.render.render(write_still=True)
        print('COACHSEAT: picture', name)
    AT = (0.0, -0.1, 0.68)
    shoot('CoachSeat-1-front-aisle', (2.3, -2.7, 1.55), AT, 50)
    shoot('CoachSeat-2-rear-aisle', (2.0, 2.6, 1.6), AT, 50)
    shoot('CoachSeat-3-side', (3.5, -0.1, 0.70), (0, -0.1, 0.70), 50, 2.6, hide_wall=True)
    shoot('CoachSeat-4-front', (0, -3.6, 0.70), (0, -0.1, 0.70), 50, 2.6)
    shoot('CoachSeat-5-leg-closeup', (1.25, -0.75, 0.42), (0.45, -0.25, 0.22), 50)
    shoot('CoachSeat-6-back-closeup', (0.75, 1.2, 0.95), (0.1, 0.12, 0.75), 50)
    # the wireframe sheet's views: every mesh edge drawn (hidden lines removed), as the owner's sheet
    sc.render.use_freestyle = True; sc.render.line_thickness_mode = 'ABSOLUTE'; sc.render.line_thickness = 0.9
    fs = bpy.context.view_layer.freestyle_settings; fs.crease_angle = math.radians(179.5)
    ls = fs.linesets[0] if len(fs.linesets) else fs.linesets.new('All')
    ls.select_by_visibility = True; ls.visibility = 'VISIBLE'; ls.select_by_edge_types = True
    ls.select_crease = True; ls.select_silhouette = True; ls.select_border = True
    if ls.linestyle is None: ls.linestyle = bpy.data.linestyles.new('Wire')
    ls.linestyle.color = (0.82, 0.9, 1.0); ls.linestyle.thickness = 0.9
    flat = bpy.data.materials.new('WireBlue'); flat.use_nodes = True
    flat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.12, 0.3, 0.62, 1)
    bpy.context.view_layer.material_override = flat
    fl.hide_render = True
    shoot('CoachSeat-7-wire-front', (2.3, -2.7, 1.55), AT, 50, res=(2400, 1800), hide_wall=True)
    shoot('CoachSeat-8-wire-rear', (2.0, 2.6, 1.6), AT, 50, res=(2400, 1800), hide_wall=True)
