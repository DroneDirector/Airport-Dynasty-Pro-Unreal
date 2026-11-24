# De-icing truck (to-do 212): our own two-axle de-icer for the A320. Tanks for water, type I and type IV, heater, pump panel, ground hose, a turret behind the cab with a three-piece
# telescoping boom that lies back over the tanks, and the glass spray cabin at its end standing over the low rear deck.
# Axes: x forward, y left, z up; metres; origin under the middle. Driver front left.   blender -b --python Art/Deicer/make_deicer.py [-- --pic DIR]
import os, sys, math
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, 'Tools'), os.path.join(ROOT, 'Art', 'CleaningKit')]
import bpy, bmesh, json
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
import vehicle_kit as K
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, ALL, IKS
NAME, KEY, TAG, PFX, DASHWORD, MAIN = 'Deicer', 'deicer', 'DI', 'DI_', '', (.86, .47, .02)
A_ = os.path.join(ROOT, 'Art'); ART = os.path.join(A_, NAME); TEX = os.path.join(A_, 'PushbackTug', 'Textures'); OWN = os.path.join(A_, 'PassengerStairs', 'Textures'); CART = os.path.join(A_, 'BaggageCart', 'Textures'); MINE = os.path.join(ART, 'Textures')
K.reset()
import make_cleaningkit as CK
import vehicle_skin as S
from vehicle_skin import rawprism, boole, patch, grow, rnd, ip
CK.join.__globals__['K'] = K
for _o in list(ALL): bpy.data.objects.remove(_o)
ALL.clear(); IKS.clear()
FX, DX, RW, TW, FLOOR, CABB = 3.50, -1.55, .48, .27, 1.10, 3.10
SRC = lambda *p: open(os.path.join(ROOT, *p), encoding='utf-8').read()
_t = SRC('Tools', 'truck_cab.py').split('# #region ----'); exec(compile('# ' + _t[1], 'truck_cab materials', 'exec'))
SIDES = ((1, 'L', 'y+'), (-1, 'R', 'y-')); _s = _t[3]; exec(compile(_s[_s.index('def seat('):_s.index("seat('SeatDriver'")], 'truck_cab seat', 'exec')); exec(compile('# ' + _t[4], 'truck_cab wheels', 'exec'))
_st = SRC('Tools', 'small_truck.py'); _ps = SRC('Art', 'PassengerStairs', 'make_passengerstairs.py')
exec(compile(_st[_st.index('def solidx('):_st.index('def rail(')], 'small_truck bodies', 'exec'))
exec(compile(_ps[_ps.index('def paint('):_ps.index('M_DISP = P(')].replace('PS_', 'DI_').replace("K.photo(M_DIAL, OWN, 'Dials'", "K.photo(M_DIAL, MINE, 'DialsDI'"), 'stairs materials', 'exec'))      # photo sets everywhere
exec(compile(_ps[_ps.index('def decal('):_ps.index('# #endregion', _ps.index('def revx('))], 'stairs helpers', 'exec'))
M_DISP = P('DI_Screens', (.02, .06, .08)); K.photo(M_DISP, MINE, 'ScreenDI', metal=0.0)
M_TANK = P('DI_TankSteel', (.7, .71, .72)); paint(M_TANK, (.74, .76, .77), 1.0, .5); M_BOOM = P('DI_Boom_Paint', MAIN); paint(M_BOOM, (.88, .50, .02), .8, .7)
M_GREEN, M_ORNG, M_BLUE = (P('DI_Fluid' + n, c, 0, .2) for n, c in (('IV', (.05, .5, .12)), ('I', (.9, .38, .03)), ('W', (.35, .6, .8)))); [paint(m, c, .2, .2) for m, c in ((M_GREEN, (.05, .5, .12)), (M_ORNG, (.9, .38, .03)), (M_BLUE, (.3, .55, .8)))]
CUV = []
def AFTER_UV():
    for e in CUV:
        o, ctr, w, h, a0, a1 = e[:6]; U = Vector(e[6] if len(e) > 6 else (0, -1, 0)); uv = o.data.uv_layers.active.data
        for p in o.data.polygons:
            for li in p.loop_indices: d = o.data.vertices[o.data.loops[li].vertex_index].co - ctr; uv[li].uv = (a0 + (a1 - a0) * (d.dot(U) / w + .5), d.z / h + .5)

# #region ---- cab: tall, flat face, deep screen, roof window (the driver sees the boom), lamps stacked in the corners
XFt = [(.42, 4.84), (.60, 4.92), (.85, 4.94), (1.45, 4.94), (1.72, 4.915), (2.35, 4.80), (2.55, 4.74), (2.65, 4.66), (2.71, 4.52), (2.76, 4.2)]; XRt = [(.42, CABB), (2.55, CABB), (2.65, CABB + .03), (2.71, CABB + .09), (2.76, CABB + .3)]
WHt = [(.42, 1.15), (.60, 1.18), (1.6, 1.185), (2.35, 1.15), (2.55, 1.10), (2.65, 1.02), (2.71, .86), (2.76, .3)]; LNt = [(.42, .22), (1.72, .24), (2.55, .30), (2.76, .4)]
ZSt = [.42, .48, .55, .62, .70, .78, .86, .94, 1.02, 1.10, 1.18, 1.26, 1.34, 1.42, 1.50, 1.58, 1.66, 1.74, 1.82, 1.90, 1.98, 2.06, 2.14, 2.22, 2.30, 2.37, 2.44, 2.50, 2.55, 2.59, 2.625, 2.65, 2.67, 2.69, 2.705, 2.717, 2.727, 2.735]
S.setup(XFt, XRt, WHt, LNt, ZSt, 2.56, [M_BODY, M_MAIN, M_BUMP, M_LIN, M_TRIM, M_SEAL], M_GLASS, M_SEAL, n=4.6, rr=.10)
SH = .036; OY = (.5, 1.6); XB = ('x', 4.2, 5.6); XH = ('x', 4.70, 5.6); ZC, DIN = FLOOR + .575, 1.105
shell = S.loft('Body', 0.0); inner = S.loft('inner', SH, bot=.5, mi=3)
for p in inner.data.polygons:
    if p.center.z < 1.75: p.material_index = 4
boole(shell, inner, 'DIFFERENCE', False)
for p in shell.data.polygons:
    c = p.center
    if p.material_index == 0 and abs(p.normal.z) < .9: p.material_index = 1 if c.z > 2.40 else 2 if c.z < 1.02 else 0
boole(shell, rawprism('c', [(2.0, .2), (4.09, .2), (4.09, 1.062), (2.0, 1.062)], 'y', -2, 2), 'DIFFERENCE', False)
for s in (-1, 1): boole(shell, rawprism('c', [(4.0, .40), (4.64, .40), (4.64, 1.062), (4.0, 1.062)], 'y', s * .84, s * 1.5), 'DIFFERENCE', False)
DOORF = rnd([(3.38, 1.125), (4.67, 1.125), (4.67, 1.70), (4.555, 2.50), (3.38, 2.50)], [.06, .06, .12, .10, .07]); GAP = .004; shell0 = S.copy(shell, 'shell0')
def opening(name, pts, axis, a, b):
    pan = S.copy(shell0, name); boole(pan, rawprism('c', grow(pts, -GAP), axis, a, b), 'INTERSECT', False); boole(shell, rawprism('c', pts, axis, a, b), 'DIFFERENCE', False); return pan
doorD = opening('DoorDriver', DOORF, 'y', *OY); doorP = opening('DoorPassenger', DOORF, 'y', -OY[1], -OY[0]); S.rm(shell0)
WSC = rnd([(-1.05, 1.71), (1.05, 1.71), (1.0, 2.48), (-1.0, 2.48)], [.06, .06, .09, .09]); WRO = rnd([(3.55, -.62), (4.35, -.62), (4.35, .62), (3.55, .62)], .07); WD = rnd([(3.47, 1.66), (4.585, 1.60), (4.48, 2.42), (3.47, 2.42)], .06); WRE = rnd([(-.5, 1.86), (.5, 1.86), (.5, 2.38), (-.5, 2.38)], .06)
for pts, ax, a, b in ((WSC, *XB), (WRO, 'z', 2.5, 3.0), (WRE, 'x', 2.9, 3.4)): boole(shell, rawprism('c', pts, ax, a, b), 'DIFFERENCE', False)
for o, s in ((doorD, 1), (doorP, -1)): boole(o, rawprism('c', WD, 'y', s * OY[0], s * OY[1]), 'DIFFERENCE', False)
HLZ = (1.02, 1.17, 1.30); HF = rnd([(.79, .93), (1.00, .93), (1.00, 1.375), (.79, 1.375)], .06); HY = .895
for s in (-1, 1): boole(shell, rawprism('c', [(s * y, z) for y, z in HF], *XB), 'DIFFERENCE', False)
for o in (shell, doorD, doorP):
    used = sorted({p.material_index for p in o.data.polygons}); ms = [o.data.materials[i] for i in used]; ix = [used.index(p.material_index) for p in o.data.polygons]; o.data.materials.clear()
    for m in ms: o.data.materials.append(m)
    for p, i in zip(o.data.polygons, ix): p.material_index = i; p.use_smooth = True
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(32); ALL.append(o) if o not in ALL else None
S.pane('Windscreen', WSC, *XB); S.pane('RearWindow', WRE, 'x', 2.9, 3.4); S.pane('RoofWindow', WRO, 'z', 2.5, 3.0); S.pane('DoorDriver_Window', WD, 'y', *OY); S.pane('DoorPassenger_Window', WD, 'y', -OY[1], -OY[0])
def unit(nm, c, r, mt, d=1):
    """A lamp: parabola with rings, emitter lens in its middle, collar, fins; d = 1 looks forwards, -1 back."""
    f = lambda pr: [(a, b * d) for a, b in pr]
    revx(nm + '_Reflector', f([(.012, -.004)] + [(r * k / 8, .052 * (k / 8) ** 2 + (.0015 if k % 2 else 0)) for k in range(2, 9)] + [(r + .004, .054), (r + .004, -.004), (.012, -.004)]), c, M_CHROME)
    revx(nm + '_Lens', f([(0, .030), (r * .16, .0285), (r * .30, .023), (r * .36, .015), (r * .36, -.0065), (0, -.0065)]), c, mt); revx(nm + '_Collar', f([(r * .30, -.001), (r * .46, -.001), (r * .46, .010), (r * .30, .010), (r * .30, -.001)]), c, M_PLAST)
    for k in range(8): a = math.pi / 4 * k; box('%s_Fin%d' % (nm, k), (c[0] + .030 * d, c[1] + r * .75 * math.cos(a), c[2] + r * .75 * math.sin(a)), (.012, .006, .006), M_CHROME, .001, 1)
