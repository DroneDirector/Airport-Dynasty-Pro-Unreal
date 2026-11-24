# The aircraft refueller (to-do 212): a rigid 6x2 tanker for A320-class aircraft, 20 m3, our own shape, no brand.
# Published sizes of the 20 000 l rigid class: 10 200 x 2 500 x 3 250 (chinatanktruck.com), 10 525 x 2 450 x 3 650 (flofuel-sp.co.uk);
# tyres 315/70 R22.5 (dia 1.014), low day cab, 2 reels of 20 m, hydraulic rear platform with fixed steps. Shape seen in photos
# (named on docs/Blueprints/Apron/Refueller.png). Cab | meter cabinet (pump panel left, locker right) | tank | platform + reels.
# Axes: x forward, y left, z up; metres; origin on the ground under the middle. Driver front left. Names as GSE.cpp uses them.
#   blender -b --python Art/Refueller/make_refueller.py [-- --pic DIR]
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, 'Tools'), os.path.join(ROOT, 'Art', 'CleaningKit')]
import vehicle_kit as K
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, ALL, IKS
ART = os.path.join(ROOT, 'Art', 'Refueller'); TEX = os.path.join(ROOT, 'Art', 'PushbackTug', 'Textures')
K.reset()
import make_cleaningkit as CK
import vehicle_skin as S
from vehicle_skin import rawprism, boole, patch, grow, rnd, ip
CK.join.__globals__['K'] = K
for _o in list(ALL): bpy.data.objects.remove(_o)
ALL.clear(); IKS.clear()
FX, DX, TX, RW, TW = 3.70, -1.20, -2.55, .507, .30               # front, drive (twin), tag (steered) axles; wheelbase 4.90
FLOOR, CABB = 1.18, 3.05                                          # the cab's floor; its back wall
ox, oz = 3.21, FLOOR - .575                                       # (the van's cab fittings, moved into this cab)

# #region ---- materials
P = K.pmat
M_BODY = P('RF_Livery_Body_Paint', (.90, .91, .92), .1, .35); M_MAIN = P('RF_Livery_Main_Paint', (.03, .20, .36), .1, .35); M_NAME = P('RF_NamePanel_Paint', (.92, .92, .92), 0, .4)
M_BUMP = P('RF_BumperPlastic', (.05, .05, .055), 0, .62); M_PLAST = P('RF_BlackPlastic', (.035, .035, .04), 0, .55); M_DASH = P('RF_DashPlastic', (.10, .105, .115), 0, .7)
M_LIN = P('RF_HeadLining', (.60, .60, .58), 0, .9); M_TRIM = P('RF_GreyTrimPlastic', (.30, .31, .33), 0, .75); M_GLASS = P('RF_Glass', (.2, .3, .35), 0, .05); M_SEAL = P('RF_SealRubber', (.02, .02, .02), 0, .85)
M_TYRE = P('RF_Tyre', (.03, .03, .03), 0, .9); M_STEEL = P('RF_Steel', (.5, .51, .53), 1, .4); M_DARK = P('RF_DarkSteel', (.09, .09, .1), .6, .55); M_RIM = P('RF_RimSteel', (.62, .63, .65), .9, .35)
M_CHAS = P('RF_Chassis_Paint', (.06, .065, .07), .3, .5); M_FRM = P('RF_Platform_Paint', (.36, .38, .40), .4, .5); M_YEL = P('RF_SafetyYellow_Paint', (.95, .72, .02), .05, .4); M_ALU = P('RF_Aluminium', (.72, .73, .75), .9, .4)
M_DECK = P('RF_DiamondPlate', (.55, .56, .57), .9, .5); M_MAT = P('RF_FloorMat', (.05, .05, .05), 0, .9); M_SEAT = P('RF_SeatFabric', (.06, .065, .075), 0, .85); M_SEAT2 = P('RF_SeatBolster', (.03, .06, .14), 0, .85)
M_HOSE = P('RF_HoseRubber', (.025, .025, .027), 0, .7); M_CAB = P('RF_CableGreenYellow', (.45, .65, .06), 0, .5); M_BRASS = P('RF_Brass', (.75, .55, .22), 1, .3); M_RED = P('RF_RedPaint', (.70, .03, .02), .05, .35)
M_AMB = P('RF_AmberLens', (1, .5, .04), 0, .2, 1.0); M_RLENS = P('RF_RedLens', (.85, .04, .03), 0, .2, .6); M_WHT = P('RF_WhiteLens', (.95, .95, .92), 0, .1, 1.0); M_CHROME = P('RF_Reflector', (.8, .8, .82), 1, .12); M_MIR = P('RF_MirrorGlass', (.8, .85, .9), 1, .02)
M_SCR = P('RF_Screen', (.03, .10, .14), 0, .1, .4); M_LCD = P('RF_MeterLcd', (.55, .64, .50), 0, .2, .3); M_HVR = P('RF_HiVisRedReflector', (.75, .03, .02), 0, .35); M_HVW = P('RF_HiVisWhiteReflector', (.88, .9, .9), 0, .3)
M_BELT = P('RF_BeltWebbing', (.02, .02, .022), 0, .8); M_SBLK = P('RF_SignBlack', (.01, .01, .012), 0, .5); M_SWHT = P('RF_SignWhite', (.95, .95, .95), 0, .5); M_SORG = P('RF_SignOrange', (1.0, .38, .02), 0, .5); M_SRED = P('RF_SignRed', (.80, .02, .02), 0, .5); M_GRN = P('RF_GreenPlastic', (.03, .42, .12), 0, .5); M_BLU = P('RF_SignBlue', (.02, .18, .55), 0, .5)
for m, t in ((M_STEEL, None), (M_DARK, (.25, .25, .27)), (M_RIM, (1.05, 1.06, 1.08)), (M_ALU, (1.3, 1.3, 1.32))): K.photo(m, TEX, 'Metal027', tile=.6, tint=t)
K.photo(M_TYRE, TEX, 'Rubber004', tile=.5, tint=(.35, .35, .35), metal=0.0); K.photo(M_MAT, TEX, 'Rubber001', tile=.5, tint=(.4, .4, .4), metal=0.0); K.photo(M_DECK, TEX, 'DiamondPlate008A', tile=.5); K.photo(M_HOSE, TEX, 'Rubber001', tile=.3, tint=(.28, .28, .28), metal=0.0)
for m, t in ((M_BUMP, (.40, .40, .42)), (M_PLAST, (.32, .32, .34)), (M_DASH, (.75, .77, .8))): K.photo(m, TEX, 'Plastic006', tile=.4, tint=t, metal=0.0)
def ik(n, loc): return K.ik(n, loc)
def piv(n, loc): return K.ik(n, loc, prefix='PIV_')
def text(name, s, c, size, face, m, ex=.0016, align='CENTER'):
    """Raised lettering, middle at c, standing on a face ('y+', 'y-', 'x-', 'x+'); sits 1.1 mm in it."""
    cu = bpy.data.curves.new(name, 'FONT'); cu.body = s; cu.size = size; cu.extrude = max(ex, .0012); cu.align_x = align; cu.align_y = 'CENTER'; cu.resolution_u = 4
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o); X, Z = {'y+': ((-1, 0, 0), (0, 1, 0)), 'y-': ((1, 0, 0), (0, -1, 0)), 'x-': ((0, -1, 0), (-1, 0, 0)), 'x+': ((0, 1, 0), (1, 0, 0))}[face]
    X, Z = Vector(X), Vector(Z); R = Matrix((X, (0, 0, 1), Z)).transposed().to_4x4()
    for x in bpy.context.selected_objects: x.select_set(False)
    bpy.context.view_layer.objects.active = o; o.select_set(True); bpy.ops.object.convert(target='MESH'); o = bpy.context.view_layer.objects.active; o.select_set(False)
    bb = [Vector(b) for b in o.bound_box]; mid = Vector(((bb[0].x + bb[6].x) / 2, (bb[0].y + bb[6].y) / 2, 0))
    ex = max(ex, .0012); o.data.transform(Matrix.Translation(Vector(c) + Z * (ex - .0011)) @ R @ Matrix.Translation(-mid)); o.data.update(); return K.finish(o, m, 20)
def plate(name, c, w, h, face, m, t=.006, bev=.0015):
    """A flat sign plate whose front is t in front of the face point c (half of it in the face)."""
    s = (w, t * 2, h) if face[0] == 'y' else (t * 2, w, h); return box(name, c, s, m, bev, 1)
def off(c, face, d):
    v = Vector(c); i = 'xyz'.index(face[0]); v[i] += d * (1 if face[1] == '+' else -1); return tuple(v)
