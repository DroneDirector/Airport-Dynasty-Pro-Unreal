# The second part of make_ambulift.py: the passenger cabin, its front platform and tail lift (all of it moves up with the lift).
BF, BX0, BX1, BY, BT = 1.28, -4.0, 2.56, 1.20, 3.42; IY, IT = 1.15, 3.35; IX0, IX1 = BX0 + .05, BX1 - .05
def ring(n, pts, ax, a, b, w, m): o = rawprism(n, grow(pts, w), ax, a, b); boole(o, rawprism('c', grow(pts, -w), ax, a - 1, b + 1), 'DIFFERENCE', False); return K.finish(o, m, 30)
def pr(n, pts, ax, a, b, m, sm=30): return K.finish(rawprism(n, pts, ax, a, b), m, sm)
# #region ---- body shell, windows, sliding doors
shellb = rawprism('BoxShell', rnd([(-BY, 1.20), (BY, 1.20), (BY, BT), (-BY, BT)], [.03, .03, .14, .14], 6), 'x', BX0, BX1); boole(shellb, rawprism('c', rnd([(-IY, BF), (IY, BF), (IY, IT), (-IY, IT)], [.01, .01, .10, .10], 5), 'x', IX0, IX1), 'DIFFERENCE', False)
WX = ((1.02, 2.20), (-.52, .82), (-2.06, -.72), (-3.60, -2.26)); WZ = (2.06, 3.04); DY_ = .55
WP = [rnd([(a, WZ[0]), (b, WZ[0]), (b, WZ[1]), (a, WZ[1])], .07) for a, b in WX]; EP = [rnd([(min(s * .68, s * 1.06), WZ[0]), (max(s * .68, s * 1.06), WZ[0]), (max(s * .68, s * 1.06), WZ[1]), (min(s * .68, s * 1.06), WZ[1])], .06) for s in (1, -1)]
for p in WP: boole(shellb, rawprism('c', p, 'y', -2, 2), 'DIFFERENCE', False)
for p in EP: boole(shellb, rawprism('c', p, 'x', -5, 5), 'DIFFERENCE', False)
boole(shellb, rawprism('c', rnd([(-DY_, BF), (DY_, BF), (DY_, 3.24), (-DY_, 3.24)], [.005, .005, .06, .06], 4), 'x', -5, 5), 'DIFFERENCE', False)
shellb.data.materials.clear(); [shellb.data.materials.append(m) for m in (M_BODY, M_WALL)]
for p in shellb.data.polygons: c = p.center; p.material_index = 1 if abs(c.y) < IY + .004 and IX0 - .004 < c.x < IX1 + .004 and BF - .01 < c.z < IT + .004 else 0; p.use_smooth = True
shellb.data.use_auto_smooth = True; shellb.data.auto_smooth_angle = math.radians(30); ALL.append(shellb) if shellb not in ALL else None
for s, sn, f in SIDES:
    for i, p in enumerate(WP): pr('BoxWin%d%s_Glass' % (i, sn), grow(p, .008), 'y', *sorted((s * 1.176, s * 1.184)), M_GLASS); ring('BoxWin%d%s_Seal' % (i, sn), p, 'y', *sorted((s * 1.146, s * 1.2045)), .016, M_SEAL)
    for e, x in (('F', BX1 - .025), ('R', BX0 + .025)): p = EP[s < 0]; pr('BoxWin%s%s_Glass' % (e, sn), grow(p, .008), 'x', x - .004, x + .004, M_GLASS); ring('BoxWin%s%s_Seal' % (e, sn), p, 'x', x - .0275, x + .0275, .016, M_SEAL)
    box('BoxBand_' + sn, (-.72, s * 1.2, 1.62), (6.40, .009, .42), M_MAIN, .002, 1); box('NamePanel_' + sn, (.30, s * 1.2045, 1.62), (2.80, .010, .30), M_NAME, .003, 1); ik('Name_' + sn, (.30, s * 1.2102, 1.62))
    for i in range(9): box('BoxHi%d_%s' % (i, sn), (-3.60 + .72 * i, s * 1.2, 1.325), (.62, .008, .06), M_HVR if i % 2 else M_HVW, .001, 1)
    c = (-2.85, s * 1.2045, 1.62); box('BoxSign_' + sn, c, (.34, .010, .34), M_BLU, .003, 1); fc = (c[0], s * 1.2095, c[2])
    facepoly('BoxSign_%sHead' % sn, [(.026 * math.cos(math.pi * k / 10) - .02, .115 + .026 * math.sin(math.pi * k / 10)) for k in range(20)], fc, f, M_SWHT)
    facepoly('BoxSign_%sBody' % sn, [(-.035, .08), (-.008, .08), (.0, .028), (.06, .028), (.06, .008), (.07, -.008), (.11, -.09), (.088, -.10), (.052, -.020), (-.022, -.020)], fc, f, M_SWHT)
    facepoly('BoxSign_%sWheel' % sn, [(.085 * math.cos(math.radians(a)) - .005, -.065 + .085 * math.sin(math.radians(a))) for a in range(155, 336, 10)] + [(.064 * math.cos(math.radians(a)) - .005, -.065 + .064 * math.sin(math.radians(a))) for a in range(335, 154, -10)], fc, f, M_SWHT)
    for i, x in enumerate((2.40, -3.85)): box('BoxSideMark%d%s_Base' % (i, sn), (x, s * 1.204, 1.90), (.11, .016, .05), M_PLAST, .004); box('BoxSideMark%d%s_Lens' % (i, sn), (x, s * 1.216, 1.90), (.085, .012, .034), M_AMB, .004)
