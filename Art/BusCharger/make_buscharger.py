# 277 E3a: the free-standing 150 kW DC fast charger at the vehicle area's bus bays. x = its front (towards the bus), y left, z up,
# ground at 0. Cabinet 0.50 x 0.80 x 1.87 on a plinth, two impact posts, one CCS2 cable on its right side (-y) over a saddle.
#   blender -b --python Art/BusCharger/make_buscharger.py      (or Tools/import_model.py)
import bpy, math, os, sys, glob
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))); sys.path.insert(0, os.path.join(ROOT, 'Tools'))
from mathutils import Matrix, Vector
import vehicle_kit as K, ccs2
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, ik
HERE = os.path.dirname(os.path.abspath(__file__)); GPU = os.path.join(ROOT, 'Art', 'GPU', 'textures')
K.reset()


def mat(name, col, metal=0.0, rough=0.5, tex=None, tile=1.0, nrm=0.5, photo=False, where=GPU, pic=None, emit=0.0):
    """A material: a plain colour (or the photo's) with the texture's roughness and relief; pic: a picture that glows."""
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough

    def img(f, data):
        n = nt.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(f, check_existing=True)
        if data: n.image.colorspace_settings.name = 'Non-Color'
        return n
    if pic:
        n = img(pic, False); nt.links.new(n.outputs[0], b.inputs['Base Color']); 
        if emit: nt.links.new(n.outputs[0], b.inputs['Emission']); b.inputs['Emission Strength'].default_value = emit
    elif emit: b.inputs['Emission'].default_value = (*col, 1); b.inputs['Emission Strength'].default_value = emit
    if tex:
        tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / tile, 1 / tile, 1); nt.links.new(tc.outputs['UV'], mp.inputs['Vector'])
        for suf in ('Color', 'Roughness', 'NormalGL'):
            if suf == 'Color' and not photo: continue
            f = glob.glob(os.path.join(where, tex, tex + '_*_%s.jpg' % suf)) + glob.glob(os.path.join(where, tex + '_*_%s.jpg' % suf))
            assert f, (tex, suf)
            n = img(f[0], suf != 'Color'); nt.links.new(mp.outputs[0], n.inputs[0])
            if suf == 'Color': nt.links.new(n.outputs[0], b.inputs['Base Color'])
            elif suf == 'Roughness': nt.links.new(n.outputs[0], b.inputs['Roughness'])
            else:
                q = nt.nodes.new('ShaderNodeNormalMap'); q.inputs['Strength'].default_value = nrm; nt.links.new(n.outputs[0], q.inputs['Color']); nt.links.new(q.outputs[0], b.inputs['Normal'])
    return m


M_CONC = mat('BC_Concrete', (0.5, 0.5, 0.5), tex='Concrete034', tile=1.2, nrm=0.9, photo=True, where=os.path.join(HERE, 'textures'))
M_WHITE = mat('BC_PaintLight', (0.70, 0.72, 0.73), tex='Paint004', tile=0.6, nrm=0.2)
M_DARK = mat('BC_PaintDark', (0.045, 0.050, 0.056), tex='Paint004', tile=0.5, nrm=0.25)
M_YEL = mat('BC_PaintYellow', (0.83, 0.50, 0.015), tex='Paint004', tile=0.5, nrm=0.3)
M_BLK = mat('BC_BandBlack', (0.02, 0.02, 0.022), tex='Paint004', tile=0.5, nrm=0.3)
M_STEEL = mat('BC_Steel', (0.56, 0.57, 0.58), 1.0, tex='Metal027', tile=0.4, nrm=0.3)
M_PLA = mat('BC_PlasticBlack', (0.022, 0.023, 0.025), tex='Plastic013A', tile=0.25, nrm=0.6)
M_INS = mat('BC_PlasticGrey', (0.42, 0.43, 0.44), tex='Plastic013A', tile=0.25, nrm=0.4)
M_RUB = mat('BC_Cable', (0.018, 0.018, 0.019), tex='Rubber004', tile=0.3, nrm=0.5)
M_RED = mat('BC_StopRed', (0.62, 0.012, 0.012), 0.0, 0.35)
M_LAMP = mat('BC_LampGreen', (0.05, 0.85, 0.22), 0.0, 0.3, emit=2.0)
M_SCR = mat('BC_Screen', (0, 0, 0), 0.0, 0.15, pic=os.path.join(HERE, 'textures', 'screen.png'), emit=1.0)
M_SIGN = mat('BC_Sign', (1, 1, 1), 0.0, 0.4, pic=os.path.join(HERE, 'textures', 'sign.png'))
M_GLASS = mat('BC_Bezel', (0.008, 0.009, 0.010), 0.0, 0.12)

