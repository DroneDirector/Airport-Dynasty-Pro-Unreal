# The small 2-axle service truck (potable water truck and toilet truck, to-do 212): low electric day cab, wheels, frame, side boxes, rear bar, chocks,
# and what both bodies share (hose reels, tanks, joining, checks, saving). Materials, lettering, seat and mudguard come from Tools/truck_cab.py; the cab is its own.
# It runs in the caller's own names: set PFX, DASHWORD, MAIN, NAME ('WaterTruck'), KEY ('watertruck'), TAG;  exec(compile(open(f).read(), f, 'exec'))
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
import vehicle_kit as K
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, ALL, IKS
ART = os.path.join(ROOT, 'Art', NAME); TEX = os.path.join(ROOT, 'Art', 'PushbackTug', 'Textures')
K.reset()
import make_cleaningkit as CK
import vehicle_skin as S
from vehicle_skin import rawprism, boole, patch, grow, rnd, ip
CK.join.__globals__['K'] = K
for _o in list(ALL): bpy.data.objects.remove(_o)
ALL.clear(); IKS.clear()
FX, DX, RW, TW, FLOOR, CABB, ox, oz = 1.72, -1.28, .383, .215, .82, 1.18, .985, .245      # axles, tyre radius and width, cab floor, cab's back, dashboard's place
_t = open(os.path.join(ROOT, 'Tools', 'truck_cab.py'), encoding='utf-8').read().split('# #region ----')
exec(compile('# ' + _t[1], 'truck_cab materials', 'exec'))
bpy.data.materials.remove(M_MAIN); M_MAIN = P(PFX + 'Livery_Main_Paint', MAIN, .1, .35)
M_SS = P(PFX + 'StainlessSteel', (.80, .81, .83), .55, .30)
SIDES = ((1, 'L', 'y+'), (-1, 'R', 'y-'))
_s = _t[3]; exec(compile(_s[_s.index('def seat('):_s.index("seat('SeatDriver'")], 'truck_cab seat', 'exec'))
_s = _t[4]; exec(compile(_s[_s.index('def fender('):], 'truck_cab fender', 'exec'))

# #region ---- cab skin
XFt = [(.34, 2.80), (.48, 2.865), (.75, 2.895), (1.05, 2.89), (1.30, 2.855), (1.44, 2.82), (1.80, 2.69), (2.10, 2.57), (2.20, 2.50), (2.28, 2.38), (2.33, 2.22), (2.36, 1.98), (2.42, 1.7)]
XRt = [(.34, CABB), (2.16, CABB), (2.26, CABB + .03), (2.33, CABB + .09), (2.36, CABB + .22), (2.42, CABB + .4)]
WHt = [(.34, .955), (.48, .985), (.75, 1.0), (1.35, 1.0), (1.80, .965), (2.10, .935), (2.20, .905), (2.28, .84), (2.33, .72), (2.36, .52), (2.42, .25)]
LNt = [(.34, .30), (1.4, .32), (2.1, .36), (2.36, .46), (2.42, .46)]
ZSt = [.34, .39, .45, .51, .56, .62, .69, .76, .83, .90, .96, 1.02, 1.10, 1.18, 1.26, 1.34, 1.42, 1.50, 1.58, 1.66, 1.74, 1.82, 1.90, 1.97, 2.04, 2.10, 2.15, 2.19, 2.22, 2.25, 2.275, 2.295, 2.31, 2.325, 2.337, 2.345, 2.352, 2.36]
S.setup(XFt, XRt, WHt, LNt, ZSt, 2.16, [M_BODY, M_MAIN, M_BUMP, M_LIN, M_TRIM, M_SEAL], M_GLASS, M_SEAL, n=3.0, rr=.10)
SH = .040; OY = (.4, 1.5)
shell = S.loft('Body', 0.0); inner = S.loft('inner', SH, bot=.5, mi=3)
for p in inner.data.polygons:
    if p.center.z < 1.45: p.material_index = 4
boole(shell, inner, 'DIFFERENCE', False)
for p in shell.data.polygons:
    c = p.center
    if p.material_index == 0 and abs(p.normal.z) < .9:
        if .62 < c.z < 1.02: p.material_index = 1
        if c.z < .62: p.material_index = 2
boole(shell, rawprism('c', [(.8, .1), (2.18, .1), (2.18, .785), (.8, .785)], 'y', -2, 2), 'DIFFERENCE', False)                     # over the wheels and behind: open
for s in (-1, 1): boole(shell, rawprism('c', [(2.10, .35), (2.565, .35), (2.565, .787), (2.10, .787)], 'y', s * .70, s * 1.5), 'DIFFERENCE', False)      # step wells
DOORF = rnd([(1.385, .86), (2.565, .86), (2.565, 1.40), (2.345, 2.10), (1.385, 2.10)], [.06, .06, .12, .10, .08]); GAP = .004
shell0 = S.copy(shell, 'shell0')
def opening(name, pts, axis, a, b):
    pan = S.copy(shell0, name); boole(pan, rawprism('c', grow(pts, -GAP), axis, a, b), 'INTERSECT', False); boole(shell, rawprism('c', pts, axis, a, b), 'DIFFERENCE', False); return pan
