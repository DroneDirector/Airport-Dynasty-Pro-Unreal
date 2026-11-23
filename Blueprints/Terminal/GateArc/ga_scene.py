# Gate arc, step 2: gate 9 with the curved window built into the real terminal (F:/ADP-Export/ga_base.blend), and the apron
# round it as Airfield.cpp paints it.  Collections: EXIST, OLD9 (gate 9's flat window, the 'before'), ARC (the new bay), ENV.
#   blender -b F:/ADP-Export/ga_base.blend --python ga_scene.py     -> F:/ADP-Export/ga_scene.blend
import bpy, sys, os, math
from mathutils import Vector
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..')); sys.path.insert(0, HERE)
from ga_geom import *
sc = bpy.context.scene; OX = 200.0


def coll(n):
    c = bpy.data.collections.new(n); sc.collection.children.link(c); return c


EX = bpy.data.collections['EXIST']; OLD, ARC, ENV = coll('OLD9'), coll('ARC'), coll('ENV')


def P(x, z, h=0.0): return Vector((x - OX, z, h))          # the scene's frame: X = plan x - 200


def bnd(o):
    b = [o.matrix_world @ Vector(c) for c in o.bound_box]
    return (min(v.x for v in b) + OX, max(v.x for v in b) + OX, min(v.y for v in b), max(v.y for v in b), min(v.z for v in b), max(v.z for v in b))


def mat(n, c, r=0.6, m=0.0, e=0.0):
    M = bpy.data.materials.new('GA_' + n); M.use_nodes = True; b = M.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*c, 1); b.inputs['Roughness'].default_value = r; b.inputs['Metallic'].default_value = m
    if e: b.inputs['Emission'].default_value = (*c, 1); b.inputs['Emission Strength'].default_value = e
    return M


def get(n, alt):
    for m in bpy.data.materials:
        if m.name.split('.')[0] == n: return m
    return alt


WHITE = get('white', mat('white', (0.9, 0.9, 0.9), 0.35)); MUL = get('mullion', mat('mul', (0.12, 0.13, 0.15), 0.4, 0.6)); CARP = get('carpetBlue', mat('carp', (0.05, 0.1, 0.25), 0.9))
OFFW = get('offwhite', mat('offw', (0.85, 0.83, 0.8), 0.6)); CONC = get('concrete', mat('conc', (0.5, 0.5, 0.5), 0.8)); STEEL = mat('steel', (0.36, 0.38, 0.40), 0.42, 0.75)
DARK = mat('epdm', (0.015, 0.015, 0.017), 0.75); GLOW = mat('lamp', (1, 0.95, 0.87), 0.4, 0, 6.0); PLASTER = get('plaster', WHITE)
# one architectural glass for every window in the picture (terminal and jetways): clear, a faint green, real reflections
GLASS = bpy.data.materials.new('GA_glass'); GLASS.use_nodes = True; nt = GLASS.node_tree; nt.nodes.clear()
out = nt.nodes.new('ShaderNodeOutputMaterial'); mix = nt.nodes.new('ShaderNodeMixShader'); tr = nt.nodes.new('ShaderNodeBsdfTransparent'); gl = nt.nodes.new('ShaderNodeBsdfGlossy'); fr = nt.nodes.new('ShaderNodeFresnel')
tr.inputs[0].default_value = (0.82, 0.90, 0.88, 1); gl.inputs['Roughness'].default_value = 0.0; gl.inputs[0].default_value = (0.95, 1, 1, 1); geo = nt.nodes.new('ShaderNodeNewGeometry'); mi = nt.nodes.new('ShaderNodeMath'); mi.operation = 'MULTIPLY_ADD'      # (the same on both faces of the pane)
mi.inputs[1].default_value = 1 / 1.6 - 1.6; mi.inputs[2].default_value = 1.6; nt.links.new(geo.outputs['Backfacing'], mi.inputs[0]); nt.links.new(mi.outputs[0], fr.inputs['IOR'])
nt.links.new(fr.outputs[0], mix.inputs[0]); nt.links.new(tr.outputs[0], mix.inputs[1]); nt.links.new(gl.outputs[0], mix.inputs[2]); nt.links.new(mix.outputs[0], out.inputs[0])
for me in bpy.data.meshes:
    for i, s in enumerate(me.materials):
        if s and s.name.split('.')[0] in ('glassOuter', 'glass', 'X_glass'): me.materials[i] = GLASS