for s, sn, f in SIDES:
    h = [(s * y, z) for y, z in HF]; n = 'DoorDriver' if s > 0 else 'DoorPassenger'
    patch('Headlamp_%s_Bezel' % sn, grow(h, .012), *XH, M_PLAST, -.004, .036, hole=grow(h, -.010)); patch('Headlamp_%s_Cover_Glass' % sn, grow(h, -.006), *XH, M_GLASS, .002, .007); patch('Headlamp_%s_Pan' % sn, grow(h, -.004), *XH, M_PLAST, .130, .143, smooth=30); patch('Headlamp_%s_Tunnel' % sn, grow(h, .004), *XH, M_PLAST, .030, .145, hole=grow(h, -.008))
    for nm, z, mt, r in (('LowBeam', HLZ[0], M_WHT, .064), ('HighBeam', HLZ[1], M_WHT, .064), ('IndicatorF', HLZ[2], M_AMB, .048)):
        x0 = S.surf(HY + r, z).x - .092; c = (x0, s * HY, z); unit('%s_%s' % (nm, sn), c, r, mt); revx('Headlamp_%s_Stem%s' % (sn, nm), [(0, -.0035), (.010, -.0035), (.010, -.075), (0, -.075)], c, M_PLAST)
    for i, (x0, x1) in enumerate(((3.42, 3.82), (3.83, 4.22), (4.23, 4.63))): patch(n + '_HiVis%d' % i, [(x0, 1.20), (x1, 1.20), (x1, 1.28), (x0, 1.28)], 'y', s * OY[0], s * OY[1], M_HVR if i % 2 else M_HVW, -.0025, .006)
    patch('CabSideMarker_' + sn, rnd([(3.22, 1.22), (3.30, 1.22), (3.30, 1.29), (3.22, 1.29)], .014, 3), 'y', s * .5, s * 1.6, M_AMB, -.009, .004)
patch('Grille_Field', rnd([(-.66, 1.00), (.66, 1.00), (.70, 1.34), (-.70, 1.34)], .05), *XB, M_PLAST, -.004, .02)
for i in range(9): patch('Grille_Slat%d' % i, [(-.60 + .150 * i - .05, 1.04), (-.60 + .150 * i + .05, 1.04), (-.60 + .150 * i + .05, 1.30), (-.60 + .150 * i - .05, 1.30)], *XB, M_DARK, -.014, .002)
patch('NamePanel_Front', rnd([(-.62, 1.46), (.62, 1.46), (.62, 1.58), (-.62, 1.58)], .02, 3), *XB, M_NAME, -.004, .006); patch('PlateFront', [(-.26, .80), (.26, .80), (.26, .91), (-.26, .91)], *XB, M_NAME, -.005, .006)
box('FrontBumper', (4.93, 0, .60), (.14, 1.70, .20), M_BUMP, .04, 3); decal(box('FrontBumper_Chev', (5.0005, 0, .60), (.004, 1.50, .09), M_CHEV), 'x', M_BUMP); box('FrontTowEye', (4.95, 0, .49), (.10, .05, .05), M_YEL, .012)
def inside(name, pts, axis, a, b, d, m): o = rawprism(name, pts, axis, a, b); boole(o, S.LF(d), 'INTERSECT'); return K.finish(o, m, 32)
inside('Floor', [(3.08, 1.066), (5.2, 1.066), (5.2, FLOOR), (3.08, FLOOR)], 'y', -1.3, 1.3, .033, M_TRIM); inside('Firewall', [(-1.3, 1.071), (1.3, 1.071), (1.3, ZC - .003), (-1.3, ZC - .003)], 'x', 4.805, 4.821, .033, M_PLAST); inside('Cowl', [(-1.3, ZC - .011), (1.3, ZC - .011), (1.3, ZC + .005), (-1.3, ZC + .005)], 'x', 4.52, 5.4, .034, M_PLAST)
for s in (-1, 1): boole(bpy.data.objects['Cowl'], rawprism('c', [(4.0, 1.5), (4.70, 1.5), (4.70, 1.9), (4.0, 1.9)], 'y', s * DIN, s * 1.5), 'DIFFERENCE', False)
K.finish(rawprism('CabMat', [(3.2, -1.06), (4.79, -1.06), (4.79, 1.06), (3.2, 1.06)], 'z', FLOOR - .004, FLOOR + .008), M_MAT, 30); box('RoofLight_Base', (3.40, 0, 2.660), (.12, .30, .06), M_PLAST, .008); box('RoofLight_Lamp', (3.40, 0, 2.627), (.10, .27, .010), M_WHT, .003)
for s, n in ((1, 'Driver'), (-1, 'Passenger')):
    box('Step%s_Riser' % n, (4.365, s * .848, .74), (.53, .016, .64), M_BUMP, .003, 1); box('Step%s_Top' % n, (4.365, s * 1.00, 1.052), (.53, .29, .018), M_BUMP, .002, 1)
    for e, x in ((0, 4.099), (1, 4.632)): box('Step%s_End%d' % (n, e), (x, s * 1.00, .74), (.016, .30, .64), M_BUMP, .003, 1)
    for k, (z, y0) in enumerate(((.45, .92), (.78, .88))): box('Step%s_Tread%d' % (n, k), (4.365, s * (y0 + 1.16) / 2, z), (.517, 1.16 - y0, .035), M_TREAD, .006, 2); box('Step%s_Edge%d' % (n, k), (4.365, s * 1.145, z + .0185), (.50, .028, .006), M_NOSE, .002, 1)
    ik('Step_Door' + n, (4.36, s * 1.06, .80)); ik('Step_Door%s_Low' % n, (4.36, s * 1.08, .47))
ox, oz = 3.19, FLOOR - .575; Xo = lambda x: x + ox; Zo = lambda z: z + oz; DY = .60
for s, n in ((1, 'DoorDriver'), (-1, 'DoorPassenger')):
    a, b = s * OY[0], s * OY[1]; sd = 'L' if s > 0 else 'R'
    patch(n + '_Trim', grow(DOORF, -.026), 'y', a, b, M_DASH, .034, .054, hole=grow(WD, .012)); box(n + '_Armrest', (4.02, s * 1.110, 1.53), (.55, .055, .06), M_DASH, .02, 3); box(n + '_Pocket', (4.02, s * 1.120, 1.27), (.80, .040, .15), M_DASH, .015)
    box(n + '_HandleIn_Cup', (4.42, s * 1.126, 1.545), (.13, .018, .06), M_PLAST, .008); box(n + '_HandleIn', (4.41, s * 1.115, 1.55), (.085, .012, .022), M_CHROME, .004); box(n + '_Switches', (4.17, s * 1.110, 1.562), (.14, .045, .012), M_PLAST, .004)
    for k in range(3): box(n + '_Switch%d' % k, (4.13 + .04 * k, s * 1.110, 1.570), (.030, .022, .010), M_DARK, .003)
    patch(n + '_HandleCup', rnd([(3.43, 1.465), (3.57, 1.465), (3.57, 1.535), (3.43, 1.535)], .025, 4), 'y', a, b, M_PLAST, -.002, .008); box(n + '_Handle', (3.505, s * 1.206, 1.502), (.15, .018, .030), M_PLAST, .008, 3)
    for e, x in enumerate((3.442, 3.568)): box(n + '_Handle_P%d' % e, (x, s * 1.194, 1.502), (.02, .028, .022), M_PLAST, .004)
    for i, z in enumerate((1.30, 1.56)): box(n + '_Hinge%d' % i, (4.615, s * 1.122, z), (.07, .035, .08), M_DARK, .004)
    box(n + '_MirrorFoot', (4.50, s * 1.165, 1.86), (.07, .07, .10), M_PLAST, .018, 3); box(n + '_MirrorArm', (4.515, s * 1.255, 1.89), (.05, .17, .06), M_PLAST, .018, 3); box(n + '_MirrorShell', (4.53, s * 1.365, 1.98), (.115, .19, .58), M_PLAST, .045, 4); box(n + '_MirrorGlass', (4.4695, s * 1.365, 2.07), (.010, .155, .31), M_MIR, .001, 1); box(n + '_MirrorGlass2', (4.4695, s * 1.365, 1.80), (.010, .155, .13), M_MIR, .001, 1)
    piv(n, (4.70, s * 1.180, 1.5)); ik('Handle_%s_Out' % n, (3.505, s * 1.216, 1.502)); ik('Handle_%s_In' % n, (4.41, s * 1.107, 1.55)); ik('Stand_%s' % n, (4.10, s * 1.85, 0))
    gs = [ip(WHt, z) for z in (1.36, 1.42, 1.98, 2.04)]; tube('GrabHandle_' + sd, [(3.30, s * (gs[0] + .004), 1.36), (3.30, s * (gs[1] + .062), 1.40), (3.30, s * (gs[2] + .062), 2.0), (3.30, s * (gs[3] + .004), 2.04)], .013, M_PLAST); ik('Grab_' + sd, (3.30, s * (gs[1] + .052), 1.70))      # flat side pillar behind the door
    for e, z in enumerate((1.36, 2.04)): box('GrabHandle_%s_F%d' % (sd, e), (3.30, s * (ip(WHt, z) + .004), z), (.046, .020, .058), M_PLAST, .008, 3)
    tube('GrabIn_' + sd, [(4.60, s * 1.06, 1.78), (4.575, s * 1.03, 1.83), (4.50, s * 1.01, 2.12), (4.49, s * 1.03, 2.18)], .012, M_PLAST); box('GrabIn_%s_F0' % sd, (4.63, s * 1.085, 1.77), (.08, .08, .06), M_PLAST, .012); box('GrabIn_%s_F1' % sd, (4.52, s * 1.075, 2.195), (.08, .11, .05), M_PLAST, .012); ik('GrabIn_' + sd, (4.54, s * 1.02, 2.0))