doorD = opening('DoorDriver', DOORF, 'y', *OY); doorP = opening('DoorPassenger', DOORF, 'y', -OY[1], -OY[0]); S.rm(shell0)
WSC = rnd([(-.86, 1.46), (.86, 1.46), (.78, 2.09), (-.78, 2.09)], [.07, .07, .10, .10]); boole(shell, rawprism('c', WSC, 'x', 2.0, 3.4), 'DIFFERENCE', False)
WD = rnd([(1.47, 1.50), (2.475, 1.44), (2.285, 2.035), (1.47, 2.035)], .06)
for o, s in ((doorD, 1), (doorP, -1)): boole(o, rawprism('c', WD, 'y', s * OY[0], s * OY[1]), 'DIFFERENCE', False)
HL = rnd([(.60, .66), (.85, .66), (.87, .93), (.66, .93)], .04)                                                                    # upright lamps
for s in (-1, 1): boole(shell, rawprism('c', [(s * y, z) for y, z in HL], 'x', 2.2, 3.4), 'DIFFERENCE', False)
for o in (shell, doorD, doorP):
    used = sorted({p.material_index for p in o.data.polygons}); ms = [o.data.materials[i] for i in used]; ix = [used.index(p.material_index) for p in o.data.polygons]; o.data.materials.clear()
    for m in ms: o.data.materials.append(m)
    for p, i in zip(o.data.polygons, ix): p.material_index = i; p.use_smooth = True
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(32); ALL.append(o)
S.pane('Windscreen', WSC, 'x', 2.0, 3.4); S.pane('DoorDriver_Window', WD, 'y', *OY); S.pane('DoorPassenger_Window', WD, 'y', -OY[1], -OY[0])
for s, sn in ((1, 'L'), (-1, 'R')):
    h = [(s * y, z) for y, z in HL]; B = ('x', 2.2, 3.4)
    patch('Headlamp_%s_Bezel' % sn, grow(h, .012), *B, M_PLAST, -.004, .045, hole=grow(h, -.012)); patch('Headlamp_%s_Bowl' % sn, grow(h, -.008), *B, M_CHROME, .022, .040)
    for nm, z0, z1, mt in (('LowBeam', .67, .755, M_WHT), ('HighBeam', .76, .845, M_WHT), ('IndicatorF', .85, .925, M_AMB)):
        o = patch('%s_%s_Lens' % (nm, sn), grow(h, -.010), *B, mt, .001, .006); boole(o, rawprism('c', [(-1.5, z0), (1.5, z0), (1.5, z1), (-1.5, z1)], 'x', 2, 4), 'INTERSECT', False); K.finish(o, mt, 32); ALL.remove(o); assert len(o.data.polygons), nm
    dn = 'DoorDriver' if s > 0 else 'DoorPassenger'
    patch('CabSideMarker_' + sn, rnd([(1.225, 1.06), (1.335, 1.06), (1.335, 1.11), (1.225, 1.11)], .015, 3), 'y', s * OY[0], s * OY[1], M_AMB, -.010, .004)
    for i, (x0, x1) in enumerate(((1.42, 1.78), (1.79, 2.15), (2.16, 2.53))): patch(dn + '_HiVis%d' % i, [(x0, .92), (x1, .92), (x1, .99), (x0, .99)], 'y', s * OY[0], s * OY[1], M_HVR if i % 2 else M_HVW, -.0025, .006)
patch('Grille_Field', rnd([(-.44, .68), (.44, .68), (.50, .86), (-.50, .86)], .05), 'x', 2.4, 3.4, M_PLAST, -.004, .02)
for i in range(4): patch('Grille_Slat%d' % i, [(-.40 - .006 * i, .705 + .038 * i), (.40 + .006 * i, .705 + .038 * i), (.40 + .006 * i, .718 + .038 * i), (-.40 - .006 * i, .718 + .038 * i)], 'x', 2.4, 3.4, M_DARK, -.014, .002)
patch('PlateFront', [(-.26, .46), (.26, .46), (.26, .57), (-.26, .57)], 'x', 2.4, 3.4, M_NAME, -.005, .006)
patch('NamePanel_Front', rnd([(-.55, 1.13), (.55, 1.13), (.55, 1.215), (-.55, 1.215)], .02, 3), 'x', 2.4, 3.4, M_NAME, -.004, .006)
patch('FrontStrip', rnd([(-.74, 1.30), (.74, 1.30), (.72, 1.40), (-.72, 1.40)], .03, 3), 'x', 2.3, 3.4, M_PLAST, -.003, .006)      # black under the glass
box('FrontTowEye', (2.83, -.42, .44), (.10, .05, .08), M_YEL, .012)
fl = box('Floor', (1.95, 0, FLOOR - .020), (1.46, 1.90, .04), M_TRIM, .004, 1)
for s in (-1, 1): cut(fl, box('c', (2.3725, s * .94, FLOOR), (.385, .22, .3), M_TRIM))
box('CabMat', (1.93, 0, FLOOR + .003), (1.30, 1.56, .012), M_MAT, .004); box('EngineHump', (1.60, 0, FLOOR + .12), (.80, .40, .25), M_TRIM, .06, 3)
cw = rawprism('Cowl', [(-1.3, 1.392), (1.3, 1.392), (1.3, 1.410), (-1.3, 1.410)], 'x', 2.46, 3.2); boole(cw, rawprism('c', [(2, 1.2), (3.4, 1.2), (3.4, 1.6), (2, 1.6)], 'y', -.90, .90), 'INTERSECT', False); boole(cw, S.LF(.037), 'INTERSECT'); K.finish(cw, M_PLAST, 32)
fw = rawprism('Firewall', [(-1.3, FLOOR - .04), (1.3, FLOOR - .04), (1.3, 1.400), (-1.3, 1.400)], 'x', 2.715, 2.732); boole(fw, S.LF(.037), 'INTERSECT'); K.finish(fw, M_PLAST, 30)
patch('CabLiner', [(1.24, 2.14), (2.24, 2.14), (2.24, 2.6), (1.24, 2.6)], 'y', -1.4, 1.4, M_LIN, .037, .052)
box('RoofLight_Base', (1.85, 0, 2.298), (.26, .12, .03), M_PLAST, .006); box('RoofLight_Glass', (1.85, 0, 2.282), (.24, .10, .012), M_NAME, .004)
for s, sn, n in ((1, 'L', 'Driver'), (-1, 'R', 'Passenger')):
    box('Step%s_Riser' % n, (2.372, s * .700, .575), (.40, .016, .45), M_BUMP, .003, 1)
    for e, x in ((0, 2.176), (1, 2.569)): box('Step%s_End%d' % (n, e), (x, s * .835, .575), (.016, .27, .45), M_BUMP, .003, 1)
    box('Step%s_Tread' % n, (2.3725, s * .8325, .42), (.385, .265, .035), M_DECK, .006, 2)
    ik('Step_Door%s' % n, (2.37, s * .83, .4395)); ik('Step_Door%s_Low' % n, (2.37, s * .83, .4395))