for me in bpy.data.meshes:                                      # the ceiling lamps
    for i, s in enumerate(me.materials):
        if s and s.name.split('.')[0] == 'glow#24': me.materials[i] = GLOW


def mesh(name, V, F, m, col=ARC, smooth=False):
    me = bpy.data.meshes.new(name); me.from_pydata([tuple(P(*v)) for v in V], [], F); me.update()
    me.materials.append(m); o = bpy.data.objects.new(name, me); col.objects.link(o)
    if smooth:
        for p in me.polygons: p.use_smooth = True
        me.use_auto_smooth = True; me.auto_smooth_angle = math.radians(30)
    return o


def box(x0, z0, x1, z1, h0, h1, m, col=ARC, name='box'):
    V = [(x, z, h) for h in (h0, h1) for (x, z) in ((x0, z0), (x1, z0), (x1, z1), (x0, z1))]
    return mesh(name, V, [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], m, col)


def ring(t0, t1, o0, o1, h0, h1, m, name='ring', step=0.2, col=ARC):
    """a curved bar: from t0 to t1 along the arc, between the offsets o0 and o1, the heights h0 and h1 - a true curve, smooth"""
    n = max(2, int(abs(t1 - t0) / step) + 1); V = []; F = []
    for i in range(n + 1):
        t = t0 + (t1 - t0) * i / n; a, b = A.pt(t, o0), A.pt(t, o1)
        V += [(*a, h0), (*b, h0), (*b, h1), (*a, h1)]
    for i in range(n):
        p, q = 4 * i, 4 * (i + 1)
        for k in range(4): F.append((p + k, p + (k + 1) % 4, q + (k + 1) % 4, q + k))
    F += [(0, 3, 2, 1), (4 * n, 4 * n + 1, 4 * n + 2, 4 * n + 3)]
    return mesh(name, V, F, m, col, True)


def plate(h, off, m, name, z0=ZE, slot=None, up=True, col=ARC):
    """a flat segment between the old straight edge (z0) and the arc (offset off), with a slot (x0, x1) left out"""
    xs = [A.pt(-A.L / 2 + A.L * i / 180, off)[0] for i in range(181)] + (list(slot) if slot else [])
    xs = sorted(x for x in xs if A.z(x, off) >= z0); V = []; F = []
    for x in xs: V += [(x, z0, h), (x, A.z(x, off), h)]
    for i in range(len(xs) - 1):
        if slot and xs[i] >= slot[0] - 1e-6 and xs[i + 1] <= slot[1] + 1e-6: continue
        a, b = 2 * i, 2 * i + 2; F.append((a, b, b + 1, a + 1) if up else (a, a + 1, b + 1, b))
    return mesh(name, V, F, m, col)


# ---------------------------------------------------------------- the smooth wave roof (Tools/wave_roof.py) instead of the old copies set end to end
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import wave_roof as WR
for o in list(EX.objects):
    if o.type == 'MESH' and str(o.get('grp') or '').startswith('ROOF'): EX.objects.unlink(o)
TIMB, WGL = get('timber', mat('timber', (0.45, 0.2, 0.07), 0.5)), get('whiteGloss', WHITE)
NXR, NZR = int((WR.X1 - WR.X0) / 0.75), 34; VB, VT, FR = [], [], []
for i in range(NXR + 1):
    x = WR.X0 + (WR.X1 - WR.X0) * i / NXR; x = min(max(x, WR.X0 + 0.01), WR.X1 - 0.01); s0 = WR.south(x)
    for k in range(NZR + 1): z = s0 + (WR.Z1 - s0) * k / NZR; h = WR.height(x, z); VB.append((x, z, h)); VT.append((x, z, h + WR.THICK))
