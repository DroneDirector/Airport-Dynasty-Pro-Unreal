# The catering high-loader (to-do 212): 2-axle truck whose box rises on a scissor to the A320's service doors. Our own shape, no brand.
# Sizes: the class of narrow-body high-loaders (overall 8.5 - 10 m, body 2.5 m wide and 2.1 m high inside, body floor 1.2 - 1.5 m down and 4 m and more up);
# shape seen in photos (named on docs/Blueprints/Apron/CateringTruck.png). Sills measured on Art/A320/A320.glb: front right 3.87 m, rear right 3.49 m.
# Axes: x forward, y left, z up; metres; built round the chassis, moved at the end (SHIFT) so that the origin is under the middle. Driver front left.
#   blender -b --python Art/CateringTruck/make_cateringtruck.py [-- --pic DIR [--up]]
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, 'Tools'), os.path.join(ROOT, 'Art', 'CleaningKit')]
import vehicle_kit as K
from vehicle_kit import box, cyl, lathe, torus, tube, prism, cut, turn, ALL, IKS
ART = os.path.join(ROOT, 'Art', 'CateringTruck'); TEX = os.path.join(ROOT, 'Art', 'PushbackTug', 'Textures')
K.reset()
import make_cleaningkit as CK
import vehicle_skin as S
from vehicle_skin import rawprism, boole, patch, grow, rnd, ip
CK.join.__globals__['K'] = K
for _o in list(ALL): bpy.data.objects.remove(_o)
ALL.clear(); IKS.clear()
PFX, DASHWORD = 'CT_', 'LIFT BODY   DOWN / UP'
FX, DX, RW, TW = 3.70, -1.20, .507, .30; FLOOR, CABB = 1.18, 3.05; ox, oz = 3.21, FLOOR - .575; SHIFT = -.537
f_ = os.path.join(ROOT, 'Tools', 'truck_cab.py'); exec(compile(open(f_, encoding='utf-8').read(), f_, 'exec'))
M_WALL = P('CT_WallPanel', (.80, .81, .82), .05, .5); M_STRAP = P('CT_StrapWebbing', (.95, .45, .03), 0, .8)
SIDES = ((1, 'L', 'y+'), (-1, 'R', 'y-'))
for o in [o for o in ALL if o.name.startswith('Aerial')]: S.rm(o)                                                   # (the platform slides out over the cab)

# #region ---- wheels, axles, chassis, steps, stabilisers (all stays down)
FYW, DYO, DYI = 1.025, 1.045, .715
for s, sn, f in SIDES:
    wheel('F' + sn, FX, s * FYW, s, .085); wheel('R' + sn, DX, s * DYO, s, -.085); wheel('R' + sn, DX, s * DYI, s, .085, '_In')
    piv('Wheel_F' + sn, (FX, s * FYW, RW)); piv('Wheel_R' + sn, (DX, s * (DYO + DYI) / 2, RW)); piv('Steer_F' + sn, (FX, s * .80, RW))
    box('Spindle_F' + sn, (FX, s * .80, RW), (.13, .08, .24), M_DARK, .014); cyl('Spindle_F%s_Axle' % sn, (FX, s * .90, RW), .05, .20, 'y', M_STEEL, 28); cyl('SpindleDrum_F' + sn, (FX, s * .965, RW), .205, .15, 'y', M_DARK, 48, .01)
    box('SteerArm_F' + sn, (FX - .10, s * .775, RW - .05), (.18, .04, .03), M_DARK, .008); cyl('AxleBeamF_' + sn, (FX, s * .40, RW - .06), .055, .76, 'y', M_DARK, 28); box('AxleEndF_' + sn, (FX, s * .745, RW), (.12, .05, .18), M_DARK, .012)
    box('LeafF_' + sn, (FX, s * .52, RW + .055), (1.10, .08, .06), M_DARK, .012)
    for e in (-.5, .5): box('HangerF_%s%d' % (sn, e > 0), (FX + e * 1.04, s * .49, .69), (.07, .10, .30), M_CHAS, .006)
    cyl('DriveAxle_' + sn, (DX, s * .62, RW), .075, .60, 'y', M_DARK, 32); cyl('DriveDrum_' + sn, (DX, s * .60, RW), .20, .10, 'y', M_DARK, 40, .01)
    for e in (-.42, .42): cyl('AirSpring_%s%d' % (sn, e > 0), (DX + e, s * .47, .655), .10, .30, 'z', M_SEAL, 32, .03); box('AirBeam_%s%d' % (sn, e > 0), (DX + e / 2, s * .47, .455), (.52, .09, .07), M_DARK, .01)
    prism('FenderF_' + sn, [(FX - .62, .74), (FX - .57, 1.01), (FX - .46, 1.095), (FX + .50, 1.095), (FX + .50, 1.107), (FX - .465, 1.107), (FX - .58, 1.02), (FX - .632, .745)], s * .84, s * 1.205, M_BUMP); tube('FenderBarF_' + sn, [(FX - .30, s * .40, 1.02), (FX - .30, s * .50, 1.114), (FX - .30, s * 1.15, 1.114)], .018, M_DARK, bez=False)
    fender('FenderD_' + sn, DX, s * .62, s * 1.215, .56, 8, 172); tube('FenderBarD_' + sn, [(DX - .1, s * .40, .90), (DX - .1, s * .58, .97), (DX - .1, s * .66, 1.060)], .018, M_DARK, bez=False); box('FenderFoot_' + sn, (DX - .1, s * .80, 1.062), (.06, .34, .012), M_DARK, .002, 1)
    box('MudFlap0_' + sn, (FX - .625, s * 1.02, .42), (.012, .34, .46), M_SEAL, .003, 1); box('MudFlapBar0_' + sn, (FX - .625, s * .735, .62), (.03, .64, .03), M_DARK, .004); box('MudFlapPost0_' + sn, (FX - .625, s * .44, .74), (.03, .05, .26), M_DARK, .004)
    box('MudFlap1_' + sn, (DX - .60, s * .93, .42), (.012, .56, .46), M_SEAL, .003, 1); box('MudFlapBar1_' + sn, (DX - .60, s * .70, .62), (.03, .64, .03), M_DARK, .004); box('MudFlapPost1_' + sn, (DX - .60, s * .42, .74), (.03, .05, .26), M_DARK, .004)
    box('Frame_' + sn, (.95, s * .43, .91), (7.40, .08, .24), M_CHAS, .010, 1)                                      # x -2.75 .. 4.65
    for k, x in (('F', 1.78), ('R', -2.32)):                                                                        # stabilisers: the leg goes 30 cm down
        n = k + sn; box('StabArm_' + n, (x, s * .74, .86), (.16, .62, .14), M_CHAS, .010); cyl('StabTube_' + n, (x, s * 1.08, .80), .070, .62, 'z', M_YEL, 36, .006); cyl('StabTubeCap_' + n, (x, s * 1.08, 1.115), .055, .03, 'z', M_DARK, 28, .004)
        cyl('StabLeg_' + n, (x, s * 1.08, .66), .045, .66, 'z', M_STEEL, 32); cyl('StabLeg_%s_Foot' % n, (x, s * 1.08, .315), .115, .03, 'z', M_DARK, 40, .006); cyl('StabLeg_%s_Ball' % n, (x, s * 1.08, .34), .062, .05, 'z', M_STEEL, 28, .006)
        piv('StabLeg_' + n, (x, s * 1.08, .30)); box('StabHose_' + n, (x + .075, s * .95, .91), (.03, .12, .03), M_HOSE, .006)
    box('SideBox_' + sn, (.55, s * .86, .68), (1.15, .56, .46), M_PLAST if s > 0 else M_FRM, .03, 3); box('SideBoxArm_' + sn, (.55, s * .52, .80), (.9, .14, .10), M_CHAS, .006); box('SideBoxLid_' + sn, (.55, s * 1.143, .68), (1.02, .012, .38), M_TRIM, .006); box('SideBoxCatch_' + sn, (.55, s * 1.153, .82), (.14, .016, .035), M_STEEL, .005)
    for i in range(3): box('SideHi%d_%s' % (i, sn), (.22 + .33 * i, s * 1.1505, .53), (.28, .006, .06), M_HVR if i % 2 else M_HVW, .001, 1)
    box('SideMarkerArm_' + sn, (1.29, s * .80, .80), (.06, .72, .04), M_CHAS, .004); box('SideMarker_%s1_Base' % sn, (1.29, s * 1.165, .80), (.14, .05, .07), M_PLAST, .008); box('SideMarker_%s1_Lens' % sn, (1.29, s * 1.193, .80), (.11, .014, .045), M_AMB, .003)
    box('RearArm_' + sn, (-3.06, s * .94, .72), (1.02, .08, .10), M_CHAS, .008); box('RearLampBox_' + sn, (-3.58, s * .94, .74), (.09, .46, .20), M_PLAST, .012)      # the lamp arms reach back beside the tail lift's arms
    for nm, y, mt, w in (('TailBrake', .83, M_RLENS, .12), ('IndicatorR', .985, M_AMB, .12), ('Reverse', 1.11, M_WHT, .08)): box('%s_%s_Lens' % (nm, sn), (-3.6255, s * y, .74), (.008, w, .13), mt, .003)
    box('RearDrop_' + sn, (-2.62, s * .43, .79), (.12, .10, .12), M_CHAS, .006)
