# The A320's moving control surfaces (owner 2026-11-28, step B: "every part that has to do with the motion also animates
# correctly"): flaps (Fowler: aft on their tracks and down), slats, ailerons, elevators, rudder and the flap track fairings.
# The hinges are FlightGear's own (Models/Fuselage/fuselage.xml of the A320-family, GPL v2: x aft, y right, z up, metres),
# carried into the game model's frame exactly as Art/A320/make_a320_fg.py placed the parts (the rigid move found from the
# source's and the built model's own vertices).
#   blender -b Art/A320/A320-fg.blend --python Art/A320/a320_surfaces.py -- [render.png]
# Prints "SURF <name> pivot (cm) axis (game) trans (cm at full)" for SimTraffic.cpp Surfaces(): in the game frame
# (cm: Blender (x, y, z) m -> (x, -y, z) * 100); the axis already signed so FQuat(axis, +deg) is the surface's positive
# deflection: trailing edge DOWN for flaps, ailerons, elevators and fairings, leading edge DOWN for slats, the rudder's
# trailing edge to the LEFT. With a png: poses every part at full deflection and renders the check once.
# Also "SPOIL" lines: the spoilers, their actuators and rods, and the CFM56 reverser doors (see below).
import bpy, sys, math
import numpy as np
from mathutils import Matrix, Vector, Quaternion

SRC = 'F:/FlightGear-Aircraft/out/A320-200-CFM.glb'
PNG = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv and len(sys.argv) > sys.argv.index('--') + 1 else ''

# FlightGear: name, hinge p1, p2, kind (TE / LE / RUD / FAIR), translation (direction, metres at full flap) or None
FG = [
    ('FlapL1', (-0.40897, -1.96981, -1.4725), (-0.40885, -6.33455, -0.802401), 'TE', ((1, 0, -0.5), 0.30)),
    ('FlapL2', (-0.40519, -6.44122, -0.788081), (1.92808, -13.1844, -0.065162), 'TE', ((1, 0.2, -0.5), 0.27)),
    ('FlapR1', (-0.40897, 1.96981, -1.4725), (-0.40885, 6.33455, -0.802401), 'TE', ((1, 0, -0.5), 0.30)),
    ('FlapR2', (-0.40519, 6.44122, -0.788081), (1.92808, 13.1844, -0.065162), 'TE', ((1, -0.2, -0.5), 0.27)),
    ('SlatL1', (-3.79913, -5.19121, -1.27821), (-5.30738, -2.12168, -1.49252), 'LE', None),
    ('SlatL2', (1.76797, -16.34866, -0.00821), (-3.54786, -5.99527, -1.06999), 'LE', None),
    ('SlatR1', (-3.79913, 5.1891, -1.27821), (-5.30738, 2.11957, -1.49252), 'LE', None),
    ('SlatR2', (1.76797, 16.34650, -0.00821), (-3.54786, 5.99316, -1.06999), 'LE', None),
    ('AileronL', (1.44774, -13.2833, -0.005817), (2.53379, -16.2917, 0.293572), 'TE', None),
    ('AileronR', (1.44774, 13.2833, -0.005817), (2.53379, 16.2917, 0.293572), 'TE', None),
    ('ElevatorL', (15.5724, -0.909268, 0.658597), (17.3985, -6.1085, 1.48068), 'TE', None),
    ('ElevatorR', (17.3985, 6.1085, 1.48068), (15.5724, 0.909268, 0.658597), 'TE', None),
    ('Rudder', (15.6479, 0.001397, 3.88092), (16.4401, -0.000603, 5.71732), 'RUD', None),
    ('FairingL1', (-1.35375, -5.04032, -1.19595), (-1.35375, -4.6714, -1.24688), 'FAIR', None),
    ('FairingL2', (-0.75735, -8.46311, -0.789946), (-0.75735, -8.09297, -0.831006), 'FAIR', None),
    ('FairingL3', (0.60609, -12.0651, -0.344492), (0.60609, -11.7946, -0.374512), 'FAIR', None),
    ('FairingR1', (-1.35375, 5.04032, -1.19595), (-1.35375, 4.6714, -1.24688), 'FAIR', None),
    ('FairingR2', (-0.75735, 8.46311, -0.789946), (-0.75735, 8.09297, -0.831006), 'FAIR', None),
    ('FairingR3', (0.60609, 12.0651, -0.344492), (0.60609, 11.7946, -0.374512), 'FAIR', None),
]

built = {o.name: o for o in bpy.data.objects if o.type == 'MESH'}
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=SRC)
src = {o.name.rsplit('.', 1)[0] if o.name not in built else o.name: o for o in set(bpy.data.objects) - before if o.type == 'MESH'}
A, Bv = [], []
for n in ('Rudder', 'ElevatorL', 'AileronR', 'SlatL1', 'Hstabs'):
    if n in src and n in built and len(src[n].data.vertices) == len(built[n].data.vertices):
        Ms, Mb = src[n].matrix_world, built[n].matrix_world
        A += [tuple(Ms @ v.co) for v in src[n].data.vertices]; Bv += [tuple(Mb @ v.co) for v in built[n].data.vertices]