_d = _ps[_ps.index("inside('Dashboard'"):_ps.index("box('Console'")] + _ps[_ps.index('SC = Vector((Xo('):_ps.index('_a = _st.index(')]
for a, b in (('(2.0, .5), (2.51, .5), (2.51, 1.3), (2.0, 1.3)', '(4.0, 1.0), (4.70, 1.0), (4.70, 1.9), (4.0, 1.9)'), ('s * .895', 's * DIN'), ('.098, 0, .5)', '.098, 0, 1 / 6)'), ('.098, .5, 1)', '.098, 1 / 6, 2 / 6)'), ('Battery', 'Fuel'), ('Screen_Stair', 'Screen_Dash'), ('.19, .090, 0, 1)', '.19, .090, 0, .5)'), ('(-.80, -.25, .83)', '(-.95, -.25, .96)')): assert a in _d, a; _d = _d.replace(a, b)
exec(compile(_d, 'stairs dashboard', 'exec')); _a = _st.index("tilt(torus('SteeringWheel_Rim'"); exec(compile(_st[_a:_st.index("seat('SeatDriver'", _a)], 'small_truck wheel and pedals', 'exec'))
seat('SeatDriver', 3.96, DY, .50); seat('SeatPassenger', 3.96, -DY, .50); box('Console', (3.98, 0, FLOOR + .16), (.70, .34, .31), M_TRIM, .035, 4); box('Console_Top', (3.98, 0, FLOOR + .318), (.60, .28, .012), M_PLAST, .004)
for n, x, y, m in (('GearSelect', 4.16, .07, M_CHROME), ('PtoLever', 4.02, .07, M_YEL)): cyl('Console_%s_Boot' % n, (x, y, FLOOR + .327), .030, .018, 'z', M_SEAL, 32, .006); cyl(n, (x, y, FLOOR + .37), .008, .09, 'z', M_STEEL, 20); cyl(n + '_Knob', (x, y, FLOOR + .42), .020, .034, 'z', m, 28, .008); ik(n, (x, y, FLOOR + .43)); piv(n, (x, y, FLOOR + .33))
box('ParkBrake', (4.16, -.07, FLOOR + .337), (.06, .035, .03), M_RED, .008); ik('ParkBrake', (4.16, -.07, FLOOR + .355)); box('Locker_Back', (3.20, 0, 1.45), (.13, 1.30, .66), M_TRIM, .03, 3)
for s, n in ((1, 'D'), (-1, 'P')):
    box('Belt%s_Top' % n, (3.26, s * 1.105, 2.22), (.05, .06, .07), M_PLAST, .01); box('Belt%s_Web' % n, (3.26, s * 1.095, 1.77), (.046, .006, .86), M_BELT, .001, 1); box('Belt%s_Reel' % n, (3.26, s * 1.095, 1.28), (.10, .09, .16), M_PLAST, .015); box('Belt%s_Tongue' % n, (3.26, s * 1.088, 1.74), (.05, .012, .07), M_STEEL, .004); ik('BeltTongue_' + n, (3.26, s * 1.08, 1.74)); box('SunVisor_' + n, (4.50, s * .52, 2.555), (.17, .56, .020), M_LIN, .008, 3)
    for e in (.30, .74): box('SunVisor_%s_F%d' % (n, e > .5), (4.545, s * e, 2.59), (.05, .03, .075), M_PLAST, .006)
for s, n, py in ((1, 'L', .94), (-1, 'R', -.05)):
    pv = S.surf(py, 1.685, .006); piv('Wiper_' + n, tuple(pv)); cyl('Wiper%s_Hub' % n, tuple(pv), .024, .06, 'z', M_PLAST, 20, .004); tube('Wiper%s_Arm' % n, [tuple(S.surf(py, 1.70, .024)), tuple(S.surf(py - .22, 1.732, .030)), tuple(S.surf(py - .46, 1.765, .016))], .009, M_PLAST); tube('Wiper%s_Blade' % n, [tuple(S.surf(py - .05 - .10 * k, 1.745 + .004 * k, .003)) for k in range(9)], .011, M_SEAL); ik('WiperTip_' + n, tuple(S.surf(py - .85, 1.777, .003)))
def beacon(n, c):
    x, y, z = c; cyl(n + '_Base', (x, y, z + .02), .072, .06, 'z', M_PLAST, 48, .006); CK.lathez(n if n == 'BeaconLens' else n + '_Lens', [(0, z + .043), (.060, z + .043)] + [(.063 + (.003 if k % 2 else 0), z + .049 + .006 * k) for k in range(12)] + [(.056, z + .128), (.04, z + .140), (0, z + .146)], (x, y, 0), M_AMB, 64); cyl(n + '_Cap', (x, y, z + .146), .022, .012, 'z', M_PLAST, 24, .003)
box('Beacon_Mount', (3.40, 0, 2.735), (.14, 1.16, .03), M_PLAST, .010); beacon('BeaconLens', (3.40, .48, 2.745)); beacon('BeaconR', (3.40, -.48, 2.745)); ik('Beacon', (3.40, .48, 2.82))
# #endregion

# #region ---- wheels, axles, frame, rear bar with lamps
FYW, DYO, DYI = 1.0, 1.015, .70; FE = -4.50
for s, sn, f in SIDES:
    wheel('F' + sn, FX, s * FYW, s, .075); wheel('R' + sn, DX, s * DYO, s, -.075); wheel('R' + sn, DX, s * DYI, s, .075, '_In'); piv('Wheel_F' + sn, (FX, s * FYW, RW)); piv('Wheel_R' + sn, (DX, s * (DYO + DYI) / 2, RW)); piv('Steer_F' + sn, (FX, s * .80, RW))
    box('Spindle_F' + sn, (FX, s * .80, RW), (.12, .08, .22), M_DARK, .012); cyl('Spindle_F%s_Axle' % sn, (FX, s * .89, RW), .05, .18, 'y', M_STEEL, 28); cyl('SpindleDrum_F' + sn, (FX, s * .93, RW), .17, .10, 'y', M_DARK, 48, .01); box('SteerArm_F' + sn, (FX - .10, s * .77, RW - .05), (.18, .04, .03), M_DARK, .008)
    cyl('AxleBeamF_' + sn, (FX, s * .375, RW - .06), .05, .75, 'y', M_DARK, 28); box('AxleEndF_' + sn, (FX, s * .735, RW - .01), (.10, .05, .18), M_DARK, .010); box('LeafF_' + sn, (FX, s * .42, RW + .035), (1.0, .07, .05), M_DARK, .010); cyl('DriveAxle_' + sn, (DX, s * .55, RW), .065, .60, 'y', M_DARK, 32); cyl('DriveDrum_' + sn, (DX, s * .52, RW), .17, .06, 'y', M_DARK, 40, .01); box('LeafR_' + sn, (DX, s * .41, RW + .10), (1.20, .07, .07), M_DARK, .010)
    for e in (-.5, .5): box('HangerF_%s%d' % (sn, e > 0), (FX + e * .94, s * .42, .60), (.07, .075, .21), M_CHAS, .006); box('HangerR_%s%d' % (sn, e > 0), (DX + e * 1.14, s * .41, .63), (.08, .075, .16), M_CHAS, .006)
    fender('FenderF_' + sn, FX, s * .83, s * 1.165, .548, 52, 118); box('FenderBarF_' + sn, (FX - .10, s * .62, 1.017), (.05, .44, .03), M_DARK, .004); fender('FenderD_' + sn, DX, s * .555, s * 1.185, .548, 14, 166); box('FenderBarD_' + sn, (DX, s * .50, 1.022), (.06, .16, .03), M_DARK, .004); box('FenderPostD_' + sn, (DX, s * .41, .94), (.06, .06, .20), M_DARK, .004)
    box('MudFlap1_' + sn, (DX - .57, s * .865, .40), (.012, .60, .40), M_SEAL, .003, 1); box('MudFlapBar1_' + sn, (DX - .57, s * .60, .575), (.03, .60, .03), M_DARK, .004); box('MudFlapPost_' + sn, (DX - .57, s * .39, .60), (.05, .05, .10), M_DARK, .004)
    box('Frame_' + sn, ((4.60 + FE) / 2, s * .37, .74), (4.60 - FE, .08, .20), M_CHAS, .008, 1); box('FrameFrontArm_' + sn, (4.73, s * .37, .62), (.28, .07, .14), M_CHAS, .006); box('RearLampBox_' + sn, (-4.655, s * .93, .64), (.075, .46, .19), M_PLAST, .014)
    for nm, y, mt in (('TailBrake', .78, M_RLENS), ('IndicatorR', .93, M_AMB), ('Reverse', 1.08, M_WHT)): revx('%s_%s_Ring' % (nm, sn), [(.058, .002), (.058, -.018), (.064, -.018), (.068, .002), (.058, .002)], (-4.692, s * y, .64), M_CHROME); revx('%s_%s_Lens' % (nm, sn), [(0, -.016), (.030, -.014), (.050, -.009), (.0565, -.002), (.0565, .002), (0, .002)], (-4.692, s * y, .64), mt)
    for i in range(2): box('RearHi%d_%s' % (i, sn), (-4.6615, s * (.28 + .22 * i), .50), (.006, .20, .06), M_HVR if i % 2 else M_HVW, .001, 1)
    for i, x in enumerate((1.02, -2.25, -4.20)): z = .96 if i < 2 else .86; box('SideMarker_%s%d_Base' % (sn, i), (x, s * (1.175 if i < 2 else 1.19), z), (.11, .03, .045), M_PLAST, .006); box('SideMarker_%s%d_Lens' % (sn, i), (x, s * (1.195 if i < 2 else 1.21), z), (.085, .014, .032), M_AMB, .004)
