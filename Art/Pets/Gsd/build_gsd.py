# The German Shepherd (owner, 2026-11-15): built from the owner's own reference sheets - front, right profile, back and
# left profile, all standing (Art/Pets/Reference/dog_german_shepherd/sheet_main.jpg), the face from front_closeup.jpg,
# checked against the four three-quarter views (sheet_three_quarter.jpg). ONE dog: the views only show its shape from
# each side. Standing is the rest pose (the owner: choose standing or sitting); sitting is one of the moves.
# Lengths and heights from the left profile (traced, checked: Art/Pets/trace_outline.py), widths from the front and back
# views at the same scale (their ground-to-ear-tip height matched to the profile's), ears from the front close-up and
# the profile. Same method as the Labrador (Art/Pets/Dog/build_dog.py): each part one continuous swept shape, smooth
# unions, the field blurred 0.55 cm, the surface by marching cubes - no box, no corner.
# Writes Art/Pets/Gsd/gsd_body.ply (cm: X forward to the nose, Y to the dog's left, Z up, the ground at Z 0).
#   python Art/Pets/Gsd/build_gsd.py
import json, math, os
import numpy as np
from skimage import measure
from scipy.interpolate import splprep, splev

HERE = os.path.dirname(os.path.abspath(__file__))
MASK = np.load(os.path.join(HERE, 'gsd_left_mask.npy'))
GROUND_PX, WITHERS_PX, REAR_PX = 526.0, 150.0, 640.0
K = 62.5 / (GROUND_PX - WITHERS_PX)               # cm per profile pixel (withers 62.5 cm: FCI 166, males 60-65)


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
LO = np.array([-14.0, -19.0, -1.0]); HI = np.array([104.0, 19.0, 88.0])
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



# ---- the front and back views, at the profile's scale (ground to ear tip: front 548 px, back 524 px, profile 519 px)
KF = K * 519.0 / 548.0; KB = K * 519.0 / 524.0

# ================= torso: one sweep along the back, rump to chest =================
# underline: the brisket (deep chest, 366 px) under the front legs, the tuck-up at the loin (345), up into the thigh
under = [(262, 330), (290, 362), (330, 366), (396, 362), (430, 352), (446, 345), (470, 356), (500, 368), (540, 360), (590, 330), (632, 290)]
# half widths (cm): front view 27 cm across the shoulders with the coat, ribs, loin, the back view's 28 cm hips with fur
wid = [(262, 9.5), (290, 12.6), (330, 13.4), (380, 13.0), (430, 11.2), (470, 10.4), (520, 11.8), (560, 12.6), (600, 11.0), (632, 8.0)]
ax, ry, rz = [], [], []
for x in np.arange(276, 633, 6.0):
    top = col_top(x) if x > 300 else min(col_top(x), 150.0); bot = interp(under, x); c = P(x, (top + bot) / 2)
    ax.append((c[0], 0, c[1])); ry.append(interp(wid, x)); rz.append((bot - top) / 2 * K)
ax = np.array(ax); ax[:, 2] = gaussian_filter1d(ax[:, 2], 2.0, mode='nearest'); rz = gaussian_filter1d(np.array(rz), 2.0, mode='nearest')
sweep(ax[::-1], ry[::-1], rz[::-1], k=0)

# ================= neck: strong, with the ruff; sections perpendicular to its axis from the outline =================
na, nb = (300, 250), (160, 150)
ax, ry, rz = [], [], []
for t in np.linspace(0.0, 1.0, 16):
    c, h = section(na, nb, t, 110); C = P(*c)
    ax.append((C[0], 0, C[1])); rz.append(min(h * K, 13.0 - 3.0 * t)); ry.append(11.0 - 3.4 * t)
ax = np.array(ax); ax[:, 2] = gaussian_filter1d(ax[:, 2], 1.5, mode='nearest'); rz = gaussian_filter1d(np.array(rz), 1.5, mode='nearest')
sweep(ax, ry, rz, k=3.0)

# ================= head: wedge-shaped - skull broad between the ears, the muzzle long and tapering to the nose ========
jaw = [(24, 150), (36, 163), (48, 172), (60, 176), (84, 176), (108, 175), (124, 178), (140, 178)]
# the skull's top behind the stop: in the profile the ears stand on it, so the outline's top edge there is the ear,
# not the head (g1 grew a dome up to the ear tips) - the head's own top line, read under the ears on the gridded crop
skull_top = [(96, 81), (108, 71), (118, 67), (130, 66), (145, 69), (160, 76), (175, 88)]
def head_top(x):
    return col_top(x) if x < 104 else interp(skull_top, x)