box('BoxFloorCover', ((IX0 + IX1) / 2, 0, BF + .001), (IX1 - IX0 - .012, 2 * IY - .012, .012), M_SFL, .002, 1)
for i in range(8): box('BoxUnder%d' % i, (-3.2 + .72 * i, 0, 1.183), (.06, .70, .036), M_CHAS, .004, 1)
for n, xw, d in (('DoorFront', IX1, -1), ('DoorRear', IX0, 1)):
    xm = xw + d * .036; box('BoxTrack' + n, (xw + d * .034, 0, 3.295), (.066, 2.26, .07), M_ALU, .004); box('BoxSill' + n, (xw + d * .036, 0, BF + .0105), (.05, 2.26, .010), M_ALU, .002, 1)
    for s, sn, f in SIDES:
        yy = sorted((s * .003, s * .585)); lf = rawprism(n + sn, [(yy[0], 1.300), (yy[1], 1.300), (yy[1], 3.255), (yy[0], 3.255)], 'x', xm - .014, xm + .014); y2 = sorted((s * .11, s * .50)); wp = rnd([(y2[0], 1.98), (y2[1], 1.98), (y2[1], 3.06), (y2[0], 3.06)], .05)
        boole(lf, rawprism('c', wp, 'x', -5, 5), 'DIFFERENCE', False); K.finish(lf, M_BODY, 30); pr('%s%s_Pane_Glass' % (n, sn), grow(wp, .006), 'x', xm - .003, xm + .003, M_GLASS); ring('%s%s_Seal' % (n, sn), wp, 'x', xm - .0165, xm + .0165, .012, M_SEAL); box('%s%s_Edge' % (n, sn), (xm, s * .0115, 2.278), (.0305, .0205, 1.95), M_SEAL, .004, 1)
        for e, xx in (('In', xm + d * .024), ('Out', xm - d * .020)): box('%s%s_Grip%s' % (n, sn, e), (xx, s * .062, 1.80), (.026 if e == 'In' else .018, .028, .22), M_YEL, .006); ik('Handle_%s%s_%s' % (n, sn, e), (xx, s * .062, 1.80))
        piv(n + sn, (xm, s * .30, 2.2))
# #endregion
# #region ---- inside: rails, seats, wheelchair places, stretcher, heater, lamps, panel
for s, sn, f in SIDES:
    tube('BoxHand_' + sn, [(-3.80, s * 1.085, 1.98), (2.20, s * 1.085, 1.98)], .016, M_YEL, 8, False); [tube('BoxHandFoot%d%s' % (i, sn), [(x, s * 1.085, 1.98), (x, s * 1.152, 1.98)], .010, M_YEL, 6, False) for i, x in enumerate((-3.7, -2.16, -.62, .92, 2.14))]; ik('BoxRail_%s_R' % sn, (-3.0, s * 1.085, 1.98)); ik('BoxRail_%s_F' % sn, (1.9, s * 1.085, 1.98))
    tube('BoxCeil_' + sn, [(-3.4, s * .55, 3.18), (2.0, s * .55, 3.18)], .016, M_YEL, 8, False); [tube('BoxCeilFoot%d%s' % (i, sn), [(x, s * .55, 3.18), (x, s * .55, 3.352)], .010, M_YEL, 6, False) for i, x in enumerate((-3.2, -1.6, 0, 1.8))]; box('BoxSkirting_' + sn, (-.72, s * 1.1435, 1.43), (6.0, .014, .26), M_TRIM, .003, 1)
for i in range(5):
    for s in (1, -1): x = -3.1 + 1.25 * i; box('BoxLamp%d%s_Base' % (i, 'LR'[s < 0]), (x, s * .85, 3.338), (.62, .12, .030), M_PLAST, .006); box('BoxLight%d%s_Lens' % (i, 'LR'[s < 0]), (x, s * .85, 3.320), (.58, .09, .012), M_WHT, .004)
def xform(n0, i0, M):
    for o in ALL[n0:]: o.data.transform(M); o.data.update()
    for e in IKS[i0:]: e.location = M @ e.location
_f0 = FLOOR; FLOOR = BF + .007
for i in range(5):                                                  # along the left wall, looking right
    n0, i0 = len(ALL), len(IKS); x = 2.00 - .54 * i; seat('BoxSeat%d' % i, 0, 0, .44, depth=.44, back=.50, zc=FLOOR + .37); xform(n0, i0, Matrix.Translation((x, .60, 0)) @ Matrix.Rotation(-math.pi / 2, 4, 'Z'))
    ik('Foot_BoxSeat%d_L' % i, (x + .10, .10, BF + .007)); ik('Foot_BoxSeat%d_R' % i, (x - .10, .10, BF + .007)); box('BoxSeatBelt%d_Reel' % i, (x - .238, .78, BF + .40), (.026, .08, .09), M_PLAST, .008); tube('BoxSeatBelt%d_Web' % i, [(x - .238, .78, BF + .43), (x - .240, .74, BF + .50), (x - .242, .70, BF + .52)], .008, M_BELT, 6); box('BoxSeatBelt%d_Tongue' % i, (x - .243, .675, BF + .522), (.034, .05, .010), M_STEEL, .003); ik('BeltTongue_BoxSeat%d' % i, (x - .243, .66, BF + .525))
