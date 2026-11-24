# Self-propelled passenger stairs (to-do 212): our own small electric vehicle for the A320's doors. Cab with two seats and a glass roof under the top platform,
# a fixed lower flight and an upper flight with the platform that slides up along the same slope (platform 2.32 .. 4.22 m); covered lower treads fold away.
# Treads 190 rise x 280 going (34.2 deg): the platform is level with whole treads at 3.46 and 3.84 m = 3 cm under our A320's aft and front sills (3.49 / 3.87).
# Axes: x forward (the aircraft's side), y left, z up; metres; origin under the middle. Driver front left.   blender -b --python Art/PassengerStairs/make_passengerstairs.py [-- --pic DIR]
import os, sys, math
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, 'Tools'), os.path.join(ROOT, 'Art', 'CleaningKit')]
PFX, DASHWORD, MAIN, NAME, KEY, TAG = 'PS_', '', (.02, .26, .24), 'PassengerStairs', 'passengerstairs', 'PS'
import bpy, bmesh, json
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
import vehicle_kit as K
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, ALL, IKS
ART = os.path.join(ROOT, 'Art', NAME); TEX = os.path.join(ROOT, 'Art', 'PushbackTug', 'Textures'); OWN = os.path.join(ART, 'Textures'); CART = os.path.join(ROOT, 'Art', 'BaggageCart', 'Textures')
K.reset()
import make_cleaningkit as CK
import vehicle_skin as S
from vehicle_skin import rawprism, boole, patch, grow, rnd, ip
CK.join.__globals__['K'] = K
for _o in list(ALL): bpy.data.objects.remove(_o)
ALL.clear(); IKS.clear()
FX, DX, RW, TW, FLOOR, CABB = .90, -1.45, .32, .20, .48, 1.42                  # axles, tyre radius and width, cab floor, cab's back
_t = open(os.path.join(ROOT, 'Tools', 'truck_cab.py'), encoding='utf-8').read().split('# #region ----')
exec(compile('# ' + _t[1], 'truck_cab materials', 'exec'))
SIDES = ((1, 'L', 'y+'), (-1, 'R', 'y-'))
_s = _t[3]; exec(compile(_s[_s.index('def seat('):_s.index("seat('SeatDriver'")], 'truck_cab seat', 'exec'))
_s = _t[4]; exec(compile(_s[_s.index('def fender('):], 'truck_cab fender', 'exec'))
_st = open(os.path.join(ROOT, 'Tools', 'small_truck.py'), encoding='utf-8').read()
exec(compile(_st[_st.index('kr, kw = RW'):_st.index('FE = globals()')], 'small_truck wheel', 'exec'))

# #region ---- materials: photo sets everywhere (owner: no plain flat colours)
def paint(m, col, t=.7, n=.5): K.photo(m, OWN, 'PaintGSE', tile=t, tint=tuple(c * 1.06 for c in col), metal=0.0, normal=n)
paint(M_BODY, (.90, .91, .90)); paint(M_MAIN, MAIN); paint(M_NAME, (.88, .88, .87)); paint(M_CHAS, (.10, .11, .12)); paint(M_FRM, (.34, .36, .38)); paint(M_YEL, (.90, .58, .02)); paint(M_RED, (.62, .03, .02))
for m, c in ((M_SBLK, (.012, .012, .014)), (M_SWHT, (.90, .90, .88)), (M_SRED, (.68, .02, .02)), (M_HVR, (.65, .03, .02)), (M_HVW, (.85, .86, .86)), (M_GRN, (.03, .34, .10)), (M_BLU, (.02, .16, .45))): paint(m, c, .4, .3)
for m, c in ((M_PLAST, (.55, .55, .56)), (M_BUMP, (.75, .75, .76)), (M_DASH, (1.25, 1.25, 1.28))): K.photo(m, OWN, 'Plastic012A', tile=.35, tint=c, metal=0.0, normal=.6, rough_mul=1.9)
for m, c in ((M_TRIM, (.62, .63, .65)), (M_LIN, (1.35, 1.35, 1.32))): K.photo(m, OWN, 'Plastic018A', tile=.45, tint=c, metal=0.0, normal=.7)
K.photo(M_SEAT, CART, 'Fabric074', tile=.35, tint=(.16, .18, .20), metal=0.0); K.photo(M_SEAT2, TEX, 'Leather026', tile=.4, metal=0.0); K.photo(M_BELT, CART, 'Fabric074', tile=.1, tint=(.05, .05, .055), metal=0.0); K.photo(M_SEAL, TEX, 'Rubber001', tile=.25, tint=(.25, .25, .25), metal=0.0)
M_GRIT = P('PS_GritBlack', (.05, .05, .05)); K.photo(M_GRIT, TEX, 'Rubber001', tile=.22, tint=(.55, .55, .55), metal=0.0, normal=1.4); M_NOSE = P('PS_NosingYellow', (.9, .6, .03)); K.photo(M_NOSE, OWN, 'PaintGSE', tile=.2, tint=(.95, .60, .03), metal=0.0, normal=1.2, rough_mul=1.8)
M_CHEV = P('PS_Chevrons', (.9, .6, .05)); K.photo(M_CHEV, OWN, 'Chevron', tile=.4, metal=0.0, normal=.5); M_TREAD = P('PS_TreadAlu', (.7, .7, .7)); K.photo(M_TREAD, TEX, 'DiamondPlate008A', tile=.45, tint=(.95, .95, .97))
for m, c in ((M_AMB, (1, .42, .03)), (M_RLENS, (.80, .03, .02)), (M_WHT, (.97, .97, .95))): K.photo(m, OWN, 'LensPillow', tile=.10, tint=c, metal=0.0)
M_STEP = P('PS_StepLight', (1, .93, .78), 0, .3, 1.5); M_DIAL = P('PS_Dials', (.02, .02, .02)); K.photo(M_DIAL, OWN, 'Dials', metal=0.0); M_DISP = P('PS_Screen_Stair', (.02, .06, .08)); K.photo(M_DISP, OWN, 'Screen', metal=0.0)
CUV = []                                                                        # flat faces with their own picture: object, middle, width, height, the picture's u range
def AFTER_UV():
    for o, ctr, w, h, a0, a1 in CUV:
        uv = o.data.uv_layers.active.data
        for p in o.data.polygons:
            for li in p.loop_indices: d = o.data.vertices[o.data.loops[li].vertex_index].co - ctr; uv[li].uv = (a0 + (a1 - a0) * (-d.y / w + .5), d.z / h + .5)
def revx(name, prof, c, m, segs=48, smooth=40):
    """Turned about x through c; prof = (radius, x forwards), open or closed at radius 0."""
    bm = bmesh.new(); rings = [[bm.verts.new((c[0] + x, c[1] + r * math.cos(2 * math.pi * k / segs), c[2] + r * math.sin(2 * math.pi * k / segs))) for k in range(segs)] if r > 1e-6 else [bm.verts.new((c[0] + x, c[1], c[2]))] for r, x in prof]
    for A, B in zip(rings, rings[1:]):
        for k in range(segs):
            u = []
            for v in (A[k % len(A)], A[(k + 1) % len(A)], B[(k + 1) % len(B)], B[k % len(B)]):
                if v not in u: u.append(v)
            if len(u) > 2: bm.faces.new(u)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5); bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); return K.finish(K.link(name, bm), m, smooth)
# #endregion