# #endregion

# #region ---- cab doors, dashboard, seats
for s, n in ((1, 'DoorDriver'), (-1, 'DoorPassenger')):
    a, b = s * OY[0], s * OY[1]; side = 'L' if s > 0 else 'R'
    patch(n + '_Trim', grow(DOORF, -.028), 'y', a, b, M_DASH, .038, .060, hole=grow(WD, .012))
    box(n + '_Armrest', (1.90, s * .912, 1.26), (.50, .055, .06), M_DASH, .02, 3); box(n + '_Pocket', (1.90, s * .924, 1.00), (.66, .040, .14), M_DASH, .015)
    box(n + '_HandleIn_Cup', (2.26, s * .933, 1.34), (.13, .018, .06), M_PLAST, .008); box(n + '_HandleIn', (2.25, s * .922, 1.345), (.085, .012, .022), M_CHROME, .004)
    box(n + '_Pull', (2.28, s * .920, 1.14), (.05, .05, .18), M_PLAST, .016, 3); box(n + '_Switches', (2.08, s * .912, 1.293), (.13, .045, .014), M_PLAST, .005)
    patch(n + '_HandleCup', rnd([(1.445, 1.135), (1.585, 1.135), (1.585, 1.205), (1.445, 1.205)], .025, 4), 'y', a, b, M_PLAST, -.002, .008)
    box(n + '_Handle', (1.52, s * 1.022, 1.172), (.15, .018, .030), M_PLAST, .008, 3); box(n + '_Handle_P0', (1.455, s * 1.009, 1.172), (.02, .028, .022), M_PLAST, .004); box(n + '_Handle_P1', (1.58, s * 1.009, 1.172), (.02, .028, .022), M_PLAST, .004)
    for i, z in enumerate((.97, 1.30)): box(n + '_Hinge%d' % i, (2.500, s * .940, z), (.07, .035, .08), M_DARK, .004)
    box(n + '_MirrorFoot', (2.40, s * .982, 1.585), (.07, .07, .09), M_PLAST, .018, 3); box(n + '_MirrorArm', (2.415, s * 1.055, 1.62), (.05, .15, .055), M_PLAST, .018, 3); box(n + '_MirrorShell', (2.43, s * 1.165, 1.66), (.10, .18, .44), M_PLAST, .045, 4)
    box(n + '_MirrorGlass', (2.3795, s * 1.165, 1.72), (.004, .145, .24), M_MIR, .001, 1); box(n + '_MirrorGlass2', (2.3795, s * 1.165, 1.525), (.004, .145, .10), M_MIR, .001, 1)
    piv(n, (2.590, s * .985, 1.15)); ik('Handle_%s_Out' % n, (1.52, s * 1.031, 1.172)); ik('Handle_%s_In' % n, (2.25, s * .915, 1.345)); ik('Stand_%s' % n, (2.02, s * 1.66, 0))
    tube('GrabHandle_' + side, [(1.315, s * 1.015, 1.16), (1.315, s * 1.048, 1.22), (1.315, s * 1.040, 1.66), (1.315, s * .985, 1.72)], .013, M_PLAST); ik('Grab_' + side, (1.315, s * 1.044, 1.45))
    box('GrabHandle_%s_F0' % side, (1.315, s * 1.004, 1.16), (.045, .05, .06), M_PLAST, .012); box('GrabHandle_%s_F1' % side, (1.315, s * .976, 1.72), (.045, .05, .06), M_PLAST, .012)
    tube('GrabIn_' + side, [(2.42, s * .845, 1.60), (2.38, s * .835, 1.64), (2.28, s * .825, 1.86), (2.26, s * .850, 1.90)], .012, M_PLAST); box('GrabIn_%s_F0' % side, (2.455, s * .865, 1.585), (.10, .08, .06), M_PLAST, .012); box('GrabIn_%s_F1' % side, (2.295, s * .870, 1.91), (.13, .06, .05), M_PLAST, .012); ik('GrabIn_' + side, (2.33, s * .830, 1.75))
Xo = lambda x: x + ox
Zo = lambda z: z + oz
DY = .50
da = rawprism('Dashboard', [(Xo(x), Zo(z)) for x, z in rnd([(1.740, .86), (1.38, .90), (1.28, 1.00), (1.26, 1.10), (1.30, 1.145), (1.46, 1.158), (1.740, 1.158)], [.02, .06, .05, .05, .03, .04, .02], 4)], 'y', -1.3, 1.3); boole(da, S.LF(.0385), 'INTERSECT')
boole(da, rawprism('c', [(2.0, .8), (3.2, .8), (3.2, 1.9), (2.0, 1.9)], 'y', -.865, .865), 'INTERSECT', False); K.finish(da, M_DASH, 32)
box('Dash_Binnacle', (Xo(1.36), DY, Zo(1.175)), (.22, .40, .11), M_DASH, .04, 4); box('Dash_Cluster', (Xo(1.2495), DY, Zo(1.175)), (.010, .32, .080), M_SCR, .004); ik('Cluster', (Xo(1.24), DY, Zo(1.175)))
box('Dash_Centre', (Xo(1.265), .04, Zo(1.02)), (.06, .36, .20), M_PLAST, .015, 3); box('Dash_Screen', (Xo(1.2325), .04, Zo(1.055)), (.010, .20, .10), M_SCR, .003); text('Dash_Word', DASHWORD, (Xo(1.2275), .04, Zo(1.085)), .016, 'x-', M_SWHT, .0008)
for i in range(6): box('Dash_Btn%d' % i, (Xo(1.2335), -.085 + .05 * i, Zo(.955)), (.010, .036, .022), M_RED if i == 3 else M_DARK, .003)
for i, y in enumerate((-.76, -.24, .24, .76)):
    box('Dash_Vent%d' % i, (Xo(1.285), y, Zo(1.06)), (.07, .10, .08), M_PLAST, .012)
    for k in range(4): box('Dash_Vent%d_S%d' % (i, k), (Xo(1.2485), y, Zo(1.033 + .018 * k)), (.010, .08, .006), M_DARK, .001, 1)