FLOOR = _f0
for n, x, s in (('L0', -3.05, 1), ('L1', -1.60, 1), ('R0', -3.05, -1), ('R1', -1.60, -1)):
    yb = s * .74; ik('Bay_' + n, (x, yb, BF + .007))
    for k, dy in enumerate((-.22, .22)):
        box('BoxRailFloor%s%d' % (n, k), (x, yb + dy, BF + .0085), (1.20, .046, .010), M_ALU, .002, 1); [cyl('BoxRailFloor%s%d_H%d' % (n, k, j), (x - .375 + .125 * j, yb + dy, BF + .0135), .010, .003, 'z', M_DARK, 16) for j in range(7)]
        for e in (-1, 1):
            nm = 'Strap_%s_%d%d' % (n, k, e > 0); sx = x + e * .50; box(nm + '_Foot', (sx, yb + dy, BF + .026), (.07, .040, .026), M_STEEL, .004); box(nm + '_Reel', (sx, yb + dy, BF + .062), (.08, .055, .052), M_PLAST, .012); box(nm + '_Web', (sx - e * .07, yb + dy, BF + .050), (.075, .030, .007), M_STRAP, .001, 1); tube(nm + '_Hook', [(sx - e * .10, yb + dy, BF + .052), (sx - e * .13, yb + dy, BF + .054), (sx - e * .145, yb + dy, BF + .040)], .006, M_STEEL, 6); ik(nm, (sx - e * .145, yb + dy, BF + .05)); piv(nm, (sx - e * .04, yb + dy, BF + .05))
    box('BoxMark%s' % n, (x, s * .30, BF + .0085), (1.20, .05, .006), M_NOSE, .001, 1); xb = -3.66 if x < -2.5 else -2.155
    box('BeltBay%s_Reel' % n, (xb, s * 1.118, 1.76), (.10, .070, .16), M_PLAST, .012); box('BeltBay%s_Top' % n, (xb, s * 1.1385, 3.13), (.05, .030, .09), M_PLAST, .008); box('BeltBay%s_Web' % n, (xb, s * 1.119, 2.45), (.046, .005, 1.28), M_BELT, .001, 1); box('BeltBay%s_Tongue' % n, (xb, s * 1.114, 2.20), (.05, .012, .07), M_STEEL, .004); ik('BeltTongue_Bay' + n, (xb, s * 1.105, 2.20)); ik('Attend_' + n, (x + .30, s * .10, BF + .007))
# stretcher along the right wall, on its own trolley
sx0, sx1, sy, sz = .02, 1.98, -.80, BF + .62
for e in (-1, 1):
    en = e > 0; tube('Stretcher_Rail%d' % en, [(sx0, sy + e * .255, sz), (sx1, sy + e * .255, sz)], .018, M_ALU, 8, False); tube('Stretcher_Side%d' % en, [(.55, sy + e * .293, sz + .005), (.55, sy + e * .293, sz + .27), (1.45, sy + e * .293, sz + .27), (1.45, sy + e * .293, sz + .005)], .011, M_STEEL, 8); [box('Stretcher_SideFoot%d%d' % (en, k), (x, sy + e * .278, sz), (.03, .045, .03), M_PLAST, .004) for k, x in enumerate((.55, 1.45))]
    for a, b in ((sx0 + .25, sx1 - .25), (sx1 - .25, sx0 + .25)): tube('Stretcher_Leg%d%d' % (en, a < b), [(a, sy + e * (.225 if a < b else .192), sz), (b, sy + e * (.225 if a < b else .192), BF + .17)], .014, M_ALU, 8, False)
    for k, x in enumerate((sx0 + .25, sx1 - .25)): lathe('Stretcher_Castor%d%d' % (en, k), [(0, -.018), (.060, -.018), (.068, -.010), (.068, .010), (.060, .018), (0, .018)], (x, sy + e * .21, BF + .075), M_TYRE, 40); box('Stretcher_Fork%d%d' % (en, k), (x, sy + e * .21, BF + .165), (.05, .09, .03), M_PLAST, .008); [box('Stretcher_ForkSide%d%d%d' % (en, k, j > 0), (x, sy + e * .21 + j * .030, BF + .115), (.028, .010, .09), M_STEEL, .003, 1) for j in (-1, 1)]
for k, x in enumerate((sx0 + .02, .66, 1.32, sx1 - .02)): tube('Stretcher_Cross%d' % k, [(x, sy - .255, sz), (x, sy + .255, sz)], .015, M_ALU, 8, False)
box('Stretcher_Mattress', ((sx0 + sx1) / 2 - .19, sy, sz + .065), (1.52, .47, .09), M_PAD, .035, 4); box('Stretcher_Head', (sx1 - .19, sy, sz + .065), (.36, .47, .09), M_PAD, .035, 4); box('Stretcher_Pillow', (sx1 - .19, sy, sz + .140), (.26, .40, .08), M_NAME, .035, 4); box('Stretcher_BasePlate', ((sx0 + sx1) / 2, sy, sz + .012), (1.86, .46, .014), M_ALU, .003, 1)
for k, x in enumerate((.45, .95, 1.40)):
    pr('Stretcher_Strap%d' % k, [(-.246, sz + .02), (-.239, sz + .02), (-.237, sz + .108), (.237, sz + .108), (.239, sz + .02), (.246, sz + .02), (.244, sz + .116), (-.244, sz + .116)], 'x', x - .025, x + .025, M_STRAP).data.transform(Matrix.Translation((0, sy, 0))); box('Stretcher_Buckle%d' % k, (x, sy, sz + .120), (.06, .05, .016), M_PLAST, .004); ik('Stretcher_Buckle%d' % k, (x, sy, sz + .13))
