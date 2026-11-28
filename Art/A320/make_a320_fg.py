# The game's A320 from the FlightGear A320-200 CFM model (GPL v2, FlightGear A320-family authors - THIRD-PARTY-NOTICES),
# owner 2026-11-28, step A: the model in the game with today's behaviour. Converted from the AC3D files by
# F:/FlightGear-Aircraft/tools/ac3d_convert.py (F:/FlightGear-Aircraft/out/A320-200-CFM.glb); plain paint from
# Art/A320/clean_fg_textures.py (run that first).
#   blender -b --python Art/A320/make_a320_fg.py      (or: python Tools/import_model.py Art/A320/make_a320_fg.py Art/A320/A320.glb A320)
# What it does:
#  - puts the model in the old A320.glb's convention: the nose wheel's ground point at the origin, the nose towards
#    Blender +y, up +z, its left at -x (the game: cm (x, -y, z) * 100), pitched by a hair so nose and main tyres both touch
#  - names the gear parts as the game binds them (SimTraffic.cpp Gear()): NoseWheels, NoseLeg, NoseLegUpper, NoseDoor_L/R,
#    MainWheels_L/R, MainLeg_L/R, MainLegUpper_L/R, MainDoor_L/R, MainBayDoor_L/R, SideBraceUp/Lo_L/R; their linkages as
#    "<part>.<piece>" (BindGroup takes "<name>." with the part); the hinge parts' origins on their hinges
#  - keeps every control surface, reverser door, cabin and cargo door its own named part (later steps animate them)
#  - prints the gear pivots, door and lamp positions the code uses (cm, game frame) as "FGPOS ..." lines
import bpy, mathutils, mathutils.bvhtree, bmesh, math, os, sys
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = 'F:/FlightGear-Aircraft/out/A320-200-CFM.glb'
TEX = os.path.join(HERE, 'Textures', 'FG')
OUT = os.path.join(HERE, 'A320.glb')
if '--' in sys.argv and len(sys.argv) > sys.argv.index('--') + 1:
    OUT = sys.argv[sys.argv.index('--') + 1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)
obs = [o for o in bpy.data.objects if o.type == 'MESH']
for o in obs:   # every node at the origin in the source: vertices hold the places
    o.data.transform(o.matrix_world); o.matrix_world = Matrix.Identity(4)
    o.parent = None
for o in [o for o in bpy.data.objects if o.type != 'MESH']:
    bpy.data.objects.remove(o)
O = {o.name: o for o in obs}


def verts(o):
    M = o.matrix_basis.copy(); return [M @ v.co for v in o.data.vertices]


def bbox(names):
    vs = [v for n in names for v in verts(bpy.data.objects[n])]
    return Vector([min(v[i] for v in vs) for i in range(3)]), Vector([max(v[i] for v in vs) for i in range(3)])


def apply_all(M):
    for o in obs:
        o.data.transform(M)


# 1. turn: the source's nose is Blender -x, its left +y -> nose +y, left -x (a quarter turn about z, no mirroring)
apply_all(Matrix.Rotation(math.radians(-90), 4, 'Z'))
lo, hi = bbox(['NLGWheels'])
apply_all(Matrix.Translation(Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))))
# 2. pitch so the main tyres stand on the ground too
lm, hm = bbox(['MLGWheelsL', 'MLGWheelsR'])
ang = math.atan2(-lm.z, -(lm.y + hm.y) / 2)   # (main tyres are behind: y < 0)
apply_all(Matrix.Rotation(-ang, 4, 'X'))
lm, hm = bbox(['MLGWheelsL', 'MLGWheelsR'])
print('FGPOS pitch %.3f deg, main tyre bottom %.4f m' % (math.degrees(ang), lm.z))