box('RearCross', (-2.62, 0, .72), (.14, 1.96, .10), M_CHAS, .008); box('RearPlate', (-2.694, 0, .72), (.012, .52, .11), M_NAME, .002, 1)
cyl('DriveAxle_Mid', (DX, 0, RW), .075, .80, 'y', M_DARK, 32); cyl('Differential', (DX, 0, RW), .21, .30, 'x', M_DARK, 40, .06); cyl('PropShaft', (.90, 0, .60), .05, 4.0, 'x', M_DARK, 28); box('Gearbox', (3.00, 0, .70), (.85, .50, .46), M_DARK, .06, 3); box('Engine', (3.95, 0, .86), (1.15, .74, .50), M_DARK, .06, 3)
for i, x in enumerate((4.50, 2.35, 1.35, -.30, -1.95, -2.62)): box('FrameCross%d' % i, (x, 0, .85), (.10, .80, .10), M_CHAS, .008, 1)
cyl('Exhaust', (4.55, .0, .50), .07, .9, 'y', M_DARK, 32, .02); box('ExhaustHang', (4.55, 0, .66), (.06, .06, .34), M_DARK, .006)
# steps up to the box's side door (stay with the chassis: used only with the box down)
for i, (z, y0) in enumerate(((.40, 1.02), (.70, .98), (1.00, .94))): box('BoxStep_Tread%d' % i, (2.45, (y0 + 1.22) / 2, z), (.50, 1.22 - y0, .035), M_DECK, .006, 2); ik('Step_BoxDoorL_%d' % i, (2.45, 1.11, z + .018))
for e, x in ((0, 2.195), (1, 2.705)): box('BoxStep_Side%d' % e, (x, 1.04, .70), (.016, .32, .70), M_CHAS, .003, 1)
box('BoxStep_Arm', (2.45, .68, .86), (.40, .46, .10), M_CHAS, .008); box('BoxStep_Back', (2.45, .90, .72), (.52, .014, .64), M_CHAS, .003, 1)
# the stabilisers' control box, left behind the cab (lever down = legs down)
box('StabPanel', (1.78, 1.20, .95), (.24, .07, .30), M_FRM, .010); box('StabPanel_Slot', (1.75, 1.2365, .97), (.03, .006, .18), M_PLAST, .002, 1)
tube('Lever_Stab', [(1.75, 1.215, .97), (1.75, 1.295, .97)], .009, M_STEEL, bez=False); cyl('Lever_Stab_Knob', (1.75, 1.302, .97), .020, .035, 'y', M_RED, 24, .006); piv('Lever_Stab', (1.75, 1.225, .97)); ik('Lever_Stab', (1.75, 1.305, .97)); ik('Stand_Stab', (1.72, 1.80, 0)); cyl('StabPanel_EStop', (1.84, 1.241, 1.04), .022, .022, 'y', M_RED, 28, .004); ik('Button_StabEStop', (1.84, 1.256, 1.04))
text('StabPanel_Word', 'LEGS', (1.79, 1.2355, .85), .035, 'y+', M_SWHT, .0012)
# #endregion

# #region ---- scissor: L 4.40, fixed ends in front, sliding ends at the back; lift q -> t = asin((.12 + q) / 4.40)
SL, SX, SZ0, H0, LIFT = 4.40, 1.90, 1.10, .12, 2.75; T0 = math.asin(H0 / SL); T1 = math.asin((H0 + LIFT) / SL); XL = SX - SL * math.cos(T0)
for nm, y, za, zb in (('ScissorA', .455, SZ0, SZ0 + H0), ('ScissorB', .575, SZ0 + H0, SZ0)):                       # A: low in front, high at the back
    a, b = Vector((SX, 0, za)), Vector((XL, 0, zb)); u = (b - a).normalized(); n = Vector((-u.z, 0, u.x)); q = []
    for k in range(9): th = math.pi / 2 - math.pi * k / 8; q.append(a + (n * math.sin(th) - u * math.cos(th)) * .055)
    for k in range(9): th = -math.pi / 2 + math.pi * k / 8; q.append(b + (n * math.sin(th) + u * math.cos(th)) * .055)
    for s in (-1, 1):
        prism('%s_Arm%s' % (nm, 'LR'[s < 0]), [(p.x, p.z) for p in q], s * (y - .04), s * (y + .04), M_FRM, .006, 2)
        for e, p in (('Lo', a), ('Hi', b)): cyl('%s_Eye%s%s' % (nm, e, 'LR'[s < 0]), (p.x, s * y, p.z), .050, .10, 'y', M_DARK, 32, .004)
    m_ = Vector(((SX + XL) / 2, 0, SZ0 + H0 / 2)); cyl(nm + '_Mid', tuple(m_), .040 if nm[-1] == 'B' else .045, 1.27 if nm[-1] == 'B' else .84, 'y', M_STEEL, 32, .004)