A, Bv = np.array(A), np.array(Bv)
ca, cb = A.mean(0), Bv.mean(0)
U, S, Vt = np.linalg.svd((A - ca).T @ (Bv - cb))
D = np.diag([1, 1, np.sign(np.linalg.det(Vt.T @ U.T))])
R = Vt.T @ D @ U.T; t = cb - R @ ca
err = np.abs((A @ R.T + t) - Bv).max()
print('SURF fit: %d vertices, worst %.4f m' % (len(A), err))
for o in set(bpy.data.objects) - before: bpy.data.objects.remove(o)
Rm = Matrix(R.tolist())
def place(p): return Rm @ Vector(p) + Vector(t.tolist())
def game(v): return (v.x * 100, -v.y * 100, v.z * 100)


def pts(o): return [o.matrix_world @ v.co for v in o.data.vertices]


out = {}
for name, p1, p2, kind, tr in FG:
    o = built.get(name)
    if not o: print('SURF missing', name); continue
    a, b = place(p1), place(p2); ax = (b - a).normalized()
    vs = pts(o)
    near = min(min((v - a).length for v in vs), min((v - b).length for v in vs))
    # the edge that moves: aft (Blender -y) for trailing surfaces, forward for slats; its motion under +1 deg
    def off(v): return (v - a - ax * (v - a).dot(ax)).length
    if kind == 'LE': tip = max(vs, key=lambda v: v.y + 0.001 * off(v))
    else: tip = max(vs, key=lambda v: -v.y + 0.001 * off(v))
    q = Quaternion(ax, math.radians(1.0)); d = q @ (tip - a) - (tip - a)
    s = (1 if d.x < 0 else -1) if kind == 'RUD' else (1 if d.z < 0 else -1)
    gax = Vector((ax.x, -ax.y, ax.z)) * -s   # (the y mirror turns the sense: the game's FQuat(gax, +deg))
    tv = (0, 0, 0)
    if tr:
        dv = (Rm @ Vector(tr[0])).normalized() * tr[1]; tv = game(dv)
    out[name] = (a, ax * s, tr and (Rm @ Vector(tr[0])).normalized() * tr[1])
    pv = game(a)
    print('SURF %-10s pivot (%.1f, %.1f, %.1f) axis (%.5f, %.5f, %.5f) trans (%.1f, %.1f, %.1f) hinge-to-mesh %.3f m' %
          (name, pv[0], pv[1], pv[2], gax.x, gax.y, gax.z, tv[0], tv[1], tv[2], near))


# Spoilers (to-do: ground spoilers 1-5 at 50 deg on touchdown, roll spoilers 2-5 in the air) and the CFM56's pivot-door
# reversers, read straight from FlightGear's animation XMLs: fuselage.xml (SpoilerL/R1-5, their actuators turning
# factor x deg about their own pivot, the rods sliding out of them by the table) and Fuselage/a320.cfm.xml
# (ReverserL/RDoor1-4 turning factor x reverser-pos-norm; the engine model sits at <offsets> z +0.83109 m in
# A320-200-CFM.xml - kept only if it puts the hinges on the doors). FlightGear's rotation sense is taken from the
# spoilers themselves (+deg = trailing edge up), so every other part follows the same rule.
#   "SPOIL <name> pivot (cm) axis (game; FQuat(axis, +FlightGear deg)) dir (game, rod slide) factor f table ..."
import xml.etree.ElementTree as ET
FGM = 'F:/FlightGear-Aircraft/Airbus/A320-family/A320-family/Models/'
def anims(path, zoff=0.0):
    res = {}
    for a in ET.parse(path).getroot().iter('animation'):
        ax = a.find('axis')
        if ax is None or ax.find('x1-m') is None: continue
        g = lambda k: float(ax.find(k).text)
        p1 = (g('x1-m'), g('y1-m'), g('z1-m') + zoff); p2 = (g('x2-m'), g('y2-m'), g('z2-m') + zoff)
        f = a.find('factor'); tab = [(float(e.find('ind').text), float(e.find('dep').text)) for e in a.iter('entry')]
        for n in a.findall('object-name'):
            res.setdefault(n.text.strip(), (a.find('type').text.strip(), p1, p2, float(f.text) if f is not None else 1.0, tab))
    return res
