# The caterers' things (to-do 212): a full-size galley trolley (0.302 x 0.81 x 1.03 m, the common full trolley), its door, its two brake pedals, its castor,
# a standard galley box (0.272 x 0.405 x 0.285) and its door. Own models, no brand. Each is ONE mesh (instanced, Nanite) with its own origin:
# Trolley / GalleyBox: on the ground under the middle, front (door) along +y; TrolleyDoor: its hinge at the bottom (closed place in sizes.json; the same mesh
# turned 180 deg is the back door); pedals: the axle; castor: its king pin on the trolley's underside; GalleyBoxDoor: its hinge (top edge).
#   blender -b --python Art/CateringKit/make_cateringkit.py
import bpy, math, os, sys, json
from mathutils import Vector, Matrix
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))); sys.path[:0] = [os.path.join(ROOT, 'Tools'), os.path.join(ROOT, 'Art', 'CleaningKit')]
import vehicle_kit as K
K.reset()
from vehicle_kit import box, cyl, tube, prism, ALL, IKS
import make_cleaningkit as CK
CK.join.__globals__['K'] = K
for _o in list(ALL): bpy.data.objects.remove(_o)
ALL.clear(); IKS.clear()
ART = os.path.join(ROOT, 'Art', 'CateringKit'); TEX = os.path.join(ROOT, 'Art', 'PushbackTug', 'Textures'); P = K.pmat
M_ALU = P('CG_Aluminium', (.72, .73, .75), .9, .4); M_PAN = P('CG_Panel', (.78, .79, .80), .1, .45); M_BLK = P('CG_BlackPlastic', (.03, .03, .033), 0, .5); M_RUB = P('CG_Rubber', (.03, .03, .03), 0, .9)
M_STL = P('CG_Steel', (.6, .61, .63), 1, .3); M_RED = P('CG_RedPedal', (.70, .03, .02), 0, .4); M_GRN = P('CG_GreenPedal', (.03, .42, .12), 0, .4); M_IN = P('CG_InsidePanel', (.55, .56, .58), .3, .5); M_CARD = P('CG_Card', (.95, .95, .93), 0, .6)
K.photo(M_ALU, TEX, 'Metal027', tile=.5, tint=(1.3, 1.3, 1.32)); K.photo(M_STL, TEX, 'Metal027', tile=.5); K.photo(M_RUB, TEX, 'Rubber004', tile=.3, tint=(.35, .35, .35), metal=0.0)
W, D, H, ZB = .302, .81, 1.03, .115
DONE = set()
def part(name, offs=(0, 0, 0)):
    """Everything made since the last part becomes one mesh; its origin at offs."""
    l = [o for o in ALL if o.name not in DONE]; l[0].name = 'tmp_' + name; CK.join(name, l); o = bpy.data.objects[name]; o.data.transform(Matrix.Translation(-Vector(offs))); o.data.update(); DONE.add(name)
# the trolley: base, two sides, middle, top, runners; open at both ends (the doors are parts of their own)
box('T_Base', (0, 0, ZB + .012), (W, D, .024), M_ALU, .004)
for s in (-1, 1):
    box('T_Side%d' % s, (s * (W / 2 - .008), 0, (ZB + .02 + .985) / 2), (.016, D - .012, .985 - ZB - .02), M_PAN, .003); box('T_Skirt%d' % s, (s * (W / 2 - .004), 0, ZB + .02), (.012, D - .004, .05), M_BLK, .003)
    for e in (-1, 1): box('T_Post%d%d' % (s, e), (s * (W / 2 - .011), e * (D / 2 - .030), (ZB + .985) / 2 + .01), (.024, .030, .985 - ZB - .01), M_ALU, .004)
    for k in range(13): box('T_Run%d_%02d' % (s, k), (s * (W / 2 - .022), 0, .19 + .059 * k), (.016, D - .10, .006), M_ALU, .001, 1)
    for i in range(3): tube('T_Vent%d%d' % (s, i), [(s * (W / 2 - .0005), -.30 + .30 * i, .93), (s * (W / 2 - .0005), -.20 + .30 * i, .93)], .006, M_BLK, bez=False)
    box('T_Rim%d' % s, (s * (W / 2 - .008), 0, 1.020), (.014, D - .03, .020), M_BLK, .005, 2); box('T_RimE%d' % s, (0, s * (D / 2 - .012), 1.020), (W - .012, .018, .020), M_BLK, .005, 2)
    for y in (-1, 1): box('T_CastorPlate%d%d' % (s, y), (s * .115, y * .335, ZB - .004), (.062, .075, .012), M_STL, .002, 1)
box('T_Mid', (0, 0, .55), (W - .02, .012, .85), M_IN, .0, 1); box('T_Top', (0, 0, 1.000), (W, D, .032), M_ALU, .008, 2)
for e in (-1, 1):                                                        # a handle bar set into the top at each end, the pedals' bracket under the base
    tube('T_Handle%d' % e, [(-.105, e * (D / 2 - .004), .990), (-.105, e * (D / 2 + .028), .990), (.105, e * (D / 2 + .028), .990), (.105, e * (D / 2 - .004), .990)], .009, M_BLK, bez=False)
    box('T_PedalBox%d' % e, (0, e * (D / 2 - .06), ZB - .014), (.13, .09, .034), M_BLK, .006); box('T_Bumper%d' % e, (0, e * (D / 2 - .004), ZB + .018), (W + .006, .016, .030), M_RUB, .005, 2)
