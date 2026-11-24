# The potable water truck (to-do 212): our own small electric 2-axle truck, stainless tank 2.5 m3, pump panel and hose reel at the back, fixed work platform.
# Sizes: the class of small water trucks (7.3 x 2.2 x 2.4 m with 3 m3, 6.1 m with 1 m3; ours 5.8 x 2.0 x 2.5). Panel it must reach: the A320's water door 171AL,
# 31.3 m behind the nose, 0.3 m left of the middle, 2.6 m up (Airbus AC A320, 5-4-9); measured on Art/A320/A320.glb the belly there is at 2.93 m.
# Axes: x forward, y left, z up; metres; origin under the middle. Driver front left.   blender -b --python Art/WaterTruck/make_watertruck.py [-- --pic DIR]
import os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, 'Tools'), os.path.join(ROOT, 'Art', 'CleaningKit')]
PFX, DASHWORD, MAIN, NAME, KEY, TAG = 'WT_', 'TANK   PUMP   REEL', (.015, .17, .50), 'WaterTruck', 'watertruck', 'WT'
f_ = os.path.join(ROOT, 'Tools', 'small_truck.py'); exec(compile(open(f_, encoding='utf-8').read(), f_, 'exec'))
M_HB = P('WT_HoseBlue', (.05, .25, .62), 0, .5); M_PANEL = P('WT_PanelPaint', (.80, .82, .84), .05, .45); M_SBLUE = P('WT_SignBlue', (.01, .12, .42), 0, .5)

# #region ---- deck, tank, side boards
box('Deck', (-.45, 0, .862), (3.06, 1.98, .030), M_DECK, .005, 1)
EL = [(.84 * math.copysign(abs(math.cos(a)) ** .85, math.cos(a)), 1.42 + .50 * math.copysign(abs(math.sin(a)) ** .85, math.sin(a))) for a in [2 * math.pi * k / 72 for k in range(72)]]
solidx('Tank', EL, -1.36, .94, .13, M_SS, 40, 8)
for i, x in enumerate((-1.06, -.36, .34, .72)): bandx('TankBand%d' % i, EL, x, .045, .007, M_SS)
for i, x in enumerate((-.96, .60)):
    o = box('TankSaddle%d' % i, (x, 0, .97), (.12, 1.40, .24), M_CHAS, .006, 1); cut(o, cyl('c', (x, 0, 2.12), 1.195, .4, 'x', M_CHAS, 96))
for i, x in enumerate((-.70, .30)):
    cyl('Manhole%d' % i, (x, 0, 1.925), .235, .06, 'z', M_SS, 56, .006); cyl('Manhole%d_Lid' % i, (x, 0, 1.968), .225, .032, 'z', M_SS, 56, .010); box('Manhole%d_Bar' % i, (x, 0, 1.992), (.05, .40, .022), M_STEEL, .004)
    for e in (-1, 1): cyl('Manhole%d_Knob%d' % (i, e > 0), (x, e * .17, 2.008), .030, .03, 'z', M_SBLK, 24, .005)
    ik('Manhole_%d' % i, (x, .17, 2.02))
cyl('TankVent', (-.18, 0, 1.96), .045, .10, 'z', M_SS, 32, .006); CK.lathez('TankVent_Hat', [(0, 2.045), (.03, 2.045), (.075, 2.0), (.07, 1.992), (0, 2.0)], (-.18, 0, 0), M_SS, 36)
for s, sn, f in SIDES:
    for i, x in enumerate((-1.02, -.20, .62)): box('BoardPost%d_%s' % (i, sn), (x, s * .873, 1.235), (.05, .035, .73), M_CHAS, .004, 1)
    for i, z in enumerate((1.10, 1.52)): box('BoardRail%d_%s' % (i, sn), (-.20, s * .893, z), (1.90, .025, .05), M_CHAS, .003, 1)
    box('SideBoard_' + sn, (-.20, s * .911, 1.32), (2.04, .018, .58), M_BODY, .006, 2); box('SideBoard_%s_Stripe' % sn, (-.20, s * .9205, 1.135), (2.02, .006, .19), M_MAIN, .001, 1)
    box('NamePanel_' + sn, (-.06, s * .922, 1.42), (1.56, .008, .30), M_NAME, .002, 1); ik('Name_' + sn, (-.06, s * .926, 1.42))
    text('Word_%s' % sn, 'DRINKING WATER ONLY', (-.10, s * .9234, 1.135), .125, f, M_SWHT); dx = -1.02
    drop = [(.075 * math.sin(a) * (1 if abs(a) < 2 else 1), -.02 - .075 * math.cos(a)) for a in [math.radians(d) for d in range(-120, 121, 12)]] + [(.030, .065), (0, .135), (-.030, .065)]
    facepoly('Drop_' + sn, drop, (dx, s * .921, 1.42), f, M_SBLUE, .003)
    tube('HandRail_' + sn, [(.62, s * .873, 1.60), (.62, s * .873, 1.665), (-1.02, s * .873, 1.665), (-1.02, s * .873, 1.60)], .015, M_STEEL, bez=False)
