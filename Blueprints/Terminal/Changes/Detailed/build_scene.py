# To-do 213, detailed renders: the REAL existing terminal (the game's own layout, Data/terminal-layout.json, with the
# meshes of F:/ADP-Export/terminal.glb and the annex FBX pieces of Art/TerminalAnnex, the jetways of
# F:/ADP-Export/jetways.glb, the A320 of Art/A320/A320-whole.glb on every stand) plus the NEW parts of the accepted
# plan (docs/Blueprints/Terminal-Changes/TC-6-Summary.md) in their own collection NEW, every new part with a gold edge.
# Frame: plan x -> X - 200, plan z (airside) -> Y, height -> Z (as render_concept.py).  Saves F:/ADP-Export/tc_detailed.blend
#   "C:\Program Files\Blender Foundation\Blender 3.3\blender.exe" -b --python Blueprints/Terminal/Changes/Detailed/build_scene.py
import bpy, json, math, os, random
from mathutils import Matrix, Vector, Quaternion
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
ANNEX = os.path.join(ROOT, 'Art', 'TerminalAnnex')
OUTBLEND = 'F:/ADP-Export/tc_detailed.blend'
OX = 200.0
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene


def coll(name):
    c = bpy.data.collections.new(name); sc.collection.children.link(c); return c


EX, NEWC, PLANES, LBL = coll('EXIST'), coll('NEW'), coll('AIRCRAFT'), coll('LABELS')
C = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))      # glTF -> Blender
CI = C.inverted()
R = Matrix.Translation((-OX, 0, 0)) @ Matrix.Rotation(math.pi, 4, 'Z')    # Blender import frame -> plan frame

# ---------------------------------------------------------------- the terminal's meshes
bpy.ops.import_scene.gltf(filepath='F:/ADP-Export/terminal.glb')
MESH = {m.name: m for m in bpy.data.meshes}
for o in list(bpy.data.objects): bpy.data.objects.remove(o)


def mat(n, c, r=0.6, m=0.0, e=0.0, a=1.0, tr=0.0):
    M = bpy.data.materials.new(n); M.use_nodes = True; b = M.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*c, 1); b.inputs['Roughness'].default_value = r; b.inputs['Metallic'].default_value = m
    if e: b.inputs['Emission'].default_value = (*c, 1); b.inputs['Emission Strength'].default_value = e
    if tr: b.inputs['Transmission'].default_value = tr
    if a < 1.0: b.inputs['Alpha'].default_value = a; M.blend_method = 'BLEND'
    return M


GLASS = mat('X_glass', (0.75, 0.86, 0.9), 0.03, 0, 0, 0.22)
FBXCOL = {'terrazzo': (0.82, 0.8, 0.76, 0.3), 'concrete': (0.55, 0.55, 0.53, 0.85), 'basalt': (0.12, 0.12, 0.13, 0.6),
          'mullion': (0.14, 0.15, 0.17, 0.4), 'cedar': (0.42, 0.26, 0.15, 0.7), 'roofzinc': (0.62, 0.65, 0.68, 0.4),
          'black': (0.03, 0.03, 0.03, 0.5), 'plaster': (0.88, 0.88, 0.86, 0.8), 'oak': (0.6, 0.43, 0.26, 0.5),
          'dark': (0.1, 0.12, 0.15, 0.5), 'epoxy': (0.42, 0.45, 0.47, 0.35), 'white': (0.92, 0.92, 0.92, 0.4),
          'offwhite': (0.86, 0.84, 0.8, 0.5), 'steel': (0.6, 0.62, 0.64, 0.35), 'tile': (0.8, 0.82, 0.83, 0.25),
          'wood': (0.5, 0.33, 0.2, 0.45), 'marble': (0.9, 0.89, 0.86, 0.15), 'marbledark': (0.16, 0.15, 0.15, 0.15),
          'slate': (0.2, 0.21, 0.22, 0.6), 'walnut': (0.3, 0.19, 0.11, 0.45), 'stone': (0.7, 0.68, 0.63, 0.6)}


