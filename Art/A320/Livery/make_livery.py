# The house livery "Airport Dynasty Pro" for the game's A320 (FlightGear A320-200 CFM model, Art/A320/A320.glb),
# painted into the model's EXISTING texture layout as a NEW texture set (owner 2026-11-28; concept ADP-livery-concept.png).
# The plain white textures in Art/A320/Textures/FG stay untouched (they are the backup); geometry and UVs are not changed.
#
# How: the design is drawn as vector art in side view (fuselage, tail cone, fin) and in an engine frame, at ~4 mm a pixel.
# Every texel of the existing texture is traced back to its point on the aircraft (triangles rasterised in UV space with
# the glb's own UVs), and the design is looked up there - so lines run on across UV islands, fuselage halves, doors,
# tail cone and fin by construction. The paint is a layer: it is multiplied with the base texture's own shading, so panel
# lines, rivets, doors and placards stay visible; windows and coloured placards stay as they are.
#
#   1. blender -b --python Art/A320/Livery/make_livery.py -- export     (triangles + UVs of A320.glb -> F:/ADP-Export/livery/work)
#   2. python Art/A320/Livery/make_livery.py                             (paints Art/A320/Livery/*.png)
#   3. blender -b --python Art/A320/Livery/make_livery.py -- render     (preview PNGs to F:/ADP-Export/livery)
# The lettering: Blender left is -x (nose +y); each side's words are drawn readable for that side and checked in the renders.
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
GLB = os.path.join(REPO, 'Art', 'A320', 'A320.glb')
BASE = os.path.join(REPO, 'Art', 'A320', 'Textures', 'FG')
WORK = 'F:/ADP-Export/livery/work'
RENDERS = 'F:/ADP-Export/livery'
IMGS = ('Fuse-Main', 'engine-CFM56')
OUTNAME = {'Fuse-Main': 'ADP-Fuse-Main.png', 'engine-CFM56': 'ADP-engine-CFM56.png'}
MODE = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv and len(sys.argv) > sys.argv.index('--') + 1 else 'paint'

# ----------------------------------------------------------------------------------------------------------------------
if MODE == 'export':
    import bpy, numpy as np
    os.makedirs(WORK, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=GLB)
    for IMG in IMGS:
        P = []; U = []; N = []; OI = []; names = []
        for o in bpy.data.objects:
            if o.type != 'MESH': continue
            use = []
            for s in o.material_slots:
                m = s.material
                use.append(bool(m and m.use_nodes and any(n.type == 'TEX_IMAGE' and n.image and n.image.name == IMG
                                                           for n in m.node_tree.nodes)))
            if not any(use): continue
            me = o.data; me.calc_loop_triangles(); me.calc_normals_split()
            uv = me.uv_layers.active.data
            M = o.matrix_world; R = M.to_3x3().inverted().transposed()
            names.append(o.name); k = len(names) - 1
            for t in me.loop_triangles:
                if not use[t.material_index]: continue
                P.append([tuple(M @ me.vertices[v].co) for v in t.vertices])
                U.append([tuple(uv[l].uv) for l in t.loops])
                N.append([tuple((R @ me.loops[l].normal).normalized()) for l in t.loops]); OI.append(k)
        np.savez(os.path.join(WORK, IMG + '.npz'), P=np.array(P, np.float32), U=np.array(U, np.float32),
                 N=np.array(N, np.float32), O=np.array(OI, np.int32), names=np.array(names))
        print('EXPORT', IMG, len(P), 'triangles', len(names), 'parts')
    sys.exit(0)