# 2b. the struts compressed as at the stand (owner 2026-11-28: it sat a little high). The model comes with its oleos fully
# out (in the air); a real A320 at the stand has them pushed in: VT tip about 11.9-12.0 m, door 1 sill about 3.4-3.5 m, the
# CFM56 nacelles about 0.6 m off the ground (Airbus A320 Aircraft Characteristics, ground clearances). The sliding tubes
# and wheels go up into their cylinders by STATIC_M, the torque and slave links fold their knees to stay on both ends, and
# the whole aircraft comes down by the same: the tyres stay on the ground, the attitude is unchanged. (SimTraffic.cpp's
# ExtMain / ExtNose = this in cm: in the air the struts hang out to the model's full length again.)
STATIC_M = 0.30
def _ends(o):
    vs = [v.co.copy() for v in o.data.vertices]; c = sum(vs, Vector()) / len(vs)
    far = max(vs, key=lambda v: (v - c).length); other = max(vs, key=lambda v: (v - far).length)
    a = [v for v in vs if (v - far).length < 0.03]; b = [v for v in vs if (v - other).length < 0.03]
    return sum(a, Vector()) / len(a), sum(b, Vector()) / len(b)
def _turn(o, pivot, q, move=Vector()):
    o.data.transform(Matrix.Translation(pivot + move) @ q.to_matrix().to_4x4() @ Matrix.Translation(-pivot))
def fold(upper, lower, up):
    U, L = O[upper], O[lower]; u1, u2 = _ends(U); l1, l2 = _ends(L)
    uk, lk = min([(a, b) for a in (u1, u2) for b in (l1, l2)], key=lambda p: (p[0] - p[1]).length)
    A = u2 if uk is u1 else u1; B = l2 if lk is l1 else l1; K = (uk + lk) / 2
    B2 = B + up; La, Lb = (K - A).length, (K - B).length; D = (B2 - A).length
    e1 = (B2 - A).normalized(); nrm = (B - A).cross(K - A).normalized(); e2 = nrm.cross(e1)
    if (K - A).dot(e2) < 0: e2 = -e2
    a = (La * La - Lb * Lb + D * D) / (2 * D); h = math.sqrt(max(La * La - a * a, 0.0))
    K2 = A + e1 * a + e2 * h
    _turn(U, A, (K - A).rotation_difference(K2 - A))
    _turn(L, B, (K - B).rotation_difference(K2 - B2), up)
    print('FGPOS fold %s/%s A %s B %s knee %s -> %s' % (upper, lower, tuple(round(x, 2) for x in A), tuple(round(x, 2) for x in B),
                                                     tuple(round(x, 2) for x in K), tuple(round(x, 2) for x in K2)))
up = Vector((0, 0, STATIC_M))
for s in ('L', 'R'):
    for n in ('MLGWheels' + s, 'MLGSlidingTubeAxle' + s, 'MLGCables%s2' % s):
        O[n].data.transform(Matrix.Translation(up))
    fold('MLGTorqueLink%s1' % s, 'MLGTorqueLink%s2' % s, up)
    fold('MLGSlaveLink%s1' % s, 'MLGSlaveLink%s2' % s, up)
for n in ('NLGWheels', 'NLGSlidingTubeAxle'):
    O[n].data.transform(Matrix.Translation(up))
fold('NLGTorqueLink1', 'NLGTorqueLink2', up)
apply_all(Matrix.Translation(-up))
lo, hi = bbox(['Fuselage']); print('FGPOS static: fuselage top %.3f m, bottom %.3f m' % (hi.z, lo.z))

# 3. the plain paint
for img in bpy.data.images:
    p = os.path.join(TEX, img.name + '.png')
    if os.path.isfile(p):
        img.filepath = p; img.source = 'FILE'; img.unpack(method='REMOVE') if img.packed_file else None; img.reload()
        img.pack()
        print('paint', img.name)

