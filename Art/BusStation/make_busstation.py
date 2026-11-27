# Chablais bus station - the STRUCTURE: the island platform and the glazed barrel vault over it.
# Our own design (docs/Bus-Station-Plan.md + docs/Pictures/busstation-blueprint/), nothing bought or copied.
# Real metres. Origin: the platform's west end, on its centreline; ground z = 0; +x east, +y to the terminal side.
# Every part is its own named object BusStn_<Group>_<Part> with its own pivot.
# The owner's rule (plan, "Nothing floats"): every arch foot carries down through a welded shoe, a base plate, four
# holding-down bolts, a concrete pedestal and a footing pad onto the foundation raft; the purlins, the glazing bars,
# the panes and the cable bracing run arch to arch so the vault is ONE connected structure; the gutters sit on
# modelled brackets off the arches and drain down real downpipes; every joint is a real overlap (plate, collar,
# flange, burial), never two surfaces grazing.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/BusStation/make_busstation.py
#   ... -- --render [--only=hall]   3840x2160 views to docs/Pictures/busstation-*.png
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import busstn_dims as D

RENDER = '--render' in sys.argv
QUICK = '--quick' in sys.argv
ONLY = [a[7:] for a in sys.argv if a.startswith('--only=')]   # --only=hall renders just that picture
bpy.ops.wm.read_factory_settings(use_empty=True)
rng = np.random.default_rng(11)

# ---------------------------------------------------------------- textures (our own, made here)
NPX = 512
yy, xx = np.mgrid[0:NPX, 0:NPX] / NPX


def blur(a, n=1):
    for _ in range(n):
        a = (a + np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 5
    return a


def save_img(name, rgb, nonc=False):
    h, w = rgb.shape[:2]
    im = bpy.data.images.new(name, w, h, alpha=False)
    px = np.ones((h, w, 4), np.float32)
    px[..., :3] = np.clip(rgb, 0, 1)
    im.pixels = px.ravel()
    im.filepath_raw = os.path.join(HERE, name + '.png')
    im.file_format = 'PNG'
    im.save()
    if nonc:
        im.colorspace_settings.name = 'Non-Color'
    return im


def normal_from(hgt, k):
    gy, gx = np.gradient(hgt)
    n = np.dstack((-gx * k, -gy * k, np.ones_like(hgt)))
    n /= np.linalg.norm(n, axis=2, keepdims=True)
    return n * 0.5 + 0.5


def plain(name, base, grain, k=3.0, streak=False):
    g = blur(rng.random((NPX, NPX)), 1)
    if streak:
        g = 0.6 * blur(np.repeat(rng.random((NPX, 1)), NPX, 1), 1) + 0.4 * g
    col = np.array(base)[None, None, :] * (1 - grain + 2 * grain * g[..., None])
    return save_img(name, col), save_img(name + '_N', normal_from(g, k), True)


def speckle(name, base, fleck, k=4.0, dens=0.26):
    g = blur(rng.random((NPX, NPX)), 1)
    m = (rng.random((NPX, NPX)) < dens).astype(float)
    m = blur(m, 1)
    col = np.array(base)[None, None, :] * (0.88 + 0.26 * g[..., None]) + np.array(fleck)[None, None, :] * m[..., None] * 0.35
    return save_img(name, col), save_img(name + '_N', normal_from(g * 0.6 + m * 0.5, k), True)


def ribbed(name, base, per=26):
    h = 0.5 + 0.5 * np.sin(yy * math.pi * 2 * per)
    col = np.array(base)[None, None, :] * (0.86 + 0.22 * h[..., None])
    return save_img(name, col), save_img(name + '_N', normal_from(h, 6.0), True)


def strands(name, base, per=60):
    h = 0.5 + 0.5 * np.sin((xx * 0.35 + yy) * math.pi * 2 * per)
    col = np.array(base)[None, None, :] * (0.8 + 0.4 * h[..., None])
    return save_img(name, col), save_img(name + '_N', normal_from(h, 7.0), True)


def glassy(name, base):
    g = blur(rng.random((NPX, NPX)), 3)
    col = np.array(base)[None, None, :] * (0.97 + 0.06 * g[..., None])
    return save_img(name, col), save_img(name + '_N', normal_from(g, 0.4), True)


T_WHITE = plain('T_BusStn_WhiteSteel', (0.90, 0.91, 0.92), 0.035, 1.2, True)
T_ALU = plain('T_BusStn_Alu', (0.70, 0.72, 0.75), 0.06, 1.8, True)
T_CABLE = strands('T_BusStn_Cable', (0.46, 0.47, 0.49))
T_CONC = speckle('T_BusStn_Concrete', (0.56, 0.56, 0.54), (0.70, 0.69, 0.66), 3.0, 0.18)
T_GRAN = speckle('T_BusStn_Granite', (0.40, 0.41, 0.43), (0.66, 0.67, 0.70), 5.0, 0.30)
T_KERB = speckle('T_BusStn_Kerb', (0.46, 0.46, 0.47), (0.63, 0.64, 0.66), 4.0, 0.24)
T_TACT = ribbed('T_BusStn_Tactile', (0.84, 0.84, 0.82))
T_DARK = plain('T_BusStn_DarkSteel', (0.20, 0.21, 0.23), 0.08, 2.5)
T_GLASS = glassy('T_BusStn_Glass', (0.86, 0.95, 0.90))   # faintly green, very light: a glass tint


def mat(name, tex, metal, rough, alpha=1.0, trans=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes['Principled BSDF']
    ti = nt.nodes.new('ShaderNodeTexImage'); ti.image = tex[0]
    nt.links.new(ti.outputs['Color'], b.inputs['Base Color'])
    tn = nt.nodes.new('ShaderNodeTexImage'); tn.image = tex[1]
    nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.6
    nt.links.new(tn.outputs['Color'], nm.inputs['Color'])
    nt.links.new(nm.outputs['Normal'], b.inputs['Normal'])
    b.inputs['Metallic'].default_value = metal
    b.inputs['Roughness'].default_value = rough
    b.inputs['Transmission'].default_value = trans
    b.inputs['Alpha'].default_value = alpha
    if alpha < 1.0:
        m.blend_method = 'BLEND'
        m.shadow_method = 'HASHED'
        m.use_backface_culling = False
        m.show_transparent_back = False      # one glass surface, not a milky stack
        m.use_screen_refraction = True
        m.refraction_depth = 0.024
        b.inputs['IOR'].default_value = 1.52
        b.inputs['Specular'].default_value = 0.72
        nm.inputs['Strength'].default_value = 0.08
    return m


M_STEEL = mat('BusStn_WhiteSteel', T_WHITE, 0.85, 0.30)
M_ALU = mat('BusStn_Alu', T_ALU, 1.0, 0.34)
M_CABLE = mat('BusStn_Cable', T_CABLE, 1.0, 0.42)
M_CONC = mat('BusStn_Concrete', T_CONC, 0.0, 0.80)
M_GRAN = mat('BusStn_Granite', T_GRAN, 0.0, 0.62)
M_KERB = mat('BusStn_Kerb', T_KERB, 0.0, 0.66)
M_TACT = mat('BusStn_Tactile', T_TACT, 0.0, 0.70)
M_DARK = mat('BusStn_DarkSteel', T_DARK, 0.9, 0.46)
M_GLASS = mat('BusStn_Glass', T_GLASS, 0.0, 0.025, 0.13, 0.95)

# ---------------------------------------------------------------- geometry helpers
ALL = []
NO_PIVOT = set()        # parts whose origin is set where they are built (the clock hands), not by the generic pass


def add_cube(bm, c, ax, ay, az):
    """A box: centre c, the three half-axis vectors ax, ay, az (so it can be turned any way)."""
    c, ax, ay, az = Vector(c), Vector(ax), Vector(ay), Vector(az)
    vs = [bm.verts.new(c + sx * ax + sy * ay + sz * az)
          for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
    f = ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3))
    for q in f:
        bm.faces.new([vs[i] for i in q])
    return bm


def box(bm, x0, y0, z0, x1, y1, z1):
    """An axis-aligned box from its two corners."""
    return add_cube(bm, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2),
                    ((x1 - x0) / 2, 0, 0), (0, (y1 - y0) / 2, 0), (0, 0, (z1 - z0) / 2))


def bar(bm, p, q, w, d, up=(0, 0, 1)):
    """A straight bar from p to q, w wide across, d deep along `up` (its section is w x d)."""
    p, q = Vector(p), Vector(q)
    t = (q - p)
    L = t.length
    t = t.normalized()
    u = Vector(up).normalized()
    if abs(u.dot(t)) > 0.98:
        u = Vector((1, 0, 0)) if abs(t.x) < 0.9 else Vector((0, 0, 1))
    s = t.cross(u).normalized()
    u = s.cross(t).normalized()
    return add_cube(bm, (p + q) / 2, t * (L / 2), s * (w / 2), u * (d / 2))


def tube(bm, pts, r, segs=16, cap=True):
    """A round tube along the path pts (capped ends, smooth round section)."""
    pts = [Vector(p) for p in pts]
    tans = []
    for i in range(len(pts)):
        a = pts[max(i - 1, 0)]
        b = pts[min(i + 1, len(pts) - 1)]
        tans.append((b - a).normalized())
    ref = Vector((0, 0, 1))
    if abs(ref.dot(tans[0])) > 0.9:
        ref = Vector((1, 0, 0))
    u = (ref - tans[0] * ref.dot(tans[0])).normalized()
    rings = []
    for i, p in enumerate(pts):
        t = tans[i]
        u = (u - t * u.dot(t))
        if u.length < 1e-6:
            u = Vector((1, 0, 0)) if abs(t.x) < 0.9 else Vector((0, 0, 1))
        u = u.normalized()
        v = t.cross(u)
        rings.append([bm.verts.new(p + u * (r * math.cos(a)) + v * (r * math.sin(a)))
                      for a in (2 * math.pi * k / segs for k in range(segs))])
    for i in range(len(rings) - 1):
        A, B = rings[i], rings[i + 1]
        for k in range(segs):
            bm.faces.new((A[k], A[(k + 1) % segs], B[(k + 1) % segs], B[k]))
    if cap:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    return bm


