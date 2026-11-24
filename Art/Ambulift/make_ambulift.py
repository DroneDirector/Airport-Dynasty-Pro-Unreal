# Ambulift (to-do 212): our own lift vehicle for passengers with reduced mobility. Low forward cab, passenger cabin on a scissor, folding front platform with sliding bridge, rear tail lift.
# Axes: x forward, y left, z up; metres; origin near the middle. Driver front left.   blender -b --python Art/Ambulift/make_ambulift.py [-- --pic DIR] [--nochk]
# The cab's parts come from the de-icer's text (Art/Deicer/make_deicer.py), lowered by .60 m, with its own face, lamps and skirt; the scissor from the catering truck's.
import os, math
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
def SRC(*p): return open(os.path.join(ROOT, *p), encoding='utf-8').read()
def rep(t, pairs):
    for a, b in pairs: assert a in t, a; t = t.replace(a, b)
    return t
_di = SRC('Art', 'Deicer', 'make_deicer.py'); _i0 = _di.index('import os, sys, math'); _i1 = _di.index('# #region ---- cab:'); _i2 = _di.index('# #endregion', _i1)
_hd = rep(_di[_i0:_i1], [("'Deicer', 'deicer', 'DI', 'DI_', '', (.86, .47, .02)", "'Ambulift', 'ambulift', 'AL', 'AL_', '', (.02, .22, .50)"), ("MINE = os.path.join(ART, 'Textures')", "MINE = os.path.join(A_, 'Deicer', 'Textures')"), ('= 3.50, -1.55, .48', '= 2.30, -2.20, .48'), ("'DI_", "'AL_"), ('AL_Boom_Paint', 'AL_Spare_Paint')])
exec(compile(_hd, 'deicer head', 'exec'))
M_WALL = P('AL_WallPanel', (.86, .87, .86), 0, .5); K.photo(M_WALL, OWN, 'Plastic018A', tile=.6, tint=(1.75, 1.78, 1.75), metal=0.0, normal=.5); M_SFL = P('AL_SafetyFloor', (.2, .24, .28), 0, .85); K.photo(M_SFL, TEX, 'Rubber001', tile=.30, tint=(1.0, 1.15, 1.35), metal=0.0, normal=1.2)
M_PAD = P('AL_MattressVinyl', (.03, .05, .12), 0, .5); K.photo(M_PAD, TEX, 'Leather026', tile=.4, tint=(.35, .5, .9), metal=0.0); M_STRAP = P('AL_StrapWebbing', (.9, .4, .03), 0, .8); K.photo(M_STRAP, CART, 'Fabric074', tile=.1, tint=(1.0, .42, .05), metal=0.0)
M_BLED = P('AL_BlueLens', (.02, .10, 1.0), 0, .15, 1.0); K.photo(M_BLED, OWN, 'LensPillow', tile=.10, tint=(.03, .12, 1.0), metal=0.0); M_BCOV = P('AL_BlueCover_Glass', (.02, .10, .9), 0, .05); K.glass(M_BCOV, .35)

# #region ---- cab
_cb = rep(_di[_i1:_i2], [('[(.42, 4.84), (.60, 4.92), (.85, 4.94)', '[(.84, 4.88), (.92, 4.925), (1.02, 4.94)'), ('[(.42, CABB)', '[(.84, CABB)'), ('[(.42, 1.15), (.60, 1.18)', '[(.84, 1.15), (.95, 1.18)'), ('[(.42, .22)', '[(.84, .22)'), ('[.42, .48, .55, .62, .70, .78, .86, .94', '[.84, .86, .90, .95'),
    ('(1.72, 4.915), (2.35, 4.80), (2.55, 4.74)', '(1.72, 4.90), (2.35, 4.77), (2.55, 4.70)'), ('1 if c.z > 2.40 else 2 if c.z < 1.02 else 0', '2 if c.z < .95 else 1 if c.z < 1.34 else 0'), ("[(2.0, .2), (4.09, .2), (4.09, 1.062), (2.0, 1.062)], 'y', -2, 2)", "[(2.9, .2), (3.2, .2), (3.2, 1.062), (2.9, 1.062)], 'y', -1.0, 1.0)"),
    ('rnd([(.79, .93), (1.00, .93), (1.00, 1.375), (.79, 1.375)], .06)', 'rnd([(.555, 1.165), (.99, 1.165), (.99, 1.335), (.555, 1.335)], .06)'), ("for nm, z, mt, r in (('LowBeam', HLZ[0], M_WHT, .064), ('HighBeam', HLZ[1], M_WHT, .064), ('IndicatorF', HLZ[2], M_AMB, .048)):", "for nm, HY, mt, r in (('LowBeam', .905, M_WHT, .064), ('HighBeam', .76, M_WHT, .064), ('IndicatorF', .632, M_AMB, .048)):\n        z = 1.25"),
    ('rnd([(-.66, 1.00), (.66, 1.00), (.70, 1.34), (-.70, 1.34)], .05)', 'rnd([(-.46, 1.165), (.46, 1.165), (.46, 1.335), (-.46, 1.335)], .05)'), ('[(-.26, .80), (.26, .80), (.26, .91), (-.26, .91)]', '[(-.26, 1.035), (.26, 1.035), (.26, 1.135), (-.26, 1.135)]'), ("'FrontBumper', (4.93, 0, .60), (.14, 1.70, .20)", "'FrontBumper', (4.93, 0, .93), (.14, 1.70, .14)"), ("(5.0005, 0, .60)", "(5.0005, 0, .93)"), ("(4.95, 0, .49), (.10, .05, .05)", "(4.95, 0, .842), (.10, .05, .05)"),
    ('((.45, .92), (.78, .88))', '((.90, .88),)'), ("(4.36, s * 1.06, .80)", "(4.36, s * 1.06, .92)"), ("(4.36, s * 1.08, .47)", "(4.36, s * 1.08, .92)"), ("(4.365, s * .848, .74), (.53, .016, .64)", "(4.365, s * .848, .952), (.53, .016, .218)"), ("(x, s * 1.00, .74), (.016, .30, .64)", "(x, s * 1.00, .952), (.016, .30, .218)")])