# #region ---- cab skin: low, upright, glass roof
XFt = [(.24, 2.58), (.36, 2.70), (.60, 2.765), (.95, 2.765), (1.08, 2.745), (1.50, 2.665), (1.80, 2.585), (1.90, 2.53), (1.97, 2.42), (2.02, 2.25), (2.07, 1.9)]
XRt = [(.24, CABB), (1.88, CABB), (1.97, CABB + .03), (2.02, CABB + .10), (2.07, CABB + .3)]
WHt = [(.24, .955), (.36, .985), (.60, 1.0), (1.08, 1.0), (1.50, .975), (1.80, .945), (1.90, .905), (1.97, .82), (2.02, .66), (2.07, .25)]
LNt = [(.24, .20), (1.08, .22), (1.80, .26), (2.07, .40)]
ZSt = [.24, .28, .33, .39, .46, .53, .60, .68, .76, .84, .92, 1.0, 1.08, 1.16, 1.24, 1.32, 1.40, 1.48, 1.56, 1.64, 1.72, 1.79, 1.85, 1.89, 1.925, 1.95, 1.975, 1.995, 2.01, 2.023, 2.035, 2.043]
S.setup(XFt, XRt, WHt, LNt, ZSt, 1.90, [M_BODY, M_MAIN, M_BUMP, M_LIN, M_TRIM, M_SEAL], M_GLASS, M_SEAL, n=4.2, rr=.10)
SH = .036; OY = (.4, 1.5); XB = ('x', 2.0, 3.4); XH = ('x', 2.56, 3.4)
shell = S.loft('Body', 0.0); inner = S.loft('inner', SH, bot=.5, mi=3)
for p in inner.data.polygons:
    if p.center.z < 1.10: p.material_index = 4
boole(shell, inner, 'DIFFERENCE', False)
for p in shell.data.polygons:
    c = p.center
    if p.material_index == 0 and abs(p.normal.z) < .9:
        if .50 < c.z < .83: p.material_index = 1
        if c.z < .50: p.material_index = 2
for s in (-1, 1): boole(shell, rawprism('c', [(1.68, .20), (2.24, .20), (2.24, .425), (1.68, .425)], 'y', s * .70, s * 1.5), 'DIFFERENCE', False)      # recessed steps under the doors
DOORF = rnd([(1.60, .52), (2.46, .52), (2.46, 1.10), (2.33, 1.86), (1.60, 1.86)], [.05, .05, .10, .10, .07]); GAP = .004
shell0 = S.copy(shell, 'shell0')
def opening(name, pts, axis, a, b):
    pan = S.copy(shell0, name); boole(pan, rawprism('c', grow(pts, -GAP), axis, a, b), 'INTERSECT', False); boole(shell, rawprism('c', pts, axis, a, b), 'DIFFERENCE', False); return pan
doorD = opening('DoorDriver', DOORF, 'y', *OY); doorP = opening('DoorPassenger', DOORF, 'y', -OY[1], -OY[0]); S.rm(shell0)
WSC = rnd([(-.86, 1.09), (.86, 1.09), (.82, 1.83), (-.82, 1.83)], [.06, .06, .09, .09]); boole(shell, rawprism('c', WSC, *XB), 'DIFFERENCE', False)
WRE = rnd([(-.60, 1.12), (.60, 1.12), (.60, 1.80), (-.60, 1.80)], .06); boole(shell, rawprism('c', WRE, 'x', 1.3, 1.7), 'DIFFERENCE', False)
WRO = rnd([(1.66, -.60), (2.18, -.60), (2.18, .60), (1.66, .60)], .06); boole(shell, rawprism('c', WRO, 'z', 1.85, 2.3), 'DIFFERENCE', False)
WD = rnd([(1.68, .98), (2.385, .94), (2.265, 1.79), (1.68, 1.79)], .06)
for o, s in ((doorD, 1), (doorP, -1)): boole(o, rawprism('c', WD, 'y', s * OY[0], s * OY[1]), 'DIFFERENCE', False)
HLY, HLZ = (.60, .735, .86), .665                                               # three round units a side: low beam, high beam, indicator
HF = rnd([(.50, .575), (.935, .575), (.935, .755), (.50, .755)], .06)
for s in (-1, 1): boole(shell, rawprism('c', [(s * y, z) for y, z in HF], *XB), 'DIFFERENCE', False)
for o in (shell, doorD, doorP):
    used = sorted({p.material_index for p in o.data.polygons}); ms = [o.data.materials[i] for i in used]; ix = [used.index(p.material_index) for p in o.data.polygons]; o.data.materials.clear()
    for m in ms: o.data.materials.append(m)
    for p, i in zip(o.data.polygons, ix): p.material_index = i; p.use_smooth = True
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(32); ALL.append(o) if o not in ALL else None
S.pane('Windscreen', WSC, *XB); S.pane('RearWindow', WRE, 'x', 1.3, 1.7); S.pane('RoofWindow', WRO, 'z', 1.85, 2.3); S.pane('DoorDriver_Window', WD, 'y', *OY); S.pane('DoorPassenger_Window', WD, 'y', -OY[1], -OY[0])
for s, sn, f in SIDES:
    h = [(s * y, z) for y, z in HF]
    patch('Headlamp_%s_Bezel' % sn, grow(h, .012), *XH, M_PLAST, -.004, .036, hole=grow(h, -.010)); patch('Headlamp_%s_Cover_Glass' % sn, grow(h, -.006), *XH, M_GLASS, .002, .007)
    patch('Headlamp_%s_Pan' % sn, grow(h, -.004), *XH, M_PLAST, .085, .098, smooth=30); patch('Headlamp_%s_Tunnel' % sn, grow(h, .004), *XH, M_PLAST, .030, .100, hole=grow(h, -.008))
    for nm, y, mt, r in (('LowBeam', HLY[0], M_WHT, .062), ('HighBeam', HLY[1], M_WHT, .062), ('IndicatorF', HLY[2], M_AMB, .046)):
        x0 = S.surf(s * (y + r), HLZ).x - .088; c = (x0, s * y, HLZ)            # parabola with rings, emitter lens in its middle
        revx('%s_%s_Reflector' % (nm, sn), [(.012, -.004)] + [(r * k / 8, .052 * (k / 8) ** 2 + (.0015 if k % 2 else 0)) for k in range(2, 9)] + [(r + .004, .054), (r + .004, -.004), (.012, -.004)], c, M_CHROME)
        revx('%s_%s_Lens' % (nm, sn), [(0, .030), (r * .16, .0285), (r * .30, .023), (r * .36, .015), (r * .36, -.0065), (0, -.0065)], c, mt); revx('%s_%s_Collar' % (nm, sn), [(r * .30, -.001), (r * .46, -.001), (r * .46, .010), (r * .30, .010), (r * .30, -.001)], c, M_PLAST)
        for k in range(8): a = math.pi / 4 * k; box('%s_%s_Fin%d' % (nm, sn, k), (x0 + .030, s * y + r * .75 * math.cos(a), HLZ + r * .75 * math.sin(a)), (.012, .006, .006), M_CHROME, .001, 1)
    for i, (x0, x1) in enumerate(((1.64, 1.90), (1.91, 2.16), (2.17, 2.43))): patch(('DoorDriver' if s > 0 else 'DoorPassenger') + '_HiVis%d' % i, [(x0, .70), (x1, .70), (x1, .76), (x0, .76)], 'y', s * OY[0], s * OY[1], M_HVR if i % 2 else M_HVW, -.0025, .006)
    patch('CabSideMarker_' + sn, rnd([(1.44, .86), (1.50, .86), (1.50, .95), (1.44, .95)], .012, 3), 'y', s * OY[0], s * OY[1], M_AMB, -.009, .004)