for i, x in enumerate((4.50, 2.60, 1.0, -.3, -2.3, -4.40)): box('FrameCross%d' % i, (x, 0, .74), (.09, .70, .11), M_CHAS, .006, 1)
cyl('DriveAxle_Mid', (DX, 0, RW), .065, .56, 'y', M_DARK, 32); cyl('Differential', (DX, 0, RW), .20, .30, 'x', M_DARK, 40, .05); tube('PropShaft', [(DX + .13, 0, RW), (2.0, 0, .56)], .045, M_DARK, 8, False); box('Gearbox', (2.45, 0, .56), (.92, .44, .28), M_DARK, .05, 3); box('Engine', (3.40, 0, .80), (.90, .56, .46), M_DARK, .05, 3)
box('RearBar', (-4.63, 0, .50), (.06, 1.50, .14), M_CHAS, .010); box('RearPlate', (-4.6625, 0, .50), (.008, .30, .10), M_NAME, .002, 1); [box('RearBarDrop_%s' % sn, (-4.55, s * .37, .56), (.12, .07, .24), M_CHAS, .006) for s, sn, f in SIDES]; [box('RearLampArm_' + sn, (-4.61, s * .72, .56), (.05, .10, .10), M_CHAS, .006) for s, sn, f in SIDES]
box('ReverseHorn', (-4.55, -.15, .63), (.10, .12, .10), M_PLAST, .012); cyl('ReverseHorn_Cone', (-4.605, -.15, .64), .045, .03, 'x', M_DARK, 32, .004); ik('ReverseHorn', (-4.63, -.15, .64)); box('RearWorkLamp_Base', (-4.645, 0, .66), (.05, .14, .10), M_PLAST, .010); box('RearWorkLamp_Lens', (-4.672, 0, .66), (.012, .12, .08), M_WHT, .003)
cyl('FuelTank', (.10, .86, .66), .27, .90, 'x', M_ALU, 48, .03); [box('FuelTank_Band%d' % i, (x, .62, .80), (.05, .46, .10), M_CHAS, .006) for i, x in enumerate((-.20, .40))]; cyl('FuelTank_Cap', (.40, 1.06, .86), .045, .05, 'z', M_PLAST, 32, .006)
box('BatteryBox', (.10, -.84, .66), (.80, .50, .40), M_PLAST, .03, 3); box('BatteryBox_Arm', (.10, -.50, .74), (.6, .20, .08), M_CHAS, .006); box('BatteryBox_Lid', (.10, -1.096, .67), (.70, .012, .30), M_TRIM, .006)
# #endregion

# #region ---- heater module, pump module, tanks, rear deck
MZ0, MZ1, MY = .88, 2.30, 1.165
def mod(n, x0, x1, z1): o = box(n, ((x0 + x1) / 2, 0, (MZ0 + z1) / 2), (x1 - x0, 2 * MY, z1 - MZ0), M_BODY, .03, 3); [box(n + '_Feet%s%d' % (sn, i), (x, s * .37, .861), (.12, .10, .042), M_CHAS, .004) for s, sn, f in SIDES for i, x in enumerate((x0 + .15, x1 - .15))]; return o
H = mod('Heater', 1.50, 2.78, MZ1); U_ = mod('PumpHouse', .55, 1.44, 2.36)
HD = rnd([(1.60, 1.02), (2.68, 1.02), (2.68, 2.16), (1.60, 2.16)], .04); PD = rnd([(.62, 1.02), (1.37, 1.02), (1.37, 2.22), (.62, 2.22)], .04)
def recess(o, pts, s, d): [boole(o, rawprism('c', grow(pts, g), 'y', s * (MY - q), s * 1.5), 'DIFFERENCE', False) for g, q in ((-.03, d), (.004, .028))]; K.finish(o, M_BODY, 40)
recess(H, HD, 1, .42); recess(U_, PD, 1, .27); recess(U_, PD, -1, .56)
def door(n, pts, s, hx, word, m=M_BODY):
    xs = [p[0] for p in pts]; x0, x1 = min(xs), max(xs); K.finish(rawprism(n, pts, 'y', s * (MY - .026), s * (MY - .001)), m, 30); hs = 1 if hx < (x0 + x1) / 2 else -1; lx = x1 - .09 if hs > 0 else x0 + .09; box(n + '_Latch', (lx, s * (MY + .004), 1.62), (.07, .012, .13), M_PLAST, .004); box(n + '_LatchBar', (lx, s * (MY + .016), 1.62), (.022, .014, .10), M_STEEL, .004)
    for i, z in enumerate((1.18, 2.04)): cyl('%s_Hinge%d' % (n, i), (hx, s * (MY + .010), z), .010, .10, 'z', M_STEEL, 20); box('%s_HingeLeaf%d' % (n, i), (hx + hs * .035, s * (MY + .0015), z), (.06, .006, .09), M_STEEL, .001, 1)
    text(n + '_Word', word, ((x0 + x1) / 2, s * (MY - .001), 1.90), .075, 'y+' if s > 0 else 'y-', M_SBLK); piv(n, (hx, s * (MY + .010), 1.6)); ik('Handle_' + n, (lx, s * (MY + .026), 1.62)); ik('Stand_' + n, (lx - hs * .10, s * 1.85, 0)); ik('Stand_%s_Open' % n, ((x0 + x1) / 2, s * 1.78, 0))
door('HeaterDoor', HD, 1, 1.60, 'HEATER'); door('PanelDoorL', PD, 1, .62, 'PUMPS', M_MAIN); door('PanelDoorR', PD, -1, .62, 'HAND HOSE', M_MAIN)
for k in range(8): turn(box('HeaterDoor_Louvre%d' % k, (2.14, MY + .003, 1.15 + .05 * k), (.80, .012, .030), M_DARK, .002, 1), (2.14, MY + .003, 1.15 + .05 * k), 'x', -18)
box('HeaterGrille', (2.14, -MY - .004, 1.60), (1.06, .020, 1.00), M_PLAST, .008)
for k in range(17): turn(box('HeaterGrille_S%d' % k, (2.14, -MY - .016, 1.14 + .057 * k), (.98, .012, .040), M_DARK, .002, 1), (2.14, -MY - .016, 1.14 + .057 * k), 'x', 18)
cyl('HeaterBurner', (2.14, .95, 1.62), .17, .84, 'x', M_STEEL, 56, .02); [torus('HeaterBurner_Band%d' % i, (x, .95, 1.62), .171, .008, 'x', M_DARK, 48, 10) for i, x in enumerate((1.84, 2.14, 2.44))]; [tube('HeaterPipe%d' % i, [(1.80 + .34 * i, .76, 1.40), (1.80 + .34 * i, .90, 1.38), (1.80 + .34 * i, .95, 1.46)], .022, M_BRASS) for i in range(3)]; box('HeaterFan', (2.14, .85, 1.18), (.50, .22, .20), M_FRM, .03, 3); [box('HeaterFoot%d' % i, (x, .80, 1.60), (.06, .16, .10), M_DARK, .004) for i, x in enumerate((1.84, 2.44))]
cyl('Exhaust', (1.68, -.92, 2.38), .085, .18, 'z', M_STEEL, 48, .004); cut(bpy.data.objects['Exhaust'], cyl('c', (1.68, -.92, 2.46), .076, .14, 'z', M_STEEL, 40)); torus('Exhaust_Lip', (1.68, -.92, 2.468), .081, .006, 'z', M_DARK, 48, 10); cyl('Exhaust_Flange', (1.68, -.92, 2.303), .13, .014, 'z', M_DARK, 40, .003); box('AirIntake', (2.60, .86, 2.36), (.26, .40, .13), M_PLAST, .03, 3); [box('AirIntake_S%d' % k, (2.472, .86, 2.335 + .022 * k), (.010, .34, .010), M_DARK, .002, 1) for k in range(4)]
tube('CabExhaust', [(2.93, -.95, .80), (2.93, -.95, 2.55)], .055, M_STEEL, 10, False); box('CabExhaust_Arm', (2.835, -.95, 2.0), (.13, .05, .05), M_CHAS, .004); cyl('CabExhaust_Guard', (2.93, -.95, 1.55), .075, 1.1, 'z', M_DARK, 36, .006); box('CabExhaust_Foot', (2.93, -.70, .86), (.07, .56, .05), M_CHAS, .004)
# the pump panel (left): dials, valves, buttons, fill connections
PY = MY - .27
box('Panel_Plate', (.995, PY + .004, 1.78), (.60, .012, .66), M_FRM, .004)
for i, (nm, z, x, v) in enumerate((('TempI', 2.00, 1.17, 82), ('PressI', 2.00, .995, 0), ('PressIV', 2.00, .82, 0), ('Heater', 1.84, 1.17, 82))):
    c = Vector((x, PY + .022, z)); o = cyl('Panel_Dial' + nm, tuple(c), .060, .030, 'y', M_DIAL, 48); CUV.append((o, c, .122, .122, (i + 2) / 6, (i + 3) / 6, (-1, 0, 0))); torus('Panel_Ring' + nm, tuple(c + Vector((0, .014, 0))), .061, .006, 'y', M_CHROME, 48, 10); fr = v / (100 if i == 0 else 120); turn(box('GaugeNeedle_%s_Arm' % nm, (x, PY + .041, z + .018), (.0035, .002, .055), M_SBLK, .0004, 1), (x, PY + .041, z), 'y', -135 + 270 * fr); cyl('GaugeNeedle_%s_Cap' % nm, (x, PY + .040, z), .008, .010, 'y', M_PLAST, 20); piv('GaugeNeedle_' + nm, (x, PY + .041, z))
