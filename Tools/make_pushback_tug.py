# The pushback tug (towbar type, A320/B737 class) - owner 2026-11-21: every vehicle made in Blender, blueprint = wireframe
# views of the real mesh, his OK before textures and the next vehicle. Rules: nothing floats (every part sits on, hangs on
# or is bolted to the part under it), curves only where the real tug curves (wheel arches, cab corners, steering wheel,
# tyres, seat), flat/chamfered steel everywhere else, lots of detail, a full connected interior with real sizes, real
# tug seats (an operator's suspension seat), IK points every time.
# Real figures (narrow-body tug spec sheet, Eagle XL-30 class): L 5.105, W 2.515, deck 1.321, with cab 2.134, wheelbase
# 2.311, track 2.108, tyres 315/80R22.5 (dia 1.076), step 0.533, ground clearance 0.228 (m); 3 seats, 4WD, 4WS.
# Shape from photos of real towbar tugs (Wikimedia Commons): long low slab body, wheels inside arches in the sides, the
# cab set into the left side between the axles with a sunken floor and wrap windows, chamfered corners, heavy bumpers
# with the lights set into them.
# Axes: x forward (front +x), y to the left, z up; metres; origin on the ground under the body's middle.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_pushback_tug.py [-- --wire]
import bpy, bmesh, math, os, sys
from mathutils import Vector, Matrix

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, 'Art', 'PushbackTug'); os.makedirs(ART, exist_ok=True)
WIRE = os.path.join(ROOT, 'Blueprints', 'Vehicles', 'PushbackTug', 'wire'); os.makedirs(WIRE, exist_ok=True)
DO_WIRE = '--wire' in sys.argv
bpy.ops.wm.read_factory_settings(use_empty=True)

L, W, DECK, CABH, WB, TR, TD, TW, GC = 5.105, 2.515, 1.321, 2.134, 2.311, 2.108, 1.076, 0.315, 0.228
FX, RX = WB / 2, -WB / 2                     # axle x
CFLOOR = 0.60                                # the cab floor, sunk into the body between the axles
CAB_X0, CAB_X1 = -0.56, 0.56                 # the cab's lower box (between the wheel arches)
CABU_X0, CABU_X1 = -0.66, 0.86               # its upper part (over the arches, from the deck up)
CAB_Y0, CAB_Y1 = W / 2 - 0.08 - 1.40, W / 2 - 0.08   # across: the left side, 1.40 wide outside

# ---------------------------------------------------------------- materials (plain for now: the wireframe stage)
def pmat(name, col, metal=0.0, rough=0.5, emit=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if emit: b.inputs['Emission'].default_value = (*col, 1); b.inputs['Emission Strength'].default_value = emit
    return m
M_PAINT = pmat('Tug_Paint', (0.85, 0.62, 0.05), 0.2, 0.45)
M_STEEL = pmat('Tug_Steel', (0.35, 0.36, 0.38), 1.0, 0.45)
M_DARK = pmat('Tug_DarkSteel', (0.08, 0.08, 0.09), 0.6, 0.55)
M_RUB = pmat('Tug_Rubber', (0.03, 0.03, 0.03), 0.0, 0.9)
M_GLASS = pmat('Tug_Glass', (0.2, 0.3, 0.35), 0.0, 0.05)
M_VINYL = pmat('Tug_SeatVinyl', (0.05, 0.05, 0.06), 0.0, 0.6)
M_PLAST = pmat('Tug_BlackPlastic', (0.04, 0.04, 0.045), 0.0, 0.5)
M_AMBER = pmat('Tug_BeaconLens', (1.0, 0.55, 0.05), 0.0, 0.2, 2.0)
M_RED = pmat('Tug_RedLens', (0.9, 0.05, 0.03), 0.0, 0.2, 1.0)
M_WHITE = pmat('Tug_HeadLens', (0.95, 0.95, 0.9), 0.0, 0.1, 1.5)
M_FLOOR = pmat('Tug_FloorMat', (0.06, 0.06, 0.06), 0.0, 0.9)

# ---------------------------------------------------------------- helpers
ALL = []
def finish(o, m, smooth=40):
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
def box(name, c, s, m, bevel=0.0, segs=2, cuts=1, sub=0, deform=None, smooth=40):
    """A block (centre c, size s); bevel = edge chamfer/round (segs 1 = a flat chamfer, as cut steel)."""
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    if cuts > 1: bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts - 1, use_grid_fill=True)
    for v in bm.verts: v.co = Vector((c[0] + v.co.x * s[0], c[1] + v.co.y * s[1], c[2] + v.co.z * s[2]))
    o = link(name, bm)
    if bevel > 0:
        md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = segs; md.limit_method = 'ANGLE'; md.angle_limit = math.radians(30)
    if sub: md = o.modifiers.new('s', 'SUBSURF'); md.levels = sub; md.render_levels = sub
    apply_mods(o)
    if deform:
        for v in o.data.vertices:
            u, vv, w = ((v.co.x - c[0]) / (s[0] / 2), (v.co.y - c[1]) / (s[1] / 2), (v.co.z - c[2]) / (s[2] / 2))
            v.co = deform(Vector(v.co), u, vv, w)
    return finish(o, m, smooth)
def cyl(name, c, r, depth, axis, m, verts=32, bevel=0.0, r2=None):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r, radius2=r if r2 is None else r2, depth=depth)
    rot = {'x': Matrix.Rotation(math.pi / 2, 4, 'Y'), 'y': Matrix.Rotation(math.pi / 2, 4, 'X'), 'z': Matrix.Identity(4)}[axis]
    bmesh.ops.transform(bm, matrix=Matrix.Translation(Vector(c)) @ rot, verts=bm.verts)
    o = link(name, bm)
    if bevel: md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = 2; md.limit_method = 'ANGLE'; apply_mods(o)
    return finish(o, m, 35)
def tube(name, pts, r, m, res=8, bez=True):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = r; cu.bevel_resolution = 3; cu.use_fill_caps = True; cu.resolution_u = res
    if bez:
        sp = cu.splines.new('BEZIER'); sp.bezier_points.add(len(pts) - 1)
        for p, q in zip(sp.bezier_points, pts): p.co = q; p.handle_left_type = p.handle_right_type = 'AUTO'
    else:
        sp = cu.splines.new('POLY'); sp.points.add(len(pts) - 1)
        for p, q in zip(sp.points, pts): p.co = (*q, 1)
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o)
    bpy.context.view_layer.objects.active = o
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.ops.object.convert(target='MESH'); o = bpy.context.view_layer.objects.active; o.select_set(False)
    return finish(o, m, 50)
