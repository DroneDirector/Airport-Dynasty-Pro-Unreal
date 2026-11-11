# The airport's crash tender modelled after the owner's reference photos of the 6x6 airport fire truck (Panther-type),
# as a working base (owner, 2026-11-11: to be licensed or redesigned before release). No maker's badges or names; our
# own livery: MÝRDALSSANDUR AIRPORT and the truck's number.
# The shape: a cab well ahead of the front axle, all black glass above a yellow band, the windscreen wrapping round
# rounded corners, black doors nearly all glass, a black lower front with yellow corner wings and slim LED lamps; the
# front wheel under the cab's rear with a yellow fender; a tall body standing higher than the cab, a tall roller-shutter
# locker behind the cab, a lower locker between the axles, the rear module raised a step, double black stripes stepping
# down towards the rear into chevrons; LED light columns at the seams; roof hoops; a roof turret and light bar on the cab.
# Parts the game moves or lights apart: the doors (DoorL/DoorR with their glass and frame, hinged at the front edge),
# six wheels (one mesh), the steering wheel, lights by what they do (Light_HeadLow/HeadHigh/Brake/TurnL/TurnR/Blue*/
# Reverse). Metres, x across (right +), y along (front +y), z up; the ground at z = 0.
# Blender 3.3:  blender -b --python Art/FireTruck/make_panther.py -- <number> [<preview .blend>]
import bpy, math, os, sys
from mathutils import Vector, Quaternion, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
NO = argv[0] if argv else '1'
PREVIEW = argv[1] if len(argv) > 1 else ''
bpy.ops.wm.read_factory_settings(use_empty=True)
sys.path.insert(0, HERE)
from truck_kit import *          # noqa: E402,F403  (mat, M, box, side, slab, strut, cyl, text, new_obj, bmesh)
import bmesh                     # noqa: E402

W = 1.5                          # half width (3.0 m)
FRONT, REAR = 6.05, -6.0
AXLES = (2.85, -2.55, -4.2)
RW, TW = 0.75, 0.62              # 1.5 m off-road tyres
TRACK = 1.22
CAB_R = 2.2                      # the cab's rear
DOOR0, DOOR1 = 3.68, 5.02        # the door opening along y
WELL_Y0, WELL_Y1, WELL_TOP, WELL_X = 1.98, 3.86, 1.68, 0.54   # the front wheel housings: from, to, top, inner wall
STAIR_Y0, STAIR_Y1 = 4.30, 4.94                                  # the cab stairs, along y (behind the doors)
KINGPIN, MAX_STEER = 0.12, 25.0  # the front wheels steer about an axis 12 cm in from the wheel's centre, 25 deg each way
REAR_STEER_MAX = 10.0             # the rearmost axle (Wheel_*3) steers the other way, up to 10 deg (owner: rear-axle steering);
                                  # its angle from the front's: atan(1.65 * tan(front) / 5.4) (the middle axle the pivot line)
ZB, ZBR, ZT = 1.62, 2.05, 2.98   # glass bottom (front, behind the door) and top
YF, S, RC = 5.82, 0.85, 0.62     # the glass's front at the bottom, its lean back at the top, the corner radius
BODY_TOP, REAR_TOP = 3.35, 3.55

def tube(name, pts, r, material, sides=10, closed=False):
    """A round tube along a polyline of (x, y, z) points: one smooth piece, no segments or gaps."""
    P = [Vector(q) for q in pts]; bm = bmesh.new(); rings = []
    for k, c in enumerate(P):
        t = (P[min(k + 1, len(P) - 1)] - P[max(k - 1, 0)]).normalized()
        up = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
        n1 = t.cross(up).normalized(); n2 = t.cross(n1).normalized()
        rings.append([bm.verts.new(c + (n1 * math.cos(2 * math.pi * j / sides) + n2 * math.sin(2 * math.pi * j / sides)) * r) for j in range(sides)])
    for a_, b_ in zip(rings, rings[1:] + ([rings[0]] if closed else [])):
        for j in range(sides): bm.faces.new((a_[j], a_[(j + 1) % sides], b_[(j + 1) % sides], b_[j]))
    if not closed:
        bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, material, angle=80)




def housed_lamp(name, lens, p, n, w, h, depth_in=0.03, proud=0.022):
    """A lamp as on a real truck: a black housing set into the body at p (its face along the outward normal n), the lens
    in a shallow recess in its front, nothing floating. The lens object carries the light's name (switched by the game)."""
    n = Vector(n).normalized(); yaw = math.atan2(-n.x, n.y)
    Rz = Matrix.Rotation(yaw, 3, 'Z')
    tilt = math.asin(max(-1.0, min(1.0, n.z)))
    Rx = Matrix.Rotation(tilt, 3, 'X')
    R = Rz @ Rx

    def put(ob, c):
        ob.location = Vector(p) + R @ Vector(c); ob.rotation_euler = (tilt, 0, yaw)
        return ob
    put(box(name + 'Housing', -(w / 2 + 0.018), -(depth_in + proud) / 2, -(h / 2 + 0.018), w / 2 + 0.018, (depth_in + proud) / 2, h / 2 + 0.018, M['satin'], 0.008),
        (0, (proud - depth_in) / 2, 0))
    put(box(name, -w / 2, -0.006, -h / 2, w / 2, 0.006, h / 2, lens, 0.004), (0, proud + 0.001, 0))   # the lens just proud of the housing's face
    return R


def front_surface(x, z):
    """A point on the cab's lower front at (x, z) and its outward normal (from the same rings the front is lofted from)."""
    RS = [(0.6, 5.15, 5.95, W - 0.08), (0.82, 5.2, 6.12, W - 0.02), (1.0, 5.15, 6.08, W), (1.32, 5.1, 6.0, W)]
    for (z0, a0, b0, c0), (z1, a1, b1, c1) in zip(RS, RS[1:]):
        if z0 <= z <= z1:
            t = (z - z0) / (z1 - z0); ys, yf, hw = a0 + (a1 - a0) * t, b0 + (b1 - b0) * t, c0 + (c1 - c0) * t
            break
    cv = front_curve(ys, yf, hw, k=400)
    i = min(range(len(cv)), key=lambda k: abs(cv[k][0] - x) + (0 if cv[k][1] > ys + 0.05 else 1))
    (xa, ya), (xb, yb) = cv[max(0, i - 1)], cv[min(len(cv) - 1, i + 1)]
    nrm = Vector((yb - ya, -(xb - xa), 0)).normalized()
    if nrm.y < 0: nrm = -nrm
    return Vector((cv[i][0], cv[i][1], z)), nrm

# ---------------------------------------------------------------- chassis and wheels
box('Chassis', -0.55, REAR + 0.3, 0.5, 0.55, FRONT - 0.6, 0.95, M['satin'], 0.02)
for y in AXLES:
    box('Diff', -0.32, y - 0.3, RW - 0.26, 0.32, y + 0.3, RW + 0.22, M['satin'], 0.04)


CALIPER_RED = mat('TruckBoltRed', (0.7, 0.04, 0.02), 0.2, 0.35)


def make_wheel():
    """The wheel as in the owner's photos: a tall off-road tyre (a turned profile: rounded shoulders, a slightly bulging
    sidewall, the bead), its tread of big staggered blocks that wrap over the shoulders onto the sidewall, a row of
    smaller blocks down the middle; a deep-set black dish rim with a thin outer flange, and a small domed centre cap with
    red-capped bolts. Built for the right side (outward +x); the left wheels are the same mesh turned half round."""
    hw = TW / 2; R = RW - 0.065                                   # the carcass under the blocks
    prof = [(-hw + 0.05, 0.40), (-hw + 0.01, 0.45), (-hw - 0.012, 0.53), (-hw - 0.004, 0.61), (-hw + 0.03, 0.665),
            (-hw + 0.085, R), (hw - 0.085, R), (hw - 0.03, 0.665), (hw + 0.004, 0.61), (hw + 0.012, 0.53),
            (hw - 0.01, 0.45), (hw - 0.05, 0.40)]
    seg = 96
    bm = bmesh.new(); rings = []
    for k in range(seg):
        a = 2 * math.pi * k / seg
        rings.append([bm.verts.new((x, r * math.sin(a), -r * math.cos(a))) for x, r in prof])
    for k in range(seg):
        r0, r1 = rings[k], rings[(k + 1) % seg]
        for i in range(len(prof) - 1): bm.faces.new((r0[i], r0[i + 1], r1[i + 1], r1[i]))
        bm.faces.new((r0[-1], r0[0], r1[0], r1[-1]))              # the inside, across the bead seat
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.normal_update()
    # the sidewalls carry the moulded markings (T_Tyre_Sidewall, one flat map 1.5 m across, read from the side); the
    # tread its rubber grain (u round the tyre, v across it, in 15 cm tiles)
    side_m = bpy.data.materials.get('TruckTyreSidewall')
    if side_m is None:
        side_m = bpy.data.materials.new('TruckTyreSidewall'); side_m.use_nodes = True
        nt = side_m.node_tree; bs = nt.nodes['Principled BSDF']
        t1 = nt.nodes.new('ShaderNodeTexImage'); t1.image = bpy.data.images.load(os.path.join(HERE, 'T_Tyre_Sidewall.png'))
        t2 = nt.nodes.new('ShaderNodeTexImage'); t2.image = bpy.data.images.load(os.path.join(HERE, 'T_Tyre_Sidewall_N.png'))
        t2.image.colorspace_settings.name = 'Non-Color'
        nm_ = nt.nodes.new('ShaderNodeNormalMap'); nm_.inputs['Strength'].default_value = 1.4
        nt.links.new(t1.outputs['Color'], bs.inputs['Base Color']); nt.links.new(t2.outputs['Color'], nm_.inputs['Color'])
        nt.links.new(nm_.outputs['Normal'], bs.inputs['Normal'])
        bs.inputs['Roughness'].default_value = 0.82
    uvl = bm.loops.layers.uv.new('UVMap')
    for f in bm.faces:
        side = abs(f.normal.x) > 0.55
        f.material_index = 1 if side else 0
        for l in f.loops:
            co = l.vert.co
            if side: l[uvl].uv = (co.y / 1.5 + 0.5, co.z / 1.5 + 0.5)
            else: l[uvl].uv = (math.atan2(co.y, -co.z) * R / 0.15, co.x / 0.15)
    parts = [new_obj('Tyre', bm, [M['rubber'], side_m], angle=50)]

    def block(ax0, ax1, tang, h, r_mid, ang, chev):
        bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
        bmesh.ops.scale(bm, vec=Vector((ax1 - ax0, tang, h)), verts=bm.verts)
        o = new_obj('Lug', bm, M['rubber'], 0.012, 2)
        o.rotation_mode = 'QUATERNION'
        o.rotation_quaternion = Quaternion((1, 0, 0), ang) @ Quaternion((0, 0, 1), math.radians(chev))
        o.location = ((ax0 + ax1) / 2, r_mid * math.sin(ang), -r_mid * math.cos(ang))
        return o

    n = 30
    for k in range(n):
        for sd in (-1, 1):
            a = 2 * math.pi * (k + (0.5 if sd > 0 else 0)) / n
            # the big shoulder block across the tread, angled (a directional chevron), and its lug down the shoulder
            ax0, ax1 = (0.03, hw + 0.012) if sd > 0 else (-hw - 0.012, -0.03)
            parts.append(block(ax0, ax1, 0.105, 0.07, R + 0.03, a, 16 * sd))
            x_sh = sd * (hw - 0.02)
            parts.append(block(x_sh - 0.032, x_sh + 0.032, 0.1, 0.11, R - 0.025, a, 8 * sd))   # under the overhang
        # the smaller centre block, between the shoulder blocks
        a = 2 * math.pi * (k + 0.25) / n
        parts.append(block(-0.07, 0.07, 0.07, 0.06, R + 0.025, a, 0))
    # the rim: barrel, a deep-set dish, the outer flange, the domed centre cap and its red-capped bolts
    parts.append(cyl('RimBarrel', 0.405, TW - 0.22, (-0.06, 0, 0), M['rim'], 'x', 96))
    bpy.ops.mesh.primitive_torus_add(major_radius=0.405, minor_radius=0.016, major_segments=96, minor_segments=12, location=(hw - 0.07, 0, 0), rotation=(0, math.pi / 2, 0))
    _fl = bpy.context.active_object; _fl.name = 'RimFlange'; finish(_fl, M['rim'])   # a ring: the dish stays open to view
    parts.append(_fl)
    parts.append(cyl('RimDish', 0.395, 0.03, (hw - 0.17, 0, 0), M['rim'], 'x', 96, 0.01))
    parts.append(cyl('RimDishRing', 0.30, 0.02, (hw - 0.155, 0, 0), M['satin'], 'x', 96, 0.006))
    parts.append(cyl('HubCap', 0.15, 0.06, (hw - 0.13, 0, 0), M['rim'], 'x', 64, 0.03))
    parts.append(cyl('HubDome', 0.08, 0.04, (hw - 0.09, 0, 0), M['satin'], 'x', 48, 0.02))
    for k in range(10):                                        # the wheel nuts round the hub, on the dish ring
        a = 2 * math.pi * (k + 0.5) / 10
        parts.append(cyl('LugNut', 0.022, 0.03, (hw - 0.14, math.cos(a) * 0.22, math.sin(a) * 0.22), M['alu'], 'x', 6, 0.003))
    parts.append(cyl('Valve', 0.008, 0.05, (hw - 0.13, 0.0, 0.345), M['alu'], 'x', 8))
    for k in range(6):
        a = 2 * math.pi * k / 6
        parts.append(cyl('BoltCap', 0.014, 0.025, (hw - 0.095, math.cos(a) * 0.115, math.sin(a) * 0.115), CALIPER_RED, 'x', 12, 0.004))
    bpy.ops.object.select_all(action='DESELECT')
    for q in parts:
        bpy.context.view_layer.objects.active = q
        for m in list(q.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
    for q in parts: q.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]; bpy.ops.object.join()
    w = bpy.context.active_object
    bpy.ops.object.select_all(action='DESELECT'); w.select_set(True); bpy.context.view_layer.objects.active = w
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)   # the axle along x, upright
    return w


