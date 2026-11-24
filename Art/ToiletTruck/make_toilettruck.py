# The toilet (lavatory) truck (to-do 212): the same small electric chassis as the water truck with a waste tank, a blue rinse-fluid tank, a 4 inch waste hose,
# a rinse hose reel and a lifting platform on a mast at the back.
# Sizes: the class of small lavatory trucks (6.1 - 7.3 m x 2.0 - 2.4 m, waste 1000 - 3000 l). Panel it must reach: the A320's waste panel 172AR, 31.3 m behind the nose,
# 0.8 m right of the middle, 2.8 m up (Airbus AC A320, 5-4-10: drain 4 in, rinse 1 in, tank 177 l); on Art/A320/A320.glb the belly there is at 3.13 m.
# Axes: x forward, y left, z up; metres; origin under the middle. Driver front left.   blender -b --python Art/ToiletTruck/make_toilettruck.py [-- --pic DIR]
import os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, 'Tools'), os.path.join(ROOT, 'Art', 'CleaningKit')]
PFX, DASHWORD, MAIN, NAME, KEY, TAG = 'TT_', 'WASTE   RINSE   LIFT', (.78, .20, .02), 'ToiletTruck', 'toilettruck', 'TT'
FRAME_END = -1.62
f_ = os.path.join(ROOT, 'Tools', 'small_truck.py'); exec(compile(open(f_, encoding='utf-8').read(), f_, 'exec'))
M_RT = P('TT_RinseTankBlue', (.02, .13, .55), 0, .42); M_WH = P('TT_WasteHose', (.16, .38, .33), 0, .55); M_HB = P('TT_RinseHose', (.02, .10, .42), 0, .5); M_PANEL = P('TT_PanelPaint', (.80, .82, .84), .05, .45); M_SBLUE = P('TT_SignBlue', (.01, .12, .42), 0, .5)