w_ = NZR + 1
for i in range(NXR):
    for k in range(NZR): a = i * w_ + k; FR.append((a, a + w_, a + w_ + 1, a + 1))
mesh('WaveTop', VT, FR, WGL, EX, True); mesh('WaveSoffit', VB, [f[::-1] for f in FR], TIMB, EX, True)
for k in (0, NZR): mesh('WaveEdge', [v for i in range(NXR + 1) for v in (VB[i * w_ + k], VT[i * w_ + k])], [(2 * i, 2 * i + 2, 2 * i + 3, 2 * i + 1) for i in range(NXR)], WGL, EX, True)
for i in (0, NXR): mesh('WaveEnd', [v for k in range(w_) for v in (VB[i * w_ + k], VT[i * w_ + k])], [(2 * k, 2 * k + 2, 2 * k + 3, 2 * k + 1) for k in range(NZR)], WGL, EX, True)
for x, h, z in WR.downlights():
    if x < 80 and z > 6: mesh('DL', [(x + 0.18 * math.cos(q * math.pi / 5), z + 0.18 * math.sin(q * math.pi / 5), h) for q in range(10)], [tuple(range(10))], GLOW, EX)
# ---------------------------------------------------------------- what the bay replaces: the flat window of gate 9
gone = []
for o in list(EX.objects):
    if o.type != 'MESH': continue
    n = o.name; x0, x1, z0, z1, h0, h1 = bnd(o)
    if n.startswith('w#') and h0 > 5.9 and x0 > -234 and x1 < -199.6 and z0 > 81.8: gone.append(o)
    if n.startswith(('F2-GATE-9/STRUCTURE-mullion', 'F2-GATE-9/STRUCTURE-plaster')): gone.append(o)
for o in gone: EX.objects.unlink(o); OLD.objects.link(o)
print('GONE', [o.name for o in gone])
box(-233.67, 68, -233.55, 82, 6, 11.4, PLASTER); box(-199.8, 68, -199.6, 82, 6, 11.4, PLASTER)      # its side walls stay

# ---------------------------------------------------------------- the bay
SLOT = (JCL - 0.045, JCR + 0.045); E1 = 0.09
plate(F2 + 0.008, 0.0, CARP, 'ArcCarpet', slot=SLOT); plate(F2 - 0.01, E1, CONC, 'ArcSlabTop', slot=SLOT); plate(5.40, E1, WHITE, 'ArcSoffit', slot=SLOT, up=False, z0=82.07)
plate(CEIL, 0.0, OFFW, 'ArcCeiling', up=False); plate(GTOP, E1, WHITE, 'ArcRoofUnder', up=False); plate(ROOF, E1, WHITE, 'ArcRoof')
TS0, TS1 = A.t(SLOT[0]), A.t(SLOT[1])
for a, b in ((-A.L / 2, TS0), (TS1, A.L / 2)): ring(a, b, 0.0, E1, 5.40, F2 - 0.004, WHITE, 'ArcSlabBand')
ring(-A.L / 2, A.L / 2, -0.05, E1, GTOP - 0.002, ROOF + 0.004, WHITE, 'ArcRoofBand')
for x in SLOT: box(x - 0.02, 82.06, x + 0.02, A.z(x) + E1 - 0.01, 5.40, F2 - 0.004, CONC)          # the slot's sides
# the mullions: the same 80 x 140 profile, radial, 20 of them; the two over the corridor stop on the portal's head
M = A.mull()
for i, t in enumerate(M): ring(t - 0.04, t + 0.04, -0.07, 0.07, PORT_HEAD[1] - 0.005 if i in (13, 14) else F2, GTOP, MUL, 'Mull%d' % i, step=1)
g, lo = 0.04, F2 + 0.06                                              # the glass: truly curved, one pane between mullions
for i in range(NBAY):
    a, b = M[i] + g, M[i + 1] - g
    if i in (12, 13, 14): ring(a, b, -0.015, 0.015, PORT_HEAD[1], GTOP - 0.04, GLASS, 'PaneHi%d' % i, step=0.12)
    else: ring(a, b, -0.015, 0.015, lo, GTOP - 0.04, GLASS, 'Pane%d' % i, step=0.12)
    if i == 14: ring(TJR + 0.075, b, -0.015, 0.015, lo, PORT_HEAD[1], GLASS, 'Pane14lo', step=0.12)