box('Dash_Glove', (Xo(1.262), -.50, Zo(1.0)), (.03, .36, .15), M_DASH, .012); box('Dash_GloveCatch', (Xo(1.2455), -.50, Zo(1.06)), (.010, .10, .022), M_PLAST, .004)
cyl('GearSelect_Base', (1.88, .0, FLOOR + .25), .052, .02, 'z', M_PLAST, 32, .004); cyl('GearSelect', (1.88, 0, FLOOR + .285), .026, .06, 'z', M_CHROME, 32, .006); ik('GearSelect', (1.88, 0, FLOOR + .315)); box('ParkBrake', (1.74, .0, FLOOR + .27), (.05, .03, .07), M_RED, .008); ik('ParkBrake', (1.74, 0, FLOOR + .305))
SC = Vector((Xo(1.085), DY, Zo(1.170))); AXW = Vector((-.7071, 0, .7071))
def tilt(o): o.data.transform(Matrix.Translation(SC) @ AXW.to_track_quat('Z', 'Y').to_matrix().to_4x4()); o.data.update(); return o
tilt(torus('SteeringWheel_Rim', (0, 0, 0), .205, .0165, 'z', M_PLAST, 64, 16)); tilt(cyl('SteeringWheel_Boss', (0, 0, -.012), .078, .045, 'z', M_PLAST, 40, .012)); tilt(cyl('SteeringWheel_Cap', (0, 0, .012), .040, .012, 'z', M_DASH, 32, .004))
for i, a in enumerate((90, 210, 330)):
    q = Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0)); tilt(tube('SteeringWheel_Spoke%d' % i, [tuple(q * .05 + Vector((0, 0, -.015))), tuple(q * .13 + Vector((0, 0, -.008))), tuple(q * .203)], .014, M_PLAST))
tube('SteerColumn', [tuple(SC - AXW * .035), tuple(SC - AXW * .34)], .032, M_PLAST, bez=False); tilt(box('SteerShroud', (0, 0, -.10), (.12, .13, .11), M_PLAST, .025, 3))
for s, n in ((1, 'Ind'), (-1, 'Wip')): tilt(tube('Stalk' + n, [(0, s * .05, -.075), (0, s * .13, -.055), (0, s * .165, -.05)], .008, M_PLAST, bez=False))
piv('SteeringWheel', tuple(SC))
for s, n in ((1, 'L'), (-1, 'R')): ik('Hand_Wheel_' + n, tuple(SC + Vector((0, s * .205, 0))))
for n, y, w, zz in (('Accel', DY - .12, .055, .70), ('Brake', DY + .01, .085, .71)):
    c = (Xo(1.475), y, Zo(zz)); turn(box('Pedal_%s_Pad' % n, c, (.018, w, .10), M_SEAL, .004), c, 'y', -25); tube('Pedal_%s_Arm' % n, [(c[0] + .003, y, c[2]), (Xo(1.53), y, Zo(zz + .10)), (Xo(1.60), y, Zo(.93))], .010, M_DARK); ik('Foot_' + n, (Xo(1.462), y, Zo(zz))); piv('Pedal_' + n, (Xo(1.60), y, Zo(.93)))
turn(box('FootRest', (Xo(1.50), DY + .15, Zo(.66)), (.02, .075, .20), M_SEAL, .005), (Xo(1.50), DY + .15, Zo(.66)), 'y', -35); ik('Foot_Rest', (Xo(1.485), DY + .15, Zo(.66))); box('FootRest_B', (Xo(1.56), DY + .15, Zo(.61)), (.11, .05, .08), M_PLAST, .01)
seat('SeatDriver', 1.76, DY, .48, back=.58); seat('SeatPassenger', 1.76, -DY, .48, back=.58)
for s, n in ((1, 'D'), (-1, 'P')):
    box('Belt%s_Top' % n, (1.335, s * .905, 1.86), (.05, .05, .07), M_PLAST, .01); box('Belt%s_Web' % n, (1.335, s * .895, 1.41), (.046, .006, .84), M_BELT, .001, 1); box('Belt%s_Reel' % n, (1.335, s * .905, .95), (.10, .08, .16), M_PLAST, .015); box('Belt%s_Tongue' % n, (1.335, s * .889, 1.40), (.05, .012, .07), M_STEEL, .004); ik('BeltTongue_' + n, (1.335, s * .88, 1.40))
    patch('BPillar_' + n, [(1.28, .87), (1.375, .87), (1.375, 2.08), (1.28, 2.08)], 'y', s * .6, s * 1.4, M_TRIM, .037, .050)
    box('SunVisor_' + n, (2.16, s * .42, 2.185), (.16, .50, .022), M_LIN, .008, 3); cyl('SunVisor_%s_Rod' % n, (2.245, s * .42, 2.188), .008, .50, 'y', M_PLAST, 16)
    for e in (.20, .64): box('SunVisor_%s_F%d' % (n, e > .5), (2.245, s * e, 2.22), (.045, .03, .075), M_PLAST, .006)
box('Locker_Back', (1.270, 0, 1.45), (.12, .30, .60), M_TRIM, .03, 3); box('Clipboard', (1.336, 0, 1.55), (.012, .22, .30), M_DARK, .003); box('Clipboard_Paper', (1.3435, 0, 1.54), (.005, .20, .26), M_SWHT, .001, 1); ik('Clipboard', (1.344, 0, 1.55))
for s, n, py in ((1, 'L', .76), (-1, 'R', -.04)):
    pv = S.surf(py, 1.440, .006); piv('Wiper_' + n, tuple(pv)); cyl('Wiper%s_Hub' % n, tuple(pv), .022, .06, 'z', M_PLAST, 20, .004)
    tube('Wiper%s_Arm' % n, [tuple(S.surf(py, 1.455, .022)), tuple(S.surf(py - .18, 1.482, .030)), tuple(S.surf(py - .38, 1.510, .016))], .008, M_PLAST)
    tube('Wiper%s_Blade' % n, [tuple(S.surf(py - .04 - .085 * k, 1.492 + .004 * k, .003)) for k in range(9)], .010, M_SEAL); ik('WiperTip_' + n, tuple(S.surf(py - .72, 1.524, .003)))