cyl('ScissorA_RamTube', (SX - 2.6 * math.cos(T0), 0, SZ0 + 2.6 * math.sin(T0) - .012), .038, .84, 'y', M_DARK, 28); cyl('ScissorB_Tie', (XL + .28, 0, SZ0 + .008), .030, 1.10, 'y', M_DARK, 28); cyl('ScissorA_Tie', (XL + 1.55, 0, SZ0 + H0 * (1 - 1.55 / SL)), .030, .84, 'y', M_DARK, 28)
EYE, RB = Vector((SX - 2.6 * math.cos(T0), 0, SZ0 + 2.6 * math.sin(T0) - .012)), Vector((.60, 0, .83)); ud = (EYE - RB).normalized()
for s, sn, f in SIDES:
    y = Vector((0, s * .22, 0)); tube('RamBarrel_' + sn, [tuple(RB + y + ud * .04), tuple(RB + y + ud * (EYE - RB).length * .80)], .062, M_YEL, bez=False, bres=6); tube('RamRod_' + sn, [tuple(RB + y + ud * (EYE - RB).length * .08), tuple(EYE + y)], .036, M_STEEL, bez=False, bres=6)
    piv('Ram_' + sn, tuple(RB + y)); piv('RamEye_' + sn, tuple(EYE + y)); box('RamFoot_' + sn, (.60, s * .22, .83), (.12, .16, .12), M_DARK, .01)
    box('ScissorFoot_' + sn, (SX, s * .383, 1.075), (.14, .03, .15), M_CHAS, .006); box('ScissorRail_' + sn, (XL + .55, s * .645, 1.085), (1.50, .025, .17), M_CHAS, .006, 1); cyl('ScissorFootPin_' + sn, (SX, s * .43, SZ0), .026, .12, 'y', M_STEEL, 24)
    for i, x in enumerate((XL + .05, XL + .95)): box('ScissorRailArm%d_%s' % (i, sn), (x, s * .555, 1.015), (.08, .20, .035), M_CHAS, .004)
    cyl('ScissorFootRoller_' + sn, (XL, s * .63, SZ0), .040, .04, 'y', M_DARK, 28)
box('RamCross', (.60, 0, .83), (.14, .80, .10), M_CHAS, .008)
piv('ScissorA', (SX, 0, SZ0)); piv('ScissorB', (SX, 0, SZ0 + H0)); piv('ScissorMid', tuple(m_))
# #endregion

# #region ---- box: floor 1.50 down, 4.25 up; everything named as in BOXP goes up with it
BX0, BX1, BY, BF, BT, WT = -3.70, 2.90, 1.25, 1.50, 3.76, .05
box('Box_Floor', ((BX0 + BX1) / 2, 0, 1.4825), (BX1 - BX0 - .10, 2 * BY - .10, .035), M_DECK, .0, 1)
for s, sn, f in SIDES:
    w = box('Box_Wall' + sn, ((BX0 + BX1) / 2, s * (BY - WT / 2), (BF - .06 + BT) / 2), (BX1 - BX0, WT, BT - BF + .06), M_BODY, .006, 2)
    if s > 0: cut(w, box('c', (2.40, BY, 2.485), (.70, .4, 1.91), M_BODY))
    box('Box_LinerW' + sn, ((BX0 + BX1) / 2 - (.45 if s > 0 else 0), s * (BY - WT - .006), 2.58), (BX1 - BX0 - .12 - (.90 if s > 0 else 0), .016, 2.14), M_WALL, .002, 1)
    box('Box_CornerR' + sn, (BX0 + .05, s * (BY - .045), (BF + BT) / 2 - .03), (.112, .098, BT - BF + .08), M_ALU, .016, 3); box('Box_CornerF' + sn, (BX1 - .05, s * (BY - .045), (BF + BT) / 2 - .03), (.112, .098, BT - BF + .08), M_ALU, .016, 3)
    box('Box_RoofRail' + sn, ((BX0 + BX1) / 2, s * (BY - .03), BT - .005), (BX1 - BX0 - .02, .075, .075), M_ALU, .016, 3); box('Box_Skirt' + sn, ((BX0 + BX1) / 2 - (.36 if s > 0 else 0), s * (BY - .015), 1.425), (BX1 - BX0 - .03 - (.72 if s > 0 else 0), .045, .07), M_ALU, .010, 2)
    box('Box_Stripe' + sn, (-1.00 if s > 0 else -.40, s * (BY + .0005), 1.92), (5.00 if s > 0 else 6.2, .004, .42), M_MAIN, .0012, 1); box('NamePanel_' + sn, (-.95 if s > 0 else -.40, s * BY, 2.86), (3.60, .030, .62), M_NAME, .004, 1); ik('Name_' + sn, (-.95 if s > 0 else -.40, s * (BY + .0152), 2.86))
    for i in range(9): box('Box_Hi%d%s' % (i, sn), (-3.36 + .62 * i, s * (BY + .0005), 1.56), (.52, .006, .055), M_HVR if i % 2 else M_HVW, .0012, 1)
    # under the floor: the long beams, the platform's guide rails, the scissor's upper track
    box('Box_Beam' + sn, ((BX0 + BX1) / 2, s * .95, 1.375), (BX1 - BX0 - .16, .11, .19), M_CHAS, .008, 1); box('Box_Guide' + sn, (1.20, s * .842, 1.395), (3.30, .030, .155), M_STEEL, .004, 1); box('Box_GuideLip' + sn, (1.125, s * .815, 1.322), (3.15, .054, .012), M_STEEL, .002, 1); box('Box_GuideFix' + sn, (1.20, s * .875, 1.40), (.50, .06, .05), M_CHAS, .004)
    box('Box_Track' + sn, (-.45, s * .515, 1.311), (5.0, .22, .044), M_CHAS, .004, 1)
    for i, x in enumerate((SX, XL)): box('Box_TrackLug%d%s' % (i, sn), (x, s * (.645 if i == 0 else .383), 1.24), (.14, .028, .16), M_CHAS, .006); cyl('Box_TrackPin%d%s' % (i, sn), (x, s * (.61 if i == 0 else .425), SZ0 + H0), .026, .10, 'y', M_STEEL, 24)
    box('Box_TrackLugTop' + sn, (SX, s * .64, 1.311), (.14, .06, .044), M_CHAS, .004); box('Box_TrackSlide' + sn, (XL + .55, s * .383, 1.235), (1.50, .024, .155), M_CHAS, .004, 1)
    for i, x in enumerate((2.00, -.55, -2.80)): box('Box_Cross%d%s' % (i, sn), (x, s * .77, 1.3085), (.12, .36, .036), M_CHAS, .004)
    for i, x in enumerate((-.75, -1.75, -2.75)): box('Box_Out%d%s' % (i, sn), (x, s * 1.10, 1.43), (.08, .24, .07), M_CHAS, .004)
    for i, x in enumerate((-.80, -1.8, -2.8, -3.5)): box('Box_In%d%s' % (i, sn), (x, s * .45, 1.40), (.10, .94, .135), M_CHAS, .004)
    # inside: load rails with slots, scuff board, lamp strips, straps
    for k, z in enumerate((2.05, 2.75)):
        box('Box_Rail%d%s' % (k, sn), (-.95 if s > 0 else -.4, s * 1.180, z), (4.9 if s > 0 else 6.1, .020, .10), M_ALU, .003, 1)
        for i in range(24 if s > 0 else 30): box('Box_Rail%d%s_S%02d' % (k, sn, i), (-3.28 + .2 * i, s * 1.1695, z), (.018, .004, .06), M_DARK, .0, 1)
    box('Box_Scuff' + sn, (-.95 if s > 0 else -.4, s * 1.181, 1.60), (4.9 if s > 0 else 6.1, .018, .18), M_ALU, .003, 1)
    for i, x in enumerate((-2.7, -.9, .9)): box('BoxLight%d%s_Base' % (i, sn), (x, s * .95, 3.700), (1.2, .12, .03), M_ALU, .006); box('BoxLight%d%s_Lens' % (i, sn), (x, s * .95, 3.6825), (1.16, .09, .012), M_WHT, .003)
    for i, x in enumerate((-2.35, -1.15, .05)):
        z = 2.75; box('Box_StrapHook%d%s' % (i, sn), (x, s * 1.158, z), (.05, .03, .07), M_STEEL, .004); tube('BoxStrap_%s%d' % (sn, i), [(x, s * 1.140, z), (x + .01, s * 1.125, z - .09), (x, s * 1.14, z - .15)], .010, M_STRAP); box('BoxStrap_%s%d_Buckle' % (sn, i), (x, s * 1.137, z - .17), (.05, .03, .08), M_STEEL, .006); ik('Strap_%s%d' % (sn, i), (x, s * 1.13, z - .17))