ds = box('Panel_Screen', (.865, PY + .014, 1.84), (.20, .014, .10), M_DISP, .002, 1); CUV.append((ds, Vector((.865, PY, 1.84)), .20, .10, 0, .5, (-1, 0, 0))); ik('Panel_Screen', (.865, PY + .022, 1.84)); ik('PanelStand', (1.0, 1.75, 0))
for i, (nm, m) in enumerate((('W', M_BLU), ('I', M_SORG), ('IV', M_GRN), ('Hand', M_YEL))): x = 1.22 - .15 * i; cyl('Panel_ValveBody' + nm, (x, PY + .030, 1.66), .022, .05, 'y', M_BRASS, 28); box('Valve_%s_Arm' % nm, (x, PY + .062, 1.70), (.024, .012, .11), m, .005, 2); piv('Valve_' + nm, (x, PY + .062, 1.66)); ik('Valve_' + nm, (x, PY + .075, 1.74)); text('Panel_Word' + nm, {'W': 'WATER', 'I': 'TYPE I', 'IV': 'TYPE IV', 'Hand': 'HAND'}[nm], (x, PY + .010, 1.595), .020, 'y+', M_SWHT, .0012)
for i, (nm, m) in enumerate((('PumpOn', M_GRN), ('PumpOff', M_SRED), ('HeatOn', M_GRN), ('HeatOff', M_SRED), ('Light', M_DARK))): cyl('Panel_Btn' + nm, (1.20 - .085 * i, PY + .018, 1.52), .016, .022, 'y', m, 28, .004); ik('Button_' + nm, (1.20 - .085 * i, PY + .032, 1.52))
cyl('Panel_EStopBase', (.75, PY + .014, 1.52), .030, .012, 'y', M_YEL, 32, .003); cyl('Panel_EStop', (.75, PY + .034, 1.52), .022, .034, 'y', M_SRED, 32, .006); ik('Button_EStop', (.75, PY + .055, 1.52))
for i, (nm, m) in enumerate((('W', M_BLU), ('I', M_SORG), ('IV', M_GRN))): x = 1.19 - .195 * i; cyl('Panel_Coup' + nm, (x, PY + .05, 1.23), .050, .10, 'y', M_ALU, 40, .004); torus('Panel_CoupRing' + nm, (x, PY + .06, 1.23), .052, .007, 'y', m, 40, 10); cyl('CoupCap_%s_Cap' % nm, (x, PY + .118, 1.23), .056, .034, 'y', M_ALU, 40, .006); [box('CoupCap_%s_Ear%d' % (nm, e > 0), (x + e * .060, PY + .118, 1.23), (.03, .014, .05), M_ALU, .004) for e in (-1, 1)]; piv('CoupCap_' + nm, (x, PY + .10, 1.23)); ik('FillCoupling_' + nm, (x, PY + .10, 1.23)); text('Panel_CoupWord' + nm, {'W': 'WATER', 'I': 'TYPE I', 'IV': 'TYPE IV'}[nm], (x, PY, 1.335), .026, 'y+', M_SBLK)
box('PanelLamp_Base', (.995, PY + .10, 2.165), (.40, .05, .03), M_PLAST, .006); box('PanelLamp_Lens', (.995, PY + .10, 2.147), (.37, .035, .010), M_WHT, .003)
# the ground hose (right)
HRC = (.985, -.885, 1.50); HOSE_M, HRF, HRW = reel('HoseReel', HRC, .014, .16, 2, 8, M_HOSE)
[box('ReelArm%d' % (e > 0), (.985, HRC[1] + e * (HRW + .030), 1.36), (.09, .020, .37), M_FRM, .004) for e in (-1, 1)]; box('ReelFoot', (.985, HRC[1], 1.160), (.30, .40, .032), M_FRM, .006); [box('ReelLeg%d' % i, (.985 + x, HRC[1] + y, 1.097), (.06, .06, .10), M_FRM, .006) for i, (x, y) in enumerate(((-.12, -.15), (-.12, .15), (.12, -.15), (.12, .15)))]
ik('HoseOut', (.985 - .12, HRC[1], 1.50 + .20)); tube('HandNozzle_Body', [(1.12, -.90, 2.03), (.98, -.90, 2.00), (.90, -.90, 1.99)], .020, M_BRASS); tube('HandNozzle_Grip', [(1.10, -.90, 2.015), (1.13, -.90, 1.93)], .018, M_PLAST, 8, False); cyl('HandNozzle_Tip', (.865, -.90, 1.988), .026, .08, 'x', M_PLAST, 32, .006); tube('HandNozzle_Trigger', [(1.06, -.90, 1.99), (1.075, -.90, 1.92)], .006, M_RED, 6, False); piv('HandNozzle', (1.12, -.90, 1.99)); ik('HandNozzle_Hand', (1.115, -.90, 1.965)); ik('HandNozzle_Hose', (1.13, -.90, 1.93)); ik('HandNozzle_Mouth', (.825, -.90, 1.988))
for i, x in enumerate((.94, 1.04)): box('HandHolder%d' % i, (x, -.76, 1.962), (.03, .31, .020), M_FRM, .004)
tube('HoseTail', [(1.13, -.90, 1.92), (1.15, -.90, 1.82), (1.12, -.89, 1.74), (1.02, -.885, 1.703), (.97, -.885, 1.6995)], .0135, M_HOSE); ik('Stand_HoseReel', (1.0, -1.75, 0))
# tanks: water, type I, type IV on saddles; sight gauges both sides; caps on top
TO = [(y, z) for y, z in rnd([(-1.15, 1.10), (1.15, 1.10), (1.15, 2.36), (-1.15, 2.36)], .17, 6)]
for nm, x0, x1, m, lab in (('W', -.55, .49, M_BLUE, 'WATER'), ('I', -1.54, -.60, M_ORNG, 'TYPE I'), ('IV', -2.65, -1.70, M_GREEN, 'TYPE IV')):
    xm = (x0 + x1) / 2; solidx('Tank_' + nm, TO, x0, x1, .06, M_TANK, 45); [bandx('TankBand_%s%d' % (nm, i), TO, x, .05, .006, M_STEEL) for i, x in enumerate((x0 + .13, x1 - .13))]
    for s, sn, f in SIDES:
        box('Saddle_%s%s' % (nm, sn), (xm, s * .37, .971), (x1 - x0 - .20, .12, .262), M_CHAS, .006); gx_ = x0 + .24; gy = s * 1.185
        for z in (1.40, 2.10): box('Sight_%s%s_%d' % (nm, sn, z > 2), (gx_, s * 1.172, z), (.05, .050, .06), M_BRASS, .006)
        cyl('Sight_%s%s_Tube_Glass' % (nm, sn), (gx_, gy, 1.75), .013, .642, 'z', M_GLASS, 24); cyl('Sight_%s%s_Fluid' % (nm, sn), (gx_, gy, 1.68), .0095, .50, 'z', m, 20); box('Sight_%s%s_Scale' % (nm, sn), (gx_ + .055, s * 1.1515, 1.75), (.035, .007, .60), M_SWHT, .001, 1); [box('Sight_%s%s_Mark%d' % (nm, sn, k), (gx_ + .055, s * 1.1555, 1.49 + .13 * k), (.030, .004, .006), M_SBLK, .0005, 1) for k in range(5)]
        box('TankLabel_%s%s' % (nm, sn), (xm + .10, s * 1.1515, 1.40), (.48, .007, .16), m, .002, 1); text('TankWord_%s%s' % (nm, sn), lab, (xm + .10, s * 1.155, 1.40), .085, f, M_SWHT)
    cx = xm; cy = .62; lz = 2.358; CK.lathez('FillNeck_' + nm, [(.15, lz - .004), (.15, lz + .016), (.125, lz + .018), (.125, lz + .055), (.105, lz + .055), (.105, lz - .004), (.15, lz - .004)], (cx, cy, 0), M_STEEL, 48); CK.lathez('FillCap_%s_Lid' % nm, [(0, lz + .057), (.128, lz + .057), (.130, lz + .075), (.10, lz + .090), (0, lz + .092)], (cx, cy, 0), m, 48); box('FillCap_%s_Hinge' % nm, (cx + .14, cy, lz + .068), (.05, .06, .016), M_STEEL, .003); box('FillNeck_%s_Hinge' % nm, (cx + .150, cy, lz + .040), (.03, .05, .036), M_STEEL, .003); box('FillCap_%s_Grip' % nm, (cx - .145, cy, lz + .066), (.05, .03, .014), M_PLAST, .004); piv('FillCap_' + nm, (cx + .16, cy, lz + .066)); ik('FillCap_' + nm, (cx - .165, cy, lz + .066)); cyl('Vent_' + nm, (cx, -.72, lz + .05), .03, .11, 'z', M_STEEL, 28, .004); cyl('Vent_%s_Hood' % nm, (cx, -.72, lz + .112), .05, .02, 'z', M_PLAST, 32, .006)
