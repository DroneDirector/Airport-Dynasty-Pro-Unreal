# The dog (owner, 2026-11-15: "build a dog and a cat in Blender from reference photos ... the ears should not be
# squares, the nose shouldn't be either, the legs, the body ... look at the photos in very, very high looks, each shape,
# length, width and form of the entire body"). A Labrador Retriever, male, 57 cm at the withers (FCI standard 122).
#
# The shape comes from the traced, checked outlines of the reference photos (Art/Pets/trace_outline.py): the standing
# side view (dog_side2: head level, all four legs) gives every length and height - each part read as cross-sections
# perpendicular to its own axis - and the ear outline; widths come from the breed's build (front views refine them).
# Each part is ONE continuous swept shape (an elliptic section that changes smoothly along the part's axis - no stacked
# pieces, so no rings or seams), parts meet in small smooth unions, and the surface is taken from the field (marching
# cubes) - no box, no corner anywhere. v1 (stacked ellipsoids) had ring seams, a giraffe neck and 1-2 cm of inflation.
# Writes Art/Pets/Dog/dog_body.ply (cm: X forward to the nose, Y to the dog's left, Z up, the ground at Z 0).
#   python Art/Pets/Dog/build_dog.py
import json, math, os
import numpy as np
from skimage import measure
from scipy.interpolate import splprep, splev

HERE = os.path.dirname(os.path.abspath(__file__))
MASK = np.load(os.path.join(HERE, 'dog_side2_mask.npy'))
GROUND_PX, WITHERS_PX, REAR_PX = 466.0, 148.0, 500.0
K = 57.0 / (GROUND_PX - WITHERS_PX)               # cm per photo pixel (withers 57 cm)


def P(x, y):
    """Photo pixel -> model (X forward, Z up) cm (the photo's dog faces left)."""
    return np.array([(REAR_PX - x) * K, (GROUND_PX - y) * K])


def section(a_px, b_px, t, reach_px=80):
    """The outline's cross-section perpendicular to the photo line a->b at fraction t: (centre px, half-thickness px)."""
    a, b = np.array(a_px, float), np.array(b_px, float)
    c = a + (b - a) * t; d = (b - a) / np.linalg.norm(b - a); n = np.array([-d[1], d[0]])
    def run(sign):
        for s in np.arange(0, reach_px, 0.5):
            q = c + n * s * sign; xi, yi = int(round(q[0])), int(round(q[1]))
            if not (0 <= yi < MASK.shape[0] and 0 <= xi < MASK.shape[1]) or not MASK[yi, xi]: return s
        return reach_px
    s1, s2 = run(1), run(-1)
    return c + n * (s1 - s2) / 2, (s1 + s2) / 2


_TOP = np.array([np.nonzero(MASK[:, i])[0][0] if MASK[:, i].any() else MASK.shape[0] for i in range(MASK.shape[1])], float)
from scipy.ndimage import gaussian_filter1d, gaussian_filter
_TOPS = gaussian_filter1d(_TOP, 3.0)


def col_top(x):
    """The outline's top edge at photo column x, smoothed over about 1 cm (fur tufts and tracing steps left out)."""
    return float(np.interp(x, np.arange(len(_TOPS)), _TOPS))


# ---------------- the field ----------------
RES = 0.25
LO = np.array([-30.0, -18.0, -1.0]); HI = np.array([93.0, 18.0, 76.0])
SHAPE = np.ceil((HI - LO) / RES).astype(int) + 1
F = np.full(SHAPE, 50.0, np.float32)


def box_of(lo, hi):
    i0 = np.clip(np.floor((np.array(lo) - LO) / RES).astype(int), 0, SHAPE - 1)
    i1 = np.clip(np.ceil((np.array(hi) - LO) / RES).astype(int) + 1, 0, SHAPE)
    xs = [LO[k] + RES * np.arange(i0[k], i1[k]) for k in range(3)]
    X, Y, Z = np.meshgrid(*xs, indexing='ij')
    return (slice(i0[0], i1[0]), slice(i0[1], i1[1]), slice(i0[2], i1[2])), np.stack([X, Y, Z], -1)


def smin(a, b, k):
    if k <= 0: return np.minimum(a, b)
    h = np.maximum(k - np.abs(a - b), 0) / k
    return np.minimum(a, b) - h * h * k * 0.25