def prism(name, pts_xz, y0, y1, m, bevel=0.0, segs=1):
    """A side outline (x, z) pushed across y0..y1 - the way plate steel is cut and welded."""
    bm = bmesh.new()
    a = [bm.verts.new((x, y0, z)) for x, z in pts_xz]; b = [bm.verts.new((x, y1, z)) for x, z in pts_xz]
    bm.faces.new(a[::-1]); bm.faces.new(b)
    n = len(pts_xz)
    for i in range(n): bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = link(name, bm)
    if bevel: md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = segs; md.limit_method = 'ANGLE'; md.angle_limit = math.radians(30); apply_mods(o)
    return finish(o, m, 30)
def cut(target, cutter):
    md = target.modifiers.new('cut', 'BOOLEAN'); md.operation = 'DIFFERENCE'; md.object = cutter; md.solver = 'EXACT'
    apply_mods(target); ALL.remove(cutter); bpy.data.objects.remove(cutter, do_unlink=True)
IKS = []
def ik(name, loc, rot=(0, 0, 0)):
    e = bpy.data.objects.new('IK_' + name, None); e.empty_display_type = 'ARROWS'; e.empty_display_size = 0.08
    e.location = loc; e.rotation_euler = rot; bpy.context.collection.objects.link(e); IKS.append(e); return e

# ---------------------------------------------------------------- the body: plate steel, flat faces, chamfered edges
# side outline: low nose plate sloping to the front bumper, flat deck, square-ish tail
body_xz = [(-L / 2, GC + 0.12), (-L / 2, DECK - 0.10), (-L / 2 + 0.10, DECK), (L / 2 - 0.62, DECK), (L / 2, DECK - 0.30),
           (L / 2, GC + 0.16), (L / 2 - 0.16, GC), (-L / 2 + 0.12, GC)]
body = prism('Body', body_xz, -W / 2, W / 2, M_PAINT, bevel=0.035, segs=1)
# the wheel arches: round cut-outs through the body sides (the real curves), deep enough for steering lock
for i, x in enumerate((FX, RX)):
    for s in (-1, 1):
        cutter = cyl(f'ArchCut_{i}{s}', (x, s * (TR / 2), TD / 2), TD / 2 + 0.07, TW + 0.30, 'y', M_DARK, 48)
        cut(body, cutter)
# the cab well: the body is cut away where the sunken cab sits
well = box('WellCut', ((CAB_X0 + CAB_X1) / 2, (CAB_Y0 + CAB_Y1) / 2 + 0.05, (CFLOOR + DECK) / 2 + 0.3), (CAB_X1 - CAB_X0 + 0.02, CAB_Y1 - CAB_Y0 + 0.12, DECK - CFLOOR + 0.6), M_DARK)
cut(body, well)
# arch liners (inner wheel-house walls) so you never see into the body
for i, x in enumerate((FX, RX)):
    for s in (-1, 1):
        y = s * (TR / 2 - TW / 2 - 0.16)
        o = cyl(f'ArchLiner_{i}{s}', (x, y, TD / 2), TD / 2 + 0.07, 0.012, 'y', M_DARK, 48)
# fender lips: flat steel strip bent round each arch, welded to the body side
import math as _m
for i, x in enumerate((FX, RX)):
    for s in (-1, 1):
        pts = [(x + (TD / 2 + 0.09) * _m.cos(a), s * (W / 2 + 0.012), TD / 2 + (TD / 2 + 0.09) * _m.sin(a)) for a in [_m.radians(d) for d in range(8, 173, 6)]]
        tube(f'FenderLip_{i}{s}', pts, 0.018, M_PAINT, bez=False)

# ---------------------------------------------------------------- running gear: axles, hubs, tyres (hang on the frame)
for i, x in enumerate((FX, RX)):
    cyl(f'Axle_{i}', (x, 0, TD / 2), 0.075, TR - TW, 'y', M_DARK, 24)
    box(f'AxleMount_{i}', (x, 0, TD / 2 + 0.17), (0.32, 0.9, 0.24), M_DARK, bevel=0.02)          # axle to frame
    cyl(f'Diff_{i}', (x, 0, TD / 2), 0.20, 0.36, 'y', M_DARK, 32, bevel=0.02)
    for s in (-1, 1):
        y = s * TR / 2
        tyre = cyl(f'Tyre_{i}{s}', (x, y, TD / 2), TD / 2, TW, 'y', M_RUB, 64, bevel=0.06)
        # tread blocks: chevron lugs set into the tyre's face
        for k in range(28):
            a = k * 2 * math.pi / 28
            c = Vector((x + (TD / 2 - 0.005) * math.cos(a), y, TD / 2 + (TD / 2 - 0.005) * math.sin(a)))
            lug = box(f'Lug_{i}{s}{k}', c, (0.045, TW * 0.82, 0.03), M_RUB, bevel=0.006)
            lug.data.transform(Matrix.Translation(c) @ Matrix.Rotation(-a, 4, 'Y') @ Matrix.Translation(-c))
        cyl(f'Rim_{i}{s}', (x, y + s * 0.02, TD / 2), 0.29, TW * 0.9, 'y', M_STEEL, 48, bevel=0.01)
        cyl(f'Hub_{i}{s}', (x, y + s * 0.15, TD / 2), 0.11, 0.06, 'y', M_DARK, 32, bevel=0.01)
        for k in range(10):
            a = k * 2 * math.pi / 10
            cyl(f'Nut_{i}{s}{k}', (x + 0.19 * math.cos(a), y + s * 0.145, TD / 2 + 0.19 * math.sin(a)), 0.016, 0.03, 'y', M_STEEL, 6)
        cyl(f'Kingpin_{i}{s}', (x, s * (TR / 2 - 0.22), TD / 2), 0.05, 0.36, 'z', M_DARK, 16)      # the steering pivot (4WS)