# ----------------------------------------------------------------------------------------------------------------------
if MODE == 'render':   # blender -b --python make_livery.py -- render [res%] [only-view]
    import bpy, math
    from mathutils import Vector
    args = sys.argv[sys.argv.index('--') + 1:]
    pct = int(args[1]) if len(args) > 1 else 100
    only = args[2] if len(args) > 2 else None
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=GLB)
    for img in bpy.data.images:
        if img.name in OUTNAME:
            img.filepath = os.path.join(HERE, OUTNAME[img.name]); img.source = 'FILE'
            if img.packed_file: img.unpack(method='REMOVE')
            img.filepath = os.path.join(HERE, OUTNAME[img.name]); img.reload()
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_EEVEE'
    sc.eevee.taa_render_samples = 64; sc.eevee.use_gtao = True; sc.eevee.use_soft_shadows = True
    sc.eevee.shadow_cube_size = '2048'; sc.eevee.shadow_cascade_size = '4096'
    sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = 3840, 2160, pct
    sc.view_settings.view_transform = 'Standard'
    w = bpy.data.worlds.new('studio'); sc.world = w; w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.42, 0.44, 0.47, 1); w.node_tree.nodes['Background'].inputs[1].default_value = 0.9
    def lamp(name, rot, e):
        l = bpy.data.lights.new(name, 'SUN'); l.energy = e; l.angle = math.radians(8)
        o = bpy.data.objects.new(name, l); sc.collection.objects.link(o); o.rotation_euler = [math.radians(a) for a in rot]
    lamp('key', (50, 0, 35), 3.2); lamp('fillL', (60, 0, -120), 1.4); lamp('fillR', (60, 0, 120), 1.4)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
    cam.data.clip_end = 500
    C = Vector((0, -13.5, 4.0))
    views = {   # name: (camera position, look-at, focal mm)
        'left':  (Vector((-60, -13.5, 4.2)), C, 50),
        'right': (Vector((60, -13.5, 4.2)), C, 50),
        'tail':  (Vector((-22, -36, 9.0)), Vector((0, -28.0, 7.4)), 50),
        'top':   (Vector((0, -13.5, 62)), C, 50),
        'front34': (Vector((32, 34, 9.0)), Vector((0, -10.0, 4.0)), 40),
    }
    os.makedirs(RENDERS, exist_ok=True)
    for name, (p, at, f) in views.items():
        if only and name != only: continue
        cam.location = p; cam.data.lens = f
        dirv = at - p
        cam.rotation_euler = dirv.to_track_quat('-Z', 'Y').to_euler()
        if name == 'top': cam.rotation_euler = (0, 0, math.radians(-90))   # nose to the left
        sc.render.filepath = os.path.join(RENDERS, 'A320-ADP-livery-%s.png' % name)
        bpy.ops.render.render(write_still=True); print('RENDER', sc.render.filepath)
    sys.exit(0)

# ----------------------------------------------------------------------------------------------------------------------
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy import ndimage


def raster(d, W, H):
    """Every texel centre -> its point, smooth normal and part on the aircraft (texels outside all islands: -1)."""
    U, P, N, O = d['U'].astype(np.float64), d['P'].astype(np.float64), d['N'].astype(np.float64), d['O']
    pos = np.zeros((H, W, 3), np.float32); nrm = np.zeros((H, W, 3), np.float32); obj = np.full((H, W), -1, np.int32)
    clash = 0
    for t in range(len(U)):
        x = U[t, :, 0] * W - 0.5; y = (1.0 - U[t, :, 1]) * H - 0.5
        x0, x1 = int(np.floor(x.min())), int(np.ceil(x.max())); y0, y1 = int(np.floor(y.min())), int(np.ceil(y.max()))
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
        den = (y[1] - y[2]) * (x[0] - x[2]) + (x[2] - x[1]) * (y[0] - y[2])
        if abs(den) < 1e-12: continue
        a = ((y[1] - y[2]) * (gx - x[2]) + (x[2] - x[1]) * (gy - y[2])) / den
        b = ((y[2] - y[0]) * (gx - x[2]) + (x[0] - x[2]) * (gy - y[2])) / den
        c = 1.0 - a - b
        m = (a >= -1e-6) & (b >= -1e-6) & (c >= -1e-6)
        if not m.any(): continue
        a, b, c = a[m], b[m], c[m]; px = gx[m] % W; py = gy[m] % H
        p = a[:, None] * P[t, 0] + b[:, None] * P[t, 1] + c[:, None] * P[t, 2]
        n = a[:, None] * N[t, 0] + b[:, None] * N[t, 1] + c[:, None] * N[t, 2]
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-9)
        old = obj[py, px] >= 0
        if old.any():
            clash += int((np.linalg.norm(pos[py, px][old] - p[old], axis=1) > 0.05).sum())
        pos[py, px] = p; nrm[py, px] = n; obj[py, px] = O[t]
    return pos, nrm, obj, clash


def dilate(pos, nrm, obj, r=12):
    """Texels just outside the islands take their nearest island texel, so filtering and mips show no white edge."""
    miss = obj < 0
    dist, (iy, ix) = ndimage.distance_transform_edt(miss, return_indices=True)
    grow = miss & (dist <= r)
    pos[grow] = pos[iy[grow], ix[grow]]; nrm[grow] = nrm[iy[grow], ix[grow]]; obj[grow] = obj[iy[grow], ix[grow]]
    return grow