def obj(name, bm, m, bevel=0.0, smooth=True):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(o)
    o.data.materials.append(m)
    if bevel:
        md = o.modifiers.new('BEVEL', 'BEVEL')
        md.width = bevel
        md.segments = 2
        md.limit_method = 'ANGLE'
        md.angle_limit = math.radians(35)
        md.harden_normals = False
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.modifier_apply(modifier=md.name)
    if smooth:
        for p in o.data.polygons:
            p.use_smooth = True
        o.data.use_auto_smooth = True
        o.data.auto_smooth_angle = math.radians(32)
    ALL.append(o)
    return o


def P(t, dr=0.0):
    """A point on the arch's axis (dr = radial offset), as (y, z)."""
    return D.arch_pt(t, dr)


def P3(x, t, dr=0.0):
    y, z = D.arch_pt(t, dr)
    return (x, y, z)


ARCH_X = [i * D.BAY for i in range(D.N_ARCH)]
ROW_T = [D.row_t(k) for k in range(D.N_ROW + 1)]
RPO = D.RP - D.R                       # the purlin/glazing radius as an offset from R

# ---------------------------------------------------------------- 1  foundations and the island platform
bm = bmesh.new(); box(bm, -0.35, -7.7, D.RAFT_TOP - D.RAFT_T, D.PL_LEN + 0.35, 14.3, D.RAFT_TOP)
obj('BusStn_Found_Raft', bm, M_CONC)

bm = bmesh.new(); box(bm, 0.0, -3.95, -0.10, D.PL_LEN, 3.95, D.DECK_Z - 0.035)
obj('BusStn_Platform_Slab', bm, M_CONC, 0.01)

bm = bmesh.new(); box(bm, 0.0, -3.78, 0.10, D.PL_LEN, 3.78, D.DECK_Z)
obj('BusStn_Platform_Paving', bm, M_GRAN, 0.006)

for s, sn in ((-1, 'South'), (1, 'North')):
    ye = s * D.PL_HALF
    yi = s * (D.PL_HALF - D.KERB_W)
    bm = bmesh.new(); box(bm, 0.0, min(ye, yi), D.DECK_Z + 0.01 - D.KERB_DEEP, D.PL_LEN, max(ye, yi), D.DECK_Z + 0.01)
    obj('BusStn_Platform_Kerb' + sn, bm, M_KERB, 0.012)

    # tactile edge strip: a plate with corduroy ribs, bedded 15 mm into the paving
    y1 = s * (D.PL_HALF - D.KERB_W - D.TACT_OFF)
    y0 = y1 - s * D.TACT_W
    # on the south side the column pedestals stand on the platform: the strip runs round them
    segs = [(0.4, D.PL_LEN - 0.4)]
    if s < 0:
        segs = []
        xa = 0.55
        for xc in ARCH_X:
            if xc - 0.55 > xa:
                segs.append((xa, xc - 0.55))
            xa = xc + 0.55
        if D.PL_LEN - 0.55 > xa:
            segs.append((xa, D.PL_LEN - 0.55))
    bm = bmesh.new()
    for xa, xb in segs:
        box(bm, xa, min(y0, y1), D.DECK_Z - 0.015, xb, max(y0, y1), D.DECK_Z + D.TACT_T)
        for k in range(5):
            yr = min(y0, y1) + D.TACT_W * (k + 0.5) / 5
            tube(bm, [(xa, yr, D.DECK_Z + D.TACT_T), (xb, yr, D.DECK_Z + D.TACT_T)], 0.013, 10)
    obj('BusStn_Platform_Tactile' + sn, bm, M_TACT, 0.0)

    # slot drain: channel walls + sole + the two top plates, 25 mm slot between them, frame 5 mm proud
    yc = s * (D.PL_HALF - D.DRN_OFF)
    hw = D.DRN_W / 2
    zb = D.DECK_Z - D.DRN_D
    bm = bmesh.new()
    box(bm, 0.3, yc - hw, zb, D.PL_LEN - 0.3, yc - hw + 0.03, D.DECK_Z - 0.03)
    box(bm, 0.3, yc + hw - 0.03, zb, D.PL_LEN - 0.3, yc + hw, D.DECK_Z - 0.03)
    box(bm, 0.3, yc - hw + 0.005, zb, D.PL_LEN - 0.3, yc + hw - 0.005, zb + 0.045)
    for sg in (-1, 1):
        box(bm, 0.3, yc + sg * D.DRN_SLOT / 2, D.DECK_Z - 0.043, D.PL_LEN - 0.3, yc + sg * hw, D.DECK_Z + 0.005)
    obj('BusStn_Platform_Drain' + sn, bm, M_DARK, 0.004)

# ---------------------------------------------------------------- 2  the feet: pad, pedestal, shoe
for i, x in enumerate(ARCH_X):
    for s, sn in ((-1, 'S'), (1, 'N')):
        yf = D.COL_Y[s]
        tag = 'A%02d%s' % (i + 1, sn)
        h = D.FOOT_PAD / 2
        bm = bmesh.new(); box(bm, x - h, yf - h, -0.30, x + h, yf + h, D.FOOT_PAD_Z)
        obj('BusStn_Foot_Pad' + tag, bm, M_CONC, 0.02)

        h = D.PED_W / 2
        bm = bmesh.new(); box(bm, x - h, yf - h, 0.0, x + h, yf + h, D.PED_Z)
        obj('BusStn_Foot_Pedestal' + tag, bm, M_CONC, 0.02)

        # base plate, 4 holding-down bolts with nuts, the tube's welded shoe collar and two gussets
        h = D.PLATE_W / 2
        bm = bmesh.new()
        box(bm, x - h, yf - h, D.PED_Z - 0.01, x + h, yf + h, D.FOOT_Z)
        for dx in (-0.22, 0.22):
            for dy in (-0.22, 0.22):
                tube(bm, [(x + dx, yf + dy, D.PED_Z - 0.12), (x + dx, yf + dy, D.FOOT_Z + 0.030)], D.BOLT_OD / 2, 10)
                tube(bm, [(x + dx, yf + dy, D.FOOT_Z - 0.002), (x + dx, yf + dy, D.FOOT_Z + 0.028)], 0.036, 6)
        base = Vector((x, yf, D.FOOT_Z - 0.01))
        tube(bm, [base, base + Vector((0, 0, 0.32))], D.COL_R + 0.055, 18)            # welded shoe collar
        for sg in (-1, 1):
            add_cube(bm, (x + sg * (D.COL_R + 0.030), yf, D.FOOT_Z + 0.22),
                     (0.012, 0, 0), (0, 0.18, 0), (0, 0, 0.22))                       # gussets
            add_cube(bm, (x, yf + sg * (D.COL_R + 0.030), D.FOOT_Z + 0.22),
                     (0.18, 0, 0), (0, 0.012, 0), (0, 0, 0.22))
        obj('BusStn_Foot_Shoe' + tag, bm, M_STEEL, 0.006)

# ---------------------------------------------------------------- 2b  the columns: 5.50 m clear under the eaves
for i, x in enumerate(ARCH_X):
    for s, sn in ((-1, 'S'), (1, 'N')):
        yf = D.COL_Y[s]
        tag = 'A%02d%s' % (i + 1, sn)
        tc = D.T_COL[s]
        zax = P(tc)[1]                                   # the arch's axis over the column
        zsof = zax - D.ARCH_R / math.cos(tc)             # its soffit on the column's centreline
        bm = bmesh.new()
        tube(bm, [(x, yf, D.FOOT_Z - 0.06), (x, yf, zsof + 0.005)], D.COL_R, 24)
        obj('BusStn_Column_Post' + tag, bm, M_STEEL)

        # head: plate under the arch, welded shoe collar round the arch, gussets down the column
        h = D.COL_HEAD / 2
        bm = bmesh.new()
        box(bm, x - h, yf - h, zsof - 0.055, x + h, yf + h, zsof + 0.010)
        a_ax = (Vector(P3(x, tc + 0.05)) - Vector(P3(x, tc - 0.05))).normalized()
        ctr = Vector((x, P(tc)[0], zax))
        tube(bm, [ctr - a_ax * 0.26, ctr + a_ax * 0.26], D.ARCH_R + 0.055, 18)
        for sg in (-1, 1):
            add_cube(bm, (x + sg * (D.COL_R + 0.030), yf, zsof - 0.25),
                     (0.014, 0, 0), (0, 0.20, 0), (0, 0, 0.22))
        obj('BusStn_Column_Head' + tag, bm, M_STEEL, 0.006)

# ---------------------------------------------------------------- 3  the arches
NSEG = 56
for i, x in enumerate(ARCH_X):
    pts = [P3(x, -D.TE + 2 * D.TE * k / NSEG) for k in range(NSEG + 1)]
    bm = bmesh.new(); tube(bm, pts, D.ARCH_R, 24)
    obj('BusStn_Vault_ArchA%02d' % (i + 1), bm, M_STEEL)