# ---------------------------------------------------------------- bumpers + hitches (bolted to the body ends)
box('BumperRear', (-L / 2 - 0.05, 0, GC + 0.36), (0.101, W - 0.30, 0.47), M_DARK, bevel=0.012)
box('BumperFront', (L / 2 + 0.019, 0, GC + 0.36), (0.038, W - 0.50, 0.40), M_DARK, bevel=0.008)
for s in (-1, 1):
    for x in (-L / 2 - 0.05, L / 2 + 0.019):
        for z in (GC + 0.20, GC + 0.52):
            cyl(f'BumperBolt_{x > 0}{s}{z > 0.5}', (x + (0.055 if x > 0 else -0.055), s * 0.85, z), 0.02, 0.025, 'x', M_STEEL, 6)
for nm, x, d in (('Rear', -L / 2 - 0.10, -1), ('Front', L / 2 + 0.038, 1)):
    for z in (0.38, 0.62):                                                                    # multi-level jaws
        box(f'HitchJaw{nm}_{z}', (x + d * 0.09, 0, z), (0.18, 0.22, 0.07), M_STEEL, bevel=0.01)
        box(f'HitchJawLow{nm}_{z}', (x + d * 0.09, 0, z - 0.09), (0.18, 0.22, 0.03), M_STEEL, bevel=0.006)
    cyl(f'HitchPin{nm}', (x + d * 0.12, 0, 0.50), 0.022, 0.42, 'z', M_STEEL, 16)
    box(f'HitchPlate{nm}', (x + d * 0.01, 0, 0.50), (0.03, 0.40, 0.46), M_DARK, bevel=0.006)   # welded to the bumper
    for s in (-1, 1): cyl(f'TowEye{nm}{s}', (x + d * 0.04, s * 0.95, GC + 0.15), 0.04, 0.03, 'x', M_STEEL, 16)
# lights: every lamp is a housing SET INTO the bumper or body (half its depth inside the steel), the lens recessed in it
def lamp(name, face_pt, normal, w, h, m, depth=0.06):
    """face_pt: a point on the face it sits in; normal: 'x+', 'x-', 'y+', 'y-'; w across, h up (m)."""
    ax, sg = normal[0], (1 if normal[1] == '+' else -1)
    c = Vector(face_pt)
    if ax == 'x':
        hous = box(name + '_Housing', (c.x, c.y, c.z), (depth, w + 0.03, h + 0.03), M_DARK, bevel=0.008)          # half in, half out
        rim = box(name + '_Rim', (c.x + sg * depth / 2, c.y, c.z), (0.008, w + 0.03, h + 0.03), M_PLAST, bevel=0.004)
        cutter = box(name + '_Recess', (c.x + sg * depth / 2, c.y, c.z), (0.03, w, h), M_DARK); cut(rim, cutter)
        box(name + '_Lens', (c.x + sg * (depth / 2 - 0.008), c.y, c.z), (0.006, w, h), m, bevel=0.003)
    else:
        hous = box(name + '_Housing', (c.x, c.y, c.z), (w + 0.03, depth, h + 0.03), M_DARK, bevel=0.008)
        rim = box(name + '_Rim', (c.x, c.y + sg * depth / 2, c.z), (w + 0.03, 0.008, h + 0.03), M_PLAST, bevel=0.004)
        cutter = box(name + '_Recess', (c.x, c.y + sg * depth / 2, c.z), (w, 0.03, h), M_DARK); cut(rim, cutter)
        box(name + '_Lens', (c.x, c.y + sg * (depth / 2 - 0.008), c.z), (w, 0.006, h), m, bevel=0.003)
FRONT_FACE, REAR_FACE = L / 2 + 0.038, -L / 2 - 0.1005
for s in (-1, 1):
    lamp(f'HeadLamp_{s}', (FRONT_FACE, s * 0.95, GC + 0.48), 'x+', 0.24, 0.12, M_WHITE)
    lamp(f'IndicatorF_{s}', (FRONT_FACE, s * 0.95, GC + 0.30), 'x+', 0.12, 0.07, M_AMBER)
    lamp(f'TailLamp_{s}', (REAR_FACE, s * 0.90, GC + 0.48), 'x-', 0.22, 0.12, M_RED)
    lamp(f'IndicatorR_{s}', (REAR_FACE, s * 0.90, GC + 0.30), 'x-', 0.12, 0.07, M_AMBER)
    for x in (-L / 2 + 0.35, L / 2 - 0.35):
        lamp(f'SideMarker_{s}{x > 0}', (x, s * W / 2, DECK - 0.22), 'y' + ('+' if s > 0 else '-'), 0.10, 0.05, M_AMBER, depth=0.04)
# chevron panel frames on the bumpers (the chevrons themselves come with the paint texture)
# ---------------------------------------------------------------- deck: engine cover, grilles, ballast, tanks, boxes
box('EngineCover', (1.30, -0.15, DECK + 0.06), (1.20, 1.90, 0.12), M_PAINT, bevel=0.03, segs=3)      # its edges rolled (pressed sheet)
for k in range(7): box(f'GrilleSlat_{k}', (0.97 + k * 0.11, -0.15, DECK + 0.125), (0.05, 1.40, 0.012), M_DARK, bevel=0.003)
for s in (-1, 1): cyl(f'CoverHinge_{s}', (0.72, -0.15 + s * 0.75, DECK + 0.05), 0.02, 0.14, 'y', M_STEEL, 12)
box('CoverLatch', (1.91, -0.15, DECK + 0.06), (0.03, 0.12, 0.05), M_STEEL, bevel=0.004)
box('BallastRear', (-1.65, 0, DECK + 0.075), (1.30, W - 0.30, 0.15), M_DARK, bevel=0.01)
for k in range(4): box(f'BallastPlate_{k}', (-1.65, 0, DECK + 0.03 + k * 0.035), (1.28, W - 0.32, 0.004), M_STEEL)
for x in (-2.15, -1.15):
    for s in (-1, 1): cyl(f'BallastBolt_{x}{s}', (x, s * 1.0, DECK + 0.16), 0.025, 0.03, 'z', M_STEEL, 6)