def load(IMG):
    d = np.load(os.path.join(WORK, IMG + '.npz'))
    base = Image.open(os.path.join(BASE, IMG + '.png'))
    return d, base


if MODE == 'probe':   # side views of the bare aircraft (design canvas), and a per-station outline of the body
    d, base = load('Fuse-Main'); W, H = base.size
    pos, nrm, obj, clash = raster(d, W, H); print('clash texels', clash)
    names = list(d['names']); rgb = np.asarray(base.convert('RGB'))
    fus = np.isin(obj, [names.index(n) for n in names if not any(n.startswith(w) for w in (
        'Wing', 'Flap', 'Slat', 'Spoiler', 'Aileron', 'Fairing', 'Hstab', 'Elevator', 'LightWing', 'EngineFairing', 'navglass'))])
    R = 0.02
    for side, s in (('L', -1), ('R', 1)):
        m = fus & (nrm[..., 0] * s > 0.2)
        cv = np.zeros((int(11.5 / R), int(38.5 / R), 3), np.uint8)
        cx = ((pos[m][:, 1] + 33.0) / R).astype(int); cy = ((12.5 - pos[m][:, 2]) / R).astype(int)
        cv[cy, cx] = rgb[m]
        Image.fromarray(cv).save(os.path.join(WORK, 'probe-side-%s.png' % side))
    f = obj == names.index('Fuselage')
    p = pos[f]
    for y in np.arange(5.0, -33, -1.0):
        q = p[(np.abs(p[:, 1] - y) < 0.05) & (np.abs(p[:, 0]) < 0.3)]
        if len(q): print('station y %6.1f  crown/keel z %.2f..%.2f  maxhalfwidth %.2f' % (
            y, q[:, 2].min(), q[:, 2].max(), np.abs(p[np.abs(p[:, 1] - y) < 0.05][:, 0]).max()))
    sys.exit(0)

# ----------------------------------------------------------------------------------------------------------------------
# The design. Blender frame: nose +y, up +z, left side -x. Lengths in metres.
from scipy.interpolate import PchipInterpolator as Pchip

NAVY = np.array([0.043, 0.075, 0.150])              # deep navy (sRGB 0..1)
GOLD_HI = np.array([0.910, 0.765, 0.455])           # gold, light edge
GOLD_LO = np.array([0.700, 0.525, 0.235])           # gold, deep edge
AA = 0.011                                          # one texel on the fuselage (~1.1 cm): edge softening width

# top of the navy belly, white gap, gold band width - as smooth curves along the fuselage (y)
_Y = [6.0, 4.2, 3.0, 1.0, -2.0, -6.0, -10.0, -13.0, -16.0, -19.0, -22.0, -25.0, -28.0, -31.0, -34.0]
_N = [1.60, 2.05, 2.40, 2.85, 3.05, 3.10, 3.06, 3.02, 3.18, 3.58, 4.30, 5.05, 5.60, 6.00, 6.30]
_GAP = [0.10, 0.10, 0.10, 0.10, 0.10, 0.11, 0.14, 0.20, 0.30, 0.42, 0.55, 0.62, 0.66, 0.70, 0.72]
_WID = [0.0, 0.0, 0.0, 0.030, 0.045, 0.08, 0.20, 0.36, 0.55, 0.75, 0.90, 0.95, 0.96, 0.96, 0.96]
zN = Pchip(_Y[::-1], _N[::-1]); gap = Pchip(_Y[::-1], _GAP[::-1]); wid = Pchip(_Y[::-1], _WID[::-1])
GOLD_FROM = 1.6                                     # the gold line starts (pointed) here, aft of the nose
FIN_Y = -21.0; FIN_Z = 6.12                         # above the gold and aft/above these: the navy fin
ZC, RC = 3.90, 2.00                                 # fuselage section centre height and radius (lettering unwrap)


def smooth01(t):
    t = np.clip(t, 0.0, 1.0); return t * t * (3 - 2 * t)


def edge(dist):
    """Coverage of a sharp edge at signed distance `dist` (m, + inside), softened over one texel."""
    return np.clip(dist / AA + 0.5, 0.0, 1.0)


def gold_col(t):
    t = np.clip(t, 0, 1)[..., None]; s = 0.5 - 0.5 * np.cos(np.pi * np.clip(t * 1.6, 0, 1))   # light band near the top
    return GOLD_LO + (GOLD_HI - GOLD_LO) * (1 - np.abs(2 * t - 1) * 0.85)


