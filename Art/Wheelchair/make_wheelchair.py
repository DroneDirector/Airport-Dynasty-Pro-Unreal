# Airport wheelchair (to-do 212, for the ambulift): steel frame, 24 in wheels with spokes and push rims, castors, swing-away footrests, brakes, lap belt. Real sizes of the class.
# Axes: x forwards, y left, z up; metres; origin on the floor under the middle.   blender -b --python Art/Wheelchair/make_wheelchair.py [-- --pic DIR]
import os, sys, math
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path[:0] = [os.path.join(ROOT, 'Tools'), os.path.join(ROOT, 'Art', 'CleaningKit')]
import bpy, json
from mathutils import Vector, Matrix
import vehicle_kit as K
from vehicle_kit import box, cyl, lathe, torus, tube, ALL, IKS
K.reset()
import make_cleaningkit as CK
CK.join.__globals__['K'] = K
for _o in list(ALL): bpy.data.objects.remove(_o)
ALL.clear(); IKS.clear()
A_ = os.path.join(ROOT, 'Art'); TEX = os.path.join(A_, 'PushbackTug', 'Textures'); OWN = os.path.join(A_, 'PassengerStairs', 'Textures'); CART = os.path.join(A_, 'BaggageCart', 'Textures'); P = K.pmat
def mat(n, c, d, t, tile, tint, metal=0.0, **k): m = P('WC_' + n, c, metal, .5); K.photo(m, d, t, tile=tile, tint=tint, metal=metal, **k); return m
M_FR = mat('Frame_Paint', (.02, .10, .28), OWN, 'PaintGSE', .3, (.03, .14, .38), normal=.4); M_ST = mat('Steel', (.6, .6, .62), TEX, 'Metal027', .3, (1.0, 1.0, 1.02), 1.0); M_TY = mat('Tyre', (.08, .08, .08), TEX, 'Rubber004', .2, (.5, .5, .5)); M_PL = mat('BlackPlastic', (.04, .04, .04), OWN, 'Plastic012A', .2, (.5, .5, .5), normal=.6, rough_mul=1.6)
M_VI = mat('SeatVinyl', (.03, .03, .035), TEX, 'Leather026', .25, (.35, .35, .38)); M_GR = mat('GripRubber', (.04, .04, .04), TEX, 'Rubber001', .12, (.4, .4, .4)); M_BE = mat('BeltWebbing', (.03, .03, .03), CART, 'Fabric074', .08, (.06, .06, .07)); M_RD = mat('RedPlastic', (.6, .03, .02), OWN, 'PaintGSE', .2, (.65, .03, .02))
def ik(n, l): K.ik(n, l)
def piv(n, l): K.ik(n, l, prefix='PIV_')
def T(n, p, r=.011, m=None, b=True): return tube(n, p, r, m or M_FR, 8, b)
RX, RR, RY, CX, CR, SZ = -.14, .305, .285, .25, .095, .49
for s, sn in ((1, 'L'), (-1, 'R')):
    c = (RX, s * RY, RR); w = 'Wheel_' + sn; fy = s * .235
    torus(w + '_Tyre', c, RR - .013, .013, 'y', M_TY, 96, 16); torus(w + '_Rim', c, RR - .031, .0085, 'y', M_ST, 96, 12); torus(w + '_Hand', (RX, s * (RY + .030), RR), .262, .0085, 'y', M_ST, 96, 12); cyl(w + '_Hub', c, .022, .060, 'y', M_ST, 32, .004); cyl(w + '_Cap', (RX, s * (RY + .034), RR), .016, .014, 'y', M_PL, 28, .004); [cyl('%s_Flange%d' % (w, e > 0), (RX, s * RY + e * .024, RR), .032, .004, 'y', M_ST, 32) for e in (-1, 1)]
    for k in range(6): a = math.pi / 3 * k + .2; q = Vector((math.cos(a), 0, math.sin(a))); T('%s_Lug%d' % (w, k), [tuple(Vector(c) + q * .272), tuple(Vector(c) + q * .262 + Vector((0, s * .030, 0)))], .004, M_ST, False)
    for k in range(32): a0 = 2 * math.pi * k / 32; a1 = a0 + (1 if k % 4 < 2 else -1) * .50; e = 1 if k % 2 else -1; T('%s_Spoke%d' % (w, k), [(RX + .028 * math.cos(a1), s * RY + e * .024, RR + .028 * math.sin(a1)), (RX + .270 * math.cos(a0), s * RY, RR + .270 * math.sin(a0))], .0014, M_ST, False)
    piv(w, c); F = 'Frame_' + sn
    T(F + 'Back', [(RX - .02, fy, .20), (RX - .02, fy, .86), (RX - .05, fy, .905), (RX - .10, fy, .915), (RX - .20, fy, .915)], .0115, M_FR, False); T(F + 'Seat', [(RX - .02, fy, SZ - .02), (.24, fy, SZ - .02), (.275, fy, SZ - .05), (.275, fy, .24)], .0115, M_FR, False); T(F + 'Low', [(RX - .02, fy, .215), (.275, fy, .255)], .0115, M_FR, False); box(F + 'Axle', (RX, fy + s * .004, RR), (.06, .020, .16), M_ST, .004); cyl(F + 'AxlePin', (RX, s * .250, RR), .010, .05, 'y', M_ST, 24)
    T(F + 'ArmF', [(.14, fy, SZ - .02), (.14, fy, .69), (.11, fy, .715), (RX - .02, fy, .715)], .0105, M_FR, False); box(F + 'ArmPad', (-.01, fy, .735), (.27, .055, .028), M_PL, .010, 3); box(F + 'Guard', (.0, fy, .59), (.27, .006, .24), M_PL, .002, 1); ik('Arm_' + sn, (.03, fy, .75)); cyl(F + 'Grip', (RX - .165, fy, .915), .0165, .11, 'x', M_GR, 28, .004); ik('Push_' + sn, (RX - .165, fy, .915))
    k_ = 'CastorFork_' + sn; cyl(k_ + '_Stem', (CX + .025, fy, .225), .016, .06, 'z', M_ST, 28, .003); box(k_ + '_Crown', (CX + .015, fy, .202), (.07, .064, .018), M_ST, .005); [box('%s_Side%d' % (k_, e > 0), (CX - .005, fy + e * .027, .145), (.026, .006, .12), M_ST, .002, 1) for e in (-1, 1)]; cyl(k_ + '_Axle', (CX - .005, fy, CR), .006, .066, 'y', M_ST, 20)
    lathe('Castor_%s_Tyre' % sn, [(.066, -.013), (.072, -.018), (.087, -.017), (.095, -.008), (.095, .008), (.087, .017), (.072, .018), (.066, .013), (.066, -.013)], (CX - .005, fy, CR), M_TY, 56, 30); cyl('Castor_%s_Disc' % sn, (CX - .005, fy, CR), .068, .024, 'y', M_PL, 40, .003); piv('Castor_' + sn, (CX - .005, fy, CR)); piv(k_, (CX + .025, fy, .22))
    r = 'Footrest_' + sn; ly = s * .205; T(r + '_Tube', [(.300, fy, SZ - .05), (.335, fy, SZ - .05), (.375, ly, SZ - .09), (.455, s * .18, .17), (.47, s * .175, .145)], .0105, M_ST, False); box(r + '_Hook', (.292, fy, SZ - .05), (.030, .030, .03), M_PL, .005); box(r + '_Release', (.31, fy + s * .018, SZ - .035), (.05, .014, .018), M_RD, .004); piv(r, (.292, fy, SZ - .05)); ik(r, (.31, fy + s * .02, SZ - .03))
    f = 'FootPlate_' + sn; box(f + '_Pad', (.475, s * .095, .128), (.15, .150, .012), M_PL, .004, 2); [box('%s_Rib%d' % (f, k), (.425 + .025 * k, s * .095, .1355), (.008, .13, .005), M_GR, .001, 1) for k in range(5)]; cyl(f + '_Hinge', (.47, s * .175, .135), .014, .07, 'x', M_ST, 24, .003); piv(f, (.47, s * .175, .135)); ik('Foot_' + sn, (.455, s * .095, .138))
    b = 'Brake_' + sn; box(F + 'BrakeClamp', (.150, fy + s * .012, SZ - .02), (.05, .028, .034), M_ST, .004); T(b + '_Lever', [(.150, fy + s * .030, SZ - .015), (.185, fy + s * .034, SZ + .07), (.195, fy + s * .034, SZ + .11)], .006, M_ST); cyl(b + '_Knob', (.195, fy + s * .034, SZ + .12), .012, .035, 'z', M_RD, 24, .005); T(b + '_Shoe', [(.150, fy + s * .030, SZ - .015), (.158, s * RY, .459)], .006, M_ST, False); cyl(b + '_Pad', (.160, s * RY, .457), .010, .030, 'y', M_GR, 24, .003); piv(b, (.150, fy + s * .030, SZ - .015)); ik(b, (.195, fy + s * .034, SZ + .13))
    T('Belt_%s_Web' % sn, [(RX, s * .222, SZ + .03), (-.06, s * .20, SZ + .06), (.02, s * .12, SZ + .026)], .008, M_BE); box('Belt_%s_%s' % (sn, 'Buckle' if s > 0 else 'Tongue'), (.025, s * .10, SZ + .026), (.05, .05, .016), M_RD if s > 0 else M_ST, .004)
    ik('Strap_F' + sn, (.275, fy, .30)); ik('Strap_R' + sn, (RX - .02, fy, .26))