for s in (-1, 1): box(f'LiftEye_{s}', (-2.35, s * 1.05, DECK + 0.20), (0.06, 0.03, 0.10), M_STEEL, bevel=0.01)
cyl('FuelFiller', (-0.95, -0.95, DECK + 0.03), 0.06, 0.06, 'z', M_STEEL, 24, bevel=0.005)
cyl('HydFiller', (-0.95, -0.55, DECK + 0.03), 0.05, 0.06, 'z', M_DARK, 24, bevel=0.005)
box('ToolBox', (-0.92, -0.10, DECK + 0.12), (0.45, 0.55, 0.24), M_DARK, bevel=0.01)
box('ToolBoxLid', (-0.92, -0.10, DECK + 0.245), (0.47, 0.57, 0.02), M_DARK, bevel=0.005)
# the exhaust: a stack clamped to the cab's back wall, its pipe down into the body
cyl('Exhaust', (CABU_X0 - 0.06, CAB_Y1 - 0.25, DECK + 0.45), 0.045, 0.90, 'z', M_STEEL, 20)
cyl('ExhaustCap', (CABU_X0 - 0.06, CAB_Y1 - 0.25, DECK + 0.91), 0.05, 0.03, 'z', M_DARK, 20)
for z in (DECK + 0.25, DECK + 0.70): box(f'ExhaustClamp_{z}', (CABU_X0 - 0.03, CAB_Y1 - 0.25, z), (0.07, 0.11, 0.03), M_STEEL)

# ---------------------------------------------------------------- the cab (sunk into the body; rounded corners: real cabs are pressed)
cab = box('CabShell', ((CABU_X0 + CABU_X1) / 2, (CAB_Y0 + CAB_Y1) / 2, (CFLOOR + CABH) / 2 + (DECK - CFLOOR) / 2),
          (CABU_X1 - CABU_X0, CAB_Y1 - CAB_Y0, CABH - DECK), M_PAINT, bevel=0.07, segs=5, cuts=4)
low = box('CabLower', ((CAB_X0 + CAB_X1) / 2, (CAB_Y0 + CAB_Y1) / 2, (CFLOOR + DECK) / 2),
          (CAB_X1 - CAB_X0, CAB_Y1 - CAB_Y0, DECK - CFLOOR), M_PAINT, bevel=0.03, segs=3)
# hollow the cab: an inner volume 50 mm in from the walls
inner = box('CabInnerCut', ((CABU_X0 + CABU_X1) / 2, (CAB_Y0 + CAB_Y1) / 2, (DECK + CABH) / 2 - 0.03), (CABU_X1 - CABU_X0 - 0.10, CAB_Y1 - CAB_Y0 - 0.10, CABH - DECK - 0.06), M_DARK, bevel=0.04, segs=3)
cut(cab, inner)
inner2 = box('CabInnerCut2', ((CAB_X0 + CAB_X1) / 2, (CAB_Y0 + CAB_Y1) / 2, (CFLOOR + DECK) / 2 + 0.06), (CAB_X1 - CAB_X0 - 0.10, CAB_Y1 - CAB_Y0 - 0.10, DECK - CFLOOR + 0.02), M_DARK)
cut(low, inner2)
inner3 = box('CabTopCut', ((CAB_X0 + CAB_X1) / 2, (CAB_Y0 + CAB_Y1) / 2, DECK + 0.02), (CAB_X1 - CAB_X0 - 0.10, CAB_Y1 - CAB_Y0 - 0.10, 0.2), M_DARK)
cut(cab, inner3)
# window openings: front, both sides (door + fixed), rear; the glass set in rubber in each
WIN_Z0, WIN_Z1 = DECK + 0.10, CABH - 0.10
def window(name, c, s):
    cutter = box(name + 'Cut', c, s, M_DARK, bevel=0.03, segs=3); cut(cab, cutter)
    g = box(name + 'Glass', c, tuple(max(0.008, v - 0.03) if v < 0.2 else v - 0.03 for v in s), M_GLASS, bevel=0.025, segs=3)
    return g
wz, wh = (WIN_Z0 + WIN_Z1) / 2, WIN_Z1 - WIN_Z0
window('WinFront', (CABU_X1, (CAB_Y0 + CAB_Y1) / 2, wz), (0.12, CAB_Y1 - CAB_Y0 - 0.22, wh))
window('WinRear', (CABU_X0, (CAB_Y0 + CAB_Y1) / 2, wz + 0.1), (0.12, CAB_Y1 - CAB_Y0 - 0.30, wh - 0.30))
window('WinDoor', (0.20, CAB_Y1, wz), (0.62, 0.12, wh))                  # the door's window (left side)
window('WinLeftRear', (-0.38, CAB_Y1, wz), (0.42, 0.12, wh))
window('WinRight', (0.10, CAB_Y0, wz), (1.05, 0.12, wh))
# door: panel line, hinges on the front post, handles outside and in
box('DoorSeamF', (0.56, CAB_Y1 + 0.003, (CFLOOR + CABH) / 2), (0.006, 0.006, CABH - CFLOOR - 0.12), M_DARK)
box('DoorSeamR', (-0.15, CAB_Y1 + 0.003, (CFLOOR + CABH) / 2), (0.006, 0.006, CABH - CFLOOR - 0.12), M_DARK)
for z in (CFLOOR + 0.25, CABH - 0.35): cyl(f'DoorHinge_{z}', (0.57, CAB_Y1 + 0.015, z), 0.015, 0.10, 'z', M_STEEL, 12)
box('DoorHandleOut', (-0.05, CAB_Y1 + 0.03, DECK - 0.02), (0.14, 0.025, 0.035), M_STEEL, bevel=0.008)
for x in (-0.11, 0.01): box(f'DoorHandleMount_{x}', (x, CAB_Y1 + 0.015, DECK - 0.02), (0.02, 0.03, 0.03), M_STEEL)
box('DoorHandleIn', (-0.05, CAB_Y1 - 0.07, DECK - 0.05), (0.12, 0.02, 0.03), M_PLAST, bevel=0.006)
box('DoorPanelIn', (0.20, CAB_Y1 - 0.055, (CFLOOR + DECK) / 2 + 0.1), (0.66, 0.01, DECK - CFLOOR - 0.1), M_PLAST, bevel=0.005)
# roof: drip rail round the edge, beacon on its base plate, work lamps on brackets, horn, antenna
for s in (-1, 1): box(f'DripRail_{s}', ((CABU_X0 + CABU_X1) / 2, (CAB_Y1 if s > 0 else CAB_Y0) - s * 0.01, CABH - 0.03), (CABU_X1 - CABU_X0 - 0.10, 0.02, 0.02), M_PAINT)
box('BeaconBase', (0.0, (CAB_Y0 + CAB_Y1) / 2, CABH + 0.012), (0.20, 0.20, 0.024), M_DARK, bevel=0.005)
cyl('BeaconLens', (0.0, (CAB_Y0 + CAB_Y1) / 2, CABH + 0.09), 0.075, 0.13, 'z', M_AMBER, 32, bevel=0.02)
cyl('BeaconCap', (0.0, (CAB_Y0 + CAB_Y1) / 2, CABH + 0.16), 0.06, 0.02, 'z', M_DARK, 32)
for k, y in enumerate((CAB_Y0 + 0.25, CAB_Y1 - 0.25)):
    box(f'WorkLampBracket_{k}', (CABU_X0 + 0.10, y, CABH + 0.03), (0.04, 0.06, 0.06), M_DARK)
    box(f'WorkLamp_{k}', (CABU_X0 + 0.06, y, CABH + 0.09), (0.08, 0.16, 0.10), M_DARK, bevel=0.012)
    box(f'WorkLampLens_{k}', (CABU_X0 + 0.026, y, CABH + 0.09), (0.006, 0.13, 0.075), M_WHITE)