# -- vector art (drawn 4x supersampled, then reduced: crisp, smooth edges) --------------------------------------------
FONT = 'C:/Windows/Fonts/seguibl.ttf'   # Segoe UI Black


def bez(p0, p1, p2, n=40):
    t = np.linspace(0, 1, n)[:, None]; p0, p1, p2 = map(np.array, (p0, p1, p2))
    return ((1 - t) ** 2 * p0 + 2 * (1 - t) * t * p1 + t * t * p2).tolist()


def emblem(w):
    """Gold crown over spread wings, symmetric, as an L mask w x 0.8w pixels."""
    S = 4; W_, H_ = w * S, int(w * 0.8) * S
    im = Image.new('L', (W_, H_), 0); dr = ImageDraw.Draw(im)
    k = W_ / 1000.0
    P = lambda pts: [(x * k, y * k) for x, y in pts]
    cx = 500
    # crown: band + five points with balls
    dr.polygon(P([(380, 330), (620, 330), (612, 372), (388, 372)]), fill=255)
    pts = [(372, 315), (360, 175), (420, 255), (455, 140), (500, 240), (545, 140), (580, 255), (640, 175), (628, 315)]
    dr.polygon(P(pts), fill=255)
    for x, y, r in ((360, 165, 16), (455, 128, 17), (500, 92, 20), (545, 128, 17), (640, 165, 16)):
        dr.ellipse(P([(x - r, y - r), (x + r, y + r)]), fill=255)
    dr.polygon(P([(500, 104), (512, 160), (500, 240), (488, 160)]), fill=255)   # centre spike
    dr.polygon(P([(500, 395), (540, 430), (500, 470), (460, 430)]), fill=255)   # jewel under the crown
    # wings: five swept feathers a side, each a tapered crescent
    for side in (1, -1):
        for i in range(5):
            r0 = (cx + side * (55 + i * 10), 420 + i * 46)            # root
            tip = (cx + side * (470 - i * 62), 250 + i * 88)          # tip
            up = bez(r0, (cx + side * (230 - i * 20), 300 + i * 70), tip)
            th = 34 - i * 3
            lo = bez(tip, (cx + side * (250 - i * 25), 330 + i * 72 + th * 1.6), (r0[0], r0[1] + th))
            dr.polygon(P(up + lo), fill=255)
    return im.resize((w, int(w * 0.8)), Image.LANCZOS)


def text_masks(side, px):
    """Lettering 'Airport Dynasty Pro' for one side, in the (y, v) frame: v = unwrapped height on the section.
    Returns (navy L mask, gold L mask, emblem L mask, y0, v1) at px metres a pixel."""
    Y0, Y1, V0, V1 = -13.2, 0.2, 4.55, 6.10
    W_, H_ = int((Y1 - Y0) / px), int((V1 - V0) / px)
    S = 2
    nav = Image.new('L', (W_ * S, H_ * S), 0); gol = Image.new('L', (W_ * S, H_ * S), 0)
    cap = 0.70                                     # cap height (m)
    f = ImageFont.truetype(FONT, int(cap / 0.70 / px * S))
    a = 'Airport Dynasty '; b = 'Pro'
    wa = f.getlength(a); wb = f.getlength(b); total = wa + wb
    base_v = 4.98                                  # baseline height (unwrapped), above the window line
    asc = f.getmetrics()[0]
    yA_fwd = -2.75                                 # forward end of the lettering (aft of door 1 and the emblem)
    tw = total / S * px
    # drawn in the view from that side: left side looks at the nose on the LEFT, so its text starts forward
    dn = ImageDraw.Draw(nav); dg = ImageDraw.Draw(gol)
    top = (V1 - base_v) / px * S - asc
    if side == 'R':   # view from +x: nose on the right; canvas column = y; text ends at the forward end
        x0 = (yA_fwd - tw - Y0) / px * S
    else:             # view from -x: nose on the left; canvas column = -y (flipped below)
        x0 = (Y1 - yA_fwd) / px * S
    dn.text((x0, top), a, font=f, fill=255); dg.text((x0 + wa, top), b, font=f, fill=255)
    nav = nav.resize((W_, H_), Image.LANCZOS); gol = gol.resize((W_, H_), Image.LANCZOS)
    if side == 'L':
        nav = nav.transpose(Image.FLIP_LEFT_RIGHT); gol = gol.transpose(Image.FLIP_LEFT_RIGHT)
    # the crown emblem forward of the lettering (symmetric: same both sides)
    ew = 1.55; em = emblem(int(ew / px))
    emb = Image.new('L', (W_, H_), 0)
    ey = -1.40; ev = base_v + 0.42
    emb.paste(em, (int((ey - ew / 2 - Y0) / px), int((V1 - ev - ew * 0.4) / px)))
    return [np.asarray(m, np.float32) / 255 for m in (nav, gol, emb)], (Y0, V1, px)