for e in (-1, 1): T('Frame_X%d' % (e > 0), [(.06 + e * .012, -.235 * e, .235), (.06 + e * .012, .235 * e, SZ - .025)], .0105, M_FR, False)
for x, n in ((-.10, 0), (.20, 1)): T('Frame_Tie%d' % n, [(x, -.235, SZ - .02), (x, .235, SZ - .02)], .009, M_ST, False)
se = box('Seat_Cushion', (.045, 0, SZ + .002), (.37, .435, .035), M_VI, .014, 4, cuts=6)
for v in se.data.vertices: v.co.z -= .014 * max(0, 1 - (v.co.y / .20) ** 2)
bk = box('Seat_Back', (RX - .012, 0, .70), (.030, .45, .36), M_VI, .012, 4, cuts=6)
for v in bk.data.vertices: v.co.x -= .020 * max(0, 1 - (v.co.y / .20) ** 2)
box('Seat_Pocket', (RX - .046, 0, .66), (.012, .34, .20), M_VI, .004, 2); ik('Seat', (-.02, 0, SZ + .01)); ik('Belt', (.025, 0, SZ + .03)); ik('Pusher', (-.80, 0, 0))
K.uv_cube(); K.checks('WC', ground_ok=('Wheel_', 'Castor_'), overhang_skip_prefix=('',))
if '--pic' in sys.argv:
    OUT = sys.argv[sys.argv.index('--pic') + 1]; sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'; sc.eevee.taa_render_samples = 8; w = bpy.data.worlds.new('W'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (.6, .68, .8, 1); sc.world = w
    so = bpy.data.objects.new('S', bpy.data.lights.new('S', 'SUN')); so.data.energy = 3; sc.collection.objects.link(so); so.rotation_euler = (.8, .1, .6); bc = bpy.data.objects.new('C', bpy.data.cameras.new('C')); sc.collection.objects.link(bc); sc.camera = bc; sc.render.resolution_x, sc.render.resolution_y = 480, 440
    bc.location = (1.7, 1.3, 1.2); bc.rotation_euler = (Vector((.05, 0, .45)) - bc.location).to_track_quat('-Z', 'Y').to_euler(); bc.data.lens = 45; sc.render.filepath = os.path.join(OUT, 'wc.png'); bpy.ops.render.render(write_still=True)
