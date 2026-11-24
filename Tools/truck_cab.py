# The low day cab of our service trucks (the refueller's, Art/Refueller/make_refueller.py, made a text of its own for the next trucks of to-do 212):
# materials, signs' helpers, the cab with doors, seats, wheel and pedals, wheels and mudguards. It runs in the caller's own names:
#   PFX = 'CT_'; DASHWORD = 'HIGH LOADER'; FX, RW, TW, FLOOR, CABB, ox, oz set as in make_refueller.py; then  exec(compile(open(f).read(), f, 'exec'))
# #region ---- materials
P = K.pmat
M_BODY = P(PFX + 'Livery_Body_Paint', (.90, .91, .92), .1, .35); M_MAIN = P(PFX + 'Livery_Main_Paint', (.03, .20, .36), .1, .35); M_NAME = P(PFX + 'NamePanel_Paint', (.92, .92, .92), 0, .4)
M_BUMP = P(PFX + 'BumperPlastic', (.05, .05, .055), 0, .62); M_PLAST = P(PFX + 'BlackPlastic', (.035, .035, .04), 0, .55); M_DASH = P(PFX + 'DashPlastic', (.10, .105, .115), 0, .7)
M_LIN = P(PFX + 'HeadLining', (.60, .60, .58), 0, .9); M_TRIM = P(PFX + 'GreyTrimPlastic', (.30, .31, .33), 0, .75); M_GLASS = P(PFX + 'Glass', (.2, .3, .35), 0, .05); M_SEAL = P(PFX + 'SealRubber', (.02, .02, .02), 0, .85)
M_TYRE = P(PFX + 'Tyre', (.03, .03, .03), 0, .9); M_STEEL = P(PFX + 'Steel', (.5, .51, .53), 1, .4); M_DARK = P(PFX + 'DarkSteel', (.09, .09, .1), .6, .55); M_RIM = P(PFX + 'RimSteel', (.62, .63, .65), .9, .35)
M_CHAS = P(PFX + 'Chassis_Paint', (.06, .065, .07), .3, .5); M_FRM = P(PFX + 'Platform_Paint', (.36, .38, .40), .4, .5); M_YEL = P(PFX + 'SafetyYellow_Paint', (.95, .72, .02), .05, .4); M_ALU = P(PFX + 'Aluminium', (.72, .73, .75), .9, .4)
M_DECK = P(PFX + 'DiamondPlate', (.55, .56, .57), .9, .5); M_MAT = P(PFX + 'FloorMat', (.05, .05, .05), 0, .9); M_SEAT = P(PFX + 'SeatFabric', (.06, .065, .075), 0, .85); M_SEAT2 = P(PFX + 'SeatBolster', (.03, .06, .14), 0, .85)
M_HOSE = P(PFX + 'HoseRubber', (.025, .025, .027), 0, .7); M_CAB = P(PFX + 'CableGreenYellow', (.45, .65, .06), 0, .5); M_BRASS = P(PFX + 'Brass', (.75, .55, .22), 1, .3); M_RED = P(PFX + 'RedPaint', (.70, .03, .02), .05, .35)
M_AMB = P(PFX + 'AmberLens', (1, .5, .04), 0, .2, 1.0); M_RLENS = P(PFX + 'RedLens', (.85, .04, .03), 0, .2, .6); M_WHT = P(PFX + 'WhiteLens', (.95, .95, .92), 0, .1, 1.0); M_CHROME = P(PFX + 'Reflector', (.8, .8, .82), 1, .12); M_MIR = P(PFX + 'MirrorGlass', (.8, .85, .9), 1, .02)
M_SCR = P(PFX + 'Screen', (.03, .10, .14), 0, .1, .4); M_LCD = P(PFX + 'MeterLcd', (.55, .64, .50), 0, .2, .3); M_HVR = P(PFX + 'HiVisRedReflector', (.75, .03, .02), 0, .35); M_HVW = P(PFX + 'HiVisWhiteReflector', (.88, .9, .9), 0, .3)
M_BELT = P(PFX + 'BeltWebbing', (.02, .02, .022), 0, .8); M_SBLK = P(PFX + 'SignBlack', (.01, .01, .012), 0, .5); M_SWHT = P(PFX + 'SignWhite', (.95, .95, .95), 0, .5); M_SORG = P(PFX + 'SignOrange', (1.0, .38, .02), 0, .5); M_SRED = P(PFX + 'SignRed', (.80, .02, .02), 0, .5); M_GRN = P(PFX + 'GreenPlastic', (.03, .42, .12), 0, .5); M_BLU = P(PFX + 'SignBlue', (.02, .18, .55), 0, .5)
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
box('Dash_Centre', (Xo(1.265), .10, Zo(1.02)), (.06, .40, .20), M_PLAST, .015, 3); box('Dash_Screen', (Xo(1.2325), .10, Zo(1.055)), (.010, .20, .10), M_SCR, .003); text('Dash_Word', DASHWORD, (Xo(1.2275), .10, Zo(1.085)), .018, 'x-', M_SWHT, .0008)
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

# #region ---- wheels and mudguards
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
# #endregion