def fix_fbx_mats(objs, glass):
    for o in objs:
        for s in o.material_slots:
            if s.material is None: continue
            n = s.material.name.lower().split('.')[0].replace('mi_t_', '').replace('m_', '').replace('tf_', '')
            if glass or 'glass' in n: s.material = GLASS; continue
            if n.startswith('l_'):                                         # lights (l_RRGGBB)
                h = n[2:8]
                try: c = tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4))
                except Exception: c = (1, 1, 1)
                s.material = mat('X_' + n, c, 0.4, 0, 4.0); continue
            k = next((k for k in sorted(FBXCOL, key=len, reverse=True) if k in n), None)
            if k: c = FBXCOL[k]; s.material = mat('X_' + n, c[:3], c[3], 0.6 if k in ('steel', 'mullion') else 0.0)


ASSET = {}


def asset_mesh(name):
    if name in ASSET: return ASSET[name]
    p = os.path.join(ANNEX, name + '.fbx')
    if not os.path.exists(p): ASSET[name] = None; return None
    before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=p)
    new = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    fix_fbx_mats(new, name.endswith('_glass'))
    for o in new: o.data.transform(o.matrix_world)
    me = new[0].data if new else None
    if len(new) > 1:
        for o in new[1:]:
            pass
    for o in [o for o in bpy.data.objects if o not in before]: bpy.data.objects.remove(o)
    ASSET[name] = me; return me


L = json.load(open(os.path.join(ROOT, 'Data', 'terminal-layout.json')))
N = L['nodes']


def local(n):
    t = Matrix.Translation(n.get('translation', (0, 0, 0)))
    q = n.get('rotation'); r = Quaternion((q[3], q[0], q[1], q[2])).to_matrix().to_4x4() if q else Matrix()
    s = n.get('scale'); S = Matrix.Diagonal((*s, 1)) if s else Matrix()
    return t @ r @ S


count = 0


def walk(i, M, grp):
    global count
    n = N[i]; W = M @ local(n)
    g = grp or n['name']
    me = None
    if 'asset' in n: me = asset_mesh(n['asset'])
    elif 'mesh' in n: me = MESH.get('Mesh_%d' % n['mesh'])
    if me is not None:
        o = bpy.data.objects.new(n['name'], me); EX.objects.link(o); o.matrix_world = R @ C @ W @ CI
        o['grp'] = g; count += 1
    for c in n.get('children', []): walk(c, W, g if grp else (n['name'] if n['name'] != 'TERMINAL' else None))


for r0 in L['roots']: walk(r0, Matrix(), None)
print('TERMINAL objects', count)

# ---------------------------------------------------------------- the jetways (all 16 gates, the game's export)
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath='F:/ADP-Export/jetways.glb')
for o in [o for o in bpy.data.objects if o not in before]:
    for c in o.users_collection: c.objects.unlink(o)
    EX.objects.link(o); o['grp'] = 'JETWAY'
    if o.parent is None: o.matrix_world = R @ o.matrix_world

# ---------------------------------------------------------------- the A320 on every stand (one instance collection)
before = set(bpy.data.objects)
bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, 'Art', 'A320', 'A320-whole.glb'))
ac = [o for o in bpy.data.objects if o not in before]
A = bpy.data.collections.new('A320src')
for o in ac:
    for c in o.users_collection: c.objects.unlink(o)
    A.objects.link(o)