# #region ---- deck, waste tank, rinse tank, side boards
box('Deck', (-.23, 0, .862), (2.62, 1.98, .030), M_DECK, .005, 1)                                              # x -1.54 .. 1.08
WP = rnd([(-.80, .99), (.80, .99), (.80, 1.62), (-.80, 1.62)], .16, 8); solidx('WasteTank', WP, -1.26, .34, .07, M_SS, 40, 7)
for i, x in enumerate((-.98, -.46, .06)): bandx('WasteTank_Band%d' % i, WP, x, .05, .010, M_SS)
for i, x in enumerate((-.98, .06)): box('WasteTank_Foot%d' % i, (x, 0, .935), (.10, 1.30, .125), M_CHAS, .005)
RP_ = rnd([(-.80, .90), (.80, .90), (.80, 1.56), (-.80, 1.56)], .12, 8); solidx('RinseTank', RP_, .44, 1.02, .06, M_RT, 40, 7)
for i, x in enumerate((.60, .86)): bandx('RinseTank_Rib%d' % i, RP_, x, .07, .012, M_RT)
box('RinseTank_Foot', (.73, 0, .888), (.40, 1.30, .035), M_CHAS, .004)
cyl('RinseTank_Cap', (.73, -.45, 1.585), .10, .06, 'z', M_SBLK, 48, .008); cyl('RinseTank_Vent', (.73, .45, 1.585), .03, .07, 'z', M_SBLK, 28, .006); ik('RinseTank_Cap', (.73, -.45, 1.62))
cyl('WasteTank_Hatch', (-.15, -.52, 1.64), .19, .06, 'z', M_SS, 56, .006); cyl('WasteTank_HatchLid', (-.15, -.52, 1.680), .18, .03, 'z', M_SS, 56, .008); box('WasteTank_HatchBar', (-.15, -.52, 1.703), (.05, .32, .02), M_STEEL, .004); cyl('WasteTank_Vent', (-.72, -.60, 1.665), .035, .11, 'z', M_SS, 32, .006)
cyl('WasteTank_Dump', (-1.28, 0, 1.06), .09, .12, 'x', M_SS, 40, .006); cyl('WasteTank_DumpValve', (-1.36, 0, 1.06), .105, .05, 'x', M_DARK, 40, .006); cyl('WasteTank_DumpCap', (-1.41, 0, 1.06), .095, .06, 'x', M_RED, 40, .008); ik('WasteTank_Dump', (-1.45, 0, 1.06))
tube('Lever_Dump', [(-1.36, .085, 1.09), (-1.36, .16, 1.16), (-1.36, .22, 1.16)], .010, M_STEEL, bez=False); cyl('Lever_Dump_Knob', (-1.36, .235, 1.16), .018, .05, 'y', M_RED, 24, .005); piv('Lever_Dump', (-1.36, 0, 1.06)); ik('Lever_Dump', (-1.36, .24, 1.16))
for s, sn, f in SIDES:
    for i, x in enumerate((-1.05, -.15, .90)): box('BoardPost%d_%s' % (i, sn), (x, s * .873, 1.20), (.05, .035, .66), M_CHAS, .004, 1)
    for i, z in enumerate((1.06, 1.42)): box('BoardRail%d_%s' % (i, sn), (-.075, s * .893, z), (2.14, .025, .05), M_CHAS, .003, 1)
    box('SideBoard_' + sn, (-.075, s * .911, 1.235), (2.28, .018, .56), M_BODY, .006, 2); box('SideBoard_%s_Stripe' % sn, (-.075, s * .9205, 1.06), (2.26, .006, .18), M_MAIN, .001, 1)
    box('NamePanel_' + sn, (.11, s * .922, 1.34), (1.60, .008, .28), M_NAME, .002, 1); ik('Name_' + sn, (.11, s * .926, 1.34))
    text('Word_%s' % sn, 'LAVATORY SERVICE', (-.04, s * .9234, 1.06), .12, f, M_SWHT); x = -.93
    facepoly('Drop_' + sn, [(-.11, .10), (.11, .10), (.11, .06), (.02, -.10), (-.02, -.10), (-.11, .06)], (x, s * .921, 1.36), f, M_SBLK, .003); facepoly('Drop2_' + sn, [(-.11, .115), (.11, .115), (.11, .135), (-.11, .135)], (x, s * .921, 1.36), f, M_SBLK, .003)
for s in (-1, 1): box('HeadPost%d' % (s > 0), (1.06, s * .70, 1.27), (.05, .07, .80), M_CHAS, .006)
for i, z in enumerate((1.02, 1.36, 1.64)): box('HeadBar%d' % i, (1.06, 0, z), (.04, 1.44, .06), M_CHAS, .005)
# #endregion

# #region ---- the waste hose: from the tank's inlet elbow forward in its rack, round and back to the coupling behind (4 inch, 110 mm outside)
WR, YI, YC, ZH = .055, .16, .66, 1.745
cyl('HoseInlet', (-1.08, YI, 1.655), .075, .10, 'z', M_SS, 40, .006); torus('HoseInlet_Flange', (-1.08, YI, 1.63), .085, .016, 'z', M_STEEL, 40, 12)
tube('HoseInlet_Elbow', [(-1.08, YI, 1.68), (-1.08, YI, 1.725), (-1.06, YI, ZH), (-1.00, YI, ZH), (-.92, YI, ZH)], .060, M_SS, 16)
path = [(-.95, YI, ZH), (-.60, YI, ZH), (-.20, YI, ZH), (.30, YI, ZH)]
cx, rr = .65, (YC - YI) / 2; path += [(cx + rr * math.sin(math.radians(a)), (YI + YC) / 2 - rr * math.cos(math.radians(a)), ZH) for a in range(0, 181, 15)]
path += [(.30, YC, ZH), (-.30, YC, ZH), (-.70, YC, ZH), (-1.05, YC, ZH), (-1.28, YC, ZH)]
tube('WasteHose', path, WR - .006, M_WH, 16, bez=False)
def along(step):
    d = 0; out = []; L = [(Vector(b) - Vector(a)).length for a, b in zip(path, path[1:])]; tot = sum(L); k = 0; acc = 0
    n = int(tot / step)
    for i in range(1, n):
        t = i * step
        while t > acc + L[k]: acc += L[k]; k += 1
        a, b = Vector(path[k]), Vector(path[k + 1]); u = (t - acc) / L[k]; out.append((a.lerp(b, u), (b - a).normalized()))
    return out, tot