for a, b in ((-A.L / 2, TJL), (TJR, A.L / 2)): ring(a, b, -0.05, 0.05, F2, F2 + 0.06, MUL, 'Sill')
ring(-A.L / 2, A.L / 2, -0.05, 0.05, GTOP - 0.04, GTOP, MUL, 'Head')
for x in (X0, X1): box(x - 0.07, ZG - 0.09, x + 0.07, ZG + 0.085, F2, GTOP, MUL)                   # the ends: two posts
box(X1 + 0.07, ZG - 0.015, XR - 0.04, ZG + 0.015, F2, GTOP, GLASS); box(XR - 0.04, 81.84, XR + 0.04, 81.98, F2, GTOP, MUL)
# the portal: a 150 mm steel collar, the face of his own gate trim, set in the curve; a rubber joint to the corridor
for t in (TJL, TJR): ring(t - 0.075, t + 0.075, -0.16, 0.16, 5.40, PORT_HEAD[0], STEEL, 'Jamb', step=1)
ring(TJL - 0.075, TJR + 0.075, -0.16, 0.16, PORT_HEAD[0], PORT_HEAD[1], STEEL, 'PortalHead')
for xa, xb in ((-213.45, JCL), (JCR, -209.95)):
    zc = A.z((xa + xb) / 2); box(xa, zc - 0.10, xb, zc + 0.10, 5.2, 9.10, DARK)
ring(A.t(JCL) - 0.02, A.t(JCR) + 0.02, -0.08, 0.08, 8.95, 9.11, DARK, 'JointTop'); ring(A.t(JCL), A.t(JCR), -0.06, 0.06, 5.36, 5.50, DARK, 'JointBot')
mesh('SlotSoffit', [(SLOT[0], 82.08, 5.42), (SLOT[1], 82.08, 5.42), (SLOT[1], A.z(SLOT[1]), 5.42), (SLOT[0], A.z(SLOT[0]), 5.42)], [(0, 1, 2, 3)], DARK)
# the door wall at z 82: the end of the corridor, now standing free in the bay (the door untouched)
for x in (-212.6, -210.8): box(x - 0.04, 81.84, x + 0.04, 81.98, F2, 9.10, MUL)
for a, b in ((JCL, -212.64), (-210.76, JCR)): box(a, ZG - 0.015, b, ZG + 0.015, F2, 9.10, GLASS)
box(-212.6, 81.85, -210.8, 81.97, 8.39, 9.10, PLASTER); box(JCL - 0.14, 81.84, JCR + 0.14, 81.985, 9.10, 9.245, STEEL)
for a, b in ((JCL - 0.14, JCL), (JCR, JCR + 0.14)): box(a, 81.84, b, 81.985, F2, 9.10, STEEL)
for k in range(9):                                                   # lamps along the curve (the gate's own lamp size, 1.2 x 0.6)
    t = -12.8 + 3.2 * k
    if abs(A.pt(t, -1.35)[0] - JX) < 2.2 or A.z(A.pt(t)[0]) - 82 < 1.8: continue
    ring(t - 0.6, t + 0.6, -1.65, -1.05, CEIL - 0.045, CEIL - 0.005, GLOW, 'Lamp')
# ---------------------------------------------------------------- the gate's seats (his GateSeat, as the game swaps them in)
before = set(bpy.data.objects); bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, 'Art', 'GateSeat', 'gateseat.glb')); bpy.context.view_layer.update()
SEAT = {}
for o in [o for o in bpy.data.objects if o not in before]:
    if o.type == 'MESH': o.data.transform(o.matrix_world); SEAT[o.name.split('.')[0]] = o.data
    bpy.data.objects.remove(o)