for s, sn, f in SIDES:
    box('NamePanel_' + sn, (-.03, s * 1.1525, 1.95) if s > 0 else (-.03, s * 1.1525, 1.95), (.44, .009, .22), M_NAME, .002, 1); ik('Name_' + sn, (-.03, s * 1.158, 1.95)); [box('SideHi%d_%s' % (i, sn), (-.55 + .30 * i, s * 1.1515, 1.275), (.26, .007, .06), M_HVR if i % 2 else M_HVW, .001, 1) for i in range(0)]
    box('RestPost_' + sn, (-1.62, s * .30, 1.668), (.06, .08, 1.66), M_FRM, .006); box('RestFoot_' + sn, (-1.62, s * .33, .835), (.06, .16, .014), M_FRM, .002, 1)
box('RestBeam', (-1.62, 0, 2.53), (.10, .72, .07), M_FRM, .008); box('RestPad', (-1.62, 0, 2.591), (.09, .34, .054), M_SEAL, .008); ik('BoomRest', (-1.62, 0, 2.618))
# the rear deck
DK = .945
box('Deck', (-3.69, 0, .92), (1.86, 2.36, .05), M_TREAD, .004, 1); cut(bpy.data.objects['Deck'], box('c', (-3.95, 1.10, .92), (.56, .22, .2), M_TREAD)); [box('DeckBeam%d' % i, (x, 0, .868), (.08, 2.20 if i != 2 else 1.90, .056), M_CHAS, .004) for i, x in enumerate((-2.84, -3.45, -3.95, -4.50))]
decal(box('DeckEdge_Chev', (-4.6215, 0, .918), (.006, 2.20, .042), M_CHEV), 'x', M_BUMP)
for k, (z, y0) in enumerate(((.32, 1.00), (.62, 1.00))): box('RearStep%d' % k, (-3.95, 1.095, z), (.50, .19, .03), M_TREAD, .005, 2); box('RearStep%d_Edge' % k, (-3.95, 1.178, z + .0165), (.48, .022, .006), M_NOSE, .002, 1); ik('Step_Deck_%d' % k, (-3.95, 1.10, z + .02))
for e, x in enumerate((-4.205, -3.695)): box('RearStepSide%d' % e, (x, 1.06, .60), (.014, .20, .62), M_CHAS, .003, 1)
ik('Step_Deck_2', (-3.95, .92, DK)); ik('Deck_Door', (-3.95, .90, DK)); ik('Stand_DeckSteps', (-3.95, 1.75, 0)); RAIL = lambda n, p: tube(n, p, .019, M_YEL, 6, False)
for s, sn, f in SIDES:
    if s > 0: segs = ((-2.80, -3.62), (-4.28, -4.55))
    else: segs = ((-2.80, -4.55),)
    for i, (a, b) in enumerate(segs):
        RAIL('DeckRail_%s%d_T' % (sn, i), [(a, s * 1.14, 1.95), (b, s * 1.14, 1.95)]); RAIL('DeckRail_%s%d_M' % (sn, i), [(a, s * 1.14, 1.45), (b, s * 1.14, 1.45)])
        for j, x in enumerate((a, b) if a - b < 1.2 else (a, (a + b) / 2, b)): RAIL('DeckPost_%s%d%d' % (sn, i, j), [(x, s * 1.14, .944), (x, s * 1.14, 1.95)]); cyl('DeckPostFoot_%s%d%d' % (sn, i, j), (x, s * 1.14, .951), .038, .014, 'z', M_FRM, 28, .003)
    RAIL('DeckRail_%sB_T' % sn, [(-4.55, s * 1.14, 1.95), (-4.55, s * .74, 1.95)]); RAIL('DeckRail_%sB_M' % sn, [(-4.55, s * 1.14, 1.45), (-4.55, s * .74, 1.45)]); RAIL('DeckPost_%sB' % sn, [(-4.55, s * .74, .944), (-4.55, s * .74, 1.95)])
ik('Grab_DeckSteps_F', (-3.62, 1.14, 1.50)); ik('Grab_DeckSteps_R', (-4.28, 1.14, 1.50)); [box("CradlePad%d" % i, (x, y, .955), (.16, .14, .025), M_SEAL, .004) for i, (x, y) in enumerate(((-3.40, .42), (-3.40, -.42), (-4.22, .42), (-4.22, -.42)))]
# #endregion

# #region ---- turret, boom, hose chain
SX, PX, PZ, TX = 2.15, 2.45, 2.86, -2.90
CK.lathez('SlewRing', [(0, 2.296), (.50, 2.296), (.50, 2.315), (.46, 2.32), (.44, 2.395), (0, 2.395)], (SX, 0, 0), M_DARK, 80); [cyl('SlewBolt%d' % k, (SX + .48 * math.cos(math.pi / 12 * k), .48 * math.sin(math.pi / 12 * k), 2.322), .011, .018, 'z', M_STEEL, 6) for k in range(24)]
CK.lathez('Tur_Table', [(0, 2.401), (.45, 2.401), (.455, 2.41), (.455, 2.455), (.44, 2.47), (0, 2.47)], (SX, 0, 0), M_BOOM, 80); ck = rnd([(1.72, 2.466), (2.56, 2.466), (2.66, 2.80), (2.60, 3.0), (2.45, 3.06), (2.30, 3.0), (2.10, 2.72), (1.86, 2.64), (1.72, 2.62)], [.01, .01, .1, .08, .08, .08, .1, .05, .03], 5)
for s, sn, f in SIDES: K.finish(rawprism('Tur_Cheek' + sn, ck, 'y', s * .225, s * .255), M_BOOM, 30); cyl('Tur_PinCap' + sn, (PX, s * .268, PZ), .085, .03, 'y', M_STEEL, 40, .004); cyl('Tur_LiftCap' + sn, (1.82, s * .262, 2.54), .045, .016, 'y', M_STEEL, 32, .003)
cyl('Tur_Pin', (PX, 0, PZ), .055, .50, 'y', M_STEEL, 40); cyl('Tur_LiftPin', (1.82, 0, 2.54), .028, .50, 'y', M_STEEL, 32); box('Tur_Motor', (2.37, .375, 2.565), (.18, .16, .20), M_FRM, .02, 3); text('Tur_Word', 'KEEP CLEAR', (2.30, -.255, 2.56), .055, 'y-', M_SBLK); cyl('Tur_Swivel', (2.15, 0, 2.505), .07, .08, 'z', M_BRASS, 36, .006); piv('Slew', (SX, 0, 2.40)); piv('BoomLift', (PX, 0, PZ))
def tb(n, x0, x1, w, z0, z1, t, m): o = rawprism(n, rnd([(-w, z0), (w, z0), (w, z1), (-w, z1)], .035, 4), 'x', x0, x1); boole(o, rawprism('c', rnd([(-w + t, z0 + t), (w - t, z0 + t), (w - t, z1 - t), (-w + t, z1 - t)], .025, 4), 'x', x0 - 1, x1 + 1), 'DIFFERENCE', False); return K.finish(o, m, 32)
tb('BoomBase', -2.52, 2.62, .20, 2.62, 3.10, .012, M_BOOM); tb('BoomSec2', -2.66, 1.80, .165, 2.66, 3.06, .010, M_BOOM); tb('BoomSec3', -2.80, 1.50, .13, 2.70, 3.02, .010, M_BOOM)
for nm, x, w, z0, z1, g in (('BoomBase', -2.50, .20, 2.62, 3.10, .035), ('BoomSec2', -2.64, .165, 2.66, 3.06, .035)):
    for s in (-1, 1): box(nm + '_Collar%d' % (s > 0), (x, s * (w + .011), 2.86), (.05, .024, z1 - z0 - .07), M_DARK, .004); box(nm + '_Pad%d' % (s > 0), (x, s * (w - .012 - .0105), 2.86), (.05, .020, .24), M_PLAST, .002, 1)
    for e, z in ((0, z0 - .011), (1, z1 + .011)): box(nm + '_CollarZ%d' % e, (x, 0, z), (.05, 2 * w - .07, .024), M_DARK, .004)
