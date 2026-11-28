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
# 7. the parts Tools/find_zfight.py found fighting, moved by its hair (glTF y-up vectors, summed over its rounds)
NUDGE = os.path.join(HERE, 'fg-nudge.json')
if os.path.exists(NUDGE):
    import json
    for n, d in json.load(open(NUDGE)).items():
        o = bpy.data.objects.get(n)
        if o is None or o.type != 'MESH': continue
        w = mathutils.Vector((d[0], -d[2], d[1]))
        loc = o.matrix_basis.to_3x3().inverted() @ w
        for v in o.data.vertices: v.co += loc
empty = sum(1 for o in obs for s in o.material_slots if s.material is None)
assert empty == 0, 'empty material slots: %d' % empty
for o in obs: o.select_set(True)
bpy.context.view_layer.objects.active = obs[0]
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'A320-fg.blend'))
bpy.ops.export_scene.gltf(filepath=OUT, export_format='GLB', use_selection=True, export_yup=True)
print('DONE', OUT, len(obs), 'parts')