part('Trolley')
# the door: hinge on its left edge (x -); latch at the top right
box('D_Panel', (0, D / 2 - .004, .555), (W - .006, .014, .83), M_PAN, .004); box('D_Frame', (0, D / 2 + .004, .955), (W - .010, .012, .030), M_ALU, .003); box('D_FrameB', (0, D / 2 + .004, .168), (W - .010, .012, .030), M_ALU, .003)
cyl('D_Hinge', (-W / 2 + .006, D / 2 + .004, .555), .007, .80, 'z', M_ALU, 20); box('D_LatchPlate', (.085, D / 2 + .005, .905), (.085, .012, .075), M_BLK, .004); box('D_Latch', (.085, D / 2 + .017, .905), (.060, .016, .022), M_RED, .005, 2)
box('D_CardFrame', (-.02, D / 2 + .0035, .70), (.18, .004, .13), M_ALU, .001, 1); box('D_Card', (-.02, D / 2 + .0050, .70), (.16, .005, .11), M_CARD, .001, 1)
part('TrolleyDoor', (-W / 2 + .006, D / 2 + .004, .14))
for nm, m in (('TrolleyPedalRed', M_RED), ('TrolleyPedalGreen', M_GRN)):
    box(nm + 'a', (0, .035, .004), (.052, .062, .010), m, .004, 2); box(nm + 'b', (0, .0, .0), (.030, .05, .010), M_STL, .002, 1); cyl(nm + 'c', (0, -.020, 0), .008, .050, 'x', M_STL, 20)
    for k in range(3): box(nm + 'r%d' % k, (0, .020 + .016 * k, .0100), (.040, .005, .004), M_RUB, .001, 1)
    part(nm, (0, -.020, 0))
box('C_Fork0', (-.020, -.010, -.035), (.005, .040, .062), M_STL, .001, 1); box('C_Fork1', (.020, -.010, -.035), (.005, .040, .062), M_STL, .001, 1); box('C_Top', (0, 0, -.006), (.046, .046, .010), M_STL, .002, 1); cyl('C_Wheel', (0, -.018, -.061), .043, .028, 'x', M_RUB, 48, .006); cyl('C_Hub', (0, -.018, -.061), .022, .034, 'x', M_BLK, 28); cyl('C_Axle', (0, -.018, -.061), .006, .050, 'x', M_STL, 16)
part('TrolleyCastor')
BW, BD, BH = .272, .405, .285
for s in (-1, 1): box('B_Side%d' % s, (s * (BW / 2 - .005), 0, BH / 2), (.010, BD - .010, BH - .006), M_ALU, .002); box('B_Grip%d' % s, (s * (BW / 2 - .001), 0, .20), (.008, .12, .030), M_BLK, .003)
box('B_Base', (0, 0, .006), (BW, BD, .012), M_ALU, .003); box('B_Top', (0, 0, BH - .006), (BW, BD, .012), M_ALU, .003); box('B_Back', (0, -BD / 2 + .006, BH / 2), (BW - .004, .010, BH - .004), M_PAN, .002); box('B_Lip', (0, BD / 2 - .006, .020), (BW - .02, .008, .024), M_ALU, .001, 1)
part('GalleyBox')
box('G_Panel', (0, 0, -.126), (BW - .026, .010, .236), M_PAN, .003); cyl('G_Hinge', (0, 0, -.004), .006, BW - .03, 'x', M_ALU, 18); box('G_LatchPlate', (0, .006, -.215), (.10, .008, .036), M_BLK, .003); box('G_Latch', (0, .013, -.215), (.05, .012, .016), M_RED, .004, 2); box('G_Card', (0, .0050, -.10), (.11, .004, .07), M_CARD, .0008, 1)
part('GalleyBoxDoor')
K.uv_cube()
for i, o in enumerate(ALL): assert o.data.materials and all(o.data.materials); o.location.x = i * 1.2
last = K.save_export(ROOT, os.path.join(ART, 'CateringKit.blend'), os.path.join(ART, 'cateringkit.glb'), os.path.join(ART, 'zfight-check.json'), 'CG', 'CateringKit')
A = {'note': 'metres, Blender axes (x right, y = the front with the door, z up) in the Trolley / GalleyBox; Unreal: (x, -y, z) * 100. In the glb the meshes stand 1.2 m apart along x.', 'items': {o.name: [round(v, 4) for v in o.dimensions] for o in ALL},
     'trolley': {'door_front': [-W / 2 + .006, D / 2 + .004, .14], 'door_back_yaw180': [W / 2 - .006, -D / 2 - .004, .14], 'door_opens_deg': 175, 'pedal_red': [-.034, D / 2 - .020, ZB - .016], 'pedal_green': [.034, D / 2 - .020, ZB - .016], 'pedal_down_deg': 14,
                 'pedals_back_yaw180': [[.034, -D / 2 + .020, ZB - .016], [-.034, -D / 2 + .020, ZB - .016]], 'castors': [[x * .115, y * .335, ZB - .010] for x in (-1, 1) for y in (-1, 1)], 'castor_radius': .043, 'castor_trail': .018,
                 'hand_L': [-.07, D / 2 + .028, .990], 'hand_R': [.07, D / 2 + .028, .990], 'push_stand_y': D / 2 + .55},
     'galley_box': {'door': [0, BD / 2 - .004, BH - .014], 'door_opens_deg': 110, 'grips': [[-BW / 2, 0, .20], [BW / 2, 0, .20]]}, 'flicker': last}
json.dump(A, open(os.path.join(ART, 'sizes.json'), 'w'), indent=1); print('DONE CG', {o.name: len(o.data.polygons) for o in ALL}, A['items'], last)
