# The airport's crash tender (ARFF, 6x6), our own design in the manner of the newest airport fire trucks (owner,
# 2026-11-11: close to the photos, not a copy, at least as striking; the same big off-road wheels; the airport's name on
# its sides). Four design variants to choose from (owner: "4 different fire truck variations"):
#   A  the wedge: a steeply raked wraparound windscreen with black pillars, a body chamfered at the rear top, a low
#      jutting bumper turret on a long arm, the roof turret on the cab
#   B  the upright: a near-upright windscreen, a tall square cab and body, the roof turret on the body behind the cab,
#      a short bumper turret
#   C  the low-nose: the glass running down to a low line, the body's roof sloping down to the rear, a skirt over the
#      rear wheels, a folded boom (high-reach arm with its piercing tip) along the roof
#   D  the heavy: a split two-pane windscreen with a centre pillar, a raised tank deck, a bull bar, two roof turrets
# Common to all: the cab-forward cab (driver in the middle), roller-shutter lockers, black chevrons, MÝRDALSSANDUR
# AIRPORT on both sides and the truck's number. Parts the game moves or lights apart (each its own object): the two cab
# doors (hinged at their front edge), the six wheels (one wheel mesh, used six times), the steering wheel, and the
# lights by what they do (HeadLow, HeadHigh, Brake, TurnL, TurnR, Blue, Reverse). The cab glass is a hollow shell, so
# the interior (seats, dashboard with screens, steering wheel, pedals) is seen through it.
# Metres, x across (right +), y along (front +y), z up; the ground at z = 0.
# Blender 3.3:  blender -b --python Art/FireTruck/make_firetruck.py -- <number> [<variant A-D>] [<preview .blend>]
# Without a preview path: writes Art/FireTruck/FireTruck.blend and Art/FireTruck/firetruck<number>.glb (mesh names
# taken out). With one: saves only that .blend (for the variant previews).
import bpy, bmesh, math, os, sys
from mathutils import Vector, Quaternion

HERE = os.path.dirname(os.path.abspath(__file__))
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
NO = argv[0] if argv else '1'
VAR = (argv[1] if len(argv) > 1 else 'A').upper()
PREVIEW = argv[2] if len(argv) > 2 else ''
bpy.ops.wm.read_factory_settings(use_empty=True)
coll = bpy.context.scene.collection


def mat(name, color, metal=0.0, rough=0.5, emit=None, strength=0.0, alpha=1.0, coat=0.0):
    m = bpy.data.materials.get(name)
    if m: return m
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*color, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if coat: b.inputs['Clearcoat'].default_value = coat; b.inputs['Clearcoat Roughness'].default_value = 0.05
    if emit: b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = strength
    if alpha < 1.0:
        b.inputs['Alpha'].default_value = alpha; m.blend_method = 'BLEND'; m.shadow_method = 'HASHED'; m.use_backface_culling = False
    return m


M = {
    'paint': mat('TruckPaint', (0.62, 0.78, 0.02), 0.25, 0.28, coat=0.9),
    'black': mat('TruckBlack', (0.012, 0.012, 0.013), 0.1, 0.35, coat=0.5),
    'satin': mat('TruckSatin', (0.03, 0.032, 0.035), 0.2, 0.6),
    'rubber': mat('TruckRubber', (0.018, 0.018, 0.018), 0.0, 0.9),
    'rim': mat('TruckRim', (0.025, 0.026, 0.028), 0.6, 0.45),
    'chrome': mat('TruckChrome', (0.85, 0.86, 0.88), 1.0, 0.12),
    'alu': mat('TruckAlu', (0.62, 0.64, 0.66), 0.9, 0.32),
    'glass': mat('TruckGlass', (0.05, 0.07, 0.08), 0.0, 0.03, alpha=0.32),
    'lensClear': mat('TruckLensHead', (0.9, 0.92, 0.95), 0.0, 0.05),
    'lensHigh': mat('TruckLensHigh', (0.9, 0.92, 0.95), 0.0, 0.05),
    'lensRed': mat('TruckLensBrake', (0.55, 0.02, 0.02), 0.0, 0.1),
    'lensAmberL': mat('TruckLensTurnL', (0.9, 0.42, 0.02), 0.0, 0.1),
    'lensAmberR': mat('TruckLensTurnR', (0.9, 0.42, 0.02), 0.0, 0.1),
    'lensBlue': mat('TruckLensBlue', (0.02, 0.12, 0.85), 0.0, 0.1),
    'lensWhite': mat('TruckLensReverse', (0.85, 0.85, 0.85), 0.0, 0.1),
    'seat': mat('TruckSeat', (0.05, 0.055, 0.06), 0.0, 0.8),
    'dash': mat('TruckDash', (0.06, 0.065, 0.07), 0.0, 0.55),
    'screen': mat('TruckScreen', (0.02, 0.05, 0.08), 0.0, 0.1, (0.15, 0.35, 0.55), 1.5),
    'floor': mat('TruckFloor', (0.05, 0.05, 0.05), 0.0, 0.85),
    'decal': mat('TruckLettering', (0.01, 0.01, 0.012), 0.0, 0.4),
    'reflect': mat('TruckReflective', (0.92, 0.92, 0.9), 0.3, 0.25),
    'nozzle': mat('TruckNozzle', (0.12, 0.12, 0.13), 0.8, 0.3),
}