def head_sweep(x0, x1, hw, pw, k, inset):
    ax, ry, rz = [], [], []
    for x in np.arange(x0, x1 + 0.1, 3.0):
        top = head_top(x); bot = interp(jaw, x); c = P(x, (top + bot) / 2)
        ax.append((c[0], 0, c[1])); ry.append(interp(hw, x)); rz.append((bot - top) / 2 * K)
    sweep(ax[::-1], ry[::-1], rz[::-1], k=k, inset=inset, pw=pw)
head_sweep(30, 100, [(30, 2.4), (44, 2.9), (60, 3.3), (80, 3.9), (100, 5.0)], 2.4, 1.0, True)          # muzzle
head_sweep(88, 150, [(88, 4.6), (100, 5.6), (114, 6.6), (130, 7.0), (150, 6.2)], 2.2, 2.0, False)     # skull
NOSE = P(26, 148)
ellipsoid((NOSE[0] - 1.3, 0, NOSE[1] + 0.1), (1.6, 1.9, 1.5), k=0.6)                     # the nose leather
for s in (-1, 1):
    ellipsoid((P(110, 0)[0], s * 4.6, P(0, 150)[1]), (3.6, 2.4, 2.8), k=1.5)              # cheek
    ellipsoid((P(92, 0)[0], s * 2.6, P(0, 98)[1]), (1.8, 1.8, 1.0), k=1.2)                # brow (a slight stop)
    sweep([(NOSE[0] - 1.4, s * 1.0, NOSE[1] - 2.0), (NOSE[0] - 4.0, s * 2.3, NOSE[1] - 2.7), (P(70, 0)[0], s * 3.0, P(0, 170)[1]),
           (P(100, 0)[0], s * 3.4, P(0, 175)[1])], [1.0, 1.2, 1.3, 1.1], [1.2, 1.4, 1.4, 1.1], k=1.0)      # upper lips

# ================= legs (the near leg read from the profile; the far one mirrored) =================
FY, RY = 6.6, 8.2          # half the distance between the legs' centres (front view 13.2 cm, back view 16.5 cm)
for s in (-1, 1):
    # front: shoulder blade, upper arm, elbow, forearm (outline), pastern (sloping ~25 degrees), paw
    sweep([(P(318, 0)[0], s * 9.5, P(0, 175)[1]), (P(300, 0)[0], s * 10.2, P(0, 235)[1]), (P(270, 0)[0], s * 9.6, P(0, 290)[1])],
          [3.2, 3.8, 4.2], [5.8, 6.4, 5.4], k=3.0)
    sweep([(P(270, 0)[0], s * 8.8, P(0, 292)[1]), (P(282, 0)[0], s * 7.8, P(0, 330)[1]), (P(286, 0)[0], s * FY, P(0, 366)[1])],
          [4.4, 4.2, 3.8], [5.4, 5.0, 4.2], k=2.5)
    ax, ry, rz = [], [], []
    for t in np.linspace(0, 1, 8):
        c, h = section((268, 368), (262, 466), t, 34); C = P(*c)
        r = min(max(h * K, 2.6), 3.4); ax.append((C[0], s * FY, C[1])); rz.append(r); ry.append(r * 0.95)
    ax.append((P(250, 0)[0], s * FY, P(0, 500)[1])); ry.append(2.3); rz.append(2.2)
    sweep(ax, ry, rz, k=1.5)
    pc = np.array([P(238, 0)[0] + 0.4, s * FY, 2.3])
    ellipsoid(pc, (4.1, 3.4, 2.3), k=1.2)
    for dx, dy in [(3.2, -1.9), (4.0, -0.66), (4.0, 0.66), (3.2, 1.9)]:
        ellipsoid((pc[0] + dx - 0.4, pc[1] + dy, 1.5), (1.5, 1.05, 1.5), k=0.5)
    ellipsoid((pc[0] - 2.7, s * FY, 1.8), (1.6, 1.7, 1.8), k=0.8)
    # rear: the thigh (one big muscle, the fur "trousers"), the second thigh to the hock (outline), metatarsus, paw
    sweep([(P(590, 0)[0], s * 8.4, P(0, 248)[1]), (P(575, 0)[0], s * 9.4, P(0, 290)[1]), (P(545, 0)[0], s * 9.0, P(0, 340)[1]),
           (P(515, 0)[0], s * RY, P(0, 372)[1])], [4.6, 5.4, 5.0, 3.6], [8.0, 8.6, 7.0, 4.2], k=4.0)
    ax, ry, rz = [], [], []
    for t in np.linspace(0, 1, 9):
        c, h = section((515, 374), (636, 462), t, 50); C = P(*c)
        r = min(h * K, 4.6); ax.append((C[0], s * RY, C[1])); rz.append(r); ry.append(max(2.5, r * 0.65))
    sweep(ax, ry, rz, k=1.5)
    hk, mt = P(638, 466), P(612, 508)
    sweep([(hk[0], s * RY, hk[1]), (mt[0], s * RY, mt[1])], [2.0, 1.9], [2.6, 2.2], k=1.5)
    pc = np.array([P(600, 0)[0] + 1.2, s * RY, 2.2])
    ellipsoid(pc, (3.9, 3.2, 2.2), k=1.2)
    for dx, dy in [(3.0, -1.8), (3.8, -0.62), (3.8, 0.62), (3.0, 1.8)]:
        ellipsoid((pc[0] + dx - 0.4, pc[1] + dy, 1.45), (1.45, 1.0, 1.45), k=0.5)
    ellipsoid((pc[0] - 2.5, s * RY, 1.8), (1.5, 1.6, 1.7), k=0.8)