for k, x in (('F', 2.80), ('R', 1.985)): tube('Box_GrabDoor' + k, [(x, 1.240, 1.75), (x, 1.295, 1.79), (x, 1.295, 2.50), (x, 1.240, 2.54)], .014, M_YEL); ik('Grab_BoxDoor_' + k, (x, 1.295, 2.2))
box('Box_Roof', ((BX0 + BX1) / 2, 0, BT - .02), (BX1 - BX0 - .02, 2 * BY - .06, .05), M_BODY, .008, 2); box('Box_Ceil', ((BX0 + BX1) / 2, 0, BT - .050), (BX1 - BX0 - .12, 2 * BY - .12, .016), M_WALL, .002, 1)
for i, x in enumerate((-2.4, -.7, 1.0)): box('Box_RoofBow%d' % i, (x, 0, BT + .006), (.05, 2.36, .014), M_ALU, .004, 1)
def endwall(nm, x, half, top, sg):
    w = box('Box_' + nm, (x, 0, (BF - .06 + BT) / 2), (.05, 2 * BY - .08, BT - BF + .04), M_BODY, .003, 1); cut(w, box('c', (x, 0, (1.50 + top) / 2), (.3, 2 * half, top - 1.50), M_BODY))
    if sg > 0: cut(w, box('c', (x, 0, 1.40), (.3, 1.70, .18), M_BODY))
FH, FTOP, RH, RTOP = .66, 3.40, 1.04, 3.40
endwall('FrontWall', BX1 - .03, FH, FTOP, 1); endwall('RearWall', BX0 + .03, RH, RTOP, -1)
for nm, x, half, sg in (('F', BX1 - .10, FH, 1), ('R', BX0 + .10, RH, -1)):
    for s in (-1, 1): g = box('Box_RollGuide%s%d' % (nm, s > 0), (x, s * (half + .018), 2.52), (.085, .075, 2.04), M_ALU, .006); cut(g, box('c', (x, s * (half - .005), 2.5), (.030, .04, 2.2), M_ALU))
    box('Box_RollHood' + nm, (x - sg * .10, 0, 3.565), (.30, 2 * half + .20, .24), M_WALL, .020, 2)
    for i in range(12): box('Roll%s_%02d' % (nm, i), (x, 0, 1.592 + i * .1585), (.018, 2 * half + .016, .1595), M_ALU, .004, 1)
    box('Roll%s_00_Lip' % nm, (x, 0, 1.509), (.024, 2 * half - .03, .016), M_SEAL, .003, 1)
    for d, t in ((1, 'Out'), (-1, 'In')): box('Roll%s_00_Grip%s' % (nm, t), (x + d * sg * .019, 0, 1.60), (.022, .16, .03), M_PLAST, .006); ik('Handle_Roll%s_%s' % (nm, t), (x + d * sg * .028, 0, 1.60))
    piv('Roll' + nm, (x, 0, 3.47))
# side door, left (hinge in front, 100 deg)
box('BoxDoorL', (2.40, BY - WT / 2, 2.485), (.690, .046, 1.90), M_BODY, .005, 2); box('BoxDoorL_Liner', (2.40, BY - WT - .004, 2.485), (.62, .014, 1.82), M_WALL, .002, 1); box('BoxDoorL_Stripe', (2.37, BY - .0015, 1.92), (.54, .004, .42), M_MAIN, .0012, 1)
for i, z in enumerate((1.64, 2.48, 3.24)): cyl('BoxDoorL_Hinge%d' % i, (2.748, BY + .004, z), .013, .12, 'z', M_STEEL, 20); box('BoxDoorL_Leaf%d' % i, (2.705, BY - .0005, z), (.085, .008, .08), M_STEEL, .002, 1)
box('BoxDoorL_Cup', (2.11, BY - .0015, 2.36), (.06, .010, .20), M_PLAST, .004, 1); box('BoxDoorL_Handle', (2.11, BY + .018, 2.36), (.028, .030, .16), M_CHROME, .008, 2); box('BoxDoorL_HandleIn', (2.11, BY - .075, 2.36), (.028, .03, .16), M_CHROME, .008, 2); box('BoxDoorL_FootIn', (2.11, BY - .058, 2.36), (.05, .03, .06), M_PLAST, .006)
piv('BoxDoorL', (2.748, BY + .004, 2.4)); ik('Handle_BoxDoorL_Out', (2.11, BY + .035, 2.36)); ik('Handle_BoxDoorL_In', (2.11, BY - .092, 2.36)); ik('Stand_BoxDoorL', (2.30, 1.75, 0)); ik('Box_In', (2.35, .65, BF + .002))
# lamps, beacon, warnings of the box
for s, sn, f in SIDES:
    box('BoxMark_%s_Base' % sn, (BX0 - .004, s * 1.16, 3.60), (.03, .12, .06), M_PLAST, .006); box('BoxMark_%s_Lens' % sn, (BX0 - .020, s * 1.16, 3.60), (.012, .10, .04), M_RLENS, .003); box('BoxMarkF_%s_Base' % sn, (BX1 + .004, s * 1.16, 3.60), (.03, .12, .06), M_PLAST, .006); box('BoxMarkF_%s_Lens' % sn, (BX1 + .020, s * 1.16, 3.60), (.012, .10, .04), M_WHT, .003)
    for i in range(7): facepoly('Box_Chev%s%d' % (sn, i), [(-.030, -.075), (.030, -.015), (.030, .065), (-.030, .005)], (BX0 + .005, s * 1.105, 1.66 + .22 * i), 'x-', M_SRED if i % 2 else M_YEL, .0016)