cyl('Beacon_Base', (1.65, 0, 2.370), .085, .05, 'z', M_PLAST, 40, .006); CK.lathez('BeaconLens', [(0, 2.383), (.074, 2.383), (.078, 2.40), (.076, 2.465), (.066, 2.495), (.04, 2.512), (0, 2.515)], (1.65, 0, 0), M_AMB, 48); cyl('Beacon_Cap', (1.65, 0, 2.514), .03, .012, 'z', M_PLAST, 24, .003); ik('Beacon', (1.65, 0, 2.44))
# #endregion

# #region ---- wheels, frame, side boxes, rear bar, chocks
kr, kw = RW / .372, TW / .233; kd = RW / .507
TP = [(r * kr, y * kw) for r, y in ((.200, -.098), (.232, -.112), (.275, -.1165), (.318, -.112), (.348, -.100), (.362, -.086), (.3695, -.066), (.372, -.030), (.372, .030), (.3695, .066), (.362, .086), (.348, .100), (.318, .112), (.275, .1165), (.232, .112), (.200, .098), (.196, 0))]
RP = [(r * kr, y * kw) for r, y in ((.214, -.104), (.208, -.098), (.196, -.092), (.186, -.050), (.182, 0), (.186, .050), (.196, .092), (.208, .098), (.214, .104), (.204, .106), (.190, .098), (.176, .050), (.174, 0), (.176, -.050), (.190, -.098), (.204, -.106))]
def wheel(key, x, y, s, dish, part=''):
    c = (x, y, RW); lathe('Tyre_%s%s' % (key, part), TP, c, M_TYRE, 80, 40); lathe('Rim_%s%s' % (key, part), RP, c, M_RIM, 80, 40); d = s * dish
    wd = lathe('WheelDisc_%s%s' % (key, part), [(0, d * .55), (.10 * kd, d * .55), (.125 * kd, d * .9), (.165 * kd, d), (.200 * kd, d * .8), (.225 * kd, d * .35), (.2445 * kd, d * .2), (.2445 * kd, d * .2 - s * .014), (.218 * kd, d * .35 - s * .014), (.195 * kd, d * .8 - s * .014), (.165 * kd, d - s * .014), (.130 * kd, d * .9 - s * .014), (.105 * kd, d * .55 - s * .014), (0, d * .55 - s * .014)], c, M_RIM, 80, 35)
    for j in range(8):
        a = 2 * math.pi * j / 8 + .15; cut(wd, cyl('c', (x + .212 * kd * math.cos(a), y + d * .5, RW + .212 * kd * math.sin(a)), .018, .14, 'y', M_RIM, 20))
    if not part:
        cyl('Hub_' + key, (x, y + d * .55 - s * .03, RW), .07, .10, 'y', M_DARK, 36, .004); cyl('HubCap_' + key, (x, y + d * .55 + s * .020, RW), .048, .05, 'y', M_STEEL, 36, .012)
        for j in range(6):
            a = 2 * math.pi * j / 6; cyl('Nut_%s_%d' % (key, j), (x + .165 * kd * math.cos(a), y + d + s * .007, RW + .165 * kd * math.sin(a)), .013, .030, 'y', M_STEEL, 6)
FE = globals().get('FRAME_END', -2.05); FYW, DYO, DYI = .855, .868, .622; DECK = .875
for s, sn, f in SIDES:
    wheel('F' + sn, FX, s * FYW, s, .062); wheel('R' + sn, DX, s * DYO, s, -.062); wheel('R' + sn, DX, s * DYI, s, .062, '_In')
    piv('Wheel_F' + sn, (FX, s * FYW, RW)); piv('Wheel_R' + sn, (DX, s * (DYO + DYI) / 2, RW)); piv('Steer_F' + sn, (FX, s * .68, RW))
    box('Spindle_F' + sn, (FX, s * .68, RW), (.10, .07, .19), M_DARK, .012); cyl('Spindle_F%s_Axle' % sn, (FX, s * .76, RW), .04, .16, 'y', M_STEEL, 28); cyl('SpindleDrum_F' + sn, (FX, s * .80, RW), .150, .10, 'y', M_DARK, 48, .01)
    box('SteerArm_F' + sn, (FX - .08, s * .655, RW - .04), (.15, .035, .025), M_DARK, .008); cyl('AxleBeamF_' + sn, (FX, s * .33, RW - .05), .042, .64, 'y', M_DARK, 28); box('AxleEndF_' + sn, (FX, s * .630, RW - .01), (.09, .045, .15), M_DARK, .010)
    box('LeafF_' + sn, (FX, s * .42, RW + .030), (.90, .06, .045), M_DARK, .010)
    for e in (-.5, .5): box('HangerF_%s%d' % (sn, e > 0), (FX + e * .84, s * .42, .515), (.06, .065, .25), M_CHAS, .006)
    cyl('DriveAxle_' + sn, (DX, s * .50, RW), .055, .50, 'y', M_DARK, 32); cyl('DriveDrum_' + sn, (DX, s * .50, RW), .150, .08, 'y', M_DARK, 40, .01); box('LeafR_' + sn, (DX, s * .41, RW + .080), (1.10, .07, .06), M_DARK, .010)
    for e in (-.5, .5): box('HangerR_%s%d' % (sn, e > 0), (DX + e * 1.04, s * .41, .545), (.07, .075, .26), M_CHAS, .006)
    prism('FenderF_' + sn, [(FX - .48, .52), (FX - .44, .72), (FX - .40, .772), (FX + .435, .772), (FX + .435, .783), (FX - .405, .783), (FX - .45, .73), (FX - .492, .525)], s * .70, s * .985, M_BUMP); box('FenderBarF_' + sn, (FX - .30, s * .55, .758), (.05, .42, .03), M_DARK, .004)
    fender('FenderD_' + sn, DX, s * .505, s * .985, .435, 12, 168); box('FenderBarD_' + sn, (DX, s * .50, .836), (.06, .16, .03), M_DARK, .004)
    box('MudFlap1_' + sn, (DX - .46, s * .745, .33), (.010, .46, .34), M_SEAL, .003, 1); box('MudFlapBar1_' + sn, (DX - .46, s * .55, .48), (.025, .50, .025), M_DARK, .004)
    box('Frame_' + sn, ((2.47 + FE) / 2, s * .37, .645), (2.47 - FE, .07, .17), M_CHAS, .008, 1)
    box('SubFrame_' + sn, ((1.05 + FE + .10) / 2, s * .37, .785), (.95 - FE, .07, .13), M_CHAS, .006, 1)