rings, HOSEL = along(.030); bm = bmesh.new()
for p, u in rings:                                                                                               # its ribs, one mesh
    q = u.to_track_quat('Z', 'Y').to_matrix(); R = []
    for rad, dx in ((WR - .008, -.0095), (WR - .001, -.005), (WR - .001, .005), (WR - .008, .0095)): R.append([bm.verts.new(p + q @ Vector((rad * math.cos(2 * math.pi * k / 28), rad * math.sin(2 * math.pi * k / 28), dx))) for k in range(28)])
    for A, B in zip(R, R[1:]):
        for k in range(28): bm.faces.new((A[k], A[(k + 1) % 28], B[(k + 1) % 28], B[k]))
bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:]); K.finish(K.link('WasteHose_Ribs', bm), M_WH, 50)
for i, x in enumerate((-.72, -.15)):
    box('HoseRack_Bar%d' % i, (x, (YI + YC) / 2, 1.664), (.05, .74, .045), M_CHAS, .004)
    for e, y in enumerate((YI - .095, YI + .095, YC - .095, YC + .095)): box('HoseRack_Post%d_%d' % (i, e), (x, y, 1.735), (.03, .016, .14), M_CHAS, .003)
box('HoseRack_Bar2', (.50, (YI + YC) / 2, 1.618), (.05, .74, .140), M_CHAS, .004); box('HoseRack_Stop', (.975, (YI + YC) / 2, 1.67), (.016, .34, .26), M_CHAS, .003)
for e, y in enumerate((YI - .095, YI + .095, YC - .095, YC + .095)): box('HoseRack_Post2_%d' % e, (.50, y, 1.735), (.03, .016, .14), M_CHAS, .003)
ik('WasteHose_In', (-.92, YI, ZH)); ik('Stand_WasteHose', (-1.55, 1.40, 0))
# its coupling lies on a cradle over the splash tray: cam lever, two handles, cap on a chain
cX = -1.40; CK_ = (cX, YC, ZH)
cyl('WasteCoupling', (cX + .05, YC, ZH), .064, .18, 'x', M_ALU, 48, .006); cyl('WasteCoupling_Head', (cX - .085, YC, ZH), .086, .11, 'x', M_ALU, 56, .010); torus('WasteCoupling_Seal', (cX - .140, YC, ZH), .068, .010, 'x', M_SEAL, 48, 12); cyl('WasteCoupling_Ferrule', (cX + .125, YC, ZH), .066, .06, 'x', M_STEEL, 40, .004)
for e in (-1, 1):
    cyl('WasteCoupling_Lug%d' % (e > 0), (cX - .085, YC + e * .092, ZH), .018, .03, 'y', M_STEEL, 24, .003); tube('WasteCoupling_Handle%d' % (e > 0), [(cX - .07, YC + e * .08, ZH + .02), (cX - .07, YC + e * .125, ZH + .03), (cX + .04, YC + e * .125, ZH + .03), (cX + .06, YC + e * .058, ZH + .02)], .012, M_SBLK)
    ik('WasteCoupling_Hand_' + ('L' if e > 0 else 'R'), (cX - .01, YC + e * .125, ZH + .03))