def fin_emblem(px):
    Y0, Z1 = -31.8, 11.5
    ew = 2.90; em = emblem(int(ew / px))
    W_, H_ = int(5.5 / px), int(4.0 / px)
    can = Image.new('L', (W_, H_), 0)
    cy, cz = -29.30, 9.35                          # centre on the fin (chord there ~4 m)
    can.paste(em, (int((cy - ew / 2 - Y0) / px), int((Z1 - cz - ew * 0.4) / px)))
    return np.asarray(can, np.float32) / 255, (Y0, Z1, px)


def sample(mask, frame, y, z):
    Y0, Z1, px = frame
    return ndimage.map_coordinates(mask, [(Z1 - z) / px - 0.5, (y - Y0) / px - 0.5], order=1, mode='constant', cval=0)

# ----------------------------------------------------------------------------------------------------------------------
WINGISH = ('Wing', 'Flap', 'Slat', 'Spoiler', 'Aileron', 'Fairing', 'Hstab', 'Elevator', 'LightWing', 'EngineFairing',
           'navglass', 'AOASensor', 'WindshildWiper')   # grey/white: base paint stays


def detail(base_rgb, cover):
    """The base texture's own shading as a ratio to its local paint colour (panel lines, rivets, door gaps < 1),
    and a 'keep' weight for windows and coloured placards, which are not painted over."""
    lum = base_rgb.mean(axis=2)
    chroma = base_rgb.max(axis=2) - base_rgb.min(axis=2)
    paint = cover & (lum > 0.62) & (chroma < 0.10)                      # plain paint texels
    k = 9
    num = ndimage.uniform_filter(np.where(paint, lum, 0).astype(np.float32), 2 * k + 1)
    den = ndimage.uniform_filter(paint.astype(np.float32), 2 * k + 1)
    ref = np.where(den > 0.02, num / np.maximum(den, 1e-6), 0.92)
    ratio = np.clip(lum / np.maximum(ref, 0.3), 0.0, 1.08)
    keep = np.maximum(smooth01((0.40 - lum / np.maximum(ref, 0.3)) / 0.15), smooth01((chroma - 0.10) / 0.08))
    return ratio, keep


def lay(out, col, cov, ratio, keep, g):
    """Paint `col` with coverage `cov` as a layer: multiplied by the base shading (ratio**g)."""
    a = (cov * (1 - keep))[..., None]
    out[:] = out * (1 - a) + (col * (ratio ** g)[..., None]) * a