# ================= tail: long and bushy, hanging almost to the ground in a gentle curve (the profile) ===============
tl = [P(624, 214), P(640, 280), P(652, 350), P(664, 420), P(676, 470), P(684, 500)]
sweep([(p[0], 0, p[1]) for p in tl], [3.2, 3.6, 3.7, 3.4, 2.6, 1.4], [3.2, 3.6, 3.7, 3.4, 2.6, 1.4], k=2.0, inset=True)

# ================= the field -> the surface =================
# ================= one dog seen from every side: carved to the profile, the front and the back at once ==========
# (owner, 2026-11-15: "the model quality is very far off" - g2 followed the profile only and guessed every width.) A
# point of the dog must lie inside the dog's outline in each view it is seen from; the rounded parts above stay as the
# shape's inside (so no view's straight-on carving makes it boxy), trimmed to that carved shape.
from scipy.ndimage import distance_transform_edt as _edt
_X = LO[0] + RES * np.arange(SHAPE[0]); _Y = LO[1] + RES * np.arange(SHAPE[1]); _Z = LO[2] + RES * np.arange(SHAPE[2])
def _inside(mask, cols, rows):
    ok = (cols >= 0) & (cols < mask.shape[1]); okr = (rows >= 0) & (rows < mask.shape[0])
    m = mask[np.clip(rows, 0, mask.shape[0] - 1)][:, np.clip(cols, 0, mask.shape[1] - 1)]
    return m & okr[:, None] & ok[None, :]                                   # (rows x cols)
# the left profile: X, Z
prof = _inside(MASK, np.round(REAR_PX - _X / K).astype(int), np.round(GROUND_PX - _Z / K).astype(int))    # (Z, X)
hull = prof.T[:, None, :].copy()
# the front (the high-resolution close-up of the sheet's front view): Y, Z - the dog's left on the picture's right
KFC = K * 519.0 / 2072.0
FM = np.load(os.path.join(HERE, 'gsd_front_closeup_mask.npy'))
front = _inside(FM, np.round(530 + _Y / KFC).astype(int), np.round(2072 - _Z / KFC).astype(int))          # (Z, Y)
hull = hull & front.T[None, :, :]
# the back: Y, Z - the dog's left on the picture's left
KBK = K * 519.0 / 524.0
BM = np.load(os.path.join(HERE, 'gsd_back_mask.npy'))
back = _inside(BM, np.round(121.5 - _Y / KBK).astype(int), np.round(533 - _Z / KBK).astype(int))
hull = hull & back.T[None, :, :]
HD = ((_edt(~hull) - _edt(hull)) * RES).astype(np.float32)                 # (signed distance to the carved shape)
HD = gaussian_filter(HD, 0.8 / RES)                                         # (g3: stair stripes from the pictures' pixel rows)
del hull, prof, front, back
# the shape: inside the carving AND inside the rounded parts grown by 0.4 cm, blended over 2.5 cm - the carving's
# straight cuts round off (g3 with 1.2 cm / 1.0: flat slab sides)
F = -smin(-HD, -(F - 0.4), 2.5)
del HD
F = gaussian_filter(np.minimum(F, 6.0), 0.55 / RES)      # (0.55 cm blur: every crease and join rounds off)