for i, x in enumerate((2.36, 1.12, .20, -.70, FE + .20)): box('FrameCross%d' % i, (x, 0, .64), (.08, .70, .10), M_CHAS, .006, 1)
cyl('DriveAxle_Mid', (DX, 0, RW), .055, .60, 'y', M_DARK, 32); cyl('Differential', (DX, 0, RW), .15, .26, 'x', M_DARK, 40, .05); box('Motor', (DX + .30, 0, .44), (.36, .40, .28), M_FRM, .05, 3)
box('BatteryBox_L', (.22, .665, .545), (1.50, .50, .36), M_PLAST, .03, 3); box('BatteryBox_LArm', (.22, .46, .69), (1.2, .18, .06), M_CHAS, .006); box('BatteryBox_LLid', (.22, .916, .55), (1.34, .012, .28), M_TRIM, .006)
box('ChargeFlap', (.78, .9235, .58), (.20, .008, .16), M_BODY, .003, 1); text('ChargeFlap_Word', 'CHARGE', (.78, .9275, .60), .032, 'y+', M_SBLK, .0012); ik('ChargeFlap', (.78, .93, .58))
box('SideBox_R', (.52, -.665, .545), (.90, .50, .36), M_PLAST, .03, 3); box('SideBox_RArm', (.52, -.46, .69), (.7, .18, .06), M_CHAS, .006); box('SideBox_RLid', (.52, -.916, .55), (.78, .012, .28), M_TRIM, .006); box('SideBox_RCatch', (.52, -.925, .65), (.12, .014, .03), M_STEEL, .005)
for s, sn, f in SIDES:
    for i in range(3): box('SideHi%d_%s' % (i, sn), (.22 + .25 * (i - 1) + (.28 if s < 0 else -.2), s * .9235, .445), (.21, .006, .05), M_HVR if i % 2 else M_HVW, .001, 1)
# chocks in an open tray, right side behind the box
CX = -.28; box('ChockTray', (CX, -.80, .375), (.62, .28, .02), M_DARK, .004); box('ChockTray_Back', (CX, -.668, .49), (.62, .016, .25), M_DARK, .003, 1); box('ChockTray_Arm', (CX, -.52, .59), (.10, .30, .05), M_CHAS, .006)
for e in (-.302, .302): box('ChockTray_End%d' % (e > 0), (CX + e, -.80, .425), (.016, .28, .11), M_DARK, .003, 1)
for i, x in enumerate((CX + .15, CX - .15)):
    prism('Chock_%d' % i, [(x - .115, .386), (x + .115, .386), (x + .115, .42), (x + .05, .535), (x + .015, .55), (x - .02, .53), (x - .115, .42)], -.89, -.73, M_YEL, .012, 2); tube('Chock_%d_Grip' % i, [(x, -.855, .535), (x, -.85, .585), (x, -.81, .60), (x, -.77, .585), (x, -.765, .535)], .009, M_SBLK)
    for k in range(3): box('Chock_%d_Rib%d' % (i, k), (x - .06 + .05 * k, -.81, .3855), (.02, .145, .006), M_SBLK, .001, 1)
    ik('Chock_%d' % i, (x, -.81, .60))
ik('ChockPlace_0', (DX + RW + .06, DYO, 0)); ik('ChockPlace_1', (DX - RW - .06, DYO, 0)); ik('Stand_Chocks', (CX, -1.50, 0)); ik('Stand_ChockPlace', (DX, 1.65, 0))
# the rear bar with the lamps: behind everything
box('RearBar', (-2.80, 0, .47), (.10, 1.94, .12), M_CHAS, .010); box('RearPlate', (-2.854, 0, .47), (.012, .52, .11), M_NAME, .002, 1)
for s, sn, f in SIDES:
    box('RearBarArm_' + sn, ((FE + .12 - 2.79) / 2, s * .37, .52), (FE + .12 + 2.79, .068, .10), M_CHAS, .006); box('RearDrop_' + sn, (FE + .05, s * .37, .585), (.16, .066, .20), M_CHAS, .006)
    box('RearLampBox_' + sn, (-2.83, s * .79, .615), (.08, .36, .18), M_PLAST, .012)
    for nm, y, mt, w in (('TailBrake', .70, M_RLENS, .11), ('IndicatorR', .815, M_AMB, .09), ('Reverse', .91, M_WHT, .07)): box('%s_%s_Lens' % (nm, sn), (-2.8725, s * y, .615), (.008, w, .12), mt, .003)
    for i in range(2): box('RearHi%d_%s' % (i, sn), (-2.8525, s * (.42 + .22 * i), .47), (.006, .20, .06), M_HVR if i % 2 else M_HVW, .001, 1)
    box('SideMarker_%s1_Base' % sn, (-1.88, s * .985, .86), (.12, .05, .05), M_PLAST, .008); box('SideMarker_%s1_Lens' % sn, (-1.88, s * 1.012, .86), (.09, .012, .035), M_AMB, .003)
box('RearFog_Base', (-2.83, .46, .58), (.06, .14, .12), M_PLAST, .010); box('RearFog_Lens', (-2.8625, .46, .58), (.008, .11, .085), M_RLENS, .003)
# #endregion