for e in (-1, 1): box('BoomBase_LiftEar%d' % (e > 0), (.75, e * .075, 2.575), (.14, .020, .10), M_BOOM, .006); box('BoomBase_PinBoss%d' % (e > 0), (PX, e * .211, PZ), (.20, .020, .20), M_DARK, .006); text('BoomBase_Word%s' % ('L' if e > 0 else 'R'), 'DE-ICING', (-.6, e * .20, 2.86), .20, 'y+' if e > 0 else 'y-', M_SBLK); [box('BoomBase_Chev%d%d' % (i, e > 0), (-2.10 + .16 * i, e * .201, 2.86), (.08, .006, .34), M_SBLK, .001, 1) for i in range(3)]
cyl('BoomBase_LiftPin', (.75, 0, 2.55), .024, .19, 'y', M_STEEL, 28)
A, B = Vector((1.82, 0, 2.54)), Vector((.75, 0, 2.55)); q = Matrix.Translation(A) @ (B - A).to_track_quat('Z', 'Y').to_matrix().to_4x4(); L = (B - A).length
for n, r, z0, z1, m in (('LiftBarrel_Tube', .055, .05, .93, M_FRM), ('LiftBarrel_Eye', .048, -.048, .06, M_DARK), ('LiftRod_Bar', .034, .70, L - .02, M_CHROME)): o = cyl(n, (0, 0, (z0 + z1) / 2), r, z1 - z0, 'z', m, 40, .004); o.data.transform(q); o.data.update()
cyl('LiftRod_Eye', tuple(B), .045, .10, 'y', M_DARK, 36, .004); piv('Lift', tuple(A)); piv('LiftEye', tuple(B)); piv('LevelTip', (TX, 0, PZ))
box('BoomSec3_Head', (-2.812, 0, PZ), (.024, .34, .38), M_BOOM, .006); [K.finish(rawprism('BoomSec3_Fork%d' % (e > 0), rnd([(-2.82, 2.74), (-2.82, 2.98), (-2.90, 2.96), (-2.975, 2.86), (-2.90, 2.76)], [.01, .01, .06, .06, .06], 5), 'y', e * .120, e * .145), M_BOOM, 30) for e in (-1, 1)]; cyl('BoomSec3_Pin', (TX, 0, PZ), .035, .33, 'y', M_STEEL, 36); box('BoomSec3_Post', (-2.812, 0, 3.17), (.024, .22, .26), M_BOOM, .004)
box('BoomSec3_Tray', (-1.175, 0, 3.2925), (3.30, .25, .010), M_ALU, .002, 1); [box('BoomSec3_TrayLip%d' % (e > 0), (-1.175, e * .122, 3.317), (3.30, .006, .045), M_ALU, .001, 1) for e in (-1, 1)]; [box('BoomBase_Guide%d' % (e > 0), (-.95, e * .125, 3.122), (3.0, .008, .055), M_ALU, .001, 1) for e in (-1, 1)]
CHAIN = {'pitch': .095, 'radius': .105, 'fixed_x': -2.45, 'moving_x': -2.75, 'z_low': 3.15, 'z_high': 3.36, 'loop_x': .55, 'links': 70}
box('lk', (0, 0, 0), (.088, .160, .010), M_PLAST, .003, 1); parts = [bpy.data.objects['lk']] + [prism('lk%d' % e, [(-.044, 0), (-.030, -.025), (.030, -.025), (.044, 0), (.030, .025), (-.030, .025)], e * .078 - .006, e * .078 + .006, M_PLAST) for e in (-1, 1)] + [cyl('lkh%d' % i, (0, y, -.004 - r), r, .0935, 'x', m, 16) for i, (y, r, m) in enumerate(((-.045, .014, M_HOSE), (0, .014, M_HOSE), (.042, .010, M_SBLK)))]
CK.join('lkj', parts); lk = bpy.data.objects['lkj']; ALL.remove(lk)
for i in range(CHAIN['links']):
    s_ = (i + .5) * .095; o = bpy.data.objects.new('ChainLink_%02d' % i, lk.data); bpy.context.collection.objects.link(o); ALL.append(o); d = 3.0
    if s_ < d: o.location = (-2.45 + s_, 0, 3.15)
    elif s_ < d + math.pi * .105: a = (s_ - d) / .105; o.location = (.55 + .105 * math.sin(a), 0, 3.255 - .105 * math.cos(a)); o.rotation_euler = (0, -a, 0)
    else: o.location = (.55 - (s_ - d - math.pi * .105), 0, 3.36); o.rotation_euler = (0, math.pi, 0)
bpy.data.objects.remove(lk, do_unlink=True)
ik('Chain_Fixed', (-2.45, 0, 3.15)); ik('Chain_Moving', (-2.75, 0, 3.36)); box('BoomBase_ChainClamp', (-2.475, 0, 3.135), (.035, .17, .05), M_STEEL, .004); box('BoomSec3_ChainClamp', (-2.795, 0, 3.335), (.06, .17, .075), M_STEEL, .004)
tube('HoseLift', [(2.18, .0, 2.545), (2.20, 0, 2.60), (2.36, 0, 2.60), (2.52, .05, 2.585), (2.72, .08, 2.66), (2.79, .08, 2.86), (2.74, .07, 3.09), (2.58, .06, 3.135), (2.42, .06, 3.135)], .019, M_HOSE); ik('HoseLift_A', (2.18, 0, 2.545)); ik('HoseLift_B', (2.42, .06, 3.135)); box('BoomBase_HoseClamp', (2.40, .06, 3.125), (.05, .08, .055), M_STEEL, .004)
tube('BoomBase_Pipe0', [(2.40, .06, 3.135), (.80, .06, 3.135), (.72, .06, 3.125)], .018, M_STEEL, 8, False); tube('BoomBase_Pipe1', [(.72, .06, 3.125), (.64, .105, 3.118), (.50, .150, 3.118), (-2.40, .150, 3.118), (-2.47, .10, 3.125)], .014, M_STEEL, 8, False)
tube('HoseTip', [(-2.825, .05, 3.345), (-2.90, .05, 3.36), (-3.0, .05, 3.30), (-3.10, .05, 3.14), (-3.13, .05, 3.02)], .019, M_HOSE); ik('HoseTip_A', (-2.825, .05, 3.345)); ik('HoseTip_B', (-3.13, .05, 3.02))
# #endregion

# #region ---- the spray cabin (stays level; stowed over the rear deck, its front looks back)
CF = 1.06; CO = rnd([(-3.25, 1.02), (-4.36, 1.02), (-4.58, 1.46), (-4.44, 2.86), (-3.25, 2.93)], [.05, .05, .06, .12, .10], 6); CW = .60
body = rawprism('Cabin_Body', CO, 'y', -CW, CW); boole(body, rawprism('c', grow(CO, -.04), 'y', -CW + .04, CW - .04), 'DIFFERENCE', False); DRO = rnd([(-4.28, 1.10), (-3.66, 1.10), (-3.66, 2.72), (-4.28, 2.72)], .05)
CSR = rnd([(-3.36, 1.50), (-4.48, 1.50), (-4.37, 2.74), (-3.36, 2.78)], .07); CFR = rnd([(-.52, 1.56), (.52, 1.56), (.52, 2.72), (-.52, 2.72)], .07); CFL = rnd([(-.48, 1.12), (.48, 1.12), (.48, 1.40), (-.48, 1.40)], .05); CRF = rnd([(-4.25, -.44), (-3.45, -.44), (-3.45, .44), (-4.25, .44)], .07); CBK = rnd([(-.22, 1.50), (.22, 1.50), (.22, 2.40), (-.22, 2.40)], .05)
boole(body, rawprism('c', DRO, 'y', .3, 1), 'DIFFERENCE', False); boole(body, rawprism('c', CSR, 'y', -1, -.3), 'DIFFERENCE', False)
for pts, ax, a, b in ((CFR, 'x', -5, -4.2), (CFL, 'x', -5, -4.2), (CRF, 'z', 2.6, 3.2), (CBK, 'x', -3.4, -3.0)): boole(body, rawprism('c', pts, ax, a, b), 'DIFFERENCE', False)
K.finish(body, M_BOOM, 32)
def cpane(n, pts, ax, a, b, d0=.012, d1=.018, m=M_GLASS, hole=None):
    o = rawprism(n, pts, ax, a, b); hole and boole(o, rawprism('h', hole, ax, a - 1, b + 1), 'DIFFERENCE', False)
    if ax == 'y': k = sorted((math.copysign(CW - d0, a), math.copysign(CW - d1, a))); boole(o, rawprism('c', [(-5, 0), (-2, 0), (-2, 4), (-5, 4)], 'y', *k), 'INTERSECT', False)
    else: boole(o, rawprism('c', grow(CO, -d0), 'y', -2, 2), 'INTERSECT', False); boole(o, rawprism('c', grow(CO, -d1), 'y', -3, 3), 'DIFFERENCE', False)
    assert len(o.data.polygons), n; return K.finish(o, m, 32)