# the nose's fine shapes, after the smoothing
for s_ in (-1, 1):
    carve((NOSE[0] + 0.2, s_ * 0.9, NOSE[1] - 0.25), (0.85, 0.5, 0.4), k=0.25)          # nostril
    carve((NOSE[0] - 0.1, s_ * 1.4, NOSE[1] - 0.05), (0.6, 0.22, 0.5), k=0.2)            # its slit curling up the side
carve((NOSE[0] + 0.2, 0, NOSE[1] - 0.95), (0.8, 0.15, 0.65), k=0.2)                          # the groove

# eyes: sockets; the eyeballs are their own objects (positions to gsd_parts.json)
EYE = P(94, 101)
eyes = []
for s_ in (-1, 1):
    ix = int(round((EYE[0] - LO[0]) / RES)); iz = int(round((EYE[1] - LO[2]) / RES))
    colm = F[ix, :, iz]; ys_ = LO[1] + RES * np.nonzero(colm < 0)[0]
    ysurf = ys_.max() if s_ > 0 else ys_.min()
    c = np.array([EYE[0] - 0.3, ysurf - s_ * 0.9, EYE[1]])
    sl, p_ = box_of(c - 2.0, c + 2.0)
    F[sl] = np.maximum(F[sl], -(np.linalg.norm(p_ - c, axis=-1) - 1.15))
    eyes.append({'centre_cm': c.round(3).tolist(), 'radius_cm': 1.05})

# ears: erect, cupped (open to the front), tilted out ~17 degrees (front view) and a little forward (profile): at each
# height the ear is a thin U-shaped wall, its width from the front close-up, its depth from the profile, tapering to
# the tip
EB = P(150, 74); ET = P(132, 8)                                   # base centre and tip in the profile (X, Z)
for s_ in (-1, 1):
    base = np.array([EB[0], s_ * 6.6, EB[1] - 1.5]); tip = np.array([ET[0], s_ * 8.9, ET[1]])
    up = tip - base; L = np.linalg.norm(up); up /= L
    fw = np.array([1.0, 0, 0]); fw -= up * (fw @ up); fw /= np.linalg.norm(fw)
    sd = np.cross(up, fw)
    sl, p_ = box_of(np.minimum(base, tip) - 6, np.maximum(base, tip) + 6)
    q = p_ - base; h = q @ up; a = q @ sd; b = q @ fw
    t = np.clip(h / L, 0, 1)
    w = 4.4 * (1 - t) ** 0.85 + 0.25                               # half width (front close-up: 9 cm at the base)
    d = 3.2 * (1 - t) ** 0.9 + 0.2                                 # cup depth (profile)
    e = np.sqrt((a / w) ** 2 + (np.minimum(b, 0) / d) ** 2)
    wall = np.abs(e - 1) * np.minimum(w, d) - (0.22 + 0.15 * (1 - t))
    front = b - 0.6 * d                                              # (open to the front: cut beyond the rim)
    dE = np.maximum(wall, front)
    dE = np.maximum(dE, np.maximum(-h - 1.0, h - L))                 # (between the base and the tip)
    F[sl] = smin(F[sl], dE.astype(np.float32), 0.6)
json.dump({'eyes': eyes}, open(os.path.join(HERE, 'gsd_parts.json'), 'w'), indent=1)

v, f, n, _ = measure.marching_cubes(F, 0.0, spacing=(RES, RES, RES))
v += LO
print('body: %d vertices, %d triangles; size %s cm' % (len(v), len(f), (v.max(0) - v.min(0)).round(1)))
with open(os.path.join(HERE, 'gsd_body.ply'), 'wb') as fp:
    fp.write(('ply\nformat binary_little_endian 1.0\nelement vertex %d\nproperty float x\nproperty float y\nproperty float z\n'
              'element face %d\nproperty list uchar int vertex_indices\nend_header\n' % (len(v), len(f))).encode())
    fp.write(v.astype('<f4').tobytes())
    ff = np.empty(len(f), dtype=[('n', 'u1'), ('i', '<i4', (3,))]); ff['n'] = 3; ff['i'] = f[:, ::-1]
    fp.write(ff.tobytes())
print('wrote gsd_body.ply')