# #region ---- what both bodies use
def solidx(name, pts, x0, x1, r, m, smooth=40, seg=6):
    """A convex outline (y, z) pushed from x0 to x1 with domed, rounded ends (tanks, drums)."""
    cy = (min(p[0] for p in pts) + max(p[0] for p in pts)) / 2; cz = (min(p[1] for p in pts) + max(p[1] for p in pts)) / 2; hy = max(p[0] for p in pts) - cy; hz = max(p[1] for p in pts) - cz
    L = [(x0 + r - r * math.sin(math.pi / 2 * k / seg), r * (1 - math.cos(math.pi / 2 * k / seg))) for k in range(seg, -1, -1)]
    R = [(x1 - (x - x0), g) for x, g in reversed(L)]; bm = bmesh.new(); rings = []
    for x, g in L + R: rings.append([bm.verts.new((x, cy + (p[0] - cy) * (hy - g) / hy, cz + (p[1] - cz) * (hz - g) / hz)) for p in pts])
    n = len(pts)
    for A, B in zip(rings, rings[1:]):
        for k in range(n): bm.faces.new((A[k], A[(k + 1) % n], B[(k + 1) % n], B[k]))
    for rg in (rings[0], rings[-1]): bm.faces.new(rg)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); return K.finish(K.link(name, bm), m, smooth)