ik('Stretcher_Lie', (.95, sy, sz + .11)); ik('Stretcher_Head', (sx1 - .19, sy, sz + .18)); ik('Stretcher_Attend', (1.0, -.25, BF + .007)); ik('Stretcher_Hand_F', (sx1, sy + .25, sz)); ik('Stretcher_Hand_R', (sx0, sy + .25, sz)); piv('Stretcher', ((sx0 + sx1) / 2, sy, BF + .007))
box('BoxHeater', (-.62, -1.075, BF + .30), (.56, .14, .46), M_PLAST, .02, 3); [box('BoxHeater_S%d' % k, (-.62, -1.002, BF + .16 + .035 * k), (.48, .012, .014), M_DARK, .002, 1) for k in range(9)]
cyl('BoxExt', (2.30, -1.02, BF + .26), .075, .46, 'z', M_RED, 48, .03); cyl('BoxExt_Neck', (2.30, -1.02, BF + .52), .022, .08, 'z', M_BRASS, 28); box('BoxExt_Lever', (2.30, -.98, BF + .565), (.03, .13, .02), M_PLAST, .005); box('BoxExt_Clamp', (2.30, -1.12, BF + .40), (.18, .07, .05), M_DARK, .006)
box('BoxAid', (-.62, -1.0975, 1.83), (.30, .105, .20), M_GRN, .012, 3); box('BoxAid_C0', (-.62, -1.045, 1.83), (.10, .006, .03), M_SWHT, .0005, 1); [box('BoxAid_C%d' % (2 + e), (-.62, -1.045, 1.83 + e * .0325), (.03, .006, .034), M_SWHT, .0005, 1) for e in (-1, 1)]
for s, sn, f in SIDES: box('BoxHammer_' + sn, (.92, s * 1.140, 2.22), (.10, .024, .05), M_PLAST, .004); box('BoxHammer_%sHead' % sn, (.92, s * 1.122, 2.22), (.07, .016, .022), M_RED, .004); box('BoxExitWord_%s_Base' % sn, (.92, s * 1.1465, 3.13), (.30, .012, .07), M_GRN, .002, 1); text('BoxExitWord_' + sn, 'EMERGENCY EXIT', (.92, s * 1.1405, 3.13), .028, 'y-' if s > 0 else 'y+', M_SWHT)
box('BoxPanel', (2.345, 1.10, 2.00), (.22, .10, .60), M_FRM, .010, 3); pf = 1.05; px = 2.345
for i, (nm, x, m) in enumerate((('Lift', px + .06, M_YEL), ('Plat', px, M_BLU), ('Bridge', px - .06, M_GRN))): box('BoxPanel_Slot' + nm, (x, pf - .001, 1.98), (.020, .006, .13), M_PLAST, .001, 1); tube('Lever_%s_Arm' % nm, [(x, pf + .01, 1.98), (x, pf - .060, 1.98)], .007, M_STEEL, 6, False); cyl('Lever_%s_Knob' % nm, (x, pf - .068, 1.98), .016, .030, 'y', m, 28, .005); piv('Lever_' + nm, (x, pf, 1.98)); ik('Lever_' + nm, (x, pf - .075, 1.98)); text('BoxPanel_Word' + nm, {'Lift': 'LIFT', 'Plat': 'PLATF', 'Bridge': 'BRIDGE'}[nm], (x, pf, 2.075), .011, 'y-', M_SWHT, .0012)
sc_ = box('BoxPanel_Screen', (px, pf - .002, 2.17), (.15, .008, .10), M_DISP, .001, 1); CUV.append((sc_, Vector((px, pf, 2.17)), .15, .10, 0, .5, (1, 0, 0))); ik('BoxPanel_Screen', (px, pf - .008, 2.17)); cyl('BoxPanel_EStopBase', (px + .05, pf - .004, 1.80), .028, .012, 'y', M_YEL, 32, .003); cyl('Button_EStop', (px + .05, pf - .020, 1.80), .021, .030, 'y', M_SRED, 32, .006); ik('Button_EStop', (px + .05, pf - .038, 1.80)); cyl('Button_Light', (px - .04, pf - .006, 1.80), .014, .018, 'y', M_GRN, 28, .003); ik('Button_Light', (px - .04, pf - .018, 1.80))
for n, l in (('BoxPanel_Stand', (2.25, .35, 0)), ('Attend_Front', (2.1, -.30, 0)), ('Box_RearFloor', (-3.4, 0, 0)), ('Box_FrontFloor', (2.0, 0, 0)), ('Box_Aisle', (-.7, 0, 0))): ik(n, (l[0], l[1], BF + .007))
# #endregion
# #region ---- roof: blue bars, beacon, work lamps
def ledbar(n, x):
    z = BT + .052; box(n + '_Base', (x, 0, BT + .012), (.22, 1.30, .032), M_PLAST, .010); box(n + '_Mid', (x, 0, z), (.05, 1.24, .060), M_PLAST, .006)
    for k in range(8):
        for e in (1, -1): unit('%s_%s%d' % (n, 'FR'[e < 0], k), (x + e * .030, -.56 + .16 * k, z), .026, M_BLED, e)
    sh = rnd([(x - .118, BT + .024), (x + .118, BT + .024), (x + .100, BT + .105), (x - .100, BT + .105)], [.005, .005, .03, .03], 5); o = rawprism(n + '_Cover_Glass', sh, 'y', -.66, .66); boole(o, rawprism('c', grow(sh, -.004), 'y', -.655, .655), 'DIFFERENCE', False); K.finish(o, M_BCOV, 30)
    for e in (-1, 1): box('%s_End%d' % (n, e > 0), (x, e * .670, BT + .060), (.215, .024, .10), M_PLAST, .010, 3)
    ik(n, (x, 0, z))