bpy.context.view_layer.update()
vs = [o.matrix_world @ v.co for o in ac if o.type == 'MESH' for v in list(o.data.vertices)[::7]]
mn = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
mx = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
long_y = (mx.y - mn.y) > (mx.x - mn.x)
tail = [v for v in vs if v.z > mn.z + 0.8 * (mx.z - mn.z)]
tc = sum(tail, Vector()) / len(tail)
if long_y: nose_sign = -1 if tc.y > (mn.y + mx.y) / 2 else 1; nose = Vector(((mn.x + mx.x) / 2, mx.y if nose_sign > 0 else mn.y, mn.z))
else: nose_sign = -1 if tc.x > (mn.x + mx.x) / 2 else 1; nose = Vector((mx.x if nose_sign > 0 else mn.x, (mn.y + mx.y) / 2, mn.z))
A.instance_offset = nose
fwd = Vector((0, nose_sign, 0)) if long_y else Vector((nose_sign, 0, 0))
yaw0 = math.atan2(fwd.y, fwd.x)
print('A320 bbox', mn, mx, 'nose', nose, 'fwd', fwd)
AF = json.load(open(os.path.join(ROOT, 'Data', 'airfield.json')))
for s in AF['stands']:
    if not (-260 < s['x'] < 560 and 80 < s['nose'] < 140): continue
    o = bpy.data.objects.new('A320@' + s['id'], None); o.instance_type = 'COLLECTION'; o.instance_collection = A
    PLANES.objects.link(o)
    d = s.get('dir', -1)                                  # nose points toward -z (the terminal) when dir = -1
    o.location = (s['x'] - OX, s['nose'], 0.0); o.rotation_euler = (0, 0, math.atan2(d, 0) - yaw0)

# ---------------------------------------------------------------- the landside (forecourt road, kerb, car park, coaches)
def P(x, z, h=0.0): return Vector((x - OX, z, h))


def box(x0, z0, x1, z1, h0, h1, m, name='B', col=None, new=False):
    me = bpy.data.meshes.new(name)
    v = [P(x, z, h) for h in (h0, h1) for (x, z) in ((x0, z0), (x1, z0), (x1, z1), (x0, z1))]
    me.from_pydata(v, [], [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)])
    o = bpy.data.objects.new(name, me); (col or (NEWC if new else EX)).objects.link(o); me.materials.append(m)
    if new: edge(o)
    return o


ASPH, PAVE, GRASSM = mat('X_asph', (0.07, 0.07, 0.075), 0.85), mat('X_pave', (0.55, 0.54, 0.51), 0.85), mat('X_grass', (0.16, 0.27, 0.1), 0.95)
WHITEP, YELP = mat('X_paint', (0.85, 0.85, 0.85), 0.6), mat('X_yel', (0.9, 0.7, 0.08), 0.6)
GOLD = mat('X_GOLD', (1.0, 0.62, 0.12), 0.4, 0, 9.0)


def edge(o, w=0.22):
    """the gold edge glow round a new part: a wireframe copy of its own box"""
    e = bpy.data.objects.new(o.name + '_edge', o.data.copy()); NEWC.objects.link(e); e.matrix_world = o.matrix_world
    e.data.materials.clear(); e.data.materials.append(GOLD)
    m = e.modifiers.new('wf', 'WIREFRAME'); m.thickness = w; m.use_replace = True; m.use_even_offset = False
    e.visible_shadow = False; e['edge'] = 1
    return e


box(-1200, -700, 1500, 700, -0.35, -0.12, GRASSM, 'Ground')
box(-265, 82, 570, 238, -0.12, -0.02, mat('X_apron', (0.5, 0.5, 0.48), 0.8), 'ApronFill')
box(-300, -52, 700, -25, -0.12, 0.0, ASPH, 'ForecourtRoad')
box(-120, -25, 560, -0.5, -0.12, 0.12, PAVE, 'Kerb')
for x in range(-300, 700, 9): box(x, -47, x + 4.5, -46.8, 0.0, 0.01, WHITEP, 'Lane')
box(-93, -247, 367, -54.5, -0.12, 0.0, ASPH, 'CarPark'); box(416, -247.3, 524, -234.8, -0.12, 0.0, ASPH, 'RentalLot')
random.seed(3)
CARC = [mat('X_car%d' % i, c, 0.25, 0.5) for i, c in enumerate(((0.8, 0.8, 0.82), (0.05, 0.05, 0.06), (0.5, 0.05, 0.05), (0.15, 0.2, 0.35), (0.45, 0.46, 0.48)))]
for row in range(10):
    zc = -62 - row * 18
    for x in range(-90, 365, 3):
        box(x, zc - 6, x + 0.1, zc - 1, 0.0, 0.01, WHITEP, 'Bay')
        if random.random() < 0.7:
            c = random.choice(CARC); b = box(x + 0.6, zc - 5.4, x + 2.4, zc - 1.0, 0.15, 0.95, c, 'Car')
            box(x + 0.75, zc - 4.2, x + 2.25, zc - 2.1, 0.95, 1.45, c, 'CarTop')