def put(d, sl, k):
    F[sl] = smin(F[sl], d.astype(np.float32), k)


def ellipsoid(c, r, k=1.0):
    c, r = np.array(c, float), np.array(r, float); m = r.max() + k + 1
    sl, p = box_of(c - m, c + m); p = p - c
    k0 = np.linalg.norm(p / r, axis=-1); k1 = np.linalg.norm(p / (r * r), axis=-1)
    put(k0 * (k0 - 1.0) / np.maximum(k1, 1e-6), sl, k)


def sweep(axis, ry, rz, k=1.0, side=(0.0, 1.0, 0.0), inset=False, pw=2.0):
    """One continuous part along a 3D axis polyline: at axis point i the section is an ellipse with half width ry[i]
    (across, along `side`) and half height rz[i] (the other way); between points everything changes smoothly (spline).
    The ends are rounded by the section itself; with inset the axis is shortened by the end sections' radius so the
    rounded end lands where the outline ends (v2 overshot the nose by 2.5 cm without it) - only at free ends (nose, tail
    tip): at a joint it opened gaps (v5: paws floating under the legs). pw > 2 makes the section a rounded box (a
    superellipse - the Labrador's muzzle seen from the front)."""
    A = np.array(axis, float); ry = np.array(ry, float); rz = np.array(rz, float)
    n = max(30, len(A) * 8)
    uu = np.linspace(0, 1, n)
    if len(A) >= 4:
        tck, u = splprep(A.T, s=0, k=3); A2 = np.array(splev(uu, tck)).T
        ry2 = np.interp(uu, u, ry); rz2 = np.interp(uu, u, rz)
    else:
        s0 = np.linspace(0, 1, len(A))
        A2 = np.array([np.interp(uu, s0, A[:, j]) for j in range(3)]).T; ry2 = np.interp(uu, s0, ry); rz2 = np.interp(uu, s0, rz)
    if inset:   # (trim the axis itself by the end sections' radius - moving only the end point left the rest reaching out)
        cl = np.r_[0, np.cumsum(np.linalg.norm(A2[1:] - A2[:-1], axis=1))]
        c0 = min(ry2[0], rz2[0]) * 0.9; c1 = cl[-1] - min(ry2[-1], rz2[-1]) * 0.9
        if c1 > c0 + 0.5:
            cs = np.linspace(c0, c1, n)
            A2 = np.array([np.interp(cs, cl, A2[:, j]) for j in range(3)]).T
            ry2 = np.interp(cs, cl, ry2); rz2 = np.interp(cs, cl, rz2)
    m = max(ry2.max(), rz2.max()) + k + 1
    slA, pA = box_of(A2.min(0) - m, A2.max(0) + m)
    T = np.full(pA.shape[:-1], 50.0, np.float32)
    o = np.array([slA[0].start, slA[1].start, slA[2].start])
    S = np.array(side, float); S /= np.linalg.norm(S)
    for i in range(len(A2) - 1):
        a0, a1 = A2[i], A2[i + 1]; seg = a1 - a0; L = np.linalg.norm(seg); d = seg / L
        mm = max(ry2[i], rz2[i], ry2[i + 1], rz2[i + 1]) + 0.5
        sl, p = box_of(np.minimum(a0, a1) - mm, np.maximum(a0, a1) + mm)
        w = p - a0; along0 = w @ d; t = np.clip(along0 / L, 0, 1); along = along0 - t * L
        r = w - (t * L)[..., None] * d
        sy = S - d * (S @ d); sy /= np.linalg.norm(sy); sz = np.cross(d, sy)
        a = ry2[i] + (ry2[i + 1] - ry2[i]) * t; b = rz2[i] + (rz2[i + 1] - rz2[i]) * t
        e = (np.abs(r @ sy / a) ** pw + np.abs(r @ sz / b) ** pw) ** (1.0 / pw); dd = (e - 1.0) * np.minimum(a, b)
        mn = np.minimum(a, b)   # (beyond an end: a dome of the section's size - the old formula made cones, v5's knee points)
        dd = np.where(np.abs(along) > 0, np.sqrt(along ** 2 + (e * mn) ** 2) - mn, dd)
        ls = tuple(slice(sl[q].start - o[q], sl[q].stop - o[q]) for q in range(3))
        T[ls] = np.minimum(T[ls], dd)
    put(T, slA, k)