# ---------------------------------------------------------------- 4  purlins (the whole 150 m, arch to arch) + eaves
for k in D.PUR_K:
    y, z = P(ROW_T[k], RPO)
    bm = bmesh.new()
    tube(bm, [(-0.30, y, z), (D.PL_LEN + 0.30, y, z)], D.PUR_R, 14)
    for x in ARCH_X:                                   # a welded collar at every arch: the joint, not a graze
        tube(bm, [(x - 0.14, y, z), (x + 0.14, y, z)], D.PUR_R + 0.045, 14)
    obj('BusStn_Vault_PurlinR%02d' % k, bm, M_STEEL)

for k, sn in ((0, 'South'), (D.N_ROW, 'North')):
    y, z = P(ROW_T[k], RPO)
    hw = D.EAVE_W / 2
    bm = bmesh.new(); box(bm, -0.30, y - hw, z - hw, D.PL_LEN + 0.30, y + hw, z + hw)
    obj('BusStn_Vault_EavesBeam' + sn, bm, M_STEEL, 0.012)

# ---------------------------------------------------------------- 5  glazing: bars between the purlins, then the panes
COLS = [0.0, 2.5, 5.0]
for b in range(D.N_BAY):
    x0 = b * D.BAY
    cols = [x0 + c for c in COLS] + ([D.PL_LEN] if b == D.N_BAY - 1 else [])
    bm = bmesh.new()
    for cx in cols:
        for r in range(D.N_ROW):
            a = Vector(P3(cx, ROW_T[r], RPO))
            c = Vector(P3(cx, ROW_T[r + 1], RPO))
            d = (c - a).normalized()
            bar(bm, a + d * 0.02, c - d * 0.02, D.GL_BAR_D, D.GL_BAR_W, up=(1, 0, 0))
    obj('BusStn_Glazing_BarB%02d' % (b + 1), bm, M_STEEL, 0.004)

    bm = bmesh.new()
    xs = [x0 + c for c in COLS] + [x0 + D.BAY]
    for ci in range(len(xs) - 1):
        xa, xb = xs[ci] + 0.025, xs[ci + 1] - 0.025
        for r in range(D.N_ROW):
            a = Vector(P(ROW_T[r], RPO))
            c = Vector(P(ROW_T[r + 1], RPO))
            dv = (c - a).normalized()
            p0 = a + dv * 0.03
            p1 = c - dv * 0.03
            nr = Vector((math.sin(ROW_T[r]), math.cos(ROW_T[r]))) * (D.PANE_T / 2)
            nr1 = Vector((math.sin(ROW_T[r + 1]), math.cos(ROW_T[r + 1]))) * (D.PANE_T / 2)
            vs = []
            for xq in (xa, xb):
                for pp, nn in ((p0, nr), (p1, nr1)):
                    vs.append(bm.verts.new((xq, pp.x - nn.x, pp.y - nn.y)))
                    vs.append(bm.verts.new((xq, pp.x + nn.x, pp.y + nn.y)))
            # vs: 0,1 = (xa,p0 in/out) 2,3 = (xa,p1) 4,5 = (xb,p0) 6,7 = (xb,p1)
            for q in ((0, 1, 3, 2), (4, 6, 7, 5), (0, 2, 6, 4), (1, 5, 7, 3), (0, 4, 5, 1), (2, 3, 7, 6)):
                bm.faces.new([vs[j] for j in q])
    obj('BusStn_Glazing_PaneB%02d' % (b + 1), bm, M_GLASS, 0.0, smooth=False)

# ---------------------------------------------------------------- 6  cable bracing, arch to arch, under the arches
for b in D.BRACE_BAYS:
    x0, x1 = b * D.BAY, (b + 1) * D.BAY
    bmc = bmesh.new()
    bml = bmesh.new()
    for s in (-1, 1):
        ta, tb = s * 0.30, s * 0.84
        for pa, pb in (((x0, ta), (x1, tb)), ((x0, tb), (x1, ta))):
            A = Vector(P3(pa[0], pa[1], D.CABLE_DR))
            B = Vector(P3(pb[0], pb[1], D.CABLE_DR))
            tube(bmc, [A, B], D.CABLE_OD / 2, 10)
        for xq, tq in ((x0, ta), (x0, tb), (x1, ta), (x1, tb)):
            inner = Vector(P3(xq, tq, -D.ARCH_R + 0.05))
            outer = Vector(P3(xq, tq, D.CABLE_DR - 0.03))
            bar(bml, inner, outer, 0.17, D.LUG_T, up=(1, 0, 0))
    obj('BusStn_Brace_CableB%02d' % (b + 1), bmc, M_CABLE)
    obj('BusStn_Brace_LugB%02d' % (b + 1), bml, M_STEEL, 0.004)

# ---------------------------------------------------------------- 7  gutters, brackets, downpipes
GZ = P(D.TE, RPO)[1] + D.GUT_DZ                         # the gutters hang under the two cantilever tips
for s, sn in ((-1, 'South'), (1, 'North')):
    gy = P(s * D.TE, RPO)[0] + s * D.GUT_DY
    hw = D.GUT_W / 2
    bm = bmesh.new()
    box(bm, -0.30, gy - hw + 0.005, GZ, D.PL_LEN + 0.30, gy + hw - 0.005, GZ + 0.014)
    for sg in (-1, 1):
        box(bm, -0.30, gy + sg * hw - sg * D.GUT_T, GZ, D.PL_LEN + 0.30, gy + sg * hw, GZ + D.GUT_D)
    obj('BusStn_Gutter_Channel' + sn, bm, M_ALU, 0.004)

    # bracket plates: off the arch near its tip, down and out to the gutter
    ay, az = P(s * (D.TE - 0.04))
    bm = bmesh.new()
    for x in ARCH_X:
        y0, y1 = sorted((ay, gy + s * hw))
        box(bm, x - 0.008, y0, GZ - 0.075, x + 0.008, y1, az + 0.02)
    obj('BusStn_Gutter_Bracket' + sn, bm, M_STEEL, 0.003)

    # downpipe: a sloped collector from the gutter back to the column, then down the column to its footing
    cy = D.COL_Y[s]
    py = cy + s * 0.55                                   # clear of the pedestal, on the footing pad
    for i, x in enumerate(ARCH_X):
        if i % D.DOWN_EVERY:
            continue
        bm = bmesh.new()
        tube(bm, [(x, gy, GZ + 0.026), (x, gy, GZ - 0.28), (x, py, GZ - 1.05), (x, py, 0.07)], D.DOWN_OD / 2, 14)
        for zc in (1.10, 2.60, 3.90):
            tube(bm, [(x, py, zc), (x, py, zc + 0.05)], D.DOWN_OD / 2 + 0.018, 14)     # pipe clamps
            bar(bm, (x, py, zc + 0.025), (x, cy + s * 0.17, zc + 0.025), 0.05, 0.05)   # into the column
        obj('BusStn_Gutter_DownpipeA%02d%s' % (i + 1, 'S' if s < 0 else 'N'), bm, M_ALU, 0.004)

# ================================================================ 8  the waiting hall and the platform furniture
# Added 2026-11-27 (docs/Bus-Station-Plan.md, "Waiting hall" + "Platform furniture"). The vault above is untouched.
# Nothing floats: the hall's glass sits in a frame that carries to a sill bedded in the deck paving; every sign hangs
# off an arm on a real post with a bolted base plate, or off a strap collar on a real column; benches, bins, bollards
# and rails all stand on base plates set into the paving; the sliding leaves hang from carriages inside their track.
# Lettering (numbers, and the word TAXI) is drawn MIRRORED in x, because the game's model loader mirrors Blender x.

# ---------------------------------------------------------------- extra textures and materials
T_OAK = plain('T_BusStn_Oak', (0.47, 0.35, 0.22), 0.09, 2.6, True)
T_RUB = plain('T_BusStn_Rubber', (0.045, 0.047, 0.05), 0.09, 2.0)
T_PANEL = plain('T_BusStn_Panel', (0.88, 0.89, 0.90), 0.025, 1.0)
M_OAK = mat('BusStn_Oak', T_OAK, 0.0, 0.52)
M_RUB = mat('BusStn_Rubber', T_RUB, 0.0, 0.88)
M_PANEL = mat('BusStn_Panel', T_PANEL, 0.25, 0.42)

FONT = {
    '0': ('.###.', '#...#', '#..##', '#.#.#', '##..#', '#...#', '.###.'),
    '1': ('..#..', '.##..', '..#..', '..#..', '..#..', '..#..', '.###.'),
    '2': ('.###.', '#...#', '....#', '...#.', '..#..', '.#...', '#####'),
    '3': ('####.', '....#', '...#.', '..##.', '....#', '#...#', '.###.'),
    '4': ('...#.', '..##.', '.#.#.', '#..#.', '#####', '...#.', '...#.'),
    '5': ('#####', '#....', '####.', '....#', '....#', '#...#', '.###.'),
    '6': ('..##.', '.#...', '#....', '####.', '#...#', '#...#', '.###.'),
    '7': ('#####', '....#', '...#.', '..#..', '.#...', '.#...', '.#...'),
    '8': ('.###.', '#...#', '#...#', '.###.', '#...#', '#...#', '.###.'),
    '9': ('.###.', '#...#', '#...#', '.####', '....#', '...#.', '.##..'),
    'A': ('..#..', '.#.#.', '#...#', '#...#', '#####', '#...#', '#...#'),
    'T': ('#####', '..#..', '..#..', '..#..', '..#..', '..#..', '..#..'),
    'X': ('#...#', '#...#', '.#.#.', '..#..', '.#.#.', '#...#', '#...#'),
    'I': ('.###.', '..#..', '..#..', '..#..', '..#..', '..#..', '.###.'),
    ':': ('.....', '..#..', '.....', '.....', '.....', '..#..', '.....'),
    '-': ('.....', '.....', '.....', '#####', '.....', '.....', '.....'),
    '.': ('.....', '.....', '.....', '.....', '.....', '..##.', '..##.'),
    ' ': ('.....', '.....', '.....', '.....', '.....', '.....', '.....'),
}
GLYPH_W, GLYPH_H = 5, 7