NAVY = mat('navy', (0.02, 0.045, 0.12), 0.5); MCH = mat('mchrome', (0.55, 0.56, 0.58), 0.35, 0.9)
for me in SEAT.values():
    print('SEAT', me.name, [m.name for m in me.materials], [tuple(round(min(v.co[i] for v in me.vertices), 2) for i in range(3)), tuple(round(max(v.co[i] for v in me.vertices), 2) for i in range(3))])
    for i, s in enumerate(me.materials): me.materials[i] = NAVY if 'shell' in s.name.lower() else DARK if 'black' in s.name.lower() else MCH


def seat(x, z, yaw, col, leg=False):
    for k in ('SeatModule', 'ArmModule') + (('LegModule',) if leg else ()):
        if k in SEAT: o = bpy.data.objects.new(k, SEAT[k]); col.objects.link(o); o.location = P(x, z, F2); o.rotation_euler = (0, 0, yaw)


for o in [o for o in EX.objects if o.type == 'MESH' and o.data.materials and o.data.materials[0] and o.data.materials[0].name.split('.')[0] in ('navy', 'steel')]:
    x0, x1, z0, z1, h0, h1 = bnd(o)
    if -235 < x0 and x1 < -165 and 70 < z0 and z1 < 79 and h0 > 5.9 and h1 < 7.2: EX.objects.unlink(o)
YAW0 = math.pi                                                      # (today's rows: backs towards the walkway, facing the apron)
for z in (71.4, 74.4, 77.4):
    for k in range(37):
        for x00 in (-230.61, -196.61):
            x = x00 + 0.62 * k
            if x00 > -200 and x > -168: continue
            seat(x, z, YAW0, EX, k % 3 == 0)
TSEAT = []; t = A.t(-230.4); end = TJL - 2.2                          # the new row along the curve, facing out
while t < end: TSEAT.append(t); t += 0.62 * A.R / (A.R - 1.75)
for k, t in enumerate(TSEAT):
    x, z = A.pt(t, -1.75); seat(x, z, YAW0 - t / A.R, ARC, k % 3 == 0)
print('NEW SEATS', len(TSEAT), A.pt(TSEAT[0], -1.75), A.pt(TSEAT[-1], -1.75))


# ---------------------------------------------------------------- the apron as Airfield.cpp paints it
def tex(n, sub, scale):
    M = bpy.data.materials.new('GA_' + n); M.use_nodes = True; nt = M.node_tree; b = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / scale,) * 3; nt.links.new(tc.outputs['Object'], mp.inputs[0])

    def img(f, cs):
        q = nt.nodes.new('ShaderNodeTexImage'); q.image = bpy.data.images.load(os.path.join(ROOT, 'Art', 'Ground', f)); q.image.colorspace_settings.name = cs; nt.links.new(mp.outputs[0], q.inputs[0]); return q
    nt.links.new(img(sub + '_albedo.png', 'sRGB').outputs[0], b.inputs['Base Color']); nt.links.new(img(sub + '_roughness.png', 'Non-Color').outputs[0], b.inputs['Roughness'])
    k = nt.nodes.new('ShaderNodeNormalMap'); k.inputs[0].default_value = 0.22; nt.links.new(img(sub + '_normal.png', 'Non-Color').outputs[0], k.inputs[1]); nt.links.new(k.outputs[0], b.inputs['Normal'])
    return M