_cb = rep(_cb, [("[(4.0, 1.5), (4.70, 1.5), (4.70, 1.9), (4.0, 1.9)], 'y', s * DIN,", "[(4.0, 1.5), (4.707, 1.5), (4.707, 1.9), (4.0, 1.9)], 'y', s * (DIN - .005),"), ('[(4.0, .40), (4.64, .40), (4.64, 1.062), (4.0, 1.062)]', '[(4.093, .40), (4.64, .40), (4.64, 1.062), (4.093, 1.062)]')]); _cb = chr(10).join(l for l in _cb.split(chr(10)) if not l.startswith(("for i in range(9): patch('Grille_Slat", "box('Beacon_Mount'")))
_cb = _cb.replace("        z = 1.25 x0", "        z = 1.25; x0")
DZ = .60; _n0, _e0 = len(ALL), len(IKS); exec(compile(_cb, 'deicer cab', 'exec'))
for i in range(4): patch('Grille_Slat%d' % i, [(-.42, 1.185 + .036 * i), (.42, 1.185 + .036 * i), (.42, 1.199 + .036 * i), (-.42, 1.199 + .036 * i)], *XB, M_DARK, -.014, .002)
for o in ALL[_n0:]: assert o.matrix_world == Matrix.Identity(4), o.name; o.data.transform(Matrix.Translation((0, 0, -DZ))); o.data.update()
for n_, v_ in (('Cowl', (0, 0, -.007)), ('FootRest_B', (0, 0, .007))): bpy.data.objects[n_].data.transform(Matrix.Translation(v_))
for e in IKS[_e0:]: e.location.z -= DZ
for c in CUV: c[1].z -= DZ
SC = Vector((SC.x, SC.y, SC.z - DZ)); FLOOR -= DZ; ik('Aisle_Row0', (3.96, 0, FLOOR))
# #endregion