tube('WasteCoupling_Lever', [(cX - .06, YC + .02, ZH + .075), (cX - .04, YC + .02, ZH + .125), (cX + .09, YC + .02, ZH + .135)], .009, M_RED); ik('WasteCoupling_Lever', (cX + .08, YC + .02, ZH + .135)); piv('WasteCoupling_Lever', (cX - .06, YC + .02, ZH + .075))
piv('WasteCoupling', CK_); ik('WasteCoupling_Mouth', (cX - .148, YC, ZH)); ik('WasteCoupling_Hose', (cX + .155, YC, ZH))
cyl('WasteCap', (cX - .172, YC, ZH), .082, .05, 'x', M_SBLK, 48, .008); box('WasteCap_Tab', (cX - .20, YC, ZH), (.03, .10, .03), M_SBLK, .006); piv('WasteCap', (cX - .172, YC, ZH)); ik('WasteCap', (cX - .21, YC, ZH))
box('Cradle_Post', (cX + .02, YC, 1.40), (.07, .07, .52), M_CHAS, .004); box('Cradle_Foot', (cX + .06, YC, 1.145), (.16, .10, .03), M_CHAS, .004)
for e in (-1, 1): turn(box('Cradle_V%d' % (e > 0), (cX + .02, YC + e * .050, 1.680), (.07, .11, .014), M_CHAS, .003), (cX + .02, YC + e * .050, 1.680), 'x', e * 35)
# splash tray: hinged at its front edge, drops to the side of the platform while the coupling is off
TX0, TX1, TY0, TY1, TZ = -1.70, -1.46, .50, .86, 1.56
box('SplashTray', ((TX0 + TX1) / 2, (TY0 + TY1) / 2, TZ), (TX1 - TX0, TY1 - TY0, .014), M_SS, .003, 1)
for n, c, sz in (('A', (TX0 + .006, (TY0 + TY1) / 2, TZ + .025), (.012, TY1 - TY0, .06)), ('B', (TX1 - .006, (TY0 + TY1) / 2, TZ + .025), (.012, TY1 - TY0, .06)), ('C', ((TX0 + TX1) / 2, TY0 + .006, TZ + .025), (TX1 - TX0, .012, .06)), ('D', ((TX0 + TX1) / 2, TY1 - .006, TZ + .025), (TX1 - TX0, .012, .06))): box('SplashTray_' + n, c, sz, M_SS, .003, 1)
cyl('SplashTray_Hinge', (TX1 + .008, (TY0 + TY1) / 2, TZ - .01), .012, .30, 'y', M_STEEL, 20); box('TrayArm', (-1.40, YC, TZ - .025), (.12, .06, .03), M_CHAS, .003); piv('SplashTray', (TX1 + .008, (TY0 + TY1) / 2, TZ - .01)); ik('SplashTray', (TX0 - .01, YC, TZ + .03))
cyl('SplashTray_Drain', (-1.50, .56, TZ - .03), .016, .06, 'z', M_SS, 24); box('CradleBeam', (-1.40, YC, 1.02), (.10, .10, .25), M_CHAS, .005); box('CradleBeam_Foot', (-1.40, YC, .89), (.18, .16, .03), M_CHAS, .004)
# #endregion