def finish(ob, material, smooth=True, angle=40):
    for m in (material if isinstance(material, (list, tuple)) else [material]): ob.data.materials.append(m)
    if smooth:
        for p in ob.data.polygons: p.use_smooth = True
        ob.data.use_auto_smooth = True; ob.data.auto_smooth_angle = math.radians(angle)
    return ob


def bevel(ob, w, seg=3):
    b = ob.modifiers.new('bevel', 'BEVEL'); b.width = w; b.segments = seg; b.limit_method = 'ANGLE'
    return ob


def new_obj(name, bm, material, bev=0.0, seg=3, angle=40):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); coll.objects.link(ob); finish(ob, material, True, angle)
    if bev: bevel(ob, bev, seg)
    return ob


def box(name, x0, y0, z0, x1, y1, z1, material, bev=0.0, seg=3):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector((abs(x1 - x0), abs(y1 - y0), abs(z1 - z0))), verts=bm.verts)
    ob = new_obj(name, bm, material, bev, seg); ob.location = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
    return ob


def _extrude(name, pts3a, pts3b, material, bev, seg):
    bm = bmesh.new()
    a = [bm.verts.new(p) for p in pts3a]; b = [bm.verts.new(p) for p in pts3b]
    bm.faces.new(a); bm.faces.new(list(reversed(b)))
    n = len(a)
    for i in range(n): bm.faces.new((a[i], b[i], b[(i + 1) % n], a[(i + 1) % n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, material, bev, seg)


def side(name, pts, x0, x1, material, bev=0.0, seg=3):
    """A side profile (y, z points) run across x from x0 to x1."""
    return _extrude(name, [(x0, y, z) for y, z in pts], [(x1, y, z) for y, z in pts], material, bev, seg)


def slab(name, outline, z0, z1, material, bev=0.0, seg=3):
    """A plan outline (x, y points) run up from z0 to z1."""
    return _extrude(name, [(x, y, z0) for x, y in outline], [(x, y, z1) for x, y in outline], material, bev, seg)


def strut(name, p0, p1, r, material):
    """A square bar from p0 to p1 (the cab's pillars and sills)."""
    p0, p1 = Vector(p0), Vector(p1); d = p1 - p0
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector((2 * r, 2 * r, d.length)), verts=bm.verts)
    ob = new_obj(name, bm, material, r * 0.4, 2)
    ob.rotation_mode = 'QUATERNION'; ob.rotation_quaternion = Vector((0, 0, 1)).rotation_difference(d.normalized())
    ob.location = (p0 + p1) / 2
    return ob


def cyl(name, r, depth, at, material, axis='x', verts=48, bev=0.0):
    rot = {'x': (0, math.pi / 2, 0), 'y': (math.pi / 2, 0, 0), 'z': (0, 0, 0)}[axis]
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=depth, location=at, rotation=rot)
    ob = bpy.context.active_object; ob.name = name
    finish(ob, material, True, 35)
    if bev: bevel(ob, bev, 3)
    return ob


def text(name, s, size, at, rot, material, align='CENTER', depth=0.004):
    cu = bpy.data.curves.new(name, 'FONT'); cu.body = s; cu.size = size; cu.extrude = depth; cu.align_x = align; cu.align_y = 'CENTER'
    ob = bpy.data.objects.new(name, cu); coll.objects.link(ob); ob.location = at; ob.rotation_euler = rot
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.object.convert(target='MESH'); ob = bpy.context.active_object; ob.data.materials.clear(); ob.data.materials.append(material)
    return ob