def text_w(s, px, gap=1):
    return len(s) * (GLYPH_W + gap) * px - gap * px


def draw_text(img, s, x, y, px, col):
    """Paint s into img (h, w, 3) with its top-left at (x, y); px pixels per font cell."""
    h, w = img.shape[:2]
    for ch in s:
        g = FONT.get(ch.upper(), FONT[' '])
        for r in range(GLYPH_H):
            for c in range(GLYPH_W):
                if g[r][c] == '#':
                    y0, x0 = y + r * px, x + c * px
                    img[max(y0, 0):min(y0 + px, h), max(x0, 0):min(x0 + px, w)] = col
        x += (GLYPH_W + 1) * px


LIT_FACE = (0.93, 0.95, 0.97)
INK = (0.07, 0.08, 0.10)


def label_tex(name, lines, w=512, h=256, bg=LIT_FACE, col=INK, px=None, pad=None):
    """A sign face: light diffuser, dark characters, centred; mirrored in x for the game's loader."""
    img = np.ones((h, w, 3), np.float32) * np.array(bg, np.float32)
    n = len(lines)
    if px is None:
        px = int(min(h / (n * (GLYPH_H + 3)), w / (max(text_w(s, 1, 1) for s in lines) + 6)))
        px = max(px, 1)
    lh = (GLYPH_H + 3) * px
    y = int((h - n * lh + 3 * px) / 2)
    for s in lines:
        x = int((w - text_w(s, px)) / 2) if pad is None else pad
        draw_text(img, s, x, y, px, col)
        y += lh
    # a faint lit sheen, then the mirror the loader undoes
    img *= (0.94 + 0.08 * blur(rng.random((h, w)), 2))[..., None]
    img = np.ascontiguousarray(img[:, ::-1])
    return save_img(name, img), T_PANEL[1]


def mat_lit(name, tex, strength=2.2, rough=0.38):
    m = mat(name, tex, 0.0, rough)
    nt = m.node_tree
    b = nt.nodes['Principled BSDF']
    ti = [n for n in nt.nodes if n.type == 'TEX_IMAGE' and n.image is tex[0]][0]
    nt.links.new(ti.outputs['Color'], b.inputs['Emission'])
    b.inputs['Emission Strength'].default_value = strength
    return m


FACE_UV = []      # (object, centre, u axis, u width, v axis, v height) - fitted after the global UV pass


def face_plate(name, c, n, w, h, m, t=0.006):
    """A thin lettered/graphic plate at c with its visible face looking along n (a unit axis), w across, h up.
    The u axis is the one a viewer of that face reads left to right (the texture is already mirrored)."""
    n = Vector(n)
    V = Vector((0, 0, 1))
    if abs(n.x) > 0.5:
        U = Vector((0, n.x, 0))
    else:
        U = Vector((-n.y, 0, 0))
    bm = bmesh.new()
    add_cube(bm, Vector(c), U * (w / 2), n * (t / 2), V * (h / 2))
    o = obj(name, bm, m, 0.0)
    FACE_UV.append((o, Vector(c), U, w, V, h))
    return o


def lit_box(group, part, c, size, m_face, dep_faces=(1, -1), arm_free=None):
    """A lit sign box: a welded housing ring with a back pan, a diffuser in it, and a lettered face each side."""
    cx, cy, cz = c
    w, dy, hz = size
    hw, hd, hh = w / 2, dy / 2, hz / 2
    bm = bmesh.new()
    box(bm, cx - hw, cy - hd, cz + hh - 0.05, cx + hw, cy + hd, cz + hh)          # top rail
    box(bm, cx - hw, cy - hd, cz - hh, cx + hw, cy + hd, cz - hh + 0.05)          # bottom rail
    box(bm, cx - hw, cy - hd, cz - hh, cx - hw + 0.05, cy + hd, cz + hh)          # end rails
    box(bm, cx + hw - 0.05, cy - hd, cz - hh, cx + hw, cy + hd, cz + hh)
    if len(dep_faces) == 1:                                                        # single-sided: a closed back pan
        yb = cy + 0.5 * dy * (-dep_faces[0])
        box(bm, cx - hw, yb - 0.012, cz - hh, cx + hw, yb + 0.012, cz + hh)
    obj('BusStn_%s_Housing%s' % (group, part), bm, M_STEEL, 0.006)

    bm = bmesh.new()
    box(bm, cx - hw + 0.02, cy - hd + 0.03, cz - hh + 0.02, cx + hw - 0.02, cy + hd - 0.03, cz + hh - 0.02)
    obj('BusStn_%s_Diffuser%s' % (group, part), bm, M_PANEL, 0.004)
    for s in dep_faces:
        face_plate('BusStn_%s_Face%s%s' % (group, part, 'N' if s > 0 else 'S'),
                   (cx, cy + s * (hd - 0.028), cz), (0, s, 0), w - 0.07, hz - 0.05, m_face, 0.008)


def base_plate(bm, x, y, w, zt=None, bolts=0.0, fz=None):
    """A bolted base plate bedded into the floor it stands on: 20 mm into the deck paving, or - on the pavement by
    the hall, where we lay no paving of our own - 60 mm down into the foundation raft just under it. Four nuts."""
    fz = D.DECK_Z if fz is None else fz
    zt = fz + 0.03 if zt is None else zt
    zb = fz - 0.02 if abs(fz - D.DECK_Z) < 1e-9 else fz - 0.06
    h = w / 2
    box(bm, x - h, y - h, zb, x + h, y + h, zt)
    if bolts:
        for dx in (-bolts, bolts):
            for dy in (-bolts, bolts):
                tube(bm, [(x + dx, y + dy, zt - 0.005), (x + dx, y + dy, zt + 0.022)], 0.016, 8)
    return bm


# ---------------------------------------------------------------- 8a  the hall: sill, frame, glass, roof
# The hall stands on the PAVEMENT between the platform and the terminal (busstn_dims: HALL_YS..HALL_YN), which is
# 0.18 m lower than the island deck, so every height here is measured off HALL_FLOOR_Z, not DECK_Z. The doors face
# the platform: two sets in the south wall, one in the north wall towards the terminal.
HX0, HX1 = D.HALL_X0, D.HALL_X1
HYN, HYS, HYM = D.HALL_YN, D.HALL_YS, D.HALL_YM
HFZ = D.HALL_FLOOR_Z
PW, PD = D.HALL_POST
BW, BD = D.HALL_BEAM
SZ = D.HALL_SILL_Z
HEAD = D.HALL_HEAD_Z
MULL_X = [HX0 + i * D.HALL_BAY for i in range(int(round((HX1 - HX0) / D.HALL_BAY)) + 1)]
# (x on the wall, wall y, outward sign, part tag, wall side)
DOORS = ((D.HALL_DOOR_S1_X, HYS, -1, 'S1', 'S'), (D.HALL_DOOR_S2_X, HYS, -1, 'S2', 'S'),
         (D.HALL_DOOR_N_X, HYN, 1, 'N1', 'N'))
DOOR_BAY = set((dsd, round(dx, 3)) for dx, dy, sg, dt, dsd in DOORS)


def is_door_bay(side, xc):
    return (side, round(xc, 3)) in DOOR_BAY


def ypair(a, b):
    """The two y of a box, smallest first."""
    return (a, b) if a <= b else (b, a)


# sill: a steel channel bedded into the paving, in segments that leave the door thresholds free
for side, yw in (('N', HYN), ('S', HYS)):
    cuts = sorted((dx - 1.42, dx + 1.42) for dx, dy, sg, dt, dsd in DOORS if dsd == side)
    segs = []
    xa = HX0
    for ca, cb in cuts:
        segs.append((xa, ca))
        xa = cb
    segs.append((xa, HX1))
    bm = bmesh.new()
    for xa, xb in segs:
        if xb - xa > 0.02:
            box(bm, xa, yw - D.HALL_SILL_W / 2, HFZ - 0.06, xb, yw + D.HALL_SILL_W / 2, SZ)
            box(bm, xa, yw - D.HALL_SILL_W / 2 + 0.03, HFZ - 0.06, xb, yw + D.HALL_SILL_W / 2 - 0.03, SZ - 0.04)
    obj('BusStn_Hall_Sill' + side, bm, M_STEEL, 0.008)

bm = bmesh.new()
for xw in (HX0, HX1):
    box(bm, xw - D.HALL_SILL_W / 2, HYS - 0.08, HFZ - 0.06, xw + D.HALL_SILL_W / 2, HYN + 0.08, SZ)
obj('BusStn_Hall_SillEnds', bm, M_STEEL, 0.008)

# mullions on the bay lines, both long walls, and the end-wall posts
for i, xm in enumerate(MULL_X):
    for side, yw in (('N', HYN), ('S', HYS)):
        bm = bmesh.new()
        box(bm, xm - PW / 2, yw - PD / 2, SZ - 0.06, xm + PW / 2, yw + PD / 2, HEAD + BD)
        box(bm, xm - PW / 2 - 0.02, yw - PD / 2 - 0.015, SZ - 0.02, xm + PW / 2 + 0.02, yw + PD / 2 + 0.015, SZ + 0.06)
        obj('BusStn_Hall_Mullion%s%02d' % (side, i + 1), bm, M_STEEL, 0.006)