def mirror_axis(v): return Vector((v.x, -v.y, v.z))   # (Blender right-handed axis -> the game's FQuat axis is -mirror)
spo = anims(FGM + 'Fuselage/fuselage.xml')
sense = 0; SP = {}
for side in 'LR':
    for i in range(1, 6):
        n = 'Spoiler%s%d' % (side, i); o = built.get(n)
        if not o or n not in spo: print('SPOIL missing', n); continue
        typ, p1, p2, f, tab = spo[n]
        a, b = place(p1), place(p2); ax = (b - a).normalized(); vs = pts(o)
        tip = max(vs, key=lambda v: -v.y)
        d = Quaternion(ax, math.radians(1.0)) @ (tip - a) - (tip - a)
        s = 1 if d.z > 0 else -1; sense += s   # (+1: FlightGear's +deg is the right-hand turn about p1->p2)
        near = min(min((v - a).length for v in vs), min((v - b).length for v in vs))
        SP[n] = (a, ax * s); g, pv = -mirror_axis(ax * s), game(a)
        print('SPOIL %-21s pivot (%.1f, %.1f, %.1f) axis (%.5f, %.5f, %.5f) hinge-to-mesh %.3f m' % (n, pv[0], pv[1], pv[2], g.x, g.y, g.z, near))
print('SPOIL FlightGear rotation sense from the spoilers: %+d of 10 (right-hand about p1->p2 = %s)' % (sense, 'yes' if sense > 0 else 'no'))
fs = 1 if sense > 0 else -1
for side in 'LR':
    for i in range(1, 6):
        for n in ('SpoilerActuator%s%d' % (side, i), 'SpoilerActuatorRod%s%d' % (side, i)):
            if n not in spo or n not in built: print('SPOIL missing', n); continue
            typ, p1, p2, f, tab = spo[n]; a, b = place(p1), place(p2); ax = (b - a).normalized()
            if typ == 'rotate':
                SP[n] = (a, ax * fs * (1 if f >= 0 else -1), abs(f)); g, pv = -mirror_axis(ax * fs * (1 if f >= 0 else -1)), game(a)
                print('SPOIL %-21s pivot (%.1f, %.1f, %.1f) axis (%.5f, %.5f, %.5f) factor %.3f' % (n, pv[0], pv[1], pv[2], g.x, g.y, g.z, abs(f)))
            else:
                SP[n] = (ax, tab); dv = mirror_axis(ax)
                print('SPOIL %-21s dir (%.5f, %.5f, %.5f) table %s' % (n, dv.x, dv.y, dv.z, ' '.join('%g:%g' % e for e in tab)))
eng = None
for zoff in (0.0, 0.83109):
    r = anims(FGM + 'Fuselage/a320.cfm.xml', zoff)
    worst = max(min((v - place(r[n][1])).length for v in pts(built[n])) for n in r if n.startswith('ReverserLDoor') and '.' not in n)
    print('SPOIL reverser hinges with engine offset z %.5f: worst hinge-to-mesh %.3f m' % (zoff, worst))
    if eng is None or worst < eng[0]: eng = (worst, r, zoff)
for side in 'LR':
    for i in range(1, 5):
        n = 'Reverser%sDoor%d' % (side, i); o = built.get(n)
        if not o or n not in eng[1]: print('SPOIL missing', n); continue
        typ, p1, p2, f, tab = eng[1][n]; a, b = place(p1), place(p2); ax = (b - a).normalized() * fs * (1 if f >= 0 else -1)
        # (the check: at +deg the door's front edge must swing out of the nacelle and its back edge into the fan stream)
        c = sum((v for v in pts(built['EngineCFM56' + side])), Vector()) / len(built['EngineCFM56' + side].data.vertices)
        vs = pts(o); fr = max(vs, key=lambda v: v.y); bk = min(vs, key=lambda v: v.y)
        q = Quaternion(ax, math.radians(5.0))
        def radial(v): w = v - c; w.y = 0.0; return w.length
        out_f = radial(q @ (fr - a) + a) - radial(fr); out_b = radial(q @ (bk - a) + a) - radial(bk)
        SP[n] = (a, ax, abs(f)); g, pv = -mirror_axis(ax), game(a)
        print('SPOIL %-21s pivot (%.1f, %.1f, %.1f) axis (%.5f, %.5f, %.5f) factor %.0f front %+.3f back %+.3f m at 5 deg' % (n, pv[0], pv[1], pv[2], g.x, g.y, g.z, abs(f), out_f, out_b))

