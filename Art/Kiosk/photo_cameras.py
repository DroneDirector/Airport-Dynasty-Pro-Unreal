# The cameras of the owner's four kiosk photos (Art/Kiosk/References), worked out from the base plate (64 x 68 cm, its
# top at 1.2 cm): each photo's base plate corners (picked by hand off grid crops) and a pinhole camera fitted to them
# (focal length, turn, place) by least squares. Used to measure the kiosk from the photos (two views: a point seen in
# both is triangulated) and to put Blender cameras where the photos were taken (the model's renders laid over them).
#   python Art/Kiosk/photo_cameras.py           (prints each camera, writes photo_cameras.json)
import json, math, os
import numpy as np
from scipy.optimize import least_squares

HERE = os.path.dirname(os.path.abspath(__file__))
PZ = 0.012   # the plate's top
# kiosk space: x to its right, y to its back (front toward -y), z up; metres. Plate corners (their rounded tips' crossing)
C = {'FR': (0.32, -0.34), 'FL': (-0.32, -0.34), 'BR': (0.32, 0.34), 'BL': (-0.32, 0.34)}
PHOTOS = {
    'front': ('References/kiosk_front.png', (1086, 1448), {'FR': (400, 1307), 'FL': (775, 1267), 'BR': (321, 1250), 'BL': (621, 1188)}),
    'side': ('References/kiosk_side.jpg', (896, 1195), {'BR': (347, 1079), 'FR': (617, 1085), 'FL': (620, 1008), 'BL': (373, 1015)}),
}

def rot(rx, ry, rz):
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]]); Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]); Rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return Rz @ Ry @ Rx

def project(p, cam, size):
    """cam: f, rx, ry, rz, tx, ty, tz (world to camera: X_c = R (X - t)); camera looks along +z, image y down"""
    f, rx, ry, rz, tx, ty, tz = cam
    R = rot(rx, ry, rz); Xc = R @ (np.asarray(p) - np.array([tx, ty, tz]))
    return np.array([size[0] / 2 + f * Xc[0] / Xc[2], size[1] / 2 + f * Xc[1] / Xc[2]])

def fit(size, pts, guess):
    keys = list(pts)
    def res(c): return np.concatenate([project((*C[k], PZ), c, size) - np.array(pts[k]) for k in keys])
    best = None
    for f0 in (900, 1300, 1800, 2500, 3500):
        g = list(guess); g[0] = f0
        lo = [500, -math.pi, -math.pi, -math.pi, -10, -10, 0.3]; hi = [6000, math.pi, math.pi, math.pi, 10, 10, 2.5]   # (above the floor: four points on a flat plate also fit a mirrored camera under it)
        g = [min(max(v, l + 1e-6), h - 1e-6) for v, l, h in zip(g, lo, hi)]
        r = least_squares(res, g, bounds=(lo, hi), max_nfev=20000)
        if best is None or r.cost < best.cost: best = r
    return best

def look_guess(eye, size):
    """a starting camera at `eye` looking at the kiosk's middle (0, 0, 0.8)"""
    d = np.array([0, 0, 0.8]) - np.array(eye); d /= np.linalg.norm(d)
    # camera z = d; x = right; y = down
    x = np.cross(d, [0, 0, 1.0]); x /= np.linalg.norm(x); y = np.cross(d, x)
    R = np.vstack([x, y, d])   # rows: camera axes in world
    ry = math.asin(-R[2, 0]); rx = math.atan2(R[2, 1], R[2, 2]); rz = math.atan2(R[1, 0], R[0, 0])
    return [1500, rx, ry, rz, *eye]

def triangulate(cams, obs):
    """a 3D point from its pixel in two or more photos (least squares over the reprojection)"""
    def res(p): return np.concatenate([project(p, cams[k]['cam'], cams[k]['size']) - np.array(uv) for k, uv in obs.items()])
    return least_squares(res, [0, 0, 1.0]).x

if __name__ == '__main__':
    out = {}
    for name, (f, size, pts) in PHOTOS.items():
        eye = {'front': (0.6, -3.0, 1.2), 'side': (3.0, 0.4, 1.2)}[name]
        r = fit(size, pts, look_guess(eye, size))
        err = np.abs(r.fun).max()
        out[name] = {'file': f, 'size': size, 'cam': list(map(float, r.x)), 'max_err_px': float(err)}
        print('%s: f %.0f px, eye at (%.2f, %.2f, %.2f) m, worst corner off by %.1f px' % (name, r.x[0], r.x[4], r.x[5], r.x[6], err))
    json.dump(out, open(os.path.join(HERE, 'photo_cameras.json'), 'w'), indent=1)