ART = os.path.join(A_, 'Wheelchair'); BL, GLB, ZF = (os.path.join(ART, f) for f in ('Wheelchair.blend', 'wheelchair.glb', 'zfight-check.json')); K.save_export(ROOT, BL, GLB, ZF, 'WC', 'Wheelchair'); G = {}
GR = [p + '_' + s for p in ('Wheel', 'CastorFork', 'Castor', 'Footrest', 'FootPlate', 'Brake', 'Belt') for s in 'LR'] + ['Frame', 'Seat']
for o in ALL: G.setdefault(next(g for g in sorted(GR, key=len, reverse=True) if o.name.startswith(g)), []).append(o)
for g, l in G.items(): l[0].name = 'tmp_' + g; CK.join(g, l)
for o in ALL: assert o.data.materials and all(o.data.materials) and {p.material_index for p in o.data.polygons} == set(range(len(o.data.materials))), o.name
last = K.save_export(ROOT, BL, GLB, ZF, 'WC joined', 'WheelchairGame')
def ue(v): return [round(v[0] * 100, 1), round(-v[1] * 100, 1), round(v[2] * 100, 1)]
lo = [min(K.bbox(o)[0][i] for o in ALL) for i in range(3)]; hi = [max(K.bbox(o)[1][i] for o in ALL) for i in range(3)]
json.dump({'size_m': [round(hi[i] - lo[i], 3) for i in range(3)], 'points_cm': {e.name: ue(e.location) for e in IKS}, 'parts': sorted(o.name for o in ALL), 'tris': sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in ALL), 'flicker': last}, open(os.path.join(ART, 'wheelchair-parts.json'), 'w'), indent=1)
json.dump({'points': {e.name: [round(c, 4) for c in e.location] for e in IKS}}, open(os.path.join(ROOT, 'Data', 'wheelchair-riders.json'), 'w'), indent=1); print('DONE WC', len(ALL), [round(hi[i] - lo[i], 3) for i in range(3)], round(lo[2], 4), last)