# ---- plinth and anchor bolts
plinth = [box('Plinth', (0, 0, 0.07), (0.75, 1.00, 0.16), M_CONC, bevel=0.015, segs=3)]          # (1 cm in the ground)
for sx in (1, -1):
    for sy in (1, -1):
        plinth.append(cyl('Bolt%d%d' % (sx, sy), (sx * 0.275, sy * 0.33, 0.158), 0.012, 0.020, 'z', M_STEEL, 6, bevel=0.002))
        plinth.append(box('Foot%d%d' % (sx, sy), (sx * 0.262, sy * 0.33, 0.1485), (0.085, 0.07, 0.009), M_DARK, bevel=0.002))
# ---- the cabinet
base = box('Base', (0, 0, 0.1885), (0.47, 0.77, 0.085), M_DARK, bevel=0.004)
body = box('Cabinet', (0, 0, 1.10), (0.50, 0.80, 1.74), M_WHITE, bevel=0.010, segs=3)
roof = prism('Roof', [(-0.27, 1.962), (-0.27, 1.985), (-0.255, 1.992), (0.255, 2.018), (0.27, 2.008), (0.27, 1.962)], -0.42, 0.42, M_DARK, bevel=0.004, segs=2)
cab = [base, body, roof]
cut(body, box('c', (0.25, 0, 1.095), (0.028, 0.73, 1.60), M_DARK, bevel=0.004))                  # the door's recess (14 mm)
door = box('Door', (0.23925, 0, 1.095), (0.0185, 0.720, 1.590), M_WHITE, bevel=0.0035)
doorp = [door]


def louvres(tag, parts, hosts, centre, axis, sgn, w, h, n):
    """A louvred opening: a recess in the hosts, its slats laid into it. axis = the wall's normal."""
    cx, cy, cz = centre; d = 0.05
    for q in hosts:
        g = 0.012 if q is body and len(hosts) > 1 else 0.0                                          # (wider behind the door: no two walls in one plane)
        cut(q, box('c', centre, (d, w + g, h + g) if axis == 'x' else (w + g, d, h + g), M_DARK, bevel=0.004))
    for i in range(n):
        z = cz - h / 2 + h * (i + 0.5) / n
        c = (cx - sgn * 0.011, cy, z) if axis == 'x' else (cx, cy - sgn * 0.011, z)
        s = box('%s%d' % (tag, i), c, (0.024, w + 0.003, 0.0025) if axis == 'x' else (w + 0.003, 0.024, 0.0025), M_DARK)
        turn(s, c, 'Y' if axis == 'x' else 'X', sgn * 35 if axis == 'x' else -sgn * 35); parts.append(s)


louvres('LvF', doorp, [body, door], (0.2485, 0, 0.56), 'x', 1, 0.52, 0.36, 14)
louvres('LvL', cab, [body], (0.0, 0.40, 1.35), 'y', 1, 0.36, 0.80, 28)
louvres('LvB', cab, [body], (-0.25, 0.0, 1.45), 'x', -1, 0.60, 0.60, 21)
louvres('LvC', cab, [body], (-0.25, 0.0, 0.55), 'x', -1, 0.60, 0.40, 14)
for i, z in enumerate((0.45, 1.10, 1.75)):                                                        # hinges on the door's right edge
    doorp.append(cyl('Hinge%d' % i, (0.2555, -0.3675, z), 0.009, 0.075, 'z', M_STEEL, 24, bevel=0.0015))
