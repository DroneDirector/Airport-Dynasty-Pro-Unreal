# The terminal's escalators (owner, 2026-11-15: "two escalators side by side, one going up and one going down, just
# like in a real airport ... the stairs can keep on rotating endlessly ... very fine detail, just like a real
# escalator"), built from the reference photos in Art/Escalator/Reference (Commons, CC BY / CC0 / PD):
#   SM_EscStep    one step: a 1000 mm aluminium tread with its cleats (ribs along the travel, 8.5 mm apart), the
#                 cleated riser hanging below its lower edge, its side plates and axle, yellow lines on its sides and
#                 its upper edge. The game moves copies of it round the loop (Escalators.h).
#   SM_EscBody    one escalator's still parts: the stainless skirts and decking, the outer cladding and the underside,
#                 the floor plates and yellow comb plates (teeth into the cleats) at both ends, the glass balustrades,
#                 the newels; the handrails (black rubber, their own material slot, UVs along their length so the
#                 game can run them).
#   SM_EscRamp    an invisible sloped walking surface along the steps (what you walk on; the game carries you).
# The steps' path (the treads' front edge: in from under the bottom floor plate, 3 flat steps, a 1.0 m curve into 30
# degrees, the slope, a 2.6 m curve out, 3 flat steps, under the top floor plate) goes to Data/escalator.json.
# Units: metres; +Y = up the escalator (uphill), X across, Z up; origin = the bottom comb's teeth, middle of the step.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Escalator/make_escalator.py
import bpy, bmesh, json, math, os
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
RISE = 6.0
ANG = math.radians(30.0)
RL, RU = 1.0, 2.6                  # the lower and upper curves' radii
PITCH = 0.40                       # step to step, along the path
FLAT = 3 * PITCH                   # three flat steps at each end
W = 1.0                            # tread width
SKIRT = 0.506                      # skirts' inner face (4-6 mm gap to the step)
COMB = 0.25                        # comb plate length (to its teeth)
PLATE = 1.30                       # floor plate length behind the comb

# ---- the path: tread-top height h along the horizontal u ----
segs = []                          # (u0, u1, kind, data)
u = -1.4
segs.append(('pit', -1.4, -0.35))  # under the bottom floor plate (dipping)
segs.append(('flat0', -0.35, FLAT))
u0 = FLAT
segs.append(('arcL', u0, u0 + RL * math.sin(ANG)))
hL = RL * (1 - math.cos(ANG))
hU = RU * (1 - math.cos(ANG))
run = (RISE - hL - hU) / math.tan(ANG)
u1 = u0 + RL * math.sin(ANG)
segs.append(('slope', u1, u1 + run))
u2 = u1 + run
segs.append(('arcU', u2, u2 + RU * math.sin(ANG)))
u3 = u2 + RU * math.sin(ANG)
U_TOPCOMB = u3 + FLAT
segs.append(('flat1', u3, U_TOPCOMB + 0.35))
segs.append(('pitT', U_TOPCOMB + 0.35, U_TOPCOMB + 1.4))


def h(u):
    if u < -0.35: return -0.09 * min(1.0, (-0.35 - u) / 0.3)
    if u <= u0: return 0.0
    if u <= u1:
        a = math.asin(min(1.0, (u - u0) / RL)); return RL * (1 - math.cos(a))
    if u <= u2: return hL + (u - u1) * math.tan(ANG)
    if u <= u3:
        # the upper curve: centre below, the slope turning flat
        a = ANG - math.asin(min(1.0, max(0.0, (u3 - u) / RU)))
        return RISE - RU * (1 - math.cos(ANG - a)) if False else RISE - RU * (1 - math.cos(math.asin(min(1.0, (u3 - u) / RU))))
    if u <= U_TOPCOMB + 0.35: return RISE
    return RISE - 0.09 * min(1.0, (u - U_TOPCOMB - 0.35) / 0.3)