for xw, xn in ((HX0, 'W'), (HX1, 'E')):
    bm = bmesh.new()
    box(bm, xw - PD / 2, HYM - 0.05, SZ - 0.06, xw + PD / 2, HYM + 0.05, HEAD + BD)
    obj('BusStn_Hall_PostMid' + xn, bm, M_STEEL, 0.006)

# head beams: a closed ring of box section on the mullion heads
for side, yw in (('N', HYN), ('S', HYS)):
    bm = bmesh.new()
    box(bm, HX0 - BW / 2 - 0.02, yw - BW / 2, HEAD, HX1 + BW / 2 + 0.02, yw + BW / 2, HEAD + BD)
    obj('BusStn_Hall_HeadBeam' + side, bm, M_STEEL, 0.008)
for xw, xn in ((HX0, 'W'), (HX1, 'E')):
    bm = bmesh.new()
    box(bm, xw - BW / 2, HYS - BW / 2 + 0.01, HEAD + 0.012, xw + BW / 2, HYN + BW / 2 - 0.01, HEAD + BD - 0.012)
    obj('BusStn_Hall_HeadBeam' + xn, bm, M_STEEL, 0.008)

# glazing: one pane per bay in a rebate, buried 30 mm in the mullions, the sill and the head beam
for side, yw in (('N', HYN), ('S', HYS)):
    for i in range(len(MULL_X) - 1):
        xa, xb = MULL_X[i], MULL_X[i + 1]
        xc = (xa + xb) / 2
        if is_door_bay(side, xc):
            continue
        bm = bmesh.new()
        box(bm, xa + PW / 2 - 0.03, yw - D.PANE_T / 2, SZ - 0.03, xb - PW / 2 + 0.03, yw + D.PANE_T / 2, HEAD + 0.03)
        obj('BusStn_Hall_Glass%s%02d' % (side, i + 1), bm, M_GLASS, 0.0)
for xw, xn in ((HX0, 'W'), (HX1, 'E')):
    for k, (ya, yb) in enumerate(((HYS, HYM), (HYM, HYN))):
        bm = bmesh.new()
        box(bm, xw - D.PANE_T / 2, ya + PW / 2 - 0.03, SZ - 0.03, xw + D.PANE_T / 2, yb - PW / 2 + 0.03, HEAD + 0.03)
        obj('BusStn_Hall_Glass%s%d' % (xn, k + 1), bm, M_GLASS, 0.0)

# the hall's own roof: purlins on the beams, a ribbed deck pan, a fascia round it, a flat ceiling under it
for i, xm in enumerate(MULL_X):
    bm = bmesh.new()
    box(bm, xm - 0.05, HYS - BW / 2, HEAD + BD - 0.03, xm + 0.05, HYN + BW / 2, HEAD + BD + 0.14)
    obj('BusStn_Hall_Purlin%02d' % (i + 1), bm, M_STEEL, 0.006)

bm = bmesh.new()
box(bm, HX0 - 0.25, HYS - 0.30, D.HALL_ROOF_Z - 0.08, HX1 + 0.25, HYN + 0.30, D.HALL_ROOF_Z)
for k in range(9):
    yr = HYS - 0.18 + k * (HYN - HYS + 0.36) / 8
    tube(bm, [(HX0 - 0.22, yr, D.HALL_ROOF_Z - 0.01), (HX1 + 0.22, yr, D.HALL_ROOF_Z - 0.01)], 0.035, 10)
obj('BusStn_Hall_RoofDeck', bm, M_ALU, 0.008)

bm = bmesh.new()
for yf in (HYS - 0.30, HYN + 0.30):
    sg = 1 if yf > HYM else -1
    box(bm, HX0 - 0.29, yf - sg * 0.022, D.HALL_ROOF_Z - 0.21, HX1 + 0.29, yf + sg * 0.022, D.HALL_ROOF_Z - 0.02)
for xf in (HX0 - 0.25, HX1 + 0.25):
    sg = 1 if xf > (HX0 + HX1) / 2 else -1
    box(bm, xf - sg * 0.022, HYS - 0.29, D.HALL_ROOF_Z - 0.21, xf + sg * 0.022, HYN + 0.29, D.HALL_ROOF_Z - 0.02)
obj('BusStn_Hall_Fascia', bm, M_ALU, 0.006)

bm = bmesh.new()
box(bm, HX0 - 0.03, HYS + 0.05, D.HALL_CEIL_Z, HX1 + 0.03, HYN - 0.05, D.HALL_CEIL_Z + 0.02)
obj('BusStn_Hall_Ceiling', bm, M_PANEL, 0.004)

# ---------------------------------------------------------------- 8b  the three sliding door sets
TZ = HFZ + 2.27                                     # track soffit level, as before over the floor
LB = D.HALL_THRESH_Z + 0.012                        # the leaves' bottom rail, 12 mm over the threshold
for dx, dy, sg, dt, dsd in DOORS:
    hw = D.HALL_DOOR_CLEAR / 2
    # a real threshold: a granite slab across the opening, bedded into the paving and lapped under the sill ends,
    # standing 22 mm proud with a set-back top edge
    bm = bmesh.new()
    a, b = ypair(dy - 0.21, dy + 0.21)
    box(bm, dx - 1.42, a, HFZ - 0.09, dx + 1.42, b, HFZ + 0.004)
    a, b = ypair(dy - 0.175, dy + 0.175)
    box(bm, dx - 1.40, a, HFZ - 0.01, dx + 1.40, b, D.HALL_THRESH_Z)
    obj('BusStn_Hall_Threshold' + dt, bm, M_GRAN, 0.006)

    # the stainless guide plate let into the slab, with the bottom guide groove between its two ribs
    bm = bmesh.new()
    a, b = ypair(dy + sg * 0.004, dy + sg * 0.086)
    box(bm, dx - 1.30, a, D.HALL_THRESH_Z - 0.012, dx + 1.30, b, D.HALL_THRESH_Z + 0.004)
    for off in (0.010, 0.056):
        a, b = ypair(dy + sg * off, dy + sg * (off + 0.014))
        box(bm, dx - 1.30, a, D.HALL_THRESH_Z - 0.012, dx + 1.30, b, D.HALL_THRESH_Z + 0.014)
    obj('BusStn_Hall_ThreshGuide' + dt, bm, M_ALU, 0.004)

    # track box across the opening, bolted to both jamb mullions, with the running rail inside
    bm = bmesh.new()
    box(bm, dx - hw - 0.14, dy - 0.11, TZ, dx + hw + 0.14, dy + 0.11, TZ + 0.16)
    box(bm, dx - hw - 0.02, dy - 0.085, TZ - 0.035, dx + hw + 0.02, dy + 0.085, TZ + 0.01)
    obj('BusStn_Hall_DoorTrack' + dt, bm, M_ALU, 0.006)

    # transom bar over the track, then the fanlight pane up to the head beam
    bm = bmesh.new()
    box(bm, dx - hw - 0.08, dy - 0.075, TZ + 0.14, dx + hw + 0.08, dy + 0.075, TZ + 0.24)
    obj('BusStn_Hall_DoorTransom' + dt, bm, M_STEEL, 0.006)
    bm = bmesh.new()
    box(bm, dx - hw - 0.02, dy - D.PANE_T / 2, TZ + 0.21, dx + hw + 0.02, dy + D.PANE_T / 2, HEAD + 0.03)
    obj('BusStn_Hall_DoorFanlight' + dt, bm, M_GLASS, 0.0)

    for lf, ln in ((-1, 'A'), (1, 'B')):
        yl = dy + sg * (0.055 if ln == 'A' else -0.005)      # two running planes, the leaves lapping at the middle
        x0 = dx - hw - 0.015 if lf < 0 else dx - 0.03
        x1 = dx + 0.03 if lf < 0 else dx + hw + 0.015
        # leaf frame: stiles, top and bottom rails, two hanger plates up into the track, a seal on the leading edge
        bm = bmesh.new()
        for xs in (x0, x1):
            box(bm, xs - 0.03, yl - 0.022, LB, xs + 0.03, yl + 0.022, TZ - 0.01)
        box(bm, x0 - 0.03, yl - 0.022, LB, x1 + 0.03, yl + 0.022, LB + 0.06)
        box(bm, x0 - 0.03, yl - 0.022, TZ - 0.09, x1 + 0.03, yl + 0.022, TZ - 0.01)
        for hx in (x0 + 0.26, x1 - 0.26):
            box(bm, hx - 0.05, yl - 0.012, TZ - 0.05, hx + 0.05, yl + 0.012, TZ + 0.02)
            tube(bm, [(hx, yl - 0.055, TZ + 0.005), (hx, yl + 0.055, TZ + 0.005)], 0.028, 12)
        obj('BusStn_Hall_DoorLeaf%s%s_Frame' % (dt, ln), bm, M_ALU, 0.005)
        bm = bmesh.new()
        xe = x1 if lf < 0 else x0
        box(bm, xe - 0.018, yl - 0.026, LB + 0.01, xe + 0.018, yl + 0.026, TZ - 0.02)
        obj('BusStn_Hall_DoorLeaf%s%s_Seal' % (dt, ln), bm, M_RUB, 0.004)
        bm = bmesh.new()
        box(bm, x0 + 0.02, yl - D.PANE_T / 2, LB + 0.04, x1 - 0.02, yl + D.PANE_T / 2, TZ - 0.07)
        obj('BusStn_Hall_DoorLeaf%s%s_Glass' % (dt, ln), bm, M_GLASS, 0.0)

# ---------------------------------------------------------------- 8c  benches (hall and platform), all sittable
SITS = []


