# The opening mouth for a pet (owner, 2026-11-15: the dog pants - "putting his mouth out ... and then their mouth shut
# again", "see its teeth and its tongue", "the jaw very accurate ... according to the model"). Used by rig_pet.py when
# the pet's entry in pet_skeletons.json has a "mouth":
#   line    the lips' meeting line on the model's side view, [y, z] points from the front back to the corner
#   corner  where the lips end (y); behind it the cheek is whole skin
#   hinge   the jaw's hinge (y, z) - below and in front of the ear, at the end of the mouth line
# 1. the skin is cut along the lips' line (the faces below it, in front of the corner, are the lower jaw) and the cut's
#    edges are laid onto the line, so the lips part cleanly;
# 2. the inside is built to the muzzle's own width: palate and gums above, the floor of the mouth and gums below,
#    cheek walls and a back wall that stretch as the jaw opens, a dog's teeth (per side: upper 3 incisors, canine,
#    4 premolars, 2 molars; lower 3 incisors, canine, 4 premolars, 3 molars - the big carnassials upper P4 and lower
#    M1), and a tongue in three parts that can hang out over the lower teeth;
# 3. weights: the lower jaw's skin to 'jaw' (fading out behind the corner towards the throat), the inside's upper
#    half to 'head', lower half to 'jaw', the tongue to tongue_01..03.
# Colours by the mesh's colour attribute 'MouthColour' on its own material slot (M_<Name>_Mouth).
import bmesh, math
import numpy as np
from mathutils import Vector

TEETH_UP = [  # (where along the row 0 front .. 1 back, crown height %, crown length %, kind)
    (0.00, 0.55, 0.30, 'i'), (0.05, 0.60, 0.30, 'i'), (0.10, 0.75, 0.35, 'i'), (0.22, 2.10, 0.75, 'c'),
    (0.33, 0.45, 0.45, 'p'), (0.43, 0.70, 0.70, 'p'), (0.54, 0.80, 0.85, 'p'), (0.69, 1.15, 1.35, 'p'),
    (0.84, 0.65, 0.90, 'm'), (0.95, 0.45, 0.55, 'm')]
TEETH_LO = [
    (0.02, 0.50, 0.28, 'i'), (0.065, 0.55, 0.28, 'i'), (0.11, 0.65, 0.32, 'i'), (0.17, 1.80, 0.70, 'c'),
    (0.32, 0.40, 0.45, 'p'), (0.42, 0.60, 0.65, 'p'), (0.52, 0.70, 0.75, 'p'), (0.62, 0.80, 0.85, 'p'),
    (0.75, 1.05, 1.45, 'm'), (0.89, 0.60, 0.80, 'm'), (0.97, 0.35, 0.45, 'm')]
C_TOOTH = (0.93, 0.90, 0.80, 1); C_GUM = (0.30, 0.10, 0.11, 1); C_PALATE = (0.45, 0.17, 0.19, 1)
C_INSIDE = (0.20, 0.05, 0.06, 1); C_TONGUE = (0.78, 0.33, 0.38, 1)


def lineZ(line, y):
    ys = [p[0] for p in line]; zs = [p[1] for p in line]
    return float(np.interp(y, ys, zs))