# ---------------------------------------------------------------- the four variants
W = 1.46                 # half the body's width
FRONT, REAR = 5.95, -5.85
AXLES = (3.55, -1.75, -3.55)
RW, TW = 0.72, 0.6        # wheel radius (big 24R21-class off-road tyre), tyre width
TRACK = 1.24              # wheel centres from the middle
CAB_Y0 = 2.1
BODY_Y0, BODY_Y1 = REAR + 0.15, 2.05
R0 = BODY_Y0
V = {
    # ZB/ZT the glass's bottom and top; YF the glass's front at the bottom; S how far back the windscreen leans at the
    # top; C the chamfer of the front corners; NOSE the lower cab's front slope; BODY the body's side profile (y, z)
    'A': dict(ZB=1.92, ZT=3.15, YF=5.62, S=0.95, C=0.42, NOSE=0.1,
              BODY=[(R0, 1.05), (BODY_Y1, 1.05), (BODY_Y1, 3.08), (R0 + 0.55, 3.08), (R0, 2.72)]),
    'B': dict(ZB=1.98, ZT=3.32, YF=5.66, S=0.32, C=0.22, NOSE=0.04,
              BODY=[(R0, 1.05), (BODY_Y1, 1.05), (BODY_Y1, 3.3), (R0, 3.3)]),
    'C': dict(ZB=1.62, ZT=3.05, YF=5.72, S=1.25, C=0.5, NOSE=0.16,
              BODY=[(R0, 1.05), (BODY_Y1, 1.05), (BODY_Y1, 3.1), (R0 + 2.2, 2.92), (R0 + 0.3, 2.5), (R0, 2.3)]),
    'D': dict(ZB=1.95, ZT=3.22, YF=5.6, S=0.6, C=0.3, NOSE=0.06,
              BODY=[(R0, 1.05), (BODY_Y1, 1.05), (BODY_Y1, 3.0), (R0, 3.0)]),
}[VAR]
ZB, ZT, YF, S, C = V['ZB'], V['ZT'], V['YF'], V['S'], V['C']
CAB_Y1 = FRONT - 0.25
D0, D1 = CAB_Y0 + 0.95, CAB_Y0 + 2.0      # the doors' rear and front edges


def ztop(y):
    """The body's roof height at y (its profile's upper edge)."""
    up = sorted([p for p in V['BODY'] if p[1] > 2.0], key=lambda p: p[0])
    if y <= up[0][0]: return up[0][1]
    for (y0, z0), (y1, z1) in zip(up, up[1:]):
        if y <= y1: return z0 + (z1 - z0) * (y - y0) / max(1e-6, y1 - y0)
    return up[-1][1]


# ---------------------------------------------------------------- the chassis and the wheels
box('Chassis', -0.55, REAR + 0.2, 0.55, 0.55, FRONT - 0.5, 0.95, M['satin'], 0.02)
for y in AXLES:
    cyl('Axle', 0.13, 2 * TRACK - 0.3, (0, y, RW), M['satin'], 'x', 24)
    box('Diff', -0.3, y - 0.28, RW - 0.25, 0.3, y + 0.28, RW + 0.2, M['satin'], 0.04)


def make_wheel():
    """A big off-road tyre (rounded shoulders, chunky lugs in two staggered, angled rows), a black dished rim, bolts, hub."""
    parts = [cyl('Tyre', RW - 0.045, TW, (0, 0, 0), M['rubber'], 'x', 72, 0.12)]
    n = 26
    for i in range(n):
        for s in (-1, 1):
            a = 2 * math.pi * (i + (0.5 if s > 0 else 0)) / n
            bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
            bmesh.ops.scale(bm, vec=Vector((0.25, 0.15, 0.075)), verts=bm.verts)
            lug = new_obj('Lug', bm, M['rubber'], 0.018, 2)
            lug.rotation_mode = 'QUATERNION'
            lug.rotation_quaternion = Quaternion((1, 0, 0), a) @ Quaternion((0, 0, 1), math.radians(22 * s))
            r = RW - 0.03
            lug.location = (s * 0.15, r * math.sin(a), -r * math.cos(a))
            parts.append(lug)
    parts.append(cyl('Rim', RW * 0.56, TW * 0.84, (0, 0, 0), M['rim'], 'x', 64, 0.02))
    parts.append(cyl('Dish', RW * 0.47, 0.05, (TW * 0.40, 0, 0), M['rim'], 'x', 64, 0.012))
    parts.append(cyl('DishRing', RW * 0.53, 0.04, (TW * 0.41, 0, 0), M['satin'], 'x', 64, 0.01))
    for k in range(10):
        a = 2 * math.pi * k / 10
        parts.append(cyl('Bolt', 0.024, 0.06, (TW * 0.44, math.cos(a) * RW * 0.3, math.sin(a) * RW * 0.3), M['chrome'], 'x', 6))
    for k in range(6):   # the dish's vent holes, as dark inset discs
        a = 2 * math.pi * (k + 0.5) / 6
        parts.append(cyl('Vent', 0.05, 0.02, (TW * 0.425, math.cos(a) * RW * 0.4, math.sin(a) * RW * 0.4), M['satin'], 'x', 20))
    parts.append(cyl('Hub', RW * 0.17, 0.14, (TW * 0.47, 0, 0), M['rim'], 'x', 32, 0.02))
    parts.append(cyl('HubCap', RW * 0.09, 0.05, (TW * 0.55, 0, 0), M['chrome'], 'x', 24))
    bpy.ops.object.select_all(action='DESELECT')
    for p in parts:
        bpy.context.view_layer.objects.active = p
        for m in list(p.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
    for p in parts: p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]; bpy.ops.object.join()
    w = bpy.context.active_object; w.name = 'WheelMesh'
    return w