patch('Grille_Field', rnd([(-.40, .60), (.40, .60), (.40, .74), (-.40, .74)], .04), *XB, M_PLAST, -.004, .02)
for i in range(4): patch('Grille_Slat%d' % i, [(-.36, .622 + .030 * i), (.36, .622 + .030 * i), (.36, .632 + .030 * i), (-.36, .632 + .030 * i)], *XB, M_DARK, -.012, .002)
patch('NamePanel_Front', rnd([(-.55, .86), (.55, .86), (.55, .97), (-.55, .97)], .02, 3), *XB, M_NAME, -.004, .006)
patch('PlateFront', [(-.26, .40), (.26, .40), (.26, .51), (-.26, .51)], *XB, M_NAME, -.005, .006)
def inside(name, pts, axis, a, b, d, m): o = rawprism(name, pts, axis, a, b); boole(o, S.LF(d), 'INTERSECT'); return K.finish(o, m, 32)
inside('Floor', [(1.40, .440), (2.80, .440), (2.80, FLOOR), (1.40, FLOOR)], 'y', -1.3, 1.3, .033, M_TRIM)                                   # whole, out to the skin: no holes
inside('Firewall', [(-1.3, .445), (1.3, .445), (1.3, 1.052), (-1.3, 1.052)], 'x', 2.634, 2.650, .033, M_PLAST); inside('Cowl', [(-1.3, 1.044), (1.3, 1.044), (1.3, 1.060), (-1.3, 1.060)], 'x', 2.36, 3.2, .034, M_PLAST)
for s in (-1, 1): boole(bpy.data.objects['Cowl'], rawprism('c', [(2.0, .9), (2.51, .9), (2.51, 1.3), (2.0, 1.3)], 'y', s * .895, s * 1.4), 'DIFFERENCE', False)     # clear of the doors
K.finish(rawprism('CabMat', [(1.52, -.86), (2.62, -.86), (2.62, .86), (1.52, .86)], 'z', FLOOR - .004, FLOOR + .008), M_MAT, 30)
box('RoofLight_Base', (1.56, 0, 1.952), (.10, .30, .05), M_PLAST, .008); box('RoofLight_Lamp', (1.56, 0, 1.924), (.08, .27, .010), M_WHT, .003)
for s, n in ((1, 'Driver'), (-1, 'Passenger')):
    box('Step%s_Riser' % n, (1.96, s * .700, .325), (.59, .014, .225), M_BUMP, .002, 1); box('Step%s_Top' % n, (1.96, s * .815, .431), (.59, .245, .014), M_BUMP, .002, 1)
    for e, x in ((0, 1.673), (1, 2.247)): box('Step%s_End%d' % (n, e), (x, s * .820, .325), (.014, .254, .225), M_BUMP, .002, 1)
    box('Step%s_Tread' % n, (1.96, s * .830, .255), (.575, .275, .030), M_TREAD, .006, 2); box('Step%s_Edge' % n, (1.96, s * .945, .2715), (.56, .035, .006), M_NOSE, .002, 1); ik('Step_Door%s' % n, (1.96, s * .87, .2745)); ik('Step_Door%s_Low' % n, (1.96, s * .87, .2745))
    cyl('StabHouse_F%s' % n[0], (2.875, s * .80, .375), .060, .29, 'z', M_FRM, 36, .006); [box('StabClamp_F%s%d' % (n[0], k), (2.69, s * .80, z), (.27, .10, .05), M_CHAS, .006) for k, z in enumerate((.30, .45))]; [torus('StabBand_F%s%d' % (n[0], k), (2.875, s * .80, z), .060, .006, 'z', M_CHAS) for k, z in enumerate((.30, .45))]
box('FrontBumper', (2.80, 0, .305), (.24, 1.36, .13), M_BUMP, .03, 3); box('FrontBumper_Chev', (2.9215, 0, .305), (.010, 1.22, .058), M_CHEV, .003, 1); box('FrontTowEye', (2.95, 0, .305), (.08, .05, .07), M_YEL, .012)
# #endregion

# #region ---- cab doors, dashboard with real dials, stair console, seats
ox, oz = 1.015, -.095; Xo = lambda x: x + ox; Zo = lambda z: z + oz; DY = .48
for s, n in ((1, 'DoorDriver'), (-1, 'DoorPassenger')):
    a, b = s * OY[0], s * OY[1]; sd = 'L' if s > 0 else 'R'
    patch(n + '_Trim', grow(DOORF, -.026), 'y', a, b, M_DASH, .034, .054, hole=grow(WD, .012))
    box(n + '_Armrest', (1.96, s * .918, .90), (.46, .055, .055), M_DASH, .02, 3); box(n + '_Pocket', (1.98, s * .930, .62), (.64, .040, .13), M_DASH, .015)
    box(n + '_HandleIn_Cup', (2.28, s * .940, .905), (.12, .018, .055), M_PLAST, .008); box(n + '_HandleIn', (2.27, s * .930, .908), (.08, .012, .020), M_CHROME, .004); box(n + '_Switches', (2.10, s * .920, .931), (.12, .045, .012), M_PLAST, .004)
    for k in range(2): box(n + '_Switch%d' % k, (2.075 + .05 * k, s * .920, .939), (.034, .022, .010), M_DARK, .003)
    patch(n + '_HandleCup', rnd([(1.64, .875), (1.78, .875), (1.78, .945), (1.64, .945)], .025, 4), 'y', a, b, M_PLAST, -.002, .008)
    box(n + '_Handle', (1.712, s * 1.020, .912), (.14, .018, .028), M_PLAST, .008, 3)
    for e, x in enumerate((1.652, 1.772)): box(n + '_Handle_P%d' % e, (x, s * 1.008, .912), (.018, .028, .020), M_PLAST, .004)
    for i, z in enumerate((.66, .98)): box(n + '_Hinge%d' % i, (2.400, s * .945, z), (.07, .035, .08), M_DARK, .004)
    box(n + '_MirrorFoot', (2.345, s * .985, 1.16), (.06, .07, .09), M_PLAST, .018, 3); box(n + '_MirrorArm', (2.36, s * 1.05, 1.20), (.045, .14, .05), M_PLAST, .018, 3); box(n + '_MirrorShell', (2.375, s * 1.15, 1.28), (.09, .17, .40), M_PLAST, .04, 4)
    box(n + '_MirrorGlass', (2.3290, s * 1.15, 1.33), (.012, .135, .22), M_MIR, .001, 1); box(n + '_MirrorGlass2', (2.3290, s * 1.15, 1.155), (.012, .135, .09), M_MIR, .001, 1)
    piv(n, (2.485, s * .992, 1.0)); ik('Handle_%s_Out' % n, (1.712, s * 1.030, .912)); ik('Handle_%s_In' % n, (2.27, s * .922, .908)); ik('Stand_%s' % n, (1.96, s * 1.62, 0))
    gs = [ip(WHt, z) for z in (.99, 1.06, 1.40, 1.47)]                        # on the side, on the pillar behind the door (owner: not over the corner)
    tube('GrabHandle_' + sd, [(1.550, s * (gs[0] + .004), .99), (1.550, s * (gs[1] + .062), 1.03), (1.550, s * (gs[2] + .062), 1.43), (1.550, s * (gs[3] + .004), 1.47)], .012, M_PLAST); ik('Grab_' + sd, (1.550, s * (ip(WHt, 1.23) + .052), 1.23))
    for e, z in enumerate((.99, 1.47)): box('GrabHandle_%s_F%d' % (sd, e), (1.550, s * (ip(WHt, z) + .004), z), (.044, .020, .056), M_PLAST, .008, 3)
    tube('GrabIn_' + sd, [(2.43, s * .850, 1.14), (2.40, s * .835, 1.20), (2.33, s * .825, 1.52), (2.32, s * .84, 1.58)], .011, M_PLAST); box('GrabIn_%s_F0' % sd, (2.45, s * .885, 1.13), (.06, .10, .05), M_PLAST, .012); box('GrabIn_%s_F1' % sd, (2.335, s * .875, 1.59), (.06, .10, .05), M_PLAST, .012); ik('GrabIn_' + sd, (2.365, s * .830, 1.36))