doorp += [box('HandleBase', (0.2500, 0.315, 1.02), (0.008, 0.036, 0.190), M_PLA, bevel=0.003), box('Handle', (0.2610, 0.315, 1.035), (0.020, 0.022, 0.130), M_PLA, bevel=0.006, segs=3),
          cyl('Lock', (0.2545, 0.315, 0.950), 0.011, 0.006, 'x', M_STEEL, 28, bevel=0.001),
          box('Bezel', (0.2515, 0, 1.50), (0.012, 0.30, 0.215), M_GLASS, bevel=0.005, segs=3),
          ]
sign = box('Sign', (0.2500, 0.0, 1.15), (0.0070, 0.22, 0.11), M_SIGN, bevel=0.0008); doorp.append(sign)   # the voltage warning and the rating
screen = box('Screen', (0.2580, 0, 1.50), (0.0064, 0.260, 0.1625), M_SCR)
doorp.append(box('LampHousing', (0.2520, 0, 1.78), (0.013, 0.50, 0.040), M_PLA, bevel=0.005, segs=3))
lampo = box('Lamp', (0.2590, 0, 1.78), (0.006, 0.470, 0.018), M_LAMP, bevel=0.0025)
doorp += [box('EStopPlate', (0.2515, 0.20, 1.15), (0.008, 0.10, 0.10), M_YEL, bevel=0.003), cyl('EStopCollar', (0.2610, 0.20, 1.15), 0.024, 0.014, 'x', M_PLA, 36, bevel=0.002)]
cap = lathe('EStopCap', [(0.0, 0.0), (0.0200, 0.0), (0.0205, 0.004), (0.0195, 0.010), (0.014, 0.0145), (0.0, 0.0158)], (0, 0, 0), M_RED, segs=40)
cap.data.transform(Matrix.Translation((0.2805, 0.20, 1.15)) @ Matrix.Rotation(-math.pi / 2, 4, 'Z')); cap.data.update()
estop = [cyl('EStopStem', (0.2740, 0.20, 1.15), 0.012, 0.018, 'x', M_STEEL, 24), cap]
# ---- the right side: gland, saddle and holster
XS, ZS, RS, RC = -0.09, 1.70, 0.11, 0.018
side = [cyl('SaddleFlange', (XS, -0.403, ZS), 0.130, 0.010, 'y', M_DARK, 56, bevel=0.002), cyl('SaddleDrum', (XS, -0.490, ZS), RS, 0.170, 'y', M_PLA, 64, bevel=0.003), cyl('SaddleLip', (XS, -0.578, ZS), 0.138, 0.010, 'y', M_PLA, 64, bevel=0.004),
        cyl('SaddleBolt', (XS, -0.5845, ZS), 0.018, 0.006, 'y', M_STEEL, 6, bevel=0.0015)]