wheel = make_wheel()
wheel.data.name = 'Wheel'
for i, y in enumerate(AXLES):
    for s in (-1, 1):
        ob = bpy.data.objects.new('Wheel_%s%d' % ('L' if s < 0 else 'R', i + 1), wheel.data)
        coll.objects.link(ob); ob.location = (s * TRACK, y, RW)
        if s < 0: ob.rotation_euler = (0, 0, math.pi)
bpy.data.objects.remove(wheel)

# ---------------------------------------------------------------- the body behind the cab
side('Body', V['BODY'], -W, W, M['paint'], 0.05, 4)
for y in AXLES[1:]:
    for s in (-1, 1):
        side('Flare', [(y - 0.95, 1.02), (y - 0.85, 1.55), (y + 0.85, 1.55), (y + 0.95, 1.02), (y + 0.8, 1.02), (y + 0.7, 1.42), (y - 0.7, 1.42), (y - 0.8, 1.02)],
             s * (W + 0.02), s * (W + 0.1), M['black'], 0.01)
if VAR == 'C':   # a skirt over the rear wheels' upper part, from the front of the rear pair to the back
    for s in (-1, 1):
        side('Skirt', [(AXLES[2] - 1.0, 0.95), (AXLES[1] + 1.0, 0.95), (AXLES[1] + 1.0, 1.45), (AXLES[2] - 1.0, 1.45)],
             s * (W + 0.1), s * (W + 0.15), M['black'], 0.02)
box('SillL', -W - 0.04, BODY_Y0 + 0.3, 0.98, -W + 0.02, BODY_Y1 - 0.2, 1.1, M['black'], 0.01)
box('SillR', W - 0.02, BODY_Y0 + 0.3, 0.98, W + 0.04, BODY_Y1 - 0.2, 1.1, M['black'], 0.01)
# the lockers: roller shutters in frames, on both sides
for s in (-1, 1):
    x = s * (W + 0.005)
    for (a, b) in [(-0.75, 1.85), (-5.55, -4.75)]:
        top = min(2.3, ztop(a) - 0.35, ztop(b) - 0.35)
        box('LockerFrame', x - 0.012, a, 1.2, x + 0.012, b, top, M['satin'], 0.006)
        box('Shutter', x + s * 0.006 - 0.006, a + 0.05, 1.25, x + s * 0.006 + 0.006, b - 0.05, top - 0.05, M['paint'], 0.004)
        z = 1.29
        while z < top - 0.06:
            box('Rib', x + s * 0.014 - 0.004, a + 0.06, z, x + s * 0.014 + 0.004, b - 0.06, z + 0.012, M['paint'], 0.003, 2); z += 0.075
        box('ShutterBar', x + s * 0.022 - 0.012, a + 0.06, 1.27, x + s * 0.022 + 0.012, b - 0.06, 1.31, M['alu'], 0.006)
    box('Seam', x - 0.004, BODY_Y0 + 0.2, 2.36, x + 0.004, BODY_Y1 - 0.1, 2.38, M['satin'])
# the roof: rails along both sides on posts (not on the sloping roof of C), the deck, the ladder at the rear
if VAR != 'C':
    for s in (-1, 1):
        for y in (-5.3, -3.6, -1.9, -0.2, 1.4):
            cyl('RailPost', 0.022, 0.42, (s * (W - 0.18), y, ztop(y) + 0.21), M['alu'], 'z', 12)
        box('Rail', s * (W - 0.18) - 0.025, -5.35, ztop(0) + 0.4, s * (W - 0.18) + 0.025, 1.45, ztop(0) + 0.45, M['alu'], 0.012)
if VAR == 'D':   # the raised tank deck
    box('TankDeck', -1.05, -4.9, ztop(0) - 0.02, 1.05, 1.0, ztop(0) + 0.35, M['paint'], 0.12, 5)
elif VAR != 'C':
    box('DeckCover', -0.9, -4.8, ztop(0) - 0.02, 0.9, 0.9, ztop(0) + 0.08, M['satin'], 0.03)
ztr = ztop(REAR + 0.2)
for i in range(int((ztr - 1.2) / 0.27) + 1):
    box('LadderRung', -0.28, REAR + 0.05, 1.2 + i * 0.27, 0.28, REAR + 0.11, 1.23 + i * 0.27, M['alu'], 0.008)
for s in (-1, 1):
    box('LadderSide', s * 0.3 - 0.02, REAR + 0.04, 1.1, s * 0.3 + 0.02, REAR + 0.12, ztr + 0.1, M['alu'], 0.01)
# the rear: the black lower band, the chevron board, the light clusters
box('RearBand', -W, REAR + 0.1, 1.05, W, REAR + 0.16, 1.55, M['black'], 0.01)
for i in range(9):
    x = -W + 0.12 + i * 0.32
    bar = box('RearChevron', x, REAR + 0.09, 1.62, x + 0.13, REAR + 0.1, 2.2, M['black'], 0.0)
    bar.rotation_euler = (0, math.radians(35 if x < 0 else -35), 0)