for s in (-1, 1): box('HeadPost%d' % (s > 0), (1.04, s * .70, 1.33), (.06, .07, .92), M_CHAS, .006)
for i, z in enumerate((1.02, 1.40, 1.76)): box('HeadBar%d' % i, (1.04, 0, z), (.045, 1.44, .06), M_CHAS, .005)
tube('SuctionPipe', [(-1.30, -.25, .98), (-1.42, -.25, .98), (-1.60, -.25, 1.00)], .038, M_SS, bez=False); box('SuctionFoot', (-1.40, -.25, .915), (.06, .06, .09), M_CHAS, .004)
# #endregion

# #region ---- the cabinet at the back: reel bay (left), pump panel (right), fill point behind the left side door
C0, C1, CZ0, CZ1, CY = -1.97, -1.42, .875, 1.90, .93
box('Cab_Floor', ((C0 + C1) / 2, 0, .889), (C1 - C0, 2 * CY, .032), M_PANEL, .003, 1); box('Cab_Roof', ((C0 + C1) / 2 - .02, 0, 1.885), (C1 - C0 + .05, 2 * CY + .01, .032), M_BODY, .008, 2); box('Cab_Front', (C1 - .010, 0, 1.388), (.020, 2 * CY, 1.0), M_PANEL, .003, 1)
wl = box('Cab_WallL', ((C0 + C1) / 2, CY - .010, 1.388), (C1 - C0, .020, .99), M_BODY, .003, 1); box('Cab_WallR', ((C0 + C1) / 2, -CY + .010, 1.388), (C1 - C0, .020, .99), M_BODY, .003, 1)
PD = rnd([(-1.90, 1.00), (-1.50, 1.00), (-1.50, 1.78), (-1.90, 1.78)], .03, 4); c_ = rawprism('c', PD, 'y', .5, 1.3); c_.data.materials.clear(); c_.data.materials.append(M_BODY); ALL.append(c_); cut(wl, c_)
for n, y in (('A', .56), ('B', -.01)): box('Cab_Div' + n, ((C0 + C1) / 2 + .01, y, 1.388), (C1 - C0 - .03, .018, .985), M_PANEL, .002, 1)
box('PumpPanel', (-1.80, -.470, 1.388), (.016, .905, .99), M_PANEL, .003, 1)
pd = K.finish(rawprism('PanelDoorL', grow(PD, -.004), 'y', .9165, .9345), M_BODY, 30); ALL.remove(pd) if ALL.count(pd) > 1 else None
box('PanelDoorL_Band', (-1.70, .936, 1.16), (.36, .006, .12), M_MAIN, .001, 1); box('PanelDoorL_Cup', (-1.84, .936, 1.42), (.06, .010, .14), M_PLAST, .004); box('PanelDoorL_Handle', (-1.84, .944, 1.42), (.022, .014, .11), M_CHROME, .004)
text('PanelDoorL_Word', 'FILL', (-1.66, .9355, 1.60), .07, 'y+', M_SBLUE)
for i, z in enumerate((1.12, 1.66)): cyl('PanelDoorL_Hinge%d' % i, (-1.497, .938, z), .010, .09, 'z', M_STEEL, 20)
piv('PanelDoorL', (-1.497, .938, 1.39)); ik('Handle_PanelDoorL', (-1.84, .955, 1.42)); ik('Stand_PanelDoorL', (-1.95, 1.50, 0))
tube('FillPipe', [(-1.62, .57, 1.05), (-1.62, .70, 1.05), (-1.62, .78, 1.15), (-1.62, .80, 1.24)], .032, M_SS); cyl('FillValve', (-1.62, .80, 1.26), .045, .07, 'z', M_BRASS, 32, .006); cyl('FillCoupling', (-1.62, .80, 1.335), .040, .09, 'z', M_SS, 36, .006); cyl('FillCap', (-1.62, .80, 1.395), .048, .04, 'z', M_SBLUE, 36, .008); tube('FillChain', [(-1.62, .80, 1.41), (-1.66, .78, 1.46), (-1.72, .70, 1.44), (-1.74, .568, 1.46)], .004, M_STEEL); ik('FillCoupling', (-1.62, .80, 1.41))
tube('Lever_Fill', [(-1.655, .80, 1.26), (-1.76, .80, 1.26)], .010, M_STEEL, bez=False); cyl('Lever_Fill_Knob', (-1.77, .80, 1.26), .016, .05, 'x', M_RED, 24, .005); piv('Lever_Fill', (-1.62, .80, 1.26)); ik('Lever_Fill', (-1.77, .80, 1.26))
# the reel: 14 m of drinking-water hose, 28 mm outside
RC = (-1.67, .275, 1.42); HR = .014; HOSEM, RF, HW = reel('HoseReel', RC, HR, .13, 2, 7, M_HB)
for e, y in (('A', .435), ('B', .115)): box('ReelStand' + e, (RC[0], y, 1.17), (.10, .022, .56), M_CHAS, .005)
cyl('ReelMotor', (RC[0], .485, RC[2]), .060, .11, 'y', M_DARK, 32, .008); box('ReelMotor_Foot', (RC[0], .485, RC[2]), (.06, .155, .05), M_CHAS, .004)
gx, gz = -1.955, 1.32
for e in (-.026, .026): cyl('Guide_V%d' % (e > 0), (gx, RC[1] + e, gz), .011, .10, 'z', M_STEEL, 24); cyl('Guide_H%d' % (e > 0), (gx, RC[1], gz + e), .011, .09, 'y', M_STEEL, 24)
for e in (-.055, .055): box('Guide_P%d' % (e > 0), (gx, RC[1], gz + e), (.034, .62, .014), M_CHAS, .003) if e < 0 else box('Guide_P1', (gx, RC[1], gz + e), (.034, .12, .014), M_CHAS, .003)
for e in (-.055, .055): box('Guide_S%d' % (e > 0), (gx, RC[1] + e, gz), (.030, .012, .118), M_CHAS, .003)
ik('HoseOut', (gx - .02, RC[1], gz)); cyl('ReelRewind', (-1.972, .50, 1.12), .024, .03, 'x', M_SBLUE, 28, .004); box('ReelRewind_Plate', (-1.957, .50, 1.12), (.02, .10, .12), M_FRM, .004); ik('Button_Rewind', (-1.99, .50, 1.12))
text('ReelRewind_Word', 'REEL IN', (-1.9675, .50, 1.165), .018, 'x-', M_SWHT, .0008)
# the coupling (3/4 inch twist ring, ISO 17775), in its capped cup beside the reel
hx, hy, hz = -2.015, .06, 1.30
box('CouplingHolder_Arm', (-1.965, hy, 1.385), (.10, .05, .025), M_CHAS, .004); box('CouplingHolder_Foot', (-1.965, hy - .035, 1.385), (.03, .10, .08), M_CHAS, .004); cyl('CouplingHolder_Cup', (hx, hy, 1.392), .042, .034, 'z', M_SBLUE, 40, .006)
CK.lathez('Coupling', [(0, -.11), (.016, -.11), (.018, -.085), (.022, -.075), (.022, -.02), (.030, -.012), (.030, .020), (.024, .026), (.024, .050), (0, .050)], (hx, hy, hz), M_ALU, 40); cyl('Coupling_Ring', (hx, hy, hz + .022), .036, .040, 'z', M_STEEL, 40, .005); cyl('Coupling_Dust', (hx, hy, hz + .060), .030, .03, 'z', M_SBLUE, 32, .004)
for j in range(3): turn(box('Coupling_Lug%d' % j, (hx + .038, hy, hz + .022), (.016, .014, .026), M_STEEL, .003), (hx, hy, hz), 'z', 120 * j)
tube('Coupling_Lever', [(hx, hy - .018, hz - .04), (hx, hy - .050, hz - .045), (hx, hy - .058, hz - .11)], .007, M_RED); cyl('Coupling_Grip', (hx, hy, hz - .075), .024, .05, 'z', M_SBLK, 32, .005)
cyl('Coupling_Ferrule', (hx, hy, hz - .125), .019, .04, 'z', M_STEEL, 32, .003); piv('Coupling', (hx, hy, hz)); ik('Coupling_Hand', (hx, hy, hz - .075)); ik('Coupling_Ring', (hx, hy, hz + .022)); ik('Coupling_Mouth', (hx, hy, hz + .075)); ik('Coupling_Hose', (hx, hy, hz - .145)); ik('Coupling_Lever', (hx, hy - .058, hz - .10))
tube('HoseTail', [(RC[0] - .03, RC[1], RC[2] + .171), (RC[0] - .15, RC[1], RC[2] + .10), (gx + .06, RC[1], gz + .02), (gx, RC[1], gz), (gx - .05, RC[1] - .015, gz - .04), (gx - .075, RC[1] - .06, gz - .18), (gx - .07, RC[1] - .16, gz - .24), (hx, hy + .01, hz - .22), (hx, hy, hz - .138)], HR - .0005, M_HB, 12)
# the pump panel
py = -.47
box('Panel_Screen', (-1.812, py - .18, 1.62), (.012, .24, .12), M_PLAST, .005); box('Panel_Lcd', (-1.819, py - .18, 1.62), (.004, .20, .085), M_LCD, .001, 1); text('Panel_LcdWord', 'LITRES', (-1.8205, py - .18, 1.635), .026, 'x-', M_SBLK, .0012); text('Panel_LcdNum', '000', (-1.8205, py - .18, 1.602), .030, 'x-', M_SBLK, .0012); ik('Panel_Screen', (-1.825, py - .18, 1.62))
gauge('Panel_Pressure', (-1.815, py + .12, 1.62), .062, 'x-'); gauge('Panel_Level', (-1.815, py + .29, 1.62), .062, 'x-'); text('Panel_PressureWord', 'BAR', (-1.8075, py + .12, 1.525), .028, 'x-', M_SBLK); text('Panel_LevelWord', 'TANK', (-1.8075, py + .29, 1.525), .028, 'x-', M_SBLK)
for i, (n, y, m) in enumerate((('PumpOn', -.30, M_SBLUE), ('PumpOff', -.18, M_SRED), ('Light', -.06, M_SWHT))): cyl('Button_' + n, (-1.818, py + y, 1.44), .024, .028, 'x', m, 28, .004); cyl('Button_%s_Ring' % n, (-1.810, py + y, 1.44), .032, .012, 'x', M_STEEL, 32, .003); ik('Button_' + n, (-1.835, py + y, 1.44)); text('Button_%s_Word' % n, {'PumpOn': 'PUMP', 'PumpOff': 'STOP', 'Light': 'LAMP'}[n], (-1.8075, py + y, 1.385), .024, 'x-', M_SBLK)
cyl('Button_EStop_Ring', (-1.812, py + .22, 1.40), .050, .014, 'x', M_YEL, 36, .003); cyl('Button_EStop', (-1.828, py + .22, 1.40), .033, .035, 'x', M_SRED, 36, .006); ik('Button_EStop', (-1.848, py + .22, 1.40))
tube('PanelPipe', [(-1.80, py - .30, 1.18), (-1.86, py - .30, 1.18), (-1.87, py - .24, 1.18), (-1.87, py + .24, 1.18), (-1.86, py + .30, 1.18), (-1.80, py + .30, 1.18)], .026, M_SS); cyl('PanelValve', (-1.87, py, 1.18), .045, .10, 'y', M_BRASS, 32, .006)
tube('Lever_Valve', [(-1.895, py, 1.18), (-1.925, py, 1.21), (-1.925, py, 1.33)], .009, M_STEEL, bez=False); cyl('Lever_Valve_Knob', (-1.925, py, 1.345), .020, .05, 'z', M_SBLUE, 24, .006); piv('Lever_Valve', (-1.87, py, 1.18)); ik('Lever_Valve', (-1.925, py, 1.345))
box('PanelSign', (-1.812, py, 1.055), (.010, .78, .09), M_MAIN, .003, 1); text('PanelSign_Word', 'CLEAN HANDS AND GLOVES - CAP ON WHEN NOT IN USE', (-1.8165, py, 1.055), .026, 'x-', M_SWHT, .0012)
box('GloveBox', (-1.87, py - .34, 1.31), (.13, .14, .10), M_SBLUE, .012); text('GloveBox_Word', 'GLOVES', (-1.9345, py - .34, 1.31), .026, 'x-', M_SWHT, .0012); ik('GloveBox', (-1.94, py - .34, 1.33))
box('WorkLamp_Body', (-2.0, 0, 1.935), (.10, .30, .075), M_PLAST, .012); box('WorkLamp_Foot', (-1.96, 0, 1.91), (.10, .12, .035), M_CHAS, .004); box('WorkLamp_Lens', (-2.052, 0, 1.935), (.008, .26, .05), M_WHT, .002)
for s, sn in ((1, 'L'), (-1, 'R')): box('CabTop_HiVis' + sn, (-2.0155, s * .62, 1.885), (.006, .40, .026), M_HVR, .001, 1)
text('RoofWord', 'DRINKING WATER', (-2.015, 0, 1.885), .022, 'x-', M_SBLUE, .0012)
# #endregion