COACHM = mat('X_coach', (0.9, 0.9, 0.92), 0.3, 0.3)
for i in range(7):                                         # the 7 coach stands (13 m yellow boxes, 3 m gaps) with coaches
    x0 = -2 + i * 16
    for (a, b, c, d) in ((x0, -31, x0 + 13, -30.8), (x0, -27.4, x0 + 13, -27.2), (x0, -31, x0 + 0.2, -27.2), (x0 + 12.8, -31, x0 + 13, -27.2)):
        box(a, b, c, d, 0.0, 0.015, YELP, 'CoachStand')
    box(x0 + 0.5, -30.5, x0 + 12.5, -27.7, 0.3, 3.6, COACHM, 'Coach')
print('LANDSIDE done')

# ---------------------------------------------------------------- the NEW parts (accepted plan, to-do 213)
def under(x, x0=24.0): return 16.2 + 3.9 * math.cos(2 * math.pi * (x - (x0 + 51.0)) / 30.0)


ZINC = mat('N_zinc', (0.62, 0.65, 0.68), 0.38, 0.85); WOODU = mat('N_wood', (0.58, 0.4, 0.24), 0.55)
MUL = mat('N_mull', (0.14, 0.15, 0.17), 0.35, 0.7); WHITE = mat('N_white', (0.93, 0.93, 0.93), 0.35)
FLOOR = mat('N_marble', (0.86, 0.84, 0.8), 0.18); DESK = mat('N_desk', (0.92, 0.92, 0.94), 0.3)
DWOOD = mat('N_dwood', (0.25, 0.15, 0.09), 0.45); SIGNB = mat('N_sign', (0.08, 0.3, 0.65), 0.4, 0, 2.5)
SIGNG = mat('N_grn', (0.1, 0.65, 0.3), 0.4, 0, 2.5); SIGNR = mat('N_red', (0.75, 0.1, 0.1), 0.4, 0, 2.5)
SIGNY = mat('N_ysign', (0.95, 0.75, 0.1), 0.4, 0, 2.0); BELT = mat('N_belt', (0.1, 0.1, 0.11), 0.6, 0.3)
PLAST = mat('N_plaster', (0.88, 0.87, 0.84), 0.75); SHOPL = mat('N_shop', (1.0, 0.9, 0.75), 0.4, 0, 3.0)
TINT = mat('N_tint', (1.0, 0.7, 0.25), 0.5, 0, 0.0, 0.10)       # a faint gold veil over each new room volume


def mesh(name, V, F, m, new=True):
    me = bpy.data.meshes.new(name); me.from_pydata(V, [], F); me.update()
    o = bpy.data.objects.new(name, me); (NEWC if new else EX).objects.link(o); me.materials.append(m)
    for p in me.polygons: p.use_smooth = True
    return o


def label(txt, x, z, h):
    e = bpy.data.objects.new('LBL:' + txt, None); LBL.objects.link(e); e.location = P(x, z, h)