for s in (-1, 1):
    box('RearLampHousing', s * (W - 0.42) - 0.24, REAR + 0.08, 1.15, s * (W - 0.42) + 0.24, REAR + 0.12, 1.45, M['black'], 0.01)
    box('Light_Brake', s * (W - 0.32) - 0.08, REAR + 0.06, 1.2, s * (W - 0.32) + 0.08, REAR + 0.085, 1.4, M['lensRed'], 0.008)
    box('Light_Turn' + ('L' if s < 0 else 'R'), s * (W - 0.52) - 0.07, REAR + 0.06, 1.2, s * (W - 0.52) + 0.07, REAR + 0.085, 1.4, M['lensAmberL' if s < 0 else 'lensAmberR'], 0.008)
    box('Light_Reverse', s * (W - 0.72) - 0.05, REAR + 0.06, 1.2, s * (W - 0.72) + 0.05, REAR + 0.085, 1.4, M['lensWhite'], 0.008)
    box('Light_BlueRear', s * (W - 0.1) - 0.05, REAR + 0.1, ztr - 0.42, s * (W - 0.1) + 0.05, REAR + 0.13, ztr - 0.2, M['lensBlue'], 0.01)
    box('Mudflap', s * TRACK - 0.32, AXLES[2] - 0.98, 0.28, s * TRACK + 0.32, AXLES[2] - 0.95, 0.95, M['rubber'], 0.01)

# ---------------------------------------------------------------- the cab: the lower cab, the glass shell, its pillars
n = V['NOSE']
side('CabLower', [(CAB_Y0, 0.95), (FRONT - 0.15, 0.95), (FRONT - 0.05, 1.25), (FRONT - 0.05 - n, 1.55), (YF, ZB), (CAB_Y0, ZB)], -W, W, M['paint'], 0.05, 4)
# the glass shell: a plan outline at the bottom and the same outline leaned back at the top; the door openings left
# open (the doors carry their own glass); the back wall solid
bot = [(-W, CAB_Y0), (-W, D0), (-W, D1), (-W, YF - C), (-W + C, YF), (W - C, YF), (W, YF - C), (W, D1), (W, D0), (W, CAB_Y0)]
lean = lambda x, y: (x, y - S * max(0.0, (y - D1)) / max(1e-6, YF - D1))
top = [lean(x, y) for x, y in bot]
bm = bmesh.new()
vb = [bm.verts.new((x, y, ZB)) for x, y in bot]; vt = [bm.verts.new((x, y, ZT)) for x, y in top]
for i in range(len(bot)):
    j = (i + 1) % len(bot)
    if (i, j) in ((1, 2), (7, 8)): continue
    f = bm.faces.new((vb[i], vb[j], vt[j], vt[i])); f.material_index = 1 if j == 0 else 0
new_obj('Glass_Cab', bm, [M['glass'], M['black']], angle=20)
# the pillars at every corner of the glass, the sill under it, the roof over it
for i in range(1, 9):
    strut('Pillar', (*bot[i], ZB), (*top[i], ZT), 0.045 if i in (3, 4, 5, 6) else 0.035, M['black'])
if VAR == 'D': strut('CentrePillar', (0, YF, ZB), (0, YF - S, ZT), 0.04, M['black'])
for i in range(2, 7):
    strut('Sill', (*bot[i], ZB + 0.02), (*bot[i + 1], ZB + 0.02), 0.035, M['black'])
slab('CabRoof', [(x * 1.01, y + (0.04 if y > D1 else 0)) for x, y in top], ZT - 0.02, ZT + 0.14, M['paint'], 0.05, 4)
roof_front = top[4][1]
# the front face: the grille, the LED headlamp clusters, the turn lamps and the blue strobes, the bumper
box('Grille', -0.75, FRONT - 0.1, 1.05, 0.75, FRONT - 0.04, 1.5, M['black'], 0.02)
for i in range(8):
    box('GrilleSlat', -0.72, FRONT - 0.045, 1.09 + i * 0.05, 0.72, FRONT - 0.03, 1.105 + i * 0.05, M['satin'], 0.004)
for s in (-1, 1):
    hx = s * 1.08
    box('LampHousing', hx - 0.3, FRONT - 0.13, 1.18, hx + 0.3, FRONT - 0.04, 1.5, M['black'], 0.03)
    box('Light_HeadLow', hx - 0.26, FRONT - 0.045, 1.33, hx - 0.02, FRONT - 0.03, 1.46, M['lensClear'], 0.01)
    box('Light_HeadHigh', hx + 0.02, FRONT - 0.045, 1.33, hx + 0.26, FRONT - 0.03, 1.46, M['lensHigh'], 0.01)
    box('Light_Turn' + ('L' if s < 0 else 'R'), hx - 0.26, FRONT - 0.045, 1.22, hx + 0.26, FRONT - 0.03, 1.29, M['lensAmberL' if s < 0 else 'lensAmberR'], 0.008)
    box('Light_BlueCorner', s * W - 0.03, CAB_Y1 - 0.4, 1.4, s * W + 0.03, CAB_Y1 - 0.1, 1.5, M['lensBlue'], 0.008)