side += [box('GlandBox', (-0.22, -0.4335, 1.215), (0.090, 0.075, 0.110), M_DARK, bevel=0.006), cyl('Gland', (-0.22, -0.4400, 1.1475), 0.0265, 0.034, 'z', M_PLA, 12, bevel=0.002), cyl('GlandNut', (-0.22, -0.4400, 1.124), 0.0225, 0.020, 'z', M_PLA, 32, bevel=0.003)]
HX, HY, HZ = 0.165, -0.455, 1.05
FR = Matrix.Translation((HX, HY, HZ)) @ Matrix.Rotation(math.pi / 2, 4, 'Z')                      # the coupling in the holster: x = into the cabinet
hol = box('Holster', (0.030, 0, -0.002), (0.060, 0.100, 0.140), M_PLA, bevel=0.010, segs=3)
cut(hol, ccs2.extr('c', ccs2.ac(0.0015), -0.02, ccs2.NOSE + 0.004, M_PLA)); cut(hol, ccs2.extr('c', ccs2.dc(0.0015), -0.02, ccs2.NOSE + 0.004, M_PLA))
hp = [hol, box('HolsterPlate', (0.0535, 0, -0.002), (0.006, 0.130, 0.190), M_STEEL, bevel=0.002)]
for s in (1, -1): hp.append(cyl('HolBolt%d' % s, (0.0490, 0, -0.002 + s * 0.082), 0.007, 0.006, 'x', M_STEEL, 6, bevel=0.001))
for o in hp: o.data.transform(FR); o.data.update()
side += hp
# ---- the plug (coupling frame, then moved into the holster)
pw = lambda v, e: math.copysign(abs(v) ** e, v)
flange = ccs2.extr('Pl_Flange', [(0.033 * pw(math.cos(a), 0.45), 0.003 + 0.0585 * pw(math.sin(a), 0.55)) for a in (2 * math.pi * (i + 0.5) / 64 for i in range(64))], -0.044, -0.0012, M_PLA, bevel=0.003)
pl = ccs2.nose(M_INS, M_INS) + [flange]
pl.append(tube('Pl_Body', [(-0.030, 0, -0.008), (-0.070, 0, -0.014), (-0.130, 0, -0.030), (-0.185, 0, -0.052), (-0.206, 0, -0.072)], 0.0285, M_PLA, res=20))
pl.append(tube('Pl_Handle', [(-0.028, 0, 0.044), (-0.060, 0, 0.0535), (-0.110, 0, 0.0545), (-0.160, 0, 0.036), (-0.190, 0, 0.000), (-0.193, 0, -0.045)], 0.0155, M_PLA, res=20))
pl.append(box('Pl_Button', (-0.062, 0, 0.0695), (0.030, 0.016, 0.006), M_INS, bevel=0.0025))
for i in range(4):                                                                              # the grip's ribs
    r = torus('Pl_Rib%d' % i, (-0.085 - 0.009 * i, 0, 0.0545 - 0.00004 * (9 * max(0, i - 2)) ** 2), 0.0150, 0.0024, 'x', M_INS, 32, 10); pl.append(r)
A = Vector((-0.206, 0, -0.072)); D = Vector((-1, 0, -1)).normalized(); BL = 0.085
TILT = Matrix.Translation(A) @ Vector((0, 0, 1)).rotation_difference(D).to_matrix().to_4x4()
boot = [cyl('Pl_Boot%d' % i, (0, 0, i * 0.017 + 0.004), r, 0.0125, 'z', M_RUB, 36, bevel=0.003) for i, r in enumerate((0.0305, 0.0285, 0.0265, 0.0245, 0.0230))] + [cyl('Pl_Core', (0, 0, BL / 2 - 0.006), 0.0215, BL + 0.012, 'z', M_RUB, 36, bevel=0.003)]
for o in boot: o.data.transform(TILT); o.data.update()
pl += boot; END = A + D * BL
for o in pl: o.data.transform(FR); o.data.update()
W = FR @ END; WD = (FR.to_3x3() @ D).normalized()
# ---- the cable, at rest: out of the gland, down, up over the saddle, down again, round and into the plug


def arc(cx, cz, r, a0, a1, y0, y1, n=14):
    return [Vector((cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / n)), y0 + (y1 - y0) * i / n, cz + r * math.sin(math.radians(a0 + (a1 - a0) * i / n)))) for i in range(n + 1)]