inside('Dashboard', [(Xo(x), Zo(z)) for x, z in rnd([(1.740, .80), (1.46, .86), (1.31, .97), (1.27, 1.08), (1.30, 1.135), (1.42, 1.152), (1.740, 1.152)], [.02, .08, .06, .05, .03, .04, .02], 5)], 'y', -1.3, 1.3, .034, M_DASH)
for s in (-1, 1): boole(bpy.data.objects['Dashboard'], rawprism('c', [(2.0, .5), (2.51, .5), (2.51, 1.3), (2.0, 1.3)], 'y', s * .895, s * 1.4), 'DIFFERENCE', False)
K.finish(rawprism('Dash_Hood', [(Xo(x), Zo(z)) for x, z in rnd([(1.235, 1.262), (1.235, 1.278), (1.40, 1.262), (1.50, 1.14), (1.30, 1.10), (1.275, 1.12), (1.300, 1.255)], [.004, .010, .05, .02, .02, .01, .01], 4)], 'y', DY - .235, DY + .235), M_DASH, 35)
for e in (-1, 1): K.finish(rawprism('Dash_HoodEnd%d' % (e > 0), [(Xo(x), Zo(z)) for x, z in rnd([(1.240, 1.270), (1.39, 1.258), (1.48, 1.14), (1.31, 1.09), (1.262, 1.11)], .015, 3)], 'y', DY + e * .232 - .010, DY + e * .232 + .010), M_DASH, 35)
xf_ = Xo(1.270); zc_ = Zo(1.188); box('Dash_Cluster', (xf_ + .017, DY, zc_), (.050, .445, .135), M_PLAST, .004)
for nm, y, a0, a1 in (('Speed', DY + .098, 0, .5), ('Battery', DY - .098, .5, 1)):
    o = revx('Dash_Dial' + nm, [(0, 0), (.055, 0), (.055, -.004), (0, -.004)], (xf_ - .0070, y, zc_), M_DIAL); CUV.append((o, Vector((xf_, y, zc_)), .112, .112, a0, a1)); c = (xf_ - .0070, y, zc_)
    revx('Dash_Ring' + nm, [(.054, .004), (.054, -.014), (.058, -.017), (.063, -.012), (.063, .004), (.054, .004)], c, M_CHROME); revx('Dash_Dial%s_Glass' % nm, [(0, -.0165), (.0545, -.0150), (.0545, -.0138), (0, -.0153)], c, M_GLASS)
    turn(box('Needle_%s_Arm' % nm, (xf_ - .0145, y, zc_ + .016), (.0016, .0034, .058), M_SRED, .0004, 1), (xf_, y, zc_), 'x', -135 if nm == 'Speed' else 60); revx('Needle_%s_Cap' % nm, [(0, -.0030), (.007, -.0025), (.008, .0065), (0, .0065)], (xf_ - .0140, y, zc_), M_PLAST); piv('Needle_' + nm, (xf_ - .0145, y, zc_))
for i, mt in enumerate((M_AMB, M_RLENS, M_GRN, M_AMB)): box('Dash_Tell%d' % i, (xf_ - .0085, DY, zc_ + .038 - .025 * i), (.004, .014, .014), mt, .002, 1)
ik('Cluster', (xf_ - .02, DY, zc_))
box('Dash_Centre', (Xo(1.265), .02, Zo(1.065)), (.07, .30, .13), M_PLAST, .014, 3); ds = box('Dash_Screen', (Xo(1.228), .02, Zo(1.075)), (.008, .19, .090), M_DISP, .002, 1); CUV.append((ds, Vector((Xo(1.228), .02, Zo(1.075))), .19, .090, 0, 1)); ik('Screen_Stair', (Xo(1.222), .02, Zo(1.075)))
for i in range(5): box('Dash_Btn%d' % i, (Xo(1.230), -.062 + .041 * i, Zo(1.016)), (.010, .030, .016), M_RED if i == 2 else M_DARK, .003)
for i, y in enumerate((-.80, -.25, .83)):
    box('Dash_Vent%d' % i, (Xo(1.283), y, Zo(1.065)), (.06, .11, .075), M_PLAST, .012)
    for k in range(4): turn(box('Dash_Vent%d_S%d' % (i, k), (Xo(1.252), y, Zo(1.040 + .017 * k)), (.012, .09, .004), M_DARK, .001, 1), (Xo(1.252), y, Zo(1.040 + .017 * k)), 'y', 18)
box('Dash_Glove', (Xo(1.275), -.53, Zo(1.035)), (.03, .40, .12), M_DASH, .012); box('Dash_GloveCatch', (Xo(1.2585), -.53, Zo(1.075)), (.010, .10, .020), M_PLAST, .004)
box('Console', (1.93, 0, FLOOR + .165), (.66, .26, .33), M_TRIM, .035, 4); box('Console_Top', (1.93, 0, FLOOR + .333), (.58, .21, .012), M_PLAST, .004)                       # drive selector, park brake, stair and leg levers
for n, x, y, m in (('GearSelect', 2.10, .055, M_CHROME), ('StairLever', 1.96, .055, M_YEL), ('LegLever', 1.96, -.055, M_BLU)):
    cyl('Console_%s_Boot' % n, (x, y, FLOOR + .342), .030, .018, 'z', M_SEAL, 32, .006); cyl(n, (x, y, FLOOR + .385), .008, .09, 'z', M_STEEL, 20); cyl(n + '_Knob', (x, y, FLOOR + .435), .020, .034, 'z', m, 28, .008); ik(n, (x, y, FLOOR + .445)); piv(n, (x, y, FLOOR + .345))
box('ParkBrake', (2.10, -.055, FLOOR + .352), (.06, .035, .03), M_RED, .008); ik('ParkBrake', (2.10, -.055, FLOOR + .37)); cyl('CabEStop_Base', (1.80, .05, FLOOR + .344), .030, .014, 'z', M_YEL, 32, .003); cyl('CabEStop', (1.80, .05, FLOOR + .364), .022, .034, 'z', M_SRED, 32, .006); ik('CabEStop', (1.80, .05, FLOOR + .383))
SC = Vector((Xo(1.085), DY, Zo(1.170))); AXW = Vector((-.7071, 0, .7071))
def tilt(o): o.data.transform(Matrix.Translation(SC) @ AXW.to_track_quat('Z', 'Y').to_matrix().to_4x4()); o.data.update(); return o
_a = _st.index("tilt(torus('SteeringWheel_Rim'"); exec(compile(_st[_a:_st.index("seat('SeatDriver'", _a)], 'small_truck steering wheel and pedals', 'exec'))
seat('SeatDriver', 1.78, DY, .46, depth=.48, back=.58, zc=FLOOR + .31); seat('SeatPassenger', 1.78, -DY, .46, depth=.48, back=.58, zc=FLOOR + .31)
for s, n in ((1, 'D'), (-1, 'P')):
    box('Belt%s_Top' % n, (1.492, s * .905, 1.60), (.05, .06, .07), M_PLAST, .01); box('Belt%s_Web' % n, (1.492, s * .895, 1.12), (.046, .006, .92), M_BELT, .001, 1); box('Belt%s_Reel' % n, (1.492, s * .910, .64), (.09, .09, .16), M_PLAST, .015); box('Belt%s_Tongue' % n, (1.492, s * .888, 1.10), (.05, .012, .07), M_STEEL, .004); ik('BeltTongue_' + n, (1.492, s * .88, 1.10))
    box('SunVisor_' + n, (2.42, s * .42, 1.865), (.15, .46, .020), M_LIN, .008, 3)
    for e in (.24, .60): box('SunVisor_%s_F%d' % (n, e > .5), (2.46, s * e, 1.893), (.05, .03, .06), M_PLAST, .006)
for s, n, py in ((1, 'L', .78), (-1, 'R', -.03)):
    pv = S.surf(py, 1.062, .006); piv('Wiper_' + n, tuple(pv)); cyl('Wiper%s_Hub' % n, tuple(pv), .022, .05, 'z', M_PLAST, 20, .004)
    tube('Wiper%s_Arm' % n, [tuple(S.surf(py, 1.080, .024)), tuple(S.surf(py - .18, 1.112, .030)), tuple(S.surf(py - .38, 1.145, .016))], .008, M_PLAST); tube('Wiper%s_Blade' % n, [tuple(S.surf(py - .04 - .085 * k, 1.125 + .004 * k, .003)) for k in range(9)], .010, M_SEAL); ik('WiperTip_' + n, tuple(S.surf(py - .72, 1.157, .003)))
box('Beacon_Mount', (2.36, 0, 1.975), (.16, .16, .05), M_PLAST, .012, 3); cyl('Beacon_Base', (2.36, 0, 2.005), .072, .05, 'z', M_PLAST, 48, .006)
CK.lathez('BeaconLens', [(0, 2.028), (.060, 2.028)] + [(.063 + (.003 if k % 2 else 0), 2.034 + .006 * k) for k in range(14)] + [(.058, 2.126), (.045, 2.140), (.02, 2.147), (0, 2.148)], (2.36, 0, 0), M_AMB, 64); cyl('Beacon_Cap', (2.36, 0, 2.150), .022, .012, 'z', M_PLAST, 24, .003); ik('Beacon', (2.36, 0, 2.09))
# #endregion