box('Bumper', -W + 0.05, FRONT - 0.2, 0.62, W - 0.05, FRONT + 0.12, 0.98, M['black'], 0.05, 4)
box('BumperStep', -1.0, FRONT + 0.05, 0.55, 1.0, FRONT + 0.25, 0.62, M['alu'], 0.01)
for s in (-1, 1):
    box('FogLamp', s * 1.05 - 0.12, FRONT + 0.11, 0.72, s * 1.05 + 0.12, FRONT + 0.125, 0.86, M['lensClear'], 0.02)
text('NumberFront', NO, 0.2, (0.0, FRONT + 0.125, 0.8), (math.pi / 2, 0, 0), M['decal'])
if VAR == 'A':   # a low jutting bumper turret on a long arm
    cyl('BumperTurretBase', 0.15, 0.2, (0.0, FRONT + 0.2, 1.05), M['nozzle'], 'z', 24, 0.01)
    box('BumperTurretArm', -0.06, FRONT + 0.15, 1.1, 0.06, FRONT + 0.7, 1.22, M['nozzle'], 0.02)
    cyl('BumperNozzle', 0.065, 0.4, (0.0, FRONT + 0.9, 1.16), M['nozzle'], 'y', 24, 0.01)
elif VAR == 'B':  # a short turret on the bumper's top
    cyl('BumperTurretBase', 0.13, 0.18, (0.0, FRONT + 0.0, 1.07), M['nozzle'], 'z', 24, 0.01)
    cyl('BumperNozzle', 0.06, 0.45, (0.0, FRONT + 0.3, 1.2), M['nozzle'], 'y', 24, 0.01)
elif VAR == 'D':  # a bull bar round the front
    for s in (-1, 1):
        cyl('BullPost', 0.04, 0.75, (s * 1.25, FRONT + 0.32, 1.0), M['alu'], 'z', 16)
    cyl('BullBar', 0.04, 2.5, (0, FRONT + 0.32, 1.35), M['alu'], 'x', 16)
    cyl('BullBarLow', 0.04, 2.5, (0, FRONT + 0.32, 0.95), M['alu'], 'x', 16)
    cyl('BumperNozzle', 0.06, 0.35, (0.0, FRONT + 0.45, 1.15), M['nozzle'], 'y', 24, 0.01)


def turret(nm, y, z, reach=0.7):
    cyl(nm + 'Base', 0.22, 0.2, (0, y, z + 0.1), M['nozzle'], 'z', 32, 0.02)
    box(nm + 'Body', -0.16, y - 0.25, z + 0.15, 0.16, y + 0.25, z + 0.45, M['paint'], 0.04)
    cyl(nm + 'Nozzle', 0.08, reach, (0, y + 0.25 + reach / 2, z + 0.32), M['nozzle'], 'y', 24, 0.01)


zr = ZT + 0.14
if VAR in ('A', 'C'): turret('RoofTurret', roof_front - 0.65, zr)
if VAR == 'B': turret('RoofTurret', 1.2, ztop(1.2), 1.1)
if VAR == 'D': turret('RoofTurret', 0.6, ztop(0.6) + 0.35, 0.9); turret('RoofTurret2', -4.0, ztop(-4.0) + 0.35, 0.6)
if VAR == 'C':  # the folded boom (high-reach arm with its piercing tip) along the roof
    zb = ztop(-2.0)
    box('BoomBase', -0.35, 1.0, ztop(1.0) - 0.02, 0.35, 1.8, ztop(1.0) + 0.35, M['satin'], 0.04)
    box('Boom', -0.16, -4.6, zb + 0.45, 0.16, 1.6, zb + 0.75, M['paint'], 0.05)
    box('BoomInner', -0.11, -5.2, zb + 0.5, 0.11, -4.6, zb + 0.7, M['alu'], 0.03)
    cyl('Piercer', 0.05, 0.6, (0, -5.5, zb + 0.6), M['nozzle'], 'y', 16)
    for y in (-3.6, -0.8):
        box('BoomRest', -0.1, y - 0.1, ztop(y) - 0.02, 0.1, y + 0.1, zb + 0.46, M['satin'])
bar_y = CAB_Y0 + 0.55
box('LightBar', -1.05, bar_y - 0.15, zr, 1.05, bar_y + 0.15, zr + 0.12, M['black'], 0.03)
for i in range(6):
    x = -0.95 + i * 0.38
    box('Light_BlueBar', x, bar_y - 0.17, zr + 0.01, x + 0.28, bar_y + 0.17, zr + 0.11, M['lensBlue'], 0.01)
