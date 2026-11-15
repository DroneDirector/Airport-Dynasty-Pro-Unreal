# The tattoo places on every MetaHuman (owner, 2026-11-15: tattoos on the left and right arm, the left and right leg,
# separately or together; long ones down to the hand; the fingers but never the nails; the neck's left and right side,
# its back and the throat). Every MetaHuman body shares one skin layout and every head another (map_skin.py), so one
# map per place fits all people. For each place this writes a "where on the skin" picture in that layout:
#   G, B = cos / sin of the position round the limb (or across a patch) as an angle: the design's middle (u 0.5) lies
#          at angle 0 - the limb's outer side, a patch's middle; smooth across the wrap-around seam,
#   R    = the position along the place (0 = its top: shoulder, hip, knuckle, neck base),
#   A    = 1 where the place is, 0 elsewhere.
# The skin material reads it (Tools/set_tattoos.py) and lays a design picture there: any design on any place, chosen
# in play - nothing baked per design. 16-bit PNGs, 1024 x 1024: Placed/Spots/T_TatSpot_<place>.png
#   python Art/Tattoos/make_spots.py
import math, os, json
import numpy as np, cv2

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'Placed', 'Spots'); os.makedirs(OUT, exist_ok=True)
RES = 1024


def load(part):
    d = np.load(os.path.join(HERE, 'skin_map_%s.npz' % part))
    P = d['P'][::2, ::2]; N = d['N'][::2, ::2]
    B = {str(n): p[:3] for n, p in zip(d['bone_names'], d['bone_pts'])}
    return P, N, B


def chain(P, pts, side, reach):
    """Nearest point on a joint chain for every texel: arc length s from the chain's start, angle round it (0 toward
    `side`), distance; only texels whose nearest point lies within the chain (not beyond its ends)."""
    best = np.full(P.shape[:2], np.inf); s_at = np.zeros(P.shape[:2]); ang = np.zeros(P.shape[:2]); beyond = np.zeros(P.shape[:2])
    acc = 0.0; n = len(pts) - 1
    for i in range(n):
        a, b = pts[i], pts[i + 1]; L = np.linalg.norm(b - a); d = (b - a) / L
        sv = np.array(side, float) - d * (np.dot(side, d)); sv /= np.linalg.norm(sv); tv = np.cross(d, sv)
        w = P - a; al = w @ d; alc = np.clip(al, 0, L)
        ext = np.where(al < 0, -al if i == 0 else 0, np.where(al > L, (al - L) if i == n - 1 else 0, 0))
        r = w - alc[..., None] * d; dist = np.linalg.norm(r, axis=-1)
        better = np.isfinite(dist) & (dist < best)
        best = np.where(better, dist, best); s_at = np.where(better, acc + alc, s_at)
        ang = np.where(better, np.arctan2(r @ tv, r @ sv), ang); beyond = np.where(better, ext, beyond)
        acc += L
    ok = np.isfinite(best) & (best < reach) & (beyond < 0.5)
    return s_at, ang, ok, acc


def save(name, cs, sn, v, mask):
    img = np.zeros((RES, RES, 4), np.float64)
    img[..., 2] = np.clip(v, 0, 1); img[..., 1] = 0.5 + 0.5 * cs; img[..., 0] = 0.5 + 0.5 * sn   # (BGR order for cv2)
    img[..., 3] = mask.astype(float)
    img[~mask, :3] = 0
    # padding: the 4 texels round the place copy its nearest values, so the texture filter at a seam of the skin's layout
    # (the back of the neck: a thin line down the middle of a design) never mixes in empty texels
    from scipy.ndimage import distance_transform_edt
    dist, (iy, ix) = distance_transform_edt(~mask, return_indices=True)
    pad = (~mask) & (dist <= 4)
    img[pad] = img[iy[pad], ix[pad]]
    cv2.imwrite(os.path.join(OUT, 'T_TatSpot_%s.png' % name), np.round(img * 65535).astype(np.uint16))
    return int(mask.sum())