def slope(u, du=0.01): return math.atan2(h(u + du) - h(u - du), 2 * du)


# the path by arc length, sampled finely, for the game
samples = []
S = 0.0; prev = None
uu = -1.4
while uu <= U_TOPCOMB + 1.4 + 1e-6:
    p = (uu, h(uu))
    if prev: S += math.hypot(p[0] - prev[0], p[1] - prev[1])
    samples.append([round(S, 5), round(p[0], 5), round(p[1], 5)]); prev = p; uu += 0.02
os.makedirs(os.path.join(ROOT, 'Data'), exist_ok=True)
json.dump({'about': 'Art/Escalator/make_escalator.py: the steps\' path - arc length s, u along the escalator, h height (m); '
                    'the treads\' upper edge rides on it; visible between the combs', 'rise': RISE, 'pitch': PITCH,
           'comb_bottom_u': 0.0, 'comb_top_u': U_TOPCOMB, 'length_u': U_TOPCOMB, 'path': samples},
          open(os.path.join(ROOT, 'Data', 'escalator.json'), 'w'), indent=0)
print('ESC path: %.2f m along, %.2f m long, run %.2f, top comb at u %.2f' % (U_TOPCOMB, S, run, U_TOPCOMB))

bpy.ops.wm.read_factory_settings(use_empty=True)


def mat(name, col, rough=0.4, metal=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    m.diffuse_color = (*col, 1)
    return m


M = {k: mat(*v) for k, v in {
    'StepMetal': ('M_EscStepMetal', (0.42, 0.43, 0.45), 0.45, 1.0),
    'StepYellow': ('M_EscYellow', (0.95, 0.72, 0.05), 0.5, 0.0),
    'Stainless': ('M_EscStainless', (0.78, 0.78, 0.8), 0.22, 1.0),
    'FloorPlate': ('M_EscFloorPlate', (0.7, 0.7, 0.72), 0.35, 1.0),
    'Glass': ('M_EscGlass', (0.85, 0.92, 0.95), 0.03, 0.0),
    'Handrail': ('M_EscHandrail', (0.02, 0.02, 0.02), 0.55, 0.0),
    'Truss': ('M_EscTruss', (0.25, 0.26, 0.27), 0.7, 0.3),
    'Light': ('M_EscLight', (0.9, 0.95, 1.0), 0.3, 0.0)}.items()}


class Builder:
    def __init__(self, name):
        self.bm = bmesh.new(); self.name = name; self.mats = []
        self.uv = self.bm.loops.layers.uv.new('UVMap')
    def mi(self, key):
        if key not in self.mats: self.mats.append(key)
        return self.mats.index(key)
    def quad(self, vs, key, uvs=None):
        vv = [self.bm.verts.new(v) for v in vs]
        f = self.bm.faces.new(vv); f.material_index = self.mi(key)
        for i, l in enumerate(f.loops):
            l[self.uv].uv = uvs[i] if uvs else (l.vert.co.x + l.vert.co.z, l.vert.co.y + l.vert.co.z)
        return f
    def box(self, x0, x1, y0, y1, z0, z1, key):
        c = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        for f in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            self.quad([c[i] for i in f], key)
    def strip(self, pts_a, pts_b, key, uvlen=False):
        """Quads between two equally long point lists."""
        L = 0.0
        for i in range(len(pts_a) - 1):
            a0, a1, b1, b0 = pts_a[i], pts_a[i + 1], pts_b[i + 1], pts_b[i]
            if uvlen:
                d = (Vector(a1) - Vector(a0)).length
                self.quad([a0, a1, b1, b0], key, [(L, 0), (L + d, 0), (L + d, 1), (L, 1)]); L += d
            else: self.quad([a0, a1, b1, b0], key)
    def finish(self, smooth=()):
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-5)
        for f in self.bm.faces: f.smooth = self.mats[f.material_index] in smooth
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        me = bpy.data.meshes.new(self.name); self.bm.to_mesh(me); self.bm.free()
        for k in self.mats: me.materials.append(M[k])
        ob = bpy.data.objects.new(self.name, me); bpy.context.scene.collection.objects.link(ob)
        return ob