# the mirrors on their arms, the wipers, the cab's steps and grab handles, the side turn lamps
for s in (-1, 1):
    my = top[3][1] - 0.15
    box('MirrorArm', s * W, my - 0.03, ZT - 0.28, s * (W + 0.35), my + 0.03, ZT - 0.24, M['black'], 0.01)
    box('Mirror', s * (W + 0.32) - 0.06, my - 0.12, ZT - 0.65, s * (W + 0.32) + 0.06, my + 0.06, ZT - 0.2, M['black'], 0.03)
    box('Wiper', s * 0.45 - 0.42, YF - 0.12, ZB + 0.06, s * 0.45 + 0.42, YF - 0.04, ZB + 0.1, M['black'], 0.005)
    box('Step', s * (W - 0.2), D0 + 0.05, 0.62, s * (W + 0.05), D1 - 0.4, 0.66, M['alu'], 0.01)
    box('Step2', s * (W - 0.2), D0 + 0.05, 0.32, s * (W + 0.05), D1 - 0.4, 0.36, M['alu'], 0.01)
    cyl('Grab', 0.018, 0.9, (s * (W + 0.06), D0 - 0.05, 2.3), M['alu'], 'z', 12)
    box('Light_TurnSide' + ('L' if s < 0 else 'R'), s * (W + 0.01) - 0.01, CAB_Y0 + 0.3, 1.45, s * (W + 0.01) + 0.01, CAB_Y0 + 0.5, 1.52, M['lensAmberL' if s < 0 else 'lensAmberR'], 0.006)

# ---------------------------------------------------------------- the cab doors (hinged at their front edge)
for s, nm in ((-1, 'L'), (1, 'R')):
    side('Door' + nm, [(D0, 0.98), (D1, 0.98), (D1, ZB), (D0, ZB)], s * (W + 0.005), s * (W + 0.055), M['paint'], 0.02, 3)
    for (a, b, c, d) in [(D0, ZB, D1, ZB + 0.06), (D0, ZT - 0.06, D1, ZT), (D0, ZB, D0 + 0.06, ZT), (D1 - 0.06, ZB, D1, ZT)]:
        side('DoorFrame' + nm, [(a, b), (c, b), (c, d), (a, d)], s * (W + 0.005), s * (W + 0.05), M['black'], 0.01)
    side('Glass_Door' + nm, [(D0 + 0.04, ZB + 0.04), (D1 - 0.04, ZB + 0.04), (D1 - 0.04, ZT - 0.04), (D0 + 0.04, ZT - 0.04)], s * (W + 0.02), s * (W + 0.03), M['glass'])
    box('DoorHandle' + nm, s * (W + 0.06) - 0.012, D0 + 0.15, ZB - 0.2, s * (W + 0.06) + 0.012, D0 + 0.4, ZB - 0.15, M['alu'], 0.006)

# ---------------------------------------------------------------- the livery: black stripes, chevrons, name, number
for s in (-1, 1):
    x0, x1 = s * (W + 0.002), s * (W + 0.006)
    side('Stripe', [(BODY_Y0 + 1.6, 1.6), (BODY_Y1 - 0.02, 1.6), (BODY_Y1 - 0.02, 1.72), (BODY_Y0 + 1.6, 1.72)], x0, x1, M['black'])
    side('Stripe2', [(BODY_Y0 + 1.6, 1.8), (BODY_Y1 - 0.02, 1.8), (BODY_Y1 - 0.02, 1.86), (BODY_Y0 + 1.6, 1.86)], x0, x1, M['black'])
    side('CabStripe', [(CAB_Y0, 1.25), (D0, 1.25), (D0, 1.37), (CAB_Y0, 1.37)], x0, x1, M['black'])
    for i in range(5):
        y = BODY_Y0 + 0.25 + i * 0.26
        side('Chevron', [(y, 1.55), (y + 0.12, 1.55), (y + 0.42, 2.2), (y + 0.3, 2.2)], x0, x1, M['black'])
    rot = (math.pi / 2, 0, math.pi / 2) if s > 0 else (math.pi / 2, 0, -math.pi / 2)
    xt = s * (W + 0.007)
    text('Name1', 'MÝRDALSSANDUR', 0.34, (xt, -1.0, 2.62), rot, M['decal'])
    text('Name2', 'AIRPORT', 0.34, (xt, -1.0, 2.2), rot, M['decal'])
    text('Rescue', 'AIRCRAFT RESCUE & FIRE FIGHTING', 0.1, (xt, -1.0, 1.95), rot, M['decal'])
    text('Number', NO, 0.45, (xt, -4.25, min(2.62, ztop(-4.25) - 0.45)), rot, M['decal'])
    cyl('EmblemRing', 0.22, 0.008, (xt, 1.25, 2.55), M['decal'], 'x', 48)
    cyl('EmblemFill', 0.19, 0.01, (xt + s * 0.001, 1.25, 2.55), M['paint'], 'x', 48)
    text('Emblem', 'BIAP', 0.11, (xt + s * 0.006, 1.25, 2.55), rot, M['decal'])