info = {}
P, N, B = load('body')
for sd, sx in (('l', 1.0), ('r', -1.0)):
    # --- arm: from the shoulder down the arm and over the back of the hand to the knuckles (long designs reach the
    #     hand; the fingers are their own place)
    pts = [B['upperarm_' + sd], B['lowerarm_' + sd], B['hand_' + sd], B['middle_01_' + sd]]
    s, a, ok, L = chain(P, pts, (sx, 0, 0), 10.5)
    top = float(np.percentile(s[ok & (s < L * 0.5)], 1.0)) if ok.any() else 0.0
    # (the arm's skin starts below the shoulder joint: the place starts at the highest skin of the arm itself, the
    #  shoulder's curve over the top of the arm is in)
    ok &= s >= top
    v = (s - top) / (L - top)
    seg = [np.linalg.norm(pts[i + 1] - pts[i]) for i in range(3)]
    info['arm_' + sd] = save('arm_' + sd, np.cos(a), np.sin(a), v, ok)
    info['arm_' + sd + '_marks'] = {'elbow': round((seg[0] - top) / (L - top), 3), 'wrist': round((seg[0] + seg[1] - top) / (L - top), 3), 'length_cm': round(L - top, 1)}
    # --- leg: from the hip down the thigh, the calf, over the ankle to the top of the foot
    pts = [B['thigh_' + sd], B['calf_' + sd], B['foot_' + sd], B['ball_' + sd]]
    s, a, ok, L = chain(P, pts, (sx, 0, 0), 13.0)
    top = float(np.percentile(s[ok & (s < L * 0.5)], 1.0)) if ok.any() else 0.0
    ok &= s >= top
    info['leg_' + sd] = save('leg_' + sd, np.cos(a), np.sin(a), (s - top) / (L - top), ok)
    seg = [np.linalg.norm(pts[i + 1] - pts[i]) for i in range(3)]
    info['leg_' + sd + '_marks'] = {'knee': round((seg[0] - top) / (L - top), 3), 'ankle': round((seg[0] + seg[1] - top) / (L - top), 3), 'length_cm': round(L - top, 1)}
    # --- fingers: index, middle, ring and little finger, the two inner joints only (knuckle to the last joint) - the
    #     last joint, with the nail, is never in the place. Each finger is a quarter of the design across; the back of
    #     the finger (away from the palm) in the middle of its quarter.
    m = np.zeros(P.shape[:2], bool); cs = np.zeros(P.shape[:2]); sn = np.zeros(P.shape[:2]); vv = np.zeros(P.shape[:2])
    for i, f in enumerate(('index', 'middle', 'ring', 'pinky')):
        pts = [B['%s_01_%s' % (f, sd)], B['%s_02_%s' % (f, sd)], B['%s_03_%s' % (f, sd)]]
        s, a, ok, L = chain(P, pts, (sx, 0, 0), 1.6)
        ok &= ~m & (np.abs(a) < math.pi * 0.55)                           # (the back and the sides of the finger)
        uu = (i + 0.5 + a / (math.pi * 1.1)) / 4.0                       # (finger i's quarter of the design)
        cs = np.where(ok, np.cos(2 * math.pi * (uu - 0.5)), cs); sn = np.where(ok, np.sin(2 * math.pi * (uu - 0.5)), sn)
        vv = np.where(ok, s / L, vv); m |= ok
    info['fingers_' + sd] = save('fingers_' + sd, cs, sn, vv, m)

P, N, B = load('face')
# --- neck: four places round the neck (head mesh), each a patch 8 cm across and from the collar up to under the jaw:
#     the left side, the right side, the back and the throat. Front is -Y on the MetaHuman.
a0 = B['neck_01'] + np.array([0, 0, -9.0]); a1 = B['head']
s, a, ok, L = chain(P, [a0, a1], (0, -1, 0), 9.5)                      # (angle 0 = the throat, + toward the dog's... left)
d = (a1 - a0) / np.linalg.norm(a1 - a0)
rr = P - a0; rr = rr - (rr @ d)[..., None] * d; rad = np.linalg.norm(rr, axis=-1)
# each neck place a square 9 x 9 cm on the neck you see above a collar (2026-11-15: from the head mesh's lowest edge
# the tee's collar hid the lower part), its bottom and top per place (MetaHuman cm: neck_01 at 155.5, head at 165.5)
NECK = {'neck_throat': (0.0, 152.5), 'neck_l': (math.pi / 2, 155.0), 'neck_back': (math.pi, 155.5), 'neck_r': (-math.pi / 2, 155.0)}   # (the sides 2.5 cm higher: the tee's collar still hid the star)
SIZE = 9.0
for name, (centre, z0) in NECK.items():
    da = np.angle(np.exp(1j * (a - centre)))                            # (angle from the place's centre, wrapped)
    arc = da * np.where(np.isfinite(rad), rad, 0)
    uu = arc / SIZE + 0.5; vv = (z0 + SIZE - P[..., 2]) / SIZE          # (v 0 at the top, like every place)
    m = ok & (np.abs(arc) < SIZE / 2) & (vv >= 0) & (vv <= 1) & (np.cos(da) > 0.5) & (rad > 3.0)
    info[name] = save(name, np.cos(2 * math.pi * (uu - 0.5)), np.sin(2 * math.pi * (uu - 0.5)), vv, m)
info['neck_size_cm'] = SIZE
json.dump(info, open(os.path.join(OUT, 'spots.json'), 'w'), indent=1)
print('spots (texels):', info)