# GEAR (owner 2026-11-28: every part of the landing gear moves as on the real aircraft). Read straight from FlightGear's
# fuselage.xml: the legs' hinges and tables (gear/gear[n]/position-norm: 1 down, 0 up; the doors open 1.0-0.8, the legs
# travel 0.8-0.2, the doors close 0.2-0), the doors' hinges and tables, the door actuators (locked-track + rod table), and
# every "locked-track" linkage as a two-bar chain (pivot A, knee K, end B, knee hinge = FlightGear's lock axis): side stays,
# lock stays, nose drag strut and lock stay, torque and slave links. The model's oleos sit 0.30 m in (make_a320_fg.py
# STATIC_M, straight up); in the air they hang out to FlightGear's length (the tube 0.30 m down).
#   "GEAR <C++ constant line>" for SimTraffic.cpp (game frame cm; rotation axes signed so FQuat(axis, +FlightGear deg))
#   "GEARCHK ..." the check: pivots against their meshes, the solved knees against the built links, rod ends on the door.
# With a png whose name holds "gear": renders the gear down compressed, down extended, doors open, mid-retract and up.
GEARPNG = 'gear' in PNG.lower()
GA = []
for a in ET.parse(FGM + 'Fuselage/fuselage.xml').getroot().iter('animation'):
    d = {'type': (a.findtext('type') or '').strip(), 'objs': [n.text.strip() for n in a.findall('object-name')]}
    for k in ('center', 'target-center', 'slave-center'):
        e = a.find(k)
        if e is not None: d[k] = Vector([float(e.find(c + '-m').text) for c in 'xyz'])
    e = a.find('lock-axis')
    if e is not None: d['lock'] = Vector([float(e.find(c).text) for c in 'xyz'])
    ax = a.find('axis')
    if ax is not None and ax.find('x1-m') is not None:
        d['p1'] = Vector([float(ax.find(c + '1-m').text) for c in 'xyz']); d['p2'] = Vector([float(ax.find(c + '2-m').text) for c in 'xyz'])
    elif ax is not None and ax.find('x') is not None: d['dir'] = Vector([float(ax.find(c).text) for c in 'xyz'])
    d['tab'] = [(float(e.find('ind').text), float(e.find('dep').text)) for e in a.iter('entry')]
    GA.append(d)
def ga(obj, typ, need=()):
    for d in GA:
        if d['type'] == typ and obj in d['objs'] and all(k in d for k in need): return d
    raise KeyError('%s %s' % (obj, typ))
def gtab(tab, x):
    if x <= tab[0][0]: return tab[0][1]
    for (x0, y0), (x1, y1) in zip(tab, tab[1:]):
        if x <= x1: return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return tab[-1][1]
def hinge(d):           # (pivot, axis) in the built frame; FlightGear's +deg is the right-hand turn about axis * fs
    if 'p1' in d: a = place(d['p1']); return a, (place(d['p2']) - a).normalized() * fs
    return place(d['center']), (Rm @ d['dir']).normalized() * fs
def gv(v): return 'FVector(%.2ff, %.2ff, %.2ff)' % (v.x * 100, -v.y * 100, v.z * 100)
def ga_axis(v): g = -mirror_axis(v); return 'FVector(%.5ff, %.5ff, %.5ff)' % (g.x, g.y, g.z)
def gd(v): g = mirror_axis(v); return 'FVector(%.5ff, %.5ff, %.5ff)' % (g.x, g.y, g.z)
def near_mesh(p, names):
    best = 9.0
    for n in names:
        if n in built: best = min(best, min((v - p).length for v in pts(built[n])))
    return best
TUBE_UP = Vector((0, 0, 0.30))    # (make_a320_fg.py STATIC_M: the tubes, wheels and lower links' ends sit this far up)
G = {}
for s, S in (('L', '_L'), ('R', '_R')):
    i = 1 if s == 'L' else 2
    d = ga('MLGFitting' + s, 'rotate'); G['MainHinge' + S] = hinge(d) + (d['tab'],)
    d = ga('MLGGearDoor' + s, 'rotate'); G['BayDoor' + S] = hinge(d) + (d['tab'],)
    d = ga('MLGWingDoor%s2' % s, 'rotate'); G['Fairing' + S] = hinge(d) + (d['tab'],)
    d = ga('SideStay%s2' % s, 'locked-track', ('slave-center',))
    G['SideStay' + S] = (place(ga('SideStay%s1' % s, 'locked-track')['center']), place(d['slave-center']), place(d['center']), (Rm @ d['lock']).normalized())
    d = ga('MLGLockStay%s1' % s, 'locked-track')
    # (the knee hinge: the side stay's - FlightGear's right lock stay axis is not the left one mirrored and leaves its own
    #  end 0.17 m out of the plane; on the left both are the same axis)
    G['LockStay' + S] = (place(d['center']), place(d['slave-center']), place(d['target-center']), G['SideStay' + S][3])
    for nm, fgn in (('Torque', 'MLGTorqueLink%s1'), ('Slave', 'MLGSlaveLink%s1')):
        d = ga(fgn % s, 'locked-track')
        G[nm + S] = (place(d['center']), place(d['slave-center']), place(d['target-center']), (Rm @ d['lock']).normalized())
    d = ga('GearLDoorActuatorRod' + s, 'translate'); a = place(d['p1']); rd = (place(d['p2']) - a).normalized()
    tk = ga('Gear%sDoorActuator' % ('Left' if s == 'L' else 'Right'), 'locked-track')
    G['DoorAct' + S] = (place(tk['center']), place(tk['target-center']), (Rm @ tk['lock']).normalized(), rd, d['tab'])
    d = ga('NLGForewardDoor' + s, 'rotate'); G['NoseDoor' + S] = hinge(d) + (d['tab'], ga('NLGForewardDoor' + s, 'translate')['tab'])
    d = ga('NLGAftDoor' + s, 'rotate'); G['AftDoor' + S] = hinge(d) + (d['tab'], ga('NLGAftDoor' + s, 'translate')['tab'])