def wave_roof(xa, xb, z0=-16.0, z1=34.0, th=2.4, step=0.5):
    n = int((xb - xa) / step) + 1; V = []; Fu = []; Ft = []
    for i in range(n):
        x = xa + (xb - xa) * i / (n - 1); u = under(x)
        V += [P(x, z0, u), P(x, z1, u), P(x, z1, u + th), P(x, z0, u + th)]
    for i in range(n - 1):
        a, b = 4 * i, 4 * (i + 1)
        Fu.append((a, b, b + 1, a + 1)); Ft += [(a + 3, a + 2, b + 2, b + 3), (a, a + 3, b + 3, b), (a + 1, b + 1, b + 2, a + 2)]
    Ft += [(4 * (n - 1) + 3, 4 * (n - 1) + 2, 4 * (n - 1) + 1, 4 * (n - 1))]
    t = mesh('NewRoofTop', V, Ft, ZINC); u = mesh('NewRoofUnder', V, Fu, WOODU); t['roof'] = u['roof'] = 1
    # gold edge along the new roof's front lip and its start seam
    Ve = [P(xa + (xb - xa) * i / (n - 1), z0 - 0.05, under(xa + (xb - xa) * i / (n - 1)) + th + 0.05) for i in range(n)]
    for k in range(n - 1):
        a, b = Ve[k], Ve[k + 1]
        mesh('RoofLip', [a, b, b + Vector((0, 0, -0.5)), a + Vector((0, 0, -0.5))], [(0, 1, 2, 3)], GOLD)['roof'] = 1
    s = mesh('RoofSeam', [P(xa, z0, under(xa)), P(xa, z1, under(xa)), P(xa, z1, under(xa) + th + 0.4), P(xa, z0, under(xa) + th + 0.4)],
             [(0, 1, 2, 3)], GOLD); s['roof'] = 1


def glass_front(xa, xb, z, top=None, step=0.5, name='NewGlass'):
    n = int((xb - xa) / step) + 1; V = []; F = []
    for i in range(n):
        x = xa + (xb - xa) * i / (n - 1); t = under(x) if top is None else top
        V += [P(x, z, 0.0), P(x, z, t)]
    for i in range(n - 1): F.append((2 * i, 2 * i + 2, 2 * i + 3, 2 * i + 1))
    mesh(name, V, F, GLASS)
    x = xa
    while x <= xb + 0.01:
        t = under(x) if top is None else top
        o = box(x - 0.07, z - 0.14, x + 0.07, z + 0.14, 0, t, MUL, 'NewMullion', col=NEWC); x += 2.5
    for h in (5.4, 11.4): box(xa, z - 0.12, xb, z + 0.12, h, h + 0.12, MUL, 'NewTransom', col=NEWC)
    for k in range(n - 1):                                 # gold line along the foot and the head of the new glass
        a, b = V[2 * k], V[2 * k + 2]; c, d = V[2 * k + 1], V[2 * k + 3]
        mesh('GlassFoot', [a, b, b + Vector((0, 0, 0.25)), a + Vector((0, 0, 0.25))], [(0, 1, 2, 3)], GOLD)
        mesh('GlassHead', [c - Vector((0, 0, 0.3)), d - Vector((0, 0, 0.3)), d, c], [(0, 1, 2, 3)], GOLD)


def cyl(a, b, r, m, col=NEWC):
    a, b = Vector(a), Vector(b); d = b - a
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=d.length, location=(a + b) / 2, vertices=20)
    o = bpy.context.object
    for c in o.users_collection: c.objects.unlink(o)
    col.objects.link(o); o.rotation_euler = d.to_track_quat('Z', 'Y').to_euler(); o.data.materials.append(m); return o


def tree(x, z=-9.0):
    cyl(P(x, z, 0), P(x, z, 8), 0.45, WHITE); cyl(P(x, z, 0), P(x, z, 0.4), 0.6, GOLD)
    for dx, dz in ((-4.5, -3), (4.5, -3), (-4.5, 3), (4.5, 3)):
        cyl(P(x, z, 8), P(x + dx, z + dz, under(x + dx)), 0.2, WHITE)


def person(x, z, h=0.0):
    k = random.randrange(6)
    cyl(P(x, z, h), P(x, z, h + 0.9), 0.17, CARC[k % 5]); cyl(P(x, z, h + 0.9), P(x, z, h + 1.55), 0.23, CARC[(k + 2) % 5])
    cyl(P(x, z, h + 1.55), P(x, z, h + 1.8), 0.12, mat('N_skin', (0.75, 0.58, 0.45), 0.6) if 'N_skin' not in bpy.data.materials else bpy.data.materials['N_skin'])