def carve(c, r, k=0.3):
    """Cuts an ellipsoid out of the surface (nostrils, the groove down the nose), edges softened by k."""
    c, r = np.array(c, float), np.array(r, float); m = r.max() + k + 1
    sl, p = box_of(c - m, c + m); p = p - c
    k0 = np.linalg.norm(p / r, axis=-1); k1 = np.linalg.norm(p / (r * r), axis=-1)
    d = (k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)).astype(np.float32)
    F[sl] = -smin(-F[sl], d, k)


def interp(prof, x):
    xs, vs = zip(*prof); return float(np.interp(x, xs, vs))


# ================= torso: one sweep along the back, rump to chest =================
under = [(150, 262), (170, 285), (200, 302), (236, 306), (300, 293), (340, 281), (380, 272), (420, 276), (450, 283), (488, 288), (498, 262)]
wid = [(150, 8.0), (175, 11.5), (210, 13.4), (260, 14.6), (300, 14.8), (345, 13.0), (385, 11.2), (425, 12.4), (462, 13.0), (485, 11.5), (498, 7.5)]
ax, ry, rz = [], [], []
for x in np.arange(166, 499, 6.0):
    top = col_top(x) if x > 236 else col_top(236) + (236 - x) * 0.0
    if x <= 236: top = min(col_top(x), 147.0) if x > 200 else 150.0   # (the neck rises out of the withers; the torso stops at them)
    bot = interp(under, x); c = P(x, (top + bot) / 2)
    ax.append((c[0], 0, c[1])); ry.append(interp(wid, x)); rz.append((bot - top) / 2 * K)
ax = np.array(ax); ax[:, 2] = gaussian_filter1d(ax[:, 2], 2.0, mode='nearest'); rz = gaussian_filter1d(np.array(rz), 2.0, mode='nearest')
sweep(ax[::-1], ry[::-1], rz[::-1], k=0)

# ================= neck: sweep along its own axis, perpendicular sections from the outline =================
na, nb = (214, 232), (126, 108)
ax, ry, rz = [], [], []
for t in np.linspace(0.0, 1.0, 14):
    c, h = section(na, nb, t, 80); C = P(*c)
    ax.append((C[0], 0, C[1])); rz.append(min(h * K, 11.5 - 1.5 * t)); ry.append(10.5 - 3.0 * t)       # (neck 21 cm wide at the shoulders -> 15 at the head)
sweep(ax, ry, rz, k=3.0)

# ================= head: the skull and the muzzle, swept along the head with columns from the outline ==========
# owner 2026-11-15 on v10's front view: the snout, the nose and the chin must look like the front photos - the muzzle
# is broad with near-straight sides to the nose (about 10-11 cm across the lips, measured from Yellow_Lab.JPG against the
# head's width at the eyes, perspective taken out), the lips close it into a rounded box, the chin a soft round under them;
# the nose leather is broad (about 4 cm), wider than tall, flat in front, nostrils curling at its lower sides, a groove
# down its middle. v10 had a round ball nose, the muzzle narrowing to it and two lip lumps with a gap under the nose.
jaw = [(22, 128), (34, 137), (60, 141), (80, 142), (96, 146), (112, 150), (126, 148), (136, 138)]
def head_sweep(x0, x1, hw, pw, k, inset):
    ax, ry, rz = [], [], []
    for x in np.arange(x0, x1 + 0.1, 3.0):
        top = col_top(x); bot = interp(jaw, x); c = P(x, (top + bot) / 2)
        ax.append((c[0], 0, c[1])); ry.append(interp(hw, x)); rz.append((bot - top) / 2 * K)
    sweep(ax[::-1], ry[::-1], rz[::-1], k=k, inset=inset, pw=pw)