# #region ---- rinse hose reel (right), its coupling, the tank panel (left side)
RC = (-1.42, -.62, 1.20); HR = .016; HOSEM, RF, HW = reel('RinseReel', RC, HR, .075, 3, 5, M_HB)
for e, y in (('A', RC[1] - HW - .020), ('B', RC[1] + HW + .020)): box('ReelStand' + e, (RC[0], y, 1.05), (.09, .020, .38), M_CHAS, .005); box('ReelStand%s_Foot' % e, (RC[0], y, .89), (.14, .06, .03), M_CHAS, .004)
cyl('ReelMotor', (RC[0], RC[1] + HW + .075, RC[2]), .05, .10, 'y', M_DARK, 32, .008)
gx, gz = -1.63, 1.13
for e in (-.028, .028): cyl('Guide_V%d' % (e > 0), (gx, RC[1] + e, gz), .011, .10, 'z', M_STEEL, 24); cyl('Guide_H%d' % (e > 0), (gx, RC[1], gz + e), .011, .09, 'y', M_STEEL, 24)
for e in (-.055, .055): box('Guide_P%d' % (e > 0), (gx, RC[1], gz + e), (.034, .12, .014), M_CHAS, .003); box('Guide_S%d' % (e > 0), (gx, RC[1] + e, gz), (.030, .012, .118), M_CHAS, .003)
box('Guide_Arm', (gx, RC[1], .98), (.04, .05, .20), M_CHAS, .004); box('Guide_Foot', (-1.57, RC[1], .89), (.16, .08, .03), M_CHAS, .004); ik('RinseOut', (gx - .02, RC[1], gz))
hx, hy, hz = -1.66, -.88, 1.36
box('RinseHolder_Post', (-1.57, hy, 1.15), (.05, .05, .55), M_CHAS, .004); box('RinseHolder_Foot', (-1.57, hy, .89), (.14, .10, .03), M_CHAS, .004); box('RinseHolder_Arm', (-1.61, hy, 1.435), (.12, .04, .025), M_CHAS, .003); cyl('RinseHolder_Cup', (hx, hy, 1.442), .044, .034, 'z', M_RED, 40, .006)
CK.lathez('RinseCoupling', [(0, -.11), (.018, -.11), (.020, -.085), (.024, -.075), (.024, -.02), (.032, -.012), (.032, .020), (.026, .026), (.026, .050), (0, .050)], (hx, hy, hz), M_ALU, 40); cyl('RinseCoupling_Ring', (hx, hy, hz + .022), .038, .040, 'z', M_STEEL, 40, .005); cyl('RinseCoupling_Dust', (hx, hy, hz + .060), .032, .03, 'z', M_RED, 32, .004)
for j in range(3): turn(box('RinseCoupling_Lug%d' % j, (hx + .040, hy, hz + .022), (.016, .014, .026), M_STEEL, .003), (hx, hy, hz), 'z', 120 * j)
cyl('RinseCoupling_Grip', (hx, hy, hz - .075), .026, .05, 'z', M_SBLK, 32, .005); cyl('RinseCoupling_Ferrule', (hx, hy, hz - .125), .021, .04, 'z', M_STEEL, 32, .003); tube('RinseCoupling_Lever', [(hx, hy - .02, hz - .04), (hx, hy - .052, hz - .045), (hx, hy - .060, hz - .11)], .007, M_RED)
piv('RinseCoupling', (hx, hy, hz)); ik('RinseCoupling_Hand', (hx, hy, hz - .075)); ik('RinseCoupling_Ring', (hx, hy, hz + .022)); ik('RinseCoupling_Mouth', (hx, hy, hz + .075)); ik('RinseCoupling_Hose', (hx, hy, hz - .145)); ik('RinseCoupling_Lever', (hx, hy - .060, hz - .10))
tube('RinseTail', [(RC[0] - .02, RC[1], RC[2] + .148), (RC[0] - .12, RC[1], RC[2] + .07), (gx + .05, RC[1], gz + .02), (gx, RC[1], gz), (gx - .05, RC[1] - .02, gz - .04), (gx - .07, RC[1] - .08, gz - .12), (gx - .05, RC[1] - .22, gz - .13), (hx, hy, hz - .26), (hx, hy, hz - .138)], HR - .0005, M_HB, 12)
px = -1.35                                                                                                        # panel on the deck's left rear corner, facing left
box('TankPanel', (px + .02, .90, 1.17), (.22, .06, .36), M_PANEL, .008); box('TankPanel_Foot', (px + .02, .90, .935), (.14, .05, .14), M_CHAS, .004)
gauge('TankPanel_Level', (px - .035, .935, 1.275), .040, 'y+'); gauge('TankPanel_Rinse', (px + .075, .935, 1.275), .040, 'y+'); text('TankPanel_W0', 'WASTE', (px - .035, .9295, 1.215), .018, 'y+', M_SBLK); text('TankPanel_W1', 'RINSE', (px + .075, .9295, 1.215), .018, 'y+', M_SBLK)
for n, dx, m in (('RinsePump', -.04, M_SBLUE), ('RinseStop', .02, M_SRED), ('ReelIn', .08, M_SBLK)): cyl('Button_' + n, (px + dx, .934, 1.155), .018, .022, 'y', m, 28, .003); ik('Button_' + n, (px + dx, .95, 1.155)); text('Button_%s_W' % n, {'RinsePump': 'PUMP', 'RinseStop': 'STOP', 'ReelIn': 'REEL'}[n], (px + dx, .9295, 1.12), .014, 'y+', M_SBLK)
tube('Lever_Vent', [(px - .04, .925, 1.05), (px - .04, .965, 1.05), (px - .04, .975, 1.10)], .008, M_STEEL, bez=False); cyl('Lever_Vent_Knob', (px - .04, .976, 1.11), .014, .035, 'z', M_RED, 24, .004); piv('Lever_Vent', (px - .04, .925, 1.05)); ik('Lever_Vent', (px - .04, .976, 1.12)); text('TankPanel_W2', 'WASTE VALVE', (px + .05, .9295, 1.05), .016, 'y+', M_SBLK); ik('Stand_TankPanel', (px, 1.45, 0))
# #endregion