d = ga('NLGFitting', 'rotate'); G['NoseHinge'] = hinge(d) + (d['tab'],)
# (the steering: the NLGFittingLower rotate that is not the retraction; its axis leans ~5 deg sideways in FlightGear,
#  which is not symmetric - kept upright in the plane of symmetry)
d = [x for x in GA if x['type'] == 'rotate' and 'NLGFittingLower' in x['objs']][-1]; a = place(d['p1']); sa = place(d['p2']) - a
sa.x = 0; a.x = 0; sa = sa.normalized()
if sa.z < 0: sa = -sa
print('GEARCHK steer pick %s from %s' % (d['objs'][:2], gv(place(d['p1']))))
G['Steer'] = (a, sa)
d = ga('NLGDragStrutAssembly1', 'locked-track'); G['DragStrut'] = (place(d['center']), place(d['slave-center']), place(d['target-center']), (Rm @ d['lock']).normalized())
d = ga('NLGLockStay2', 'locked-track'); G['NoseLock'] = (place(d['center']), place(d['slave-center']), place(d['target-center']), (Rm @ d['lock']).normalized())
d = ga('NLGTorqueLink1', 'locked-track'); G['NoseTorque'] = (place(d['center']), place(d['slave-center']), place(d['target-center']), (Rm @ d['lock']).normalized())
UPV = Vector((0, 0, 1)); FGUP = (Rm @ Vector((0, 0, 1))).normalized()
print('GEARCHK FlightGear up in the built frame %s; strut axes (down): nose %s main %s' % (tuple(round(x, 4) for x in FGUP),
      tuple(round(x, 4) for x in (place(ga('NLGSlidingTubeAxle', 'translate')['p2']) - place(ga('NLGSlidingTubeAxle', 'translate')['p1'])).normalized()),
      tuple(round(x, 4) for x in (place(ga('MLGSlidingTubeAxleL', 'translate')['p2']) - place(ga('MLGSlidingTubeAxleL', 'translate')['p1'])).normalized())))

# the C++ table
print('GEAR // (Art/A320/a320_surfaces.py GEAR: FlightGear fuselage.xml, game frame cm)')
for S in ('_L', '_R'):
    p, ax, tab = G['MainHinge' + S]; print('GEAR MainHinge%s %s %s %.2f' % (S, gv(p), ga_axis(ax), gtab(tab, 0.2)))
    p, ax, tab = G['BayDoor' + S]; print('GEAR BayDoor%s %s %s %.2f' % (S, gv(p), ga_axis(ax), gtab(tab, 0.5)))
    p, ax, tab = G['Fairing' + S]; print('GEAR Fairing%s %s %s %.2f' % (S, gv(p), ga_axis(ax), gtab(tab, 0.8)))
    p, ax, tab, tt = G['NoseDoor' + S]; print('GEAR NoseDoor%s %s %s %.2f lift %s' % (S, gv(p), ga_axis(ax), gtab(tab, 0.5), tt))
    p, ax, tab, tt = G['AftDoor' + S]; print('GEAR AftDoor%s %s %s %.2f lift %s tab %s' % (S, gv(p), ga_axis(ax), gtab(tab, 1.0), tt, tab))
    c, tg, lk, rd, tab = G['DoorAct' + S]; print('GEAR DoorAct%s C %s T %s lock %s rod %s tab %s' % (S, gv(c), gv(tg), gd(lk), gd(rd), tab))
    for nm in ('SideStay', 'LockStay', 'Torque', 'Slave'):
        A, K, B, H = G[nm + S]; print('GEAR %s%s A %s K %s B %s H %s' % (nm, S, gv(A), gv(K), gv(B), gd(H)))
p, ax, tab = G['NoseHinge']; print('GEAR NoseHinge %s %s %.2f' % (gv(p), ga_axis(ax), gtab(tab, 0.2)))
p, ax = G['Steer']; print('GEAR Steer %s dir %s (FQuat(-mirror) form %s)' % (gv(p), gd(ax), ga_axis(ax)))
for nm in ('DragStrut', 'NoseLock', 'NoseTorque'):
    A, K, B, H = G[nm]; print('GEAR %s A %s K %s B %s H %s' % (nm, gv(A), gv(K), gv(B), gd(H)))
print('GEAR FG up (game) %s' % gd(FGUP))

# the same moves as SimTraffic.cpp Gear(), in the built frame, for the check and the pictures
def about(p, ax, deg): return Matrix.Translation(p) @ Quaternion(ax, math.radians(deg)).to_matrix().to_4x4() @ Matrix.Translation(-p)
def solve(A, B, L1, L2, H, side):      # the knee: on the circle about A (plane normal H), L2 from B
    H = H.normalized(); off = (B - A).dot(H); Bp = B - H * off
    r2 = math.sqrt(max(L2 * L2 - off * off, 1e-6)); D = max((Bp - A).length, 1e-6)
    D = min(max(D, abs(L1 - r2) + 1e-4), L1 + r2 - 1e-4)
    e1 = (Bp - A).normalized(); e2 = H.cross(e1)
    a = (L1 * L1 - r2 * r2 + D * D) / (2 * D); h = math.sqrt(max(L1 * L1 - a * a, 0.0))
    return A + e1 * a + e2 * h * side