CONCT = tex('apron', 'concrete', 7.0)                                # (a large, soft variation over the tiling)
nt = CONCT.node_tree; b_ = nt.nodes['Principled BSDF']; alb = b_.inputs['Base Color'].links[0].from_socket; no = nt.nodes.new('ShaderNodeTexNoise'); no.inputs['Scale'].default_value = 0.06; no.inputs['Detail'].default_value = 6
tc = nt.nodes.new('ShaderNodeTexCoord'); nt.links.new(tc.outputs['Object'], no.inputs['Vector']); ra = nt.nodes.new('ShaderNodeValToRGB'); ra.color_ramp.elements[0].color = (0.62, 0.62, 0.62, 1); ra.color_ramp.elements[0].position = 0.3; ra.color_ramp.elements[1].position = 0.7; nt.links.new(no.outputs['Fac'], ra.inputs[0])
mx = nt.nodes.new('ShaderNodeMixRGB'); mx.blend_type = 'MULTIPLY'; mx.inputs[0].default_value = 1; nt.links.new(alb, mx.inputs[1]); nt.links.new(ra.outputs[0], mx.inputs[2]); nt.links.new(mx.outputs[0], b_.inputs['Base Color'])
YEL, RED, WHT, TAR = mat('py', (0.85, 0.56, 0.03), 0.6), mat('pr', (0.55, 0.03, 0.03), 0.6), mat('pw', (0.8, 0.8, 0.8), 0.6), mat('tar', (0.09, 0.09, 0.09), 0.8)
box(-1500, -900, 1900, 1500, -0.6, -0.10, mat('grass', (0.07, 0.105, 0.04), 0.95), ENV, 'Grass'); box(-265, 82, 570, 238, -0.3, -0.02, CONCT, ENV, 'Apron'); box(-241, 0, 551, 82, -0.3, -0.03, CONCT, ENV)
box(-1500, 378, 1900, 394, -0.3, -0.05, mat('twy', (0.1, 0.1, 0.1), 0.85), ENV)
for x in range(-265, 100, 5): box(x - 0.015, 82.1, x + 0.015, 238, -0.02, -0.0185, TAR, ENV)
for z in range(87, 238, 5): box(-265, z - 0.015, 100, z + 0.015, -0.02, -0.0185, TAR, ENV)


def line(m, a, b, w, dh=0.0):
    a, b = Vector(a), Vector(b); d = (b - a).normalized(); n = Vector((-d.y, d.x)) * (w / 2); H = -0.012 + dh
    mesh('paint', [(*(a + n), H), (*(a - n), H), (*(b - n), H), (*(b + n), H)], [(0, 1, 2, 3)], m, ENV)


def rect(m, x0, z0, x1, z1, w):
    for a, b in (((x0, z0), (x1, z0)), ((x1, z0), (x1, z1)), ((x1, z1), (x0, z1)), ((x0, z1), (x0, z0))): line(m, a, b, w)


for sid, x, nose, jw in ((9, -225.42, 120, 1), (10, -184.56, 122, 0), (11, -143.71, 120, 1), (12, -102.85, 122, 0)):
    line(YEL, (x, 218), (x, nose), 0.2)
    for ng, hw in ((3.6, 1.5), (4.4, 1.5), (5.1, 3.0)): line(YEL, (x - hw, nose + ng), (x + hw, nose + ng), 0.3)
    L, Rr = x - 22.5 if sid == 9 else x - 20.43, x + 20.43
    for q in ((L, Rr) if sid == 9 else (Rr,)): line(RED, (q, 82.6), (q, nose + 47.5), 0.2)
    line(RED, (L, nose + 47.5), (Rr, nose + 47.5), 0.2)
    if jw:
        rect(WHT, x - 15, 87.0, x - 11, 99.0, 0.15); rect(WHT, x - 9.5, nose - 3.3, x - 5.2, nose - 0.7, 0.12)
        a0, a1, b0, b1 = x + 3, x + 16, 82.6, nose + 9; rect(RED, a0, b0, a1, b1, 0.15); c = b0 - (a1 - a0)
        while c < b1:
            p, q = Vector((a0, c)), Vector((a1, c + a1 - a0))
            if p.y < b0: p = Vector((p.x + b0 - p.y, b0))
            if q.y > b1: q = Vector((q.x - (q.y - b1), b1))
            if q.x - p.x > 0.3: line(RED, p, q, 0.12, -0.001)
            c += 1.8
for z in (168.5, 176.5): line(WHT, (-250, z), (60, z), 0.15)
for k in range(52): line(WHT, (-250 + 6 * k, 172.5), (-247 + 6 * k, 172.5), 0.12)