# #region ---- mast, ram, lifting platform (floor .62 down, 1.57 up), rails, gate, controls, step
MX, MY, MT = -1.72, .34, 2.30; LIFT = .95; PZ = .62
for s in (-1, 1):
    n = 'LR'[s < 0]; mp = box('Mast_' + n, (MX, s * MY, (.42 + MT) / 2), (.12, .09, MT - .42), M_FRM, .006); cut(mp, box('c', (MX, s * (MY - .035), 1.40), (.072, .07, 2.4), M_FRM))       # a channel open to the middle
    box('MastFoot_' + n, (-1.80, s * .37, .505), (.24, .12, .12), M_CHAS, .006); box('MastStay_' + n, (-1.60, s * MY, 1.20), (.14, .06, .05), M_CHAS, .004); box('MastStayPost_' + n, (-1.535, s * MY, 1.055), (.05, .06, .36), M_CHAS, .004)
box('MastTop', (MX - .0, 0, MT - .03), (.10, .70, .06), M_FRM, .006); box('MastBase', (MX, 0, .47), (.10, .66, .08), M_CHAS, .006)
cyl('RamBarrel', (MX, 0, .72), .045, .96, 'z', M_FRM, 40, .006); cyl('LiftRod', (MX, 0, .74), .026, .98, 'z', M_STEEL, 32); cyl('RamBarrel_Cap', (MX, 0, 1.190), .052, .03, 'z', M_DARK, 40, .004)
tube('RamHose', [(MX + .04, 0, .55), (MX + .14, .03, .56), (MX + .20, .10, .70), (-1.52, .20, .87)], .012, M_HOSE)
PF = ('Platform', 'Lift', 'GatePlatform', 'RailPlatform', 'Lever_Platform', 'NamePanel_Rear', 'PlatCtl')
LX = -1.815
box('LiftCarriage', (LX, 0, .94), (.05, .56, .68), M_YEL, .006)
for s in (-1, 1):
    for i, z in enumerate((.70, 1.18)): cyl('LiftRoller_%s%d' % ('LR'[s < 0], i), (MX, s * .315, z), .034, .06, 'y', M_STEEL, 32, .004); box('LiftRollerArm_%s%d' % ('LR'[s < 0], i), (MX - .045, s * .266, z), (.15, .030, .06), M_DARK, .004)
    box('LiftFork_' + 'LR'[s < 0], (-2.30, s * .24, .575), (.98, .07, .06), M_DARK, .006)
    tag = prism('LiftGusset_' + 'LR'[s < 0], [(-1.835, .60), (-2.15, .60), (-1.835, .95)], s * .24 - .012, s * .24 + .012, M_DARK)