ledbar('BlueFront', 2.36); ledbar('BlueRear', -3.80); beacon('BeaconLens', (-.72, 0, BT + .004)); ik('Beacon', (-.72, 0, BT + .10))
def wlamp(n, c, axis=-1): x, y, z = c; box(n + '_Case', (x, y, z), (.09, .15, .11), M_PLAST, .015, 3); unit(n, (x + axis * .044, y, z), .042, M_WHT, axis); revx(n + '_Cover_Glass', [(0, axis * .059), (.0475, axis * .057), (.0475, axis * .051), (0, axis * .053)], (x + axis * .044, y, z), M_GLASS)
for s, sn, f in SIDES:
    for e, x, d in (('F', BX1 + .044, 1), ('R', BX0 - .044, -1)): wlamp('BoxWork%s%s' % (e, sn), (x, s * .84, 3.18), d); box('BoxCorner%s%s_Base' % (e, sn), (BX1 + .008 if d > 0 else BX0 - .008, s * 1.04, 3.18), (.020, .05, .09), M_PLAST, .004); box('BoxCorner%s%s_Lens' % (e, sn), (BX1 + .022 if d > 0 else BX0 - .022, s * 1.04, 3.18), (.014, .036, .07), M_WHT if d > 0 else M_RLENS, .004)
    x0 = S.surf(.40, 1.40).x + .012; n = 'BlueGrille_' + sn; zc = 1.40 - DZ; box(n + '_Case', (x0 - .004, s * .40, zc), (.040, .19, .085), M_PLAST, .008, 3)
    for k in range(2): unit('%s%d' % (n, k), (x0 + .022, s * (.36 + .08 * k), zc), .034, M_BLED, 1)
    q_ = [(s * .40 - .0935, zc - .041), (s * .40 + .0935, zc - .041), (s * .40 + .0935, zc + .041), (s * .40 - .0935, zc + .041)]; pr(n + '_Cover_Glass', grow(q_, -.002), 'x', x0 + .079, x0 + .083, M_BCOV); ring(n + '_Rim', q_, 'x', x0 + .008, x0 + .088, .003, M_PLAST)