# 4. the gear names the game binds
REN = {}
for s, S in (('L', '_L'), ('R', '_R')):
    REN.update({
        'MLGWheels' + s: 'MainWheels' + S,
        'MLGSlidingTubeAxle' + s: 'MainLeg' + S,
        'MLGTorqueLink%s1' % s: 'MainLeg%s.TorqueLink1' % S, 'MLGTorqueLink%s2' % s: 'MainLeg%s.TorqueLink2' % S,
        'MLGFitting' + s: 'MainLegUpper' + S, 'MLGFitting%s.Placards' % s: 'MainLegUpper%s.Placards' % S,
        'MLGCables%s2' % s: 'MainLegUpper%s.Cables' % S,
        'MLGSlaveLink%s1' % s: 'MainLegUpper%s.SlaveLink1' % S, 'MLGSlaveLink%s2' % s: 'MainLegUpper%s.SlaveLink2' % S,
        'MLGWingDoor%s1' % s: 'MainDoor' + S, 'MLGWingDoor%s1.Inner' % s: 'MainDoor%s.Inner' % S,
        'MLGWingDoor%s2' % s: 'MainDoor%s.Fairing' % S, 'MLGWingDoor%s2.Inner' % s: 'MainDoor%s.FairingInner' % S,
        'MLGGearDoor' + s: 'MainBayDoor' + S, 'MLGGearDoor%s.Inner' % s: 'MainBayDoor%s.Inner' % S,
        'SideStay%s1' % s: 'SideBraceUp' + S, 'SideStay%s2' % s: 'SideBraceLo' + S,
        'MLGLockStay%s1' % s: 'SideBraceUp%s.LockStay1' % S, 'MLGLockStay%s2' % s: 'SideBraceUp%s.LockStay2' % S,
        'MLGLockstayActuator%s1' % s: 'SideBraceUp%s.LockActuator1' % S, 'MLGLockstayActuator%s2' % s: 'SideBraceUp%s.LockActuator2' % S,
        'NLGForewardDoor' + s: 'NoseDoor' + S, 'NLGForewardDoor%s.Inner' % s: 'NoseDoor%s.Inner' % S,
        'NLGAftDoor' + s: 'NoseLegUpper.AftDoor' + S, 'NLGAftDoor%s.Inner' % s: 'NoseLegUpper.AftDoor%sInner' % S,
    })
REN.update({
    'NLGWheels': 'NoseWheels', 'NLGSlidingTubeAxle': 'NoseLeg',
    'NLGTorqueLink1': 'NoseLeg.TorqueLink1', 'NLGTorqueLink2': 'NoseLeg.TorqueLink2',
    'NLGFitting': 'NoseLegUpper', 'NLGFitting.Placards': 'NoseLegUpper.Placards',
    'NLGFittingLower': 'NoseLegUpper.Lower', 'NLGFittingLower.Placards': 'NoseLegUpper.LowerPlacards',
    'NLGFittingPanel': 'NoseLegUpper.Panel', 'NurbsPath.004': 'NoseLegUpper.Cable',
    'NLGDragStrutAssembly1': 'NoseLegUpper.DragStrut1', 'NLGDragStrutAssembly2': 'NoseLegUpper.DragStrut2',
    'NLGLockStay1': 'NoseLegUpper.LockStay1', 'NLGLockStay2': 'NoseLegUpper.LockStay2',
    'ReverserActuators.001': 'ReverserActuatorsR', 'ReverserActuators': 'ReverserActuatorsL',
})
for a, b in REN.items():
    O[a].name = b; O[a].data.name = b
N = {o.name: o for o in obs}


def set_origin(o, p):
    o.data.transform(Matrix.Translation(-p)); o.location = p


def ends(o):
    # the two ends of a long part: the extreme vertices along its longest bbox diagonal
    vs = verts(o); c = sum(vs, Vector()) / len(vs)
    far = max(vs, key=lambda v: (v - c).length)
    other = max(vs, key=lambda v: (v - far).length)
    a = [v for v in vs if (v - far).length < 0.04]; b = [v for v in vs if (v - other).length < 0.04]
    return sum(a, Vector()) / len(a), sum(b, Vector()) / len(b)


def cm(v):
    return '(%.1f, %.1f, %.1f)' % (v.x * 100, -v.y * 100, v.z * 100)


# the hinges: the struts' trunnions at their fittings' tops, the doors' hinge edges, the stays' ends
def top_of(o, depth):
    vs = verts(o); zt = max(v.z for v in vs); t = [v for v in vs if v.z > zt - depth]
    c = sum(t, Vector()) / len(t); return Vector((c.x, c.y, zt - depth * 0.5))