# #region ---- wheels, axles, frame, boxes, deck, jacks, rear lamps
FYW, DYO = .855, .860
for s, sn, f in SIDES:
    wheel('F' + sn, FX, s * FYW, s, .058); wheel('R' + sn, DX, s * DYO, s, .058)
    piv('Wheel_F' + sn, (FX, s * FYW, RW)); piv('Wheel_R' + sn, (DX, s * DYO, RW)); piv('Steer_F' + sn, (FX, s * .68, RW))
    box('Spindle_F' + sn, (FX, s * .68, RW), (.09, .07, .17), M_DARK, .012); cyl('Spindle_F%s_Axle' % sn, (FX, s * .75, RW), .036, .16, 'y', M_STEEL, 28); cyl('SpindleDrum_F' + sn, (FX, s * .79, RW), .125, .08, 'y', M_DARK, 48, .01)
    box('SteerArm_F' + sn, (FX - .08, s * .655, RW - .04), (.15, .035, .025), M_DARK, .008); cyl('AxleBeamF_' + sn, (FX, s * .33, .205), .038, .66, 'y', M_DARK, 28); box('AxleEndF_' + sn, (FX, s * .632, .265), (.09, .045, .18), M_DARK, .010); box('LeafF_' + sn, (FX, s * .42, .238), (.60, .06, .050), M_DARK, .008)
    cyl('DriveAxle_' + sn, (DX, s * .55, RW), .05, .50, 'y', M_DARK, 32); cyl('DriveDrum_' + sn, (DX, s * .785, RW), .130, .06, 'y', M_DARK, 40, .01)
    prism('FenderF_' + sn, [(FX - .42, .30), (FX - .41, .62), (FX - .33, .715), (FX + .36, .715), (FX + .46, .60), (FX + .47, .30), (FX + .482, .30), (FX + .472, .605), (FX + .366, .727), (FX - .336, .727), (FX - .422, .624), (FX - .432, .30)], s * .70, s * .985, M_BUMP)
    box('FenderBarF_' + sn, (FX - .05, s * .56, .700), (.06, .34, .03), M_DARK, .004); box('FenderLegF_' + sn, (FX - .05, s * .42, .55), (.06, .05, .33), M_DARK, .004)
    fender('FenderD_' + sn, DX, s * .748, s * .975, .365, 6, 174); box('MudFlap1_' + sn, (DX - .385, s * .862, .26), (.010, .20, .26), M_SEAL, .003, 1); box('MudFlapBar_' + sn, (DX - .369, s * .82, .37), (.03, .20, .05), M_DARK, .004)
    box('Frame_' + sn, (.44, s * .42, .335), (4.20, .07, .15), M_CHAS, .008, 1)
    cyl('StabHouse_R' + sn, (-2.10, s * .835, .455), .055, .46, 'z', M_FRM, 36, .006); box('StabBracket_R' + sn, (-2.10, s * .775, .42), (.14, .05, .22), M_CHAS, .006)
    box('RearLampBox_' + sn, (-2.33, s * .862, .45), (.09, .215, .20), M_PLAST, .016, 3); box('RearLampArm_' + sn, (-2.26, s * .765, .45), (.10, .03, .12), M_CHAS, .004)
    for nm, y, z, mt, r in (('TailBrake', .810, .50, M_RLENS, .042), ('IndicatorR', .915, .50, M_AMB, .042), ('Reverse', .810, .402, M_WHT, .036), ('RearFog', .915, .402, M_RLENS, .036)):
        if nm == 'RearFog' and s < 0: continue
        c = (-2.3795, s * y, z); ln = 'RearFog' if nm == 'RearFog' else '%s_%s' % (nm, sn)      # pressed lens over a ringed bowl, LED domes in it
        revx(ln + '_Bowl', [(r + .007, .012), (r + .007, -.006), (r + .004, -.010), (r - .003, .0045), (0, .006), (0, .012), (r + .007, .012)], c, M_CHROME)
        l_ = revx(ln + '_Lens', [(0, -.0125)] + [(r * k / 10, -.0125 + .0055 * (k / 10) ** 2 + (.0010 if k % 2 else 0)) for k in range(1, 11)] + [(r, .002), (0, .002)], c, mt); dm = []
        for k in range(7): a = math.pi / 3 * k; q = 0 if k == 6 else r * .58; dm.append(revx('led%d' % k, [(0, -.0170), (.004, -.0160), (.006, -.0125), (.006, -.006), (0, -.006)], (c[0], c[1] + q * math.cos(a), z + q * math.sin(a)), mt, segs=14))
        l_.name = 'tmp_lens'; CK.join(ln + '_Lens', [l_] + dm)
    for i in range(3): box('SideHi%d_%s' % (i, sn), (-.62 + .26 * i, s * .9520, .292), (.23, .006, .042), M_HVR if i % 2 else M_HVW, .001, 1)
    box('SideMarker_%s_Base' % sn, (-2.00, s * .748, .70), (.11, .03, .05), M_PLAST, .008); box('SideMarker_%s_Lens' % sn, (-2.00, s * .766, .70), (.085, .012, .032), M_AMB, .003)
    box('LiftFoot_' + sn, (-.30, s * .815, .640), (.20, .13, .05), M_CHAS, .006)
    for e in (-.055, .055): box('LiftEar%d_%s' % (e > 0, sn), (-.30, s * .815 + e, .685), (.10, .014, .12), M_CHAS, .004)
    cyl('LiftPin_' + sn, (-.30, s * .815, .700), .016, .16, 'y', M_STEEL, 28)
    box('PostF_' + sn, (.42, s * .690, 1.235), (.08, .06, 1.66), M_CHAS, .006, 1); box('PostFFoot_' + sn, (.42, s * .56, .425), (.12, .34, .05), M_CHAS, .006); tube('PostBrace_' + sn, [(-.45, s * .60, .46), (.40, s * .690, 1.50)], .028, M_CHAS, bez=False)
    box('PostM_' + sn, (-.80, s * .700, .84), (.07, .06, .86), M_CHAS, .006, 1); box('PostMFoot_' + sn, (-.80, s * .56, .425), (.12, .34, .05), M_CHAS, .006)
for i, x in enumerate((2.30, 1.44, .10, -.90, -1.62)): box('FrameCross%d' % i, (x, 0, .335), (.08, .80, .10), M_CHAS, .006, 1)
box('RearBeam', (-1.60, 0, .525), (.10, 1.46, .25), M_CHAS, .008, 1); cyl('DriveAxle_Mid', (DX, 0, RW), .05, .62, 'y', M_DARK, 32); cyl('Differential', (DX, 0, RW - .02), .125, .24, 'x', M_DARK, 40, .04); box('Motor', (DX + .26, 0, .31), (.32, .36, .24), M_FRM, .05, 3)
box('DeckPlate', (-.20, 0, .4225), (1.30, 1.38, .022), M_TREAD, .004, 1); box('DeckPlateF', (.925, 0, .4215), (.96, .98, .022), M_TREAD, .004, 1)
BX0, BX1 = -.96, .40
for s, sn, f in SIDES:
    box('SideBox_' + sn, ((BX0 + BX1) / 2, s * .720, .425), (BX1 - BX0, .46, .39), M_BODY, .025, 3); box('SideBoxLid_' + sn, ((BX0 + BX1) / 2, s * .952, .455), (BX1 - BX0 - .10, .012, .27), M_MAIN, .005, 2)
    box('NamePanel_' + sn, (-.28, s * .9605, .47), (.96, .007, .20), M_NAME, .002, 1); ik('Name_' + sn, (-.28, s * .964, .47))
    for i, x in enumerate((BX0 + .12, BX1 - .12)): cyl('SideBoxCatch%d_%s' % (i, sn), (x, s * .960, .572), .012, .012, 'y', M_STEEL, 24, .003)