wheel = make_wheel(); wheel.data.name = 'Wheel'
for i, y in enumerate(AXLES):
    for s in (-1, 1):
        ob = bpy.data.objects.new('Wheel_%s%d' % ('L' if s < 0 else 'R', i + 1), wheel.data)
        coll.objects.link(ob); ob.location = (s * TRACK, y, RW)
        if s < 0: ob.rotation_euler = (0, 0, math.pi)
bpy.data.objects.remove(wheel)

# ---------------------------------------------------------------- the suspension (owner, 2026-11-11: the parts you would
# expect to see in the wheel arches, and they move as you drive). At every wheel: double wishbones (lower and upper arm,
# rubber bushes on the chassis, ball joints at the upright), the upright (knuckle) with its hub, a ventilated brake disc
# with a red caliper, a drive shaft with rubber boots, and a coil-over (red spring round a damper, chrome rod) from the
# lower arm up to a mount under the body; tie rods to the front uprights. Laid out so nothing passes through anything
# (spring beside the drive shaft, through the gap in the upper arm). Each part is its own object, ONE shared mesh per
# kind (instanced, the left side the right side turned half round), its origin where it pivots, for the driving code:
#   Knuckle_*, BrakeDisc_*, Caliper_*, BrakeHose_*, WheelSensor_*: origin the wheel centre; they follow the wheel's travel
#   and steering (the disc also spins). Steering: about the kingpin (KINGPIN in from the wheel's centre, vertical)
#   LowerArm_*, UpperArm_*: origin on the chassis pivot axis (along y); they turn about it with the travel
#   DriveShaft_*: origin at the differential; it turns (spin) and pitches to the wheel
#   Spring_*: origin the top mount, its local -z down the spring's axis: scale local z to compress it
#   DamperRod_*: origin the top mount (fixed to the body); DamperBody_*: origin the lower eye (moves with the arm),
#   its local +z up the axis; TopMount_* fixed. TieRod_*: origin at the chassis end.
SPRING = mat('TruckSpring', (0.62, 0.03, 0.02), 0.2, 0.32, coat=0.6)
CALIPER = mat('TruckCaliper', (0.7, 0.04, 0.02), 0.1, 0.35, coat=0.4)
DISC = mat('TruckBrakeDisc', (0.42, 0.42, 0.43), 1.0, 0.42)


def part_mesh(name, objs, frame):
    """Join temp objects (built in the corner's frame: the right-hand wheel's centre at the origin, inboard = -x) into
    one shared mesh, re-based so `frame` (a Matrix in the corner frame) is its origin and axes."""
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        bpy.context.view_layer.objects.active = o
        for m in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1: bpy.ops.object.join()
    j = bpy.context.active_object
    bpy.ops.object.select_all(action='DESELECT'); j.select_set(True); bpy.context.view_layer.objects.active = j
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    me = j.data; me.name = name; me.transform(frame.inverted())
    bpy.data.objects.remove(j)
    return me


def axis_frame(origin, down):
    """A frame at `origin` whose local -z points along `down`."""
    q = Vector((0, 0, -1)).rotation_difference(Vector(down).normalized())
    return Matrix.Translation(Vector(origin)) @ q.to_matrix().to_4x4()


V3 = lambda x, y, z: Vector((x, y, z))
I4 = Matrix.Identity(4)
# the geometry, relative to the right-hand wheel's centre
LA_F, LA_B, LA_O = V3(-0.66, -0.25, -0.20), V3(-0.66, 0.22, -0.20), V3(-0.41, 0.0, -0.19)
UA_F, UA_B, UA_O = V3(-0.66, -0.36, 0.27), V3(-0.66, 0.20, 0.27), V3(-0.41, 0.0, 0.25)
SB, ST = V3(-0.52, -0.11, -0.165), V3(-0.60, -0.11, 0.57)          # the coil-over's lower eye and top mount
DOWN = (SB - ST).normalized()
parts = {}
# the arms: two tubes meeting at the ball joint, bushes on the chassis
o = [strut('t', LA_F, LA_O, 0.022, M['satin']), strut('t', LA_B, LA_O, 0.022, M['satin']),
     cyl('t', 0.035, 0.07, tuple(LA_F), M['rubber'], 'y', 16), cyl('t', 0.035, 0.07, tuple(LA_B), M['rubber'], 'y', 16),
     cyl('t', 0.03, 0.05, tuple(LA_O), M['satin'], 'z', 16), box('t', -0.56, -0.15, -0.19, -0.48, -0.07, -0.165, M['satin'])]
parts['LowerArm'] = part_mesh('LowerArm', o, Matrix.Translation((LA_F + LA_B) / 2))
o = [strut('t', UA_F, UA_O, 0.02, M['satin']), strut('t', UA_B, UA_O, 0.02, M['satin']),
     cyl('t', 0.032, 0.06, tuple(UA_F), M['rubber'], 'y', 16), cyl('t', 0.032, 0.06, tuple(UA_B), M['rubber'], 'y', 16),
     cyl('t', 0.028, 0.045, tuple(UA_O), M['satin'], 'z', 16)]
parts['UpperArm'] = part_mesh('UpperArm', o, Matrix.Translation((UA_F + UA_B) / 2))
# the upright with its hub
o = [box('t', -0.41, -0.06, -0.21, -0.36, 0.06, 0.27, M['satin'], 0.01), cyl('t', 0.11, 0.05, (-0.335, 0, 0), M['satin'], 'x', 32, 0.005)]
parts['Knuckle'] = part_mesh('Knuckle', o, I4)
# the brake: a ventilated disc (two plates, vanes between) and its hat; the caliper over the top rear of the disc
o = [cyl('t', 0.29, 0.008, (-0.352, 0, 0), DISC, 'x', 64), cyl('t', 0.29, 0.008, (-0.33, 0, 0), DISC, 'x', 64),
     cyl('t', 0.15, 0.03, (-0.34, 0, 0), M['satin'], 'x', 48)]
for k in range(24):
    a = 2 * math.pi * k / 24
    v = box('t', -0.348, -0.004, 0.17, -0.334, 0.004, 0.28, M['satin']); v.rotation_euler = (a, 0, 0)
    o.append(v)
parts['BrakeDisc'] = part_mesh('BrakeDisc', o, I4)
c = box('t', -0.38, -0.06, -0.035, -0.30, 0.06, 0.035, CALIPER, 0.012, 3); c.location = (-0.34, 0.19, 0.19); c.rotation_euler = (math.radians(-45), 0, 0)
c2 = box('t', -0.383, -0.03, -0.012, -0.297, 0.03, 0.012, M['alu'], 0.004); c2.location = (-0.34, 0.215, 0.215); c2.rotation_euler = (math.radians(-45), 0, 0)
parts['Caliper'] = part_mesh('Caliper', [c, c2], I4)
# the drive shaft and its boots
o = [cyl('t', 0.035, 0.5, (-0.665, 0, 0), M['satin'], 'x', 20), cyl('t', 0.04, 0.07, (-0.86, 0, 0), M['rubber'], 'x', 20, 0.01),
     cyl('t', 0.04, 0.06, (-0.455, 0, 0), M['rubber'], 'x', 20, 0.01)]
parts['DriveShaft'] = part_mesh('DriveShaft', o, Matrix.Translation(V3(-0.92, 0, 0)))
# the coil-over: spring (a helix of wire), the damper's body from the lower eye, its chrome rod from the top mount
F_top = axis_frame(ST, DOWN); F_bot = axis_frame(SB, DOWN)
L = (SB - ST).length
turns, rr, wire = 7.5, 0.055, 0.0105
hel = []
for k in range(int(turns * 24) + 1):
    t = k / (turns * 24); a = 2 * math.pi * turns * t
    d = 0.06 + (L - 0.16) * t
    hel.append(tuple(F_top @ Vector((rr * math.cos(a), rr * math.sin(a), -d))))
parts['Spring'] = part_mesh('Spring', [tube('t', hel, wire, SPRING, 8)], F_top)
o = [cyl('t', 0.03, 0.36, (0, 0, 0), M['black'], 'z', 24, 0.004), cyl('t', 0.022, 0.05, (0, 0, 0), M['satin'], 'x', 16)]
o[0].matrix_world = F_bot @ Matrix.Translation((0, 0, 0.2)); o[1].matrix_world = F_bot @ Matrix.Rotation(math.pi / 2, 4, 'Y')
parts['DamperBody'] = part_mesh('DamperBody', o, F_bot)
o = [cyl('t', 0.012, 0.42, (0, 0, 0), M['chrome'], 'z', 16), cyl('t', 0.07, 0.012, (0, 0, 0), M['satin'], 'z', 32),
     cyl('t', 0.07, 0.012, (0, 0, 0), M['satin'], 'z', 32)]
o[0].matrix_world = F_top @ Matrix.Translation((0, 0, -0.23)); o[1].matrix_world = F_top @ Matrix.Translation((0, 0, -0.045))
o[2].matrix_world = F_bot @ Matrix.Translation((0, 0, 0.095))       # the spring's lower seat on the damper body
parts['DamperRod'] = part_mesh('DamperRod', o[:2], F_top)
parts['SpringSeat'] = part_mesh('SpringSeat', [o[2]], F_bot)
o = [box('t', -0.66, -0.17, 0.55, -0.54, -0.05, 0.86, M['satin'], 0.01)]
parts['TopMount'] = part_mesh('TopMount', o, Matrix.Translation(ST))
# the wheel wells' other parts (owner, 2026-11-11: more detail that makes sense, light enough for Nanite; one shared
# mesh per kind, instanced at every wheel): a ribbed black liner over the tyre, the brake hose down to the caliper, the
# wheel-speed sensor on the upright with its cable, a rubber bump stop over the lower arm, the drive shaft's inner joint
LINER = mat('TruckLiner', (0.012, 0.012, 0.013), 0.0, 0.75)
bm = bmesh.new(); rows = []
A0, A1, RL, X0, X1 = math.radians(55), math.radians(125), 0.875, -0.56, 0.24
for k in range(29):
    a = A0 + (A1 - A0) * k / 28
    rib = 0.012 if k % 2 else 0.0                                  # moulded ribs across the liner
    rows.append([bm.verts.new((x, (RL - rib) * math.cos(a), (RL - rib) * math.sin(a))) for x in (X0, X1)] +
                [bm.verts.new((x, (RL + 0.012) * math.cos(a), (RL + 0.012) * math.sin(a))) for x in (X1, X0)])
for r0, r1 in zip(rows, rows[1:]):
    for i in range(4): bm.faces.new((r0[i], r0[(i + 1) % 4], r1[(i + 1) % 4], r1[i]))
bm.faces.new(rows[0]); bm.faces.new(list(reversed(rows[-1])))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
parts['ArchLiner'] = part_mesh('ArchLiner', [new_obj('t', bm, LINER, angle=30)], I4)
hose = [V3(-0.39, 0.08, 0.32), V3(-0.385, 0.11, 0.315), V3(-0.375, 0.16, 0.29), V3(-0.36, 0.215, 0.245)]   # caliper to a bracket on the upright: it turns with the wheel
pts = []
for k in range(len(hose) - 1):
    for t in range(6): pts.append(tuple(hose[k].lerp(hose[k + 1], t / 6)))
pts.append(tuple(hose[-1]))
o = [tube('t', pts, 0.008, M['rubber'], 8), box('t', -0.41, 0.06, 0.30, -0.37, 0.1, 0.335, M['satin'], 0.003)]
parts['BrakeHose'] = part_mesh('BrakeHose', o, I4)
o = [cyl('t', 0.013, 0.05, (-0.385, 0.05, -0.12), M['satin'], 'x', 12), box('t', -0.40, 0.03, -0.14, -0.37, 0.07, -0.10, M['satin'], 0.003)]
parts['WheelSensor'] = part_mesh('WheelSensor', o, I4)
cab = [V3(-0.41, 0.05, -0.12), V3(-0.47, 0.07, -0.05), V3(-0.55, 0.08, 0.02), V3(-0.66, 0.08, 0.05)]
pts = []
for k in range(len(cab) - 1):
    for t in range(5): pts.append(tuple(cab[k].lerp(cab[k + 1], t / 5)))