cyl('AntennaBase', (-0.3, CAB_Y0 + 0.2, CABH + 0.01), 0.02, 0.02, 'z', M_DARK, 12)
cyl('Antenna', (-0.3, CAB_Y0 + 0.2, CABH + 0.31), 0.003, 0.60, 'z', M_DARK, 6)
# mirrors: arms bolted to the front post, both sides
for s, y in ((1, CAB_Y1), (-1, CAB_Y0)):
    tube(f'MirrorArm_{s}', [(CABU_X1 - 0.05, y, CABH - 0.40), (CABU_X1 + 0.10, y + s * 0.18, CABH - 0.36), (CABU_X1 + 0.12, y + s * 0.26, CABH - 0.30)], 0.012, M_DARK)
    box(f'Mirror_{s}', (CABU_X1 + 0.12, y + s * 0.30, CABH - 0.36), (0.04, 0.18, 0.26), M_PLAST, bevel=0.02, segs=3)
    box(f'MirrorPlate_{s}', (CABU_X1 - 0.04, y, CABH - 0.40), (0.06, 0.012, 0.08), M_DARK)
# wipers on their spindles under the front window
for k, y in enumerate((CAB_Y0 + 0.40, CAB_Y1 - 0.45)):
    cyl(f'WiperSpindle_{k}', (CABU_X1 + 0.015, y, WIN_Z0 - 0.01), 0.012, 0.03, 'x', M_DARK, 12)
    box(f'WiperArm_{k}', (CABU_X1 + 0.03, y + 0.12, WIN_Z0 + 0.20), (0.012, 0.012, 0.48), M_DARK)
    box(f'WiperBlade_{k}', (CABU_X1 + 0.04, y + 0.12, WIN_Z0 + 0.25), (0.01, 0.02, 0.44), M_RUB)
# step and grab rails to climb in (the step on 2 brackets welded to the body side; rails bolted at both ends)
box('StepTread', (0.0, W / 2 + 0.10, 0.533 - 0.02), (0.50, 0.20, 0.03), M_STEEL, bevel=0.004)
for x in (-0.20, 0.20): box(f'StepBracket_{x}', (x, W / 2 + 0.06, 0.533 - 0.12), (0.03, 0.12, 0.20), M_STEEL)
for k in range(5): box(f'StepGrip_{k}', (-0.20 + k * 0.10, W / 2 + 0.10, 0.533 - 0.003), (0.05, 0.18, 0.006), M_DARK)
tube('GrabRailFront', [(0.62, CAB_Y1 + 0.005, DECK + 0.05), (0.62, CAB_Y1 + 0.07, DECK + 0.10), (0.62, CAB_Y1 + 0.07, DECK + 0.60), (0.62, CAB_Y1 + 0.005, DECK + 0.65)], 0.016, M_STEEL, bez=False)
tube('GrabRailDeck', [(-0.62, W / 2 - 0.05, DECK), (-0.62, W / 2 - 0.05, DECK + 0.08), (-0.62, W / 2 - 0.05, DECK + 0.45)], 0.016, M_STEEL, bez=False)
# mud flaps behind each wheel, bolted to the body underside
for i, x in enumerate((FX, RX)):
    for s in (-1, 1): box(f'MudFlap_{i}{s}', (x - TD / 2 - 0.12, s * TR / 2, GC + 0.18), (0.01, TW + 0.06, 0.36), M_RUB)