box('ChargeFlap', (.29, .9600, .42), (.10, .010, .13), M_PLAST, .004); ik('ChargeFlap', (.29, .97, .42))
for k in range(5): box('HydLouvre%d' % k, (.28, -.960, .38 + .022 * k), (.12, .010, .010), M_DARK, .002, 1)
box('GroundPanel', (-.845, -.9620, .42), (.08, .014, .15), M_YEL, .004); cyl('GroundEStop', (-.845, -.981, .455), .020, .030, 'y', M_SRED, 32, .005); ik('GroundEStop', (-.845, -1.0, .455)); ik('Stand_GroundEStop', (-.845, -1.55, 0))
for i, m in enumerate((M_GRN, M_DARK)): cyl('GroundBtn%d' % i, (-.863 + .036 * i, -.972, .385), .012, .014, 'y', m, 24, .003)
for k, x, y in (('FL', 2.875, .80), ('FR', 2.875, -.80), ('RL', -2.10, .835), ('RR', -2.10, -.835)):
    cyl('Stab_%s_Rod' % k, (x, y, .335 if x > 0 else .39), .032, .35 if x > 0 else .46, 'z', M_STEEL, 32, .003); cyl('Stab_%s_Neck' % k, (x, y, .188), .022, .05, 'z', M_STEEL, 24); cyl('Stab_%s_Pad' % k, (x, y, .162), .085, .020, 'z', M_FRM, 40, .006); cyl('Stab_%s_Rubber' % k, (x, y, .148), .080, .014, 'z', M_SEAL, 40, .004); ik('Stab_' + k, (x, y, .141))
# #endregion

# #region ---- the stairs
X0, G, R, T = -2.75, .28, .19, .19 / .28; HO = .032; TH = .022; NL = lambda x: .198 + (x - X0) * T; NU = lambda x: NL(x) + HO                      # nosing lines, lower and upper (at its lowest place)
XP = X0 + 11 * G; PZ = NU(XP); DIR = Vector((G, 0, R)); ANG = math.degrees(math.atan2(R, G)); XE = X0 + 3.30
assert abs(PZ - 2.32) < 1e-6, PZ
pl = lambda pts, f: [(x, f(x) + d) for x, d in pts]
for s, sn, f in SIDES:
    zg = .10; xg = X0 + (zg - .198 + .32) / T
    prism('LowPanel_' + sn, [(X0 - .06, zg), (xg, zg)] + pl([(XE, -.32), (XE, .88), (X0 + .10, .88)], NL) + [(X0 - .02, NL(X0 - .02) + .80), (X0 - .06, NL(X0 - .06) + .66)], s * .700, s * .740, M_BODY, .004, 1)
    prism('LowStringer_' + sn, [(X0 - .04, zg + .012), (xg + .005, zg + .012)] + pl([(XE - .02, -.308), (XE - .02, -.02)], NL) + [(X0 - .04, NL(X0 - .04) - .02)], s * .738, s * .752, M_FRM, .003, 1)
    prism('LowStripe_' + sn, pl([(X0 + .02, .16), (XE - .03, .16), (XE - .03, .42), (X0 + .02, .42)], NL), s * .738, s * .7425, M_MAIN)
    for i in range(5): x = X0 + .45 + i * .66; prism('LowRib%d_%s' % (i, sn), pl([(x, .02), (x + .035, .02), (x + .035, .84), (x, .84)], NL), s * .737, s * .749, M_FRM, .002, 1)
    prism('LowGuide_' + sn, pl([(X0 + .02, .10), (XE - .04, .10), (XE - .04, .13), (X0 + .02, .13)], NL), s * .688, s * .702, M_STEEL)
    tube('LowRail_' + sn, [(x, s * .722, NL(x) + d) for x, d in ((X0 + .06, .60), (X0 - .10, .70), (X0 - .10, .90), (X0 + .04, .985), (X0 + .5, .985), (X0 + 1.2, .985), (X0 + 2.0, .985), (X0 + 2.8, .985), (XE - .02, .985))], .021, M_ALU, res=6)
    for i in range(6): x = X0 + .22 + i * .56; cyl('LowRailPost%d_%s' % (i, sn), (x, s * .722, NL(x) + .92), .010, .13, 'z', M_ALU, 20); ik('Rail_Low_%s_%d' % (sn, i), (x, s * .722, NL(x) + .985))
    cyl('LowRailEnd_' + sn, (XE - .04, s * .722, NL(XE - .04) + .91), .012, .15, 'z', M_ALU, 20)
prism('LowPan', pl([(X0 + .32, -.300), (XE - .03, -.300), (XE - .03, -.282), (X0 + .32, -.282)], NL), -.701, .701, M_FRM)
box('LowSkirt', (X0 - .035, 0, .145), (.022, 1.402, .09), M_CHEV, .004, 1); box('LowSkirtRubber', (X0 - .035, 0, .092), (.014, 1.36, .05), M_SEAL, .003, 1)
for i, (x, w) in enumerate(((X0 + 1.1, .06), (X0 + 2.3, .06), (XE - .06, .05))): box('LowCross%d' % i, (x, 0, NL(x) - .325), (w, 1.42, .06), M_CHAS, .004, 1)
def tread(n, w, dx, t, lip):
    """One tread, its back top edge at the origin, nosing at -dx."""
    Q = [box(n, (-dx / 2 + .001, 0, -t / 2), (dx - .002, w, t), M_TREAD, .003, 1), box(n + '_Nose', (-dx + .022, 0, -t / 2 + .0015), (.044, w - .004, t + .003), M_NOSE, .003, 2)]
    if lip: Q.append(box(n + '_Lip', (-dx + .010, 0, -.026), (.016, w - .006, .034), M_TREAD, .003, 1))
    for i, x in enumerate((-dx + .085, -dx + .145, -dx + .205)): Q.append(box(n + '_Grit%d' % i, (x, 0, .0002), (.036, w - .10, .004), M_GRIT, .001, 1))
    return Q
def joined(n, parts): parts[0].name = 'tmp_' + n; CK.join(n, parts); return bpy.data.objects[n]
def copyat(o, n, loc): c = bpy.data.objects.new(n, o.data); bpy.context.collection.objects.link(c); c.location = loc; ALL.append(c); return c
to = joined('TreadLow_00', tread('tl', 1.376, G, .024, True)); to.location = (X0 + G, 0, NL(X0)); copyat(to, 'TreadLow_01', (X0 + 2 * G, 0, NL(X0 + G)))
tf = joined('TreadLow_02', tread('tf', 1.376, G, .024, True)); tf.data.transform(Matrix.Rotation(math.radians(-ANG), 4, 'Y')); tf.data.update(); tf.location = (X0 + 3 * G, 0, NL(X0 + 2 * G))      # folded: the pose at the lowest height
for k in range(3, 11): copyat(tf, 'TreadLow_%02d' % k, (X0 + G * (k + 1), 0, NL(X0 + G * k)))
for k in range(11):
    x, z = X0 + G * (k + 1), NL(X0 + G * k); cyl('LowHinge_%02d' % k, (x - .004, 0, z - .030), .010, 1.41, 'y', M_STEEL, 20); ik('TreadLow_%02d' % k, (x - G * .55, 0, z))
    if k > 1: piv('TreadLow_%02d' % k, (x, 0, z))
    elif k < 2:
        for s in (-1, 1): box('LowAngle%d_%d' % (k, s > 0), (X0 + G * k + .13, s * .692, z - .035), (.20, .020, .030), M_STEEL, .002, 1)