pts.append(tuple(cab[-1]))
parts['SensorCable'] = part_mesh('SensorCable', [tube('t', pts, 0.005, M['rubber'], 6)], I4)
o = [cyl('t', 0.032, 0.07, (-0.6, 0.12, -0.08), M['rubber'], 'z', 16, 0.008), box('t', -0.67, 0.08, -0.05, -0.56, 0.16, -0.03, M['satin'], 0.004)]
parts['BumpStop'] = part_mesh('BumpStop', o, I4)
parts['InnerJoint'] = part_mesh('InnerJoint', [cyl('t', 0.06, 0.08, (-0.905, 0, 0), M['satin'], 'x', 24, 0.01)], Matrix.Translation(V3(-0.92, 0, 0)))
FRAMES = {'LowerArm': Matrix.Translation((LA_F + LA_B) / 2), 'UpperArm': Matrix.Translation((UA_F + UA_B) / 2),
          'Knuckle': I4, 'BrakeDisc': I4, 'Caliper': I4, 'DriveShaft': Matrix.Translation(V3(-0.92, 0, 0)),
          'Spring': F_top, 'DamperBody': F_bot, 'SpringSeat': F_bot, 'DamperRod': F_top, 'TopMount': Matrix.Translation(ST),
          'ArchLiner': I4, 'BrakeHose': I4, 'WheelSensor': I4, 'SensorCable': I4, 'BumpStop': I4,
          'InnerJoint': Matrix.Translation(V3(-0.92, 0, 0))}
for i, y in enumerate(AXLES):
    for sd in (-1, 1):
        tag = ('L' if sd < 0 else 'R') + str(i + 1)
        C = Matrix.Translation((sd * TRACK, y, RW)) @ (Matrix.Rotation(math.pi, 4, 'Z') if sd < 0 else I4)
        for kind, me in parts.items():
            ob = bpy.data.objects.new(kind + '_' + tag, me); coll.objects.link(ob)
            ob.matrix_world = C @ FRAMES[kind]
for sd in (-1, 1):
    box('MudflapFront', sd * TRACK - 0.32, 1.83, 0.25, sd * TRACK + 0.32, 1.86, 0.95, M['rubber'], 0.01)   # just behind the front wheel's sweep
    for yc in (1.05, -0.35):
        cyl('AirTank', 0.12, 0.95, (sd * 0.78, yc, 0.72), M['satin'], 'y', 32, 0.04)
        for dy in (-0.32, 0.32):
            bpy.ops.mesh.primitive_torus_add(major_radius=0.126, minor_radius=0.008, location=(sd * 0.78, yc + dy, 0.72), rotation=(math.pi / 2, 0, 0))
            _st = bpy.context.active_object; _st.name = 'AirTankStrap'; finish(_st, M['alu'])
        box('AirTankBracket', sd * 0.55, yc - 0.36, 0.66, sd * 0.68, yc + 0.36, 0.7, M['satin'], 0.005)
# the steering: a steering arm on each upright of the front axle and of the rearmost axle (rear-axle steering), a tie
# rod in from the chassis
for sd, ai in ((-1, 0), (1, 0), (-1, 2), (1, 2)):
    tag = ('L' if sd < 0 else 'R') + str(ai + 1)
    ka = Vector((sd * (TRACK - 0.39), AXLES[ai] - 0.06, RW + 0.12)); kb = Vector((sd * (TRACK - 0.39), AXLES[ai] - 0.32, RW + 0.12))
    strut('SteerArm_' + tag, ka, kb, 0.018, M['satin'])
    tr = strut('TieRod_' + tag, Vector((sd * 0.56, AXLES[ai] - 0.32, RW + 0.12)), kb + Vector((-sd * 0.02, 0, 0)), 0.016, M['satin'])
    cyl('TieRodEnd_' + tag, 0.026, 0.05, tuple(kb), M['satin'], 'z', 16)

# ---------------------------------------------------------------- the body: higher than the cab, the rear module a step up
yr0, yr1 = AXLES[1] + 0.95, AXLES[2] - 0.95      # the rear wheels' opening
FRONT_WELL_END = 1.78                            # the body's lower part starts behind the front wheel's sweep