box('WorkLamp_Base', (BX1 + .03, 0, 3.58), (.10, .42, .10), M_PLAST, .016, 2); box('WorkLamp_Lens', (BX1 + .0815, 0, 3.575), (.010, .36, .07), M_WHT, .003); box('TailWork_Base', (BX0 - .02, 0, 3.60), (.06, .30, .08), M_PLAST, .012); box('BoxRearWork_Lens', (BX0 - .0515, 0, 3.60), (.008, .26, .055), M_WHT, .002)
cyl('Box_BeaconBase', (-3.30, 0, BT + .020), .085, .05, 'z', M_PLAST, 40, .006); CK.lathez('BoxBeaconLens', [(0, BT + .03), (.074, BT + .03), (.078, BT + .045), (.076, BT + .11), (.066, BT + .14), (.04, BT + .157), (0, BT + .16)], (-3.30, 0, 0), M_AMB, 48); ik('BoxBeacon', (-3.30, 0, BT + .09))
text('Box_WordTail', 'KEEP CLEAR  -  BODY RISES', (BX0 + .005, 0, 3.485), .060, 'x-', M_SRED)
# panel inside, left of the front door: lift, platform, stop
box('BoxPanel', (2.83, 1.02, 2.62), (.07, .30, .42), M_FRM, .010)
for nm, y, w in (('Lift', 1.09, 'BODY'), ('Plat', .96, 'PLATFORM')):
    tube('Lever_' + nm, [(2.80, y, 2.60), (2.715, y, 2.60)], .008, M_STEEL, bez=False); cyl('Lever_%s_Knob' % nm, (2.708, y, 2.60), .019, .035, 'x', M_PLAST, 24, .006); box('BoxPanel_Slot' + nm, (2.7935, y, 2.60), (.006, .026, .16), M_PLAST, .002, 1); piv('Lever_' + nm, (2.80, y, 2.60)); ik('Lever_' + nm, (2.70, y, 2.60)); text('BoxPanel_W' + nm, w, (2.795, y, 2.475), .020, 'x-', M_SWHT, .0012)
cyl('BoxPanel_EStop', (2.789, 1.02, 2.765), .026, .024, 'x', M_RED, 28, .005); ik('Button_EStop', (2.775, 1.02, 2.765)); ik('BoxPanel_Stand', (2.25, .80, BF + .002))
# galley box rack, right rear (boxes and trolleys are the kit's own meshes, places in the json)
for i, x in enumerate((-3.30, -2.14)):
    for k, y in enumerate((-.72, -1.155)): box('Box_RackPost%d%d' % (i, k), (x, y, 2.36), (.035, .035, 1.75), M_ALU, .004, 1)
for k, z in enumerate((1.62, 2.14, 2.66)): box('Box_RackShelf%d' % k, (-2.72, -.94, z), (1.19, .46, .022), M_DECK, .003, 1); box('Box_RackLip%d' % k, (-2.72, -.712, z + .030), (1.16, .012, .05), M_ALU, .002, 1)
# #endregion

# #region ---- platform (slides 2.95 m forward), its rails, gate, ramp flap
PT = 1.365; PX0, PX1 = -.42, 2.78; SLIDE = 2.95
box('Platform_Deck', ((PX0 + PX1) / 2, 0, PT - .0125), (PX1 - PX0, 1.52, .025), M_DECK, .003, 1)
for s, sn, f in SIDES:
    box('Platform_Beam' + sn, ((PX0 + PX1) / 2, s * .787, 1.398), (PX1 - PX0, .054, .116), M_YEL, .006, 1)
    for i, x in enumerate((PX0 + .10, .6, 1.9)): cyl('Platform_Roller%d%s' % (i, sn), (x, s * .818, 1.398), .040, .012, 'y', M_DARK, 28)
for i, x in enumerate((PX0 + .04, .65, 1.70)): box('Platform_Rib%d' % i, (x, 0, 1.3385), (.05, 1.5, .010), M_CHAS, .002, 1)
box('Platform_Nose', (PX1 + .015, 0, 1.365), (.05, 1.62, .10), M_YEL, .010, 2); box('Platform_Lip', (PX1 - .03, 0, PT + .006), (.10, 1.5, .022), M_ALU, .006, 2); box('Platform_BumpBar', (PX1 + .045, 0, 1.36), (.02, 1.60, .07), M_DARK, .004)
for i in range(5): cyl('Platform_Bump%d' % i, (PX1 + .075, -.65 + .325 * i, 1.36), .045, .30, 'y', M_SEAL, 32, .012)
XS = (PX0 + .62, .95, 1.75, PX1 - .20)
def rail(n, s, yh, zh, hgt):
    """One side rail, built standing; lies folded inwards in the cassette (PIV_, about x)."""
    for t, z, r in (('Top', hgt, .0125), ('Mid', hgt * .52, .010), ('Low', .10, .010)): tube(n + '_' + t, [(XS[0], yh, zh + z), (XS[-1], yh, zh + z)], r, M_YEL, bez=False, bres=4)
    for i, x in enumerate(XS): tube('%s_Post%d' % (n, i), [(x, yh, zh - .006), (x, yh, zh + hgt)], .0125, M_YEL, bez=False, bres=4)
    piv(n, ((XS[0] + XS[-1]) / 2, yh, zh))