# the upper flight: closed sawtooth between two side plates, then the platform
us = joined('UpTread_00', tread('ut', 1.296, G + .012, TH, False)); us.location = (X0 + 2 * G, 0, NU(X0 + G))
for j in range(1, 10): copyat(us, 'UpTread_%02d' % j, (X0 + G * (j + 2), 0, NU(X0 + G * (j + 1))))
for j in range(10): ik('TreadUp_%02d' % j, (X0 + G * (j + 1) + G * .45, 0, NU(X0 + G * (j + 1)))); box('UpRiser_%02d' % j, (X0 + G * (j + 2) + .002, 0, NU(X0 + G * (j + 1)) + R / 2 - .012), (.006, 1.294, R - .010), M_FRM, .001, 1)
xu = X0 + G; xa = xu - .45; zu = NU(xu) - .020
for s, sn, f in SIDES:
    prism('UpPlate_' + sn, pl([(xa + .06, .006), (xu - .002, .006)], NU) + [(xu + .004, zu), (xu + .158 / T, zu), (XP + .078 / T, PZ - .10), (XP + .31, PZ - .10), (XP + .31, PZ + .98), (XP + .147, PZ + .98)] + pl([(xa + .10, .88), (xa + .02, .80), (xa, .66), (xa, .10)], NU), s * .645, s * .665, M_BODY, .003, 1)
    prism('UpKick_' + sn, pl([(xu + .02, .03), (XP - .02, .03), (XP - .02, .17), (xu + .02, .17)], NU), s * .6385, s * .6465, M_STEEL)
    tube('UpRail_' + sn, [(x, s * .655, NU(x) + d) for x, d in ((xa + .10, .70), (xa - .03, .80), (xa - .02, .92), (xa + .08, .98), (xa + .5, .98), (xa + 1.1, .98), (xa + 1.8, .98), (xa + 2.5, .98), (XP - .08, .98))] + [(XP + .06, s * .655, PZ + 1.05), (XP + .18, s * .655, PZ + 1.08), (XP + .33, s * .655, PZ + 1.08)], .019, M_ALU, res=6)
    for i in range(6): x = xa + .20 + i * .56; cyl('UpRailPost%d_%s' % (i, sn), (x, s * .655, NU(x) + .92), .009, .13, 'z', M_ALU, 20); ik('Rail_Up_%s_%d' % (sn, i), (x, s * .655, NU(x) + .98))
    cyl('UpRailPost6_' + sn, (XP + .22, s * .655, PZ + 1.02), .009, .13, 'z', M_ALU, 20)
    for i, x in enumerate((xa + .10, xa + .34)): cyl('UpRoller%d_%s' % (i, sn), (x, s * .676, NU(x) + .127), .027, .020, 'y', M_SEAL, 32, .003); cyl('UpRollerPin%d_%s' % (i, sn), (x, s * .668, NU(x) + .127), .008, .046, 'y', M_STEEL, 16)
    for j in range(0, 10, 2): x = xu + G * j + .17; z = NU(x) + .23; box('UpStepBase%d_%s' % (j, sn), (x, s * .642, z), (.12, .010, .040), M_PLAST, .004); box('StepLight_Up_%s%d' % (sn, j), (x, s * .6365, z), (.10, .004, .022), M_STEP, .001, 1)
    for k in (0, 1): x = X0 + G * k - .02 + .28 * (k > 0); x = X0 + (.06 if k == 0 else .20); box('LowStepBase%d_%s' % (k, sn), (x, s * .6975, NL(x) + .22), (.10, .010, .040), M_PLAST, .004); box('StepLight_Low_%s%d' % (sn, k), (x, s * .692, NL(x) + .22), (.08, .004, .022), M_STEP, .001, 1)
PX0, PX1 = XP, XP + 1.50; PH = 1.08                                             # the platform
box('UpDeck', ((PX0 - .012 + PX1) / 2, 0, PZ - .012), (PX1 - PX0 + .012, 1.296, .024), M_TREAD, .003, 1); box('UpDeckNose', (PX0 + .007, 0, PZ - .0105), (.044, 1.292, .027), M_NOSE, .003, 2)
for s in (-1, 1): box('UpDeckWing%d' % (s > 0), ((PX0 + .30 + PX1) / 2, s * .800, PZ - .0125), (PX1 - PX0 - .30, .306, .025), M_TREAD, .003, 1)
box('UpDeckFrame', ((PX0 + .31 + PX1) / 2, 0, PZ - .0785), (PX1 - PX0 - .31, 1.896, .115), M_FRM, .008, 2); box('UpDeckFrameRear', (PX0 + .20, 0, PZ - .060), (.24, 1.30, .075), M_FRM, .004, 1)
box('UpFrontGrit', (PX1 - .10, 0, PZ + .0005), (.10, 1.74, .005), M_GRIT, .001, 1); box('UpFrontNose', (PX1 - .022, 0, PZ + .001), (.040, 1.88, .006), M_NOSE, .001, 1)
prism('UpBumper', rnd([(PX1 - .01, PZ - .135), (PX1 + .07, PZ - .135), (PX1 + .12, PZ - .10), (PX1 + .12, PZ - .04), (PX1 + .07, PZ - .006), (PX1 - .01, PZ - .006)], [.01, .03, .03, .03, .03, .01], 5), -.95, .95, M_SEAL)
for i, z in enumerate((PZ - .052, PZ - .088)): box('UpBumperRib%d' % i, (PX1 + .118, 0, z), (.012, 1.88, .014), M_SEAL, .004, 2)
box('UpFrameChev_F', (PX1 - .010, 0, PZ - .157), (.030, 1.88, .045), M_CHEV, .003, 1)
for s, sn, f in SIDES:
    box('UpBack_' + sn, (XP + .300, s * .7975, PZ + .42), (.024, .305, .98), M_BODY, .004, 1)
    box('UpSide_' + sn, (XP + .735, s * .938, PZ + .42), (.85, .024, .98), M_BODY, .004, 1); box('UpSideStripe_' + sn, (XP + .735, s * .9505, PZ + .10), (.82, .006, .16), M_MAIN, .001, 1)
    box('NamePanel_Plat_' + sn, (XP + .735, s * .952, PZ + .55), (.76, .006, .28), M_NAME, .002, 1); ik('Name_Plat_' + sn, (XP + .735, s * .956, PZ + .55))
    tube('UpTopRail_' + sn, [(XP + .33, s * .655, PZ + PH), (XP + .33, s * .80, PZ + PH), (XP + .345, s * .905, PZ + PH), (XP + .42, s * .938, PZ + PH), (XP + .80, s * .938, PZ + PH), (XP + 1.14, s * .938, PZ + PH)], .019, M_ALU, res=6); ik('Rail_Plat_' + sn, (XP + .75, s * .938, PZ + PH))
    for i, x in enumerate((XP + .46, XP + .80, XP + 1.12)): cyl('UpTopPost%d_%s' % (i, sn), (x, s * .938, PZ + .99), .010, .19, 'z', M_ALU, 20)
    for i, z in enumerate((PZ + .16, PZ + .74)): box('UpGateRail%d_%s' % (i, sn), (XP + .76, s * .918, z), (.76, .022, .030), M_STEEL, .003, 1)
    box('GateSlide_%s' % sn, (XP + 1.03, s * .894, PZ + .46), (.90, .020, .84), M_BODY, .006, 2)                                              # the sliding gate: forward 0.40
    for i, z in enumerate((PZ + .16, PZ + .74)): box('GateSlide_%s_Shoe%d' % (sn, i), (XP + .72, s * .906, z), (.12, .016, .040), M_DARK, .004)
    tube('GateSlide_%s_Frame' % sn, [(XP + .60, s * .894, PZ + .86), (XP + .60, s * .894, PZ + 1.00), (XP + 1.46, s * .894, PZ + 1.00), (XP + 1.46, s * .894, PZ + .06)], .016, M_YEL, bez=False); box('GateSlide_%s_Rubber' % sn, (XP + 1.492, s * .894, PZ + .50), (.040, .040, .86), M_SEAL, .014, 3)
    tube('GateSlide_%s_Handle' % sn, [(XP + 1.24, s * .886, PZ + .62), (XP + 1.24, s * .845, PZ + .64), (XP + 1.24, s * .845, PZ + .80), (XP + 1.24, s * .886, PZ + .82)], .012, M_STEEL); ik('Handle_Gate_' + sn, (XP + 1.24, s * .845, PZ + .72)); ik('Stand_Gate_' + sn, (XP + .96, s * .50, PZ)); box('GateSlide_%s_Chev' % sn, (XP + 1.37, s * .8830, PZ + .46), (.12, .006, .78), M_CHEV, .001, 1)
    box('UpLiftBlock_' + sn, (XP + .45, s * .815, PZ - .152), (.16, .15, .04), M_CHAS, .004)
    for e in (-.055, .055): box('UpLiftEar%d_%s' % (e > 0, sn), (XP + .45, s * .815 + e, PZ - .195), (.08, .014, .09), M_CHAS, .004)
    cyl('UpLiftPin_' + sn, (XP + .45, s * .815, PZ - .20), .014, .15, 'y', M_STEEL, 28)
    A, B = Vector((-.30, s * .815, .700)), Vector((XP + .45, s * .815, PZ - .20)); D = B - A; L = D.length; q = Matrix.Translation(A) @ D.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    for i, (r, z0, z1) in enumerate(((.054, .05, 1.60), (.045, .08, 1.66), (.036, .11, 1.72), (.027, .14, L))): o = cyl('Lift_%s_%d' % (sn, i), (0, 0, (z0 + z1) / 2), r, z1 - z0, 'z', M_FRM if i == 0 else M_CHROME, 40, .004); o.data.transform(q); o.data.update()
    for n_, z, r in (('0_Eye', 0, .040), ('3_Eye', L, .034)): o = cyl('Lift_%s_%s' % (sn, n_), (0, 0, 0), r, .075, 'y', M_DARK, 36, .004); o.data.transform(Matrix.Translation(A + D.normalized() * z)); o.data.update()
    piv('Lift_' + sn, tuple(A)); ik('LiftTop_' + sn, tuple(B)); text('Word_Rail_' + sn, 'HOLD THE HANDRAIL', (XP + .735, s * .9535, PZ + .10), .058, f, M_SWHT)