def bar(P0, K0, Tp, K1):               # the bar's move: carried by its parent Tp, then turned to point at its new knee
    P1 = Tp @ P0; R0 = Tp.to_quaternion(); v0 = R0 @ (K0 - P0)
    R = v0.rotation_difference(K1 - P1) @ R0
    return Matrix.Translation(P1) @ R.to_matrix().to_4x4() @ Matrix.Translation(-P0)
class Link:
    def __init__(s, A, K, B, H, tubeB=False):
        s.A, s.Kf, s.Bf, s.H = A, K, B, H.normalized()
        s.L1, s.L2 = (K - A).length, (K - B).length
        s.side = 1.0 if (K - A).dot(s.H.cross(B - A)) >= 0 else -1.0
        s.B0 = B + (TUBE_UP if tubeB else Vector()); s.A0 = A.copy()
        s.K0 = solve(s.A0, s.B0, s.L1, s.L2, s.H, s.side)     # (the built pose: gear down, struts in)
    def pose(s, TA, TB, TH=Matrix()):
        A1, B1 = TA @ s.A0, TB @ s.B0; H1 = (TH.to_3x3() @ s.H).normalized()
        K1 = solve(A1, B1, s.L1, s.L2, H1, s.side)
        return bar(s.A0, s.K0, TA, K1), bar(s.B0, s.K0, TB, K1), K1
L = {k: Link(*G[k][:4], tubeB=k.startswith(('Torque', 'Slave', 'NoseTorque'))) for k in G if k.startswith(('SideStay', 'LockStay', 'Torque', 'Slave', 'DragStrut', 'NoseLock', 'NoseTorque'))}
for k, l in L.items():
    print('GEARCHK %-12s lengths %.3f %.3f m, built knee moved %.3f m from FlightGear\'s' % (k, l.L1, l.L2, (l.K0 - l.Kf).length))
# which built meshes each bar is (pivot end first)
BARS = {'SideStay': ('SideBraceUp%s', 'SideBraceLo%s'), 'LockStay': ('SideBraceUp%s.LockStay1', 'SideBraceUp%s.LockStay2'),
        'Torque': ('MainLeg%s.TorqueLink1', 'MainLeg%s.TorqueLink2'), 'Slave': ('MainLegUpper%s.SlaveLink1', 'MainLegUpper%s.SlaveLink2'),
        'DragStrut': ('NoseLegUpper.DragStrut1', 'NoseLegUpper.DragStrut2'), 'NoseLock': ('NoseLegUpper.LockStay2', 'NoseLegUpper.LockStay1'),
        'NoseTorque': ('NoseLeg.TorqueLink1', 'NoseLeg.TorqueLink2')}
for k, l in L.items():
    base = k.rstrip('_LR') if k.endswith(('_L', '_R')) else k; S = k[-2:] if k.endswith(('_L', '_R')) else ''
    n1, n2 = (BARS[base][0] % S if '%s' in BARS[base][0] else BARS[base][0]), (BARS[base][1] % S if '%s' in BARS[base][1] else BARS[base][1])
    print('GEARCHK %-12s mesh to A %.3f K %.3f/%.3f B %.3f m (%s, %s)' % (k, near_mesh(l.A0, [n1]), near_mesh(l.K0, [n1]), near_mesh(l.K0, [n2]), near_mesh(l.B0, [n2]), n1, n2))
for k in ('MainHinge_L', 'BayDoor_L', 'Fairing_L', 'NoseDoor_L', 'AftDoor_L', 'NoseHinge'):
    p = G[k][0]; nm = {'MainHinge_L': 'MainLegUpper_L', 'BayDoor_L': 'MainBayDoor_L', 'Fairing_L': 'MainDoor_L.Fairing', 'NoseDoor_L': 'NoseDoor_L',
                       'AftDoor_L': 'NoseLegUpper.AftDoor_L', 'NoseHinge': 'NoseLegUpper'}[k]
    ax = G[k][1].normalized(); vs = pts(built[nm]); dl = min(((v - p) - ax * (v - p).dot(ax)).length for v in vs)
    print('GEARCHK hinge %-12s to its mesh %.3f m, the hinge line %.3f m' % (k, near_mesh(p, [nm]), dl))
for nm in ('NoseLegUpper.DragStrut1', 'NoseLegUpper.DragStrut2', 'SideBraceUp_L.LockActuator1', 'SideBraceUp_L.LockActuator2', 'MainDoor_L.Fairing', 'NoseLegUpper.AftDoor_L'):
    vs = pts(built[nm]); lo = Vector([min(v[i] for v in vs) for i in range(3)]); hi = Vector([max(v[i] for v in vs) for i in range(3)])
    print('GEARCHK box %-28s %s .. %s' % (nm, gv(lo), gv(hi)))