# ---------------------------------------------------------------- the interior (all real sizes, every part on the floor or a wall)
IX0, IX1 = CAB_X0 + 0.05, CAB_X1 - 0.05
IY0, IY1 = CAB_Y0 + 0.05, CAB_Y1 - 0.05
box('FloorMat', ((IX0 + IX1) / 2, (IY0 + IY1) / 2, CFLOOR + 0.006), (IX1 - IX0, IY1 - IY0, 0.012), M_FLOOR)
for k in range(9): box(f'MatRib_{k}', ((IX0 + IX1) / 2, IY0 + 0.08 + k * 0.14, CFLOOR + 0.014), (IX1 - IX0 - 0.06, 0.02, 0.004), M_FLOOR)
# dashboard: across the front, from the floor (its own pedestal) up under the front window, on the upper box's front part
DX = CABU_X1 - 0.30
dash = box('Dash', (DX + 0.10, (IY0 + IY1) / 2, DECK - 0.05), (0.34, IY1 - IY0, 0.34), M_PLAST, bevel=0.03, segs=3)
box('DashTop', (DX + 0.15, (IY0 + IY1) / 2, DECK + 0.13), (0.30, IY1 - IY0, 0.04), M_PLAST, bevel=0.015, segs=2)
box('DashPedestal', (CAB_X1 - 0.12, IY1 - 0.40, (CFLOOR + DECK) / 2 - 0.08), (0.14, 0.40, DECK - CFLOOR - 0.15), M_PLAST, bevel=0.02)
DRY = IY1 - 0.42                       # the driver's line (y)
box('DisplayBezel', (DX - 0.07, DRY, DECK + 0.02), (0.03, 0.26, 0.17), M_PLAST, bevel=0.01)
box('Display', (DX - 0.086, DRY, DECK + 0.02), (0.004, 0.22, 0.13), M_GLASS)
for k in range(6): cyl(f'Switch_{k}', (DX - 0.07, DRY - 0.30 + k * 0.05, DECK - 0.10), 0.011, 0.02, 'x', M_DARK, 12)
cyl('EStop', (DX - 0.07, IY0 + 0.25, DECK - 0.02), 0.025, 0.03, 'x', M_RED, 24, bevel=0.005)
for k in range(4): box(f'Vent_{k}', (DX - 0.07, IY0 + 0.45 + k * 0.035, DECK + 0.08), (0.02, 0.025, 0.06), M_DARK)
# steering column out of the dash, tilted; wheel round on its hub (a real curve)
COL_TOP = Vector((DX - 0.26, DRY, DECK + 0.05)); COL_BOT = Vector((DX - 0.02, DRY, DECK - 0.12))
tube('SteeringColumn', [tuple(COL_BOT), tuple(COL_TOP)], 0.035, M_PLAST, bez=False)
wheel_c = COL_TOP + Vector((-0.02, 0, 0.012))
ax = (COL_TOP - COL_BOT).normalized()
rot = ax.to_track_quat('Z', 'Y').to_matrix().to_4x4()
bm = bmesh.new(); bmesh.ops.create_circle(bm, cap_ends=False, segments=48, radius=0.19)
wheel_ring = link('SteeringRingPath', bm)
for k, v in enumerate(wheel_ring.data.vertices): pass
pts = [tuple(wheel_c + (rot @ Vector((0.19 * math.cos(a), 0.19 * math.sin(a), 0)))) for a in [k * 2 * math.pi / 48 for k in range(49)]]
bpy.data.objects.remove(wheel_ring, do_unlink=True)
tube('SteeringWheel', pts, 0.016, M_PLAST, bez=False)
cyl_c = wheel_c
hub = cyl('SteeringHub', (0, 0, 0), 0.05, 0.05, 'z', M_PLAST, 24, bevel=0.01); hub.data.transform(Matrix.Translation(cyl_c) @ rot)
for k in range(3):
    a = math.pi / 2 + k * 2 * math.pi / 3
    tube(f'SteeringSpoke_{k}', [tuple(wheel_c), tuple(wheel_c + rot @ Vector((0.18 * math.cos(a), 0.18 * math.sin(a), 0)))], 0.012, M_PLAST, bez=False)
cyl('SpinnerKnob', tuple(wheel_c + rot @ Vector((0.19, 0, 0.04))), 0.018, 0.06, 'z', M_PLAST, 16)
# pedals: hinged on the dash pedestal's floor plate (accelerator, brake, inching)
for k, y in enumerate((DRY - 0.12, DRY, DRY + 0.12)):
    box(f'PedalPivot_{k}', (CAB_X1 - 0.14, y, CFLOOR + 0.05), (0.05, 0.06, 0.04), M_DARK)
    box(f'Pedal_{k}', (CAB_X1 - 0.20, y, CFLOOR + 0.13), (0.03, 0.08 if k else 0.06, 0.16 if k else 0.20), M_RUB, bevel=0.006)
    tube(f'PedalArm_{k}', [(CAB_X1 - 0.14, y, CFLOOR + 0.06), (CAB_X1 - 0.19, y, CFLOOR + 0.11)], 0.008, M_DARK, bez=False)

# the operator's seat: a real tug seat - suspension base with bellows, high vinyl back with side bolsters, armrests, belt
SX, SY, SZ = IX0 + 0.30, DRY, CFLOOR       # the seat's footprint centre on the floor
box('SeatRails', (SX, SY, SZ + 0.02), (0.42, 0.34, 0.04), M_DARK, bevel=0.005)
def bellows(z0, z1, rx, ry, n=7):
    for k in range(n):
        z = z0 + (z1 - z0) * (k + 0.5) / n
        box(f'Bellows_{k}', (SX, SY, z), (rx * (1.0 if k % 2 else 0.92), ry * (1.0 if k % 2 else 0.92), (z1 - z0) / n * 1.02), M_RUB, bevel=0.012, segs=3)
bellows(SZ + 0.04, SZ + 0.24, 0.40, 0.42)
box('SeatPan', (SX, SY, SZ + 0.26), (0.46, 0.48, 0.04), M_DARK, bevel=0.01)
# cushion: crowned top, side bolsters, a rolled front edge
def cushion_def(co, u, v, w):
    top = ss(-1.0, 1.0, w)
    co.z += top * (0.012 * (1 - v * v) + 0.03 * ss(0.55, 0.95, abs(v)))        # bolsters on the left and right
    fr = ss(0.55, 1.0, u)                                                        # roll over the front edge
    co.z -= fr * fr * 0.04 * top; co.x -= fr * 0.012 * (1 - top)
    return co
box('SeatCushion', (SX + 0.01, SY, SZ + 0.33), (0.48, 0.50, 0.11), M_VINYL, bevel=0.035, segs=4, cuts=8, sub=1, deform=cushion_def, smooth=60)
# backrest: leaning back 12 degrees, lumbar bulge, side wings, an integrated headrest on top
REC = math.radians(12)
BACK_C = Vector((SX - 0.27, SY, SZ + 0.70))
def back_def(co, u, v, w):
    up = (w + 1) / 2
    co.x += 0.035 * math.exp(-((up - 0.30) / 0.16) ** 2) * ss(-1.0, 0.2, u)     # lumbar on the front face
    co.x += 0.05 * ss(0.5, 0.95, abs(v)) * ss(-1, 0, u)                          # side wings forward
    return co