def facepoly(name, pts, c, face, m, t=.0016, emb=.0010):
    """A flat outline (u right, v up as seen) laid on a face at c."""
    bm = bmesh.new(); i = 'xyz'.index(face[0]); sg = 1 if face[1] == '+' else -1; ux = {'y+': (-1, 0, 0), 'y-': (1, 0, 0), 'x-': (0, -1, 0), 'x+': (0, 1, 0)}[face]
    U = Vector(ux); n = Vector((0, 0, 0)); n[i] = sg; C = Vector(c); A = [bm.verts.new(C + U * u + Vector((0, 0, v)) - n * emb) for u, v in pts]; B = [bm.verts.new(C + U * u + Vector((0, 0, v)) + n * max(t, .0012)) for u, v in pts]
    bm.faces.new(A); bm.faces.new(B)
    for k in range(len(pts)): bm.faces.new((A[k], A[(k + 1) % len(pts)], B[(k + 1) % len(pts)], B[k]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); return K.finish(K.link(name, bm), m, 20)
# #endregion

# #region ---- cab: one lofted skin, doors, panes and lamps cut out of it
XFt = [(.40, 4.98), (.55, 5.045), (.75, 5.07), (1.30, 5.065), (1.70, 5.03), (1.90, 4.995), (2.30, 4.905), (2.60, 4.825), (2.74, 4.76), (2.83, 4.66), (2.89, 4.52), (2.92, 4.30), (3.0, 4.0)]
XRt = [(.40, CABB), (2.70, CABB), (2.82, CABB + .03), (2.89, CABB + .09), (2.92, CABB + .25), (3.0, CABB + .5)]
WHt = [(.40, 1.185), (.55, 1.215), (.80, 1.225), (1.80, 1.225), (2.30, 1.205), (2.60, 1.175), (2.74, 1.135), (2.83, 1.06), (2.89, .93), (2.92, .70), (3.0, .3)]
LNt = [(.40, .26), (1.8, .26), (2.6, .30), (2.92, .40), (3.0, .4)]
ZSt = [.40, .46, .52, .60, .68, .76, .84, .92, 1.00, 1.08, 1.16, 1.25, 1.35, 1.45, 1.55, 1.65, 1.72, 1.80, 1.88, 1.96, 2.05, 2.14, 2.23, 2.32, 2.41, 2.50, 2.58, 2.65, 2.70, 2.74, 2.77, 2.80, 2.83, 2.855, 2.875, 2.89, 2.902, 2.912, 2.92]
S.setup(XFt, XRt, WHt, LNt, ZSt, 2.72, [M_BODY, M_MAIN, M_BUMP, M_LIN, M_TRIM, M_SEAL], M_GLASS, M_SEAL, rr=.10)
SH = .045
shell = S.loft('Body', 0.0); inner = S.loft('inner', SH, bot=.5, mi=3)
for p in inner.data.polygons:
    if p.center.z < 1.95: p.material_index = 4
boole(shell, inner, 'DIFFERENCE', False)
for p in shell.data.polygons:
    c = p.center
    if p.material_index == 0 and abs(p.normal.z) < .9:
        if 1.35 < c.z < 1.65: p.material_index = 1
        if c.z < .92: p.material_index = 2
boole(shell, rawprism('c', [(2.0, .2), (4.35, .2), (4.35, 1.115), (2.0, 1.115)], 'y', -2, 2), 'DIFFERENCE', False)   # over the wheels and behind: open
for s in (-1, 1): boole(shell, rawprism('c', [(4.2, .44), (4.74, .44), (4.74, 1.117), (4.2, 1.117)], 'y', s * .84, s * 1.5), 'DIFFERENCE', False)      # step wells
DOORF = rnd([(3.60, 1.14), (4.745, 1.14), (4.745, 1.74), (4.60, 2.62), (3.60, 2.62)], [.06, .06, .12, .10, .08]); GAP = .004
shell0 = S.copy(shell, 'shell0')
def opening(name, pts, axis, a, b):
    pan = S.copy(shell0, name); boole(pan, rawprism('c', grow(pts, -GAP), axis, a, b), 'INTERSECT', False); boole(shell, rawprism('c', pts, axis, a, b), 'DIFFERENCE', False); return pan
doorD = opening('DoorDriver', DOORF, 'y', .5, 1.6); doorP = opening('DoorPassenger', DOORF, 'y', -1.6, -.5); S.rm(shell0)
WSC = rnd([(-1.08, 1.80), (1.08, 1.80), (1.00, 2.60), (-1.00, 2.60)], [.07, .07, .10, .10]); boole(shell, rawprism('c', WSC, 'x', 4.0, 5.6), 'DIFFERENCE', False)
WD = rnd([(3.69, 1.88), (4.655, 1.80), (4.525, 2.54), (3.69, 2.54)], .06)
for o, s in ((doorD, 1), (doorP, -1)): boole(o, rawprism('c', WD, 'y', s * .5, s * 1.6), 'DIFFERENCE', False)
HL = rnd([(.66, .58), (1.06, .58), (1.09, .80), (.66, .80)], .04)
for s in (-1, 1): boole(shell, rawprism('c', [(s * y, z) for y, z in HL], 'x', 4.4, 5.6), 'DIFFERENCE', False)
for o in (shell, doorD, doorP):
    used = sorted({p.material_index for p in o.data.polygons}); ms = [o.data.materials[i] for i in used]; ix = [used.index(p.material_index) for p in o.data.polygons]; o.data.materials.clear()
    for m in ms: o.data.materials.append(m)
    for p, i in zip(o.data.polygons, ix): p.material_index = i; p.use_smooth = True
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(32); ALL.append(o)
S.pane('Windscreen', WSC, 'x', 4.0, 5.6); S.pane('DoorDriver_Window', WD, 'y', .5, 1.6); S.pane('DoorPassenger_Window', WD, 'y', -1.6, -.5)
for s, sn in ((1, 'L'), (-1, 'R')):
    h = [(s * y, z) for y, z in HL]
    patch('Headlamp_%s_Bezel' % sn, grow(h, .012), 'x', 4.4, 5.6, M_PLAST, -.004, .05, hole=grow(h, -.012)); patch('Headlamp_%s_Bowl' % sn, grow(h, -.008), 'x', 4.4, 5.6, M_CHROME, .022, .044)
    for nm, y0, y1, mt in (('LowBeam', .675, .82, M_WHT), ('HighBeam', .825, .945, M_WHT), ('IndicatorF', .95, 1.08, M_AMB)):
        o = patch('%s_%s_Lens' % (nm, sn), grow(h, -.010), 'x', 4.4, 5.6, mt, .001, .006); boole(o, rawprism('c', [(s * y0, .3), (s * y1, .3), (s * y1, 1.2), (s * y0, 1.2)], 'x', 4, 6), 'INTERSECT', False); K.finish(o, mt, 32); ALL.remove(o); assert len(o.data.polygons), nm
    patch('CabSideMarker_' + sn, rnd([(3.20, 1.24), (3.34, 1.24), (3.34, 1.30), (3.20, 1.30)], .015, 3), 'y', s * .5, s * 1.6, M_AMB, -.010, .004)
    for i, (x0, x1) in enumerate(((3.63, 3.98), (3.99, 4.35), (4.36, 4.71))): patch(('DoorDriver' if s > 0 else 'DoorPassenger') + '_HiVis%d' % i, [(x0, 1.20), (x1, 1.20), (x1, 1.28), (x0, 1.28)], 'y', s * .5, s * 1.6, M_HVR if i % 2 else M_HVW, -.0025, .006)
patch('Grille_Field', rnd([(-.80, 1.02), (.80, 1.02), (.86, 1.30), (-.86, 1.30)], .05), 'x', 4.6, 5.6, M_PLAST, -.004, .02)
for i in range(6): patch('Grille_Slat%d' % i, [(-.76 - .006 * i, 1.05 + .042 * i), (.76 + .006 * i, 1.05 + .042 * i), (.76 + .006 * i, 1.066 + .042 * i), (-.76 - .006 * i, 1.066 + .042 * i)], 'x', 4.6, 5.6, M_DARK, -.016, .002)
patch('LowerGrille', rnd([(-.50, .48), (.50, .48), (.50, .66), (-.50, .66)], .04), 'x', 4.6, 5.6, M_PLAST, -.004, .02); patch('PlateFront', [(-.26, .72), (.26, .72), (.26, .83), (-.26, .83)], 'x', 4.7, 5.6, M_NAME, -.005, .006)
patch('NamePanel_Front', rnd([(-.62, 1.70), (.62, 1.70), (.62, 1.785), (-.62, 1.785)], .02, 3), 'x', 4.7, 5.6, M_NAME, -.004, .006)
box('FrontTowEye', (5.04, -.40, .50), (.10, .05, .10), M_YEL, .012)
# floor, risers, steps, back wall things
fl = box('Floor', (4.015, 0, FLOOR - .025), (1.84, 2.32, .05), M_TRIM, .004, 1)
for s in (-1, 1): cut(fl, box('c', (4.505, s * 1.14, FLOOR), (.45, .26, .3), M_TRIM))
box('CabMat', (4.02, 0, FLOOR + .003), (1.80, 1.96, .012), M_MAT, .004); box('EngineHump', (3.72, 0, FLOOR + .13), (1.26, .62, .27), M_TRIM, .06, 3)
cw = rawprism('Cowl', [(-1.3, 1.755), (1.3, 1.755), (1.3, 1.775), (-1.3, 1.775)], 'x', 4.70, 5.4); boole(cw, rawprism('c', [(4, 1.6), (5.6, 1.6), (5.6, 1.9), (4, 1.9)], 'y', -1.12, 1.12), 'INTERSECT', False); boole(cw, S.LF(.041), 'INTERSECT'); K.finish(cw, M_PLAST, 32)
fw = rawprism('Firewall', [(-1.3, FLOOR - .05), (1.3, FLOOR - .05), (1.3, 1.765), (-1.3, 1.765)], 'x', 4.935, 4.955); boole(fw, S.LF(.041), 'INTERSECT'); K.finish(fw, M_PLAST, 30)
patch('CabLiner', [(3.10, 2.66), (4.62, 2.66), (4.62, 3.1), (3.10, 3.1)], 'y', -1.4, 1.4, M_LIN, .041, .057)
box('RoofLight_Base', (4.0, 0, 2.850), (.30, .12, .03), M_PLAST, .006); box('RoofLight_Glass', (4.0, 0, 2.834), (.28, .10, .012), M_NAME, .004)
for s, sn, n in ((1, 'L', 'Driver'), (-1, 'R', 'Passenger')):
    box('Step%s_Riser' % n, (4.545, s * .845, .78), (.395, .016, .70), M_BUMP, .003, 1)
    for e, x in ((0, 4.353), (1, 4.735)): box('Step%s_End%d' % (n, e), (x, s * 1.025, .78), (.016, .36, .70), M_BUMP, .003, 1)
    for k, (z, y0) in enumerate(((.47, .95), (.81, .90))): box('Step%s_Tread%d' % (n, k), (4.545, s * (y0 + 1.205) / 2, z), (.385, 1.205 - y0, .035), M_DECK, .006, 2)
    ik('Step_Door%s' % n, (4.54, s * 1.08, .83)); ik('Step_Door%s_Low' % n, (4.54, s * 1.10, .49))
# #endregion

# #region ---- cab doors, cab fittings
for s, n in ((1, 'DoorDriver'), (-1, 'DoorPassenger')):
    a, b = s * .5, s * 1.6; side = 'L' if s > 0 else 'R'
    patch(n + '_Trim', grow(DOORF, -.028), 'y', a, b, M_DASH, .043, .067, hole=grow(WD, .012))
    box(n + '_Armrest', (4.10, s * 1.130, 1.62), (.55, .055, .06), M_DASH, .02, 3); box(n + '_Pocket', (4.10, s * 1.145, 1.32), (.72, .040, .16), M_DASH, .015)
    box(n + '_HandleIn_Cup', (4.46, s * 1.152, 1.70), (.13, .018, .06), M_PLAST, .008); box(n + '_HandleIn', (4.45, s * 1.141, 1.705), (.085, .012, .022), M_CHROME, .004)
    box(n + '_Pull', (4.45, s * 1.138, 1.50), (.05, .05, .20), M_PLAST, .016, 3); box(n + '_Switches', (4.30, s * 1.130, 1.653), (.13, .045, .014), M_PLAST, .005)
    patch(n + '_HandleCup', rnd([(3.66, 1.485), (3.80, 1.485), (3.80, 1.555), (3.66, 1.555)], .025, 4), 'y', a, b, M_PLAST, -.002, .008)
    box(n + '_Handle', (3.735, s * 1.247, 1.522), (.15, .018, .030), M_PLAST, .008, 3); box(n + '_Handle_P0', (3.67, s * 1.234, 1.522), (.02, .028, .022), M_PLAST, .004); box(n + '_Handle_P1', (3.795, s * 1.234, 1.522), (.02, .028, .022), M_PLAST, .004)
    for i, z in enumerate((1.34, 1.66)): box(n + '_Hinge%d' % i, (4.690, s * 1.160, z), (.07, .035, .08), M_DARK, .004)
    box(n + '_MirrorFoot', (4.60, s * 1.205, 1.90 + .10), (.07, .06, .10), M_PLAST, .018, 3); box(n + '_MirrorArm', (4.615, s * 1.285, 2.03), (.05, .16, .06), M_PLAST, .018, 3); box(n + '_MirrorShell', (4.63, s * 1.40, 2.10), (.115, .20, .56), M_PLAST, .045, 4)
    box(n + '_MirrorGlass', (4.5725, s * 1.40, 2.19), (.004, .165, .30), M_MIR, .001, 1); box(n + '_MirrorGlass2', (4.5725, s * 1.40, 1.93), (.004, .165, .13), M_MIR, .001, 1)
    piv(n, (4.775, s * 1.215, 1.5)); ik('Handle_%s_Out' % n, (3.735, s * 1.256, 1.522)); ik('Handle_%s_In' % n, (4.45, s * 1.13, 1.705)); ik('Stand_%s' % n, (4.20, s * 1.90, 0))
    tube('GrabHandle_' + side, [(3.545, s * 1.245, 1.30), (3.545, s * 1.275, 1.36), (3.545, s * 1.275, 1.94), (3.545, s * 1.235, 2.0)], .014, M_PLAST); ik('Grab_' + side, (3.545, s * 1.275, 1.70))
    for e, z in enumerate((1.30, 2.0)): box('GrabHandle_%s_F%d' % (side, e), (3.545, s * 1.222, z), (.045, .05, .06), M_PLAST, .012)
    tube('GrabIn_' + side, [(4.60, s * 1.10, 2.02), (4.57, s * 1.06, 2.05), (4.50, s * 1.06, 2.28), (4.49, s * 1.09, 2.32)], .012, M_PLAST); box('GrabIn_%s_F0' % side, (4.63, s * 1.10, 2.00), (.09, .05, .07), M_PLAST, .012); box('GrabIn_%s_F1' % side, (4.54, s * 1.11, 2.335), (.14, .05, .06), M_PLAST, .012); ik('GrabIn_' + side, (4.53, s * 1.06, 2.16))
Xo = lambda x: x + ox
Zo = lambda z: z + oz
DY = .62                                                          # the driver's middle
da = rawprism('Dashboard', [(Xo(x), Zo(z)) for x, z in rnd([(1.745, .86), (1.38, .90), (1.28, 1.00), (1.26, 1.10), (1.30, 1.145), (1.46, 1.165), (1.745, 1.165)], [.02, .06, .05, .05, .03, .04, .02], 4)], 'y', -1.3, 1.3); boole(da, S.LF(.0445), 'INTERSECT')
boole(da, rawprism('c', [(4.2, 1), (5.2, 1), (5.2, 2.2), (4.2, 2.2)], 'y', -1.10, 1.10), 'INTERSECT', False); K.finish(da, M_DASH, 32)
box('Dash_Binnacle', (Xo(1.36), DY, Zo(1.185)), (.24, .42, .13), M_DASH, .04, 4); box('Dash_Cluster', (Xo(1.2395), DY, Zo(1.185)), (.010, .33, .09), M_SCR, .004); ik('Cluster', (Xo(1.23), DY, Zo(1.185)))
box('Dash_Centre', (Xo(1.265), .10, Zo(1.02)), (.06, .40, .20), M_PLAST, .015, 3); box('Dash_Screen', (Xo(1.2325), .10, Zo(1.055)), (.010, .20, .10), M_SCR, .003); text('Dash_Word', 'FUELLING CONTROL', (Xo(1.2275), .10, Zo(1.085)), .018, 'x-', M_SWHT, .0008)
for i in range(6): box('Dash_Btn%d' % i, (Xo(1.2335), -.025 + .05 * i, Zo(.955)), (.010, .036, .022), M_RED if i == 3 else M_DARK, .003)
for i, y in enumerate((-.95, -.25, .34, .95)):
    box('Dash_Vent%d' % i, (Xo(1.285), y, Zo(1.06)), (.07, .12, .09), M_PLAST, .012)
    for k in range(4): box('Dash_Vent%d_S%d' % (i, k), (Xo(1.2485), y, Zo(1.030 + .02 * k)), (.010, .10, .007), M_DARK, .001, 1)
box('Dash_Glove', (Xo(1.262), -.62, Zo(1.0)), (.03, .42, .15), M_DASH, .012); box('Dash_GloveCatch', (Xo(1.2455), -.62, Zo(1.06)), (.010, .10, .022), M_PLAST, .004)
box('GearSelect_Base', (4.15, .22, FLOOR + .275), (.16, .11, .03), M_PLAST, .008); cyl('GearSelect', (4.15, .22, FLOOR + .315), .026, .06, 'z', M_CHROME, 32, .006); ik('GearSelect', (4.15, .22, FLOOR + .345)); box('ParkBrake', (3.98, .22, FLOOR + .30), (.05, .03, .07), M_RED, .008); ik('ParkBrake', (3.98, .22, FLOOR + .335))
SC = Vector((Xo(1.085), DY, Zo(1.175))); AXW = Vector((-.7071, 0, .7071))
def tilt(o): o.data.transform(Matrix.Translation(SC) @ AXW.to_track_quat('Z', 'Y').to_matrix().to_4x4()); o.data.update(); return o
tilt(torus('SteeringWheel_Rim', (0, 0, 0), .215, .0165, 'z', M_PLAST, 64, 16)); tilt(cyl('SteeringWheel_Boss', (0, 0, -.012), .078, .045, 'z', M_PLAST, 40, .012)); tilt(cyl('SteeringWheel_Cap', (0, 0, .012), .040, .012, 'z', M_DASH, 32, .004))
for i, a in enumerate((90, 210, 330)):
    q = Vector((math.cos(math.radians(a)), math.sin(math.radians(a)), 0)); tilt(tube('SteeringWheel_Spoke%d' % i, [tuple(q * .05 + Vector((0, 0, -.015))), tuple(q * .13 + Vector((0, 0, -.008))), tuple(q * .213)], .014, M_PLAST))
tube('SteerColumn', [tuple(SC - AXW * .035), tuple(SC - AXW * .34)], .032, M_PLAST, bez=False); tilt(box('SteerShroud', (0, 0, -.10), (.12, .13, .11), M_PLAST, .025, 3))
for s, n in ((1, 'Ind'), (-1, 'Wip')): tilt(tube('Stalk' + n, [(0, s * .05, -.075), (0, s * .13, -.055), (0, s * .165, -.05)], .008, M_PLAST, bez=False))
piv('SteeringWheel', tuple(SC))
for s, n in ((1, 'L'), (-1, 'R')): ik('Hand_Wheel_' + n, tuple(SC + Vector((0, s * .215, 0))))
for n, y, w, zz in (('Accel', DY - .12, .055, .70), ('Brake', DY + .01, .085, .71)):
    c = (Xo(1.475), y, Zo(zz)); turn(box('Pedal_%s_Pad' % n, c, (.018, w, .10), M_SEAL, .004), c, 'y', -25); tube('Pedal_%s_Arm' % n, [(c[0] + .003, y, c[2]), (Xo(1.53), y, Zo(zz + .10)), (Xo(1.60), y, Zo(.93))], .010, M_DARK); ik('Foot_' + n, (Xo(1.462), y, Zo(zz))); piv('Pedal_' + n, (Xo(1.60), y, Zo(.93)))
turn(box('FootRest', (Xo(1.50), DY + .155, Zo(.66)), (.02, .075, .20), M_SEAL, .005), (Xo(1.50), DY + .155, Zo(.66)), 'y', -35); ik('Foot_Rest', (Xo(1.485), DY + .155, Zo(.66))); box('FootRest_B', (Xo(1.56), DY + .155, Zo(.61)), (.11, .05, .08), M_PLAST, .01)
def seat(n, x, y, w, depth=.50, back=.66, zc=FLOOR + .365):
    cu = box(n + '_Cushion', (x, y, zc), (depth, w, .15), M_SEAT, .05, 4, cuts=4)
    for v in cu.data.vertices:
        if v.co.z > zc: v.co.z += .022 * abs((v.co.y - y) / (w / 2)) ** 3 - .012 * K.ss(0, 1, 1 - abs((v.co.x - x) / (depth / 2)))
    for s in (-1, 1): box(n + '_Bolster%d' % (s > 0), (x, y + s * (w / 2 - .035), zc + .055), (depth - .08, .06, .06), M_SEAT2, .025, 3)
    bx = x - depth / 2 - .03; parts = [box(n + '_Back', (bx, y, zc + .05 + back / 2), (.13, w, back), M_SEAT, .05, 4, cuts=4)]
    for s in (-1, 1): parts.append(box(n + '_BackBolster%d' % (s > 0), (bx + .055, y + s * (w / 2 - .04), zc + .05 + back * .48), (.07, .07, back * .70), M_SEAT2, .028, 3))
    parts.append(box(n + '_BackShell', (bx - .055, y, zc + .04 + back / 2), (.05, w - .04, back - .04), M_PLAST, .02, 3))
    for s in (-1, 1): parts.append(cyl(n + '_Post%d' % (s > 0), (bx, y + s * .055, zc + .05 + back + .035), .007, .14, 'z', M_STEEL, 14))
    parts.append(box(n + '_Head', (bx + .005, y, zc + .05 + back + .145), (.11, .27, .18), M_SEAT, .045, 4))
    for o in parts: turn(o, (bx, y, zc), 'y', -12)
    box(n + '_Pan', (x, y, zc - .085), (depth - .04, w - .05, .05), M_PLAST, .015); box(n + '_Box', (x - .02, y, (zc - .10 + FLOOR) / 2), (depth - .10, w - .10, zc - .10 - FLOOR + .012), M_PLAST, .02)
    b = -1 if y > 0 else 1; yb = y + b * (w / 2 + .012); xb = x - depth / 2 + .04
    box(n + '_BuckleStem', (xb, yb, zc + .03), (.03, .014, .15), M_PLAST, .004); box(n + '_Buckle', (xb, yb, zc + .13), (.045, .024, .06), M_PLAST, .006); box(n + '_BuckleRed', (xb, yb, zc + .161), (.03, .014, .008), M_RED, .002); ik('Seat_' + n, (x - depth / 2 + .14, y, zc + .08)); ik('Buckle_' + n, (xb, yb, zc + .17))
seat('SeatDriver', Xo(.74), DY, .50); seat('SeatPassenger', Xo(.74), -DY, .50)
for s, n in ((1, 'D'), (-1, 'P')):
    box('Belt%s_Top' % n, (3.46, s * 1.145, 2.30), (.05, .05, .07), M_PLAST, .01); box('Belt%s_Web' % n, (3.46, s * 1.132, 1.83), (.046, .006, .90), M_BELT, .001, 1); box('Belt%s_Reel' % n, (3.46, s * 1.135, 1.33), (.10, .09, .16), M_PLAST, .015); box('Belt%s_Tongue' % n, (3.46, s * 1.126, 1.80), (.05, .012, .07), M_STEEL, .004); ik('BeltTongue_' + n, (3.46, s * 1.12, 1.80))
    patch('BPillar_' + n, [(3.40, 1.24), (3.52, 1.24), (3.52, 2.60), (3.40, 2.60)], 'y', s * .6, s * 1.4, M_TRIM, .042, .056)
    box('SunVisor_' + n, (4.43, s * .55, 2.725), (.19, .62, .022), M_LIN, .008, 3); cyl('SunVisor_%s_Rod' % n, (4.525, s * .55, 2.728), .008, .62, 'y', M_PLAST, 16)
    for e in (.28, .82): box('SunVisor_%s_F%d' % (n, e > .5), (4.53, s * e, 2.752), (.045, .03, .07), M_PLAST, .006)
box('Locker_Back', (3.205, 0, 1.60), (.20, .56, .80), M_TRIM, .03, 3); box('Clipboard', (3.312, 0, 1.75), (.012, .22, .30), M_DARK, .003); box('Clipboard_Paper', (3.3195, 0, 1.74), (.005, .20, .26), M_SWHT, .001, 1); ik('Clipboard', (3.32, 0, 1.75))
for s, n, py in ((1, 'L', .98), (-1, 'R', -.05)):
    pv = S.surf(py, 1.785, .006); piv('Wiper_' + n, tuple(pv)); cyl('Wiper%s_Hub' % n, tuple(pv), .024, .07, 'z', M_PLAST, 20, .004)
    tube('Wiper%s_Arm' % n, [tuple(S.surf(py, 1.80, .022)), tuple(S.surf(py - .22, 1.825, .030)), tuple(S.surf(py - .48, 1.852, .016))], .009, M_PLAST)
    tube('Wiper%s_Blade' % n, [tuple(S.surf(py - .05 - .105 * k, 1.832 + .004 * k, .003)) for k in range(9)], .011, M_SEAL); ik('WiperTip_' + n, tuple(S.surf(py - .89, 1.864, .003)))
cyl('Beacon_Base', (3.75, 0, 2.932), .085, .05, 'z', M_PLAST, 40, .006); CK.lathez('BeaconLens', [(0, 2.945), (.074, 2.945), (.078, 2.96), (.076, 3.025), (.066, 3.055), (.04, 3.072), (0, 3.075)], (3.75, 0, 0), M_AMB, 48); cyl('Beacon_Cap', (3.75, 0, 3.074), .03, .012, 'z', M_PLAST, 24, .003); ik('Beacon', (3.75, 0, 3.0))
tube('Aerial', [(3.5, -.6, 2.86), (3.42, -.6, 3.02), (3.34, -.6, 3.18)], .004, M_PLAST); cyl('Aerial_Base', (3.5, -.6, 2.89), .02, .04, 'z', M_PLAST, 20)
# #endregion

# #region ---- wheels, axles, mudguards, chassis
kr, kw = RW / .372, TW / .233
TP = [(r * kr, y * kw) for r, y in ((.200, -.098), (.232, -.112), (.275, -.1165), (.318, -.112), (.348, -.100), (.362, -.086), (.3695, -.066), (.372, -.030), (.372, .030), (.3695, .066), (.362, .086), (.348, .100), (.318, .112), (.275, .1165), (.232, .112), (.200, .098), (.196, 0))]
RP = [(r * kr, y * kw) for r, y in ((.214, -.104), (.208, -.098), (.196, -.092), (.186, -.050), (.182, 0), (.186, .050), (.196, .092), (.208, .098), (.214, .104), (.204, .106), (.190, .098), (.176, .050), (.174, 0), (.176, -.050), (.190, -.098), (.204, -.106))]
def wheel(key, x, y, s, dish, part=''):
    """dish +: the disc bulges out (front, tag); -: it lies deep (the outer drive wheel)."""
    c = (x, y, RW); lathe('Tyre_%s%s' % (key, part), TP, c, M_TYRE, 80, 40); lathe('Rim_%s%s' % (key, part), RP, c, M_RIM, 80, 40); d0 = len(ALL)
    d = s * dish; wd = lathe('WheelDisc_%s%s' % (key, part), [(0, d * .55), (.10, d * .55), (.125, d * .9), (.165, d), (.200, d * .8), (.225, d * .35), (.2445, d * .2), (.2445, d * .2 - s * .016), (.218, d * .35 - s * .016), (.195, d * .8 - s * .016), (.165, d - s * .016), (.130, d * .9 - s * .016), (.105, d * .55 - s * .016), (0, d * .55 - s * .016)], c, M_RIM, 80, 35)
    for j in range(10):
        a = 2 * math.pi * j / 10 + .15; cut(wd, cyl('c', (x + .212 * math.cos(a), y + d * .5, RW + .212 * math.sin(a)), .022, .14, 'y', M_RIM, 20))
    if not part:
        cyl('Hub_' + key, (x, y + d * .55 - s * .03, RW), .09, .10, 'y', M_DARK, 36, .004); cyl('HubCap_' + key, (x, y + d * .55 + s * .025, RW), .062, .06, 'y', M_STEEL, 36, .012)
        for j in range(10):
            a = 2 * math.pi * j / 10; cyl('Nut_%s_%d' % (key, j), (x + .168 * math.cos(a), y + d + s * .008, RW + .168 * math.sin(a)), .015, .034, 'y', M_STEEL, 6)
def fender(n, x, y0, y1, r, a0, a1, m=M_BUMP, t=.012):
    k = 30; bm = bmesh.new(); R = [[[bm.verts.new((x + q * math.cos(math.radians(a0 + (a1 - a0) * i / k)), y, RW + q * math.sin(math.radians(a0 + (a1 - a0) * i / k)))) for i in range(k + 1)] for y in (y0, y1)] for q in (r, r + t)]
    for i in range(k):
        for q in (0, 1): bm.faces.new((R[q][0][i], R[q][0][i + 1], R[q][1][i + 1], R[q][1][i]))
        for e in (0, 1): bm.faces.new((R[0][e][i], R[0][e][i + 1], R[1][e][i + 1], R[1][e][i]))
    for i in (0, k): bm.faces.new((R[0][0][i], R[0][1][i], R[1][1][i], R[1][0][i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); return K.finish(K.link(n, bm), m, 40)
FYW, DYO, DYI = 1.025, 1.045, .715
for s, sn in ((1, 'L'), (-1, 'R')):
    wheel('F' + sn, FX, s * FYW, s, .085); wheel('T' + sn, TX, s * FYW, s, .085); wheel('R' + sn, DX, s * DYO, s, -.085); wheel('R' + sn, DX, s * DYI, s, .085, '_In')
    piv('Wheel_F' + sn, (FX, s * FYW, RW)); piv('Wheel_R' + sn, (DX, s * (DYO + DYI) / 2, RW)); piv('Wheel_T' + sn, (TX, s * FYW, RW)); piv('Steer_F' + sn, (FX, s * .80, RW)); piv('Steer_T' + sn, (TX, s * .80, RW))
    for k, x in (('F', FX), ('T', TX)):
        box('Spindle_%s%s' % (k, sn), (x, s * .80, RW), (.13, .08, .24), M_DARK, .014); cyl('Spindle_%s%s_Axle' % (k, sn), (x, s * .90, RW), .05, .20, 'y', M_STEEL, 28); cyl('SpindleDrum_%s%s' % (k, sn), (x, s * .965, RW), .205, .15, 'y', M_DARK, 48, .01)
        box('SteerArm_%s%s' % (k, sn), (x - .10, s * .775, RW - .05), (.18, .04, .03), M_DARK, .008)
        cyl('AxleBeam%s_%s' % (k, sn), (x, s * .40, RW - .06), .055, .76, 'y', M_DARK, 28); box('AxleEnd%s_%s' % (k, sn), (x, s * .745, RW), (.12, .05, .18), M_DARK, .012)
        box('Leaf%s_%s' % (k, sn), (x, s * .52, RW + .055), (1.10, .08, .06), M_DARK, .012)
        for e in (-.5, .5): box('Hanger%s_%s%d' % (k, sn, e > 0), (x + e * 1.04, s * .49, .69), (.07, .10, .30), M_CHAS, .006)
    cyl('DriveAxle_' + sn, (DX, s * .62, RW), .075, .60, 'y', M_DARK, 32); cyl('DriveDrum_' + sn, (DX, s * .60, RW), .20, .10, 'y', M_DARK, 40, .01)
    for e in (-.42, .42): cyl('AirSpring_%s%d' % (sn, e > 0), (DX + e, s * .49, .655), .10, .30, 'z', M_SEAL, 32, .03); box('AirBeam_%s%d' % (sn, e > 0), (DX + e / 2, s * .49, .455), (.52, .09, .07), M_DARK, .01)
    prism('FenderF_' + sn, [(FX - .68, .74), (FX - .57, 1.01), (FX - .46, 1.095), (FX + .50, 1.095), (FX + .50, 1.107), (FX - .465, 1.107), (FX - .58, 1.02), (FX - .692, .745)], s * .84, s * 1.205, M_BUMP); fender('FenderD_' + sn, DX, s * .55, s * 1.215, .56, 8, 172); fender('FenderT_' + sn, TX, s * .84, s * 1.205, .56, 8, 172)
    for k, x in (('D', DX), ('T', TX)): tube('FenderBar%s_%s' % (k, sn), [(x, s * .40, .95), (x, s * .60, 1.06), (x, s * 1.12, 1.062)], .018, M_DARK, bez=False)
    tube('FenderBarF_' + sn, [(FX - .30, s * .40, 1.02), (FX - .30, s * .50, 1.114), (FX - .30, s * 1.15, 1.114)], .018, M_DARK, bez=False)
    for k, (x, y) in enumerate(((FX - .690, 1.02), (TX - .60, 1.02))): box('MudFlap%d_%s' % (k, sn), (x, s * y, .42), (.012, .34, .42), M_SEAL, .003, 1)
    box('MudFlapBar0_' + sn, (FX - .690, s * .735, .62), (.04, .64, .03), M_DARK, .004); box('MudFlapBar1_' + sn, (TX - .60, s * .735, .62), (.04, .64, .03), M_DARK, .004)
    box('MudFlapPost0_' + sn, (FX - .690, s * .44, .74), (.04, .05, .26), M_DARK, .004); box('MudFlapPost1_' + sn, (TX - .60, s * .44, .74), (.04, .05, .26), M_DARK, .004)
    box('Frame_' + sn, (-.10, s * .43, .91), (9.70, .08, .24), M_CHAS, .010, 1)
cyl('DriveAxle_Mid', (DX, 0, RW), .075, .80, 'y', M_DARK, 32); cyl('Differential', (DX, 0, RW), .21, .30, 'x', M_DARK, 40, .06); cyl('PropShaft', (.90, 0, .66), .05, 4.0, 'x', M_DARK, 28); box('Gearbox', (3.00, 0, .74), (.85, .50, .46), M_DARK, .06, 3); box('Engine', (3.95, 0, .86), (1.15, .74, .50), M_DARK, .06, 3)
for i, x in enumerate((4.60, 2.05, .30, -1.95, -3.35, -4.85)): box('FrameCross%d' % i, (x, 0, .91), (.10, .80, .20), M_CHAS, .008, 1)
for i, x in enumerate((1.55, -.10, -1.75, -3.05)):
    for s, sn in ((1, 'L'), (-1, 'R')): box('TankSaddle%d_%s' % (i, sn), (x, s * .43, 1.10), (.16, .18, .20), M_CHAS, .01); box('TankBolt%d_%s' % (i, sn), (x, s * .505, .95), (.10, .03, .10), M_STEEL, .004)
box('PumpHousing', (.95, 0, .88), (.50, .42, .36), M_FRM, .05, 3); cyl('PumpMotor', (1.45, 0, .88), .15, .55, 'x', M_DARK, 40, .02)
cyl('Exhaust', (4.55, .0, .50), .07, .9, 'y', M_DARK, 32, .02); cyl('ExhaustSparkTrap', (4.55, -.60, .50), .055, .32, 'y', M_STEEL, 32, .01); box('ExhaustHang', (4.55, 0, .66), (.06, .06, .34), M_DARK, .006)
# #endregion

# #region ---- tank
TA, TB, TZ, TN, TX0, TX1 = 1.215, .90, 2.04, 2.8, -3.40, 2.00
def ring(x, k, n=96):
    out = []
    for i in range(n):
        a = 2 * math.pi * i / n; c, si = math.cos(a), math.sin(a); out.append((x, TA * k * math.copysign(abs(c) ** (2 / TN), c), TZ + TB * k * math.copysign(abs(si) ** (2 / TN), si)))
    return out
def skinrings(name, rs, m, smooth=40, capends=True):
    bm = bmesh.new(); R = [[bm.verts.new(p) for p in r] for r in rs]; n = len(R[0])
    for A, B in zip(R, R[1:]):
        for i in range(n): bm.faces.new((A[i], A[(i + 1) % n], B[(i + 1) % n], B[i]))
    if capends: bm.faces.new(R[0][::-1]); bm.faces.new(R[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); return K.finish(K.link(name, bm), m, smooth)
DOME = [(.0, .35), (.05, .62), (.10, .80), (.15, .915), (.20, .975), (.25, 1.0)]
skinrings('Tank', [ring(TX0 + d, k) for d, k in DOME] + [ring(TX0 + .25 + (TX1 - TX0 - .5) * i / 16, 1.0) for i in range(1, 16)] + [ring(TX1 - d, k) for d, k in DOME[::-1]], M_BODY, 40)
def ty(z, k=1.0): return TA * k * max(1 - abs((z - TZ) / (TB * k)) ** TN, 0) ** (1 / TN)
def tz(y): return TZ + TB * max(1 - abs(y / TA) ** TN, 0) ** (1 / TN)
for i, x in enumerate((1.72, .10, -1.55, -3.12)): skinrings('TankBand%d' % i, [ring(x - .03, 1.0005), ring(x - .022, 1.006), ring(x + .022, 1.006), ring(x + .03, 1.0005)], M_BODY, 30, False)
for s, sn in ((1, 'L'), (-1, 'R')):                                  # the livery band: a thin shell along the tank, between two heights
    zs = [1.70 + .56 * j / 8 for j in range(9)]
    for seg, (xa, xb) in enumerate(((.14, 1.68), (-1.51, .06), (-3.08, -1.59))):
        bm = bmesh.new(); lo = [[bm.verts.new((x, s * (ty(z) + d), z)) for z in zs] for d in (-.004, .004) for x in (xa, xb)]
        A0, A1, B0, B1 = lo
        for j in range(8):
            bm.faces.new((A0[j], A0[j + 1], A1[j + 1], A1[j])); bm.faces.new((B0[j], B0[j + 1], B1[j + 1], B1[j])); bm.faces.new((A0[j], A0[j + 1], B0[j + 1], B0[j])); bm.faces.new((A1[j], A1[j + 1], B1[j + 1], B1[j]))
        for j in (0, 8): bm.faces.new((A0[j], A1[j], B1[j], B0[j]))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); K.finish(K.link('TankStripe_%s%d' % (sn, seg), bm), M_MAIN, 40)
    box('Coaming_' + sn, (-.70, s * .58, 2.955), (4.90, .014, .17), M_BODY, .003, 1)
    for i in range(6): box('CoamFoot%d_%s' % (i, sn), (1.5 - .88 * i, s * .565, 2.905), (.06, .05, .06), M_BODY, .004)
    tube('TopRail_' + sn, [(1.60, s * .50, 2.90), (1.55, s * .50, 3.06), (-2.95, s * .50, 3.06), (-3.0, s * .50, 2.90)], .018, M_STEEL, bez=False)
    for i in range(4): cyl('TopRailPost%d_%s' % (i, sn), (1.0 - 1.1 * i, s * .50, 2.985), .014, .16, 'z', M_STEEL, 16)
for i, x in enumerate((1.72, -3.12)): box('CoamEnd%d' % i, (x + (.03 if i else -.0), 0, 2.965), (.014, 1.17, .15), M_BODY, .003, 1)
box('TopWalk', (-.70, 0, 2.958), (4.40, .42, .022), M_DECK, .003, 1)
for i, x in enumerate((.95, -1.55)):
    cyl('Manhole%d' % i, (x, 0, 2.972), .27, .10, 'z', M_ALU, 56, .012); cyl('Manhole%d_Lid' % i, (x, 0, 3.035), .235, .04, 'z', M_ALU, 56, .012)
    for j in range(6): cyl('Manhole%d_Clamp%d' % (i, j), (x + .245 * math.cos(j * math.pi / 3), .245 * math.sin(j * math.pi / 3), 3.035), .018, .05, 'z', M_STEEL, 16)
for i, x in enumerate((.25, -2.35)): cyl('Vent%d' % i, (x, .30, 2.985), .045, .14, 'z', M_ALU, 28); cyl('Vent%d_Cap' % i, (x, .30, 3.06), .075, .035, 'z', M_ALU, 36, .012)
# the product line along the left side, the return line on the right
tube('Pipe_L', [(2.20, .95, 1.16), (1.90, .95, 1.135), (-3.45, .95, 1.135), (-3.60, .95, 1.10), (-3.66, .90, .98), (-3.72, .86, .93)], .042, M_BODY, bez=False)
for i, x in enumerate((1.3, -.2, -1.9, -3.2)): box('PipeClamp%d' % i, (x, .80, 1.135), (.05, .36, .03), M_DARK, .004); torus('PipeFlange%d' % i, (x + .12, .95, 1.135), .048, .014, 'x', M_STEEL, 28, 10)
for i, x in enumerate((1.3, -.2, -1.9, -3.2)): box('PipeArm%d' % i, (x, .55, 1.085), (.05, .20, .10), M_DARK, .004)
tube('Pipe_R', [(2.20, -.95, 1.16), (1.90, -.95, 1.12), (-3.45, -.95, 1.12), (-3.60, -.95, 1.08), (-3.66, -.90, .98), (-3.72, -.86, .93)], .030, M_BODY, bez=False)
for i, x in enumerate((1.3, -.2, -1.9, -3.2)): box('PipeClampR%d' % i, (x, -.72, 1.12), (.05, .52, .03), M_DARK, .004)
# #endregion

# #region ---- the meter cabinet: pump panel (left), locker (right)
CX0, CX1, CZ0, CZ1, CY = 2.12, 2.97, .80, 2.82, 1.205
cab = box('Cabinet', ((CX0 + CX1) / 2, 0, (CZ0 + CZ1) / 2), (CX1 - CX0, CY * 2, CZ1 - CZ0), M_BODY, .035, 3)
for s in (-1, 1): cut(cab, box('c', ((CX0 + CX1) / 2, s * .92, 1.79), (CX1 - CX0 - .07, .92, 1.74), M_FRM, .012))
ocx = (CX0 + CX1) / 2
for s, sn in ((1, 'L'), (-1, 'R')):
    n = 'PanelDoor' + sn; box(n, (ocx, s * (CY - .008), 1.79), (CX1 - CX0 - .085, .030, 1.725), M_BODY, .008, 2); box(n + '_Frame', (ocx, s * (CY - .030), 1.79), (CX1 - CX0 - .15, .020, 1.62), M_TRIM, .004, 1)
    for i, z in enumerate((1.08, 1.79, 2.50)): cyl(n + '_Hinge%d' % i, (CX0 + .035, s * (CY + .008), z), .013, .10, 'z', M_STEEL, 20)
    box(n + '_HandleCup', (CX1 - .12, s * (CY + .0075), 1.62), (.10, .010, .16), M_PLAST, .004, 1); box(n + '_Handle', (CX1 - .12, s * (CY + .020), 1.62), (.030, .024, .13), M_CHROME, .008, 2)
    piv(n, (CX0 + .035, s * (CY + .008), 1.79)); ik('Handle_%s_Out' % n, (CX1 - .12, s * (CY + .032), 1.62)); ik('Stand_' + n, (ocx + .25, s * 1.85, 0)); ik('Stand_%s_Open' % n, (CX1 + .10, s * 1.80, 0))
    for i in range(3): box('CabVent%d_%s' % (i, sn), (CX1 - .20 - .26 * i, s * (CY - .001), 2.742), (.18, .010, .04), M_PLAST, .003, 1)
    box('CabMount_' + sn, (ocx, s * .43, .80), (.70, .16, .05), M_CHAS, .008)
plate('PanelDoorL_SignPlate', (ocx - .04, CY + .007, 2.28), .56, .16, 'y+', M_SBLK, .005); text('PanelDoorL_Jet', 'JET A-1', (ocx - .04, CY + .012, 2.28), .125, 'y+', M_SWHT)
plate('PanelDoorR_SignPlate', (ocx - .04, -CY - .007, 2.28), .56, .16, 'y-', M_SBLK, .005); text('PanelDoorR_Jet', 'JET A-1', (ocx - .04, -CY - .012, 2.28), .125, 'y-', M_SWHT)
def nosmoke(n, c, face, r=.10):
    cp = off(c, face, 0); d = cyl(n + '_Disc', cp, r, .012, 'y' if face[0] == 'y' else 'x', M_SWHT, 56, .001); f = off(cp, face, .006)
    facepoly(n + '_Cig', [(-r * .55, -.012), (r * .5, -.012), (r * .5, .012), (-r * .55, .012)], f, face, M_SBLK, .0012); facepoly(n + '_Ash', [(r * .53, -.012), (r * .64, -.012), (r * .64, .012), (r * .53, .012)], f, face, M_SRED, .0012)
    torus(n + '_Ring', f, r * .88, r * .085, 'y' if face[0] == 'y' else 'x', M_SRED, 56, 10); a = .7071; w = .009
    facepoly(n + '_Bar', [(-r * .6 - w * a, r * .6 - w * a), (-r * .6 + w * a, r * .6 + w * a), (r * .6 + w * a, -r * .6 + w * a), (r * .6 - w * a, -r * .6 - w * a)], f, face, M_SRED, .0026, .0022)
def flame(n, c, face, r=.16):
    rr = r; facepoly(n + '_Edge', [(0, -rr), (rr, 0), (0, rr), (-rr, 0)], c, face, M_SWHT, .003); q = rr * .93; f = off(c, face, .003); facepoly(n + '_Red', [(0, -q), (q, 0), (0, q), (-q, 0)], f, face, M_SRED, .0022); g = off(f, face, .0022)
    facepoly(n + '_Flame', [(x * rr, z * rr) for x, z in ((-.16, -.10), (.16, -.10), (.23, .04), (.20, .18), (.13, .30), (.12, .16), (.05, .24), (.03, .42), (-.08, .28), (-.10, .14), (-.17, .24), (-.22, .10))], g, face, M_SBLK, .0012); facepoly(n + '_Line', [(-.20 * rr, -.17 * rr), (.20 * rr, -.17 * rr), (.20 * rr, -.135 * rr), (-.20 * rr, -.135 * rr)], g, face, M_SBLK, .0012); text(n + '_3', '3', off(c[:2] + (c[2] - rr * .52,), face, .0052), rr * .48, face, M_SBLK, .001)
for s, sn, f in ((1, 'L', 'y+'), (-1, 'R', 'y-')):
    nosmoke('PanelDoor%s_NoSmoke' % sn, (ocx - .18, s * (CY + .010), 1.98), f); flame('PanelDoor%s_Hazard' % sn, (ocx + .12, s * (CY + .007), 1.96), f)
    text('PanelDoor%s_Words' % sn, 'NO SMOKING   NO NAKED LIGHTS', (ocx - .04, s * (CY + .007), 1.36), .038, f, M_SRED, .0012); text('PanelDoor%s_Words2' % sn, 'SWITCH OFF MOBILE PHONES', (ocx - .04, s * (CY + .007), 1.28), .038, f, M_SBLK, .0012)
# left: the panel
PY = .640
box('Panel_Plate', (ocx, PY, 1.86), (.74, .02, 1.06), M_FRM, .004, 1); box('Panel_Feet', (ocx, .54, 1.40), (.70, .20, .04), M_DARK, .004); box('Panel_Back', (ocx, .50, 2.10), (.60, .30, .04), M_DARK, .004)
for i, (x, z, lab, r) in enumerate(((2.78, 2.23, 'PUMP bar', .075), (2.56, 2.23, 'NOZZLE bar', .075), (2.34, 2.23, 'FILTER dP', .075))):
    cyl('Gauge%d_Case' % i, (x, PY + .030, z), r, .045, 'y', M_STEEL, 48, .006); cyl('Gauge%d_Dial' % i, (x, PY + .0535, z), r * .86, .006, 'y', M_SWHT, 48)
    for j in range(11):
        a = math.radians(225 - 27 * j); rr = r * .72; b = box('Gauge%d_T%d' % (i, j), (x, PY + .0565, z + rr), (.003, .0040, .010 if j % 5 else .016), M_SBLK, 0); turn(b, (x, PY, z), 'y', 90 - math.degrees(a))
    facepoly('Gauge%d_Zone' % i, [(r * .60 * math.cos(math.radians(a)), r * .60 * math.sin(math.radians(a))) for a in (10, 0, -15, -30, -45)] + [(r * .50 * math.cos(math.radians(a)), r * .50 * math.sin(math.radians(a))) for a in (-45, -30, -15, 0, 10)], (x, PY + .0565, z), 'y+', M_SRED, .0008)
    facepoly('GaugeNeedle%d' % i, [(-.003, -.012), (.003, -.012), (.0012, r * .66), (-.0012, r * .66)], (x, PY + .0612, z), 'y+', M_SRED, .0012); cyl('Gauge%d_Pin' % i, (x, PY + .0595, z), .006, .010, 'y', M_SBLK, 16); piv('GaugeNeedle%d' % i, (x, PY + .060, z)); text('Gauge%d_Word' % i, lab, (x, PY + .010, z - .105), .026, 'y+', M_SWHT, .0008)
box('Meter_Body', (2.46, PY + .035, 1.96), (.34, .07, .19), M_DARK, .012, 3); box('Meter_Face', (2.46, PY + .0710, 1.975), (.28, .006, .085), M_LCD, .001, 1); text('Meter_Word', 'LITRES', (2.46, PY + .070, 1.895), .026, 'y+', M_SWHT, .0008); ik('Meter_Face', (2.46, PY + .075, 1.975))
box('Printer', (2.76, PY + .035, 1.97), (.16, .07, .17), M_DARK, .012, 3); box('Printer_Slot', (2.76, PY + .0710, 2.005), (.11, .006, .012), M_SBLK, .001, 1); box('Printer_Ticket', (2.76, PY + .0735, 1.975), (.095, .003, .07), M_SWHT, .0005, 1); ik('Ticket', (2.76, PY + .085, 1.955))
def knob(n, x, z, m, lab, r=.020):
    cyl('Button_%s_Base' % n, (x, PY + .014, z), r * 1.45, .014, 'y', M_DARK, 32, .003); cyl('Button_' + n, (x, PY + .028, z), r, .024, 'y', m, 32, .006); text('Button_%s_Word' % n, lab, (x, PY + .010, z - r * 1.45 - .020), .018, 'y+', M_SWHT, .0008); ik('Button_' + n, (x, PY + .042, z))
knob('Start', 2.30, 1.76, M_GRN, 'START'); knob('Stop', 2.40, 1.76, M_RED, 'STOP'); knob('LampOn', 2.50, 1.76, M_WHT, 'FLOW'); knob('Preset', 2.60, 1.76, M_BLU, 'PRESET'); knob('EStop', 2.80, 1.77, M_RED, 'EMERGENCY STOP', .032)
for i, (n, x, lab) in enumerate((('Pump', 2.28, 'PUMP'), ('HoseL', 2.45, 'HOSE 1'), ('HoseR', 2.62, 'HOSE 2'), ('Defuel', 2.79, 'DEFUEL'))):
    box('LeverGuide_' + n, (x, PY + .014, 1.555), (.05, .014, .16), M_DARK, .004); b = [cyl('Lever_%s_Boss' % n, (x, PY + .032, 1.50), .020, .03, 'y', M_STEEL, 28, .003), cyl('Lever_%s_Rod' % n, (x, PY + .050, 1.58), .008, .15, 'z', M_STEEL, 20), cyl('Lever_%s_Pivot' % n, (x, PY + .045, 1.505), .012, .02, 'y', M_STEEL, 20)]
    CK.lathez('Lever_%s_Ball' % n, [(0, -.022), (.014, -.018), (.022, -.004), (.020, .010), (.010, .021), (0, .023)], (x, PY + .050, 1.665), M_RED if n == 'Defuel' else M_SBLK, 24); piv('Lever_' + n, (x, PY + .045, 1.505)); ik('Lever_' + n, (x, PY + .050, 1.668)); text('Lever_%s' % n + 'Word', lab, (x, PY + .010, 1.425), .022, 'y+', M_SWHT, .0008)
# dead-man handle on its hook, coiled lead; the sampling jar; below: the meter and the filter vessel's foot
box('DeadmanHook', (2.86, .80, 1.335), (.05, .30, .02), M_STEEL, .004); cyl('Deadman_Grip', (2.86, .90, 1.265), .019, .13, 'z', M_SBLK, 28, .008); cyl('Deadman_Button', (2.86, .90, 1.335), .011, .025, 'z', M_RED, 20, .003); cyl('Deadman_Neck', (2.86, .90, 1.195), .010, .03, 'z', M_SBLK, 20)
tube('DeadmanLead', [(2.86, .90, 1.19)] + [(2.86 + .03 * math.cos(1.2 * k), .86 + .03 * math.sin(1.2 * k), 1.175 - .006 * k) for k in range(24)] + [(2.86, .70, 1.02), (2.86, .52, 1.02)], .004, M_SBLK, 3, True, 2); ik('Deadman', (2.86, .90, 1.265)); ik('DeadmanLead', (2.86, .70, 1.02)); box('DeadmanBox', (2.86, .52, 1.02), (.08, .12, .10), M_DARK, .008)
cyl('FlowMeter', (2.50, .80, 1.12), .13, .26, 'x', M_FRM, 48, .02); cyl('FlowMeter_End', (2.36, .80, 1.12), .155, .03, 'x', M_STEEL, 48, .004); cyl('FlowMeter_End2', (2.64, .80, 1.12), .155, .03, 'x', M_STEEL, 48, .004); tube('MeterPipe', [(2.22, .80, 1.12), (2.36, .80, 1.12)], .06, M_BODY, bez=False); tube('MeterPipe2', [(2.64, .80, 1.12), (2.76, .80, 1.12), (2.80, .72, 1.12), (2.80, .46, 1.12)], .05, M_BODY, bez=False)
box('MeterFoot', (2.50, .80, .965), (.20, .20, .10), M_DARK, .008); tube('MeterDown', [(2.22, .80, 1.13), (2.20, .90, 1.10), (2.18, .95, 1.14)], .042, M_BODY, bez=False)
cyl('SampleJar', (2.24, .80, 1.52), .045, .16, 'z', M_GLASS, 36, .004); cyl('SampleJar_Cap', (2.24, .80, 1.61), .048, .03, 'z', M_SBLK, 36, .004); cyl('SampleJar_Foot', (2.24, .80, 1.435), .052, .03, 'z', M_ALU, 36, .004); box('SampleShelf', (2.22, .74, 1.415), (.12, .24, .02), M_DARK, .004)
box('PanelLight', (ocx, .80, 2.650), (.60, .06, .04), M_PLAST, .006); box('WorkLamp_Lens', (ocx, .80, 2.627), (.56, .04, .010), M_WHT, .002)
# right: the locker: cones stacked, gloves box, absorbent, two shelves
for i, z in enumerate((1.35, 1.95)): box('LockerShelf%d' % i, (ocx, -.80, z), (.79, .70, .02), M_FRM, .003, 1)
for i in range(3): CK.lathez('LockerCone%d' % i, [(0, 0), (.15, 0), (.15, .02), (.105, .025), (.03, .34), (.02, .345), (0, .345)], (2.35, -.80, .925 + .055 * i), M_SORG, 36)
box('LockerTub', (2.72, -.80, 1.06), (.30, .44, .26), M_GRN, .03, 3); box('LockerTub_Lid', (2.72, -.80, 1.20), (.32, .46, .03), M_SBLK, .01); text('LockerTub_Word', 'SPILL KIT', (2.72, -1.0195, 1.08), .05, 'y-', M_SWHT, .001)
box('LockerBox0', (2.36, -.80, 1.49), (.36, .40, .26), M_BLU, .02, 3); box('LockerBox1', (2.74, -.82, 1.45), (.28, .36, .18), M_TRIM, .02, 3); box('LockerBox2', (2.50, -.82, 2.06), (.46, .40, .20), M_SORG, .02, 3); tube('LockerEarCord', [(2.86, -.70, 2.28), (2.80, -.70, 2.42), (2.70, -.70, 2.42), (2.64, -.70, 2.28)], .012, M_SBLK); box('LockerEar0', (2.86, -.70, 2.24), (.05, .07, .09), M_RED, .02, 3); box('LockerEar1', (2.64, -.70, 2.24), (.05, .07, .09), M_RED, .02, 3); box('LockerEarHook', (2.75, -.62, 2.425), (.03, .22, .03), M_STEEL, .004); box('LockerEarPost', (2.75, -.50, 2.425), (.10, .10, .10), M_DARK, .006)
# #endregion

# #region ---- side gear: lockers, fuel tank, extinguishers, bonding reel, chocks, shut-offs
cyl('DieselTank', (1.35, .86, .72), .26, 1.00, 'x', M_ALU, 56, .03); cyl('DieselCap', (1.60, 1.02, .955), .05, .05, 'z', M_SBLK, 28, .006)
for i, x in enumerate((1.02, 1.68)): torus('DieselStrap%d' % i, (x, .86, .72), .262, .010, 'x', M_STEEL, 48, 8); box('DieselArm%d' % i, (x, .54, .86), (.06, .20, .14), M_CHAS, .006)
box('Battery', (.36, .93, .74), (.62, .50, .42), M_BUMP, .03, 3); box('Battery_Lid', (.36, .93, .955), (.63, .51, .03), M_BUMP, .008); box('BatteryArm', (.36, .58, .88), (.40, .26, .08), M_CHAS, .006)
for i, x in enumerate((.50, 1.45)): box('SideLocker%d' % i, (x, -.92, .74), (.90, .54, .46), M_ALU, .02, 3); box('SideLocker%d_Lid' % i, (x, -1.188, .74), (.80, .012, .38), M_ALU, .004, 1); box('SideLocker%d_Catch' % i, (x, -1.199, .88), (.10, .018, .03), M_STEEL, .004); box('SideLockerArm%d' % i, (x, -.56, .86), (.40, .22, .08), M_CHAS, .006)
cyl('AirTank', (-.10, -.62, .62), .12, .0 + .60, 'x', M_DARK, 36, .03); box('AirTankArm', (-.10, -.52, .76), (.10, .10, .12), M_CHAS, .006)
for s, sn in ((1, 'L'), (-1, 'R')):
    x = -.30; c = (x, s * 1.08, 0)
    box('ExtBox_%s_Back' % sn, (x, s * .955, .76), (.24, .014, .56), M_RED, .003, 1); box('ExtBox_%s_Floor' % sn, (x, s * 1.075, .49), (.24, .25, .016), M_RED, .003, 1)
    for e in (-.113, .113): box('ExtBox_%s_Side%d' % (sn, e > 0), (x + e, s * 1.075, .60), (.014, .25, .24), M_RED, .003, 1)
    box('ExtArm_' + sn, (x, s * .70, .80), (.10, .52, .06), M_CHAS, .006)
    n = 'Extinguisher_' + sn; CK.lathez(n, [(0, .499), (.078, .499), (.082, .51), (.082, .86), (.070, .895), (.030, .925), (.024, .95), (0, .95)], c, M_RED, 44); cyl(n + '_Valve', (x, s * 1.08, .965), .022, .05, 'z', M_BRASS, 24); box(n + '_Grip', (x - .045, s * 1.08, .995), (.13, .022, .018), M_SBLK, .006); box(n + '_Lever', (x - .05, s * 1.08, 1.025), (.12, .022, .012), M_SBLK, .005); box(n + '_LeverPost', (x + .004, s * 1.08, 1.008), (.02, .02, .04), M_SBLK, .004)
    tube(n + '_Hose', [(x + .02, s * 1.08, .965), (x + .11, s * 1.08, .95), (x + .095, s * 1.10, .75), (x + .092, s * 1.11, .64)], .011, M_SBLK); box(n + '_Band', (x, s * 1.163, .70), (.085, .004, .13), M_SWHT, .001, 1); ik(n, (x - .045, s * 1.08, 1.005)); ik('Stand_' + n, (x, s * 1.75, 0))
box('ShutL_Box', (2.03, .99, 1.40), (.12, .08, .20), M_YEL, .012, 2); cyl('ShutL_Button', (2.03, 1.045, 1.42), .036, .04, 'y', M_RED, 36, .008); box('ShutL_Arm', (2.09, .99, 1.40), (.08, .05, .10), M_DARK, .006); box('ShutR_Box', (2.03, -.99, 1.40), (.12, .08, .20), M_YEL, .012, 2); cyl('ShutR_Button', (2.03, -1.045, 1.42), .036, .04, 'y', M_RED, 36, .008); box('ShutR_Arm', (2.09, -.99, 1.40), (.08, .05, .10), M_DARK, .006); ik('EmergencyL', (2.03, 1.066, 1.42)); ik('EmergencyR', (2.03, -1.066, 1.42))
# bonding reel under the panel, left: drum, flanges, cable turns, the lead to its clamp on a brass stud
bx, by, bz = 2.55, 1.06, .585
cyl('BondReel_Drum', (bx, by, bz), .085, .134, 'y', M_DARK, 40)
for e in (-.065, .065): cyl('BondReel_Flange%d' % (e > 0), (bx, by + e, bz), .165, .010, 'y', M_YEL, 56, .003)
for k in range(8):
    for j in range(2): torus('BondReel_Turn%d_%d' % (j, k), (bx, by - .049 + .014 * k, bz), .092 + .0152 * j, .0075, 'y', M_CAB, 48, 8)
cyl('BondReel_Axle', (bx, by + .075, bz), .022, .03, 'y', M_STEEL, 24); cyl('BondReel_Crank', (bx + .07, by + .078, bz), .011, .025, 'y', M_SBLK, 20, .003); tube('BondReel_CrankArm', [(bx, by + .085, bz), (bx + .07, by + .085, bz)], .008, M_STEEL, bez=False); ik('BondReel_Crank', (bx + .07, by + .095, bz))
box('BondFrame', (bx, by - .10, bz + .11), (.12, .03, .28), M_CHAS, .006); cyl('BondFrame_Axle', (bx, by - .085, bz), .02, .06, 'y', M_STEEL, 24); box('BondFrame_Foot', (bx, .76, .765), (.12, .50, .04), M_CHAS, .006); box('BondFrame_Foot2', (bx, .50, .80), (.12, .10, .10), M_CHAS, .006)
box('BondGuide', (bx - .22, by - .02, bz + .06), (.05, .06, .05), M_STEEL, .012, 2); box('BondGuide_Arm', (bx - .14, by - .10, bz + .10), (.20, .03, .04), M_CHAS, .006); box('BondGuide_Arm2', (bx - .22, by - .07, bz + .085), (.04, .08, .04), M_CHAS, .006)
box('BondStud', (bx - .40, by - .02, bz + .22), (.016, .06, .09), M_BRASS, .003); box('BondStud_Foot', (bx - .40, .75, bz + .24), (.05, .50, .04), M_CHAS, .006); box('BondStud_Foot2', (bx - .40, .50, .80), (.08, .10, .08), M_CHAS, .006)
tube('BondTail', [(bx, by - .02, bz + .105), (bx - .22, by - .02, bz + .06), (bx - .32, by - .01, bz + .00), (bx - .40, by - .0, bz + .04), (bx - .40, by + .0, bz + .115)], .0075, M_CAB)
for e in (-1, 1): turn(box('BondClamp_Jaw%d' % (e > 0), (bx - .40 + e * .014, by, bz + .175), (.014, .024, .13), M_BRASS, .004), (bx - .40, by, bz + .175), 'y', e * 4)
cyl('BondClamp_Spring', (bx - .40, by, bz + .145), .011, .05, 'x', M_STEEL, 20); box('BondClamp_GripA', (bx - .425, by, bz + .125), (.012, .026, .05), M_RED, .004); box('BondClamp_GripB', (bx - .375, by, bz + .125), (.012, .026, .05), M_RED, .004)
piv('BondReel', (bx, by, bz)); ik('BondClamp', (bx - .40, by, bz + .125)); ik('BondOut', (bx - .245, by - .02, bz + .06)); ik('Stand_Bond', (bx - .2, 1.75, 0)); text('BondWord', 'BONDING', (2.36, CY - .0, .86), .05, 'y+', M_SBLK, .0012)
# own wheel chocks: in a tray under the locker, right; where they go: before and behind the left outer drive wheel
box('ChockTray', (2.55, -1.03, .475), (.66, .30, .02), M_DARK, .004)
for e in (-.32, .32): box('ChockTray_End%d' % (e > 0), (2.55 + e, -1.03, .535), (.016, .30, .12), M_DARK, .003, 1)
box('ChockTray_Back', (2.55, -.885, .62), (.66, .016, .30), M_DARK, .003, 1); box('ChockTray_Arm', (2.55, -.66, .74), (.12, .46, .06), M_CHAS, .006); box('ChockTray_Arm2', (2.55, -.49, .80), (.12, .10, .10), M_CHAS, .006); CHK = {}
for i, x in enumerate((2.40, 2.70)):
    o = prism('Chock_%d' % i, [(x - .125, .486), (x + .125, .486), (x + .125, .52), (x + .06, .645), (x + .02, .66), (x - .02, .64), (x - .125, .52)], -1.13, -.95, M_YEL, .012, 2); tube('Chock_%d_Grip' % i, [(x, -1.09, .645), (x, -1.085, .70), (x, -1.04, .715), (x, -.995, .70), (x, -.99, .645)], .009, M_SBLK)
    for k in range(3): box('Chock_%d_Rib%d' % (i, k), (x - .06 + .05 * k, -1.04, .4855), (.02, .165, .006), M_SBLK, .001, 1)
    ik('Chock_%d' % i, (x, -1.04, .715))
ik('ChockPlace_0', (DX + RW + .06, DYO, 0)); ik('ChockPlace_1', (DX - RW - .06, DYO, 0)); ik('Stand_Chocks', (2.55, -1.80, 0)); ik('Stand_ChockPlace', (DX, 1.85, 0))
# #endregion

# #region ---- rear module: reels, scissor, platform, steps, nozzles
RXc, RZc, RR_, RY, HR = -4.06, .925, .425, .82, .032              # reel: middle x, z, flange radius, |y|; the hose's radius (64 mm outside, 50 mm bore)
LAY = [(.200 + HR + k * .062, 5) for k in range(3)]; HOSEM = sum(2 * math.pi * r * n for r, n in LAY)
tm = {}
for s, sn in ((1, 'L'), (-1, 'R')):
    n = 'HoseReel_' + sn; cyl(n + '_Drum', (RXc, s * RY, RZc), .200, .345, 'y', M_DARK, 48)
    for e in (-.180, .180):
        cyl(n + '_Flange%d' % (e > 0), (RXc, s * RY + e, RZc), RR_, .012, 'y', M_FRM, 72, .003); torus(n + '_Lip%d' % (e > 0), (RXc, s * RY + e, RZc), RR_ - .004, .010, 'y', M_FRM, 72, 10)
        for j in range(6): turn(box(n + '_Rib%d_%d' % (e > 0, j), (RXc, s * RY + e + (.010 if e > 0 else -.010), RZc + .24), (.035, .012, .26), M_FRM, .003, 1), (RXc, s * RY, RZc), 'y', 60 * j)
    cyl(n + '_Axle', (RXc, s * RY, RZc), .05, .46, 'y', M_STEEL, 36); piv(n, (RXc, s * RY, RZc))
    i = 0
    for k, (r, cnt) in enumerate(LAY):
        for j in range(cnt):
            if k not in tm: tm[k] = torus('turn%d' % k, (0, 0, 0), r, HR - .0008, 'y', M_HOSE, 64, 14); ALL.remove(tm[k])
            o = bpy.data.objects.new('HoseTurn_%s_%02d' % (sn, i), tm[k].data); bpy.context.collection.objects.link(o); o.location = (RXc, s * RY - .132 + .066 * j + (.0 if k % 2 == 0 else .0), RZc); ALL.append(o); i += 1
    for e in (-.245, .245): 
        if s * e < 0: box('ReelStand%d_%s' % (e > 0, sn), (RXc, s * RY + e, .87), (.12, .030, .22), M_CHAS, .008)
        else: box('ReelStand%d_%s' % (e > 0, sn), (RXc, s * RY + e, .74), (.12, .030, .50), M_CHAS, .008)
    box('ReelBeam_' + sn, (RXc, s * .78, .485), (.14, .66, .06), M_CHAS, .008); box('ReelPost_' + sn, (RXc, s * .50, .66), (.14, .09, .36), M_CHAS, .008)
    cyl('ReelMotor_' + sn, (RXc, s * .545, RZc), .07, .10, 'y', M_DARK, 32, .01)
    # the guide: four rollers the hose runs through, at the back
    gx, gz = -4.74, .80
    for e in (-.052, .052):
        cyl('Guide_%s_V%d' % (sn, e > 0), (gx, s * RY + e, gz), .016, .14, 'z', M_STEEL, 24); cyl('Guide_%s_H%d' % (sn, e > 0), (gx, s * RY, gz + e), .016, .13, 'y', M_STEEL, 24)
    for e in (-.075, .075): box('Guide_%s_P%d' % (sn, e > 0), (gx, s * RY, gz + e), (.05, .17, .016), M_CHAS, .004)
    box('Guide_%s_Arm' % sn, (gx, s * .61, gz - .02), (.05, .30, .05), M_CHAS, .006); ik('HoseOut_' + sn, (gx - .03, s * RY, gz)); box('Guide_%s_Side' % sn, (gx, s * .735, gz), (.05, .016, .16), M_CHAS, .004)
    # the nozzle on its holder: mouth up in a cup with the dust cap, the two handles out, hose tail down
    nx, ny, nz = -4.80, s * 1.06, 1.235; q = 'Nozzle_' + sn
    box('NozzleHolder_%s_Arm' % sn, (nx + .06, s * .83, 1.335), (.06, .62, .03), M_CHAS, .006); box('NozzleHolder_%s_Post' % sn, (nx + .06, s * .56, 1.16), (.06, .06, .36), M_CHAS, .006); box('NozzleHolder_%s_Foot' % sn, (nx + .06, s * .50, 1.0), (.10, .14, .06), M_CHAS, .006); cyl('NozzleHolder_%s_Cup' % sn, (nx, ny, 1.325), .080, .05, 'z', M_RED, 40, .006); box('NozzleHolder_%s_Neck' % sn, (nx + .04, ny, 1.335), (.10, .06, .028), M_CHAS, .004)
    CK.lathez(q + '_Body', [(0, -.125), (.032, -.125), (.036, -.10), (.040, -.06), (.058, -.03), (.062, .0), (.062, .045), (.050, .050), (.050, .070), (0, .070)], (nx, ny, nz), M_ALU, 44); cyl(q + '_Collar', (nx, ny, nz + .040), .074, .045, 'z', M_RED, 48, .006)
    for j in range(3): turn(box(q + '_Lug%d' % j, (nx + .074, ny, nz + .062), (.022, .020, .020), M_STEEL, .003), (nx, ny, nz), 'z', 120 * j)
    for e in (-1, 1):
        tube(q + '_Handle%d' % (e > 0), [(nx, ny + e * .055, nz + .02), (nx, ny + e * .10, nz + .015), (nx, ny + e * .145, nz - .005), (nx, ny + e * .150, nz - .10)], .012, M_SBLK); ik('%s_Hand_%s' % (q, 'L' if e * s > 0 else 'R'), (nx, ny + e * .150, nz - .06))
    box(q + '_FlowLever', (nx - .062, ny, nz - .01), (.02, .03, .10), M_STEEL, .004); cyl(q + '_Swivel', (nx, ny, nz - .145), .038, .05, 'z', M_STEEL, 36, .004); cyl(q + '_Ferrule', (nx, ny, nz - .185), .037, .05, 'z', M_STEEL, 36, .004)
    piv(q, (nx, ny, nz)); ik(q + '_Mouth', (nx, ny, nz + .070)); ik(q + '_Hose', (nx, ny, nz - .205)); ik('Stand_' + q, (nx - .10, s * 1.70, 0))
    r = LAY[2][0]; tube('HoseTail_' + sn, [(RXc + .05, s * RY, RZc + r), (RXc - .12, s * RY, RZc + r - .02), (RXc - .34, s * RY, RZc + .17), (gx + .10, s * RY, gz + .03), (gx, s * RY, gz), (gx - .12, s * (RY + .02), gz - .06), (gx - .17, s * (RY + .10), gz - .13), (gx - .13, s * 1.02, gz - .11), (nx - .02, ny, gz + .03), (nx, ny, nz - .20)], HR - .001, M_HOSE, 14)
# scissor: two stages of crossed arms, 1.05 between pivots, 8.49 deg lying down, 35.2 deg at the top (0.90 up)
SL, H0, LIFT = 1.05, .31, .90; th = math.asin(H0 / (2 * SL)); SX0, SZ0 = -4.72, 1.075
def arm(n, p0, p1, y, w=.030, h=.062):
    L = math.hypot(p1[0] - p0[0], p1[1] - p0[1]); a = math.degrees(math.atan2(p1[1] - p0[1], p1[0] - p0[0])); c = ((p0[0] + p1[0]) / 2, y, (p0[1] + p1[1]) / 2)
    o = box(n, c, (L + .05, w, h), M_YEL, .012, 2); turn(o, c, 'y', -a)
    for e, pp in enumerate((p0, p1)): cyl(n + '_Pin%d' % e, (pp[0], y, pp[1]), .019, w + .022, 'y', M_STEEL, 24)
    return c
cxx, sn1 = SL * math.cos(th), SL * math.sin(th)
for st in (0, 1):
    z0, z1 = SZ0 + st * sn1, SZ0 + (st + 1) * sn1
    for s in (-1, 1):
        arm('ScissorA%d_%s' % (st + 1, 'L' if s > 0 else 'R'), (SX0, z0), (SX0 + cxx, z1), s * .345); arm('ScissorB%d_%s' % (st + 1, 'L' if s > 0 else 'R'), (SX0 + cxx, z0), (SX0, z1), s * .300)
    cyl('ScissorA%d_Cross' % (st + 1), (SX0 + cxx / 2, 0, (z0 + z1) / 2), .016, .74, 'y', M_STEEL, 24); piv('Scissor%d' % (st + 1), (SX0 + cxx / 2, 0, (z0 + z1) / 2))
box('ScissorBase', (SX0 + .52, 0, 1.032), (1.20, .78, .026), M_FRM, .004, 1)
for s in (-1, 1): box('ScissorBase_Rail%d' % (s > 0), (SX0 + .52, s * .384, 1.065), (1.20, .020, .075), M_FRM, .003, 1)
cyl('RamBarrel', (SX0 + .30, 0, 1.098), .038, .42, 'x', M_DARK, 32, .006); cyl('RamRod', (SX0 + .62, 0, 1.098), .020, .30, 'x', M_CHROME, 28); box('RamFoot', (SX0 + .12, 0, 1.06), (.08, .10, .06), M_FRM, .006); box('RamEye', (SX0 + .78, 0, 1.098), (.05, .64, .03), M_STEEL, .006); piv('Ram', (SX0 + .12, 0, 1.098)); piv('RamEye', (SX0 + .78, 0, 1.098))
PX0, PX1, PW, PZ = -4.96, -3.55, 1.17, 1.45
box('Platform_Deck', ((PX0 + PX1) / 2, 0, PZ - .010), (PX1 - PX0, PW * 2, .020), M_DECK, .003, 1)
for s in (-1, 1):
    box('Platform_SideBeam%d' % (s > 0), ((PX0 + PX1) / 2, s * (PW - .02), PZ - .045), (PX1 - PX0, .04, .055), M_FRM, .004, 1); box('Platform_Channel%d' % (s > 0), ((PX0 + PX1) / 2, s * .345, PZ - .040), (PX1 - PX0 - .05, .10, .042), M_FRM, .003, 1); box('Platform_Toe%d' % (s > 0), ((PX0 + PX1) / 2, s * (PW - .012), PZ + .045), (PX1 - PX0, .010, .10), M_YEL, .002, 1)
for e, x in enumerate((PX0 + .02, PX1 - .02)): box('Platform_EndBeam%d' % e, (x, 0, PZ - .045), (.04, PW * 2 - .06, .055), M_FRM, .004, 1)
box('Platform_ToeF', (PX1 - .012, 0, PZ + .045), (.010, PW * 2 - .06, .10), M_YEL, .002, 1)
def rail(n, pts): return tube(n, pts, .020, M_YEL, 6, False)
for s in (-1, 1):
    rail('Platform_RailTop%d' % (s > 0), [(PX0 + .03, s * .38, PZ + 1.10), (PX0 + .03, s * (PW - .06), PZ + 1.10), (PX0 + .07, s * (PW - .03), PZ + 1.10), (PX1 - .07, s * (PW - .03), PZ + 1.10), (PX1 - .03, s * (PW - .07), PZ + 1.10), (PX1 - .03, 0, PZ + 1.10)])
    rail('Platform_RailMid%d' % (s > 0), [(PX0 + .03, s * .38, PZ + .58), (PX0 + .03, s * (PW - .06), PZ + .58), (PX0 + .07, s * (PW - .03), PZ + .58), (PX1 - .07, s * (PW - .03), PZ + .58), (PX1 - .03, s * (PW - .07), PZ + .58), (PX1 - .03, 0, PZ + .58)])
    for i, (x, y) in enumerate(((PX0 + .03, .38), (PX0 + .03, PW - .06), (-4.26, PW - .03), (PX1 - .07, PW - .03), (PX1 - .03, .42))): cyl('Platform_Post%d_%d' % (s > 0, i), (x, s * y, PZ + .545), .019, 1.11, 'z', M_YEL, 24); box('Platform_PostFoot%d_%d' % (s > 0, i), (x, s * y, PZ + .006), (.07, .07, .016), M_YEL, .003, 1)
    box('Platform_ToeB%d' % (s > 0), (PX0 + .012, s * .76, PZ + .045), (.010, .72, .10), M_YEL, .002, 1); ik('Grab_Platform_' + ('L' if s > 0 else 'R'), (PX0 + .03, s * .38, PZ + .70)); ik('Platform_Work_' + ('L' if s > 0 else 'R'), (-4.30, s * .62, PZ + .002))
# the gate in the back rail: hinged on the left post, opens inwards
rail('GatePlatform_Top', [(PX0 + .03, .320, PZ + 1.04), (PX0 + .03, -.33, PZ + 1.04), (PX0 + .03, -.33, PZ + .30), (PX0 + .03, .320, PZ + .30), (PX0 + .03, .320, PZ + 1.04)]); rail('GatePlatform_Mid', [(PX0 + .03, .320, PZ + .67), (PX0 + .03, -.33, PZ + .67)])
for i, z in enumerate((.40, .94)): box('GatePlatform_Hinge%d' % i, (PX0 + .03, .345, PZ + z), (.03, .060, .05), M_DARK, .004)
box('GatePlatform_Latch', (PX0 + .03, -.345, PZ + .94), (.03, .05, .03), M_RED, .005); piv('GatePlatform', (PX0 + .03, .362, PZ + .67)); ik('Handle_GatePlatform', (PX0 + .03, -.31, PZ + 1.04)); ik('Platform_Floor', (-4.26, 0, PZ))
box('Platform_Control', (-4.26, PW - .10, PZ + .92), (.20, .10, .18), M_DARK, .012, 3); box('Platform_ControlArm', (-4.26, PW - .045, PZ + .92), (.06, .05, .06), M_DARK, .004)
box('LeverPlatform_Boss', (-4.20, PW - .160, PZ + .93), (.03, .03, .03), M_STEEL, .004); cyl('LeverPlatform_Rod', (-4.20, PW - .170, PZ + .975), .007, .09, 'z', M_STEEL, 18); cyl('LeverPlatform_Knob', (-4.20, PW - .170, PZ + 1.025), .018, .035, 'z', M_SBLK, 24, .008); piv('LeverPlatform', (-4.20, PW - .165, PZ + .93)); ik('LeverPlatform', (-4.20, PW - .170, PZ + 1.03)); text('Platform_ControlWord', 'UP / DOWN', (-4.27, PW - .1505, PZ + .86), .026, 'y-', M_SWHT, .0008); cyl('Platform_EStop', (-4.31, PW - .155, PZ + .94), .024, .03, 'y', M_RED, 28, .006)
plate('Platform_LoadPlate', (PX0 + .022, .78, PZ + .985), .52, .20, 'x-', M_YEL, .006); text('Platform_Load', 'MAX 250 kg', (PX0 + .016, .78, PZ + 1.02), .060, 'x-', M_SBLK, .0012); text('Platform_Load2', '2 PERSONS   GATE SHUT', (PX0 + .016, .78, PZ + .935), .032, 'x-', M_SBLK, .0012)
plate('Platform_JetPlate', (PX0 + .022, -.78, PZ + 1.0), .52, .17, 'x-', M_SBLK, .006); text('Platform_Jet', 'JET A-1', (PX0 + .016, -.78, PZ + .995), .12, 'x-', M_SWHT)
# steps up at the back, hand rails beside them, the rear bumper with its lamps
for i, z in enumerate((.80, 1.12)):
    box('RearStep%d' % i, (-4.965, 0, z), (.17, .62, .035), M_DECK, .006, 2)
    for s in (-1, 1): pass
for s, sn in ((1, 'L'), (-1, 'R')):
    box('RearStepSide_' + sn, (-4.93, s * .318, .85), (.20, .016, .72), M_CHAS, .004, 1); tube('RearGrab_' + sn, [(-5.01, s * .318, .78), (-5.08, s * .318, .82), (-5.08, s * .318, 1.14), (-5.01, s * .318, 1.18)], .014, M_YEL, bez=False); box('RearFrameEnd_' + sn, (-4.90, s * .38, .86), (.10, .10, .20), M_CHAS, .006)
ik('Step_Platform_0', (-5.00, 0, .615)); ik('Step_Platform_1', (-4.98, 0, .82)); ik('Step_Platform_2', (-4.98, 0, 1.14)); ik('Step_Platform_3', (-4.80, 0, PZ + .002)); ik('Stand_Platform', (-5.45, 0, 0)); ik('Grab_RearSteps_L', (-5.08, .318, 1.05)); ik('Grab_RearSteps_R', (-5.08, -.318, 1.05))
box('RearBumper', (-5.02, 0, .525), (.16, 2.40, .15), M_BUMP, .02, 3); box('RearBumper_Step', (-5.02, 0, .606), (.14, .80, .016), M_DECK, .004)
for s, sn in ((1, 'L'), (-1, 'R')):
    box('BumperArm_' + sn, (-4.90, s * .43, .66), (.10, .10, .30), M_CHAS, .006)
    box('RearLamp_%s_Housing' % sn, (-5.098, s * .80, .525), (.016, .66, .115), M_PLAST, .006, 2)
    for nm, y, w, mt in (('TailBrake', .95, .26, M_RLENS), ('IndicatorR', .735, .15, M_AMB), ('Reverse', .565, .14, M_WHT)): box('%s_%s_Lens' % (nm, sn), (-5.108, s * y, .525), (.010, w, .085), mt, .004)
plate('RearPlate', (-5.098, 0, .525), .52, .11, 'x-', M_NAME, .006)
plate('UNPlate', (-4.995 + .0, -.80, .715), .40, .22, 'x-', M_SORG, .006); text('UN_30', '30', (-5.001, -.80, .770), .095, 'x-', M_SBLK); text('UN_1863', '1863', (-5.001, -.80, .662), .095, 'x-', M_SBLK); facepoly('UN_Line', [(-.19, -.004), (.19, -.004), (.19, .004), (-.19, .004)], (-5.001, -.80, .716), 'x-', M_SBLK, .0012); box('UNArm', (-4.975, -.80, .62), (.034, .30, .16), M_CHAS, .003)
plate('NamePanel_Rear', (-4.995, .80, .715), .60, .13, 'x-', M_NAME, .006); ik('Name_Rear', (-5.001, .80, .715)); box('NameRearArm', (-4.975, .80, .63), (.034, .30, .14), M_CHAS, .003)
# #endregion

# #region ---- tank signs, name panels, side marking
for s, sn, f in ((1, 'L', 'y+'), (-1, 'R', 'y-')):
    y = s * (TA + .006); box('NamePanel_' + sn, (-.72, s * 1.211, 1.98), (2.40, .030, .42), M_NAME, .004, 1); ik('Name_' + sn, (-.72, s * 1.2262, 1.98))
    box('JetPlate_' + sn, (1.18, s * 1.208, 2.02), (.84, .030, .24), M_SBLK, .004, 1); text('JetText_' + sn, 'JET A-1', (1.18, s * 1.223, 2.02), .19, f, M_SWHT); text('PanelDoor%s_Litres' % sn, '20 000 L', (ocx - .04, s * (CY + .007), 2.46), .085, f, M_SBLK, .0012)
    box('HazPlate_' + sn, (-2.45, s * 1.208, 2.02), (.40, .030, .40), M_SWHT, .004, 1); flame('Hazard_' + sn, (-2.45, s * 1.223, 2.02), f, .19); box('SmokePlate_' + sn, (-2.83, s * 1.208, 2.02), (.28, .030, .40), M_SWHT, .004, 1); nosmoke('NoSmoke_' + sn, (-2.83, s * 1.226, 2.07), f, .11); text('NoSmokeText_' + sn, 'NO SMOKING', (-2.83, s * 1.223, 1.88), .035, f, M_SRED, .0012)
    for i, x in enumerate((1.45, .50, -2.0)): pass
    for i, (x, yy, z) in enumerate(((1.95, .90, .95), (-1.88, 1.205, .93), (-3.28, 1.10, .93))): pass
    box('SideMarkerArm_' + sn, (-1.875, s * .78, .97), (.04, .74, .03), M_CHAS, .004); box('SideMarker_%s1_Base' % sn, (-1.875, s * 1.160, .97), (.14, .05, .07), M_PLAST, .008); box('SideMarker_%s1_Lens' % sn, (-1.875, s * 1.188, .97), (.11, .014, .045), M_AMB, .003)
for i, x in enumerate((.50, 1.45)):
    for k in range(3): box('SideHi%d_%d' % (i, k), (x - .26 + .26 * k, -1.196, .60), (.24, .006, .06), M_HVR if k % 2 else M_HVW, .001, 1)
for k in range(2): box('BatHi%d' % k, (.22 + .28 * k, 1.182, .60), (.24, .008, .06), M_HVR if k % 2 else M_HVW, .001, 1)
ik('Driver_Eye', (Xo(.60), DY, FLOOR + 1.145)); ik('Aisle_Row0', (4.15, DY, FLOOR + .01)); ik('Fuel_In', (.95, 0, .7)); ik('PanelStand', (ocx + .02, 1.50, 0)); ik('Tow', (5.1, -.40, .50))
# #endregion

K.uv_cube(); S.drop_lofts()
for m in tm.values(): bpy.data.objects.remove(m)
CHECK = '--nochk' not in sys.argv
if CHECK:
    K.checks('RF', ground_ok=('Tyre',), overhang_skip_prefix=('',))
    for nm, pv, ax, dg, sk in (('DoorDriver', (4.775, 1.215, 1), 'z', (-15, -40, -68), ()), ('DoorPassenger', (4.775, -1.215, 1), 'z', (15, 40, 68), ()), ('PanelDoorL', (CX0 + .035, CY + .008, 1), 'z', (20, 60, 110), ()), ('PanelDoorR', (CX0 + .035, -CY - .008, 1), 'z', (-20, -60, -110), ()), ('GatePlatform', (PX0 + .03, .362, 1), 'z', (20, 60, 90), ())):
        h = K.sweep(nm, pv, ax, dg, sk); print('RF SWEEP', nm, len(h), sorted(h)[:10])
    for k, x, sk in (('FL', FX, -1), ('FR', FX, -1)):
        h = set()
        for dg in (-38, -20, 20, 38):
            for o in ALL:
                if o.name in ('Tyre_' + k,):
                    Rm = Matrix.Translation((x, (.8 if k[1] == 'L' else -.8), 0)) @ Matrix.Rotation(math.radians(dg), 4, 'Z') @ Matrix.Translation((-x, -(.8 if k[1] == 'L' else -.8), 0)); t = BVHTree.FromPolygons([Rm @ v.co for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons])
                    for o2 in ALL:
                        if k not in o2.name and t.overlap(K.bvh(o2)): h.add((o2.name, dg))
        print('RF STEER', k, sorted(h)[:12])
# what belongs together becomes one mesh per moving group
GROUPS = ['DoorDriver', 'DoorPassenger', 'PanelDoorL', 'PanelDoorR', 'SteeringWheel', 'Platform', 'GatePlatform', 'LeverPlatform', 'BondReel', 'BondClamp', 'BondTail', 'Nozzle_L', 'Nozzle_R', 'HoseReel_L', 'HoseReel_R', 'Extinguisher_L', 'Extinguisher_R', 'Chock_0', 'Chock_1', 'Deadman_', 'DeadmanLead',
          'ScissorA1', 'ScissorA2', 'ScissorB1', 'ScissorB2', 'Lever_Pump', 'Lever_HoseL', 'Lever_HoseR', 'Lever_Defuel', 'Pedal_Accel', 'Pedal_Brake', 'WiperL', 'WiperR'] + ['Rim_' + k for k in ()]
WK = ('FL', 'FR', 'RL', 'RR', 'TL', 'TR')
def groupof(o):
    n = o.name
    if n.startswith(('Tyre_', 'HoseTurn_', 'Body', 'HoseTail', 'GaugeNeedle', 'Meter_Face', 'RamBarrel', 'RamRod')) or n.endswith('_Lens') or n == 'BeaconLens' or 'NamePanel' in n or n in ('Tank', 'Printer_Ticket', 'Deadman_Button'): return None
    for k in WK:
        if n.startswith(('Rim_' + k, 'WheelDisc_' + k, 'Hub_' + k, 'HubCap_' + k, 'Nut_' + k)): return 'Rim_' + k
        if n.startswith(('Spindle_' + k, 'SpindleDrum_' + k)): return 'Spindle_' + k
    if n.startswith('Lever_') and 'Word' in n: return 'PanelGear'
    g = next((g for g in sorted(GROUPS, key=len, reverse=True) if n.startswith(g)), None)
    if g:
        g = g.rstrip('_')
        return g + '_Glass' if n.endswith('_Glass') else g + '_Parts' if g.startswith(('Door', 'PanelDoor')) and n != g else g
    if n.endswith('_Glass'): return 'GlassFixed'
    if n.startswith(('Seat', 'Belt')): return 'Seats'
    if n.startswith(('Dash', 'CabM', 'SunV', 'GrabIn', 'Steer', 'Stalk', 'Foot', 'Gear', 'ParkB', 'Firewall', 'Cowl', 'Locker_Back', 'Clip', 'RoofL', 'CabLiner', 'Floor', 'EngineHump', 'BPillar')): return 'CabFittings'
    if n.startswith(('Panel', 'Gauge', 'Meter', 'Printer', 'Button', 'Lever', 'Deadman', 'Flow', 'Sample', 'Locker')): return 'PanelGear'
    if n.startswith(('Cabinet', 'CabVent', 'CabMount')): return 'CabinetBody'
    if n.startswith(('Tank', 'Coam', 'Top', 'Manhole', 'Vent', 'Pipe', 'Jet', 'Litre', 'Haz', 'Smoke', 'NoSmoke')): return 'TankFittings'
    if n.startswith(('Reel', 'Guide', 'NozzleH', 'Scissor', 'RamF', 'RamE', 'Rear', 'Bumper', 'UN', 'NameRear')): return 'RearFittings'
    if n.startswith(('Frame', 'Axle', 'Leaf', 'Hanger', 'Drive', 'Air', 'Fender', 'Mud', 'Prop', 'Gearbox', 'Engine', 'Diff', 'Pump', 'Exhaust', 'SteerArm')): return 'Underside'
    return 'OuterFittings'
def joined():
    G = {}
    for o in ALL:
        g = groupof(o)
        if g: G.setdefault(g, []).append(o)
    for g, l in G.items():
        l[0].name = 'tmp_' + g; CK.join(g, l)
    for o in ALL:
        assert o.data.materials and all(o.data.materials), o.name; assert {p.material_index for p in o.data.polygons} == set(range(len(o.data.materials))), 'empty slot ' + o.name
if '--pic' in sys.argv:
    OUT = sys.argv[sys.argv.index('--pic') + 1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.use_gtao = True; sc.eevee.taa_render_samples = 10
    w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (.6, .68, .8, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = .8; sc.world = w; K.glass(M_GLASS, .25)
    sd = bpy.data.lights.new('S', 'SUN'); sd.energy = 3.5; so = bpy.data.objects.new('S', sd); sc.collection.objects.link(so); so.rotation_euler = (.8, .1, .6)
    cd = bpy.data.cameras.new('C'); bc = bpy.data.objects.new('C', cd); sc.collection.objects.link(bc); sc.camera = bc; sc.render.resolution_x, sc.render.resolution_y = 900, 600
    def shot(n, loc, at, lens=40):
        bc.location = loc; bc.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cd.lens = lens; cd.clip_start = .02; sc.render.filepath = os.path.join(OUT, n + '.png'); bpy.ops.render.render(write_still=True)
    shot('a1', (10.5, 8.5, 3.2), (.2, 0, 1.4)); shot('a2', (-10.5, 7.5, 3.2), (-.6, 0, 1.4))
    def mv(pre, pv, deg):
        for o in ALL:
            if o.name.startswith(pre): o.matrix_world = Matrix.Translation(Vector(pv)) @ Matrix.Rotation(math.radians(deg), 4, 'Z') @ Matrix.Translation(-Vector(pv)) @ o.matrix_world
    mv('DoorDriver', (4.775, 1.215, 1), -68); mv('PanelDoorL', (CX0 + .035, CY + .008, 1), 110); bpy.context.view_layer.update()
    shot('a3', (3.6, 4.2, 2.1), (3.3, 0, 1.6), 24); shot('a4', (-7.4, 2.6, 2.3), (-4.4, 0, 1.2), 34); shot('b1', (3.62, .62, 2.4), (4.9, .45, 1.75), 16); shot('b2', (-7.2, -2.8, 1.6), (-4.4, -.2, 1.2), 34)
    sys.exit(0)
GLB = os.path.join(ART, 'refueller.glb'); ZF = os.path.join(ART, 'zfight-check.json')
last = K.save_export(ROOT, os.path.join(ART, 'Refueller.blend'), GLB, ZF, 'RF', 'Refueller'); joined()
last2 = K.save_export(ROOT, os.path.join(ART, 'Refueller.blend'), GLB, ZF, 'RF joined', 'RefuellerGame')
ue = lambda v: [round(v[0] * 100, 1), round(-v[1] * 100, 1), round(v[2] * 100, 1)]
tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in ALL); lo = [min(K.bbox(o)[0][i] for o in ALL) for i in range(3)]; hi = [max(K.bbox(o)[1][i] for o in ALL) for i in range(3)]
json.dump({'note': 'points in the model root, Unreal cm (Blender (x,y,z) m = (x,-y,z) * 100)', 'size_m': [round(hi[i] - lo[i], 3) for i in range(3)], 'body_m': [10.17, 2.45, 3.08], 'wheelbase': FX - DX, 'tag_behind_drive': DX - TX, 'tyre_radius_cm': RW * 100,
           'scissor': {'arm_cm': SL * 100, 'h0_cm': H0 * 100, 'lift_cm': LIFT * 100, 'deg0': round(math.degrees(th), 3), 'base_cm': [SX0 * 100, SZ0 * 100]}, 'hose': {'turn_m': [round(2 * math.pi * r, 3) for r, n in LAY for _ in range(n)], 'radius_cm': HR * 100, 'total_m': round(HOSEM, 2)},
           'points_cm': {e.name: ue(e.location) for e in IKS}, 'parts': sorted(o.name for o in ALL), 'materials': sorted({m.name for o in ALL for m in o.data.materials}), 'tris': tris, 'flicker': last2}, open(os.path.join(ART, 'refueller-parts.json'), 'w'), indent=1)
json.dump({'points': {e.name: [round(c, 4) for c in e.location] for e in IKS}}, open(os.path.join(ROOT, 'Data', 'refueller-riders.json'), 'w'), indent=1)
print('DONE RF: %d parts, %d points, %d tris, size %s, hose %.1f m, %s' % (len(ALL), len(IKS), tris, [round(hi[i] - lo[i], 3) for i in range(3)], HOSEM, last2))