# the roof, glass and tree pillars east to x 551
wave_roof(126.0, 551.2)
for i, tr in enumerate(range(150, 551, 60)): tree(tr + (-4 if i % 2 == 0 else 4))
glass_front(156, 551.2, 0.0)
label('Wave roof + glass front continued to x 551', 400, -16, 20.5)
label('Tree pillars', 330, -9, 8.5)
# the double-skin link gallery x 114..156
glass_front(114, 156, -4.0, top=6.0, name='LinkOuter'); glass_front(114, 156, -3.1, top=6.0, name='LinkInner')
box(114, -4.2, 156, 0, 6.0, 6.4, PLAST, 'LinkRoof', new=True); box(114, -4, 156, 0, 0, 0.06, FLOOR, 'LinkFloor', col=NEWC)
label('Double-skin link gallery x 114-156', 135, -4, 7.5)
# ---- Floor 1: the East Hall x 156..228
box(156, 0, 228, 40, 0, 0.05, FLOOR, 'EastHallFloor', col=NEWC); box(156, 0, 228, 40, 0.05, 11.5, TINT, 'EastHallVol', new=True)
box(156, 39.6, 228, 40, 0, 11.5, PLAST, 'EastHallBack', col=NEWC)
for i in range(8): box(162 + i * 3.4, 22, 165 + i * 3.4, 24, 0, 1.05, DESK, 'CheckDesk', col=NEWC)
box(162, 24.2, 189, 25, 0, 2.8, DWOOD, 'CheckBack', col=NEWC); box(162, 24.6, 189, 25, 3.0, 4.0, SIGNB, 'CheckSign', col=NEWC)
for i in range(4): box(192 + i * 3, 22, 194 + i * 3, 24, 0, 1.3, WHITE, 'BagDrop', col=NEWC)
box(158, 2, 172, 10, 0, 1.1, DWOOD, 'Cafe', col=NEWC); box(158, 9.6, 172, 10, 2.6, 3.4, SHOPL, 'CafeSign', col=NEWC)
for x in range(159, 172, 3): box(x, 11, x + 1.2, 12.2, 0, 0.75, DWOOD, 'CafeTable', col=NEWC)
for i in range(5): box(196 + i * 6, 1.5, 201 + i * 6, 3.0, 0, 1.1, DESK, 'Rental', col=NEWC); box(196 + i * 6, 1.5, 201 + i * 6, 1.8, 2.4, 3.2, SIGNY, 'RentalSign', col=NEWC)
for x0 in (176, 186): box(x0, 30, x0 + 8, 39.6, 0, 4.5, SHOPL, 'Shop', col=NEWC)
box(200, 12, 200.08, 30, 0.95, 1.05, WHITE, 'MeetRail', col=NEWC); box(200, 12, 226, 12.08, 0.95, 1.05, WHITE, 'MeetRail2', col=NEWC)
label('EAST HALL: check-in, cafe, shops, car rental, meeters rail', 192, 20, 6)
# ---- the east wing behind a landside promenade x 228..470
box(228, 0, 470, 6, 0, 0.05, FLOOR, 'Promenade', col=NEWC)
for x in range(232, 466, 14): box(x, 5.8, x + 11, 6.0, 0.3, 4.2, GLASS, 'ShopFront', col=NEWC); box(x, 5.9, x + 11, 6.1, 4.3, 5.0, SHOPL, 'ShopSign', col=NEWC)
box(228, 6, 236, 52, 0, 5.8, TINT, 'Customs', new=True); box(228.5, 10, 229, 16, 3.2, 4, SIGNG, 'Green'); box(228.5, 18, 229, 24, 3.2, 4, SIGNR, 'Red')
label('Customs green / red', 232, 20, 6.2)
box(236, 6, 312, 52, 0, 0.05, FLOOR, 'NSClaimFloor', col=NEWC); box(236, 6, 312, 52, 0.05, 5.8, TINT, 'NSClaim', new=True)
for xb in (250, 280):
    box(xb, 18, xb + 20, 22, 0, 0.6, BELT, 'Belt', col=NEWC); box(xb, 32, xb + 20, 36, 0, 0.6, BELT, 'Belt', col=NEWC)
    box(xb + 18, 22, xb + 20, 32, 0, 0.6, BELT, 'Belt', col=NEWC)