rail('RailPlatform_L', 1, .715, 1.384, 1.05); rail('RailPlatform_R', -1, -.715, 1.440, 1.00)
for i, x in enumerate(XS): box('Platform_ClipL%d' % i, (x, .742, 1.384), (.05, .050, .03), M_DARK, .004, 1); box('Platform_ClipR%d' % i, (x, -.742, 1.410), (.05, .050, .075), M_DARK, .004, 1)
GY, GX = -.715 + .029, PX1 - .20                                            # the gate: hinged on the right rail's front post, lies inside the rail (folds with it)
for k, z in enumerate((2.41, 1.96, 1.58)): tube('GatePlatform_Bar%d' % k, [(GX, GY, z), (GX - 1.34, GY, z)], .0115, M_YEL, bez=False, bres=4)
for k, x in enumerate((GX, GX - .67, GX - 1.34)): tube('GatePlatform_Post%d' % k, [(x, GY, 1.58), (x, GY, 2.41)], .0115, M_YEL, bez=False, bres=4)
for k, z in enumerate((1.74, 2.24)): box('GatePlatform_Hinge%d' % k, (GX, GY - .015, z), (.03, .036, .04), M_DARK, .004, 1)
piv('GatePlatform', (GX, GY, 1.9)); ik('Handle_GatePlatform', (GX - 1.30, GY, 2.41)); ik('Rail_L_Hand', (1.2, .715, 2.434)); ik('Rail_R_Hand', (1.2, -.715, 2.44))
ik('Platform_Work', (1.9, 0, PT + .002)); ik('Platform_Bumper', (PX1 + .120, 0, PT)); ik('Platform_Sill', (PX1 + .02, 0, PT))
box('RampFlap', (BX1 + .030, 0, 1.725), (.016, 1.22, .42), M_DECK, .003, 1); cyl('RampFlap_Hinge', (BX1 + .030, 0, 1.500), .014, 1.24, 'y', M_STEEL, 24); box('RampFlap_Grip', (BX1 + .034, 0, 1.90), (.030, .16, .03), M_PLAST, .006); box('Box_RampSill', (BX1 + .004, 0, 1.484), (.070, 1.30, .030), M_ALU, .004, 1)
piv('RampFlap', (BX1 + .030, 0, 1.500)); ik('Handle_RampFlap', (BX1 + .034, 0, 1.91))
# #endregion

# #region ---- tail lift: arms .90 about PIV_TailArms, the platform stays level (or stands up to drive)
AX, AZ, AL = -3.16, .80, .90; aU, aD = math.asin(.66 / AL), -math.asin(.725 / AL); HX, HZ = AX - AL * math.cos(aU), AZ + AL * math.sin(aU)
for s, sn, f in SIDES:
    prism('Box_TailBracket' + sn, [(AX - .10, .70), (AX + .12, .70), (AX + .26, 1.36), (AX - .10, 1.36)], s * .545, s * .575, M_CHAS, .004); box('Box_TailBracketTop' + sn, (AX + .08, s * .56, 1.345), (.36, .16, .04), M_CHAS, .004); box('Box_TailCross' + sn, (AX + .08, s * .75, 1.335), (.30, .40, .05), M_CHAS, .004)
    a, b = Vector((AX, 0, AZ)), Vector((HX, 0, HZ)); u = (b - a).normalized(); n = Vector((-u.z, 0, u.x)); q = []
    for k in range(9): th = math.pi / 2 - math.pi * k / 8; q.append(a + (n * math.sin(th) - u * math.cos(th)) * .045)
    for k in range(9): th = -math.pi / 2 + math.pi * k / 8; q.append(b + (n * math.sin(th) + u * math.cos(th)) * .036)
    prism('TailArms_' + sn, [(p.x, p.z) for p in q], s * .585, s * .635, M_FRM, .006, 2); cyl('TailArms_Pin' + sn, (AX, s * .59, AZ), .028, .12, 'y', M_STEEL, 24); cyl('TailArms_Tip' + sn, (HX, s * .61, HZ), .022, .09, 'y', M_STEEL, 24)
    box('TailPlatform_Ear' + sn, (HX - .004, s * .667, HZ + .015), (.05, .03, .11), M_DARK, .006)
cyl('TailArms_Tie', (AX - .30, 0, AZ + .30 * math.tan(aU)), .030, 1.20, 'y', M_FRM, 28)
pl = rawprism('TailPlatform', [(HX - .030, HZ + .05), (HX + .020, HZ + .05), (HX + .020, HZ + 1.52), (HX - .004, HZ + 1.60), (HX - .012, HZ + 1.60), (HX - .030, HZ + 1.52)], 'y', -1.16, 1.16); K.finish(pl, M_DECK, 30)
for s, sn, f in SIDES:
    box('TailPlatform_Edge' + sn, (HX - .005, s * 1.163, HZ + .78), (.062, .030, 1.46), M_ALU, .005, 1); box('TailPlatform_Flag' + sn, (HX - .034, s * .95, HZ + 1.30), (.010, .30, .20), M_HVR, .002, 1)
box('TailPlatform_Root', (HX - .005, 0, HZ + .045), (.060, 2.30, .06), M_DARK, .006); plate('TailPlatform_NamePanel_Rear', (HX - .031, 0, HZ + .85), 1.10, .26, 'x-', M_NAME, .006); ik('Name_Rear', (HX - .0372, 0, HZ + .85))
box('TailPlatform_Grip', (HX - .055, -.60, HZ + 1.40), (.028, .16, .03), M_PLAST, .006); ik('Handle_TailPlatform', (HX - .070, -.60, HZ + 1.40))
for k in (-1, 1): box('TailPlatform_GripFoot%d' % k, (HX - .038, -.60 + .06 * k, HZ + 1.40), (.02, .02, .024), M_PLAST, .003, 1)
piv('TailArms', (AX, 0, AZ)); piv('TailPlatform', (HX, 0, HZ)); ik('Tail_Stand', (-5.75, -.9, 0)); ik('Tail_Control', (-3.45, -1.275, 1.36)); ik('Tail_Control_Stand', (-3.45, -1.85, 0)); ik('Tail_Floor', (BX0 - .9, 0, BF)); ik('Box_RearFloor', (BX0 + .5, 0, BF + .002)); ik('Box_FrontFloor', (BX1 - .5, 0, BF + .002))
box('BoxTailPanel', (-3.45, -1.262, 1.36), (.20, .04, .16), M_FRM, .006); box('BoxTailPanel_Arm', (-3.45, -1.12, 1.36), (.08, .26, .05), M_CHAS, .004)
for k, (x, m) in enumerate(((-3.50, M_GRN), (-3.40, M_RED))): cyl('BoxTailPanel_B%d' % k, (x, -1.286, 1.36), .020, .018, 'y', m, 24, .004); ik('Button_Tail%s' % ('Up', 'Down')[k], (x, -1.298, 1.36))
# #endregion
ik('Driver_Eye', (.60 + ox, DY, FLOOR + 1.145)); ik('Aisle_Row0', (4.15, DY, FLOOR + .01)); ik('Tow', (5.1, -.40, .50))

# #region ---- folded as it drives: rails and gate lie in the cassette; then the whole truck moves so that the origin is its middle
def rot(pre, pv, ax, deg):
    M = Matrix.Translation(Vector(pv)) @ Matrix.Rotation(math.radians(deg), 4, ax) @ Matrix.Translation(-Vector(pv))
    for o in ALL:
        if o.name.startswith(pre): o.data.transform(M); o.data.update()
