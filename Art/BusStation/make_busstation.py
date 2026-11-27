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
#   ... -- --render          one 3840x2160 three-quarter view to docs/Pictures/busstation-vault.png
import bpy, bmesh, math, os, sys
import numpy as np
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
import busstn_dims as D

RENDER = '--render' in sys.argv
QUICK = '--quick' in sys.argv
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

# ---------------------------------------------------------------- UVs, pivots, save, export
for o in ALL:
    o.select_set(True)
bpy.context.view_layer.objects.active = ALL[0]
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.cube_project(cube_size=1.0, correct_aspect=True, scale_to_bounds=False)
bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY', center='BOUNDS')
for o in ALL:
    o.select_set(False)

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
    cam.location = (-52.0, -46.0, 25.0)
    at = Vector((58.0, 0.0, 4.0))
    cam.rotation_euler = (at - Vector(cam.location)).to_track_quat('-Z', 'Y').to_euler()
    sc.render.resolution_x, sc.render.resolution_y = (1280, 720) if QUICK else (3840, 2160)
    sc.render.filepath = os.path.join(ROOT, 'docs', 'Pictures', 'busstation-vault.png')
    bpy.ops.render.render(write_still=True)
    print('BUSSTATION: picture docs/Pictures/busstation-vault.png')