def gear_pose(pos, ext, steer=0.0):    # pos: FlightGear position-norm (1 down); ext: 0 struts in (model), 1 out (air)
    M = {}
    def setm(n, m):
        for k, o in built.items():
            if k == n or k.startswith(n + '.'): M[k] = m
    tube = Matrix.Translation(-TUBE_UP * ext)
    p, ax, tab = G['NoseHinge']; RN = about(p, ax, gtab(tab, pos))
    sp, sa = G['Steer']; ST = about(sp, sa, steer)
    setm('NoseLegUpper', RN); setm('NoseLegUpper.Lower', RN @ ST); setm('NoseLeg', RN @ ST @ tube); setm('NoseWheels', RN @ ST @ tube)
    ds = L['DragStrut']; m1, m2, _ = ds.pose(Matrix(), RN, Matrix()); setm('NoseLegUpper.DragStrut1', m1); setm('NoseLegUpper.DragStrut2', m2)
    nl = L['NoseLock']; m1b, m2b, _ = nl.pose(m2, RN, Matrix()); setm('NoseLegUpper.LockStay2', m1b); setm('NoseLegUpper.LockStay1', m2b)
    nt = L['NoseTorque']; t1, t2, _ = nt.pose(RN @ ST, RN @ ST @ tube, RN @ ST); setm('NoseLeg.TorqueLink1', t1); setm('NoseLeg.TorqueLink2', t2)
    for S in ('_L', '_R'):
        p, ax, tab = G['NoseDoor' + S][:3]; lift = FGUP * gtab(G['NoseDoor' + S][3], pos)
        setm('NoseDoor' + S, Matrix.Translation(lift) @ about(p, ax, gtab(tab, pos)))
        p, ax, tab = G['AftDoor' + S][:3]; lift = FGUP * gtab(G['AftDoor' + S][3], pos)
        setm('NoseLegUpper.AftDoor' + S, Matrix.Translation(lift) @ about(p, ax, gtab(tab, pos)))
        M['NoseLegUpper.AftDoor%sInner' % S] = M['NoseLegUpper.AftDoor' + S]
        p, ax, tab = G['MainHinge' + S]; RM = about(p, ax, gtab(tab, pos))
        for n in ('MainLegUpper' + S, 'MainDoor' + S): setm(n, RM)
        setm('MainLeg' + S, RM @ tube); setm('MainWheels' + S, RM @ tube)
        p, ax, tab = G['Fairing' + S]; F = about(p, ax, gtab(tab, pos)); M['MainDoor%s.Fairing' % S] = F; M['MainDoor%s.FairingInner' % S] = F
        p, ax, tab = G['BayDoor' + S]; BD = about(p, ax, gtab(tab, pos)); setm('MainBayDoor' + S, BD)
        c, tg, lk, rd, tab = G['DoorAct' + S]
        v0 = tg - c; v1 = BD @ tg - c; lk = lk.normalized()
        v0p = (v0 - lk * v0.dot(lk)); v1p = (v1 - lk * v1.dot(lk))
        RA = Matrix.Translation(c) @ v0p.rotation_difference(v1p).to_matrix().to_4x4() @ Matrix.Translation(-c)
        M['GearLDoorActuator' + S[1]] = RA; M['GearLDoorActuatorRod' + S[1]] = RA @ Matrix.Translation(rd * gtab(tab, pos))
        M['_rodgap' + S] = (v1.length - (v0.length + gtab(tab, pos)))
        ss = L['SideStay' + S]; u, lo, _ = ss.pose(Matrix(), RM, Matrix()); setm('SideBraceUp' + S, u); setm('SideBraceLo' + S, lo)
        ls = L['LockStay' + S]; a1, a2, _ = ls.pose(Matrix(), u, Matrix()); setm('SideBraceUp%s.LockStay1' % S, a1); setm('SideBraceUp%s.LockStay2' % S, a2)
        for nm, n1, n2 in (('Torque', 'MainLeg%s.TorqueLink1', 'MainLeg%s.TorqueLink2'), ('Slave', 'MainLegUpper%s.SlaveLink1', 'MainLegUpper%s.SlaveLink2')):
            tl = L[nm + S]; q1, q2, _ = tl.pose(RM, RM @ tube, RM); M[n1 % S] = q1; M[n2 % S] = q2
    return M
for pos, ext in ((1.0, 0.0), (1.0, 1.0), (0.5, 1.0), (0.0, 1.0)):
    M = gear_pose(pos, ext)
    gaps = []
    for k, l in L.items():
        pass
    print('GEARCHK pose pos %.1f ext %.0f: door rod end to its door fitting %+.3f / %+.3f m' % (pos, ext, M['_rodgap_L'], M['_rodgap_R']))