def prism(name, pts, y0, y1, material, bev=0.0, seg=3):
    """A cross-section (x, z points) run along y from y0 to y1."""
    bm = bmesh.new()
    a = [bm.verts.new((x, y0, z)) for x, z in pts]; b = [bm.verts.new((x, y1, z)) for x, z in pts]
    bm.faces.new(a); bm.faces.new(list(reversed(b)))
    n = len(a)
    for i in range(n): bm.faces.new((a[i], b[i], b[(i + 1) % n], a[(i + 1) % n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, material, bev, seg, 50)


def shoulders(z0, ztop, r=0.32, k=8):
    """A body cross-section: straight sides, the top corners rounded with radius r."""
    pts = [(-W, z0), (W, z0)]
    for i in range(k + 1):
        a = math.radians(90 * i / k); pts.append((W - r + r * math.cos(a), ztop - r + r * math.sin(a)))
    for i in range(k + 1):
        a = math.radians(90 + 90 * i / k); pts.append((-W + r + r * math.cos(a), ztop - r + r * math.sin(a)))
    return pts


prism('Body', shoulders(1.6, BODY_TOP), 2.2, -4.5, M['paint'], 0.02, 2)


def rear_module():
    """The raised rear section: the shoulders' cross-section, each point run from its own front to its own rear."""
    sec = [(-W, 1.0), (W, 1.0), (W, 1.3), (W, 1.62), (W, 1.66), (W, 2.0), (W, 2.6)]
    r, k = 0.36, 8
    for i in range(k + 1):
        a = math.radians(90 * i / k); sec.append((W - r + r * math.cos(a), REAR_TOP - r + r * math.sin(a)))
    for i in range(k + 1):
        a = math.radians(90 + 90 * i / k); sec.append((-W + r + r * math.cos(a), REAR_TOP - r + r * math.sin(a)))
    sec += [(-W, 2.6), (-W, 2.0), (-W, 1.66), (-W, 1.62), (-W, 1.3)]

    def yf(z):     # its front: behind the rear wheels low down, at the body's end above; the top sloping forward
        if z < 1.64: return yr1
        if z > BODY_TOP - 0.05: return -4.45 - (z - BODY_TOP + 0.05) * 1.3
        return -4.45

    def yb(z):     # its rear: the upper part out to the end, the lower part cut back under it, the face leaning
        if z >= 2.0: return REAR - 0.06 * (z - 2.0) / (REAR_TOP - 2.0)
        return REAR + 0.42 * (2.0 - z) / 1.0

    bm = bmesh.new()
    a = [bm.verts.new((x, yf(z), z)) for x, z in sec]; b = [bm.verts.new((x, yb(z), z)) for x, z in sec]
    bm.faces.new(a); f = bm.faces.new(list(reversed(b)))
    n = len(sec)
    for i in range(n): bm.faces.new((a[i], b[i], b[(i + 1) % n], a[(i + 1) % n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj('BodyRear', bm, M['paint'], 0.03, 3, 50)


rear_module()
box('BodyLow', -W, FRONT_WELL_END, 0.98, W, yr0, 1.64, M['paint'], 0.03)
box('BodyFront', -0.55, 2.2, 1.3, 0.55, 1.94, 1.64, M['paint'], 0.02)
# the shoulder: a bright trim line just under the roof edge, both sides, stepping up at the rear module
for s in (-1, 1):
    x = s * (W + 0.006)
    box('Trim', x - 0.008, -4.5, 3.0, x + 0.008, 2.15, 3.03, M['alu'])
    box('TrimRear', x - 0.008, REAR + 0.15, REAR_TOP - 0.4, x + 0.008, -4.55, REAR_TOP - 0.37, M['alu'])
# the wheel arches: angular yellow flares standing out from the sides, a black lip along their inner edge


def flare(name, path, side_s, centre, out=0.12, width=0.16):
    """A flare swept along a path of (y, z) points: its section stands out from the side by `out`, `width` wide."""
    bm = bmesh.new(); secs = []
    for k, (y, z) in enumerate(path):
        ya_, za_ = path[max(0, k - 1)]; yb_, zb_ = path[min(len(path) - 1, k + 1)]
        ty, tz = yb_ - ya_, zb_ - za_; L = math.hypot(ty, tz) or 1.0
        ny, nz = -tz / L, ty / L
        if (y - centre[0]) * ny + (z - centre[1]) * nz < 0: ny, nz = -ny, -nz
        xi, xo = side_s * (W - 0.02), side_s * (W + out)
        secs.append([bm.verts.new(v) for v in ((xi, y, z), (xo, y + ny * 0.03, z + nz * 0.03), (xo, y + ny * width, z + nz * width), (xi, y + ny * (width + 0.05), z + nz * (width + 0.05)))])
    for a, b in zip(secs, secs[1:]):
        for i in range(4): bm.faces.new((a[i], a[(i + 1) % 4], b[(i + 1) % 4], b[i]))
    bm.faces.new(secs[0]); bm.faces.new(list(reversed(secs[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = new_obj(name, bm, M['paint'], 0.015, 2, 35)
    # the black lip under its inner edge
    bm = bmesh.new(); lip = []
    for (y, z) in path:
        lip.append([bm.verts.new((side_s * (W - 0.06), y, z - 0.01)), bm.verts.new((side_s * (W + out * 0.8), y, z - 0.01)), bm.verts.new((side_s * (W + out * 0.8), y, z + 0.02)), bm.verts.new((side_s * (W - 0.06), y, z + 0.02))])
    for a, b in zip(lip, lip[1:]):
        for i in range(4): bm.faces.new((a[i], a[(i + 1) % 4], b[(i + 1) % 4], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    new_obj(name + 'Lip', bm, M['black'], 0.0)
    return ob


def arch_path(y_front, y_back, z_low, z_top, chamfer):
    """An angular arch over a wheel opening: up the front, a chamfer, across the top, a chamfer, down the back."""
    return [(y_front, z_low), (y_front, z_top - chamfer), (y_front - chamfer * 0.8, z_top), (y_back + chamfer * 0.8, z_top), (y_back, z_top - chamfer), (y_back, z_low)]


for s in (-1, 1):
    flare('RearFlare', arch_path(yr0 + 0.02, yr1 - 0.02, 0.98, 1.62, 0.32), s, ((yr0 + yr1) / 2, 0.75))
# lockers: the tall one behind the cab, the low one between the axles (roller shutters), the panel doors at the rear
def shutter(nm, s, y0, y1, z0, z1):
    x = s * (W + 0.004)
    box(nm + 'Frame', x - 0.012, y1, z0, x + 0.012, y0, z1, M['satin'], 0.006)
    box(nm, x + s * 0.008 - 0.006, y1 + 0.05, z0 + 0.05, x + s * 0.008 + 0.006, y0 - 0.05, z1 - 0.05, M['paint'], 0.004)
    z = z0 + 0.09
    while z < z1 - 0.07:
        box(nm + 'Rib', x + s * 0.016 - 0.004, y1 + 0.06, z, x + s * 0.016 + 0.004, y0 - 0.06, z + 0.012, M['paint'], 0.003, 2); z += 0.075
    box(nm + 'Bar', x + s * 0.024 - 0.012, y1 + 0.06, z0 + 0.06, x + s * 0.024 + 0.012, y0 - 0.06, z0 + 0.11, M['alu'], 0.006)


for s in (-1, 1):
    shutter('LockerTall', s, 2.08, 1.02, 1.12, 3.12)
    shutter('LockerLow', s, 0.92, yr0 + 0.08, 1.04, 1.98)
    shutter('LockerRear', s, yr1 - 0.1, REAR + 0.5, 1.06, 1.95)
    # the panel seams on the upper body
    for y in (0.95, -4.5):
        box('Seam', s * (W + 0.004) - 0.004, y - 0.01, 1.05 if y > 0 else 1.66, s * (W + 0.004) + 0.004, y + 0.01, 3.3 if y > 0 else REAR_TOP - 0.05, M['satin'])   # (not across the rear arch)
    # the light columns: blue at the cab's rear corner, red/amber at the rear module's seam
    box('LightColumnCab', s * (W + 0.0) - 0.05, 2.12, 2.1, s * (W + 0.0) + 0.05, 2.3, 3.05, M['black'], 0.02)
    for k in range(4):
        box('Light_BlueSide', s * (W + 0.05) - 0.006, 2.15, 2.25 + k * 0.2, s * (W + 0.05) + 0.006, 2.27, 2.38 + k * 0.2, M['lensBlue'], 0.004)
    box('LightColumnRear', s * (W + 0.0) - 0.05, -4.62, 2.55, s * (W + 0.0) + 0.05, -4.44, 3.3, M['black'], 0.02)
    for k in range(4):
        box('Light_TurnSide' + ('L' if s < 0 else 'R') if k == 3 else 'Light_MarkerSide', s * (W + 0.05) - 0.006, -4.6, 2.62 + k * 0.17, s * (W + 0.05) + 0.006, -4.46, 2.73 + k * 0.17,
            M['lensAmberL' if s < 0 else 'lensAmberR'] if k == 3 else M['lensRed'], 0.004)
    box('Mudflap', s * TRACK - 0.34, AXLES[2] - 0.98, 0.25, s * TRACK + 0.34, AXLES[2] - 0.95, 1.0, M['rubber'], 0.01)
    box('SideStep', s * (W - 0.15), 0.8, 0.82, s * (W + 0.03), -1.5, 0.86, M['alu'], 0.01)

# the roof: hoop rails on both sides, the hose deck between
for s in (-1, 1):
    for (y0, y1) in [(1.9, 0.0), (-0.3, -2.6), (-2.9, -4.3)]:
        z0 = BODY_TOP
        cyl('HoopPost', 0.025, 0.4, (s * (W - 0.1), y0, z0 + 0.2), M['alu'], 'z', 12)
        cyl('HoopPost', 0.025, 0.4, (s * (W - 0.1), y1, z0 + 0.2), M['alu'], 'z', 12)
        strut('Hoop', (s * (W - 0.1), y0, z0 + 0.4), (s * (W - 0.1), y1, z0 + 0.4), 0.025, M['alu'])
    cyl('HoopPost', 0.025, 0.4, (s * (W - 0.1), -4.8, REAR_TOP + 0.2), M['alu'], 'z', 12)
    cyl('HoopPost', 0.025, 0.4, (s * (W - 0.1), -5.8, REAR_TOP + 0.2), M['alu'], 'z', 12)
    strut('Hoop', (s * (W - 0.1), -4.8, REAR_TOP + 0.4), (s * (W - 0.1), -5.8, REAR_TOP + 0.4), 0.025, M['alu'])
box('RoofDeck', -1.1, -4.3, BODY_TOP - 0.01, 1.1, 1.9, BODY_TOP + 0.04, M['satin'], 0.01)
box('Beacon', -0.08, 1.95, BODY_TOP, 0.08, 2.1, BODY_TOP + 0.2, M['lensAmberL'], 0.03)

# the rear face: black lower band with the lamps, the centre grille, the chevron boards at the corners
box('RearBand', -W + 0.06, REAR - 0.03, 2.0, W - 0.06, REAR + 0.02, 2.25, M['black'], 0.02)
box('RearUnder', -W + 0.06, REAR + 0.4, 1.02, W - 0.06, REAR + 0.43, 1.97, M['black'], 0.0).rotation_euler = (math.radians(-22.8), 0, 0)
box('RearGrille', -0.8, REAR - 0.05, 2.35, 0.8, REAR + 0.02, REAR_TOP - 0.25, M['black'], 0.02)
for i in range(12):
    box('RearGrilleSlat', -0.76, REAR - 0.07, 2.42 + i * 0.07, 0.76, REAR - 0.05, 2.44 + i * 0.07, M['satin'], 0.005) if i < 11 else None
for s in (-1, 1):
    # the red/white warning board at each rear corner (its stripes a texture, sloping outwards on each side)
    box('RearChevronBoard' + ('L' if s < 0 else 'R'), s * 1.2 - 0.17, REAR - 0.08, 2.32, s * 1.2 + 0.17, REAR - 0.05, 3.32, M['lensRed'], 0.006)
    box('Light_Brake', s * 1.25 - 0.1, REAR - 0.05, 2.06, s * 1.25 + 0.1, REAR - 0.03, 2.2, M['lensRed'], 0.008)
    box('Light_Turn' + ('L' if s < 0 else 'R'), s * 1.0 - 0.08, REAR - 0.05, 2.06, s * 1.0 + 0.08, REAR - 0.03, 2.2, M['lensAmberL' if s < 0 else 'lensAmberR'], 0.008)
    box('Light_Reverse', s * 0.78 - 0.06, REAR - 0.05, 2.06, s * 0.78 + 0.06, REAR - 0.03, 2.2, M['lensWhite'], 0.008)
    housed_lamp('Light_BlueRear', M['lensBlue'], (s * 1.3, REAR - 0.055, REAR_TOP - 0.13), (0, -1, 0), 0.12, 0.12)
text('NumberRear', NO, 0.4, (0.0, REAR - 0.08, 2.75), (math.pi / 2, 0, 0), M['paint'])

# ---------------------------------------------------------------- the cab
# the front fender over the front wheel (yellow, behind the door, under the rear window)
ya, za, ra = AXLES[0], RW, RW + 0.07


def arch_band(name, y0, z0, r0, r1, x0, x1, material):
    """A wheel arch: a half ring of radius r0..r1 round (y0, z0), run across x from x0 to x1 (quads only)."""
    bm = bmesh.new(); ring = []
    for k in range(25):
        a = math.radians(180 * k / 24)
        c, sn = math.cos(a), math.sin(a)
        ring.append([bm.verts.new((x, y0 + r * c, z0 + r * sn)) for x, r in ((x0, r0), (x1, r0), (x1, r1), (x0, r1))])
    for k in range(24):
        a, b = ring[k], ring[k + 1]
        for i in range(4): bm.faces.new((a[i], a[(i + 1) % 4], b[(i + 1) % 4], b[i]))
    bm.faces.new(ring[0]); bm.faces.new(list(reversed(ring[-1])))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_obj(name, bm, material)


for s in (-1, 1):
    flare('FrontFlare', arch_path(ya + ra - 0.03, ya - ra - 0.05, 1.05, za + ra + 0.05, 0.3), s, (ya, za))
# the fender's body round the arch: over it up to the rear window, and in front of it down to the door's foot
box('FenderTop', -W, CAB_R, za + ra + 0.05, W, DOOR0, ZBR + 0.01, M['paint'], 0.03)
for s in (-1, 1):   # the fender's two black stripes, rising towards the front
    x0, x1 = s * (W + 0.002), s * (W + 0.006)
    side('FenderStripe', [(CAB_R, 1.74), (DOOR0, 1.94), (DOOR0, 2.01), (CAB_R, 1.81)], x0, x1, M['black'])
    side('FenderStripe2', [(CAB_R, 1.66), (DOOR0, 1.85), (DOOR0, 1.89), (CAB_R, 1.7)], x0, x1, M['black'])
# the cab's front: curves in plan (a superellipse, round at the corners, full across the middle), lofted ring to ring
def front_curve(yside, yfront, hw, n=2.6, k=16):
    pts = []
    for i in range(k + 1):
        t = math.pi * (1 - i / k)
        c, sn = math.cos(t), math.sin(t)
        pts.append((hw * math.copysign(abs(c) ** (2 / n), c), yside + (yfront - yside) * abs(sn) ** (2 / n)))
    return pts


K = 16
TOP_SIDE = DOOR1 - 0.3                       # the glass's side edge at the top: the door's front edge leans back


def ring(z, yside, yfront, hw):
    return [(-hw, DOOR0, z)] + [(x, y, z) for x, y in front_curve(yside, yfront, hw, k=K)] + [(hw, DOOR0, z)]


rings = [ring(0.6, 5.15, 5.95, W - 0.08), ring(0.82, 5.2, 6.12, W - 0.02), ring(1.0, 5.15, 6.08, W),
         ring(1.32, 5.1, 6.0, W), ring(1.37, 5.06, 6.0, W + 0.006), ring(ZB, DOOR1, YF, W)]
bm = bmesh.new(); vr = [[bm.verts.new(p) for p in r] for r in rings]
nR = len(rings[0])
for a in range(len(rings) - 1):
    for i in range(nR - 1):                          # a shell: open at the back (the cab's floor is behind it)
        if i == 0 or i == nR - 2: continue           # open at the doorways (owner, 2026-11-11: walk in through them)
        j = i + 1
        f = bm.faces.new((vr[a][i], vr[a][j], vr[a + 1][j], vr[a + 1][i]))
        corner = i < 6 or i > nR - 8                 # the corner wings: yellow round the corners, black in the middle
        f.material_index = 0 if a >= 3 or (a in (1, 2) and corner) else 1
# (no top of its own: the dashboard's smooth top covers it, nothing of the old flat dash left)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
new_obj('CabFront', bm, [M['paint'], M['black']], angle=50)
# the slim LED lamps along the wings' upper edge, following the curve round each corner
for s in (-1, 1):
    pts = front_curve(5.1, 6.0, W, k=K)
    for i in range(1, 6):
        x0, y0 = pts[i if s < 0 else K - i]; x1, y1 = pts[i + 1 if s < 0 else K - i - 1]
        nmL = 'Light_HeadLow' if i <= 2 else ('Light_HeadHigh' if i <= 4 else 'Light_Turn' + ('L' if s < 0 else 'R'))
        lens = M['lensClear'] if i <= 2 else (M['lensHigh'] if i <= 4 else M['lensAmberL' if s < 0 else 'lensAmberR'])
        strut(nmL, (x0 * 1.004, y0 + 0.012, 1.25), (x1 * 1.004, y1 + 0.012, 1.25), 0.035, lens)
# the glass: from the cab's rear round the front; open at the doors; its back wall solid
bot = [(-W, CAB_R + 0.05, ZBR), (-W, DOOR0, ZBR)] + [(x, y, ZB) for x, y in front_curve(DOOR1, YF, W, k=K)] + [(W, DOOR0, ZBR), (W, CAB_R + 0.05, ZBR)]
top = [(-W, CAB_R + 0.05, ZT), (-W, DOOR0, ZT)] + [(x, y, ZT) for x, y in front_curve(TOP_SIDE, YF - S, W, 2.2, k=K)] + [(W, DOOR0, ZT), (W, CAB_R + 0.05, ZT)]
nP = len(bot)
# rings between bottom and top: the front's middle pushed forward (most at the centre, none at the door edges)
rows = [bot]
for t in (0.25, 0.5, 0.75):
    row = []
    for i in range(nP):
        (x0, y0, z0), (x1, y1, z1) = bot[i], top[i]
        bulge = 0.0
        if 2 <= i <= nP - 3:
            u = (i - 2) / (nP - 5)                       # 0..1 across the front curve
            bulge = 0.16 * math.sin(math.pi * u) ** 0.7 * math.sin(math.pi * t)
        row.append((x0 + (x1 - x0) * t, y0 + (y1 - y0) * t + bulge, z0 + (z1 - z0) * t))
    rows.append(row)
rows.append(top)
bm = bmesh.new()
vv = [[bm.verts.new(p) for p in r] for r in rows]
for a in range(len(rows) - 1):
    for i in range(nP):
        j = (i + 1) % nP
        if (i, j) in ((1, 2), (nP - 3, nP - 2)): continue
        f = bm.faces.new((vv[a][i], vv[a][j], vv[a + 1][j], vv[a + 1][i])); f.material_index = 1 if j == 0 else 0
new_obj('Glass_Cab', bm, [M['glass'], M['black']], angle=60)
# a slim black frame: the pillars at the door edges and the cab's rear, the line under the glass, the header
for i in (0, 1, 2, nP - 3, nP - 2, nP - 1):
    strut('Pillar', bot[i], top[i], 0.04, M['black'])
tube('GlassSill', [(x, y, z + 0.01) for x, y, z in bot[2:nP - 2]], 0.025, M['black'], 10)
tube('GlassHeader', top[2:nP - 2], 0.05, M['black'], 10)
strut('GlassSillRear', bot[0], bot[1], 0.03, M['black'])
strut('GlassSillRear', bot[-1], bot[-2], 0.03, M['black'])
# the roof: thick, its edges well rounded, sweeping down into the pillars; a black rim under it
roof_ol = [(x * 1.012, y + (0.04 if y > DOOR0 + 0.1 else 0)) for x, y, z in top]
slab('CabRoof', roof_ol, ZT + 0.0, ZT + 0.22, M['paint'], 0.1, 6)
slab('CabRoofRim', [(x * 1.006, y + (0.02 if y > DOOR0 + 0.1 else 0)) for x, y, z in top], ZT - 0.05, ZT + 0.02, M['black'], 0.02, 2)
roof_front = max(y for x, y in roof_ol)
_lc = front_curve(DOOR1, YF, W, k=64)
bm = bmesh.new(); _lo, _hi = [], []
for x, y in _lc:
    _lo.append(bm.verts.new((x * 0.965, y - 0.06, 1.03))); _hi.append(bm.verts.new((x * 0.965, y - 0.06, ZB - 0.005)))
for i in range(len(_lc) - 1): bm.faces.new((_lo[i], _lo[i + 1], _hi[i + 1], _hi[i]))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
new_obj('CabLining', bm, M['dash'], angle=60)
# the front: the strobes, fog lamps and tow eyes in the black lower front, the grille slats, the bumper step
for s in (-1, 1):
    _p, _n = front_surface(s * 0.95, 1.16); housed_lamp('Light_BlueFront', M['lensBlue'], _p, _n, 0.15, 0.07)
    _p, _n = front_surface(s * 0.7, 0.71); housed_lamp('FogLamp', M['lensClear'], _p, _n, 0.13, 0.08)
    box('TowEye', s * 0.55 - 0.05, FRONT - 0.05, 0.55, s * 0.55 + 0.05, FRONT + 0.15, 0.62, M['alu'], 0.01)
for i in range(5):
    box('GrilleSlat', -0.6, FRONT + 0.0, 0.9 + i * 0.07, 0.6, FRONT + 0.1, 0.93 + i * 0.07, M['satin'], 0.005)
box('BumperStep', -0.9, FRONT - 0.15, 0.45, 0.9, FRONT + 0.12, 0.52, M['alu'], 0.01)
# the bumper turret on the yellow band, front left; the wipers
cyl('BumperTurretBase', 0.12, 0.12, (-0.62, YF - 0.05, ZB + 0.04), M['nozzle'], 'z', 24, 0.01)
box('BumperTurretBody', -0.74, YF - 0.12, ZB + 0.08, -0.5, YF + 0.12, ZB + 0.22, M['paint'], 0.04)
cyl('BumperNozzle', 0.05, 0.3, (-0.62, YF + 0.25, ZB + 0.15), M['nozzle'], 'y', 20, 0.01)
# the wipers (owner, 2026-11-11: switchable; they clear the rain where they sweep): each its own object, its origin at
# its spindle at the foot of the windscreen, its local Y the windscreen's normal there (the game sweeps it about local Y,
# about 100 deg up from rest); arm and blade lying on the glass at rest
_n = Vector((0, ZT - ZB, S)).normalized()                       # the windscreen's outward normal at its foot
for _nm, _px in (('Wiper_L', -0.92), ('Wiper_R', 0.02)):
    _p = Vector((_px, YF - 0.035, ZB + 0.07))
    _fx = Vector((1, 0, 0)); _fy = _n; _fz = _fx.cross(_fy)
    _F = Matrix((( _fx.x, _fy.x, _fz.x, _p.x), (_fx.y, _fy.y, _fz.y, _p.y), (_fx.z, _fy.z, _fz.z, _p.z), (0, 0, 0, 1)))
    _o = [box('t', 0.0, 0.0, -0.012, 0.9, 0.02, 0.012, M['black'], 0.004),        # the arm
          box('t', 0.06, 0.02, -0.006, 0.88, 0.035, 0.006, M['rubber'], 0.002),    # the blade
          cyl('t', 0.025, 0.04, (0, 0.0, 0), M['satin'], 'y', 16, 0.004)]          # the spindle cap
    for _q in _o: _q.matrix_world = _F @ _q.matrix_world
    _wm = part_mesh(_nm, _o, _F)
    _wo = bpy.data.objects.new(_nm, _wm); coll.objects.link(_wo); _wo.matrix_world = _F
# the roof turret (flat yellow, front of the roof), the light bar along the roof's rear edge
box('RoofTurretBase', -0.45, roof_front - 1.2, ZT + 0.16, 0.45, roof_front - 0.4, ZT + 0.24, M['nozzle'], 0.03)
box('RoofTurretBody', -0.38, roof_front - 1.1, ZT + 0.24, 0.38, roof_front - 0.25, ZT + 0.42, M['paint'], 0.08, 4)
cyl('RoofNozzle', 0.07, 0.5, (0, roof_front - 0.05, ZT + 0.34), M['nozzle'], 'y', 24, 0.01)
box('LightBar', -1.2, CAB_R + 0.2, ZT + 0.16, 1.2, CAB_R + 0.5, ZT + 0.26, M['black'], 0.03)
for i in range(8):
    x = -1.12 + i * 0.285
    box('Light_BlueBar', x, CAB_R + 0.18, ZT + 0.17, x + 0.24, CAB_R + 0.52, ZT + 0.25, M['lensRed' if i in (0, 7) else 'lensBlue'], 0.01)
# the mirrors, bus-style on arms forward from the roof's front corners
for s in (-1, 1):
    strut('MirrorArm', (s * (W - 0.1), TOP_SIDE - 0.1, ZT - 0.05), (s * (W + 0.22), TOP_SIDE + 0.6, ZT - 0.12), 0.02, M['black'])
    box('Mirror', s * (W + 0.2) - 0.07, TOP_SIDE + 0.55, ZT - 0.6, s * (W + 0.22) + 0.07, TOP_SIDE + 0.68, ZT - 0.1, M['black'], 0.03)
# the doors: black, nearly all glass, hinged at the front edge; the steps under them
for s, nm in ((-1, 'L'), (1, 'R')):
    # the door: a black frame round a real window (owner, 2026-11-11: the cab doors need their side windows), the lower
    # panel solid; all its pieces are Door<side> (.001...) so the game swings them together
    _xa, _xb = s * (W - 0.01), s * (W + 0.04)
    _g0, _g1, _gb = DOOR0 + 0.1, DOOR1 - 0.1, 1.15        # the window's rear edge, front edge (at its foot), sill
    door_ob = side('Door' + nm, [(DOOR0 + 0.02, 0.72), (DOOR1 - 0.02, 0.72), (DOOR1 - 0.02, _gb), (DOOR0 + 0.02, _gb)], _xa, _xb, M['black'], 0.02, 3)
    side('Door' + nm, [(DOOR0 + 0.02, _gb - 0.01), (_g0, _gb - 0.01), (_g0, ZT - 0.1), (DOOR0 + 0.02, ZT - 0.02)], _xa, _xb, M['black'], 0.01, 2)
    side('Door' + nm, [(DOOR0 + 0.02, ZT - 0.1), (TOP_SIDE - 0.1, ZT - 0.1), (TOP_SIDE - 0.02, ZT - 0.02), (DOOR0 + 0.02, ZT - 0.02)], _xa, _xb, M['black'], 0.01, 2)
    side('Door' + nm, [(_g1, _gb - 0.01), (DOOR1 - 0.02, _gb - 0.01), (DOOR1 - 0.02, ZB), (TOP_SIDE - 0.02, ZT - 0.02), (TOP_SIDE - 0.1, ZT - 0.1), (_g1, ZB)], _xa, _xb, M['black'], 0.01, 2)
    _dt = text('DoorModel' + nm, 'STORM 6x6', 0.075, (s * (W + 0.042), (DOOR0 + DOOR1) / 2, 1.06), (math.pi / 2, 0, math.pi / 2 if s > 0 else -math.pi / 2), mat('TruckTeal', (0.0, 0.42, 0.48), 0.2, 0.3))
    _dt.parent = door_ob                                   # it opens with the door
    # the pane set in the frame (seen through from both sides); the handle on the lower panel below the window
    side('Glass_Door' + nm, [(_g0 - 0.01, _gb - 0.01), (_g1 + 0.01, _gb - 0.01), (_g1 + 0.01, ZB), (TOP_SIDE - 0.09, ZT - 0.09), (_g0 - 0.01, ZT - 0.09)], s * (W + 0.008), s * (W + 0.016), M['glass'])
    box('DoorHandle' + nm, s * (W + 0.05) - 0.012, DOOR0 + 0.15, 1.04, s * (W + 0.05) + 0.012, DOOR0 + 0.4, 1.09, M['alu'], 0.006)
    box('DoorStripe' + nm, s * (W + 0.042) - 0.002, DOOR0 + 0.05, 0.98, s * (W + 0.042) + 0.002, DOOR1 - 0.05, 1.01, M['lensBlue'], 0.0)
    # the way up into the cab, as on the real trucks (owner's photo, 2026-11-11): one step outside, a flat tread hung
    # just outside the door's face and standing out a little, held by a single angled black arm bolted under the cab's
    # front corner, yellow marks along its edge; then the second step inside the doorway (behind the shut door), then
    # the cab floor. The game reads the treads' places (StepTread1/2<side>) for your feet as you climb in and out.
    _sx = lambda v: s * v                              # (distances measured from the body's side, W)
    _ty0, _ty1, _tz = STAIR_Y0 + 0.015, STAIR_Y1 - 0.015, 0.40   # (its arms both on the inner step's black face)
    xa, xb = _sx(W - 0.03), _sx(W + 0.24)
    box('StairTread1' + nm, min(xa, xb), _ty0, _tz - 0.035, max(xa, xb), _ty1, _tz, M['black'], 0.008)
    for k in range(3):                                 # the yellow marks along its outer edge
        yk = _ty0 + 0.08 + k * (_ty1 - _ty0 - 0.16) / 2
        box('StairMark1' + nm, min(_sx(W + 0.2), _sx(W + 0.245)), yk - 0.035, _tz - 0.03, max(_sx(W + 0.2), _sx(W + 0.245)), yk + 0.035, _tz + 0.003, M['paint'], 0.004)
    # held by two short black arms, one at each end, from the black face of the cab's side out under the tread
    # (owner, 2026-11-11: no long arm; the bracket under the front corner stays, standing out)
    box('StairBracket1' + nm, min(_sx(W - 0.04), _sx(W + 0.09)), 5.22, 0.7, max(_sx(W - 0.04), _sx(W + 0.09)), 5.38, 0.78, M['black'], 0.01)
    for yb in (_ty0 + 0.025, _ty1 - 0.025):           # right at the plate's two ends
        # its foot on a plate bolted flat to the cab's black lower face (sunk into it: no gap to see through), two bolts
        box('StairArmPlate1' + nm, min(_sx(W - 0.01), _sx(W + 0.014)), yb - 0.035, 0.61, max(_sx(W - 0.01), _sx(W + 0.014)), yb + 0.035, 0.71, M['black'], 0.004)
        for zb in (0.63, 0.69):
            cyl('StairArmBolt1' + nm, 0.009, 0.012, (_sx(W + 0.018), yb, zb), M['satin'], 'x', 6)
        strut('StairBrace1' + nm, Vector((_sx(W - 0.005), yb, 0.66)), Vector((_sx(W + 0.19), yb, _tz - 0.02)), 0.016, M['black'])
    # the step inside the doorway: its top half way up to the floor, its face flush with the cab's side (black)
    xa, xb = _sx(W - 0.02), _sx(W - 0.36)
    box('StepTread2' + nm, min(xa, xb), STAIR_Y0, 0.71, max(xa, xb), STAIR_Y1, 0.75, M['alu'], 0.008)
    box('StairNose2' + nm, min(_sx(W), _sx(W - 0.035)), STAIR_Y0, 0.6, max(_sx(W), _sx(W - 0.035)), STAIR_Y1, 0.752, M['black'], 0.006)
    box('StairRiser2' + nm, min(_sx(W - 0.36), _sx(W - 0.39)), STAIR_Y0, 0.75, max(_sx(W - 0.36), _sx(W - 0.39)), STAIR_Y1, 1.0, M['black'], 0.004)
    box('StairRiser2' + nm, min(_sx(W - 0.03), _sx(W - 0.39)), STAIR_Y0, 0.6, max(_sx(W - 0.03), _sx(W - 0.39)), STAIR_Y1, 0.71, M['black'], 0.004)
    # below the doorway either side of the stairs: closed in down to the door's foot
    box('DoorwayFill' + nm, min(_sx(W), _sx(W - 0.39)), DOOR0, 0.6, max(_sx(W), _sx(W - 0.39)), STAIR_Y0 + 0.005, 1.0, M['black'], 0.004)
    box('DoorwayFill' + nm, min(_sx(W), _sx(W - 0.39)), STAIR_Y1 - 0.005, 0.6, max(_sx(W), _sx(W - 0.39)), 5.24, 1.0, M['black'], 0.004)
    # the doorway's front jamb (the cab's corner shell starts a little ahead of the door's front edge)
    box('DoorJambFront' + nm, min(_sx(W), _sx(W - 0.08)), DOOR1 - 0.03, 0.6, max(_sx(W), _sx(W - 0.08)), 5.24, ZB, M['paint'], 0.01)
    # hand holds: the bar behind the door outside, and one on the doorway's front pillar inside
    cyl('Grab', 0.018, 1.0, (s * (W + 0.05), DOOR0 - 0.06, 1.55), M['alu'], 'z', 12)
    cyl('GrabInner' + nm, 0.016, 0.6, (s * (W - 0.12), 5.12, 1.62), M['alu'], 'z', 12)
    cyl('GrabRear' + nm, 0.016, 0.6, (s * (W - 0.1), DOOR0 + 0.1, 1.55), M['alu'], 'z', 12)   # and on the rear pillar, a hand either side

# ---------------------------------------------------------------- the maker's marks (our own, invented)
MAKER, MODEL = 'DYNASTY', 'STORM 6x6'
TEAL = mat('TruckTeal', (0.0, 0.42, 0.48), 0.2, 0.3)
CHROME_TXT = M['chrome']


def maker_badge(name, at, rot, size):
    """The maker's badge: a chrome chevron mark (two stacked chevrons in a ring) and the name beside it."""
    ob = text(name, MAKER, size, at, rot, CHROME_TXT)
    return ob


# front: on the yellow band under the windscreen, in the middle, on a black plate following the band's slope
_bz = 1.5; _by = 6.0 + (5.82 - 6.0) * (_bz - 1.37) / (ZB - 1.37) + 0.012
_tilt = math.atan2(6.0 - 5.82, ZB - 1.37)
_pl = box('MakerPlateFront', -0.24, -0.006, -0.05, 0.24, 0.006, 0.05, M['black'], 0.01); _pl.location = (0, _by, _bz); _pl.rotation_euler = (_tilt, 0, 0)
_n = Vector((0, math.cos(_tilt), math.sin(_tilt)))           # the band's outward normal
_mb = maker_badge('MakerFront', tuple(Vector((0.03, _by, _bz)) + _n * 0.008), (math.pi / 2 - _tilt, 0, math.pi), 0.06)   # facing forward, reading left to right
for _k in range(2):                                          # the chevron mark left of the name
    _c = box('MakerMark', -0.025, -0.003, -0.004, 0.025, 0.003, 0.004, CHROME_TXT); _c.location = tuple(Vector((-0.17 - 0.012, _by, _bz - 0.006 + _k * 0.02)) + _n * 0.008); _c.rotation_euler = (_tilt, math.radians(35), 0)
    _c = box('MakerMark', -0.025, -0.003, -0.004, 0.025, 0.003, 0.004, CHROME_TXT); _c.location = tuple(Vector((-0.17 + 0.012, _by, _bz - 0.006 + _k * 0.02)) + _n * 0.008); _c.rotation_euler = (_tilt, math.radians(-35), 0)
# rear: on the black band between the lamps; the mudflaps behind the rear wheels
text('MakerRear', MAKER, 0.07, (0.0, REAR - 0.035, 2.125), (math.pi / 2, 0, 0), M['lensWhite'])   # facing back, reading left to right
for s in (-1, 1):
    text('MakerMudflap', MAKER, 0.07, (s * TRACK, AXLES[2] - 0.99, 0.62), (math.pi / 2, 0, 0), M['lensWhite'])
    # side marker lamps along the sill (amber), as in the photos
    for y in (1.6, 0.2, -1.3, -5.3):
        housed_lamp('Light_MarkerSide', M['lensAmberL' if s < 0 else 'lensAmberR'], (s * W, y, 1.06), (s, 0, 0), 0.1, 0.035, 0.02, 0.014)
# the red LED strip along the cab roof's front edge, as in the photos
tube('Light_RoofEdge', [(x * 0.99, y + 0.03, ZT + 0.15) for x, y in roof_ol if y > DOOR0 + 0.3], 0.018, M['lensRed'], 10)

# ---------------------------------------------------------------- the livery
for s in (-1, 1):
    x0, x1 = s * (W + 0.002), s * (W + 0.006)
    # two black stripes behind the cab, stepping down over the rear wheels, into chevrons on the rear module
    for (za, zb) in ((2.3, 2.4), (2.12, 2.18)):
        side('Stripe', [(2.1, za), (2.1, zb), (-1.6, zb), (-2.1, zb - 0.26), (-4.45, zb - 0.26), (-4.45, za - 0.26), (-2.1, za - 0.26), (-1.6, za)], x0, x1, M['black'])
    for i in range(6):
        y = -4.62 - i * 0.2
        side('Chevron', [(y, 1.86), (y - 0.08, 1.86), (y - 0.3, 2.3), (y - 0.22, 2.3)], x0, x1, M['black'])
    rot = (math.pi / 2, 0, math.pi / 2) if s > 0 else (math.pi / 2, 0, -math.pi / 2)
    xt = s * (W + 0.007)
    text('Name1', 'MÝRDALSSANDUR', 0.3, (xt, -1.15, 2.76), rot, M['decal'])   # clear of the trim and the rounded shoulder above
    text('Name2', 'AIRPORT FIRE & RESCUE', 0.2, (xt, -1.15, 2.5), rot, M['decal'])
    text('Number', NO, 0.42, (xt, -5.3, 2.95), rot, M['decal'])
    cyl('EmblemRing', 0.22, 0.008, (xt, 0.55, 2.71), M['decal'], 'x', 48)
    cyl('EmblemFill', 0.19, 0.01, (xt + s * 0.001, 0.55, 2.71), M['paint'], 'x', 48)
    text('Emblem', 'BIAP', 0.11, (xt + s * 0.006, 0.55, 2.71), rot, M['decal'])
text('NumberRoof', NO, 0.9, (0.0, -1.2, BODY_TOP + 0.05), (0, 0, 0), M['decal'])

# ---------------------------------------------------------------- the cab interior: the driver in the middle
box('CabFloor', -WELL_X, CAB_R + 0.12, 1.0, WELL_X, WELL_Y1, 1.04, M['floor'])        # between the wheel housings
box('CabFloorFront', -(W - 0.39), WELL_Y1, 1.0, W - 0.39, YF - 0.3, 1.04, M['floor'])   # the stairs come up either side of it
for _s in (-1, 1):   # the floor beside the stairs, behind and ahead of them
    box('CabFloorSide', min(_s * (W - 0.1), _s * (W - 0.39)), WELL_Y1, 1.0, max(_s * (W - 0.1), _s * (W - 0.39)), STAIR_Y0 - 0.03, 1.04, M['floor'])
    box('CabFloorSide', min(_s * (W - 0.1), _s * (W - 0.39)), STAIR_Y1 + 0.03, 1.0, max(_s * (W - 0.1), _s * (W - 0.39)), YF - 0.3, 1.04, M['floor'])
# the wheel housings: hollow (an inner wall, a top, a front wall; the back wall behind the cab, under the body), sized
# for the wheel's full travel and its steering sweep up to MAX_STEER about the kingpin
for _s in (-1, 1):
    _t = 'L' if _s < 0 else 'R'
    box('WheelHouseInner' + _t, _s * WELL_X, WELL_Y0, 1.0, _s * (WELL_X + 0.04), WELL_Y1, WELL_TOP, M['dash'], 0.01)
    box('WheelHouseTop' + _t, _s * WELL_X, WELL_Y0, WELL_TOP - 0.04, _s * (W - 0.04), WELL_Y1, WELL_TOP, M['dash'], 0.01)
    box('WheelHouseFront' + _t, _s * WELL_X, WELL_Y1 - 0.04, 1.0, _s * (W - 0.04), WELL_Y1, WELL_TOP, M['dash'], 0.01)
    box('WheelHouseBack' + _t, _s * WELL_X, WELL_Y0, 1.0, _s * (W - 0.04), WELL_Y0 + 0.04, WELL_TOP, M['black'], 0.01)
# (the lower dash is built with the cluster: it carries on down from the swept panel's foot in one curve)
# the driver's place: the steering wheel tilted up towards the driver on a column from the dash, the stalks on the
# column (left: indicators and high beam; right: wipers), the gauge cluster in a hood on the dash behind the wheel,
# the light switch to the left of it, the warning-light, siren and pump switches to the right; the turret joystick on
# the right armrest (the console between the driver's legs is gone)
WC = Vector((0.0, 4.6, 1.87))                       # 5 cm lower (owner), the cluster still clear over its rim                       # the wheel's centre
AX = Vector((0.0, -0.5, 0.866)).normalized()        # a little flatter        # the wheel's axis, towards the driver's chest
tilt = math.atan2(-AX.y, AX.z)
strut('SteerColumn', WC - AX * 0.04, WC - AX * 0.6, 0.045, M['black'])
_ce = WC - AX * 0.55
box('SteerColumnCowl', -0.055, _ce.y - 0.02, _ce.z - 0.06, 0.055, 5.1, _ce.z + 0.055, M['dash'], 0.025, 3)   # into the dash
strut('SteerColumnShroud', WC - AX * 0.1, WC - AX * 0.3, 0.05, M['dash'])
bpy.ops.mesh.primitive_torus_add(major_radius=0.23, minor_radius=0.024, major_segments=64, minor_segments=16, location=WC, rotation=(tilt, 0, 0))
swo = bpy.context.active_object; swo.name = 'SteeringWheel'; swo.data.materials.append(M['black'])
for p_ in swo.data.polygons: p_.use_smooth = True
hub = cyl('SteeringHub', 0.07, 0.05, (0, 0, 0), M['black'], 'z', 32, 0.01); hub.parent = swo; hub.location = (0, 0, -0.01)
for ang in (90, 210, 330):                           # three spokes in the wheel's own plane
    sp = box('Spoke', -0.015, 0.0, -0.01, 0.015, 0.2, 0.01, M['black'], 0.005)
    sp.parent = swo; sp.location = (math.cos(math.radians(ang)) * 0.12, math.sin(math.radians(ang)) * 0.12, -0.01); sp.rotation_euler = (0, 0, math.radians(ang - 90))
# the stalks, each its own object so it can move: indicators/high beam on the left, wipers on the right
SB = WC - AX * 0.16
for s_, nm in ((-1, 'Stalk_Indicator'), (1, 'Stalk_Wiper')):
    tip = SB + Vector((s_ * 0.2, 0.02, -0.03))
    st = strut(nm, SB + Vector((s_ * 0.06, 0, 0)), tip, 0.009, M['black'])
    k = cyl(nm + 'Knob', 0.016, 0.05, tuple(tip), M['dash'], 'x', 16)
# the gauge cluster (owner, 2026-11-11: a shield-shaped cluster with a chrome rim, always in the driver's view): raised
# on a stem above the dash, just over the steering wheel's rim as the driver sees it, tilted to face the driver's eyes.
# Its face is the drawn texture T_Cluster_Face.png (make_cluster_texture.py, same outline); the needles are their own
# objects (Needle_Speed, Needle_Rev, Needle_Temp, Needle_Oil, Needle_Fuel, Needle_Volts) turning about their hubs.
from make_cluster_texture import SHIELD          # noqa: E402
CW, CH = 0.62, 0.29                              # the shield's size (m)
root = bpy.data.objects.new('ClusterRoot', None); coll.objects.link(root)
root.location = (0.0, 5.18, 1.98)   # 7 cm lower (owner: less of the view blocked); root.rotation_euler = (math.radians(-20), 0, 0)


def local(u, v, y=0.0):
    return ((u - 0.5) * CW, y, (v - 0.5) * CH)


def mat_face():
    m = bpy.data.materials.new('TruckClusterFace'); m.use_nodes = True
    nt = m.node_tree; bsdf = nt.nodes['Principled BSDF']
    tex = nt.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(os.path.join(HERE, 'T_Cluster_Face.png'))
    nt.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    nt.links.new(tex.outputs['Color'], bsdf.inputs['Emission'])
    bsdf.inputs['Emission Strength'].default_value = 1.4; bsdf.inputs['Roughness'].default_value = 0.35
    return m


def outline_mesh(name, rings, material, cap_first=False, cap_last=False, uv_face=False):
    """Rings of (x, y, z) points round the shield, joined ring to ring (optionally capped)."""
    bm = bmesh.new(); vv = [[bm.verts.new(p) for p in r] for r in rings]; n = len(rings[0])
    for a_ in range(len(rings) - 1):
        for i in range(n):
            j = (i + 1) % n; bm.faces.new((vv[a_][i], vv[a_][j], vv[a_ + 1][j], vv[a_ + 1][i]))
    faces = []
    if cap_first: faces.append(bm.faces.new(list(reversed(vv[0]))))
    if cap_last: faces.append(bm.faces.new(vv[-1]))
    if uv_face:
        uv = bm.loops.layers.uv.new()
        for f in faces:
            for l in f.loops:
                x, y, z = l.vert.co; l[uv].uv = (x / CW + 0.5, z / CH + 0.5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = new_obj(name, bm, material, angle=35); ob.parent = root
    return ob


def grow(pts, d):
    """The outline pushed outward by d metres (about the face's middle, kept in proportion)."""
    return [((u - 0.5) * (1 + 2 * d / CW) + 0.5, (v - 0.5) * (1 + 2 * d / CH) + 0.5) for u, v in pts]


# the face (textured, its front towards the driver: -y), the lens over it
face = [local(u, v, 0.0) for u, v in SHIELD]
bm = bmesh.new(); vs = [bm.verts.new(p) for p in face]; f = bm.faces.new(vs)
uv = bm.loops.layers.uv.new()
for l in f.loops: l[uv].uv = (l.vert.co.x / CW + 0.5, l.vert.co.z / CH + 0.5)
if f.normal.y > 0: f.normal_flip()
fo = new_obj('ClusterFace', bm, mat_face(), angle=10); fo.parent = root
bm = bmesh.new(); vs = [bm.verts.new(local(u, v, -0.022)) for u, v in SHIELD]; bm.faces.new(vs)
lo = new_obj('Glass_Cluster', bm, M['glass']); lo.parent = root
# the chrome rim round it: a rolled lip standing proud of the face, sweeping back to the housing
rim_in = SHIELD; rim_out = grow(SHIELD, 0.022)
outline_mesh('ClusterRim', [[local(u, v, 0.004) for u, v in rim_in], [local(u, v, -0.026) for u, v in rim_in],
                            [local(u, v, -0.03) for u, v in grow(SHIELD, 0.011)], [local(u, v, -0.022) for u, v in rim_out],
                            [local(u, v, 0.02) for u, v in rim_out]], M['chrome'])
# the housing behind (dark, satin), closed at the back
outline_mesh('ClusterHousing', [[local(u, v, 0.02) for u, v in grow(SHIELD, 0.02)], [local(u, v, 0.1) for u, v in grow(SHIELD, 0.012)],
                                [local(u, v, 0.13) for u, v in grow(SHIELD, -0.03)]], M['dash'], cap_last=True)
# the needles (white, red tip) and their black caps; at rest on each dial's lowest mark
for nm, (u, v, r, rest) in {'Speed': (0.645, 0.64, 0.135, -120), 'Rev': (0.355, 0.64, 0.135, -120),
                            'Temp': (0.13, 0.79, 0.055, -60), 'Oil': (0.87, 0.79, 0.055, -60),
                            'Fuel': (0.29, 0.36, 0.05, -40), 'Volts': (0.71, 0.36, 0.05, 0)}.items():
    R = r * CW
    hub = local(u, v, -0.008)
    nd = box('Needle_' + nm, -R * 0.03, -0.002, -R * 0.15, R * 0.03, 0.002, R * 0.88, M['lensWhite'], 0.001)
    nd.parent = root; nd.location = hub; nd.rotation_euler = (0, math.radians(rest), 0)
    tip = box('Needle_' + nm + 'Tip', -R * 0.032, -0.0025, R * 0.7, R * 0.032, 0.0025, R * 0.88, M['lensRed'])
    tip.parent = nd; tip.location = (0, -0.0005, R * 0.79)
    cap = cyl('NeedleCap_' + nm, R * (0.14 if r > 0.1 else 0.24), 0.014, (0, 0, 0), M['black'], 'y', 32, 0.003)
    cap.parent = root; cap.location = local(u, v, -0.016)
# the dashboard rises round it (owner: "the dashboard draws around it, so it sits into it as in real life"): a hump
# swept up from the dash top, its front face parallel to the cluster's and just behind it, its sides following the
# shield's outline a few centimetres out, so the cluster sits set into it; a brow over the top shading the dials
# owner's sketch (2026-11-11): from the cluster's top corners the dash sweeps out and down in long gentle curves all the
# way to both edges of the dashboard. The front face is one surface (parallel to the cluster's, just behind it); seen
# through the windscreen its back slopes smoothly down towards the glass, so it looks finished from outside too.
LO = -0.41                                    # the dash top, in the cluster's frame (cluster 7 cm lower)
EDGE = W - 0.17                               # the dashboard's outer edges
arch = [((u - 0.5) * CW, (v - 0.5) * CH) for u, v in grow(SHIELD, 0.045) if v > 0.6]
arch = [p for p in arch if p[1] > 0.12]       # the arched top only (right to left)
x0, z0 = arch[0][0] + 0.02, arch[0][1] - 0.01


def wing(n=32):
    """The right-hand sweep, from the cluster's top corner down to the dash edge: a gentle S (smoothstep)."""
    pts = []
    for k in range(n, -1, -1):
        t = k / n; e = t * t * (3 - 2 * t)
        pts.append((x0 + (EDGE - x0) * t, z0 + (LO - z0) * e))
    return pts                                 # from the edge up to the corner


front = [(-EDGE, LO - 0.06), (EDGE, LO - 0.06)] + wing() + arch + [(-x, z) for x, z in reversed(wing())]


def back_of(pts, k_drop=0.82, k_in=0.9):
    """The same section further back, pulled in and lowered towards the dash top: the back slopes down."""
    return [(x * k_in, LO - 0.06 + (z - (LO - 0.06)) * (1 - k_drop)) for x, z in pts]


# the upper dash (owner, 2026-11-11: no knife edges, no seams, no gaps at the ends): ONE solid, lofted across the cab
# from door to door. Each slice runs from the foot of the face (where it meets the lower dash) up the face (parallel to
# the cluster's, just behind it, its top following the cluster's arch and the long curves out to the sides), rounds
# over the top edge, runs forward over the top to the foot of the windscreen, and closes underneath.
RM = Matrix.Translation(root.location) @ root.rotation_euler.to_matrix().to_4x4()
edge_l = list(reversed(front[2:]))                           # the face's top edge, left to right (local x, z)
HALF = W - 0.03
glass = front_curve(DOOR1, YF, W, k=400)


def ztop(x):
    if abs(x) >= EDGE: return LO
    for (x0, z0), (x1, z1) in zip(edge_l, edge_l[1:]):
        if min(x0, x1) <= x <= max(x0, x1): return z0 + (z1 - z0) * (x - x0) / ((x1 - x0) or 1e-9)
    return LO


def glass_y(x):
    best = DOOR1
    for (x0, y0), (x1, y1) in zip(glass, glass[1:]):
        if min(x0, x1) <= x <= max(x0, x1): best = max(best, y0 + (y1 - y0) * (x - x0) / ((x1 - x0) or 1e-9))
    return best


NC, NT = 161, 18
cols, foot_line, edge_line, glass_line = [], [], [], []
st_face_a, st_face_b, st_top = [], [], []
for c in range(NC):
    x = (-1 + 2 * c / (NC - 1)) * HALF
    zt = ztop(x)
    foot = RM @ Vector((x, 0.02, LO - 0.06))
    face = RM @ Vector((x, 0.02, max(LO - 0.05, zt - 0.04)))
    r1 = RM @ Vector((x, 0.028, zt - 0.012))
    r2 = RM @ Vector((x, 0.045, zt + 0.004))
    r3 = RM @ Vector((x, 0.075, zt + 0.012))
    F = Vector((x * 0.985, glass_y(x * 0.985) - 0.05, ZB + 0.012))
    F.y = max(F.y, r3.y + 0.06)
    C1 = r3.lerp(F, 0.4); C1.z = max(r3.z, F.z) + 0.006       # stays up over the cluster's housing, then falls away
    top = [r3 * (1 - t) ** 2 + C1 * 2 * (1 - t) * t + F * t * t for t in [k / NT for k in range(1, NT + 1)]]
    under = [Vector((x, F.y - 0.03, 1.45)), Vector((x, foot.y + 0.03, 1.45))]
    cols.append([foot, face, r1, r2, r3] + top + under)
    if zt - 0.08 > LO - 0.02:                                 # double stitching on the face under the top edge
        st_face_a.append((RM @ Vector((x, 0.0185, zt - 0.06)), x)); st_face_b.append((RM @ Vector((x, 0.0185, zt - 0.075)), x))
    st_top.append(top[-3])                                    # a row on the top, along the windscreen
    foot_line.append(foot); edge_line.append(r2); glass_line.append(F)
bm = bmesh.new(); vv = [[bm.verts.new(q) for q in col] for col in cols]; m_ = len(cols[0])
for c in range(NC - 1):
    for i in range(m_):
        j = (i + 1) % m_; bm.faces.new((vv[c][i], vv[c][j], vv[c + 1][j], vv[c + 1][i]))
caps = [bm.faces.new(vv[0]), bm.faces.new(list(reversed(vv[-1])))]
bmesh.ops.triangulate(bm, faces=caps, ngon_method='EAR_CLIP')
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
uvl = bm.loops.layers.uv.new('UVMap'); UVD = {}
for c in range(NC):
    acc = 0.0
    for i in range(m_):
        if i: acc += (cols[c][i] - cols[c][i - 1]).length
        UVD[vv[c][i]] = (cols[c][0].x, acc)
for f_ in bm.faces:
    for l_ in f_.loops: l_[uvl].uv = UVD.get(l_.vert, (0.0, 0.0))
new_obj('DashUpper', bm, M['dash'], angle=60)

# the stitching: thin ribbons lying on the leather, a thread texture along them (4 stitches per 36 mm)
STITCH = bpy.data.materials.new('TruckStitch'); STITCH.use_nodes = True
_nt = STITCH.node_tree; _b = _nt.nodes['Principled BSDF']; _t = _nt.nodes.new('ShaderNodeTexImage')
_t.image = bpy.data.images.load(os.path.join(HERE, 'T_Cab_Stitch.png'))
_nt.links.new(_t.outputs['Color'], _b.inputs['Base Color']); _nt.links.new(_t.outputs['Alpha'], _b.inputs['Alpha'])
_b.inputs['Roughness'].default_value = 0.7
STITCH.blend_method = 'CLIP'; STITCH.shadow_method = 'CLIP'; STITCH.alpha_threshold = 0.5


def ribbon(name, pts, nrm, width=0.0045, lift=0.0007, tile=0.036):
    """A thin strip along pts (world), lying on the surface whose normal at each point is nrm(i); u runs along it."""
    bm = bmesh.new(); prev = None; acc = 0.0; rows = []
    for i, q in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        n_ = nrm(i); sd = t.cross(n_).normalized() * width / 2
        if i: acc += (q - pts[i - 1]).length
        rows.append(([bm.verts.new(q + n_ * lift - sd), bm.verts.new(q + n_ * lift + sd)], acc / tile))
    uv = bm.loops.layers.uv.new('UVMap')
    for (a0, u0), (a1, u1) in zip(rows, rows[1:]):
        f = bm.faces.new((a0[0], a1[0], a1[1], a0[1]))
        for l, uvv in zip(f.loops, ((u0, 0), (u1, 0), (u1, 1), (u0, 1))): l[uv].uv = uvv
    return new_obj(name, bm, STITCH, angle=80)


face_n = (RM.to_3x3() @ Vector((0, -1, 0))).normalized()
for nm_, row in (('StitchFaceA', st_face_a), ('StitchFaceB', st_face_b)):
    # split where the row is interrupted (behind the cluster it is hidden anyway; keep each run continuous)
    run = []
    for q, x in row + [(None, None)]:
        if q is not None and (not run or abs(x - run[-1][1]) < 0.05): run.append((q, x)); continue
        if len(run) > 3: ribbon(nm_, [r[0] for r in run], lambda i: face_n)
        run = [(q, x)] if q is not None else []
ribbon('StitchTop', st_top, lambda i: Vector((0, 0, 1)))
# the trim, seated on the surface: a bright strip at the foot of the windscreen, one in the rounded top edge (half sunk
# into it), a fine dark line on the seam with the lower dash
tube('DashTrimGlass', [tuple(v) for v in glass_line], 0.011, M['alu'], 8)
tube('DashTrimTop', [tuple(v) for v in edge_line], 0.008, M['alu'], 8)
tube('DashTrimFoot', [tuple(RM @ Vector(((-1 + 2 * k / 80) * HALF, 0.016, LO - 0.055))) for k in range(81)], 0.005, M['satin'], 8)
# the lower dash (owner: nothing of the old one left): from the swept panel's foot it curves down and away to the
# floor, full width, leaving room for the knees; the switches sit on its face
LD = [(5.04, 1.535), (5.048, 1.44), (5.07, 1.32), (5.11, 1.2), (5.16, 1.1), (5.22, 1.04), (5.6, 1.04), (5.6, 1.535)]
side('DashLower', LD, -EDGE - 0.02, EDGE + 0.02, M['dash'], 0.012, 2)


def on_lower_dash(z):
    """The lower dash's face y at height z (for things mounted on it)."""
    for (y0, z0), (y1, z1) in zip(LD, LD[1:]):
        if z1 <= z <= z0: return y0 + (y1 - y0) * (z0 - z) / max(1e-6, z0 - z1)
    return LD[0][0]
# the brow: a lip standing out over the arched top, from the hump's face towards the driver
arch_in = [p for p in grow(SHIELD, 0.03) if p[1] > 0.8]; arch_out = [p for p in grow(SHIELD, 0.06) if p[1] > 0.8]
arch_in.sort(key=lambda p: p[0]); arch_out.sort(key=lambda p: p[0])
bm = bmesh.new(); rows = []
for (ui, vi), (uo, vo) in zip(arch_in, arch_out):
    xi, zi = (ui - 0.5) * CW, (vi - 0.5) * CH; xo, zo = (uo - 0.5) * CW, (vo - 0.5) * CH
    rows.append([bm.verts.new(q) for q in ((xi, 0.03, zi), (xi, -0.085, zi + 0.012), (xo, -0.085, zo), (xo, 0.03, zo))])
for r0, r1 in zip(rows, rows[1:]):
    for i in range(4): bm.faces.new((r0[i], r0[(i + 1) % 4], r1[(i + 1) % 4], r1[i]))
bm.faces.new(rows[0]); bm.faces.new(list(reversed(rows[-1])))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
brow = new_obj('DashBrow', bm, M['dash'], 0.008, 2, 40); brow.parent = root
# the light switch (a rotary knob: off, side lights, headlights) left of the wheel, the hazard button beside it
box('SwitchPanelL', -0.62, DOOR1 - 0.02, 1.28, -0.4, DOOR1, 1.48, M['black'], 0.01)
cyl('Switch_Lights', 0.035, 0.03, (-0.51, DOOR1 - 0.035, 1.38), M['dash'], 'y', 32, 0.005)
box('Switch_LightsPointer', -0.513, DOOR1 - 0.052, 1.38, -0.507, DOOR1 - 0.048, 1.41, M['lensWhite'])
box('Switch_Hazard', 0.42, DOOR1 - 0.04, 1.455, 0.48, DOOR1 - 0.01, 1.505, M['lensRed'], 0.006)
# the rocker switches right of the cluster: warning lights, siren, work lights, pump, foam
box('SwitchPanelR', 0.4, DOOR1 - 0.02, 1.3, 0.75, DOOR1, 1.44, M['black'], 0.01)
for i, nm in enumerate(('Switch_Beacons', 'Switch_Siren', 'Switch_WorkLights', 'Switch_Pump', 'Switch_Foam')):
    x = 0.43 + i * 0.065
    sw = box(nm, x, DOOR1 - 0.04, 1.33, x + 0.045, DOOR1 - 0.02, 1.41, (M['lensBlue'], M['lensRed'], M['lensWhite'], M['dash'], M['dash'])[i], 0.004)
    sw.rotation_euler = (math.radians(12), 0, 0)
# the turret joystick on the right armrest of the driver's seat
box('ArmrestR', 0.28, YF - 1.95, 1.62, 0.38, YF - 1.45, 1.72, M['seat'], 0.03)
box('JoystickBase', 0.29, YF - 1.55, 1.72, 0.37, YF - 1.47, 1.76, M['black'], 0.01)
strut('Joystick', (0.33, YF - 1.51, 1.76), (0.33, YF - 1.49, 1.92), 0.016, M['black'])

def seat(nm, x, y):
    box(nm + 'Base', x - 0.26, y - 0.25, 1.04, x + 0.26, y + 0.25, 1.48, M['seat'], 0.06, 4)
    box(nm + 'Cushion', x - 0.25, y - 0.24, 1.48, x + 0.25, y + 0.26, 1.6, M['seat'], 0.05, 4)
    bk = box(nm + 'Back', x - 0.25, y - 0.42, 1.55, x + 0.25, y - 0.28, 2.4, M['seat'], 0.06, 4); bk.rotation_euler = (math.radians(-10), 0, 0)
    box(nm + 'Head', x - 0.16, y - 0.47, 2.42, x + 0.16, y - 0.37, 2.68, M['seat'], 0.04, 4)


seat('SeatDriver', 0.0, YF - 1.75)
seat('SeatL', -0.85, YF - 1.55)
seat('SeatR', 0.85, YF - 1.55)
seat('SeatRearL', -0.265, CAB_R + 0.75)
seat('SeatRearR', 0.265, CAB_R + 0.75)
# the pedals (owner: clutch, brake and accelerator, their joints connected, nothing passing through anything): the
# clutch and brake hang from pins in brackets on the lower dash's face, each arm running down to a bracket on the back
# of its pad; the brake pad the widest; the accelerator stands on the floor on a hinge, leaning back. Spaced to clear the
# steering column's cowl (x within 0.055) and each other.
PY = DOOR1 - 0.1
for nm_, x, w, h in (('Clutch', -0.13, 0.09, 0.07), ('Brake', 0.085, 0.12, 0.075)):
    pc = Vector((x, PY - 0.005, 1.22 + h / 2))                    # the pad's centre
    pivot = Vector((x, on_lower_dash(1.45) - 0.022, 1.45))        # the pin, just in front of the dash face
    pad = box('Pedal_' + nm_, x - w / 2, -0.01, -h / 2, x + w / 2, 0.01, h / 2, M['rubber'], 0.008)
    pad.location = (0, pc.y, pc.z); pad.rotation_euler = (math.radians(-12), 0, 0)
    back = pc + Vector((0, 0.012, 0))
    box('Pedal_' + nm_ + 'Bracket', x - 0.018, back.y - 0.004, pc.z - 0.022, x + 0.018, back.y + 0.012, pc.z + 0.022, M['satin'], 0.004)
    strut('Pedal_' + nm_ + 'Arm', back + Vector((0, 0.008, 0)), pivot, 0.011, M['satin'])
    cyl('Pedal_' + nm_ + 'Pin', 0.012, 0.07, tuple(pivot), M['alu'], 'x', 16)
    for sx in (-1, 1):                                            # the bracket's two cheeks, bolted to the dash face
        box('Pedal_' + nm_ + 'Mount', x + sx * 0.03 - 0.005, pivot.y - 0.02, 1.41, x + sx * 0.03 + 0.005, on_lower_dash(1.45) + 0.01, 1.49, M['satin'], 0.002)
ax_ = 0.235; tilt_a = math.radians(35); L_ = 0.22
acc = box('Pedal_Accel', ax_ - 0.035, -0.008, -L_ / 2, ax_ + 0.035, 0.008, L_ / 2, M['rubber'], 0.006)
hinge = Vector((0, PY + 0.08, 1.06))
acc.location = (0, hinge.y - (L_ / 2) * math.sin(tilt_a), hinge.z + (L_ / 2) * math.cos(tilt_a)); acc.rotation_euler = (tilt_a, 0, 0)
box('Pedal_AccelHinge', ax_ - 0.04, hinge.y - 0.02, 1.04, ax_ + 0.04, hinge.y + 0.02, 1.07, M['satin'], 0.005)
cyl('Pedal_AccelPin', 0.01, 0.08, (ax_, hinge.y, 1.06), M['alu'], 'x', 16)
box('BackWall', -WELL_X, CAB_R + 0.05, 1.0, WELL_X, CAB_R + 0.12, ZT, M['dash'])
for _s in (-1, 1):
    box('BackWallUpper', _s * WELL_X, CAB_R + 0.05, WELL_TOP, _s * (W - 0.06), CAB_R + 0.12, ZT, M['dash'])
box('Headliner', -W + 0.1, CAB_R + 0.1, ZT - 0.05, W - 0.1, roof_front - 0.4, ZT - 0.02, M['dash'])

# ---------------------------------------------------------------- the outside's materials and textures
# (owner, 2026-11-11: texture the outside first, the inside next, then build once). The colours as in the photos: the
# airport lime-yellow paint under a clear coat, gloss black trim and glass frames, black satin wheels, dark tinted glass;
# the drawn tiling textures (make_truck_textures.py) on the treads, tyres, rear grille and warning boards.


def set_bsdf(m, color=None, metal=None, rough=None, coat=None, alpha=None):
    b = m.node_tree.nodes['Principled BSDF']
    if color is not None: b.inputs['Base Color'].default_value = (*color, 1)
    if metal is not None: b.inputs['Metallic'].default_value = metal
    if rough is not None: b.inputs['Roughness'].default_value = rough
    if coat is not None: b.inputs['Clearcoat'].default_value = coat; b.inputs['Clearcoat Roughness'].default_value = 0.03
    if alpha is not None: b.inputs['Alpha'].default_value = alpha


set_bsdf(M['paint'], (0.47, 0.74, 0.0), 0.05, 0.22, 1.0)
set_bsdf(M['black'], (0.008, 0.008, 0.009), 0.0, 0.18, 0.8)
set_bsdf(M['satin'], (0.02, 0.021, 0.023), 0.1, 0.5)
set_bsdf(M['rim'], (0.018, 0.018, 0.02), 0.6, 0.38)
set_bsdf(M['alu'], (0.72, 0.73, 0.75), 1.0, 0.3)
set_bsdf(M['glass'], (0.015, 0.022, 0.026), 0.0, 0.02, alpha=0.38)
set_bsdf(M['nozzle'], (0.05, 0.05, 0.055), 0.7, 0.35)
set_bsdf(M['decal'], (0.006, 0.006, 0.007), 0.0, 0.3)


def textured(name, base, normal=None, metal=0.0, rough=0.5, strength=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; bsdf = nt.nodes['Principled BSDF']
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(HERE, base))
    nt.links.new(t.outputs['Color'], bsdf.inputs['Base Color'])
    if normal:
        tn = nt.nodes.new('ShaderNodeTexImage'); tn.image = bpy.data.images.load(os.path.join(HERE, normal))
        tn.image.colorspace_settings.name = 'Non-Color'
        nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = strength
        nt.links.new(tn.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    bsdf.inputs['Metallic'].default_value = metal; bsdf.inputs['Roughness'].default_value = rough
    return m


T = {
    'plate': textured('TruckTreadPlate', 'T_Truck_DiamondPlate.png', 'T_Truck_DiamondPlate_N.png', 1.0, 0.35),
    'rubber': textured('TruckTyre', 'T_Truck_Rubber.png', 'T_Truck_Rubber_N.png', 0.0, 0.88, 0.6),
    'mesh': textured('TruckGrilleMesh', 'T_Truck_Perforated.png', 'T_Truck_Perforated_N.png', 0.3, 0.55),
    'chev': textured('TruckWarningStripes', 'T_Truck_Chevrons.png', None, 0.0, 0.35),
}


def retexture(ob, mat, tile, flip_u=False):
    """Give an object a textured material (replacing all its slots) and scale its metre UVs to the texture's tile."""
    me = ob.data
    for i in range(len(me.materials)): me.materials[i] = mat
    if me.uv_layers:
        for d in me.uv_layers.active.data:
            d.uv = ((-d.uv.x if flip_u else d.uv.x) / tile, d.uv.y / tile)


for ob in list(bpy.data.objects):
    if ob.type != 'MESH': continue
    nm = ob.name
    if nm.startswith(('Step', 'CabStep', 'SideStep', 'BumperStep', 'RoofDeck')): retexture(ob, T['plate'], 0.3)
    elif nm.startswith('RearGrille') and not nm.startswith('RearGrilleSlat'): retexture(ob, T['mesh'], 0.25)
    elif nm.startswith('RearChevronBoard'): retexture(ob, T['chev'], 0.34, flip_u=nm.endswith('L'))
# the cab inside (owner, 2026-11-11): black leather on the dash with stitched seams, the roof lining fabric, textured
# wall panels, a rubber floor mat, ribbed pedal rubber
T.update({
    'leather': textured('TruckLeather', 'T_Cab_Leather.png', 'T_Cab_Leather_N.png', 0.0, 0.45, 0.35),
    'vinyl': textured('TruckDashVinyl', 'T_Cab_Vinyl.png', 'T_Cab_Vinyl_N.png', 0.0, 0.62, 0.5),
    'liner': textured('TruckHeadliner', 'T_Cab_Headliner.png', 'T_Cab_Headliner_N.png', 0.0, 0.9, 0.8),
    'wall': textured('TruckWallPanel', 'T_Cab_WallPanel.png', 'T_Cab_WallPanel_N.png', 0.0, 0.6, 0.8),
    'floor': textured('TruckFloorMat', 'T_Cab_FloorMat.png', 'T_Cab_FloorMat_N.png', 0.0, 0.8, 1.0),
    'pedal': textured('TruckPedalRubber', 'T_Cab_PedalRubber.png', 'T_Cab_PedalRubber_N.png', 0.0, 0.75, 1.0),
})
for ob in list(bpy.data.objects):
    if ob.type != 'MESH': continue
    nm = ob.name
    if nm.startswith('DashUpper'): retexture(ob, T['vinyl'], 0.12)
    elif nm.startswith(('DashLower', 'CabLining', 'SteerColumnCowl', 'WheelHouse', 'BackWallUpper')): retexture(ob, T['wall'], 0.3)
    elif nm.startswith(('DashBrow', 'SteerColumnShroud')) or nm == 'SteerColumn':
        for i in range(len(ob.data.materials)): ob.data.materials[i] = M['satin']
    elif nm == 'SteeringWheel':
        ob.data.materials[0] = T['leather']
        for d in ob.data.uv_layers.active.data: d.uv = (d.uv.x * 16, d.uv.y * 1.6)   # ~9 cm of grain round the rim, ~1 cm round the grip
    elif nm.startswith('Spoke'):
        for i in range(len(ob.data.materials)): ob.data.materials[i] = M['alu']
    elif nm.startswith('SteeringHub'):
        for i in range(len(ob.data.materials)): ob.data.materials[i] = M['satin']
    elif nm.startswith('Headliner'): retexture(ob, T['liner'], 0.3)
    elif nm.startswith('BackWall'): retexture(ob, T['wall'], 0.3)
    elif nm.startswith('CabFloor'): retexture(ob, T['floor'], 0.5)
    elif nm in ('Pedal_Clutch', 'Pedal_Brake', 'Pedal_Accel'): retexture(ob, T['pedal'], 0.06)
# the tyres: the wheel mesh is shared by all six; its rubber gets the grain
wm = bpy.data.meshes.get('Wheel')
if wm:
    for i, m in enumerate(wm.materials):
        if m is M['rubber']: wm.materials[i] = T['rubber']

for ob in list(bpy.data.objects):
    if ob.name.startswith(('Switch', 'SwitchPanel')) and ob.parent is None:
        z = ob.location.z; dy = on_lower_dash(z) - DOOR1
        ob.location.y += dy + 0.012
        ob.rotation_euler.x += math.radians(-18)
for ob in bpy.data.objects:
    if ob.type == 'MESH':
        assert len(ob.data.materials) and all(m is not None for m in ob.data.materials), ob.name

if PREVIEW:
    bpy.ops.wm.save_as_mainfile(filepath=PREVIEW)
    print('TRUCK DONE', NO, PREVIEW)
else:
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'FireTruck.blend'))
    bpy.ops.object.select_all(action='SELECT')
    out = os.path.join(HERE, 'firetruck%s.glb' % NO)
    bpy.ops.export_scene.gltf(filepath=out, use_selection=True, export_apply=True, export_format='GLB')
    import runpy
    _tool = os.path.join(HERE, '..', '..', 'Tools', 'glb_strip_mesh_names.py')
    sys.argv = [_tool, out]
    runpy.run_path(_tool, run_name='__main__')
    print('TRUCK DONE', NO)