back = box('SeatBack', tuple(BACK_C), (0.10, 0.50, 0.66), M_VINYL, bevel=0.04, segs=4, cuts=8, sub=1, deform=back_def, smooth=60)
back.data.transform(Matrix.Translation(BACK_C) @ Matrix.Rotation(-REC, 4, 'Y') @ Matrix.Translation(-BACK_C))
HEAD_C = BACK_C + Vector((-math.sin(REC) * 0.42, 0, math.cos(REC) * 0.42))
head = box('SeatHeadrest', tuple(HEAD_C), (0.09, 0.30, 0.17), M_VINYL, bevel=0.035, segs=4, cuts=4, sub=1, smooth=60)
head.data.transform(Matrix.Translation(HEAD_C) @ Matrix.Rotation(-REC, 4, 'Y') @ Matrix.Translation(-HEAD_C))
for s in (-1, 1):
    cyl(f'HeadrestPost_{s}', tuple(HEAD_C + Vector((0.02, s * 0.08, -0.10))), 0.008, 0.10, 'z', M_STEEL, 12)
box('SeatBackShell', tuple(BACK_C + Vector((-0.065, 0, 0))), (0.03, 0.46, 0.62), M_PLAST, bevel=0.012).data.transform(
    Matrix.Translation(BACK_C) @ Matrix.Rotation(-REC, 4, 'Y') @ Matrix.Translation(-BACK_C))
box('SeatRecliner', (SX - 0.21, SY - 0.27, SZ + 0.34), (0.08, 0.03, 0.08), M_DARK, bevel=0.01)
# armrests: on hinges at the backrest's sides; the right one carries the drive selector and the horn
for s in (-1, 1):
    cyl(f'ArmHinge_{s}', (SX - 0.20, SY + s * 0.28, SZ + 0.52), 0.02, 0.05, 'y', M_DARK, 16)
    box(f'Armrest_{s}', (SX + 0.02, SY + s * 0.28, SZ + 0.53), (0.42, 0.07, 0.06), M_VINYL, bevel=0.025, segs=3)
box('ControlPod', (SX + 0.18, SY - 0.28, SZ + 0.58), (0.14, 0.09, 0.05), M_PLAST, bevel=0.012)
cyl('DriveSelector', (SX + 0.20, SY - 0.28, SZ + 0.63), 0.012, 0.07, 'z', M_DARK, 12)
cyl('DriveSelectorKnob', (SX + 0.20, SY - 0.28, SZ + 0.67), 0.022, 0.03, 'z', M_PLAST, 16, bevel=0.008)
for s in (-1, 1):                                                   # lap belt: anchors on the seat pan, buckle + tongue
    box(f'BeltAnchor_{s}', (SX - 0.12, SY + s * 0.25, SZ + 0.30), (0.03, 0.01, 0.06), M_STEEL)
box('BeltBuckle', (SX - 0.10, SY - 0.23, SZ + 0.40), (0.06, 0.025, 0.035), M_STEEL, bevel=0.006)
tube('BeltStrap', [(SX - 0.12, SY + 0.25, SZ + 0.33), (SX + 0.02, SY + 0.10, SZ + 0.42), (SX + 0.02, SY - 0.10, SZ + 0.42), (SX - 0.10, SY - 0.23, SZ + 0.40)], 0.004, M_PLAST)
box('ParkBrake', (SX + 0.10, IY0 + 0.10, CFLOOR + 0.25), (0.05, 0.04, 0.40), M_DARK, bevel=0.01)
box('ParkBrakeBase', (SX + 0.10, IY0 + 0.10, CFLOOR + 0.03), (0.12, 0.10, 0.05), M_DARK, bevel=0.005)
# two fold-down crew seats on the right wall (hinged brackets bolted to the wall; contoured pads)
for k, x in enumerate((IX0 + 0.25, IX1 - 0.30)):
    for s in (-1, 1): box(f'JumpBracket_{k}{s}', (x + s * 0.17, IY0 + 0.02, CFLOOR + 0.30), (0.03, 0.04, 0.30), M_DARK)
    box(f'JumpSeat_{k}', (x, IY0 + 0.20, CFLOOR + 0.44), (0.38, 0.32, 0.07), M_VINYL, bevel=0.025, segs=3, cuts=4, sub=1, deform=cushion_def, smooth=60)
    box(f'JumpBack_{k}', (x, IY0 + 0.035, CFLOOR + 0.80), (0.36, 0.05, 0.40), M_VINYL, bevel=0.02, segs=3, cuts=3, sub=1, smooth=60)
    box(f'JumpBackPlate_{k}', (x, IY0 + 0.008, CFLOOR + 0.80), (0.38, 0.012, 0.44), M_DARK)
# roof liner, the interior lamp, sun visor, a grab handle, the fire extinguisher on its bracket, the radio
box('RoofLiner', ((CABU_X0 + CABU_X1) / 2, (IY0 + IY1) / 2, CABH - 0.045), (CABU_X1 - CABU_X0 - 0.12, IY1 - IY0 - 0.02, 0.012), M_PLAST)
box('CabLamp', (0.0, (IY0 + IY1) / 2, CABH - 0.058), (0.20, 0.08, 0.016), M_WHITE, bevel=0.005)
box('SunVisor', (CABU_X1 - 0.14, DRY, CABH - 0.10), (0.18, 0.36, 0.012), M_PLAST, bevel=0.004)
for s in (-1, 1): box(f'SunVisorClip_{s}', (CABU_X1 - 0.08, DRY + s * 0.17, CABH - 0.07), (0.02, 0.02, 0.04), M_PLAST)
tube('GrabHandleIn', [(CABU_X1 - 0.10, IY1, DECK + 0.30), (CABU_X1 - 0.10, IY1 - 0.04, DECK + 0.34), (CABU_X1 - 0.10, IY1 - 0.04, DECK + 0.62), (CABU_X1 - 0.10, IY1, DECK + 0.66)], 0.013, M_PLAST, bez=False)
box('ExtinguisherBracket', (IX0 + 0.03, IY0 + 0.70, CFLOOR + 0.30), (0.03, 0.10, 0.12), M_DARK)
cyl('Extinguisher', (IX0 + 0.08, IY0 + 0.70, CFLOOR + 0.30), 0.055, 0.40, 'z', M_RED, 24, bevel=0.02)
box('Radio', (DX - 0.06, IY0 + 0.60, DECK - 0.15), (0.05, 0.18, 0.06), M_DARK, bevel=0.006)
box('RadioHandset', (DX - 0.09, IY0 + 0.80, DECK - 0.15), (0.03, 0.05, 0.10), M_PLAST, bevel=0.008)