# #region ---- work platform at the back: floor 1.00, two steps on the left, rails, sprung gate
PX0, PX1, PY0, PY1, PZ = -2.74, -1.985, -.95, .46, 1.0
box('Platform', ((PX0 + PX1) / 2, (PY0 + PY1) / 2, PZ - .015), (PX1 - PX0, PY1 - PY0, .030), M_DECK, .004, 1); box('Platform_EdgeB', (PX0 + .010, (PY0 + PY1) / 2, PZ + .035), (.018, PY1 - PY0 - .01, .10), M_YEL, .003, 1); box('Platform_EdgeR', ((PX0 + PX1) / 2, PY0 + .010, PZ + .035), (PX1 - PX0 - .03, .018, .10), M_YEL, .003, 1)
for i, y in enumerate((-.80, -.37, .37)):
    box('Platform_Beam%d' % i, ((PX0 + PX1) / 2, y, PZ - .06), (PX1 - PX0 - .04, .05, .065), M_CHAS, .004); box('Platform_Post%d' % i, (-2.70, y, .735), (.06, .05, .43), M_CHAS, .004); box('Platform_PostF%d' % i, (-2.02, y, .91), (.06, .05, .09), M_CHAS, .004)
box('Platform_CrossF', (-2.02, -.17, .86), (.06, 1.36, .05), M_CHAS, .004); box('Platform_CrossFArm_L', (-1.97, .37, .80), (.16, .055, .12), M_CHAS, .004); box('Platform_CrossFArm_R', (-1.97, -.37, .80), (.16, .055, .12), M_CHAS, .004)
z0, z1 = PZ - .01, 2.0
for i, (x, y) in enumerate(((PX0 + .022, PY0 + .022), (PX0 + .022, -.25), (PX0 + .022, PY1 - .022), (PX1 - .06, PY0 + .022))): rail('RailPost%d' % i, [(x, y, z0), (x, y, z1)])
for i, z in enumerate((1.50, 1.995)): rail('RailBack%d' % i, [(PX0 + .022, PY0 + .022, z), (PX0 + .022, PY1 - .022, z)]); rail('RailRight%d' % i, [(PX0 + .022, PY0 + .022, z), (PX1 - .06, PY0 + .022, z)])
ik('Rail_Back', (PX0 + .022, 0, 2.0)); ik('Rail_Right', (-2.35, PY0 + .022, 2.0))
G = (PX0 + .022, PY1 - .022); rail('GatePlatform', [(G[0] + .05, G[1], 1.46), (-2.03, G[1], 1.46), (-2.03, G[1], 1.92), (G[0] + .05, G[1], 1.92)], r=.015)
for i, z in enumerate((1.46, 1.92)): cyl('GatePlatform_Hinge%d' % i, (G[0] + .022, G[1], z), .026, .06, 'z', M_DARK, 24, .004); tube('GatePlatform_Link%d' % i, [(G[0] + .03, G[1], z), (G[0] + .06, G[1], z)], .014, M_DARK, bez=False)
piv('GatePlatform', (G[0], G[1], 1.69)); ik('Handle_GatePlatform', (-2.06, G[1], 1.92))
for i, (z, ya, yb) in enumerate(((.355, .74, .965), (.68, .50, .76))): box('PlatStep%d' % i, (-2.31, (ya + yb) / 2, z), (.60, yb - ya, .035), M_DECK, .005, 1); ik('Step_Platform_%d' % i, (-2.31, (ya + yb) / 2, z + .018))
for e, x in ((0, -2.018), (1, -2.602)): K.finish(rawprism('PlatStep_Side%d' % e, [(.455, .975), (.455, .55), (.72, .33), (.97, .33), (.97, .40), (.78, .72), (.52, .975)], 'x', x - .008, x + .008), M_CHAS, 30)
box('PlatStep_Cross', (-2.31, .48, .93), (.60, .05, .05), M_CHAS, .004)
for e, x in ((0, -2.030), (1, -2.590)): tube('GrabPlatform_%s' % 'FR'[e], [(x, .93, .39), (x, .93, 1.20), (x, .80, 1.50), (x, .56, 1.72), (x, .50, 1.72), (x, .47, 1.66), (x, .455, 1.0)], .015, M_YEL); ik('Grab_Platform_' + 'FR'[e], (x, .93, 1.10))
ik('Stand_Steps', (-2.31, 1.55, 0)); ik('Platform_In', (-2.31, .30, PZ)); ik('Platform_Work', (-2.45, -.25, PZ)); ik('PanelStand', (-2.32, -.47, PZ)); ik('ReelStand', (-2.32, .20, PZ))
ik('NamePanel_Rear', (-2.765, -.35, 1.25)); box('NamePanel_Rear', (PX0 - .004, -.36, 1.28), (.010, 1.06, .20), M_NAME, .002, 1)
for i, y in enumerate((-.86, .12)):
    for k, z in enumerate((1.215, 1.345)): box('NameClip%d_%d' % (i, k), (PX0 + .008, y, z), (.016, .03, .03), M_DARK, .002, 1)