box('LiftHead', (MX - .03, 0, 1.235), (.18, .14, .030), M_DARK, .004)
X0, X1, Y0, Y1 = -2.72, -1.86, -.58, .58
box('Platform', ((X0 + X1) / 2, 0, PZ - .008), (X1 - X0, Y1 - Y0, .024), M_DECK, .004, 1)
for n, c, sz in (('Back', (X0 + .010, 0, PZ + .05), (.018, Y1 - Y0 - .006, .11)), ('R', ((X0 + X1) / 2, Y0 + .010, PZ + .05), (X1 - X0 - .03, .018, .11)), ('FL', (X1 - .010, .43, PZ + .05), (.018, .26, .11)), ('FR', (X1 - .010, -.43, PZ + .05), (.018, .26, .11))): box('Platform_Edge' + n, c, sz, M_YEL, .003, 1)
zt = PZ + 1.0
PO = [(X0 + .022, Y0 + .022), (X0 + .022, Y1 - .022), (X1 - .03, Y0 + .022), (X1 - .03, Y1 - .022)]
for i, (x, y) in enumerate(PO): rail('RailPlatform_Post%d' % i, [(x, y, PZ - .01), (x, y, zt)])
for i, z in enumerate((PZ + .52, zt)): rail('RailPlatform_Back%d' % i, [PO[0] + (z,), PO[1] + (z,)]); rail('RailPlatform_R%d' % i, [PO[0] + (z,), PO[2] + (z,)])
for s in (-1, 1): rail('RailPlatform_Front%d' % (s > 0), [(X1 - .03, s * .558, zt), (X1 - .03, s * .34, zt), (X1 - .03, s * .34, PZ - .005)], r=.015)
ik('Rail_Back', (X0 + .022, 0, zt)); ik('Rail_R', (-2.30, Y0 + .022, zt))
gy = Y1 - .022; rail('GatePlatform', [(X0 + .075, gy, PZ + .50), (X1 - .06, gy, PZ + .50), (X1 - .06, gy, zt - .04), (X0 + .075, gy, zt - .04)], r=.015)
for i, z in enumerate((PZ + .50, zt - .04)): cyl('GatePlatform_Hinge%d' % i, (X0 + .022, gy, z), .026, .06, 'z', M_DARK, 24, .004); tube('GatePlatform_Link%d' % i, [(X0 + .03, gy, z), (X0 + .085, gy, z)], .014, M_DARK, bez=False)
piv('GatePlatform', (X0 + .022, gy, PZ + .75)); ik('Handle_GatePlatform', (X1 - .09, gy, zt - .04)); ik('Grab_Platform_R', (X0 + .022, gy, zt)); ik('Grab_Platform_F', (X1 - .03, gy, zt))
box('PlatCtl_Box', (-2.02, -.545, zt + .02), (.24, .11, .14), M_PANEL, .010); text('PlatCtl_Word', 'UP / DOWN', (-2.06, -.4895, zt - .025), .022, 'y+', M_SBLK)
tube('Lever_Platform', [(-2.06, -.545, zt + .08), (-2.06, -.545, zt + .17)], .009, M_STEEL, bez=False); cyl('Lever_Platform_Knob', (-2.06, -.545, zt + .185), .020, .04, 'z', M_SBLK, 24, .006); piv('Lever_Platform', (-2.06, -.545, zt + .09)); ik('Lever_Platform', (-2.06, -.545, zt + .20))
cyl('PlatCtl_EStop', (-1.96, -.545, zt + .10), .026, .035, 'z', M_SRED, 32, .005); ik('Button_PlatEStop', (-1.96, -.545, zt + .12))
for i, y in enumerate((-.40, .40)): rail('RailPlatform_Name%d' % i, [(X0 + .022, y, PZ + .09), (X0 + .022, y, PZ + .52)], r=.012)
rail('RailPlatform_NameBar', [PO[0] + (PZ + .24,), PO[1] + (PZ + .24,)], r=.012); box('NamePanel_Rear', (X0 + .006, 0, PZ + .30), (.012, .96, .20), M_NAME, .002, 1); ik('Name_Rear', (X0 - .011, 0, PZ + .30))
ik('Platform_Work', (-2.36, 0, PZ)); ik('Platform_In', (-2.30, .28, PZ)); ik('Platform_Hose', (-2.02, .30, PZ)); ik('Platform_RinseStand', (-2.02, -.30, PZ))
box('PlatStep', (-2.30, .81, .33), (.60, .30, .035), M_DECK, .005, 1); ik('Step_Platform_0', (-2.30, .81, .348)); ik('Stand_Steps', (-2.30, 1.45, 0))
for e, x in ((0, -2.018), (1, -2.582)): box('PlatStep_Side%d' % e, (x, .70, .39), (.016, .50, .14), M_CHAS, .003, 1)
box('PlatStep_Cross', (-2.30, .45, .40), (.60, .05, .07), M_CHAS, .004); box('PlatStep_Arm0', (-2.04, .42, .45), (.08, .10, .08), M_CHAS, .004)
box('LowerCtl', (MX, .422, 1.15), (.10, .085, .16), M_PANEL, .006); tube('Lever_Lower', [(MX, .455, 1.15), (MX, .505, 1.15), (MX, .515, 1.20)], .008, M_STEEL, bez=False); cyl('Lever_Lower_Knob', (MX, .516, 1.21), .015, .035, 'z', M_SBLK, 24, .004); piv('Lever_Lower', (MX, .455, 1.15)); ik('Lever_Lower', (MX, .516, 1.22)); ik('Stand_Lower', (MX, 1.45, 0))
# signs
box('MastSign', (MX - .064, 0, MT - .030), (.010, .64, .050), M_SBLK, .002, 1); text('MastSign_Word', 'GLOVES - VISOR - APRON', (MX - .0685, 0, MT - .030), .034, 'x-', M_YEL, .0012)
box('WorkLamp_Body', (MX - .05, 0, MT + .035), (.10, .28, .07), M_PLAST, .012); box('WorkLamp_Lens', (MX - .102, 0, MT + .035), (.008, .24, .045), M_WHT, .002)
box('TankSign', (-1.2725, -.0, 1.36), (.030, .66, .16), M_SBLK, .004); text('TankSign_W0', 'WASTE WATER', (-1.287, 0, 1.395), .055, 'x-', M_YEL, .0012); text('TankSign_W1', 'KEEP AWAY FROM DRINKING WATER', (-1.287, 0, 1.325), .030, 'x-', M_YEL, .0012)
# #endregion