for n, pts, ax, a, b in (('R', CSR, 'y', -1, -.3), ('F', CFR, 'x', -5, -4.2), ('FL', CFL, 'x', -5, -4.2), ('T', CRF, 'z', 2.6, 3.2), ('B', CBK, 'x', -3.4, -3.0)): cpane('Cabin_Win%s_Glass' % n, grow(pts, .006), ax, a, b); cpane('Cabin_Win%s_Seal' % n, grow(pts, .014), ax, a, b, -.003, .026, M_SEAL, grow(pts, -.016))
K.finish(rawprism('Cabin_Floor', [(-3.295, -.555), (-4.36, -.555), (-4.36, .555), (-3.295, .555)], 'z', CF - .004, CF + .010), M_MAT, 30); ik('Cabin_Floor', (-3.80, 0, CF + .01)); ik('Cabin_In', (-3.95, .30, CF + .01))
DRP = grow(DRO, .025); dp = rawprism('CabinDoor_Panel', DRP, 'y', .612, .640); DW = rnd([(-4.21, 1.46), (-3.73, 1.46), (-3.73, 2.66), (-4.21, 2.66)], .05); boole(dp, rawprism('c', DW, 'y', .3, 1), 'DIFFERENCE', False); K.finish(dp, M_BOOM, 32); K.finish(rawprism('CabinDoor_Glass', grow(DW, .006), 'y', .622, .628), M_GLASS, 30)
ds = rawprism('CabinDoor_Seal', grow(DW, .014), 'y', .618, .643); boole(ds, rawprism('c', grow(DW, -.016), 'y', .3, 1), 'DIFFERENCE', False); K.finish(ds, M_SEAL, 30); box('CabinDoor_Handle', (-4.235, .664, 1.95), (.024, .020, .22), M_PLAST, .008, 3); [box('CabinDoor_HandleP%d' % e, (-4.235, .648, z), (.020, .024, .024), M_PLAST, .004) for e, z in enumerate((1.87, 2.03))]; box('CabinDoor_HandleIn', (-4.235, .602, 1.95), (.020, .024, .16), M_PLAST, .006)
for e, z in ((0, 1.055), (1, 2.775)): box('Cabin_Track%d' % e, (-3.70, .622, z), (1.18, .048, .026), M_ALU, .004); [cyl('CabinDoor_Roller%d%d' % (e, i), (x, .630, z + (.020 if e == 0 else -.020)), .012, .016, 'y', M_STEEL, 20) for i, x in enumerate((-4.14, -3.80))]
piv('CabinDoor', (-3.97, .626, 1.9)); ik('Handle_CabinDoor_Out', (-4.235, .676, 1.95)); ik('Handle_CabinDoor_In', (-4.235, .59, 1.95)); [tube('Cabin_Grab%d' % i, [(-4.345, .600, z0), (-4.345, .650, z0 + .03), (-4.345, .650, z0 + .37), (-4.345, .600, z0 + .40)], .012, M_YEL) for i, z0 in enumerate((1.75,))]; ik('Grab_Cabin', (-4.345, .650, 1.95))
for s, sn, f in SIDES: box('Cabin_Mast' + sn, (-3.155, s * .28, 1.99), (.13, .10, 1.90), M_BOOM, .010); box('Cabin_Skid' + sn, (-3.84, s * .42, .995), (1.24, .10, .052), M_FRM, .008); box('Cabin_MastFoot' + sn, (-3.155, s * .35, 1.0), (.13, .24, .05), M_FRM, .006); [box('Cabin_MastPad%s%d' % (sn, i), (-3.237, s * .28, z), (.04, .08, .12), M_FRM, .006) for i, z in enumerate((1.25, 2.0, 2.70))]
for i, z in enumerate((1.30, 2.20)): box('Cabin_MastBar%d' % i, (-3.155, 0, z), (.10, .46, .10), M_BOOM, .008)
box('Cabin_MastHead', (-3.155, 0, 2.86), (.13, .46, .26), M_BOOM, .012); [K.finish(rawprism('Cabin_Lug%d' % (e > 0), rnd([(-3.10, 2.75), (-3.10, 2.97), (-2.90, 2.93), (-2.84, 2.86), (-2.90, 2.79)], [.01, .01, .05, .05, .05], 5), 'y', e * .078, e * .108), M_BOOM, 30) for e in (-1, 1)]; cyl('Cabin_LevelMotor', (TX, 0, PZ), .062, .145, 'y', M_FRM, 40, .006); box('Cabin_HoseClamp', (-3.13, .05, 3.005), (.07, .09, .035), M_STEEL, .004)
tube('Cabin_Pipe', [(-3.13, .05, 2.985), (-3.075, .19, 2.86), (-3.075, .19, 1.10), (-3.10, .16, .985), (-3.30, .16, .985), (-4.30, .16, .985), (-4.42, .10, .99)], .016, M_STEEL, 8, False)
FL0 = FLOOR; FLOOR = CF + .01; n0 = len(ALL); i0 = len(IKS); seat('CabinSeat', -3.74, 0, .46, depth=.46, back=.60, zc=FLOOR + .40); R180 = Matrix.Translation((-3.74, 0, 0)) @ Matrix.Rotation(math.pi, 4, 'Z') @ Matrix.Translation((3.74, 0, 0))
for o in ALL[n0:]: o.data.transform(R180); o.data.update()
for e in IKS[i0:]: e.location = R180 @ e.location
FLOOR = FL0
for s, n in ((1, 'L'), (-1, 'R')):
    box('Cabin_Arm' + n, (-3.86, s * .37, 1.62), (.42, .13, .10), M_DASH, .025, 3); box('Cabin_ArmPost' + n, (-3.72, s * .37, 1.33), (.10, .09, .51), M_PLAST, .012); cyl('Cabin_JoyBoot' + n, (-4.0, s * .37, 1.677), .034, .022, 'z', M_SEAL, 32, .008); cyl('Joy_%s_Stick' % n, (-4.0, s * .37, 1.735), .012, .11, 'z', M_PLAST, 24); box('Joy_%s_Grip' % n, (-4.0, s * .37, 1.82), (.046, .042, .12), M_PLAST, .016, 4); box('Joy_%s_Trigger' % n, (-4.026, s * .37, 1.835), (.016, .016, .04), M_RED, .005); piv('Joy_' + n, (-4.0, s * .37, 1.685)); ik('Joy_' + n, (-4.0, s * .37, 1.81)); ik('Foot_Cabin_' + n, (-4.24, s * .12, CF + .02)); ik('GrabIn_Cabin_' + n, (-4.0, s * .50, 2.2))
for i, m in enumerate((M_GRN, M_AMB, M_SRED)): cyl('Cabin_Btn%d' % i, (-3.80 + .05 * i, -.37, 1.675), .013, .016, 'z', m, 24, .003); ik('Cabin_Btn%d' % i, (-3.80 + .05 * i, -.37, 1.685))
cyl('Cabin_EStop', (-3.80, .37, 1.685), .022, .034, 'z', M_SRED, 32, .006); ik('Cabin_EStop', (-3.80, .37, 1.705)); tube('Cabin_ScreenArm', [(-3.88, -.43, 1.60), (-4.12, -.47, 1.62), (-4.20, -.46, 1.80)], .014, M_PLAST); o = box('Cabin_ScreenBox', (-4.215, -.42, 1.88), (.030, .24, .16), M_PLAST, .010, 3); sc_ = box('Cabin_Screen', (-4.199, -.42, 1.88), (.006, .21, .12), M_DISP, .001, 1); CUV.append((sc_, Vector((-4.199, -.42, 1.88)), .21, .12, .5, 1, (0, 1, 0))); ik('Cabin_Screen', (-4.19, -.42, 1.88))
box('SprayPedal_Pad', (-4.28, -.14, CF + .065), (.16, .09, .016), M_SEAL, .004); turn(bpy.data.objects['SprayPedal_Pad'], (-4.28, -.14, CF + .065), 'y', 16); box('Cabin_PedalBase', (-4.27, -.14, CF + .020), (.18, .11, .026), M_PLAST, .005); piv('SprayPedal', (-4.20, -.14, CF + .045)); box('Cabin_Heater', (-3.40, .0, CF + .14), (.16, .50, .24), M_PLAST, .02, 3); [box('Cabin_HeaterS%d' % k, (-3.482, 0, CF + .07 + .03 * k), (.010, .42, .010), M_DARK, .002, 1) for k in range(5)]
tube('CabinWiper_Arm', [(-4.475, 0, 2.765), (-4.50, -.10, 2.60), (-4.515, -.22, 2.42)], .008, M_PLAST); cyl('CabinWiper_Hub', (-4.462, 0, 2.765), .022, .04, 'x', M_PLAST, 24, .004); tube('CabinWiper_Blade', [(-4.494, -.10, 2.62), (-4.517, -.22, 2.42), (-4.540, -.34, 2.22)], .010, M_SEAL, 8, False); piv('CabinWiper', (-4.462, 0, 2.765)); ik('CabinWiperTip', (-4.540, -.34, 2.22))
def wlamp(n, c, axis=-1): x, y, z = c; box(n + '_Case', (x, y, z), (.09, .15, .11), M_PLAST, .015, 3); unit(n, (x + axis * .044, y, z), .042, M_WHT, axis); revx(n + '_Cover_Glass', [(0, axis * .062), (.048, axis * .060), (.048, axis * .054), (0, axis * .056)], (x + axis * .044, y, z), M_GLASS)
for i, (y, z) in enumerate(((.46, 2.97), (-.46, 2.97))): wlamp('CabinLamp_%d' % i, (-4.28, y, z)); box('Cabin_LampFoot%d' % i, (-4.26, y, 2.902), (.06, .08, .035), M_FRM, .004)
for i, y in enumerate((.36, -.36)): wlamp('CabinLampLow_%d' % i, (-4.40, y, 1.015)); box('Cabin_LampArm%d' % i, (-4.33, y, 1.035), (.07, .06, .03), M_FRM, .004)
beacon('Cabin_Beacon', (-3.36, -.35, 2.915)); tube('Cabin_Horn', [(-3.36, .34, 2.925), (-3.36, .34, 2.975)], .012, M_PLAST, 6, False); cyl('Cabin_HornBell', (-3.40, .34, 2.985), .045, .10, 'x', M_PLAST, 32, .006, r2=.025); ik('Horn', (-3.45, .34, 2.985))
box('Cabin_NozArm', (-4.52, 0, .990), (.34, .12, .050), M_FRM, .008); cyl('Cabin_NozBase', (-4.66, 0, 1.022), .062, .022, 'z', M_DARK, 40, .004); cyl('NozPan_Drum', (-4.66, 0, 1.062), .058, .056, 'z', M_BOOM, 40, .006); [box('NozPan_Ear%d' % (e > 0), (-4.66, e * .135, 1.125), (.07, .016, .15), M_BOOM, .006) for e in (-1, 1)]; box('NozPan_Yoke', (-4.66, 0, 1.085), (.07, .286, .020), M_BOOM, .004)
cyl('NozTilt_Shaft', (-4.66, 0, 1.17), .020, .33, 'y', M_STEEL, 28); box('NozTilt_Block', (-4.66, 0, 1.17), (.085, .22, .085), M_FRM, .014, 3)
for n, y, r, m in (('I', .06, .030, M_SORG), ('IV', -.06, .024, M_GRN)): cyl('NozTilt_Barrel' + n, (-4.775, y, 1.17), r, .16, 'x', M_STEEL, 36, .004); torus('NozTilt_Band' + n, (-4.79, y, 1.17), r, .006, 'x', m, 36, 10); revx('NozTilt_Tip' + n, [(r, .0), (r + .012, -.02), (r + .012, -.08), (r * .70, -.095), (r * .45, -.095), (r * .45, -.03), (.0, -.03), (.0, 0), (r, 0)], (-4.852, y, 1.17), M_PLAST); ik('Nozzle_' + n, (-4.948, y, 1.17))
piv('NozPan', (-4.66, 0, 1.06)); piv('NozTilt', (-4.66, 0, 1.17)); ik('HoseNozzle_A', (-4.42, .10, .99)); ik('HoseNozzle_B', (-4.62, .06, 1.17)); tube('HoseNozzle', [(-4.42, .10, .99), (-4.50, .18, 1.05), (-4.535, .16, 1.15), (-4.56, .085, 1.185), (-4.62, .06, 1.17)], .015, M_HOSE); ik('Aisle_Row0', (3.96, 0, FLOOR)); ik('CabinStep', (-3.95, .62, 1.10))
# #endregion
exec(compile(SRC('Art', 'Deicer', 'deicer_checks.py'), 'deicer_checks', 'exec'))