def bandx(name, pts, x, w, g, m):
    """A strap round the outline: g outside it, 2 mm into it."""
    bm = bmesh.new(); cy = (min(p[0] for p in pts) + max(p[0] for p in pts)) / 2; cz = (min(p[1] for p in pts) + max(p[1] for p in pts)) / 2; hy = max(p[0] for p in pts) - cy; hz = max(p[1] for p in pts) - cz; n = len(pts)
    f = lambda xx, d: [bm.verts.new((xx, cy + (p[0] - cy) * (hy + d) / hy, cz + (p[1] - cz) * (hz + d) / hz)) for p in pts]
    Q = [f(x - w / 2, -.004), f(x - w / 2, g), f(x + w / 2, g), f(x + w / 2, -.004)]
    for i in range(4):
        A, B = Q[i], Q[(i + 1) % 4]
        for k in range(n): bm.faces.new((A[k], A[(k + 1) % n], B[(k + 1) % n], B[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); return K.finish(K.link(name, bm), m, 40)
def reel(n, c, hr, r0, layers, cnt, m):
    """A hose reel about y: drum, flanges, turns of hose (HoseTurn_<n>_00 .. = outermost last). Returns (hose metres, flange radius, half width)."""
    w = cnt * (2 * hr + .002) + .004; rl = [r0 + hr + k * (2 * hr - .003) for k in range(layers)]; RF = rl[-1] + hr + .022; hw = w / 2 + .006; x, y, z = c
    cyl(n + '_Drum', c, r0, w + .012, 'y', M_DARK, 48)
    for e in (-1, 1):
        cyl(n + '_Flange%d' % (e > 0), (x, y + e * hw, z), RF, .010, 'y', M_FRM, 72, .003); torus(n + '_Lip%d' % (e > 0), (x, y + e * hw, z), RF - .004, .008, 'y', M_FRM, 72, 10)
        for j in range(6): turn(box(n + '_Rib%d_%d' % (e > 0, j), (x, y + e * (hw + .008), z + RF * .55), (.026, .010, RF * .62), M_FRM, .003, 1), c, 'y', 60 * j)
    cyl(n + '_Axle', c, .025, 2 * hw + .09, 'y', M_STEEL, 32); piv(n, c); i = 0; L = 0
    for k, r in enumerate(rl):
        t = torus('turn_%s%d' % (n, k), (0, 0, 0), r, hr - .0008, 'y', m, 64, 14); ALL.remove(t)
        for j in range(cnt):
            o = bpy.data.objects.new('HoseTurn_%s_%02d' % (n.split('_')[-1], i), t.data); bpy.context.collection.objects.link(o); o.location = (x, y + (j - (cnt - 1) / 2) * (2 * hr + .002), z); ALL.append(o); i += 1; L += 2 * math.pi * r
        bpy.data.objects.remove(t, do_unlink=True)
    return L, RF, hw + .012
def rail(n, pts, m=None, r=.019): return tube(n, pts, r, m or M_YEL, bez=False)
def gauge(n, c, r, face):
    ax = face[0]; d = 1 if face[1] == '+' else -1; cyl(n, c, r, .05, ax, M_STEEL, 40, .004); v = lambda t: tuple(a + (d * t if 'xyz'[i] == ax else 0) for i, a in enumerate(c))
    cyl(n + '_Dial', v(.0255), r * .88, .004, ax, M_SWHT, 40); turn(box(n + '_Needle', tuple(a + (b - a) for a, b in zip(c, (v(.0295)[0], v(.0295)[1], c[2] + r * .28))), (.008 if ax == 'x' else r * .08, r * .08 if ax == 'x' else .008, r * .72), M_SRED, .0005, 1), v(.0295), ax, 40)
def finish_truck(groups, groupof_extra, sweeps, moving, extra_json):
    """Origin is under the middle already; checks, pictures, joining, saving, the points file."""
    K.uv_cube(); S.drop_lofts()
    if '--nochk' not in sys.argv:
        K.checks(TAG, ground_ok=('Tyre',), overhang_skip_prefix=('',))
        for nm, pv, ax, dg, sk in [('DoorDriver', (2.590, .985, 1), 'z', (-15, -40, -68), ())] + sweeps: h = K.sweep(nm, pv, ax, dg, sk); print(TAG, 'SWEEP', nm, len(h), sorted(h)[:8])
        for k in ('FL', 'FR'):
            h = set(); p = (FX, .68 if k[1] == 'L' else -.68, 0)
            for dg in (-38, 38):
                o = bpy.data.objects['Tyre_' + k]; Rm = Matrix.Translation(p) @ Matrix.Rotation(math.radians(dg), 4, 'Z') @ Matrix.Translation(-Vector(p)); t = BVHTree.FromPolygons([Rm @ v.co for v in o.data.vertices], [tuple(q.vertices) for q in o.data.polygons])
                for o2 in ALL:
                    if k not in o2.name and t.overlap(K.bvh(o2)): h.add((o2.name, dg))
            print(TAG, 'STEER', k, sorted(h)[:12])
        for pre, vec, steps, skip in moving:
            for q in steps:
                T = {}
                for o in ALL:
                    d = Vector(vec) * (q if o.name.startswith(pre) else 0); T[o.name] = BVHTree.FromPolygons([o.matrix_world @ v.co + d for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
                h = [(a, b) for a in T if a.startswith(pre) for b in T if not b.startswith(pre) and not b.startswith(skip) and T[a].overlap(T[b])]; print(TAG, 'MOVE', pre[0], q, len(h), h[:8])
    GR = ['DoorDriver', 'DoorPassenger', 'SteeringWheel', 'Pedal_Accel', 'Pedal_Brake', 'WiperL', 'WiperR', 'Chock_0', 'Chock_1'] + groups
    def groupof(o):
        n = o.name
        if n.startswith(('Tyre_', 'HoseTurn')) or n == 'Body' or n.endswith('_Lens') or n == 'BeaconLens' or 'NamePanel' in n: return None
        for k in ('FL', 'FR', 'RL', 'RR'):
            if n.startswith(('Rim_' + k, 'WheelDisc_' + k, 'Hub_' + k, 'HubCap_' + k, 'Nut_' + k)): return 'Rim_' + k
            if n.startswith(('Spindle_' + k, 'SpindleDrum_' + k)): return 'Spindle_' + k
        g = next((g for g in sorted(GR, key=len, reverse=True) if n.startswith(g)), None)
        if g: return g + '_Glass' if n.endswith('_Glass') else g + '_Parts' if g.startswith(('DoorD', 'DoorP')) and n != g else g
        g = groupof_extra(n)
        if g: return g
        if n.endswith('_Glass'): return 'GlassFixed'
        if n.startswith(('Seat', 'Belt')): return 'Seats'
        if n.startswith(('Dash', 'CabM', 'SunV', 'GrabIn', 'Steer', 'Stalk', 'Foot', 'Gear', 'ParkB', 'Firewall', 'Cowl', 'Locker_Back', 'Clip', 'RoofL', 'CabLiner', 'Floor', 'EngineHump', 'BPillar')) and not n.startswith('SteerArm'): return 'CabFittings'
        if n.startswith(('Frame', 'SubFrame', 'Axle', 'Leaf', 'Hanger', 'Drive', 'Fender', 'Mud', 'Diff', 'Motor', 'SteerArm', 'RearBar', 'RearDrop', 'RearLamp', 'RearHi', 'RearFog', 'Side', 'Battery', 'Charge', 'ChockTray')): return 'Underside'
        return 'OuterFittings'
    if '--pic' in sys.argv:
        OUT = sys.argv[sys.argv.index('--pic') + 1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.use_gtao = True; sc.eevee.taa_render_samples = 8
        w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (.6, .68, .8, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = .8; sc.world = w; K.glass(M_GLASS, .25)
        sd = bpy.data.lights.new('S', 'SUN'); sd.energy = 3.5; so = bpy.data.objects.new('S', sd); sc.collection.objects.link(so); so.rotation_euler = (.8, .1, .6)
        cd = bpy.data.cameras.new('C'); bc = bpy.data.objects.new('C', cd); sc.collection.objects.link(bc); sc.camera = bc; sc.render.resolution_x, sc.render.resolution_y = 560, 400
        for i, (loc, at, lens) in enumerate((((6.2, 5.2, 2.4), (.1, 0, 1.1), 42), ((-6.4, 4.6, 3.0), (-.3, 0, 1.1), 42), ((-4.6, -.6, 2.9), (-1.7, -.1, 1.3), 30), ((-5.0, -5.0, 2.6), (-.4, 0, 1.1), 40))):
            bc.location = loc; bc.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cd.lens = lens; cd.clip_start = .02; sc.render.filepath = os.path.join(OUT, 'p%d.png' % i); bpy.ops.render.render(write_still=True)
        sys.exit(0)
    GLB = os.path.join(ART, KEY + '.glb'); ZF = os.path.join(ART, 'zfight-check.json'); BL = os.path.join(ART, NAME + '.blend')
    K.save_export(ROOT, BL, GLB, ZF, TAG, NAME); G = {}; print(TAG, 'LEFT', open(ZF).read()[:2500].replace(chr(10), ' ') if os.path.exists(ZF) else '')
    for o in ALL:
        g = groupof(o)
        if g: G.setdefault(g, []).append(o)
    for g, l in G.items(): l[0].name = 'tmp_' + g; CK.join(g, l)
    for o in ALL: assert o.data.materials and all(o.data.materials), o.name; assert {p.material_index for p in o.data.polygons} == set(range(len(o.data.materials))), 'empty slot ' + o.name
    last = K.save_export(ROOT, BL, GLB, ZF, TAG + ' joined', NAME + 'Game'); ue = lambda v: [round(v[0] * 100, 1), round(-v[1] * 100, 1), round(v[2] * 100, 1)]
    tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in ALL); lo = [min(K.bbox(o)[0][i] for o in ALL) for i in range(3)]; hi = [max(K.bbox(o)[1][i] for o in ALL) for i in range(3)]
    D = {'note': 'points in the model root, Unreal cm (Blender (x,y,z) m = (x,-y,z) * 100)', 'size_m': [round(hi[i] - lo[i], 3) for i in range(3)], 'x_m': [round(lo[0], 3), round(hi[0], 3)], 'wheelbase': FX - DX, 'tyre_radius_cm': RW * 100}
    D.update(extra_json); D.update({'points_cm': {e.name: ue(e.location) for e in IKS}, 'parts': sorted(o.name for o in ALL), 'materials': sorted({m.name for o in ALL for m in o.data.materials}), 'tris': tris, 'flicker': last})
    json.dump(D, open(os.path.join(ART, KEY + '-parts.json'), 'w'), indent=1)
    json.dump({'points': {e.name: [round(c, 4) for c in e.location] for e in IKS}}, open(os.path.join(ROOT, 'Data', KEY + '-riders.json'), 'w'), indent=1)
    print('DONE %s: %d parts, %d points, %d tris, size %s x %.3f..%.3f, %s' % (TAG, len(ALL), len(IKS), tris, D['size_m'], lo[0], hi[0], last))
# #endregion