text('NumberRear', NO, 0.4, (0.85, REAR + 0.085, ztr - 0.6), (math.pi / 2, 0, math.pi), M['decal'])
text('NumberRoof', NO, 0.9, (0.0, -2.0, ztop(-2.0) + (0.35 if VAR == 'D' else 0.09)), (0, 0, 0), M['decal']) if VAR != 'C' else None

# ---------------------------------------------------------------- the cab interior: the driver in the middle
box('CabFloor', -W + 0.1, CAB_Y0 + 0.1, 0.98, W - 0.1, CAB_Y1 - 0.2, 1.02, M['floor'])
dash_top = max(1.75, ZB + 0.05)
box('Dash', -W + 0.12, CAB_Y1 - 0.75, 1.02, W - 0.12, YF - 0.12, dash_top, M['dash'], 0.06, 4)
for x in (-0.55, 0.0, 0.55):
    box('Screen', x - 0.2, CAB_Y1 - 0.77, dash_top - 0.28, x + 0.2, CAB_Y1 - 0.75, dash_top - 0.04, M['screen'], 0.005)
box('Console', -0.18, CAB_Y1 - 1.6, 1.02, 0.18, CAB_Y1 - 0.75, 1.5, M['dash'], 0.03)
box('Joystick', -0.03, CAB_Y1 - 1.25, 1.5, 0.03, CAB_Y1 - 1.19, 1.68, M['black'], 0.01)
col = cyl('SteerColumn', 0.05, 0.5, (0, CAB_Y1 - 0.95, 1.65), M['black'], 'y', 16)
col.rotation_euler = (math.radians(90 - 35), 0, 0)
bpy.ops.mesh.primitive_torus_add(major_radius=0.22, minor_radius=0.022, location=(0, CAB_Y1 - 1.13, 1.82), rotation=(math.radians(-35), 0, 0))
swo = bpy.context.active_object; swo.name = 'SteeringWheel'; finish(swo, M['black'])
for a in (0, 120, 240):
    sp = box('Spoke', -0.012, -0.012, -0.11, 0.012, 0.012, 0.11, M['black'])
    sp.parent = swo; sp.location = (math.sin(math.radians(a)) * 0.11, 0, math.cos(math.radians(a)) * 0.11); sp.rotation_euler = (0, math.radians(a), 0)


def seat(nm, x, y):
    box(nm + 'Base', x - 0.26, y - 0.25, 1.02, x + 0.26, y + 0.25, 1.48, M['seat'], 0.06, 4)
    box(nm + 'Cushion', x - 0.25, y - 0.24, 1.48, x + 0.25, y + 0.26, 1.6, M['seat'], 0.05, 4)
    bk = box(nm + 'Back', x - 0.25, y - 0.42, 1.55, x + 0.25, y - 0.28, 2.4, M['seat'], 0.06, 4); bk.rotation_euler = (math.radians(-10), 0, 0)
    box(nm + 'Head', x - 0.16, y - 0.47, 2.42, x + 0.16, y - 0.37, 2.68, M['seat'], 0.04, 4)


seat('SeatDriver', 0.0, CAB_Y1 - 1.75)
seat('SeatL', -0.85, CAB_Y1 - 1.55)
seat('SeatR', 0.85, CAB_Y1 - 1.55)
seat('SeatRearL', -0.55, CAB_Y0 + 0.75)
seat('SeatRearR', 0.55, CAB_Y0 + 0.75)
for x in (-0.12, 0.12):
    box('Pedal', x - 0.05, CAB_Y1 - 0.85, 1.05, x + 0.05, CAB_Y1 - 0.8, 1.25, M['black'], 0.01)
box('BackWall', -W + 0.1, CAB_Y0 + 0.03, 1.0, W - 0.1, CAB_Y0 + 0.1, ZT, M['dash'])
box('Headliner', -W + 0.1, CAB_Y0 + 0.1, ZT - 0.05, W - 0.1, roof_front - 0.1, ZT - 0.02, M['dash'])

# every mesh with its material (no empty slots)
for ob in bpy.data.objects:
    if ob.type == 'MESH':
        assert len(ob.data.materials) and all(m is not None for m in ob.data.materials), ob.name

if PREVIEW:
    bpy.ops.wm.save_as_mainfile(filepath=PREVIEW)
    print('TRUCK DONE', NO, VAR, PREVIEW)
else:
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'FireTruck.blend'))
    bpy.ops.object.select_all(action='SELECT')
    out = os.path.join(HERE, 'firetruck%s.glb' % NO)
    bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_apply=True, export_format='GLB')
    import runpy
    _tool = os.path.join(HERE, '..', '..', 'Tools', 'glb_strip_mesh_names.py')
    sys.argv = [_tool, out]
    runpy.run_path(_tool, run_name='__main__')
    print('TRUCK DONE', NO, VAR)