# ---------------------------------------------------------------- IK points (named; the game reads them by name)
ik('Seat_Driver', (SX + 0.02, SY, SZ + 0.39)); ik('Back_Driver', (SX - 0.24, SY, SZ + 0.66))
rc = rot
ik('Hand_L', tuple(wheel_c + rc @ Vector((0.19 * math.cos(math.radians(160)), 0.19 * math.sin(math.radians(160)), 0))))
ik('Hand_R', tuple(wheel_c + rc @ Vector((0.19 * math.cos(math.radians(20)), 0.19 * math.sin(math.radians(20)), 0))))
ik('Foot_Accel', (CAB_X1 - 0.21, DRY - 0.12, CFLOOR + 0.15)); ik('Foot_Brake', (CAB_X1 - 0.21, DRY, CFLOOR + 0.15))
ik('Hand_Selector', (SX + 0.20, SY - 0.28, SZ + 0.67)); ik('Hand_ParkBrake', (SX + 0.10, IY0 + 0.10, CFLOOR + 0.45))
for k, x in enumerate((IX0 + 0.25, IX1 - 0.30)): ik(f'Seat_Crew{k + 2}', (x, IY0 + 0.20, CFLOOR + 0.48)); ik(f'Back_Crew{k + 2}', (x, IY0 + 0.06, CFLOOR + 0.80))
ik('Step', (0.0, W / 2 + 0.10, 0.533)); ik('Grab_Outside', (0.62, CAB_Y1 + 0.07, DECK + 0.35)); ik('Grab_Inside', (CABU_X1 - 0.10, IY1 - 0.04, DECK + 0.48))
ik('Door_Outside', (-0.05, CAB_Y1 + 0.05, DECK - 0.02)); ik('Door_Inside', (-0.05, CAB_Y1 - 0.08, DECK - 0.05))
ik('Door_Hinge', (0.57, CAB_Y1 + 0.015, (CFLOOR + CABH) / 2))
ik('Towbar_Front', (L / 2 + 0.16, 0, 0.50)); ik('Towbar_Rear', (-L / 2 - 0.22, 0, 0.50))

# ---------------------------------------------------------------- save + export
col = bpy.data.collections.new('PushbackTug'); bpy.context.scene.collection.children.link(col)
for o in ALL + IKS:
    for c in list(o.users_collection): c.objects.unlink(o)
    col.objects.link(o)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ART, 'PushbackTug.blend'))
print('TUG: %d parts, %d IK points, %d triangles' % (len(ALL), len(IKS), sum(len(o.data.polygons) for o in ALL)))

# ---------------------------------------------------------------- the wireframe views (the blueprint's pictures)
if DO_WIRE:
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.taa_render_samples = 16
    w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.035, 0.08, 0.16, 1); sc.world = w
    sc.view_settings.view_transform = 'Standard'
    sc.render.use_freestyle = True; sc.render.line_thickness_mode = 'ABSOLUTE'; sc.render.line_thickness = 0.8
    fs = bpy.context.view_layer.freestyle_settings; fs.crease_angle = math.radians(179.5)
    ls = fs.linesets[0] if len(fs.linesets) else fs.linesets.new('All')
    ls.select_by_visibility = True; ls.visibility = 'VISIBLE'; ls.select_by_edge_types = True
    ls.select_crease = True; ls.select_silhouette = True; ls.select_border = True
    if ls.linestyle is None: ls.linestyle = bpy.data.linestyles.new('Wire')
    ls.linestyle.color = (0.82, 0.9, 1.0); ls.linestyle.thickness = 0.8
    flat = bpy.data.materials.new('WireBlue'); flat.use_nodes = True
    flat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (0.10, 0.25, 0.55, 1)
    flat.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = 0.8
    bpy.context.view_layer.material_override = flat
    for o in ALL:
        for i in range(len(o.data.materials)): o.data.materials[i] = flat
    for nm, loc, e in (('Key', (4, 5, 7), 4.0), ('Fill', (-5, -4, 4), 1.5)):
        ld = bpy.data.lights.new(nm, 'SUN'); ld.energy = e; lo = bpy.data.objects.new(nm, ld); sc.collection.objects.link(lo)
        lo.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    cam_d = bpy.data.cameras.new('Cam'); cam = bpy.data.objects.new('Cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
    ROOF = [o for o in ALL if o.name in ('CabShell', 'RoofLiner', 'CabLamp', 'BeaconBase', 'BeaconLens', 'BeaconCap', 'AntennaBase', 'Antenna', 'SunVisor') or o.name.startswith(('WorkLamp', 'DripRail', 'SunVisorClip', 'Win', 'Mirror', 'Wiper', 'GrabHandleIn', 'DoorSeam', 'DoorHinge'))]
    def shoot(name, loc, at, lens=50, ortho=0, res=(2400, 1500), cutaway=False):
        for o in ROOF: o.hide_render = cutaway
        cam.location = loc; cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        cam_d.type = 'ORTHO' if ortho else 'PERSP'; cam_d.lens = lens; cam_d.ortho_scale = ortho or 1
        sc.render.resolution_x, sc.render.resolution_y = res
        sc.render.filepath = os.path.join(WIRE, name + '.png'); bpy.ops.render.render(write_still=True); print('TUG: view', name)
    AT = (0.0, 0.0, 0.85)
    shoot('1-three-quarter-front', (6.2, 5.6, 3.4), AT, 45)
    shoot('2-three-quarter-rear', (-6.2, -5.4, 3.4), AT, 45)
    shoot('3-side-left', (0, 12, 1.0), (0, 0, 1.0), 50, 6.4, res=(2400, 1200))
    shoot('4-front', (12, 0, 1.0), (0, 0, 1.0), 50, 3.4, res=(1500, 1200))
    shoot('5-rear', (-12, 0, 1.0), (0, 0, 1.0), 50, 3.4, res=(1500, 1200))
    shoot('6-top', (0, 0, 14), (0, 0, 0), 50, 6.4, res=(2400, 1400))
    CC = ((CAB_X0 + CAB_X1) / 2, (CAB_Y0 + CAB_Y1) / 2, 1.05)
    shoot('7-cab-interior', (2.0, CAB_Y1 + 2.0, 3.4), CC, 32, cutaway=True)
    shoot('8-operator-seat', (SX + 1.15, SY - 0.75, SZ + 1.55), (SX - 0.08, SY, SZ + 0.50), 35, cutaway=True)