# #region ---- chassis: frame, axles, batteries, legs, lamps
FE, FYW = -3.45, 1.0
for s, sn, f in SIDES:
    for k, x in (('F', FX), ('R', DX)):
        wheel(k + sn, x, s * FYW, s, .075); piv('Wheel_' + k + sn, (x, s * FYW, RW)); fender('Fender%s_%s' % (k, sn), x, s * .83, s * 1.165, .548, *((58, 122) if k == 'F' else (40, 140))); box('FenderBar%s_%s' % (k, sn), (x, s * .622, .80), (.06, .43, .03), M_DARK, .004, 1); box('FenderPost%s_%s' % (k, sn), (x, s * .8345, .91), (.06, .013, .24), M_DARK, .002, 1)
        box('Leaf%s_%s' % (k, sn), (x, s * .42, RW + .035), (1.0, .07, .05), M_DARK, .010); [box('Hanger%s_%s%d' % (k, sn, e > 0), (x + e * .94, s * .42, .60), (.07, .075, .21), M_CHAS, .006) for e in (-.5, .5)]
        xf = x - .62; box('MudFlap%s_%s' % (k, sn), (xf, s * .99, .39), (.012, .33, .40), M_SEAL, .003, 1); box('MudFlapBar%s_%s' % (k, sn), (xf, s * .78, .575), (.03, .75, .03), M_DARK, .004); box('MudFlapPost%s_%s' % (k, sn), (xf, s * .39, .61), (.05, .05, .10), M_DARK, .004)
    piv('Steer_F' + sn, (FX, s * .80, RW)); box('Spindle_F' + sn, (FX, s * .80, RW), (.12, .08, .22), M_DARK, .012); cyl('Spindle_F%s_Axle' % sn, (FX, s * .89, RW), .05, .18, 'y', M_STEEL, 28); cyl('SpindleDrum_F' + sn, (FX, s * .93, RW), .17, .10, 'y', M_DARK, 48, .01); box('SteerArm_F' + sn, (FX - .10, s * .77, RW - .05), (.18, .04, .03), M_DARK, .008)
    cyl('AxleBeamF_' + sn, (FX, s * .375, RW - .06), .05, .75, 'y', M_DARK, 28); box('AxleEndF_' + sn, (FX, s * .735, RW - .01), (.10, .05, .18), M_DARK, .010); cyl('DriveAxle_' + sn, (DX, s * .55, RW), .065, .70, 'y', M_DARK, 32); cyl('DriveDrum_' + sn, (DX, s * .88, RW), .17, .08, 'y', M_DARK, 40, .01)
    box('Frame_' + sn, ((2.98 + FE) / 2, s * .37, .74), (2.98 - FE, .08, .20), M_CHAS, .008, 1); box('FrameDrop_' + sn, (2.955, s * .37, .60), (.05, .10, .52), M_CHAS, .006, 1); box('SubFrame_' + sn, (3.865, s * .37, .40), (1.80, .08, .12), M_CHAS, .006, 1); [box('SubFrame_%sPad%d' % (sn, i), (x, s * .37, .462), (.12, .10, .012), M_SEAL, .002, 1) for i, x in enumerate((3.4, 4.4))]
    box('BatteryBox_' + sn, (.05, s * .86, .565), (2.40, .50, .47), M_FRM, .03, 3); [box('BatteryBoxArm%d_%s' % (i, sn), (x, s * .52, .68), (.10, .24, .10), M_CHAS, .006) for i, x in enumerate((-.95, .05, 1.05))]
    for i, x in enumerate((-.74, .05, .84)):
        box('BatteryBoxLid%d_%s' % (i, sn), (x, s * 1.112, .565), (.74, .012, .39), M_MAIN, .006); [box('BatteryBoxLid%d_%sV%d' % (i, sn, k), (x, s * 1.121, .50 + .025 * k), (.40, .010, .010), M_DARK, .002, 1) for k in range(5)]; box('BatteryBoxLid%d_%sCatch' % (i, sn), (x, s * 1.121, .71), (.09, .010, .03), M_PLAST, .003, 1)
    box('RearLampBox_' + sn, (-3.80, s * .93, .64), (.075, .46, .19), M_PLAST, .014); box('RearLampArm_' + sn, (-3.60, s * .86, .64), (.34, .08, .10), M_CHAS, .006)
    for nm, y, mt in (('TailBrake', .78, M_RLENS), ('IndicatorR', .93, M_AMB), ('Reverse', 1.08, M_WHT)): revx('%s_%s_Ring' % (nm, sn), [(.058, .002), (.058, -.018), (.064, -.018), (.068, .002), (.058, .002)], (-3.837, s * y, .64), M_CHROME); revx('%s_%s_Lens' % (nm, sn), [(0, -.016), (.030, -.014), (.050, -.009), (.0565, -.002), (.0565, .002), (0, .002)], (-3.837, s * y, .64), mt)
    for i, x in enumerate((1.0, -.90)): box('SideMarker_%s%d_Base' % (sn, i), (x, s * 1.117, .350), (.11, .03, .030), M_PLAST, .006); box('SideMarker_%s%d_Lens' % (sn, i), (x, s * 1.135, .350), (.085, .014, .022), M_AMB, .004)
    for k, x in (('F', 1.56), ('R', -2.96)):
        n = k + sn; box('StabArm_' + n, (x, s * .71, .74), (.14, .62, .14), M_CHAS, .010); cyl('StabTube_' + n, (x, s * 1.06, .78), .070, .62, 'z', M_YEL, 36, .006); cyl('StabTubeCap_' + n, (x, s * 1.06, 1.095), .055, .03, 'z', M_DARK, 28, .004); cyl('StabLeg_' + n, (x, s * 1.06, .64), .045, .66, 'z', M_STEEL, 32); cyl('StabLeg_%s_Foot' % n, (x, s * 1.06, .315), .115, .03, 'z', M_DARK, 40, .006); cyl('StabLeg_%s_Ball' % n, (x, s * 1.06, .34), .062, .05, 'z', M_STEEL, 28, .006); piv('StabLeg_' + n, (x, s * 1.06, .30)); box('StabHose_' + n, (x, s * .93, .822), (.03, .16, .03), M_HOSE, .006)