def paint_fuse():
    d, base = load('Fuse-Main'); W, H = base.size
    pos, nrm, obj, clash = raster(d, W, H)
    names = list(d['names'])
    cover = obj >= 0
    grow = dilate(pos, nrm, obj)
    src = np.asarray(base.convert('RGBA'), np.float32) / 255
    rgb = src[..., :3].copy()
    ratio, keep = detail(rgb, cover)
    # dilated texels have no own detail: take the nearest island texel's ratio/keep
    _, (iy, ix) = ndimage.distance_transform_edt(~cover, return_indices=True)
    ratio = ratio[iy, ix]; keep = keep[iy, ix]
    painted = np.zeros(len(names), bool)
    for i, n in enumerate(names):
        painted[i] = not n.startswith(WINGISH)
    sel = (obj >= 0) & painted[np.maximum(obj, 0)]
    fence = (obj >= 0) & np.isin(obj, [i for i, n in enumerate(names) if n.startswith('WingtipFence')])
    out = rgb.copy()
    # body, doors, tail cone, fin, rudder: projected from the side
    m = sel & ~fence
    y, z, x = pos[..., 1][m], pos[..., 2][m], pos[..., 0][m]
    nx = nrm[..., 0][m]
    r, kp = ratio[m], keep[m]
    o = out[m]
    zn = zN(y); glo = zn + gap(y); ghi = glo + wid(y) * smooth01((GOLD_FROM - y) / 2.5)
    navy = edge(zn - z)
    gold = edge(z - glo) * edge(ghi - z) * (wid(y) > 0.004)
    fin = edge(z - ghi) * edge(z - FIN_Z) * edge(FIN_Y - y)
    t = (z - glo) / np.maximum(ghi - glo, 1e-3)
    lay(o, np.broadcast_to(NAVY, o.shape), np.maximum(navy, fin), r, kp, 2.2)
    lay(o, gold_col(t), gold, r, kp, 1.3)
    # fin emblem (both sides of the fin: symmetric art)
    fe, ff = fin_emblem(0.0025)
    e = sample(fe, ff, y, z) * fin
    lay(o, gold_col(np.clip((9.35 + 1.16 - z) / 2.32, 0, 1)), e, r, kp, 1.3)
    # lettering and crown emblem, each side readable from its own side, only on the side-facing skin
    v = ZC + RC * np.arcsin(np.clip((z - ZC) / RC, -1, 1))
    sidew = smooth01((np.abs(nx) - 0.30) / 0.15)
    for s, sg in (('L', x < 0), ('R', x >= 0)):
        (tn, tg, te), tf = text_masks(s, 0.003)
        cn = sample(tn, tf, y, v) * sidew * sg; cg = sample(tg, tf, y, v) * sidew * sg; ce = sample(te, tf, y, v) * sidew * sg
        lay(o, np.broadcast_to(NAVY, o.shape), cn, r, kp, 2.2)
        lay(o, gold_col(np.clip((v - 4.98) / 0.7, 0, 1)), cg, r, kp, 1.3)
        lay(o, gold_col(np.clip((v - 4.78) / 1.24, 0, 1)), ce, r, kp, 1.3)
    out[m] = o
    # wingtip fences: navy
    o = out[fence]; lay(o, np.broadcast_to(NAVY, o.shape), np.ones(len(o)), ratio[fence], keep[fence], 2.2); out[fence] = o
    res = np.concatenate([np.clip(out, 0, 1), src[..., 3:]], axis=2)
    Image.fromarray((res * 255 + 0.5).astype(np.uint8), 'RGBA').save(os.path.join(HERE, OUTNAME['Fuse-Main']), optimize=True)
    print('Fuse-Main: %d texels painted, %d clashing texels (two places on one texel)' % (m.sum(), clash))


def paint_engine():
    d, base = load('engine-CFM56'); W, H = base.size
    pos, nrm, obj, clash = raster(d, W, H)
    names = list(d['names']); cover = obj >= 0
    dilate(pos, nrm, obj)
    src = np.asarray(base.convert('RGB'), np.float32) / 255
    ratio, keep = detail(src, cover)
    _, (iy, ix) = ndimage.distance_transform_edt(~cover, return_indices=True)
    ratio = ratio[iy, ix]; keep = keep[iy, ix]
    outer = [i for i, n in enumerate(names) if (n.startswith('EngineCFM56') or n.startswith('ReverserL') or n.startswith('ReverserR'))
             and not n.endswith('Inner')]
    m = (obj >= 0) & np.isin(obj, outer)
    p = pos[m]; n = nrm[m]
    cx = np.where(p[:, 0] < 0, -5.62, 5.62); cz = 1.79
    rad = np.stack([p[:, 0] - cx, p[:, 2] - cz], 1); rr = np.linalg.norm(rad, axis=1)
    outward = (n[:, 0] * rad[:, 0] + n[:, 2] * rad[:, 1]) / np.maximum(rr, 1e-6)
    cov = smooth01((outward - 0.15) / 0.2) * smooth01((rr - 0.80) / 0.05)
    o = src[m].copy()
    lay(o, np.broadcast_to(NAVY, o.shape), cov, ratio[m], keep[m], 2.2)
    ring = edge(p[:, 1] - (-6.74)) * edge((-6.66) - p[:, 1]) * cov           # thin gold ring behind the intake lip
    lay(o, gold_col(np.full(len(o), 0.5)), ring, ratio[m], keep[m], 1.3)
    out = src.copy(); out[m] = o
    Image.fromarray((np.clip(out, 0, 1) * 255 + 0.5).astype(np.uint8), 'RGB').save(os.path.join(HERE, OUTNAME['engine-CFM56']), optimize=True)
    print('engine: %d texels painted, %d clashing texels' % (m.sum(), clash))


if MODE == 'paint':
    paint_fuse(); paint_engine()