# ---------- the step ----------
st = Builder('SM_EscStep')
T = 0.40; RIB = 0.0085; RIBW = 0.0028; GROOVE = 0.008; TH = 0.012
# the tread: its top a field of ribs along Y, from its lower edge (y = -T) to its upper edge (y = 0); yellow strips
# on both sides (35 mm) and the upper edge (45 mm)
YEL_S, YEL_U = 0.035, 0.045
x = -W / 2
while x < W / 2 - 1e-6:
    x1 = min(W / 2, x + RIB)
    side = x < -W / 2 + YEL_S or x1 > W / 2 - YEL_S
    # the rib (raised) and the groove beside it
    rx1 = min(x1, x + RIBW)
    for (ya, yb, key) in (((-T, -YEL_U, 'StepYellow' if side else 'StepMetal'), (-YEL_U, 0.0, 'StepYellow'))):
        st.box(x, rx1, ya, yb, -GROOVE, 0.0, key)
        st.box(rx1, x1, ya, yb, -GROOVE - TH, -GROOVE, key)
    x = x1
# the riser: a cleated curve hanging below the lower edge, 0.22 deep, bowed in (concentric with the step below)
N = 8
for i in range(N):
    a0, a1 = i / N, (i + 1) / N
    def rp(t): return (-T + 0.03 * math.sin(math.pi * t) * 0.6 + 0.018 * t, -0.22 * t - GROOVE)
    (y0_, z0_), (y1_, z1_) = rp(a0), rp(a1)
    x = -W / 2
    k = 0
    while x < W / 2 - 1e-6:
        x1 = min(W / 2, x + RIB); rx1 = min(x1, x + RIBW)
        st.quad([(x, y0_, z0_), (rx1, y0_, z0_), (rx1, y1_, z1_), (x, y1_, z1_)], 'StepMetal')
        st.quad([(rx1, y0_ + 0.006, z0_), (x1, y0_ + 0.006, z0_), (x1, y1_ + 0.006, z1_), (rx1, y1_ + 0.006, z1_)], 'StepMetal')
        x = x1
# side plates (the step's frame) and its axle
for sx in (-1, 1):
    xx = sx * (W / 2 - 0.004)
    tri = [(xx, -T + 0.02, -0.02), (xx, -0.01, -0.02), (xx, -T + 0.05, -0.26)]
    f = st.quad([tri[0], tri[1], tri[2], tri[2]], 'Truss') if False else None
    st.box(xx - 0.004 * sx, xx, -T + 0.02, -0.01, -0.06, -0.02, 'Truss')
    st.box(xx - 0.004 * sx, xx, -T + 0.03, -T + 0.09, -0.26, -0.06, 'Truss')
st.box(-W / 2 + 0.01, W / 2 - 0.01, -T + 0.04, -T + 0.08, -0.25, -0.21, 'Truss')
step = st.finish()