def bench(tag, xc, yc, face, length=2.30, fz=D.DECK_Z):
    """A bench: welded side frames on bolted base plates, oak seat slats with a flat top at 0.45 m over the floor it stands on,
    a leaning back and two armrests. face = +1 the sitter looks to +y."""
    zt = fz + 0.45
    ya = yc                         # the seat's back edge line
    yf = yc + face * 0.46           # its front edge
    fx = [xc - length / 2 + 0.11, xc + length / 2 - 0.11]
    bm = bmesh.new()
    for x in fx:
        base_plate(bm, x, yc + face * 0.34, 0.26, fz + 0.025, 0.085, fz=fz)
        base_plate(bm, x, yc - face * 0.02, 0.24, fz + 0.025, 0.075, fz=fz)
        # two legs and the seat bearer, as one welded blade frame
        box(bm, x - 0.025, yc + face * 0.30, fz + 0.01, x + 0.025, yc + face * 0.38, zt - 0.045)
        box(bm, x - 0.025, yc - face * 0.06, fz + 0.01, x + 0.025, yc + face * 0.02, zt - 0.045)
        box(bm, x - 0.028, min(ya - face * 0.06, yf), zt - 0.062, x + 0.028, max(ya - face * 0.06, yf), zt - 0.03)
        # back post, leaning back 12 degrees, and the armrest on top of it
        for k in range(7):          # the stack reaches above the top back slat, so no slat ends in the air
            zz = zt - 0.08 + k * 0.058
            yy2 = ya - face * (0.03 + 0.021 * k)
            box(bm, x - 0.025, yy2 - 0.03, zz, x + 0.025, yy2 + 0.03, zz + 0.062)
        tube(bm, [(x, ya - face * 0.10, zt + 0.25), (x, ya + face * 0.30, zt + 0.25), (x, yf - face * 0.03, zt + 0.22)],
             0.022, 12)
        box(bm, x - 0.022, min(ya + face * 0.26, ya + face * 0.30), zt - 0.09,
            x + 0.022, max(ya + face * 0.26, ya + face * 0.30), zt + 0.26)
    # the longitudinal rail that ties the two frames into one bench
    box(bm, fx[0], yc + face * 0.33, zt - 0.17, fx[1], yc + face * 0.39, zt - 0.11)
    box(bm, fx[0], yc - face * 0.05, zt - 0.17, fx[1], yc + face * 0.01, zt - 0.11)
    obj('BusStn_Bench%s_Frame' % tag, bm, M_STEEL, 0.006)

    # seat: five slats, their tops all at 0.45 m over the floor - one flat surface for the sitting IK
    bm = bmesh.new()
    for k in range(5):
        y0 = ya + face * (0.015 + k * 0.090)
        box(bm, xc - length / 2, min(y0, y0 + face * 0.072), zt - 0.042,
            xc + length / 2, max(y0, y0 + face * 0.072), zt)
    obj('BusStn_Bench%s_Seat' % tag, bm, M_OAK, 0.008)

    # back: four leaning slats, buried 25 mm into the back posts
    bm = bmesh.new()
    for k in range(4):
        zz = zt + 0.055 + k * 0.082
        yy2 = ya - face * (0.045 + 0.021 * (k + 1.2))
        box(bm, xc - length / 2 + 0.02, min(yy2, yy2 - face * 0.036), zz,
            xc + length / 2 - 0.02, max(yy2, yy2 - face * 0.036), zz + 0.062)
    obj('BusStn_Bench%s_Back' % tag, bm, M_OAK, 0.008)

    n = int((length - 0.30) / 0.52)
    for i in range(n):
        SITS.append(('IK_Seat_Bench%s%d' % (tag, i + 1),
                     (xc + (i - (n - 1) / 2) * 0.52, yc + face * 0.22, zt)))


# the hall benches, backs to the two long walls; the north row keeps clear of the north door bay
HALL_BENCHES = [('H1', 62.5, HYN - 0.20, -1), ('H2', 70.0, HYN - 0.20, -1), ('H3', 87.0, HYN - 0.20, -1),
                ('H4', 62.5, HYS + 0.20, 1), ('H5', 72.5, HYS + 0.20, 1), ('H6', 77.5, HYS + 0.20, 1)]
for tag, xc, yc, face in HALL_BENCHES:
    bench(tag, xc, yc, face, fz=HFZ)

for i, xc in enumerate((25.0, 45.0, 105.0, 125.0)):
    bench('P%dN' % (i + 1), xc, 0.16, 1)
    bench('P%dS' % (i + 1), xc, -0.16, -1)

# ---------------------------------------------------------------- 8d  bins (hall and platform)
def bin_unit(tag, x, y, fz=D.DECK_Z):
    bm = bmesh.new()
    base_plate(bm, x, y, 0.34, fz + 0.028, 0.11, fz=fz)
    tube(bm, [(x, y, fz + 0.01), (x, y, fz + 0.12)], 0.085, 16)
    obj('BusStn_Bin%s_Base' % tag, bm, M_STEEL, 0.006)
    bm = bmesh.new()
    tube(bm, [(x, y, fz + 0.09), (x, y, fz + 0.80)], 0.185, 24)
    tube(bm, [(x, y, fz + 0.30), (x, y, fz + 0.34)], 0.196, 24)
    tube(bm, [(x, y, fz + 0.68), (x, y, fz + 0.72)], 0.196, 24)
    obj('BusStn_Bin%s_Body' % tag, bm, M_STEEL, 0.006)
    bm = bmesh.new()
    tube(bm, [(x, y, fz + 0.78), (x, y, fz + 0.88)], 0.192, 24)      # liner mouth inside the body
    obj('BusStn_Bin%s_Liner' % tag, bm, M_DARK, 0.004)
    bm = bmesh.new()
    tube(bm, [(x, y, fz + 0.96), (x, y, fz + 1.04)], 0.205, 24)
    for sgx in (-1, 1):
        box(bm, x + sgx * 0.14, y - 0.075, fz + 0.76, x + sgx * 0.178, y + 0.075, fz + 1.00)
    obj('BusStn_Bin%s_Hood' % tag, bm, M_ALU, 0.005)


bin_unit('H1', 61.1, HYN - 0.45, HFZ)
bin_unit('H2', 88.9, HYS + 0.45, HFZ)
for i, x in enumerate((30.0, 50.0, 100.0, 120.0)):
    bin_unit('P%d' % (i + 1), x, 1.55)

# ---------------------------------------------------------------- 8e  ticket machine, departure board, heaters
TM_X, TM_Y = 60.95, HYS + 0.85       # by the west end wall, facing east into the room
bm = bmesh.new()
base_plate(bm, TM_X, TM_Y, 0.66, HFZ + 0.05, 0.24, fz=HFZ)
box(bm, TM_X - 0.30, TM_Y - 0.36, HFZ + 0.02, TM_X + 0.30, TM_Y + 0.36, HFZ + 0.12)
obj('BusStn_Ticket_Plinth', bm, M_DARK, 0.008)
bm = bmesh.new()
box(bm, TM_X - 0.28, TM_Y - 0.34, HFZ + 0.08, TM_X + 0.28, TM_Y + 0.34, HFZ + 1.48)
box(bm, TM_X - 0.31, TM_Y - 0.37, HFZ + 1.44, TM_X + 0.31, TM_Y + 0.37, HFZ + 1.56)   # capping
obj('BusStn_Ticket_Cabinet', bm, M_STEEL, 0.012)
bm = bmesh.new()
for _ry in (-1, 1):                                                                             # screen bezel
    box(bm, TM_X + 0.24, TM_Y + _ry * 0.23, HFZ + 0.90, TM_X + 0.34, TM_Y + _ry * 0.30, HFZ + 1.40)
for _rz in (0.90, 1.34):
    box(bm, TM_X + 0.24, TM_Y - 0.30, HFZ + _rz, TM_X + 0.34, TM_Y + 0.30, HFZ + _rz + 0.06)