# #endregion
# #region ---- front platform: modelled open, then folded up against the front wall
HXF, HZF, PL, PW, DT = 2.82, BF - .03, 2.0, .65, BF
box('BoxSillFront', ((BX1 + HXF - .03) / 2 - .002, 0, BF - .014), (HXF - .03 - BX1 + .006, 1.20, .026), M_TREAD, .003, 1); [box('BoxSillArm%d' % k, ((BX1 + HXF) / 2 - .01, s * .50, BF - .052), (HXF - BX1 + .02, .05, .055), M_CHAS, .005) for k, s in enumerate((-1, 1))]; [cyl('BoxHingePin%d' % k, (HXF, s * .46, HZF), .016, .18, 'y', M_STEEL, 28) for k, s in enumerate((-1, 1))]; _p0 = len(ALL)
box('Plat_Deck', (HXF + PL / 2 + .015, 0, DT - .0125), (PL - .03, 2 * PW, .025), M_TREAD, .003, 1); [box('Plat_Beam%d' % k, (HXF + PL / 2 + .02, s * .56, DT - .055), (PL - .06, .06, .065), M_YEL, .006, 1) for k, s in enumerate((-1, 1))]; [box('Plat_Rib%d' % k, (HXF + x, 0, DT - .046), (.05, 1.07, .045), M_FRM, .004, 1) for k, x in enumerate((.12, .70, 1.30, 1.94))]
[box('Plat_Ear%d' % k, (HXF + .05, s * .42, HZF - .005), (.13, .05, .040), M_YEL, .006) for k, s in enumerate((-1, 1))]; [box('Plat_RamEar%d' % k, (HXF + .10, s * .615, HZF - .075), (.07, .060, .09), M_YEL, .008) for k, s in enumerate((-1, 1))]
box('Plat_Nose', (HXF + PL + .012, 0, DT - .060), (.03, 1.28, .07), M_YEL, .006); [cyl('Plat_Bump%d' % k, (HXF + PL + .066, -.49 + .245 * k, DT - .062), .042, .235, 'y', M_SEAL, 36, .012) for k in range(5)]
box('Bridge_Plate', (HXF + PL - .40, 0, DT + .0075), (.80, 1.06, .010), M_TREAD, .003, 1); [box('Bridge_Edge%d' % (e > 0), (HXF + PL - .40, e * .536, DT + .0115), (.796, .016, .018), M_NOSE, .003, 1) for e in (-1, 1)]; box('Bridge_Lip', (HXF + PL - .008, 0, DT + .0125), (.026, 1.03, .014), M_SEAL, .004); [box('Bridge_Grip%d' % (e > 0), (HXF + PL - .70, e * .30, DT + .0155), (.03, .14, .012), M_YEL, .003, 1) for e in (-1, 1)]; [box('Plat_Guide%d' % (e > 0), (HXF + PL - .42, e * .560, DT + .010), (.78, .022, .022), M_ALU, .003, 1) for e in (-1, 1)]
R_ = {}
for s, sn, f in SIDES:
    r0 = len(ALL); y = s * .612; zb = DT + (.044 if s < 0 else .128); x0, x1 = HXF + .12, HXF + PL - .06; H = 1.03; gy = y + s * .0375
    for k, z in enumerate((zb + H, zb + .55)): tube('RailPlat_%s_B%d' % (sn, k), [(x0, y, z), (x1, y, z)], .0165, M_YEL, 8, False)
    for k, x in enumerate((x0, (x0 + x1) / 2 - .10, x1)): tube('RailPlat_%s_P%d' % (sn, k), [(x, y, zb), (x, y, zb + H)], .0165, M_YEL, 8, False); cyl('RailPlat_%s_H%d' % (sn, k), (x, y, zb), .019, .06, 'x', M_STEEL, 24, .002); box('Plat_Socket%s%d' % (sn, k), (x, y, (DT - .004 + zb) / 2), (.05, .030, zb - DT + .004), M_FRM, .003, 1)
    box('RailPlat_%s_Toe' % sn, ((x0 + x1) / 2, y, zb + .10), (x1 - x0 - .02, .006, .12), M_ALU, .001, 1)
    for k, z in enumerate((zb + H - .05, zb + .60, zb + .22)): tube('GateSlide_%s_B%d' % (sn, k), [(x1 - .56, gy, z), (x1 + .02, gy, z)], .0145, M_YEL, 8, False)
    for k, x in enumerate((x1 - .56, x1 + .02)): tube('GateSlide_%s_P%d' % (sn, k), [(x, gy, zb + .22), (x, gy, zb + H - .05)], .0145, M_YEL, 8, False)
    box('GateSlide_%s_Pad' % sn, (x1 + .047, gy, zb + .60), (.030, .030, .74), M_SEAL, .008); cyl('GateSlide_%s_Knob' % sn, (x1 - .48, gy, zb + H - .030), .018, .03, 'z', M_RED, 24, .005)
    for n, l in (('Handle_Gate_', (x1 - .48, gy, zb + H)), ('Stand_Gate_', (x1 - .55, s * .25, DT + .012)), ('Rail_Hand_', ((x0 + x1) / 2, y, zb + H)), ('Stand_Rail_', ((x0 + x1) / 2, s * .10, DT))): ik(n + sn, l)
    piv('RailPlat_' + sn, (HXF + 1.0, y, zb)); R_[sn] = (y, zb)
for n, l in (('Plat_Nose', (HXF + PL + .108, 0, DT)), ('Plat_Mid', (HXF + 1.0, 0, DT)), ('Plat_Agent', (HXF + 1.1, .40, DT)), ('Bridge_Tip', (HXF + PL + .005, 0, DT + .012)), ('Handle_Bridge', (HXF + PL - .70, 0, DT + .025)), ('Stand_Bridge', (HXF + PL - 1.1, 0, DT)), ('Plat_Door', (BX1 - .1, 0, DT))): ik(n, l)
RAILM = {sn: Matrix.Translation((0, y, z)) @ Matrix.Rotation(math.radians(90 if sn == 'L' else -90), 4, 'X') @ Matrix.Translation((0, -y, -z)) for sn, (y, z) in R_.items()}
FOLD = Matrix.Translation((HXF, 0, HZF)) @ Matrix.Rotation(-math.pi / 2, 4, 'Y') @ Matrix.Translation((-HXF, 0, -HZF)); PLN = ('Plat_', 'Bridge_', 'RailPlat', 'GateSlide')
for o in ALL[_p0:]:
    for sn in 'LR':
        if o.name.startswith(('RailPlat_' + sn, 'GateSlide_' + sn)): o.data.transform(RAILM[sn])
    o.data.transform(FOLD); o.data.update()
piv('Plat', (HXF, 0, HZF)); BASE, EYEO = Vector((2.44, 0, 1.02)), Vector((HXF + .10, 0, HZF - .10)); EYEF = FOLD @ EYEO
def ram(n, A, B, y, r, r2, kb):
    u = (B - A).normalized(); L = (B - A).length; Y = Vector((0, y, 0)); tube(n + 'Barrel_Tube', [tuple(A + Y + u * .02), tuple(A + Y + u * L * kb)], r, M_YEL, 8, False); cyl(n + 'Barrel_Eye', tuple(A + Y), r * .95, .05, 'y', M_DARK, 28, .003); tube(n + 'Rod_Bar', [tuple(A + Y + u * L * .22), tuple(B + Y - u * .02)], r2, M_CHROME, 8, False); cyl(n + 'Rod_Eye', tuple(B + Y), r * .9, .05, 'y', M_DARK, 28, .003)