hn = top_of(N['NoseLegUpper'], 0.16); set_origin(N['NoseLegUpper'], hn); print('FGPOS NoseLegUpper hinge', cm(hn))
for S, sg in (('_L', -1), ('_R', 1)):
    h = top_of(N['MainLegUpper' + S], 0.16); set_origin(N['MainLegUpper' + S], h); print('FGPOS MainLegUpper%s hinge' % S, cm(h))
    d = N['NoseDoor' + S]; vs = verts(d); xo = max(abs(v.x) for v in vs); e = [v for v in vs if abs(v.x) > xo - 0.03]
    p = sum(e, Vector()) / len(e); set_origin(d, p); print('FGPOS NoseDoor%s hinge' % S, cm(p))
    d = N['MainBayDoor' + S]; vs = verts(d); xi = min(abs(v.x) for v in vs); e = [v for v in vs if abs(v.x) < xi + 0.03]
    p = sum(e, Vector()) / len(e); set_origin(d, p); print('FGPOS MainBayDoor%s hinge' % S, cm(p))
    u1, u2 = ends(N['SideBraceUp' + S]); l1, l2 = ends(N['SideBraceLo' + S])
    pairs = [(u, l) for u in (u1, u2) for l in (l1, l2)]
    uk, lk = min(pairs, key=lambda p: (p[0] - p[1]).length)
    ua = u2 if uk is u1 else u1; lb = l2 if lk is l1 else l1
    set_origin(N['SideBraceUp' + S], ua); set_origin(N['SideBraceLo' + S], lb)
    print('FGPOS SideBrace%s A %s B %s knee %s' % (S, cm(ua), cm(lb), cm((uk + lk) / 2)))
    lo, hi = bbox(['MainWheels' + S]); print('FGPOS MainAxle%s %s radius %.1f' % (S, cm((lo + hi) / 2), (hi.z - lo.z) * 50))
    lo, hi = bbox(['MainLeg' + S]); print('FGPOS MainLegAt%s %s' % (S, cm((lo + hi) / 2)))
lo, hi = bbox(['NoseWheels']); print('FGPOS NoseAxle %s radius %.1f' % (cm((lo + hi) / 2), (hi.z - lo.z) * 50))
lo, hi = bbox(['NoseLeg']); print('FGPOS NoseLeg box %s %s' % (cm(lo), cm(hi)))

# 5. the places the service vehicles, jetways and lamps use (doors: outer skin, sill, centre along the body)
for n in ('DoorL1', 'DoorR1', 'DoorL4', 'DoorR4', 'Cargo1', 'Cargo2', 'Cargo3', 'EngineCFM56L', 'EngineCFM56R',
          'ExhaustCFM56L', 'ExhaustCFM56R', 'Wings', 'Fuselage', 'Rudder', 'LightWingL', 'LightWingR', 'navglass', 'GPUServiceDoor'):
    if n in N:
        lo, hi = bbox([n]); print('FGPOS %s min %s max %s' % (n, cm(lo), cm(hi)))

# 6. decals a hair off the skin (no flicker): every placard part pushed 2 mm out along its normals
for o in obs:
    if o.name.endswith('Placards'):
        bm = bmesh.new(); bm.from_mesh(o.data); bm.normal_update()
        # (out = the normal of the part it sits on, at the nearest point: the decal normals are not all outward, and
        #  the gear struts' placards wrap round the strut)
        par = bpy.data.objects.get(o.name[:-len('Placards')].rstrip('.'))
        M = o.matrix_basis; Mi = M.to_3x3().inverted()
        tree = None
        if par is not None:
            pb = bmesh.new(); pb.from_mesh(par.data); pb.transform(par.matrix_basis); pb.normal_update()
            tree = mathutils.bvhtree.BVHTree.FromBMesh(pb); pb.free()
        for v in bm.verts:
            w = M @ v.co; n = M.to_3x3() @ v.normal
            if tree is not None:
                hit = tree.find_nearest(w)
                if hit[0] is not None: n = hit[1]
            v.co += Mi @ (n.normalized() * 0.002)
        bm.to_mesh(o.data); bm.free()