# the muzzle: a rounded box (superellipse) from the nose back to the eyes
# (muzzle 0.62 of the skull's width - the snow photo; v11 was as wide as the skull. It ends behind the nose so the
#  nose leather sits on its top front and shows - v11's muzzle swallowed the nose)
head_sweep(31, 82, [(31, 3.9), (40, 4.4), (56, 4.6), (70, 4.8), (82, 5.6)], 2.6, 1.0, True)
# the skull: round, broad over the cheeks
head_sweep(70, 137, [(70, 5.4), (80, 6.2), (90, 7.3), (106, 7.8), (120, 7.4), (137, 5.5)], 2.2, 2.0, False)
# the nose leather at the muzzle's top front: broad, flatter in front; the nostrils curl in at its lower sides and a
# groove runs down its middle to the lip
NOSE = P(21, 104)
ellipsoid((NOSE[0] - 1.5, 0, NOSE[1] + 0.25), (2.1, 2.65, 1.95), k=0.7)          # (v12's nose was too small next to the photos)
# the upper lips: from under the nose down and back along each side, drooping a little at the corner of the mouth
for s in (-1, 1):
    sweep([(NOSE[0] - 1.6, s * 1.2, NOSE[1] - 2.1), (NOSE[0] - 4.0, s * 3.2, NOSE[1] - 3.0), (P(60, 0)[0], s * 3.9, P(0, 140)[1]),
           (P(80, 0)[0], s * 3.7, P(0, 143)[1])], [1.2, 1.5, 1.6, 1.2], [1.4, 1.7, 1.7, 1.2], k=1.2)
# (the nostrils and the groove are cut after the body's smoothing - see below - or the blur fills them in)
for s in (-1, 1):
    ellipsoid((P(97, 0)[0], s * 4.6, P(0, 120)[1]), (4.0, 3.0, 3.0), k=1.5)               # cheek
    ellipsoid((P(77, 0)[0], s * 2.7, P(0, 80)[1]), (2.0, 2.0, 1.2), k=1.2)                # brow over the eye (the stop)

# ================= legs =================
FY, RY = 7.2, 7.6          # half the distance between the legs' centres (front, rear)
for s in (-1, 1):
    # --- front: the shoulder blade lies along the chest under the withers; upper arm; forearm (outline); pastern; paw
    sweep([(*P(236, 160)[:1], s * 9.0, P(0, 160)[1]), (P(214, 0)[0], s * 9.6, P(0, 205)[1]), (P(186, 0)[0], s * 9.0, P(0, 248)[1])],
          [3.0, 3.6, 4.2], [5.5, 6.0, 5.0], k=3.0)
    sweep([(P(186, 0)[0], s * 8.4, P(0, 250)[1]), (P(202, 0)[0], s * 7.8, P(0, 282)[1]), (P(214, 0)[0], s * FY, P(0, 306)[1])],
          [4.4, 4.2, 3.8], [5.2, 4.8, 4.0], k=2.5)
    ax, ry, rz = [], [], []
    for t in np.linspace(0, 1, 8):
        c, h = section((211, 304), (208, 398), t, 30); C = P(*c)
        ax.append((C[0], s * FY, C[1])); r = min(max(h * K, 3.0), 3.7); rz.append(r); ry.append(r * 1.05)
    ax.append((P(205, 0)[0], s * FY, P(0, 428)[1])); ry.append(2.7); rz.append(2.6)     # pastern, slightly sloping
    sweep(ax, ry, rz, k=1.5, side=(0, 1, 0))
    # paw: round, compact; the pad below and four toes with their knuckles
    pc = np.array([P(190, 0)[0] + 0.6, s * FY, 2.4])
    ellipsoid(pc, (4.0, 3.5, 2.4), k=1.2)
    for dx, dy in [(3.1, -1.95), (3.9, -0.68), (3.9, 0.68), (3.1, 1.95)]:
        ellipsoid((pc[0] + dx - 0.4, pc[1] + dy, 1.6), (1.5, 1.1, 1.6), k=0.5)
    ellipsoid((pc[0] - 2.6, s * FY, 1.8), (1.6, 1.8, 1.8), k=0.8)                         # the heel pad
    # --- rear: the thigh (one big muscle from the hip down to the stifle), the second thigh (outline) to the hock,
    #     metatarsus, paw
    sweep([(P(470, 0)[0], s * 7.4, P(0, 205)[1]), (P(462, 0)[0], s * 7.9, P(0, 250)[1]), (P(452, 0)[0], s * 7.8, P(0, 300)[1]),
           (P(448, 0)[0], s * RY, P(0, 332)[1])], [4.2, 4.6, 4.2, 3.4], [6.4, 7.0, 6.0, 4.2], k=4.0)
    ax, ry, rz = [], [], []
    for t in np.linspace(0, 1, 9):
        c, h = section((448, 332), (506, 392), t, 40); C = P(*c)
        r = min(h * K, 4.8); ax.append((C[0], s * RY, C[1])); rz.append(r); ry.append(max(2.6, r * 0.7))
    sweep(ax, ry, rz, k=1.5, side=(0, 1, 0))
    hk, mt = P(507, 392), P(492, 446)
    sweep([(hk[0], s * RY, hk[1]), (mt[0], s * RY, mt[1])], [2.0, 1.9], [2.6, 2.2], k=1.5)
    pc = np.array([P(484, 0)[0] + 1.6, s * RY, 2.3])
    ellipsoid(pc, (3.8, 3.2, 2.3), k=1.2)
    for dx, dy in [(3.0, -1.8), (3.7, -0.62), (3.7, 0.62), (3.0, 1.8)]:
        ellipsoid((pc[0] + dx - 0.4, pc[1] + dy, 1.5), (1.4, 1.05, 1.5), k=0.5)
    ellipsoid((pc[0] - 2.4, s * RY, 1.8), (1.5, 1.7, 1.7), k=0.8)