for s, sn, f in SIDES: ram('PlatRam%s_' % sn, BASE, EYEF, s * .72, .030, .018, .66); cyl('Plat_RamPin' + sn, (EYEF.x, s * .69, EYEF.z), .014, .13, 'y', M_STEEL, 20); box('BoxRamEar' + sn, (2.44, s * .766, 1.10), (.09, .024, .21), M_CHAS, .006); cyl('BoxRamPin' + sn, (2.44, s * .735, 1.02), .014, .09, 'y', M_STEEL, 20); piv('PlatRam_' + sn, (2.44, s * .72, 1.02)); piv('PlatRamEye_' + sn, (EYEF.x, s * .72, EYEF.z))
# #endregion
# #region ---- tail lift: arms from the body, platform modelled level at the floor then stood up
AX, AZ, TL = -3.60, .66, .90; TA = math.asin((BF - .03 - AZ) / TL); HX, HZ = AX - TL * math.cos(TA), BF - .03; TPL, TPW = 1.42, .72
box('BoxTailSill', (BX0 - .12, 0, BF - .013), (.25, 1.08, .024), M_TREAD, .003, 1); [box('BoxTailSillArm%d' % k, (BX0 - .085, s * .30, BF - .049), (.22, .05, .05), M_CHAS, .004) for k, s in enumerate((-1, 1))]
for s, sn, f in SIDES:
    box('BoxTailHanger' + sn, (AX + .02, s * .675, .925), (.16, .04, .58), M_CHAS, .010); q = []; a, b = Vector((AX, 0, AZ)), Vector((HX, 0, HZ)); u = (b - a).normalized(); nn = Vector((-u.z, 0, u.x))
    for k in range(9): th = math.pi / 2 - math.pi * k / 8; q.append(a + (nn * math.sin(th) - u * math.cos(th)) * .050)
    for k in range(9): th = -math.pi / 2 + math.pi * k / 8; q.append(b + (nn * math.sin(th) + u * math.cos(th)) * .038)
    prism('TailArms_' + sn, [(p.x, p.z) for p in q], s * .585, s * .635, M_FRM, .006, 2); cyl('TailArms_Pin' + sn, (AX, s * .63, AZ), .028, .12, 'y', M_STEEL, 24); cyl('TailArms_Tip' + sn, (HX, s * .63, HZ), .020, .12, 'y', M_STEEL, 24)
cyl('TailArms_Tie', (AX - .12, 0, AZ + .12 * math.tan(TA)), .026, 1.20, 'y', M_FRM, 28); _t0 = len(ALL)
box('TailPlatform_Deck', (HX - TPL / 2 - .02, 0, BF - .013), (TPL - .04, 2 * TPW, .026), M_TREAD, .004, 1); [box('TailPlatform_Ear%d' % k, (HX - .030, s * .672, HZ - .005), (.12, .030, .06), M_FRM, .006) for k, s in enumerate((-1, 1))]; [box('TailPlatform_Beam%d' % k, (HX - TPL / 2 - .05, s * .672, BF - .040), (TPL - .10, .05, .034), M_YEL, .004, 1) for k, s in enumerate((-1, 1))]; [box('TailPlatform_Rib%d' % k, (HX - x, 0, BF - .038), (.05, 1.30, .030), M_FRM, .004, 1) for k, x in enumerate((.10, .50, .92, 1.34))]
pr('TailFlap_Plate', [(HX - TPL - .012, BF - .014), (HX - TPL - .012, BF - .002), (HX - TPL - .31, BF - .016), (HX - TPL - .33, BF - .022), (HX - TPL - .31, BF - .024)], 'y', -.66, .66, M_TREAD); cyl('TailFlap_Hinge', (HX - TPL - .006, 0, BF - .012), .011, 1.30, 'y', M_STEEL, 24); box('TailFlap_Edge', (HX - TPL - .05, 0, BF - .004), (.05, 1.28, .006), M_NOSE, .001, 1)
T_ = {}
for s, sn, f in SIDES:
    y = s * .685; zb = BF + (.030 if s < 0 else .072); x0, x1 = HX - .16, HX - TPL + .06; H = .92
    for k, z in enumerate((zb + H, zb + .50)): tube('TailRail_%s_B%d' % (sn, k), [(x0, y, z), (x1, y, z)], .0165, M_YEL, 8, False)
    for k, x in enumerate((x0, x1)): tube('TailRail_%s_P%d' % (sn, k), [(x, y, zb), (x, y, zb + H)], .0165, M_YEL, 8, False); cyl('TailRail_%s_H%d' % (sn, k), (x, y, zb), .019, .06, 'x', M_STEEL, 24, .002); box('TailPlatform_Socket%s%d' % (sn, k), (x, y, (BF + zb) / 2 - .003), (.05, .030, zb - BF + .006), M_FRM, .003, 1)
    ik('Tail_Rail_%s_Hand' % sn, ((x0 + x1) / 2, y, zb + H)); piv('TailRail_' + sn, (HX - TPL / 2, y, zb)); T_[sn] = (y, zb)