box('UpPanel', (XP + .42, .9145, PZ + .45), (.14, .030, .22), M_FRM, .006); cyl('UpEStop', (XP + .42, .890, PZ + .51), .022, .034, 'y', M_SRED, 32, .005); ik('PlatEStop', (XP + .42, .870, PZ + .51))
for i in range(2): cyl('UpBtn%d' % i, (XP + .395 + .05 * i, .897, PZ + .40), .013, .016, 'y', M_GRN, 24, .003); ik('Plat_Btn%d' % i, (XP + .395 + .05 * i, .885, PZ + .40))
for n, l in (('Plat_Nose', (XP, 0, PZ)), ('Plat_Mid', (XP + .75, 0, PZ)), ('Plat_Front', (PX1 + .12, 0, PZ)), ('Plat_Agent', (XP + .60, -.60, PZ)), ('Foot', (X0 - .55, 0, 0)), ('StairTop', (XP + 10 * G, 0, PZ + 10 * R)), ('Aisle_Row0', (1.78, 0, FLOOR))): ik(n, l)
# #endregion

# #region ---- checks, joining, saving (the small trucks' own routine)
_f = _st[_st.index('def finish_truck('):].replace('K.uv_cube();', 'K.uv_cube(); AFTER_UV();').replace('(2.590, .985, 1)', '(2.485, .992, 1)').replace('-38, 38)', '-35, 35)').replace("n.startswith(('Tyre_', 'HoseTurn'))", "n.startswith(('Tyre_', 'TreadLow'))")
_f = _f.replace('((6.2, 5.2, 2.4), (.1, 0, 1.1), 42), ((-6.4, 4.6, 3.0), (-.3, 0, 1.1), 42), ((-4.6, -.6, 2.9), (-1.7, -.1, 1.3), 30), ((-5.0, -5.0, 2.6), (-.4, 0, 1.1), 40)', '((6.8, 4.6, 2.6), (.1, 0, 1.5), 38), ((-6.6, 4.6, 3.2), (-.3, 0, 1.5), 38), ((1.62, .46, 1.56), (2.45, .12, 1.10), 15), ((4.6, 1.9, 1.1), (2.7, .45, .75), 50)')
exec(compile(_f, 'small_truck finish', 'exec'))
UPS = ('Up', 'StepLight_Up', 'GateSlide', 'NamePanel_Plat', 'Word_Rail')
def stairs_check():
    rot = Matrix.Rotation(math.radians(ANG), 4, 'Y'); bpy.context.view_layer.update()
    for f in (0, .03, .35, .7, .97, 1, 2.5, 4, 6, 8.02, 9.5, 10):
        W = {}
        for o in ALL:
            if o.name.startswith('Lift_'): continue
            m = Matrix.Translation(DIR * f) @ o.matrix_world if o.name.startswith(UPS) else o.matrix_world
            if o.name.startswith('TreadLow_') and 2 <= int(o.name[-2:]) <= 1 + math.floor(f + 1e-6): m = Matrix.Translation(o.location) @ rot
            W[o.name] = BVHTree.FromPolygons([m @ v.co for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
        h = [(a, b) for a in W if a.startswith(UPS) for b in W if not b.startswith(UPS) and W[a].overlap(W[b])]; print(TAG, 'STAIR f=%.2f platform %.2f' % (f, PZ + R * f), len(h), h[:8])
        for sn in 'LR':                                                          # the lift cylinder's line against everything that does not move with it
            A, B = bpy.data.objects['PIV_Lift_' + sn].location, bpy.data.objects['IK_LiftTop_' + sn].location + DIR * f; n_ = set()
            for k in range(8, 93):
                p = A.lerp(B, k / 100)
                for a in W:
                    if a.startswith(('LiftE', 'LiftP', 'LiftF', 'UpLift')): continue
                    if W[a].find_nearest(p, .056)[0] is not None: n_.add(a)
            if n_: print(TAG, 'LIFT %s f=%.2f' % (sn, f), sorted(n_)[:6])
        h = [(a, b) for a in W if a.startswith('TreadLow') for b in W if b != a and b.startswith('TreadLow') and W[a].overlap(W[b])]
        if h: print(TAG, 'TREADS', f, h[:4])
if '--nochk' not in sys.argv: stairs_check()
def gx(n):
    for k in ('FL', 'FR', 'RL', 'RR'):
        if n.startswith('Stab_' + k): return 'Stab_' + k
    for k in ('L', 'R'):
        for i in range(4):
            if n.startswith('Lift_%s_%d' % (k, i)): return 'Lift_%s_%d' % (k, i)
        if n.startswith('GateSlide_' + k): return 'GateSlide_' + k
    for k in ('Speed', 'Battery'):
        if n.startswith('Needle_' + k): return 'Needle_' + k
        if n == 'Dash_Dial%s_Glass' % k: return 'ClusterGlass'
    if n.startswith(('GearSelect', 'StairLever', 'LegLever')): return 'Lever_' + n.split('_')[0]
    if n.startswith('StepLight_'): return 'StepLights_Up' if n[10] == 'U' else 'StepLights_Low'
    if n.startswith(('Console', 'CabEStop')): return 'CabFittings'
    if n.startswith(UPS): return 'StairUp'
    if n.startswith('Low') and not n.startswith('LowBeam'): return 'StairLow'
    if n.startswith(('Post', 'LiftF', 'LiftE', 'LiftP', 'DeckPlate', 'Stab', 'Hyd')): return 'Underside'
    return None
finish_truck([], lambda n: gx(n), [('DoorPassenger', (2.485, -.992, 1), 'z', (15, 40, 68), ())], [(('GateSlide_L', 'GateSlide_R'), (1, 0, 0), (.0, .2, .4), ('UpGateRail',))], {
    'stairs': {'rise': R, 'going': G, 'angle_deg': round(ANG, 3), 'platform_min': round(PZ, 3), 'platform_max': round(PZ + 10 * R, 3), 'slide_dir': [round(c, 5) for c in DIR.normalized()], 'slide_len': round(10 * DIR.length, 4), 'lower_nosing_x0': X0, 'lower_tread0_z': .198, 'upper_first_nosing': [round(xu, 3), round(NU(xu), 3)],
               'platform_x': [round(PX0, 3), round(PX1 + .12, 3)], 'platform_half_width': .95, 'clear_width': 1.277, 'handrail_above_nosing': .985, 'gate_slide': .40, 'jack_stroke': .141, 'tread_fold_deg': round(ANG, 3)}})
# #endregion