box(bm, TM_X + 0.24, TM_Y - 0.30, HFZ + 0.70, TM_X + 0.36, TM_Y + 0.30, HFZ + 0.88)   # keypad shelf
for k in range(12):
    bx = TM_Y - 0.24 + (k % 4) * 0.16
    bz = HFZ + 0.845 - (k // 4) * 0.055
    tube(bm, [(TM_X + 0.33, bx, bz), (TM_X + 0.365, bx, bz)], 0.021, 10)                        # buttons
box(bm, TM_X + 0.24, TM_Y + 0.08, HFZ + 0.52, TM_X + 0.33, TM_Y + 0.22, HFZ + 0.64)   # card reader
box(bm, TM_X + 0.26, TM_Y - 0.22, HFZ + 0.44, TM_X + 0.31, TM_Y - 0.04, HFZ + 0.50)   # ticket slot
obj('BusStn_Ticket_Fascia', bm, M_DARK, 0.005)
T_TICKET = label_tex('T_BusStn_TicketScreen', ['A1 - A12', '1 - 12'], 512, 512)
M_TICKET = mat_lit('BusStn_TicketScreen', T_TICKET, 1.8)
face_plate('BusStn_Ticket_Screen', (TM_X + 0.325, TM_Y, HFZ + 1.15), (1, 0, 0), 0.52, 0.42, M_TICKET, 0.012)
bm = bmesh.new()
box(bm, TM_X + 0.18, TM_Y - 0.42, HFZ + 1.50, TM_X + 0.56, TM_Y + 0.42, HFZ + 1.58)   # weather canopy
for sgy in (-1, 1):
    box(bm, TM_X + 0.20, TM_Y + sgy * 0.38, HFZ + 1.36, TM_X + 0.46, TM_Y + sgy * 0.42, HFZ + 1.54)
obj('BusStn_Ticket_Canopy', bm, M_ALU, 0.006)

# departure board, hung from two brackets bolted to the ceiling
BRD_X, BRD_Y, BRD_Z = 75.0, HYM, HFZ + 2.32
bm = bmesh.new()
box(bm, BRD_X - 0.90, BRD_Y - 0.26, D.HALL_CEIL_Z - 0.01, BRD_X + 0.90, BRD_Y + 0.26, D.HALL_CEIL_Z + 0.03)
for hx in (BRD_X - 0.72, BRD_X + 0.72):
    for hy in (BRD_Y - 0.17, BRD_Y + 0.17):
        tube(bm, [(hx, hy, BRD_Z + 0.40), (hx, hy, D.HALL_CEIL_Z + 0.01)], 0.022, 12)
    box(bm, hx - 0.09, BRD_Y - 0.21, BRD_Z + 0.34, hx + 0.09, BRD_Y + 0.21, BRD_Z + 0.44)
obj('BusStn_Board_Hanger', bm, M_STEEL, 0.006)
T_BOARD = label_tex('T_BusStn_Board', ['A1  06:40', 'A4  06:55', 'A7  07:10', 'A9  07:25', 'A12 07:40'], 1024, 512)
M_BOARD = mat_lit('BusStn_Board', T_BOARD, 2.4)
lit_box('Board', '', (BRD_X, BRD_Y, BRD_Z), (1.80, 0.26, 0.82), M_BOARD)

# radiant overhead heaters under the ceiling, on bolted brackets
for i, hx in enumerate((63.5, 70.5, 79.5, 86.5)):
    bm = bmesh.new()
    for sgx in (-1, 1):
        box(bm, hx + sgx * 0.62 - 0.05, HYM - 0.10, D.HALL_CEIL_Z - 0.01, hx + sgx * 0.62 + 0.05, HYM + 0.10,
            D.HALL_CEIL_Z + 0.03)
        box(bm, hx + sgx * 0.62 - 0.04, HYM - 0.03, D.HALL_CEIL_Z - 0.20, hx + sgx * 0.62 + 0.04, HYM + 0.03,
            D.HALL_CEIL_Z + 0.01)
    obj('BusStn_Heater%d_Bracket' % (i + 1), bm, M_STEEL, 0.005)
    bm = bmesh.new()
    box(bm, hx - 0.78, HYM - 0.11, D.HALL_CEIL_Z - 0.26, hx + 0.78, HYM + 0.11, D.HALL_CEIL_Z - 0.16)
    for sgy in (-1, 1):
        box(bm, hx - 0.78, HYM + sgy * 0.09, D.HALL_CEIL_Z - 0.30, hx + 0.78, HYM + sgy * 0.15,
            D.HALL_CEIL_Z - 0.17)
    obj('BusStn_Heater%d_Reflector' % (i + 1), bm, M_ALU, 0.005)
    bm = bmesh.new()
    for sgx in (-1, 1):                                             # end holders up into the trough
        box(bm, hx + sgx * 0.70 - 0.03, HYM - 0.12, D.HALL_CEIL_Z - 0.36, hx + sgx * 0.70 + 0.03, HYM + 0.12,
            D.HALL_CEIL_Z - 0.19)
    tube(bm, [(hx - 0.72, HYM, D.HALL_CEIL_Z - 0.31), (hx + 0.72, HYM, D.HALL_CEIL_Z - 0.31)], 0.021, 14)
    for k in range(9):
        gx = hx - 0.70 + k * 0.175
        tube(bm, [(gx, HYM - 0.085, D.HALL_CEIL_Z - 0.333), (gx, HYM + 0.085, D.HALL_CEIL_Z - 0.333)], 0.008, 8)
    obj('BusStn_Heater%d_Element' % (i + 1), bm, M_DARK, 0.003)

# ---------------------------------------------------------------- 8f  bay signs A1 - A12, and one TAXI sign
def sign_post(tag, x, label_lines, name):
    tex = label_tex('T_BusStn_Sign' + tag, label_lines, 512, 256)
    m = mat_lit('BusStn_Sign' + tag, tex, 2.6)
    y = D.SIGN_POST_Y
    bm = bmesh.new()
    base_plate(bm, x, y, 0.30, D.DECK_Z + 0.035, 0.10)
    tube(bm, [(x, y, D.DECK_Z + 0.015), (x, y, D.DECK_Z + 0.14)], 0.085, 16)        # grout collar
    obj('BusStn_%s_Base' % name, bm, M_STEEL, 0.006)
    bm = bmesh.new()
    tube(bm, [(x, y, D.DECK_Z + 0.10), (x, y, D.SIGN_Z + 0.33)], 0.048, 18)
    obj('BusStn_%s_Post' % name, bm, M_STEEL)
    yb = y - 0.60                                                                    # the box hangs over the bay side
    bm = bmesh.new()
    bar(bm, (x, y - 0.02, D.SIGN_Z), (x, yb + 0.07, D.SIGN_Z), 0.07, 0.07)
    bar(bm, (x, y - 0.02, D.SIGN_Z + 0.28), (x, y - 0.32, D.SIGN_Z + 0.02), 0.05, 0.05)    # the arm's stay
    obj('BusStn_%s_Arm' % name, bm, M_STEEL, 0.006)
    lit_box(name, '', (x, yb, D.SIGN_Z), D.SIGN_BOX, m)


for i in range(D.N_BAYS):
    sign_post('A%d' % (i + 1), D.bay_x(i), ['A%d' % (i + 1)], 'SignA%02d' % (i + 1))
sign_post('Taxi', D.PL_LEN - 6.0, ['TAXI'], 'SignTaxi')

# ---------------------------------------------------------------- 8g  timetable cases on the arch columns
T_TTABLE = label_tex('T_BusStn_Timetable',
                     ['A1  06:40', 'A2  06:45', 'A3  06:50', 'A4  06:55', 'A5  07:00', 'A6  07:05',
                      'A7  07:10', 'A8  07:15'], 512, 1024, bg=(0.95, 0.96, 0.97))
M_TTABLE = mat('BusStn_Timetable', T_TTABLE, 0.0, 0.45)
for i, xc in enumerate((15.0, 45.0, 105.0, 135.0)):
    yc = D.COL_S_Y
    zc = 1.55
    bm = bmesh.new()
    for zz in (zc - 0.52, zc + 0.52):                                   # two strap collars round the column
        tube(bm, [(xc, yc, zz - 0.055), (xc, yc, zz + 0.055)], D.COL_R + 0.022, 20)
        bar(bm, (xc, yc + D.COL_R - 0.01, zz), (xc, yc + D.COL_R + 0.09, zz), 0.10, 0.08)
    obj('BusStn_Timetable%d_Strap' % (i + 1), bm, M_STEEL, 0.006)
    yf = yc + D.COL_R + 0.10
    bm = bmesh.new()
    box(bm, xc - 0.42, yf - 0.04, zc - 0.62, xc + 0.42, yf - 0.015, zc + 0.62)      # back pan
    for sgx in (-1, 1):                                                             # perimeter rails
        box(bm, xc + sgx * 0.375, yf - 0.03, zc - 0.62, xc + sgx * 0.42, yf + 0.065, zc + 0.62)
    for sgz in (-1, 1):
        box(bm, xc - 0.42, yf - 0.03, zc + sgz * 0.575, xc + 0.42, yf + 0.065, zc + sgz * 0.62)
    obj('BusStn_Timetable%d_Case' % (i + 1), bm, M_STEEL, 0.006)
    bm = bmesh.new()
    box(bm, xc - 0.40, yf + 0.030, zc - 0.60, xc + 0.40, yf + 0.050, zc + 0.60)
    obj('BusStn_Timetable%d_Glass' % (i + 1), bm, M_GLASS, 0.0)
    face_plate('BusStn_Timetable%d_Sheet' % (i + 1), (xc, yf - 0.016, zc), (0, 1, 0), 0.74, 1.14, M_TTABLE, 0.008)

# ---------------------------------------------------------------- 8h  handrails at both platform ends
for xe, en in ((1.10, 'W'), (D.PL_LEN - 1.10, 'E')):
    ys = [-3.00 + k * (6.00 / 5) for k in range(6)]
    for k, yp in enumerate(ys):
        bm = bmesh.new()
        base_plate(bm, xe, yp, 0.22, D.DECK_Z + 0.025, 0.07)
        tube(bm, [(xe, yp, D.DECK_Z + 0.01), (xe, yp, D.DECK_Z + 1.10)], 0.030, 16)
        obj('BusStn_Rail%s_Post%d' % (en, k + 1), bm, M_STEEL, 0.004)
    for zz, zn in ((D.DECK_Z + 1.07, 'Top'), (D.DECK_Z + 0.56, 'Mid')):
        bm = bmesh.new()
        tube(bm, [(xe, ys[0] - 0.09, zz), (xe, ys[-1] + 0.09, zz)], 0.024, 14)
        obj('BusStn_Rail%s_%s' % (en, zn), bm, M_STEEL)

# ---------------------------------------------------------------- 8i  bollards at the platform noses
for xe, en in ((0.45, 'W'), (D.PL_LEN - 0.45, 'E')):
    for k, yp in enumerate((-1.80, 0.0, 1.80)):
        bm = bmesh.new()
        base_plate(bm, xe, yp, 0.26, D.DECK_Z + 0.03, 0.085)
        obj('BusStn_Bollard%s%d_Base' % (en, k + 1), bm, M_STEEL, 0.005)
        bm = bmesh.new()
        tube(bm, [(xe, yp, D.DECK_Z + 0.012), (xe, yp, D.DECK_Z + 0.95)], 0.072, 18)
        tube(bm, [(xe, yp, D.DECK_Z + 0.93), (xe, yp, D.DECK_Z + 0.97)], 0.082, 18)       # cap
        obj('BusStn_Bollard%s%d_Post' % (en, k + 1), bm, M_STEEL, 0.004)
        bm = bmesh.new()
        for zz in (D.DECK_Z + 0.70, D.DECK_Z + 0.80):
            tube(bm, [(xe, yp, zz), (xe, yp, zz + 0.06)], 0.078, 18)
        obj('BusStn_Bollard%s%d_Band' % (en, k + 1), bm, M_DARK, 0.003)

# ---------------------------------------------------------------- 8j  the two-faced clock on an arch column
CLK_X, CLK_Y, CLK_Z = 52.5, -2.05, 3.55
T_DIAL = None
_r = np.sqrt((xx - 0.5) ** 2 + (yy - 0.5) ** 2) * 2
_a = np.arctan2(xx - 0.5, 0.5 - yy)
_dial = np.ones((NPX, NPX, 3), np.float32) * np.array((0.95, 0.96, 0.97), np.float32)
_min = (np.abs(((_a / (2 * math.pi) * 60) % 1) - 0.5) > 0.44) & (_r > 0.80) & (_r < 0.90)
_hr = (np.abs(((_a / (2 * math.pi) * 12) % 1) - 0.5) > 0.42) & (_r > 0.70) & (_r < 0.90)
_dial[_min | _hr] = np.array(INK, np.float32)
_dial[_r > 0.96] = np.array((0.80, 0.81, 0.82), np.float32)
T_DIAL = (save_img('T_BusStn_ClockDial', _dial), T_PANEL[1])
M_DIAL = mat('BusStn_ClockDial', T_DIAL, 0.0, 0.35)

CARM_Z = CLK_Z + 0.44
bm = bmesh.new()
for zz in (CARM_Z, CARM_Z - 0.55):
    tube(bm, [(CLK_X, D.COL_S_Y, zz - 0.06), (CLK_X, D.COL_S_Y, zz + 0.06)], D.COL_R + 0.022, 20)
bar(bm, (CLK_X, D.COL_S_Y + D.COL_R - 0.02, CARM_Z), (CLK_X, CLK_Y + 0.04, CARM_Z), 0.08, 0.08)
bar(bm, (CLK_X, CLK_Y, CARM_Z + 0.03), (CLK_X, CLK_Y, CLK_Z + 0.30), 0.09, 0.09)        # drop into the drum top
bar(bm, (CLK_X, D.COL_S_Y + D.COL_R - 0.02, CARM_Z - 0.55), (CLK_X, CLK_Y - 0.22, CARM_Z - 0.02), 0.05, 0.05)
obj('BusStn_Clock_Arm', bm, M_STEEL, 0.006)
bm = bmesh.new()
tube(bm, [(CLK_X - 0.11, CLK_Y, CLK_Z), (CLK_X + 0.11, CLK_Y, CLK_Z)], 0.345, 28)
obj('BusStn_Clock_Drum', bm, M_STEEL, 0.006)
for sgx, fn in ((-1, 'W'), (1, 'E')):
    bm = bmesh.new()
    tube(bm, [(CLK_X + sgx * 0.108, CLK_Y, CLK_Z), (CLK_X + sgx * 0.124, CLK_Y, CLK_Z)], 0.325, 28)
    o = obj('BusStn_Clock_Dial' + fn, bm, M_DIAL, 0.0)
    FACE_UV.append((o, Vector((CLK_X + sgx * 0.116, CLK_Y, CLK_Z)), Vector((0, sgx, 0)), 0.68,
                    Vector((0, 0, 1)), 0.68))
    # the centre boss: a cap let 6 mm into the dial, and the spindle both hands are hung on
    bm = bmesh.new()
    tube(bm, [(CLK_X + sgx * 0.118, CLK_Y, CLK_Z), (CLK_X + sgx * 0.130, CLK_Y, CLK_Z)], 0.034, 16)
    tube(bm, [(CLK_X + sgx * 0.118, CLK_Y, CLK_Z), (CLK_X + sgx * 0.160, CLK_Y, CLK_Z)], 0.0085, 12)
    obj('BusStn_Clock_Boss' + fn, bm, M_DARK, 0.003)
    # The two hands. Each is modelled pointing at 12 o'clock (straight up, +Z) and its object origin is put on
    # the centre of this face's boss, on the dial's own axis, so the runtime only has to turn the object about
    # the x axis (the dials face -x and +x) by the clock angle. They are left out of the generic origin pass
    # below, which would otherwise pull the origin back to the hand's bounding-box centre.
    boss_c = Vector((CLK_X + sgx * 0.124, CLK_Y, CLK_Z))
    for ln, nm, zr in ((0.19, 'HandH', 0.137), (0.265, 'HandM', 0.150)):
        bm = bmesh.new()
        add_cube(bm, (sgx * (zr - 0.124), 0.0, ln / 2 + 0.02),
                 (0.006, 0, 0), (0, 0.016, 0), (0, 0, ln / 2 + 0.03))
        o = obj('BusStn_Clock_%s%s' % (nm, fn), bm, M_DARK, 0.002)
        o.location = boss_c
        NO_PIVOT.add(o.name)

# the sitting IK points: an empty at every seat surface, as the coach seats have
for nm, p in SITS:
    e = bpy.data.objects.new(nm, None)
    e.empty_display_size = 0.12
    e.location = p
    bpy.context.collection.objects.link(e)


# ---------------------------------------------------------------- UVs, pivots, save, export
for o in ALL:
    o.select_set(True)
bpy.context.view_layer.objects.active = ALL[0]
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.cube_project(cube_size=1.0, correct_aspect=True, scale_to_bounds=False)
bpy.ops.object.mode_set(mode='OBJECT')
for o in ALL:                                # the clock hands keep the origin they were built with
    if o.name in NO_PIVOT:
        o.select_set(False)
assert ALL[0].name not in NO_PIVOT
bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='BOUNDS')
for o in ALL:
    o.select_set(False)