STAND = '--up' in sys.argv
if not STAND: rot('RailPlatform_L', (1.06, .715, 1.384), 'X', 90); rot(('RailPlatform_R', 'GatePlatform'), (1.06, -.715, 1.440), 'X', -90)
for e in IKS: e.location.x += SHIFT
for o in ALL: o.data.transform(Matrix.Translation((SHIFT, 0, 0))); o.data.update()
# #endregion
K.uv_cube(); S.drop_lofts()
sh = lambda p: (p[0] + SHIFT, p[1], p[2])
BOXP = ('Box', 'Roll', 'Ramp', 'Platform', 'Gate', 'RailPlatform', 'Tail', 'NamePanel_L', 'NamePanel_R', 'WorkLamp', 'Lever_Lift', 'Lever_Plat')
isbox = lambda n: n.startswith(BOXP) and not n.startswith(('BoxStep', 'TailBrake'))
PLT = ('Platform', 'Gate', 'RailPlatform')
def lift(q, slide=0.0):
    """Moves the whole thing as the game will: box up by q, platform forward, scissor turned."""
    t = math.asin((H0 + q) / SL); c0 = Vector(((SX + XL) / 2 + SHIFT, 0, SZ0 + H0 / 2)); c1 = Vector((SX - SL / 2 * math.cos(t) + SHIFT, 0, SZ0 + (H0 + q) / 2)); r0, r1 = Vector((RB.x + SHIFT, 0, RB.z)), Vector((SX - 2.6 * math.cos(t) + SHIFT, 0, SZ0 + 2.6 * math.sin(t) - .012)); e0 = Vector((EYE.x + SHIFT, 0, EYE.z))
    Rr = (e0 - r0).rotation_difference(r1 - r0).to_matrix().to_4x4()
    for o in ALL:
        n = o.name
        if isbox(n): o.matrix_world = Matrix.Translation((slide if n.startswith(PLT) else 0, 0, q))
        elif n.startswith(('ScissorA', 'ScissorB')): o.matrix_world = Matrix.Translation(c1) @ Matrix.Rotation((t - T0) * (1 if n[7] == 'A' else -1), 4, 'Y') @ Matrix.Translation(-c0)
        elif n.startswith('RamBarrel'): o.matrix_world = Matrix.Translation(r0) @ Rr @ Matrix.Translation(-r0)
        elif n.startswith('RamRod'): o.matrix_world = Matrix.Translation(r1) @ Rr @ Matrix.Translation(-e0)
    bpy.context.view_layer.update()
if '--nochk' not in sys.argv:
    K.checks('CT', ground_ok=('Tyre',), overhang_skip_prefix=('',))
    for nm, pv, ax, dg, sk in (('DoorDriver', (4.775, 1.215, 1), 'z', (-15, -40, -68), ()), ('BoxDoorL', (2.748, BY + .004, 1), 'z', (-20, -60, -100), ()), ('RampFlap', (BX1 + .030, 0, 1.5), 'y', (40, 80, 106), ('Cab', 'Body', 'Aerial', 'Locker_B', 'Seat')), ('TailPlatform', (HX, 0, HZ), 'y', (-30, -60, -90), ('TailArms',)), ('TailArms', (AX, 0, AZ), 'y', (-30, -60, -math.degrees(aU - aD)), ('TailPlatform',))):
        h = K.sweep(nm, sh(pv), ax, dg, sk); print('CT SWEEP', nm, len(h), sorted(h)[:8])
    for k in ('FL', 'FR'):
        h = set(); p = sh((FX, .8 if k[1] == 'L' else -.8, 0))
        for dg in (-38, 38):
            o = bpy.data.objects['Tyre_' + k]; Rm = Matrix.Translation(p) @ Matrix.Rotation(math.radians(dg), 4, 'Z') @ Matrix.Translation(-Vector(p)); t = BVHTree.FromPolygons([Rm @ v.co for v in o.data.vertices], [tuple(q.vertices) for q in o.data.polygons])
            for o2 in ALL:
                if k not in o2.name and t.overlap(K.bvh(o2)): h.add((o2.name, dg))
        print('CT STEER', k, sorted(h)[:12])
    for q, sl in ((.0, 0), (.6, 0), (1.5, 0), (1.75, SLIDE), (2.05, SLIDE), (2.75, SLIDE)):
        lift(q, sl); TB = {o.name: K.bvh(o) for o in ALL}; G = lambda n: 'box' if isbox(n) else 'A' if n.startswith('ScissorA') else 'B' if n.startswith('ScissorB') else 'ram' if n.startswith('Ram') else 'car'
        h = [(a, b) for a in TB if G(a) == 'box' for b in TB if G(b) in ('car', 'A', 'B', 'ram') and TB[a].overlap(TB[b])] + [(a, b) for a in TB if G(a) in 'AB' for b in TB if G(b) == 'car' and TB[a].overlap(TB[b])]
        h = [x for x in h if not any(w in x[0] + x[1] for w in ('Pin', 'Roller', '_Mid'))]; print('CT LIFT %.2f slide %.2f: %d' % (q, sl, len(h)), h[:10])
    lift(0)
    for sl in (0, .8, 1.6, 2.4, 2.95):
        TB = {o.name: BVHTree.FromPolygons([v.co + Vector((sl if o.name.startswith(PLT) else 0, 0, 0)) for v in o.data.vertices], [tuple(p.vertices) for p in o.data.polygons]) for o in ALL if isbox(o.name)}
        h = [(a, b) for a in TB if a.startswith(PLT) for b in TB if not b.startswith(PLT) and TB[a].overlap(TB[b])]; print('CT SLIDE %.1f: %d' % (sl, len(h)), [x for x in h if 'Roller' not in x[0]][:8])
    h = [(a, b) for a in TB if a.startswith('RailPlatform_L') for b in TB if b.startswith(('RailPlatform_R', 'Gate')) and TB[a].overlap(TB[b])]; print('CT FOLD L-R', h[:5])