# 7. the doubled faces Tools/find_zfight.py found (faces lying on another part's, facing the same way):
# the parts fg-nudge.json moves by a millimetre or two (the tool's own suggestions, summed), and the gear placard's
# faces lifted 2 mm off its fitting along their own normals, so too the outflow valve door's off the skin and the
# spoilers' off the flaps (curved, where a whole-part move does not clear them).
NUDGE = os.path.join(HERE, 'fg-nudge.json')
if os.path.exists(NUDGE):
    import json
    for n, d in json.load(open(NUDGE)).items():
        o = bpy.data.objects.get(n)
        if o is None or o.type != 'MESH': continue
        w = mathutils.Vector((d[0], -d[2], d[1]))
        loc = o.matrix_basis.to_3x3().inverted() @ w
        for v in o.data.vertices: v.co += loc
def lift_doubled(top, under, gap=0.002, near=0.0012):
    T, Un = bpy.data.objects[top], bpy.data.objects[under]
    ub = bmesh.new(); ub.from_mesh(Un.data); ub.transform(Un.matrix_basis); ub.normal_update()
    tree = mathutils.bvhtree.BVHTree.FromBMesh(ub); ub.free()
    bm = bmesh.new(); bm.from_mesh(T.data); bm.normal_update()
    M = T.matrix_basis; R = M.to_3x3(); Ri = R.inverted(); move = {}
    for f in bm.faces:
        n = (R @ f.normal).normalized()
        pts = [M @ v.co for v in f.verts] + [M @ f.calc_center_median()]
        hits = [tree.find_nearest(p) for p in pts]
        if any(h[0] is not None and h[3] < near and abs(h[1].dot(n)) > 0.8 for h in hits):
            for v in f.verts:
                d = Ri @ (n * gap); move[v] = d if v not in move or d.length > move[v].length else move[v]
    for v, d in move.items(): v.co += d
    bm.to_mesh(T.data); bm.free(); print('FGPOS lifted %d verts of %s off %s' % (len(move), top, under))
for top, under in (('MainLegUpper_R.Placards', 'MainLegUpper_R'), ('OutflowValveDoor1', 'Fuselage'), ('SpoilerL4', 'FlapL2'),
                   ):
    lift_doubled(top, under)
lift_doubled('SpoilerR4', 'FlapR2', near=0.004)   # (its big faces lie on the flap away from their corners)

# 8. the cockpit windows: dark tinted glass (opaque: there is no flight deck behind it) in a dark frame, each of the six
# panes inset 3.5 cm for its frame, as on a real A320's windshield and side windows
glass = bpy.data.materials.new('CockpitGlass'); glass.use_nodes = True
b = glass.node_tree.nodes['Principled BSDF']
b.inputs['Base Color'].default_value = (0.010, 0.014, 0.020, 1); b.inputs['Roughness'].default_value = 0.04
b.inputs['Specular'].default_value = 0.9; b.inputs['Metallic'].default_value = 0.0
frame = bpy.data.materials.new('CockpitFrame'); frame.use_nodes = True
b = frame.node_tree.nodes['Principled BSDF']
b.inputs['Base Color'].default_value = (0.045, 0.047, 0.05, 1); b.inputs['Roughness'].default_value = 0.5
ws = bpy.data.objects['Windsheild']; ws.data.materials.clear(); ws.data.materials.append(glass); ws.data.materials.append(frame)
bm = bmesh.new(); bm.from_mesh(ws.data)
for f in bm.faces: f.material_index = 1
seen = set()
for f in list(bm.faces):
    if f in seen: continue
    isl, st = [], [f]; seen.add(f)
    while st:
        g = st.pop(); isl.append(g)
        for e in g.edges:
            for h in e.link_faces:
                if h not in seen: seen.add(h); st.append(h)
    bmesh.ops.inset_region(bm, faces=isl, thickness=0.035, depth=0.0, use_even_offset=True)
    for g in isl: g.material_index = 0
bm.to_mesh(ws.data); bm.free()
print('FGPOS cockpit windows: 6 panes framed')
empty = sum(1 for o in obs for s in o.material_slots if s.material is None)
assert empty == 0, 'empty material slots: %d' % empty
for o in obs: o.select_set(True)
bpy.context.view_layer.objects.active = obs[0]
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'A320-fg.blend'))
bpy.ops.export_scene.gltf(filepath=OUT, export_format='GLB', use_selection=True, export_yup=True)
print('DONE', OUT, len(obs), 'parts')