for i, y in enumerate((-.88, -.25, .37)): rail('NameTube%d' % i, [(PX0 + .022, y, 1.15), (PX0 + .022, y, 1.50)], r=.012)
rail('NameRail0', [(PX0 + .022, PY0 + .022, 1.20), (PX0 + .022, PY1 - .022, 1.20)], r=.012); rail('NameRail1', [(PX0 + .022, PY0 + .022, 1.36), (PX0 + .022, PY1 - .022, 1.36)], r=.012)
# #endregion

def extra(n):
    if n.startswith(('Cab_', 'Panel', 'PumpPanel', 'Button', 'Fill', 'Reel', 'Guide', 'Glove', 'CouplingHolder', 'WorkLamp', 'CabTop', 'RoofWord')): return 'Cabinet'
    if n.startswith(('Platform', 'Rail', 'Grab', 'PlatStep', 'Name')) and not n.startswith(('GrabHandle', 'GrabIn')): return 'PlatformParts'
    if n.startswith(('Tank', 'Manhole', 'Suction', 'Head', 'Board', 'SideBoard', 'Word', 'Drop', 'HandRail', 'Deck')): return 'TankParts'
finish_truck(['PanelDoorL', 'GatePlatform', 'HoseReel', 'HoseTail', 'CouplingHolder', 'Coupling', 'Lever_Fill', 'Lever_Valve'], lambda n: extra(n),
             [('PanelDoorL', (-1.497, .938, 1), 'z', (-30, -70, -110), ()), ('GatePlatform', (G[0], G[1], 1), 'z', (-30, -60, -80), ('RailPost2',))], [],
             {'hose': {'metres': round(HOSEM, 2), 'radius_cm': HR * 100, 'drum_layers': [14.4, 17.1], 'turns': 14}, 'tank_litres': 2500, 'platform_floor_cm': PZ * 100,
              'a320': {'door': '171AL', 'behind_nose_wheel_m': 26.2, 'left_m': .3, 'height_real_m': 2.6, 'height_model_m': 2.93}})