def build(ob, M, P, U, NAME):
    """ob: the pet's mesh (rest pose); M: the mouth entry; P: % -> metres; U: metres per %. Returns the vertex sets
    for the weights: {'jaw_skin': {i: w}, 'upper': [...], 'lower': [...], 'blend': {i: t}, 'tongue': {i: (bone, w)}}"""
    line, corner, hinge = M['line'], M['corner'], M['hinge']
    y0 = P([0, 0, 0]).y
    def pc(v): return (v.y - y0) / U, v.z / U                 # metres -> (y %, z %)
    me = ob.data
    bm = bmesh.new(); bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    # --- 1. the cut along the lips
    def lower(f):
        y, z = pc(f.calc_center_median())
        return y < corner and z < lineZ(line, y)
    def near(f):
        y, z = pc(f.calc_center_median())
        return y < corner + 0.5 and abs(z - lineZ(line, min(y, corner))) < 3.0
    low = {f.index for f in bm.faces if near(f) and lower(f)}
    cut = [e for e in bm.edges if len(e.link_faces) == 2 and near(e.link_faces[0]) and
           ((e.link_faces[0].index in low) != (e.link_faces[1].index in low))]
    cutv = {v for e in cut for v in e.verts}
    for v in cutv:                                            # the cut laid onto the line: clean lips
        y, z = pc(v.co)
        if y < corner - 0.3: v.co.z = lineZ(line, y) * U
    # the mouth's own middle line and width from the lips' two edges (the owner's dog holds its head turned ~14 deg to
    # its left: the mouth's middle runs 3 % off the body's centre line at the nose, 1 % at the hinge)
    lp = np.array([((v.co.y - y0) / U, v.co.x / U) for v in cutv])
    bins = np.arange(lp[:, 0].min(), min(lp[:, 0].max(), corner), 1.2)
    bc, bm_, bw = [], [], []
    for b0 in bins:
        m = (lp[:, 0] >= b0) & (lp[:, 0] < b0 + 1.2)
        if m.sum() >= 2 and np.ptp(lp[m, 1]) > 1.0:
            bc.append(b0 + 0.6); bm_.append((lp[m, 1].max() + lp[m, 1].min()) / 2); bw.append(np.ptp(lp[m, 1]) / 2)
    CX = np.polyfit(bc, bm_, 1)
    def cx(y): return np.polyval(CX, y) if isinstance(y, np.ndarray) else float(np.polyval(CX, y))
    def lipw(y): return float(np.interp(y, bc, bw))
    print('MOUTH middle line x = %.3f y + %.2f (%%), half widths' % tuple(CX), [round(float(w), 2) for w in bw])
    bmesh.ops.split_edges(bm, edges=cut)
    bm.verts.index_update(); bm.faces.ensure_lookup_table(); bm.verts.ensure_lookup_table()
    jaw_skin = {}
    for f in bm.faces:
        if f.index in low:
            for v in f.verts: jaw_skin[v.index] = 1.0
    # behind the corner the skin under the jaw follows part way, fading towards the throat and the hinge
    for v in bm.verts:
        if v.index in jaw_skin: continue
        y, z = pc(v.co)
        lz = lineZ(line, min(y, corner))
        if corner - 0.5 <= y < hinge[0] + 1.5 and z < lz - 0.2 and z > lz - 7:
            w = max(0.0, min(1.0, (hinge[0] + 1.5 - y) / (hinge[0] + 1.5 - corner)))
            w *= max(0.0, min(1.0, (z - (lz - 7)) / 3.0))
            if w > 0.02: jaw_skin[v.index] = w
    nskin = len(bm.verts)
    # --- the muzzle's inner half width along the lips (from the skin itself, a lip's thickness in)
    co = np.array([v.co[:] for v in bm.verts]); yy = (co[:, 1] - y0) / U; zz = co[:, 2] / U
    def halfw(y): return lipw(min(y, corner - 0.3))         # the lips' own edge: exactly the inside of the lips
    # the inside kept inside the skin (owner's model: the upper lip hangs over a thin lower jaw - the chin is only ~1 %
    # below the lips): the lips' front, and below the line the lower jaw's own depth and width, measured on the skin
    m = (abs(zz - lineZ(line, line[0][0] + 1)) < 0.5) & (np.abs(co[:, 0] / U - cx(yy)) < 0.6)
    yfront = float(yy[m].min()) if m.any() else line[0][0]
    yf = yfront + 1.0; yb = corner + 0.4
    ys = np.linspace(yf, yb, 40)
    hw = np.array([max(0.5, halfw(min(y, corner)) - 0.45) for y in ys])
    print('MOUTH lips half width front..back', [round(float(h), 2) for h in hw[::8]])
    def chin(y):                                            # how deep the lower jaw is under the line at y
        lz = lineZ(line, min(y, corner)); mm = (abs(yy - y) < 0.6) & (np.abs(co[:, 0] / U - cx(y)) < 0.8) & (zz < lz)
        return float(lz - zz[mm].min()) if mm.any() else 1.0
    def lowhw(y):                                           # the lower jaw's half width just under the line
        return halfw(y) + 0.75
    DEEP = max(0.45, min(chin(y) for y in ys[::4]) - 0.35)  # the inside's floor stays this far under the line
    LOWK = max(0.5, min(1.0, np.median([(lowhw(min(y, corner)) - 0.45) / max(0.5, halfw(min(y, corner)) - 0.75) for y in ys[::4]])))
    print('MOUTH front %.1f %%, lower jaw depth room %.2f %%, lower width x%.2f' % (yfront, DEEP, LOWK))
    # the row as one curve per side, front crossing included: arc points (x %, y %)
    def row(side):
        pts = [(cx(yf - 0.35), yf - 0.35)] + [(cx(y) + side * w, y) for w, y in zip(hw, ys)]
        L = [0.0]
        for a, b in zip(pts, pts[1:]): L.append(L[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
        return pts, np.array(L) / L[-1]
    def at(side, t):
        pts, L = row(side); x = np.interp(t, L, [p[0] for p in pts]); y = np.interp(t, L, [p[1] for p in pts])
        i = min(len(pts) - 2, int(np.searchsorted(L, t)))
        d = Vector((pts[i + 1][0] - pts[max(0, i - 1)][0], pts[i + 1][1] - pts[max(0, i - 1)][1], 0)).normalized()
        return float(x), float(y), d
    cy = (yf + yb) / 2
    def inward(x, y, d):                                    # across the row, towards the middle of the mouth
        n = Vector((-d.y, d.x, 0))
        return n if n.dot(Vector((cx(cy) - x, cy - y, 0))) > 0 else -n
    col = bm.loops.layers.color.get('MouthColour') or bm.loops.layers.color.new('MouthColour')
    for f in bm.faces:
        for l in f.loops: l[col] = (1, 1, 1, 1)
    upper, lowerv, blend, tongue = [], [], {}, {}
    mouth_faces = []
    def V(x, y, z): return bm.verts.new(Vector((x * U, y0 + y * U, z * U)))
    def face(vs, c):
        try: f = bm.faces.new(vs)
        except ValueError: return None
        for l in f.loops: l[col] = c
        mouth_faces.append(f); return f
    # --- 2a. the teeth: each a crown leaning a little back, its root hidden in the gum
    def tooth(t, h, ln, kind, up, side):
        x, y, d = at(side, t)
        if not up: x = cx(y) + (x - cx(y)) * LOWK
        lz = lineZ(line, min(y, corner))
        base = lz + (0.55 if up else -min(0.4, DEEP * 0.6))  # the gum line: a little inside each lip
        s = -1 if up else 1                                 # crown towards the other jaw
        w = ln * (0.55 if kind in 'pm' else 0.8)            # across the row
        n = inward(x, y, d)                                 # across the row (to the tongue)
        segs = 6
        ring = []
        for k in range(segs):
            a = 2 * math.pi * k / segs
            off = d * (math.cos(a) * ln / 2) + n * (math.sin(a) * w / 2)
            ring.append(V(x + off.x, y + off.y, base - s * 0.25))
        if kind == 'c':                                     # a canine: one curved point
            mid = [V(x + (d * (math.cos(2 * math.pi * k / segs) * ln * 0.32)).x + (n * (math.sin(2 * math.pi * k / segs) * w * 0.32)).x,
                     y + (d * (math.cos(2 * math.pi * k / segs) * ln * 0.32)).y + (n * (math.sin(2 * math.pi * k / segs) * w * 0.32)).y + 0.15,
                     base + s * h * 0.55) for k in range(segs)]
            tip = [V(x, y + 0.35, base + s * h)]
        elif kind in 'pm' and ln > 0.5:                     # a blade: three cusps along the row, the middle highest
            mid = [V(x + (d * (math.cos(2 * math.pi * k / segs) * ln * 0.45)).x + (n * (math.sin(2 * math.pi * k / segs) * w * 0.3)).x,
                     y + (d * (math.cos(2 * math.pi * k / segs) * ln * 0.45)).y + (n * (math.sin(2 * math.pi * k / segs) * w * 0.3)).y,
                     base + s * h * (0.45 + 0.2 * abs(math.cos(2 * math.pi * k / segs)))) for k in range(segs)]
            tip = [V(x, y, base + s * h)]
        else:                                               # an incisor or a small tooth: a rounded chisel
            mid = [V(x + (d * (math.cos(2 * math.pi * k / segs) * ln * 0.4)).x + (n * (math.sin(2 * math.pi * k / segs) * w * 0.4)).x,
                     y + (d * (math.cos(2 * math.pi * k / segs) * ln * 0.4)).y + (n * (math.sin(2 * math.pi * k / segs) * w * 0.4)).y,
                     base + s * h * 0.75) for k in range(segs)]
            tip = [V(x, y, base + s * h)]
        for k in range(segs):
            k2 = (k + 1) % segs
            a, b, c, e = ring[k], ring[k2], mid[k2], mid[k]
            face([a, b, c, e] if not up else [e, c, b, a], C_TOOTH)
            face([e, c, tip[0]] if not up else [tip[0], c, e], C_TOOTH)
        for v in ring + mid + tip: (upper if up else lowerv).append(v)
    for side in (1, -1):
        for t, h, ln, kind in TEETH_UP: tooth(t, h, ln, kind, True, side)
        for t, h, ln, kind in TEETH_LO: tooth(t, h, ln, kind, False, side)
    # --- 2b. gums, palate, floor, cheeks and back wall along the row
    N = 24; ts = np.linspace(0, 1, N)
    rows = {}
    for side in (1, -1):
        r = {}
        for key, dz, inset in (('gum_up_out', 0.75, -0.2), ('gum_up_in', 0.75, 0.5), ('gum_lo_out', -0.6, -0.2), ('gum_lo_in', -0.6, 0.45),
                               ('cheek_up', 1.2, -0.3), ('cheek_lo', -1.0, -0.3)):
            vs = []
            for t in ts:
                x, y, d = at(side, t); lz = lineZ(line, min(y, corner)); n = inward(x, y, d)
                if dz < 0: x = cx(y) + (x - cx(y)) * LOWK; dz_ = -DEEP * (0.6 if key.startswith('gum') else 1.0)
                else: dz_ = dz
                vs.append(V(x + n.x * inset, y + n.y * inset, lz + dz_))
            r[key] = vs
        rows[side] = r
        up = r['gum_up_out'] + r['gum_up_in'] + r['cheek_up']; lo = r['gum_lo_out'] + r['gum_lo_in'] + r['cheek_lo']
        upper.extend(up); lowerv.extend(lo)
        for k in range(N - 1):
            fl = side > 0
            def q(a, b, c, d_, colr): face([a, b, c, d_] if fl else [d_, c, b, a], colr)
            q(r['gum_up_out'][k], r['gum_up_out'][k + 1], r['gum_up_in'][k + 1], r['gum_up_in'][k], C_GUM)
            q(r['gum_lo_in'][k], r['gum_lo_in'][k + 1], r['gum_lo_out'][k + 1], r['gum_lo_out'][k], C_GUM)
            q(r['cheek_up'][k], r['cheek_up'][k + 1], r['gum_up_out'][k + 1], r['gum_up_out'][k], C_GUM)
            q(r['gum_lo_out'][k], r['gum_lo_out'][k + 1], r['cheek_lo'][k + 1], r['cheek_lo'][k], C_GUM)
            # the cheek wall: stretches from the upper to the lower jaw as the mouth opens - only behind the canines,
            # the front stays open between the lips
            if ts[k] >= 0.24: q(r['cheek_lo'][k], r['cheek_lo'][k + 1], r['cheek_up'][k + 1], r['cheek_up'][k], C_INSIDE)
    # the palate and the floor between the two sides' inner gums, and the back wall
    for key, colr, upflag in (('gum_up_in', C_PALATE, True), ('gum_lo_in', C_INSIDE, False)):
        a, b = rows[1][key], rows[-1][key]
        lift = 0.5 if upflag else -0.3
        for k in range(N - 1):
            vs = [a[k], a[k + 1], b[k + 1], b[k]]
            face(vs if upflag else vs[::-1], colr)
    a_up, b_up, a_lo, b_lo = rows[1]['cheek_up'][-1], rows[-1]['cheek_up'][-1], rows[1]['cheek_lo'][-1], rows[-1]['cheek_lo'][-1]
    face([a_lo, a_up, b_up, b_lo], C_INSIDE)
    # (the ends of the inner rows join the back wall)
    face([rows[1]['gum_up_in'][-1], rows[1]['cheek_up'][-1], rows[-1]['cheek_up'][-1], rows[-1]['gum_up_in'][-1]], C_PALATE)
    face([rows[-1]['gum_lo_in'][-1], rows[-1]['cheek_lo'][-1], rows[1]['cheek_lo'][-1], rows[1]['gum_lo_in'][-1]], C_INSIDE)
    for v in rows[1]['cheek_up'] + rows[-1]['cheek_up']: blend[v] = 0.0
    for v in rows[1]['cheek_lo'] + rows[-1]['cheek_lo']: blend[v] = 1.0
    # --- 2c. the tongue: a flat rounded tongue on the floor of the mouth, in three parts front to back
    ty0, ty1 = yf + 0.25, yb - 0.4
    SL, RG = 16, 12
    rings = []
    for j in range(SL + 1):
        u = j / SL; y = ty0 + (ty1 - ty0) * u
        x, _, _ = at(1, min(1.0, max(0.0, (y - yf) / (yb - yf))))
        wdt = max(0.25, abs(x - cx(y)) * LOWK * 0.72) * (0.72 + 0.28 * math.sqrt(min(1.0, u * 5)))   # a broad rounded tip
        lz = lineZ(line, min(y, corner)) - DEEP * 0.45
        th = min(0.45, DEEP * 0.5) * (0.5 + 0.5 * math.sqrt(min(1.0, u * 4)))
        ring = []
        for k in range(RG):
            a = 2 * math.pi * k / RG
            ring.append(V(cx(y) + math.cos(a) * wdt, y, lz + math.sin(a) * th * (1.0 if math.sin(a) > 0 else 0.5) - th * 0.2))
        rings.append(ring)
        bone = 'tongue_03' if u < 0.33 else 'tongue_02' if u < 0.66 else 'tongue_01'
        for v in ring: tongue[v] = bone
    for j in range(SL):
        for k in range(RG):
            k2 = (k + 1) % RG
            face([rings[j][k], rings[j][k2], rings[j + 1][k2], rings[j + 1][k]], C_TONGUE)
    tip = V(cx(ty0 - 0.12), ty0 - 0.12, lineZ(line, ty0) - DEEP * 0.5); tongue[tip] = 'tongue_03'
    for k in range(RG): face([rings[0][(k + 1) % RG], rings[0][k], tip], C_TONGUE)
    bm.verts.index_update(); bm.faces.index_update()
    # the inside's faces on their own material slot
    mi = len(me.materials)
    for f in mouth_faces: f.material_index = mi
    out = dict(jaw_skin=jaw_skin, upper=[v.index for v in upper], lower=[v.index for v in lowerv],
               blend={v.index: t for v, t in blend.items()}, tongue={v.index: b for v, b in tongue.items()}, first_inside=nskin, cx=[float(CX[0]), float(CX[1])])
    bm.to_mesh(me); me.update(); bm.free()
    mat = __import__('bpy').data.materials.new('M_%s_Mouth' % NAME); mat.use_nodes = True
    nt = mat.node_tree; b = nt.nodes['Principled BSDF']; c = nt.nodes.new('ShaderNodeVertexColor'); c.layer_name = 'MouthColour'
    nt.links.new(c.outputs['Color'], b.inputs['Base Color']); b.inputs['Roughness'].default_value = 0.3
    me.materials.append(mat)
    print('MOUTH cut %d edges, skin points %d, inside points %d, teeth %d' % (len(cut), nskin, len(me.vertices) - nskin, 2 * (len(TEETH_UP) + len(TEETH_LO))))
    return out