def extra(n):
    if n.startswith(('Lift', 'Platform', 'RailPlatform', 'PlatCtl')): return 'PlatformLift'
    if n.startswith(('Mast', 'RamBarrel', 'RamHose', 'PlatStep', 'LowerCtl', 'WorkLamp')): return 'MastParts'
    if n.startswith(('Tank', 'Button', 'ReelStand', 'ReelMotor', 'Guide', 'RinseHolder', 'Cradle', 'TrayArm', 'HoseRack', 'HoseInlet')): return 'RearGear'
    if n.startswith(('WasteTank', 'RinseTank', 'Head', 'Board', 'SideBoard', 'Word', 'Drop', 'Deck')): return 'TankParts'
finish_truck(['GatePlatform', 'RinseReel', 'RinseTail', 'RinseCoupling', 'WasteCoupling', 'WasteCap', 'WasteHose', 'SplashTray', 'Lever_Platform', 'Lever_Dump', 'Lever_Vent', 'Lever_Lower', 'LiftRod'], lambda n: extra(n),
             [('GatePlatform', (X0 + .022, gy, 1), 'z', (-30, -60, -80), ('RailPlatform_Post1',)), ('SplashTray', (TX1 + .008, .68, TZ - .01), 'y', (-30, -60, -85), ('TrayArm',))],
             [(PF, (0, 0, 1), (0, .3, .6, .95), ('RamBarrel', 'Mast_'))],
             {'waste_hose': {'metres': round(HOSEL + .3, 2), 'radius_cm': WR * 100, 'path_m': [[round(c, 3) for c in p] for p in path]}, 'rinse_hose': {'metres': round(HOSEM, 2), 'radius_cm': HR * 100, 'turns': 15}, 'waste_litres': 1500, 'rinse_litres': 550,
              'lift': {'cm': LIFT * 100, 'floor_down_cm': PZ * 100, 'floor_up_cm': (PZ + LIFT) * 100, 'moves': list(PF)},
              'a320': {'door': '172AR', 'behind_nose_wheel_m': 26.2, 'right_m': .8, 'height_real_m': 2.8, 'height_model_m': 3.13}})