label('Non-Schengen claim hall, 2 wide-body belts', 274, 28, 6.5)
box(312, 6, 372, 52, 0, 0.05, FLOOR, 'PassInFloor', col=NEWC); box(312, 6, 372, 52, 0.05, 5.8, TINT, 'PassIn', new=True)
for i in range(6): box(316 + i * 4, 26, 318.5 + i * 4, 29, 0, 2.3, DWOOD, 'Booth', col=NEWC)
for i in range(8): box(342 + i * 3, 26, 342.3 + i * 3, 29, 0, 1.3, WHITE, 'EGate', col=NEWC)
label('Passport control IN: 6 booths, 8 e-gates', 342, 30, 6.5)
box(372, 6, 470, 52, 0, 5.8, TINT, 'Staff', new=True); box(372, 6, 470, 52, 5.8, 6.0, PLAST, 'StaffSlab', col=NEWC)
label('Staff + operations, staff security', 420, 30, 6.5)
box(470, 0, 551.2, 52, 0, 0.05, FLOOR, 'CoachFloor', col=NEWC); box(470, 0, 551.2, 52, 0.05, 11.5, TINT, 'CoachStation', new=True)
for x in range(480, 545, 8):
    for z in (12, 20, 28): box(x, z, x + 5, z + 0.6, 0, 0.45, DWOOD, 'Bench', col=NEWC)
box(500, 39.5, 530, 40, 4, 6, SIGNB, 'CoachBoard', col=NEWC)
for i in range(4): box(474 + i * 19, -24.5, 489 + i * 19, -21, 0.0, 0.02, YELP, 'BusBay', col=NEWC); box(475 + i * 19, -24, 488 + i * 19, -21.5, 0.3, 3.6, COACHM, 'Bus', col=NEWC)
label('Coach + bus station', 510, 20, 12.5)
# ---- the forecourt: taxi rank and parking pay machines
TAXI = mat('N_taxi', (0.95, 0.78, 0.1), 0.3, 0.3)
box(236, -24.5, 300, -21, 0.0, 0.02, YELP, 'TaxiRank', new=True)
for i in range(8): box(237 + i * 7.8, -24, 241.6 + i * 7.8, -22, 0.15, 1.5, TAXI, 'Taxi', col=NEWC)
box(232, -22, 232.3, -21.7, 0, 3.2, WHITE, 'TaxiPost', col=NEWC); box(231.4, -22.1, 233.2, -21.6, 2.4, 3.2, SIGNY, 'TaxiSign', col=NEWC)
label('Taxi rank', 268, -22, 3.5)
for i, x in enumerate((20, 120, 220, 320)): box(x, -56.5, x + 0.8, -55.8, 0, 1.7, mat('N_pay%d' % i, (0.1, 0.3, 0.6), 0.4, 0, 1.5), 'PayMachine', new=True)
label('Parking pay machines', 170, -56, 2.5)
# ---- Floor 2 (floor at +6)
F2 = 6.0
box(240, 52, 248, 68, F2, F2 + 6, TINT, 'PassWall', new=True)
gw = mesh('PassWallGlass', [P(240, 52, F2), P(240, 68, F2), P(240, 68, F2 + 6), P(240, 52, F2 + 6)], [(0, 1, 2, 3)], GLASS)
for i in range(3): box(242, 53 + i * 3, 245, 54.5 + i * 3, F2, F2 + 2.3, DWOOD, 'BoothOut', col=NEWC)
for i in range(4): box(242, 62 + i * 1.5, 245, 62.3 + i * 1.5, F2, F2 + 1.3, WHITE, 'EGateOut', col=NEWC)
label('Passport wall x 240-248 (out: 3 booths, 4 e-gates)', 244, 60, F2 + 7)
box(248, 24, 470, 48, F2 - 0.3, F2, PLAST, 'NSConcourseSlab', col=NEWC); box(248, 24, 470, 48, F2, F2 + 0.05, FLOOR, 'NSConcourseFloor', col=NEWC)
box(248, 24, 470, 48, F2 + 0.05, 11.5, TINT, 'NSConcourse', new=True)
box(252, 26, 300, 46, F2, F2 + 3.5, SHOPL, 'DutyFree', col=NEWC); label('Duty free', 276, 36, F2 + 4.5)
for x in range(306, 344, 4): box(x, 28, x + 1.4, 29.4, F2, F2 + 0.75, DWOOD, 'FoodTable', col=NEWC)
box(306, 44, 346, 47.5, F2, F2 + 3.2, SHOPL, 'FoodCourt', col=NEWC); label('Food court', 326, 36, F2 + 4.5)
box(352, 26, 392, 46, F2, F2 + 3.5, TINT, 'Lounge', new=True); box(352, 26, 392, 46, F2, F2 + 0.06, mat('N_carpet', (0.18, 0.22, 0.3), 0.9), 'LoungeCarpet', col=NEWC)
label('Lounge', 372, 36, F2 + 4.5)
box(396, 40, 410, 48, F2, F2 + 3.5, PLAST, 'Toilets', new=True); label('Toilets', 403, 44, F2 + 4.5)
for x in range(414, 466, 3):
    for z in (28, 32, 36): box(x, z, x + 2.2, z + 0.6, F2, F2 + 0.45, DWOOD, 'Seat', col=NEWC)