# ---------- the body ----------
bd = Builder('SM_EscBody')
U0, U1 = -PLATE - COMB, U_TOPCOMB + COMB + PLATE         # the whole length, floor plate to floor plate
us = []
v = U0
while v <= U1 + 1e-6: us.append(round(v, 4)); v += 0.025 if (u0 - 0.3 < v < u1 + 0.3 or u2 - 0.3 < v < u3 + 0.3) else 0.25
if us[-1] < U1: us.append(U1)
def H(u_): return max(0.0, min(RISE, h(min(max(u_, 0.0), U_TOPCOMB))))   # the step line, flat over the plates
DECK, GL0, GLX, CLAD, TRUSS = 0.10, 0.12, 0.62, 0.80, 1.05
# the ends sit flush with the floor they meet (owner 2026-11-16: no end walls, nothing standing up above the floor):
# the decking slopes down to the floor over its last 20 cm
def END(u_): return max(0.0, min(1.0, (u_ - U0) / 0.2, (U1 - u_) / 0.2))
for sx in (-1, 1):
    # the skirt: stainless, from below the steps to the decking
    bd.strip([(sx * SKIRT, u_, H(u_) - 0.32) for u_ in us], [(sx * SKIRT, u_, H(u_) + DECK * END(u_)) for u_ in us], 'Stainless')
    # the inner decking up to the glass, the outer decking over the cladding
    bd.strip([(sx * SKIRT, u_, H(u_) + DECK * END(u_)) for u_ in us], [(sx * (GLX - 0.01), u_, H(u_) + GL0 * END(u_)) for u_ in us], 'Stainless')
    bd.strip([(sx * (GLX + 0.02), u_, H(u_) + GL0 * END(u_)) for u_ in us], [(sx * CLAD, u_, H(u_) + GL0 * END(u_)) for u_ in us], 'Stainless')
    # the outer cladding down to the truss's underside (on the slope you see it from the hall)
    bd.strip([(sx * CLAD, u_, H(u_) + GL0 * END(u_)) for u_ in us], [(sx * CLAD, u_, H(u_) - TRUSS) for u_ in us], 'Stainless')
    # an LED line under the decking's lip, along the skirt (as on the references)
    bd.strip([(sx * (SKIRT - 0.001), u_, H(u_) + DECK * END(u_) - 0.012) for u_ in us if END(u_) > 0.5], [(sx * (SKIRT - 0.001), u_, H(u_) + DECK * END(u_) - 0.004) for u_ in us if END(u_) > 0.5], 'Light')
# the underside
bd.strip([(-CLAD, u_, H(u_) - TRUSS) for u_ in us], [(CLAD, u_, H(u_) - TRUSS) for u_ in us], 'Truss')
# the end walls of the pits
# (only below the floor: a 12 cm wall there stopped you stepping on and off, owner 2026-11-16 - no end walls at all)
for uu_ in (U0, U1):
    hh = H(uu_)
    bd.quad([(-CLAD, uu_, hh - TRUSS), (CLAD, uu_, hh - TRUSS), (CLAD, uu_, hh - 0.01), (-CLAD, uu_, hh - 0.01)], 'Stainless')

# floor plates (stainless, ribbed by their texture) and comb plates with teeth, at both ends
for (ua, ub, uc, sgn, hh) in ((U0, -COMB, 0.0, 1, 0.0), (U_TOPCOMB + COMB, U1, U_TOPCOMB, -1, RISE)):
    bd.box(-SKIRT, SKIRT, ua, ub, hh - 0.01, hh + 0.004, 'FloorPlate')
    # the comb plate: yellow, its teeth reaching over the step's grooves
    cb0, cb1 = (-COMB, -0.06) if sgn > 0 else (U_TOPCOMB + 0.06, U_TOPCOMB + COMB)
    bd.box(-SKIRT, SKIRT, cb0, cb1, hh - 0.01, hh + 0.006, 'StepYellow')
    x = -W / 2 + 0.002
    while x < W / 2 - RIB:
        gx0, gx1 = x + RIBW + 0.0008, x + RIB - 0.0008                     # a tooth sits in each groove
        ty0, ty1 = (-0.06, 0.0) if sgn > 0 else (U_TOPCOMB, U_TOPCOMB + 0.06)
        tip = ty1 if sgn > 0 else ty0; root = ty0 if sgn > 0 else ty1
        bd.quad([(gx0, root, hh + 0.006), (gx1, root, hh + 0.006), (gx1, tip, hh - 0.006), (gx0, tip, hh - 0.006)], 'StepYellow')
        x += RIB