def glb(path, name, x, z, yaw, small=256):
    before = set(bpy.data.objects); ib = set(bpy.data.images); bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    for i in bpy.data.images:
        if i not in ib and i.size[0] > small: i.scale(small, small); i.pack()
    e = bpy.data.objects.new(name, None); ENV.objects.link(e)
    for o in new:
        for c in list(o.users_collection): c.objects.unlink(o)
        ENV.objects.link(o)
        if o.parent is None: o.parent = e
    e.location = P(x, z, -0.02); e.rotation_euler = (0, 0, yaw); return e


glb(os.path.join(ROOT, 'Art', 'PushbackTug', 'pushbacktug.glb'), 'TUG', -225.42 - 13, 91, math.pi / 2)
glb(os.path.join(ROOT, 'Art', 'GPU', 'gpu.glb'), 'GPU', -225.42 - 7.4, 118, 0)
# ---------------------------------------------------------------- the aircraft: A320.glb is stored mirrored (the game's loader mirrors it back: Art/A320/make_a320.py)
for o in bpy.data.objects:
    if o.name.startswith('A320@'): o.scale.x = -o.scale.x
# the number signs on the cabs (the export carries J-1's picture on every one; the game sets them): each its own gate's number
bpy.context.view_layer.update(); SB = mat('signblue', (0.02, 0.05, 0.30), 0.4); SW = mat('signwhite', (0.95, 0.95, 0.95), 0.5, 0, 0.6)
for o in list(EX.objects):
    if o.type == 'MESH' and o.data.materials and o.data.materials[0] and '/GATE-NUMBER' in o.data.materials[0].name and o.name.startswith('jwp'):
        n = o.data.materials[0].name.split('/')[0].replace('J-', ''); x0, x1, z0, z1, h0, h1 = bnd(o)
        pr = o.parent.name if o.parent else ''; sib = [c for c in bpy.data.objects if c.parent == o.parent and 'jwSignBlue' in c.name]
        if not sib: continue
        num = sib[0].name.split('/')[0].replace('J-', ''); mid = (bnd(sib[0])[0] + bnd(sib[0])[1]) / 2; s_ = 1 if x0 > mid else -1
        EX.objects.unlink(o); c = bpy.data.curves.new('num', 'FONT'); c.body = num; c.size = 0.75; c.align_x = 'CENTER'; c.align_y = 'CENTER'; c.extrude = 0.003; c.offset = 0.012; c.materials.append(SW)
        t = bpy.data.objects.new('num', c); EX.objects.link(t); t.location = P(x0 + s_ * 0.004, (z0 + z1) / 2, (h0 + h1) / 2); t.rotation_euler = (math.pi / 2, 0, s_ * math.pi / 2); print('SIGN', num, x0)
# ---------------------------------------------------------------- light
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 3.2; sun.color = (1, 0.93, 0.83); sun.angle = math.radians(0.6); so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so)
so.rotation_euler = Vector((0.62, -0.66, -0.52)).to_track_quat('-Z', 'Y').to_euler()      # afternoon, from the north-west (the apron side is the sunny one)
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; nt = w.node_tree; sky = nt.nodes.new('ShaderNodeTexSky'); sky.sky_type = 'NISHITA'; sky.sun_disc = False
sky.sun_elevation = math.radians(34); sky.sun_rotation = math.radians(-43); sky.dust_density = 0.6; nt.links.new(sky.outputs[0], nt.nodes['Background'].inputs[0]); nt.nodes['Background'].inputs[1].default_value = 0.32
cam = bpy.data.cameras.new('Cam'); cam.clip_start = 0.05; cam.clip_end = 6000; co = bpy.data.objects.new('Cam', cam); sc.collection.objects.link(co); sc.camera = co
sc.render.engine = 'CYCLES'; sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'
sc.cycles.max_bounces = 8; sc.cycles.transparent_max_bounces = 48; sc.cycles.caustics_reflective = sc.cycles.caustics_refractive = False
bpy.ops.wm.save_as_mainfile(filepath='F:/ADP-Export/ga_scene.blend', compress=False); print('SAVED')