if PNG and not GEARPNG:
    full = {'Flap': 40, 'Slat': 27, 'AileronL': 20, 'AileronR': -25, 'Elevator': -30, 'Rudder': 25, 'Fairing': 22.5}
    for name, (a, ax, tr) in out.items():
        deg = next(v for k, v in full.items() if name.startswith(k))
        if name in ('FairingL2', 'FairingL3', 'FairingR2', 'FairingR3'): deg = 19.0
        M = Matrix.Translation(a) @ Quaternion(ax, math.radians(deg)).to_matrix().to_4x4() @ Matrix.Translation(-a)
        if tr: M = Matrix.Translation(tr) @ M
        o = built[name]; vs0 = pts(o); o.matrix_world = M @ o.matrix_world
        tip = max(range(len(vs0)), key=lambda k: (vs0[k] - a - ax * (vs0[k] - a).dot(ax)).length)
        d = M @ vs0[tip] - vs0[tip]
        print('SURF posed %-10s %6.1f deg: farthest point moves (%.2f, %.2f, %.2f) m (Blender: -y aft, -x left)' % (name, deg, d.x, d.y, d.z))
    # the spoilers at 50 deg (ground spoilers; their actuators turn and the rods slide as FlightGear's tables), the
    # reverser doors fully out (70 deg); .Inner and .Placards children move with their part, as BindGroup does in the game
    def kids(n): return [o for k, o in built.items() if k == n or k.startswith(n + '.')]
    def interp(tab, x):
        if x <= tab[0][0]: return tab[0][1]
        for (x0, y0), (x1, y1) in zip(tab, tab[1:]):
            if x <= x1: return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
        return tab[-1][1]
    SPD = 50.0
    for n, v in SP.items():
        if len(v) != 2 and len(v) != 3: continue
        if n.startswith('SpoilerActuatorRod'): continue
        a, ax = v[0], v[1]; deg = SPD * (v[2] if len(v) == 3 else 1.0)
        if n.startswith('Reverser'): deg = v[2]
        M = Matrix.Translation(a) @ Quaternion(ax, math.radians(deg)).to_matrix().to_4x4() @ Matrix.Translation(-a)
        for o in kids(n): o.matrix_world = M @ o.matrix_world
        if n.startswith('SpoilerActuator'):
            rn = n.replace('Actuator', 'ActuatorRod'); dr, tab = SP[rn]
            R = Quaternion(ax, math.radians(deg)).to_matrix()
            for o in kids(rn): o.matrix_world = Matrix.Translation(R @ (dr * interp(tab, SPD))) @ M @ o.matrix_world
        print('SPOIL posed %-21s %5.1f deg' % (n, deg))
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.show_cavity = True
    sc.render.resolution_x, sc.render.resolution_y = 1000, 640
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
    sc.world = sc.world or bpy.data.worlds.new('W')
    # (views: the left wing from behind and above; the tail from behind, left and above; the right wing from below, ahead)
    # (+ the spoilers from behind the left wing, the left engine's reverser doors from outboard and behind, the right engine's from below)
    for i, (at, to, lens) in enumerate((((-22, -38, 16), (-8, -14, 3.5), 35), ((-14, -31, 6.5), (0, -30, 6.5), 45), ((16, 4, -1), (8, -14, 3.5), 30),
                                        ((-9, -24, 7.5), (-8, -13.5, 3.8), 40), ((-15, -16, 3.5), (-5.6, -8.9, 1.8), 50), ((9, -14, -1.5), (5.6, -8.9, 1.6), 45))):
        cam.location = at; cam.data.lens = lens
        cam.rotation_euler = (Vector(to) - cam.location).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = PNG.replace('.png', '_%d.png' % i); bpy.ops.render.render(write_still=True)
    print('SURF rendered', PNG)

if GEARPNG:
    # the gear pose pictures: (position-norm, strut extension) = on the ground, in the air, doors open, mid-retraction, up
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.show_cavity = True
    sc.render.resolution_x, sc.render.resolution_y = 800, 520
    cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
    sc.world = sc.world or bpy.data.worlds.new('W')
    orig = {k: o.matrix_world.copy() for k, o in built.items()}
    VIEWS = (('mainside', (-10.5, -10.0, 1.2), (-3.4, -12.0, 1.6), 40), ('mainfront', (-5.5, -4.5, 0.3), (-2.6, -12.0, 1.4), 40),
             ('noseside', (-6.0, 0.8, 1.2), (0.0, -0.4, 1.3), 40))
    for tag, pos, ext in (('ground', 1.0, 0.0), ('air', 1.0, 1.0), ('doorsopen', 0.8, 1.0), ('midretract', 0.5, 1.0), ('up', 0.0, 1.0)):
        M = gear_pose(pos, ext)
        for k, o in built.items(): o.matrix_world = (M[k] @ orig[k]) if k in M else orig[k]
        for vn, at, to, lens in VIEWS:
            cam.location = at; cam.data.lens = lens
            cam.rotation_euler = (Vector(to) - cam.location).to_track_quat('-Z', 'Y').to_euler()
            sc.render.filepath = PNG.replace('.png', '-%s-%s.png' % (tag, vn)); bpy.ops.render.render(write_still=True)
    for k, o in built.items(): o.matrix_world = orig[k]
    print('GEAR rendered', PNG)