# the glass balustrades: from the decking up to under the handrail, with rounded newels at both ends
HR = 0.93                                  # handrail top above the step line
NEW_R = 0.32                               # the newel's turn radius
UH0, UH1 = -COMB - 0.75, U_TOPCOMB + COMB + 0.75     # where the handrail's straight run ends at each newel
def HH(u_): return H(min(max(u_, -COMB), U_TOPCOMB + COMB)) + HR
hus = [x_ for x_ in us if UH0 <= x_ <= UH1]
hus = [UH0] + hus + [UH1]
for sx in (-1, 1):
    gx = sx * GLX
    bd.strip([(gx, u_, H(u_) + GL0) for u_ in hus], [(gx, u_, HH(u_) - 0.05) for u_ in hus], 'Glass')
    bd.strip([(gx + sx * 0.012, u_, H(u_) + GL0) for u_ in hus], [(gx + sx * 0.012, u_, HH(u_) - 0.05) for u_ in hus], 'Glass')
    for (ue, d) in ((UH0, -1), (UH1, 1)):            # the newel: the glass rounded out under the handrail's turn
        c = (ue, HH(ue) - NEW_R)
        pts = [(gx, ue + d * NEW_R * math.sin(a), c[1] + NEW_R * math.cos(a) - 0.05 * math.cos(a)) for a in [i * math.pi / 48 for i in range(49)]]
        base = (gx, ue, H(ue) + GL0)
        for i in range(48): bd.quad([base, pts[i], pts[i + 1], base], 'Glass')
    # the handrail: a C-profile in black rubber along the top and round both newels; UVs along its length
    path = []
    for i in range(49): path.append((UH0 - NEW_R * math.sin(i * math.pi / 48), HH(UH0) - NEW_R + NEW_R * math.cos(i * math.pi / 48)))
    path = path[::-1] + [(u_, HH(u_)) for u_ in hus[1:-1]] + [(UH1 + NEW_R * math.sin(i * math.pi / 48), HH(UH1) - NEW_R + NEW_R * math.cos(i * math.pi / 48)) for i in range(49)]
    # the handrail's section: a smooth rounded C (80 mm wide, 32 mm high), its lips curling in underneath
    prof = []
    for i in range(25):
        a_ = math.pi * (1.12 - 1.24 * i / 24)                     # from the left lip round the top to the right lip
        prof.append((0.042 * math.cos(a_), 0.004 + 0.028 * math.sin(a_) if math.sin(a_) > 0 else 0.004 + 0.012 * math.sin(a_)))
    rings = []
    for i, (pu, ph) in enumerate(path):
        j0, j1 = max(0, i - 1), min(len(path) - 1, i + 1)
        tu, th = path[j1][0] - path[j0][0], path[j1][1] - path[j0][1]; n = math.hypot(tu, th) or 1
        nu, nh = -th / n, tu / n                                           # the profile's "up" across the path
        rings.append([(gx + sx * 0.0 + px, pu + nu * py, ph + nh * py) for (px, py) in prof])
    L = 0.0
    for i in range(len(rings) - 1):
        d = math.hypot(path[i + 1][0] - path[i][0], path[i + 1][1] - path[i][1])
        for k in range(len(prof) - 1):
            bd.quad([rings[i][k], rings[i + 1][k], rings[i + 1][k + 1], rings[i][k + 1]], 'Handrail',
                    [(L, k / (len(prof) - 1)), (L + d, k / (len(prof) - 1)), (L + d, (k + 1) / (len(prof) - 1)), (L, (k + 1) / (len(prof) - 1))])
        L += d
body = bd.finish(smooth=('Handrail',))

# ---------- the walking surface ----------
rp = Builder('SM_EscRamp')
rp.strip([(-0.5, u_, H(u_) + 0.005) for u_ in us], [(0.5, u_, H(u_) + 0.005) for u_ in us], 'Truss')
ramp = rp.finish()

for o in (step, body, ramp):
    print('ESC', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
out = os.path.join(HERE, 'Escalator.blend')
bpy.ops.wm.save_as_mainfile(filepath=out)
for o in (step, body, ramp):
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
print('ESC saved', out)