GROUPS = ['DoorDriver', 'DoorPassenger', 'BoxDoorL', 'SteeringWheel', 'Platform', 'GatePlatform', 'RailPlatform_L', 'RailPlatform_R', 'RampFlap', 'TailArms', 'TailPlatform', 'ScissorA', 'ScissorB', 'Lever_Stab', 'Lever_Lift', 'Lever_Plat', 'Pedal_Accel', 'Pedal_Brake', 'WiperL', 'WiperR', 'StabLeg_FL', 'StabLeg_FR', 'StabLeg_RL', 'StabLeg_RR', 'RollF_00', 'RollR_00']
WK = ('FL', 'FR', 'RL', 'RR')
def groupof(o):
    n = o.name
    if n.startswith(('Tyre_', 'Body', 'RamBarrel', 'RamRod')) or n.endswith('_Lens') or n in ('BeaconLens', 'BoxBeaconLens', 'Box_WallL', 'Box_WallR') or 'NamePanel' in n or (n.startswith('Roll') and n[6:] != '00' and '_00_' not in n): return None
    for k in WK:
        if n.startswith(('Rim_' + k, 'WheelDisc_' + k, 'Hub_' + k, 'HubCap_' + k, 'Nut_' + k)): return 'Rim_' + k
        if n.startswith(('Spindle_' + k, 'SpindleDrum_' + k)): return 'Spindle_' + k
    g = next((g for g in sorted(GROUPS, key=len, reverse=True) if n.startswith(g)), None)
    if g: return g + '_Glass' if n.endswith('_Glass') else g + '_Parts' if g.startswith(('DoorD', 'DoorP')) and n != g else g
    if n.startswith('BoxStep'): return 'BoxStep'
    if n.startswith(('Box_Beam', 'Box_Guide', 'Box_Track', 'Box_Cross', 'Box_Out', 'Box_In', 'Box_Tail')): return 'BoxUnder'
    if n.startswith(('Box', 'TailWork', 'WorkLamp')): return 'BoxInside' if n.startswith(('Box_Rail', 'Box_Rack', 'Box_Scuff', 'Box_Strap', 'BoxStrap', 'BoxLight', 'BoxPanel', 'Box_Liner', 'Box_Ceil', 'Box_Floor', 'Box_RollHood')) else 'BoxOutside'
    if n.endswith('_Glass'): return 'GlassFixed'
    if n.startswith(('Seat', 'Belt')): return 'Seats'
    if n.startswith(('Dash', 'CabM', 'SunV', 'GrabIn', 'Steer', 'Stalk', 'Foot', 'Gear', 'ParkB', 'Firewall', 'Cowl', 'Locker_Back', 'Clip', 'RoofL', 'CabLiner', 'Floor', 'EngineHump', 'BPillar')) and not n.startswith('SteerArm'): return 'CabFittings'
    if n.startswith(('Frame', 'Axle', 'Leaf', 'Hanger', 'Drive', 'Air', 'Fender', 'Mud', 'Prop', 'Gearbox', 'Engine', 'Diff', 'Exhaust', 'SteerArm', 'Scissor', 'Ram', 'Stab', 'Rear', 'Side')): return 'Underside'
    return 'OuterFittings'
def joined():
    G = {}
    for o in ALL:
        g = groupof(o)
        if g: G.setdefault(g, []).append(o)
    for g, l in G.items(): l[0].name = 'tmp_' + g; CK.join(g, l)
    for o in ALL: assert o.data.materials and all(o.data.materials), o.name; assert {p.material_index for p in o.data.polygons} == set(range(len(o.data.materials))), 'empty slot ' + o.name
if '--pic' in sys.argv:
    OUT = sys.argv[sys.argv.index('--pic') + 1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.use_gtao = True; sc.eevee.taa_render_samples = 8
    w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (.6, .68, .8, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = .8; sc.world = w; K.glass(M_GLASS, .25)
    sd = bpy.data.lights.new('S', 'SUN'); sd.energy = 3.5; so = bpy.data.objects.new('S', sd); sc.collection.objects.link(so); so.rotation_euler = (.8, .1, .6)
    cd = bpy.data.cameras.new('C'); bc = bpy.data.objects.new('C', cd); sc.collection.objects.link(bc); sc.camera = bc; sc.render.resolution_x, sc.render.resolution_y = 640, 450
    def shot(n, loc, at, lens=40): bc.location = loc; bc.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cd.lens = lens; cd.clip_start = .02; sc.render.filepath = os.path.join(OUT, n + '.png'); bpy.ops.render.render(write_still=True)
    if not STAND: shot('p1', (10, 8, 3.4), (0, 0, 1.8)); shot('p2', (-10, 8, 3.4), (0, 0, 1.8))
    else: lift(2.37, SLIDE); shot('p3', (11.5, 9.5, 5.2), (1.0, 0, 2.6)); shot('p4', (9.6, .9, 5.9), (2.2, 0, 4.4), 28)
    sys.exit(0)
GLB = os.path.join(ART, 'cateringtruck.glb'); ZF = os.path.join(ART, 'zfight-check.json')
last = K.save_export(ROOT, os.path.join(ART, 'CateringTruck.blend'), GLB, ZF, 'CT', 'CateringTruck'); joined()
last2 = K.save_export(ROOT, os.path.join(ART, 'CateringTruck.blend'), GLB, ZF, 'CT joined', 'CateringTruckGame')
ue = lambda v: [round(v[0] * 100, 1), round(-v[1] * 100, 1), round(v[2] * 100, 1)]
tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in ALL); lo = [min(K.bbox(o)[0][i] for o in ALL) for i in range(3)]; hi = [max(K.bbox(o)[1][i] for o in ALL) for i in range(3)]
st = [['Trolley', round(x + SHIFT, 3), y, BF, -90] for y in (.995, .68) for x in (-3.05, -2.16, -1.27, -.38, .51)] + [['Trolley', round(x + SHIFT, 3), -y, BF, -90] for y in (.995, .68) for x in (-1.52, -.63, .26, 1.15)]      # lengthwise, doors forward
st += [['GalleyBox', round(-3.15 + SHIFT + .285 * i, 3), -.945, z, 0] for z in (1.632, 2.152, 2.672) for i in range(4)]
json.dump({'note': 'points in the model root, Unreal cm (Blender (x,y,z) m = (x,-y,z) * 100)', 'size_m': [round(hi[i] - lo[i], 3) for i in range(3)], 'x_m': [round(lo[0], 3), round(hi[0], 3)], 'wheelbase': FX - DX, 'tyre_radius_cm': RW * 100, 'shift_m': SHIFT,
           'scissor': {'arm_cm': SL * 100, 'front_x_cm': round((SX + SHIFT) * 100, 1), 'z0_cm': SZ0 * 100, 'h0_cm': H0 * 100, 'lift_cm': LIFT * 100, 'deg0': round(math.degrees(T0), 3), 'deg1': round(math.degrees(T1), 3), 'ram_eye_on_A_cm': 260},
           'box': {'floor_cm': BF * 100, 'deck_below_floor_cm': round((BF - PT) * 100, 1), 'slide_cm': SLIDE * 100, 'moves_with_box': list(BOXP), 'not': ['BoxStep', 'TailBrake']}, 'tail': {'arm_cm': AL * 100, 'deg_up': round(math.degrees(aU), 2), 'deg_down': round(math.degrees(aD), 2)},
           'stock': [{'mesh': m, 'at_cm': ue((x, y, z)), 'yaw_blender_deg': a} for m, x, y, z, a in st], 'points_cm': {e.name: ue(e.location) for e in IKS}, 'parts': sorted(o.name for o in ALL), 'materials': sorted({m.name for o in ALL for m in o.data.materials}), 'tris': tris, 'flicker': last2}, open(os.path.join(ART, 'cateringtruck-parts.json'), 'w'), indent=1)
json.dump({'points': {e.name: [round(c, 4) for c in e.location] for e in IKS}}, open(os.path.join(ROOT, 'Data', 'cateringtruck-riders.json'), 'w'), indent=1)
print('DONE CT: %d parts, %d points, %d tris, size %s x %.2f..%.2f, %s' % (len(ALL), len(IKS), tris, [round(hi[i] - lo[i], 3) for i in range(3)], lo[0], hi[0], last2))