p = [Vector((-0.22, -0.440, z)) for z in (1.120, 1.0, 0.8, 0.6)]
p += arc(XS, 0.53, 0.13, 180, 360, -0.440, -0.470)
p += [Vector((0.04, -0.470 - 0.028 * t, 0.53 + 1.17 * t)) for t in (0.2, 0.5, 0.8)]
p += arc(XS, ZS, RS + RC + 0.0012, 0, 180, -0.498, -0.530)
p += [Vector((-0.22, -0.530 - 0.018 * t, 1.70 - 1.30 * t)) for t in (0.2, 0.5, 0.8)]
p += arc(-0.0275, 0.40, 0.1925, 180, 360, -0.548, -0.790, 18)
p += [Vector((0.165, -0.790 - 0.004 * t, 0.40 + 0.36 * t)) for t in (0.25, 0.5, 0.75, 1.0)]
q0, q2 = p[-1].copy(), W - WD * 0.004; q1 = W + WD * 0.085
p += [q0.lerp(q1, t).lerp(q1.lerp(q2, t), t) for t in (i / 10 for i in range(1, 11))]
cable = tube('Cable', p, RC, M_RUB, res=10, bez=False)
LEN = sum((p[i + 1] - p[i]).length for i in range(len(p) - 1))
# ---- the impact posts
posts = []
for s in (1, -1):
    posts.append(lathe('Post%d' % s, [(0.0, 1.0), (0.052, 0.996), (0.066, 0.985), (0.070, 0.968), (0.070, -0.02), (0.0, -0.02)], (0, 0, 0), M_YEL, segs=48))
    posts[-1].data.transform(Matrix.Translation((0.62, s * 0.55, 0)) @ Matrix.Rotation(math.pi / 2, 4, 'X')); posts[-1].data.update()
    posts.append(cyl('PostPlate%d' % s, (0.62, s * 0.55, 0.006), 0.115, 0.016, 'z', M_YEL, 40, bevel=0.003))
    for k in range(2): posts.append(cyl('PostBand%d%d' % (s, k), (0.62, s * 0.55, 0.72 + 0.17 * k), 0.0715, 0.085, 'z', M_BLK, 48, bevel=0.0006))
    for k in range(4): posts.append(cyl('PostBolt%d%d' % (s, k), (0.62 + 0.093 * math.cos(k * math.pi / 2 + 0.785), s * 0.55 + 0.093 * math.sin(k * math.pi / 2 + 0.785), 0.018), 0.009, 0.012, 'z', M_STEEL, 6, bevel=0.0015))
K.uv_cube()
for o, w, h, zc in ((screen, 0.260, 0.1625, 1.50), (sign, 0.22, 0.11, 1.15)):                        # the pictures: once, over the front face
    uv = o.data.uv_layers.active.data
    for po in o.data.polygons:
        for li in po.loop_indices:
            c = o.data.vertices[o.data.loops[li].vertex_index].co
            uv[li].uv = (0.5 + c.y / w, 0.5 + (c.z - zc) / h) if po.normal.x > 0.9 else (0.01, 0.01)
# ---- flicker part by part (BC_LOOSE), then joined to the parts the game moves
glbf = os.path.join(HERE, 'buscharger.glb')
if os.environ.get('BC_LOOSE'):
    for o in K.ALL: o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(os.environ['TEMP'], 'bc_loose.glb'), export_format='GLB', use_selection=True, export_apply=True); print('BC loose', len(K.ALL)); sys.exit()
for nm, l in (('Plinth', plinth), ('Cabinet', cab), ('Door', doorp), ('SideMounts', side), ('Plug', pl), ('EStop', estop), ('Posts', posts)): ccs2.join(nm, l)


def piv(n, loc, rot=(0, 0, 0)): return ik(n, loc, rot, prefix='PIV_')


piv('Plug', (HX, HY, HZ), (0, 0, math.pi / 2)); piv('Screen', (0.2612, 0, 1.50)); piv('Lamp', (0.259, 0, 1.78)); piv('EStop', (0.2805, 0.20, 1.15)); piv('Door', (0.2555, -0.3675, 1.095)); piv('Cable', (-0.22, -0.440, 1.12))
ik('CableOut', (-0.22, -0.440, 1.12), (0, math.pi / 2, 0), prefix='SOCK_'); ik('Holster', (HX, HY, HZ), (0, 0, math.pi / 2), prefix='SOCK_')
ik('PlugCable', tuple(W), tuple(Vector((1, 0, 0)).rotation_difference(WD).to_euler()), prefix='SOCK_'); ik('Saddle', (XS, -0.514, ZS + RS), prefix='SOCK_')
ik('Hand_Plug_R', tuple(FR @ Vector((-0.110, 0, 0.0545)))); ik('Hand_EStop', (0.297, 0.20, 1.15)); ik('Stand_Charger', (0.95, -0.15, 0.0)); ik('Stand_Plug', (0.25, -1.15, 0.0))
last = K.save_export(ROOT, os.path.join(HERE, 'BusCharger.blend'), glbf, os.path.join(HERE, 'zfight-check.json'), 'BC', 'BusCharger')
print('BC cable %.2f m, boot end %s, parts %s, tris %d' % (LEN, [round(v, 3) for v in W], [o.name for o in K.ALL], sum(len(f.vertices) - 2 for o in K.ALL for f in o.data.polygons)))
