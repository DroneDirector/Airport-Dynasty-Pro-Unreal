# The cabin cleaners' carried things (to-do 212): rubbish bags (a roll, one open and limp in the hand, full and tied),
# a caddy with bottles and cloths, a cordless vacuum, a rubbish cart. Own models, no brands; real sizes: 60 l bag
# (full 0.42 x 0.42 x 0.60), caddy 400 x 280 x 220 (+ handle), cordless stick vacuum, cart 0.95 x 0.52 x 1.02.
# Each is ONE mesh (instanced, Nanite) with its origin where the hand holds it (bag full / limp: the neck; caddy: the
# handle's middle; vacuum: the grip; cart: on the ground under its middle, grip points in sizes.json).
# The cleaning van's script (Art/CleaningVan/make_cleaningvan.py) imports this file and stands copies in the van.
#   blender -b --python Art/CleaningKit/make_cleaningkit.py
import bpy, bmesh, math, os, sys, json
from mathutils import Vector, Matrix, noise
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'Tools'))
import vehicle_kit as K
from vehicle_kit import box, cyl, torus, tube, prism, cut, turn
TEX = os.path.join(ROOT, 'Art', 'PushbackTug', 'Textures')
HANDS = {'CartGripL': (-0.46, 0.17, 1.0), 'CartGripR': (-0.46, -0.17, 1.0), 'VacuumWand2ndHand': (0.20, 0.0, -0.17)}


def mats():
    p = K.pmat
    M = dict(bag=p('CK_BagPlastic', (0.02, 0.022, 0.025), 0, 0.32), blue=p('CK_BluePlastic', (0.03, 0.12, 0.42), 0, 0.45), grey=p('CK_GreyPlastic', (0.32, 0.33, 0.35), 0, 0.5),
             black=p('CK_BlackPlastic', (0.03, 0.03, 0.033), 0, 0.5), white=p('CK_WhitePlastic', (0.82, 0.83, 0.84), 0, 0.4), red=p('CK_RedPlastic', (0.6, 0.03, 0.03), 0, 0.4),
             green=p('CK_GreenLiquid', (0.08, 0.45, 0.16), 0, 0.25), pink=p('CK_PinkLiquid', (0.75, 0.2, 0.35), 0, 0.25), label=p('CK_Label', (0.75, 0.78, 0.8), 0, 0.6),
             cloth1=p('CK_ClothBlue', (0.10, 0.30, 0.62), 0, 0.95), cloth2=p('CK_ClothYellow', (0.85, 0.68, 0.12), 0, 0.95), steel=p('CK_Steel', (0.6, 0.61, 0.63), 1, 0.35),
             rub=p('CK_Rubber', (0.03, 0.03, 0.03), 0, 0.9))
    K.photo(M['steel'], TEX, 'Metal027', tile=0.5); K.photo(M['rub'], TEX, 'Rubber004', tile=0.3, tint=(.35, .35, .35), metal=0.0)
    return M


def lathez(name, prof, c, m, segs=28, smooth=45):
    """Turned round z through c: prof = open outline (r, z) from the bottom's middle to the top's middle (closed caps)."""
    bm = bmesh.new(); rings = []
    for r, z in prof:
        rings.append([bm.verts.new((c[0] + max(r, 1e-4) * math.cos(2 * math.pi * k / segs), c[1] + max(r, 1e-4) * math.sin(2 * math.pi * k / segs), c[2] + z)) for k in range(segs)])
    for A, B in zip(rings, rings[1:]):
        for k in range(segs): bm.faces.new((A[k], A[(k + 1) % segs], B[(k + 1) % segs], B[k]))
    bm.faces.new(rings[0][::-1]); bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return K.finish(K.link(name, bm), m, smooth)


def join(name, parts):
    for x in bpy.context.selected_objects: x.select_set(False)
    for o in parts:
        o.select_set(True); K.ALL.remove(o)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1: bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active; o.name = name; o.data.name = name; o.select_set(False)
    used = sorted({p.material_index for p in o.data.polygons}); ms = [o.data.materials[i] for i in used]
    mp = {u: i for i, u in enumerate(used)}; ix = [mp[p.material_index] for p in o.data.polygons]
    o.data.materials.clear()
    for m in ms: o.data.materials.append(m)
    for p, i in zip(o.data.polygons, ix): p.material_index = i
    K.ALL.append(o); return o