TRM = {sn: Matrix.Translation((0, y, z)) @ Matrix.Rotation(math.radians(90 if sn == 'L' else -90), 4, 'X') @ Matrix.Translation((0, -y, -z)) for sn, (y, z) in T_.items()}
TFOLD = Matrix.Translation((HX, 0, HZ)) @ Matrix.Rotation(math.pi / 2, 4, 'Y') @ Matrix.Translation((-HX, 0, -HZ))
for o in ALL[_t0:]:
    for sn in 'LR':
        if o.name.startswith('TailRail_' + sn): o.data.transform(TRM[sn])
    o.data.transform(TFOLD); o.data.update()
piv('TailArms', (AX, 0, AZ)); piv('TailPlatform', (HX, 0, HZ)); piv('TailFlap', tuple(TFOLD @ Vector((HX - TPL - .006, 0, BF - .012)))); gz = BF + TPL - .24; box('TailPlatform_Grip', (HX - .050, 0, gz), (.020, .18, .03), M_PLAST, .006); [box('TailPlatform_GripFoot%d' % (e > 0), (HX - .034, e * .07, gz), (.040, .022, .024), M_PLAST, .003, 1) for e in (-1, 1)]; ik('Handle_TailPlatform', (HX - .10, 0, gz))
box('TailPlatform_NamePanel_Rear', (HX - .0475, 0, BF + .70), (.012, .90, .22), M_NAME, .003, 1); box('TailPlatform_NameBack', (HX - .022, 0, BF + .70), (.045, .86, .18), M_FRM, .004); ik('Name_Rear', (HX - .054, 0, BF + .70))
box('BoxTailCtl', (-3.82, -1.222, 1.60), (.12, .045, .24), M_PLAST, .010, 3); [(cyl('Button_Tail' + n, (-3.82, -1.250, 1.685 - .055 * i), .016, .018, 'y', m, 28, .003), ik('Button_Tail' + n, (-3.82, -1.262, 1.685 - .055 * i))) for i, (m, n) in enumerate(((M_GRN, 'Up'), (M_DARK, 'Down'), (M_YEL, 'Fold'), (M_SRED, 'Stop')))]
# the controls the player works too (to-do 238): each its own part with its PIV: cabin panels front and rear, outside tail panel, cab switches
box('BoxRearPanel', (-3.80, -1.10, 1.98), (.16, .10, .40), M_FRM, .010, 3)
def pbtn(n, c, ax, m, r=.014, d=.018): cyl('Button_' + n, c, r, d, ax, m, 28, .003); ik('Button_' + n, c); piv('Button_' + n, c)
for i, (n, m) in enumerate((('InTailUp', M_GRN), ('InTailDown', M_DARK), ('InTailFold', M_YEL), ('InFlap', M_BLU), ('InTailStop', M_SRED))): pbtn(n, (-3.80, -1.046, 2.13 - .062 * i), 'y', m)
for i, (n, m) in enumerate((('DoorFront', M_GRN), ('Blue', M_BLU), ('Heater', M_YEL), ('Work', M_SWHT))): pbtn(n, (px - .075 + .04 * i, pf - .006, 1.735), 'y', m, .012)
for n, c, ax in (('DoorRearIn', (IX0 + .013, -.62, 1.80), 'x'), ('DoorRearOut', (BX0 - .013, -.62, 1.80), 'x'), ('DoorFrontOut', (BX1 + .013, .62, 1.80), 'x')): box('BoxDoorBtn_' + n, (c[0] - .009 if ax == 'x' and n != 'DoorRearOut' else c[0] + .009, c[1], c[2]), (.016, .05, .08), M_PLAST, .004) if n != 'DoorRearIn' else None; n == 'DoorRearIn' or pbtn(n, c, ax, M_GRN)
box('BoxDoorBtn_In', (-3.80, -1.052, 1.73), (.06, .104, .06), M_FRM, .006); pbtn('DoorRearIn', (-3.80, -.995, 1.73), 'y', M_GRN); ik('RearPanel_Stand', (-3.45, -.45, BF + .007))
for n, o in [(o.name, o) for o in ALL if o.name.startswith(('Button_EStop', 'Button_Light', 'Button_TailUp', 'Button_TailDown', 'Button_TailFold', 'Button_TailStop'))]: c = [sum(v.co[i] for v in o.data.vertices) / len(o.data.vertices) for i in range(3)]; piv(n, tuple(c))
for i, n in enumerate(('Hazard', 'Blue', 'Stop', 'Beacon', 'Work')): o = bpy.data.objects['Dash_Btn%d' % i]; o.name = 'Button_Cab' + n; c = [sum(v.co[j] for v in o.data.vertices) / len(o.data.vertices) for j in range(3)]; piv(o.name, tuple(c)); ik(o.name, tuple(c))
o = bpy.data.objects['SteeringWheel_Cap']; o.name = 'Button_Horn'; piv('Button_Horn', tuple(SC)); ik('Horn', tuple(SC + Vector((-.012, 0, .012)))); o = bpy.data.objects['ParkBrake']; o.name = 'Lever_ParkBrake'; piv('ParkBrake', (4.16, -.07, FLOOR + .33))
for n, l in (('Tail_Control_Stand', (-3.95, -1.80, 0)), ('Tail_Stand', (-6.5, 0, 0)), ('Tail_Floor', (HX - .75, 0, BF)), ('Tail_Attend', (HX - .9, .35, BF)), ('Tail_Door', (BX0 - .1, 0, BF))): ik(n, l)
# #endregion