for i, x in enumerate((2.75, 1.60, -.40, -1.50, -3.20)): box('FrameCross%d' % i, (x, 0, .74), (.09, .70, .11), M_CHAS, .006, 1)
for i, x in enumerate((3.1, 3.9, 4.6)): box('SubFrameCross%d' % i, (x, 0, .40), (.08, .70, .09), M_CHAS, .005, 1)
box('RearBar', (-3.41, 0, .64), (.08, 1.80, .10), M_CHAS, .008); box('RearPlate', (-3.4535, 0, .64), (.008, .30, .10), M_NAME, .002, 1); [box('RearHi%d_%s' % (i, sn), (-3.4525, s * (.28 + .22 * i), .64), (.006, .20, .06), M_HVR if i % 2 else M_HVW, .001, 1) for s, sn, f in SIDES for i in range(2)]
cyl('DriveAxle_Mid', (DX, 0, RW), .065, .56, 'y', M_DARK, 32); cyl('Motor', (DX, 0, RW), .20, .42, 'y', M_ALU, 48, .03); box('HydUnit', (2.26, 0, .72), (.60, .67, .22), M_FRM, .03, 3); cyl('HydTank', (-2.65, 0, .74), .13, .67, 'y', M_ALU, 40, .02)
# legs' panel (left, behind the front leg)
box('StabPanel', (1.36, 1.135, .60), (.10, .06, .34), M_PLAST, .008); box('StabPanelFoot', (1.30, 1.06, .60), (.14, .12, .10), M_CHAS, .006); tube('Lever_Stab_Arm', [(1.36, 1.15, .66), (1.36, 1.225, .66)], .009, M_STEEL, 6, False); cyl('Lever_Stab_Knob', (1.36, 1.232, .66), .020, .035, 'y', M_BLU, 28, .006); piv('Lever_Stab', (1.36, 1.165, .66)); ik('Lever_Stab', (1.36, 1.235, .66)); piv('Button_StabEStop', (1.36, 1.175, .50)); cyl('Button_StabEStop', (1.36, 1.175, .50), .022, .030, 'y', M_SRED, 32, .004); ik('Button_StabEStop', (1.36, 1.195, .50)); ik('Stand_Stab', (1.36, 1.80, 0)); text('StabPanel_Word', 'LEGS', (1.36, 1.165, .745), .026, 'y+', M_SWHT, .0012)
# #endregion

# #region ---- scissor (one X a side): lift q -> t = asin((.12 + q) / 4.60)
_ca = SRC('Art', 'CateringTruck', 'make_cateringtruck.py'); _s0 = _ca.index('SL, SX, SZ0'); _s1 = _ca.index("    box('ScissorFoot_'", _s0)
exec(compile(rep(_ca[_s0:_s1], [('= 4.40, 1.90, 1.10, .12, 2.75', '= 4.60, 2.35, .97, .12, 2.92'), ('math.sin(T0) - .012', 'math.sin(T0) - .045'), ('(EYE - RB).length * .80', '(EYE - RB).length * .70'), ('Vector((.60, 0, .83))', 'Vector((1.05, 0, .70))'), ("(.60, s * .22, .83), (.12, .16, .12)", "(1.05, s * .22, .70), (.12, .16, .12)")]), 'catering scissor', 'exec'))
for s, sn, f in SIDES:
    box('ScissorFoot_' + sn, (SX, s * .383, .93), (.14, .03, .19), M_CHAS, .006); box('ScissorRail_' + sn, (XL + .70, s * .645, .94), (1.70, .025, .18), M_CHAS, .006, 1); cyl('ScissorFootPin_' + sn, (SX, s * .43, SZ0), .026, .12, 'y', M_STEEL, 24); cyl('ScissorFootRoller_' + sn, (XL, s * .63, SZ0), .040, .04, 'y', M_DARK, 28)
    for i, x in enumerate((XL + .20, XL + 1.35)): box('ScissorRailArm%d_%s' % (i, sn), (x, s * .51, .853), (.08, .29, .032), M_CHAS, .004)
    box('BoxTrack_' + sn, (XL + .62, s * .387, 1.12), (1.56, .024, .17), M_CHAS, .004, 1); box('BoxEarFront_' + sn, (SX, s * .645, 1.11), (.14, .025, .19), M_CHAS, .004, 1); cyl('BoxEarPin_' + sn, (SX, s * .615, SZ0 + H0), .026, .06, 'y', M_STEEL, 24)
box('RamCross', (1.05, 0, .70), (.14, .70, .10), M_CHAS, .008); piv('ScissorA', (SX, 0, SZ0)); piv('ScissorB', (SX, 0, SZ0 + H0)); piv('ScissorMid', tuple(m_))
# #endregion
exec(compile(SRC('Art', 'Ambulift', 'ambulift_body.py'), 'ambulift_body', 'exec'))
exec(compile(SRC('Art', 'Ambulift', 'ambulift_checks.py'), 'ambulift_checks', 'exec'))