# ================= tail: the thick "otter" tail, at rest hanging in a gentle curve =================
base = P(494, 168)
tl = [base + [0.5, 0], base + [-6.0, -4.0], base + [-11.5, -12.0], base + [-14.5, -22.0], base + [-15.5, -32.0], base + [-15.0, -38.0]]
sweep([(p[0], 0, p[1]) for p in tl], [3.4, 3.1, 2.7, 2.2, 1.6, 0.9], [3.4, 3.1, 2.7, 2.2, 1.6, 0.9], k=1.5, inset=True)

# ================= the field -> the surface =================
F = gaussian_filter(np.minimum(F, 6.0), 0.55 / RES)      # (0.55 cm blur of the body: every crease and join rounds off)

# ================= the nose's fine shapes, after the smoothing =================
for s_ in (-1, 1):
    carve((NOSE[0] + 0.3, s_ * 1.2, NOSE[1] - 0.3), (1.0, 0.65, 0.5), k=0.25)           # nostril
    carve((NOSE[0] - 0.1, s_ * 1.85, NOSE[1] - 0.05), (0.7, 0.28, 0.6), k=0.2)           # its slit curling up the side
carve((NOSE[0] + 0.25, 0, NOSE[1] - 1.05), (0.9, 0.16, 0.75), k=0.2)                         # the groove (philtrum), to the lip only

# ================= eyes: sockets in the head; the eyeballs are their own objects (positions to dog_parts.json) ======
EYE = P(76, 87)
eyes = []
for s_ in (-1, 1):
    # the eye sits on the head's surface at its height, looking forward and out (about 25 degrees)
    ix = int(round((EYE[0] - LO[0]) / RES)); iz = int(round((EYE[1] - LO[2]) / RES))
    colm = F[ix, :, iz]; ys_ = LO[1] + RES * np.nonzero(colm < 0)[0]
    ysurf = ys_.max() if s_ > 0 else ys_.min()
    c = np.array([EYE[0] - 0.4, ysurf - s_ * 0.95, EYE[1]])
    sl, p_ = box_of(c - 2.2, c + 2.2)
    d = np.linalg.norm(p_ - c, axis=-1) - 1.25
    F[sl] = np.maximum(F[sl], -d + 0.0 * 0)                       # (the socket)
    eyes.append({'centre_cm': c.round(3).tolist(), 'radius_cm': 1.1})