for o, c, U, uw, V, vh in FACE_UV:           # lettered/graphic plates: fit the texture to the plate face
    me = o.data
    uv = me.uv_layers.active.data
    M = o.matrix_world
    for poly in me.polygons:
        for li in poly.loop_indices:
            p = M @ me.vertices[me.loops[li].vertex_index].co - c
            uv[li].uv = (0.5 + p.dot(U) / uw, 0.5 + p.dot(V) / vh)

tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in ALL)
print('BUSSTATION: %d parts, %d triangles' % (len(ALL), tris))
for o in ALL:
    assert len(o.data.materials) == 1 and o.data.materials[0] is not None, o.name

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'BusStation.blend'))
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'BusStation.glb'), export_format='GLB',
                          use_selection=False, export_apply=True)
print('BUSSTATION: saved BusStation.blend and BusStation.glb')

# ---------------------------------------------------------------- one picture of the finished structure
if RENDER:
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    ee = sc.eevee
    ee.use_gtao = True
    ee.gtao_distance = 1.2
    ee.use_ssr = True
    ee.use_ssr_refraction = True
    ee.use_soft_shadows = True
    ee.shadow_cascade_size = '4096'
    ee.taa_render_samples = 16 if QUICK else 96
    sc.view_settings.view_transform = 'Filmic'
    sc.view_settings.look = 'Medium High Contrast'
    w = bpy.data.worlds.new('Sky')
    w.use_nodes = True
    sky = w.node_tree.nodes.new('ShaderNodeTexSky')
    sky.sky_type = 'NISHITA'
    sky.sun_elevation = math.radians(34)
    sky.sun_rotation = math.radians(210)
    w.node_tree.links.new(sky.outputs['Color'], w.node_tree.nodes['Background'].inputs['Color'])
    w.node_tree.nodes['Background'].inputs['Strength'].default_value = 1.0
    sc.world = w
    bm = bmesh.new(); box(bm, -70, -60, -0.03, 230, 60, 0.0)
    gnd = obj('RenderGround', bm, mat('BusStn_RenderGround', plain('T_BusStn_Apron', (0.21, 0.22, 0.23), 0.08), 0, 0.85))
    sun_d = bpy.data.lights.new('Sun', 'SUN')
    sun_d.energy = 3.4
    sun_d.angle = math.radians(1.5)
    sun = bpy.data.objects.new('Sun', sun_d)
    sc.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(52), 0, math.radians(126))
    cam_d = bpy.data.cameras.new('Cam')
    cam_d.lens = 38
    cam = bpy.data.objects.new('Cam', cam_d)
    sc.collection.objects.link(cam)
    sc.camera = cam
    sc.render.resolution_x, sc.render.resolution_y = (1280, 720) if QUICK else (3840, 2160)
    SHOTS = (('busstation-vault.png', (-52.0, -46.0, 25.0), (58.0, 0.0, 4.0), 38),
             ('busstation-platform.png', (44.0, 1.70, 1.85), (94.0, -0.60, 2.70), 28),
             # the waiting hall seen from the island platform, eye height on the deck
             ('busstation-hall.png', (56.0, -0.60, 1.80), (74.0, 6.20, 1.95), 24))
    if ONLY:
        SHOTS = tuple(sh for sh in SHOTS if any(k in sh[0] for k in ONLY))
    for fn, loc, at, lens in SHOTS:
        cam_d.lens = lens
        cam.location = loc
        cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = os.path.join(ROOT, 'docs', 'Pictures', fn)
        bpy.ops.render.render(write_still=True)
        print('BUSSTATION: picture docs/Pictures/' + fn)