def loop_mesh(name, rings, m, cap=True, smooth=60):
    bm = bmesh.new(); R = [[bm.verts.new(v) for v in ring] for ring in rings]; n = len(R[0])
    for A, B in zip(R, R[1:]):
        for k in range(n): bm.faces.new((A[k], A[(k + 1) % n], B[(k + 1) % n], B[k]))
    if cap:
        bm.faces.new(R[0][::-1]); bm.faces.new(R[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return K.finish(K.link(name, bm), m, smooth)


def bag_full(M):
    bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=5, radius=1.0)
    for v in bm.verts:
        d = v.co.normalized(); a = math.atan2(d.y, d.x); h = (d.z + 1) / 2          # 0 bottom .. 1 neck
        s = math.sqrt(max(0.0, 1 - d.z * d.z))
        if h < .6: rr = 0.212 * s ** .5
        else: rr = 0.212 * (s ** .5 * (1 - K.ss(.6, .92, h)) + K.ss(.6, .92, h) * .10 * s ** .3)
        pleat = .010 * math.sin(a * 13 + 3 * noise.noise(d * 2)) * K.ss(.45, .85, h) * (1 - .7 * K.ss(.85, 1, h))
        lump = .022 * noise.noise(d * 3.1 + Vector((4, 1, 7))) + .009 * noise.noise(d * 9.0)
        n = max(rr + (pleat + lump) * (1 - K.ss(.80, .95, h)) * min(1, s * 3), .0)
        z = -0.56 + 0.56 * h
        if z < -.54: z = -.552 + .003 * noise.noise(d * 5)
        v.co = Vector((math.cos(a) * n, math.sin(a) * n, z))
    P = [K.finish(K.link('BagFull_b', bm), M['bag'], 70), torus('BagFull_t', (0, 0, -.004), .020, .009, 'z', M['bag'], 24, 10)]
    rings = []
    for i in range(9):                                                               # the gathered frill over the knot
        t = i / 8; z = .002 + .105 * t; rb = .016 + .052 * t ** 1.3
        rings.append([(rb * (1 + .28 * t * math.sin(k * 2 * math.pi * 7 / 56)) * math.cos(2 * math.pi * k / 56), rb * (1 + .28 * t * math.sin(k * 2 * math.pi * 7 / 56 + .6)) * math.sin(2 * math.pi * k / 56), z - .02 * t * t * math.cos(6 * math.pi * k / 56)) for k in range(56)])
    rings += [[(x * .93, y * .93, z + .001) for x, y, z in R] for R in rings[::-1]]
    P.append(loop_mesh('BagFull_f', rings, M['bag'])); return join('BagFull', P)


def bag_limp(M):
    """An open, empty bag hanging from the fist: its rim gathered at the origin, the folds falling 0.62 m."""
    rings = []; N = 64
    for i in range(25):
        t = i / 24; z = .05 - .67 * t; w = .018 + .16 * K.ss(0, .45, t) + .02 * t; ring = []
        for k in range(N):
            a = 2 * math.pi * k / N; f = math.sin(a * 5 + 2.4 * math.sin(t * 3)) * .028 * K.ss(.02, .5, t) + .010 * noise.noise(Vector((math.cos(a) * 2, math.sin(a) * 2, t * 5)))
            ring.append(((w + f) * math.cos(a), (w * .28 + f * .55) * math.sin(a), z - .03 * t * math.sin(a * 2 + 1)))
        rings.append(ring)
    return join('BagEmpty', [loop_mesh('BagEmpty_b', rings, M['bag'])])


def bag_roll(M):
    P = [lathez('R_a', [(.0, 0), (.058, 0), (.062, .006), (.062, .294), (.058, .30), (.0, .30)], (0, 0, 0), M['bag'], 40), lathez('R_b', [(.0, -.002), (.0165, -.002), (.0165, .302), (.0, .302)], (0, 0, 0), M['cloth2'], 24)]
    P.append(box('R_c', (.061, 0, .15), (.004, .112, .10), M['label'], .0015, 1)); return join('BagRoll', P)


def spray(n, x, y, M, liq):
    return [lathez(n + 'b', [(0, 0), (.036, 0), (.040, .006), (.040, .130), (.034, .155), (.016, .178), (.016, .195), (0, .195)], (x, y, 0), liq, 28),
            lathez(n + 'l', [(.039, .035), (.0412, .036), (.0412, .105), (.039, .106)], (x, y, 0), M['label'], 28),
            lathez(n + 'c', [(0, .187), (.0195, .187), (.0195, .214), (0, .214)], (x, y, 0), M['white'], 20),
            box(n + 'h', (x + .012, y, .232), (.074, .024, .040), M['white'], .007), box(n + 'n', (x + .052, y, .236), (.016, .018, .018), M['red'], .003),
            turn(box(n + 't', (x + .030, y, .196), (.010, .014, .050), M['red'], .003), (x + .03, y, .21), 'y', -22)]


def caddy(M):
    P = [box('C_tray', (0, 0, .07), (.40, .28, .14), M['blue'], .022, 3)]
    cut(P[0], box('C_h', (0, 0, .085), (.384, .264, .16), M['blue'], .016, 2))
    P += [box('C_rim' + str(s), (0, s * .139, .138), (.392, .012, .014), M['blue'], .005) for s in (-1, 1)] + [box('C_rimx' + str(s), (s * .199, 0, .138), (.012, .27, .014), M['blue'], .005) for s in (-1, 1)]
    P += [box('C_div', (0, 0, .072), (.39, .006, .124), M['blue'], .001, 1)]
    for s in (-1, 1): P.append(box('C_up' + str(s), (s * .183, 0, .20), (.010, .050, .16), M['blue'], .003))
    P.append(box('C_grip', (0, 0, .278), (.376, .030, .026), M['blue'], .010, 3))
    B = []
    for i, (x, y, q) in enumerate(((-.125, .072, 'green'), (-.035, .072, 'pink'), (.055, .072, 'blue'))): B += spray('C_s%d' % i, x, y, M, M[q])
    for o in B: o.data.transform(Matrix.Translation((0, 0, .0105)))
    P += B
    P += [box('C_cl%d' % i, (-.10 + .004 * (i % 2), -.072, .020 + .017 * i), (.17, .105, .0165), M['cloth1' if i % 2 else 'cloth2'], .006, 2) for i in range(7)]
    P += [lathez('C_pad', [(0, 0), (.044, 0), (.046, .003), (.046, .16), (.043, .165), (0, .165)], (.055, -.072, .0108), M['white'], 32), box('C_brush', (.14, -.075, .10), (.030, .026, .20), M['grey'], .009), box('C_bristle', (.14, -.075, .205), (.040, .03, .03), M['white'], .004)]
    P += [box('C_box', (.145, .072, .075), (.072, .10, .13), M['label'], .004)]
    for o in P: o.data.transform(Matrix.Translation((0, 0, -.278)))
    return join('Caddy', P)


def vacuum(M):
    """Cordless stick vacuum: x forward, the grip's middle at the origin; the floor head below and ahead of it."""
    g = lambda t: Vector((.10 + .62 * t, 0, -.02 - .90 * t))
    P = [box('V_grip', (0, 0, 0), (.034, .032, .12), M['black'], .012, 3), box('V_trig', (.020, 0, .022), (.012, .014, .03), M['red'], .003), box('V_bat', (-.004, 0, -.092), (.12, .078, .076), M['black'], .012, 3),
         box('V_top', (.045, 0, .068), (.14, .060, .044), M['grey'], .012, 3), cyl('V_motor', (.085, 0, .03), .046, .15, 'x', M['grey'], 36, .008), cyl('V_filter', (-.004, 0, .072), .040, .04, 'x', M['blue'], 36, .005),
         cyl('V_bin', (.125, 0, -.035), .050, .15, 'z', M['white'], 36, .008), cyl('V_binlid', (.125, 0, -.112), .052, .016, 'z', M['red'], 36, .004)]
    P += [tube('V_wand', [tuple(g(.06)), tuple(g(.95))], .017, M['steel'], bez=False), tube('V_collar', [tuple(g(.02)), tuple(g(.14))], .022, M['black'], bez=False), tube('V_collar2', [tuple(g(.88)), tuple(g(.97))], .023, M['black'], bez=False)]
    h = g(1.0)
    P += [tube('V_neck', [tuple(g(.94)), (h.x + .01, 0, h.z + .035), (h.x + .05, 0, h.z + .018)], .019, M['black']), box('V_head', (h.x + .085, 0, h.z + .010), (.11, .25, .055), M['grey'], .014, 3), box('V_soft', (h.x + .145, 0, h.z - .004), (.022, .24, .030), M['red'], .008, 2),
          cyl('V_w0', (h.x + .04, .115, h.z + .002), .022, .014, 'y', M['rub'], 24, .003), cyl('V_w1', (h.x + .04, -.115, h.z + .002), .022, .014, 'y', M['rub'], 24, .003)]
    return join('Vacuum', P)


def cart(M):
    P = [box('T_base', (0, 0, .14), (.80, .50, .030), M['grey'], .010), box('T_lip', (.385, 0, .165), (.024, .48, .05), M['grey'], .006)]
    for x in (-.32, .32):
        for y in (-.205, .205):
            k = '%d%d' % (x > 0, y > 0)
            P += [cyl('T_w' + k, (x, y, .06), .06, .032, 'y', M['rub'], 28, .006), cyl('T_h' + k, (x, y, .06), .030, .036, 'y', M['grey'], 20, .003),
                  box('T_f' + k, (x, y, .105), (.05, .055, .05), M['steel'], .004), box('T_p' + k, (x, y + math.copysign(.024, y), .08), (.028, .006, .07), M['steel'], .001, 1)]
    for y in (-.22, .22):
        k = str(int(y > 0))
        P += [tube('T_u' + k, [(-.37, y, .145), (-.37, y, .85), (-.40, y, .97), (-.46, y, 1.0)], .014, M['steel']), tube('T_v' + k, [(.36, y, .145), (.36, y, .93)], .014, M['steel'], bez=False), tube('T_s' + k, [(-.37, y, .84), (.36, y, .84)], .011, M['steel'], bez=False)]
    P += [tube('T_grip', [(-.46, -.225, 1.0), (-.46, .225, 1.0)], .017, M['rub'], bez=False), tube('T_e', [(.36, -.22, .925), (.36, .22, .925)], .013, M['steel'], bez=False), tube('T_e2', [(-.372, -.22, .84), (-.372, .22, .84)], .011, M['steel'], bez=False)]
    pr = [(.16, .30, .17), (.165, .335, .195), (.45, .345, .205), (.82, .350, .208), (.862, .362, .220), (.866, .372, .230), (.80, .378, .237), (.70, .382, .240), (.69, .379, .236), (.80, .374, .232), (.850, .358, .217), (.80, .345, .202), (.45, .340, .200), (.19, .325, .190)]
    rings = []                                                                      # its open sack, hung in the frame, folded over the rails
    for z, hx, hy in pr:
        ring = []
        for k in range(72):
            a = 2 * math.pi * k / 72; c, s = math.cos(a), math.sin(a); e = (abs(c) ** 8 + abs(s) ** 8) ** (-1 / 8); q = .005 * math.sin(a * 11) * (1 - K.ss(.75, .86, z))
            ring.append((-.005 + (hx + q) * c * e, (hy + q) * s * e, z))
        rings.append(ring)
    P.append(loop_mesh('T_sack', rings, M['bag'])); return join('CleanCart', P)


def make_all(M): return {o.name: o for o in (bag_full(M), bag_limp(M), bag_roll(M), caddy(M), vacuum(M), cart(M))}


if __name__ == '__main__':
    K.reset(); ART = os.path.join(ROOT, 'Art', 'CleaningKit'); M = mats(); IT = make_all(M); K.uv_cube()
    sizes = {}
    for i, (n, o) in enumerate(IT.items()):
        a, b = K.bbox(o); sizes[n] = {'min': [round(v, 3) for v in a], 'max': [round(v, 3) for v in b], 'tris': sum(len(p.vertices) - 2 for p in o.data.polygons), 'mats': [m.name for m in o.data.materials]}
        o.location.x = i * 2.0
    last = K.save_export(ROOT, os.path.join(ART, 'CleaningKit.blend'), os.path.join(ART, 'cleaningkit.glb'), os.path.join(ART, 'zfight-check.json'), 'KIT', 'CleaningKit')
    json.dump({'items': sizes, 'hands': HANDS, 'flicker': last}, open(os.path.join(ART, 'sizes.json'), 'w'), indent=1)
    print('DONE KIT', {n: (s['tris'], [round(b - a, 2) for a, b in zip(s['min'], s['max'])]) for n, s in sizes.items()})