# ================= ears: the traced ear outline, a thin leaf lying on the side of the head =================
import cv2
ear_px = np.load(os.path.join(HERE, 'dog_side2_ear_outline.npy'))
ear = np.array([P(x, y) for x, y in ear_px])                       # (X, Z) cm
ex0, ez0 = ear.min(0) - 1.0; ex1, ez1 = ear.max(0) + 1.0
ix0, ix1 = int((ex0 - LO[0]) / RES), int((ex1 - LO[0]) / RES) + 1
iz0, iz1 = int((ez0 - LO[2]) / RES), int((ez1 - LO[2]) / RES) + 1
gx = LO[0] + RES * np.arange(ix0, ix1); gz = LO[2] + RES * np.arange(iz0, iz1)
GX, GZ = np.meshgrid(gx, gz, indexing='ij')
_m = np.zeros((len(gz), len(gx)), np.uint8)
cv2.fillPoly(_m, [np.round(np.c_[(ear[:, 0] - gx[0]) / RES, (ear[:, 1] - gz[0]) / RES] * 8).astype(np.int32)], 1, shift=3)
inside = _m.T.astype(bool)
from scipy.ndimage import distance_transform_edt
d2 = (distance_transform_edt(~inside) - distance_transform_edt(inside)) * RES      # signed 2D distance to the ear's edge
d2 = gaussian_filter(d2, 0.35 / RES)                                               # (a smooth edge: the raster made it ragged)
ear_top = ear[:, 1].max()
for s_ in (-1, 1):
    # the head/neck surface under the ear (outermost skin at each X, Z), smoothed: the ear follows the head's curve
    sub = F[ix0:ix1, :, iz0:iz1]
    yy = LO[1] + RES * np.arange(F.shape[1])
    inn = sub < 0
    if s_ > 0: ys = np.where(inn.any(1), (yy[None, :, None] * inn).max(1), 0.0)
    else: ys = np.where(inn.any(1), (yy[None, :, None] * inn + 1e3 * ~inn).min(1), 0.0)
    ys = gaussian_filter(ys, 1.2 / RES)
    # the ear hangs straight down from where it is set on the skull: below the head's widest point it does not follow
    # the cheek and neck inward (v8: the ears lay flat on the head like stickers)
    order = np.argsort(-gz)                                          # (top down)
    run = ys[:, order]; run = (np.maximum.accumulate(run, axis=1) if s_ > 0 else np.minimum.accumulate(run, axis=1))
    ys = np.empty_like(ys); ys[:, order] = run
    hang = np.clip((ear_top - GZ) / 12.0, 0, 1)                    # (the ear stands off more toward its tip)
    # the ear's rear edge flares out from the head (front photo: the ears' outer edges span 1.6 x the head's width at
    # the eyes - v9 had them lying on the cheeks); the front edge stays close to the cheek
    back = np.clip((ear[:, 0].max() - GX) / (ear[:, 0].max() - ear[:, 0].min()), 0, 1)
    ysurf = ys + s_ * (0.5 + 1.0 * hang + 3.6 * back ** 1.3 * (0.35 + 0.65 * np.sin(np.pi * np.clip(hang * 1.1, 0, 1))))
    T = 0.32 - 0.08 * hang                                          # (half thickness: leather + fur, thinner at the tip)
    Yg = LO[1] + RES * np.arange(F.shape[1])
    dE = np.maximum(d2[:, None, :], np.abs(Yg[None, :, None] - ysurf[:, None, :]) - T[:, None, :])
    F[ix0:ix1, :, iz0:iz1] = smin(F[ix0:ix1, :, iz0:iz1], dE.astype(np.float32), 0.35)
json.dump({'eyes': eyes}, open(os.path.join(HERE, 'dog_parts.json'), 'w'), indent=1)

      # (0.55 cm blur of the field: every crease and join rounds off)
v, f, n, _ = measure.marching_cubes(F, 0.0, spacing=(RES, RES, RES))
v += LO
print('body: %d vertices, %d triangles; size %s cm' % (len(v), len(f), (v.max(0) - v.min(0)).round(1)))
with open(os.path.join(HERE, 'dog_body.ply'), 'wb') as fp:
    fp.write(('ply\nformat binary_little_endian 1.0\nelement vertex %d\nproperty float x\nproperty float y\nproperty float z\n'
              'element face %d\nproperty list uchar int vertex_indices\nend_header\n' % (len(v), len(f))).encode())
    fp.write(v.astype('<f4').tobytes())
    ff = np.empty(len(f), dtype=[('n', 'u1'), ('i', '<i4', (3,))]); ff['n'] = 3; ff['i'] = f[:, ::-1]
    fp.write(ff.tobytes())
print('wrote dog_body.ply')