box(452, 42, 466, 45, F2, F2 + 1.1, DESK, 'TransferDesk', col=NEWC); label('Seating + transfer desk', 440, 34, F2 + 4.5)
label('NON-SCHENGEN CONCOURSE (Floor 2)', 360, 30, F2 + 7.5)
box(248, 48, 551, 52, F2, F2 + 0.05, FLOOR, 'ArrCorrFloor', col=NEWC)
mesh('ArrCorrGlassS', [P(248, 48, F2), P(551, 48, F2), P(551, 48, F2 + 3.2), P(248, 48, F2 + 3.2)], [(0, 1, 2, 3)], GLASS)
box(248, 48, 551, 52, F2 + 3.2, F2 + 3.4, PLAST, 'ArrCorrRoof', new=True)
label('Closed arrivals corridor, gates 13-16', 480, 50, F2 + 4.5)
box(312, 36, 354, 48, F2 - 0.3, F2 + 0.05, FLOOR, 'Mezzanine', new=True); label('Arrivals mezzanine down to passport IN', 333, 42, F2 + 1.5)
# people in the new halls
random.seed(11)
for (xa, xb, za, zb, n, h) in ((160, 226, 4, 34, 40, 0), (230, 468, 1, 5, 30, 0), (475, 548, 3, 36, 20, 0), (250, 466, 26, 46, 40, F2), (114, 156, -3, -0.5, 8, 0)):
    for _ in range(n): person(random.uniform(xa, xb), random.uniform(za, zb), h)
# lamps under the new roof
LAMP = mat('N_lamp', (1, 0.95, 0.85), 0.3, 0, 8.0)
for x in range(160, 551, 8):
    for z in (8, 22): box(x, z, x + 1.4, z + 0.4, 11.2, 11.3, LAMP, 'Lamp', col=NEWC)
print('NEW done')

# ---------------------------------------------------------------- light + render settings
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 3.5; sun.angle = math.radians(1.2)
so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so); so.rotation_euler = (math.radians(48), 0, math.radians(-145))
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
sky = w.node_tree.nodes.new('ShaderNodeTexSky'); sky.sky_type = 'NISHITA'; sky.sun_elevation = math.radians(38); sky.sun_rotation = math.radians(-55)
sky.sun_disc = False
w.node_tree.links.new(sky.outputs[0], w.node_tree.nodes['Background'].inputs[0]); w.node_tree.nodes['Background'].inputs[1].default_value = 0.35
cam = bpy.data.cameras.new('Cam'); cam.clip_end = 5000; co = bpy.data.objects.new('Cam', cam); sc.collection.objects.link(co); sc.camera = co
sc.render.engine = 'CYCLES'; sc.cycles.samples = 112; sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 2560, 1440; sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'
bpy.ops.wm.save_as_mainfile(filepath=OUTBLEND)
print('SAVED', OUTBLEND)
