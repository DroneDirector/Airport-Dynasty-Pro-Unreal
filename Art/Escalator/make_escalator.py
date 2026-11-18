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
# (ESC_SHORT=1: the arrivals escalators - owner 2026-11-16: the ends a third as long; 2026-11-18: built short here, not
#  squeezed afterwards, so the handrail's turn and the glass under it stay round and the glass sits inside the handrail as
#  on the long escalators. Writes SM_EscBodyShort / SM_EscRampShort only; the steps and their path are the long ones')
SHORT = os.environ.get('ESC_SHORT') == '1'
if SHORT: PLATE = 1.30 / 3

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
if not SHORT: json.dump({'about': 'Art/Escalator/make_escalator.py: the steps\' path - arc length s, u along the escalator, h height (m); '
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
    'StepGroove': ('M_EscStepGroove', (0.07, 0.075, 0.08), 0.7, 0.3),   # (the grooves between the cleats: dark, as in his photo)
    'StepYellow': ('M_EscYellow', (0.95, 0.72, 0.05), 0.5, 0.0),
    'Stainless': ('M_EscStainless', (0.78, 0.78, 0.8), 0.22, 1.0),
    'FloorPlate': ('M_EscFloorPlate', (0.7, 0.7, 0.72), 0.35, 1.0),
    'Glass': ('M_EscGlass', (0.85, 0.92, 0.95), 0.03, 0.0),
    'Handrail': ('M_EscHandrail', (0.02, 0.02, 0.02), 0.55, 0.0),
    'Truss': ('M_EscTruss', (0.25, 0.26, 0.27), 0.7, 0.3),
    'Light': ('M_EscLight', (0.9, 0.95, 1.0), 0.3, 0.0),
    'Red': ('M_EscRed', (0.85, 0.05, 0.04), 0.4, 0.0), 'Green': ('M_EscGreen', (0.1, 0.85, 0.3), 0.4, 0.0),
    'Dark': ('M_EscDark', (0.03, 0.03, 0.035), 0.6, 0.0)}.items()}


class Builder:
    def __init__(self, name):
        self.bm = bmesh.new(); self.name = name; self.mats = []
        self.uv = self.bm.loops.layers.uv.new('UVMap')
    def mi(self, key):
        if key not in self.mats: self.mats.append(key)
        return self.mats.index(key)
    proj = False                     # (the body: every face mapped at real size by the way it faces - owner 2026-11-18, to-do 110)
    def quad(self, vs, key, uvs=None):
        vv = [self.bm.verts.new(v) for v in vs]
        f = self.bm.faces.new(vv); f.material_index = self.mi(key)
        if not uvs and self.proj:
            # real size and seamless: 50 cm of surface = one tile, projected along the axis the face looks along, so faces
            # side by side continue the same picture; the brushing runs along the escalator (Y) wherever it can
            f.normal_update(); n = f.normal; ax = max(range(3), key=lambda k: abs(n[k]))
            for l in f.loops:
                c = l.vert.co
                T = 2.0                      # (50 cm per tile: a fine brushing, sharp up close)
                l[self.uv].uv = (c.y * T, c.x * T) if ax == 2 else ((c.y * T, c.z * T) if ax == 0 else (c.x * T, c.z * T))
            return f
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
# (owner 2026-11-18, photo Art/Escalator/Reference/Owner-2026-11-18/3-steps-texture.jpg: bright cleats with dark grooves
#  between them, on the treads and the risers alike - the groove floors and the risers' recesses are dark metal)
T = 0.40; RIB = 0.0085; RIBW = 0.0028; GROOVE = 0.008; TH = 0.012
YEL_S, YEL_U = 0.035, 0.045
def make_step(name, Wd, Td, riser, bowed):
    """a cleated step Wd wide, Td deep: its tread (ribs along Y from y = -Td to 0, yellow sides and upper edge) and its
    cleated riser hanging 'riser' metres below the lower edge (bowed: the escalator's curved riser; else straight)"""
    st = Builder(name)
    x = -Wd / 2
    while x < Wd / 2 - 1e-6:
        x1 = min(Wd / 2, x + RIB)
        side = x < -Wd / 2 + YEL_S or x1 > Wd / 2 - YEL_S
        rx1 = min(x1, x + RIBW)
        for (ya, yb, key) in ((-Td, -YEL_U, 'StepYellow' if side else 'StepMetal'), (-YEL_U, 0.0, 'StepYellow')):
            st.box(x, rx1, ya, yb, -GROOVE, 0.0, key)
            st.box(rx1, x1, ya, yb, -GROOVE - TH, -GROOVE, key if key == 'StepYellow' else 'StepGroove')
        x = x1
    N = 8 if bowed else 1
    for i in range(N):
        a0, a1 = i / N, (i + 1) / N
        if bowed:
            def rp(t): return (-Td + 0.03 * math.sin(math.pi * t) * 0.6 + 0.018 * t, -riser * t - GROOVE)
        else:
            def rp(t): return (-Td, -riser * t)
        (y0_, z0_), (y1_, z1_) = rp(a0), rp(a1)
        x = -Wd / 2
        while x < Wd / 2 - 1e-6:
            x1 = min(Wd / 2, x + RIB); rx1 = min(x1, x + RIBW)
            st.quad([(x, y0_, z0_), (rx1, y0_, z0_), (rx1, y1_, z1_), (x, y1_, z1_)], 'StepMetal')
            st.quad([(rx1, y0_ + 0.006, z0_), (x1, y0_ + 0.006, z0_), (x1, y1_ + 0.006, z1_), (rx1, y1_ + 0.006, z1_)], 'StepGroove')
            # (the rib's sides, so the cleats stand out of the riser)
            st.quad([(rx1, y0_, z0_), (rx1, y0_ + 0.006, z0_), (rx1, y1_ + 0.006, z1_), (rx1, y1_, z1_)], 'StepMetal')
            st.quad([(x, y0_ + 0.006, z0_), (x, y0_, z0_), (x, y1_, z1_), (x, y1_ + 0.006, z1_)], 'StepMetal')
            x = x1
    if bowed:
        for sx in (-1, 1):
            xx = sx * (Wd / 2 - 0.004)
            st.box(xx - 0.004 * sx, xx, -Td + 0.02, -0.01, -0.06, -0.02, 'Truss')
            st.box(xx - 0.004 * sx, xx, -Td + 0.03, -Td + 0.09, -0.26, -0.06, 'Truss')
        st.box(-Wd / 2 + 0.01, Wd / 2 - 0.01, -Td + 0.04, -Td + 0.08, -0.25, -0.21, 'Truss')
    return st.finish()
step = make_step('SM_EscStep', W, T, 0.22, True)
# the fixed stair beside the arrivals escalators (owner 2026-11-18: its steps as the escalator's, not wood): one step
# 2.0 m wide, 38 cm deep, a straight cleated 20 cm riser; the game lays 30 of them (Escalators.cpp, BuildArrivalsStair)
stair = make_step('SM_StairStep', 2.0, 0.38, 0.20, False)

# ---------- the body ----------
bd = Builder('SM_EscBody')
bd.proj = True
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
    # (owner 2026-11-18, to-do 112: you could see through the 3 cm slot the glass stands in where the glass and the
    #  housings end - the slot closed all along, flush with the decking either side; the glass stands on it)
    bd.strip([(sx * (GLX - 0.01), u_, H(u_) + GL0 * END(u_)) for u_ in us], [(sx * (GLX + 0.02), u_, H(u_) + GL0 * END(u_)) for u_ in us], 'Stainless')
    # and the sloped end pieces closed at the very ends and underneath down to the floor: no view in under the decking
    for uu_ in (U0, U1):
        hh = H(uu_)
        bd.quad([(sx * SKIRT, uu_, hh - 0.32), (sx * CLAD, uu_, hh - 0.32), (sx * CLAD, uu_, hh + 0.001), (sx * SKIRT, uu_, hh + 0.001)], 'Stainless')
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
# (the new escalator, owner 2026-11-18, approved concept docs/Blueprints/EscalatorNew): the floor plates screwed down -
# countersunk screws along their edges; a red emergency stop and a key switch on the skirt beside each comb; green/red
# direction lights in the skirt at each end
def screw(x, y, z, r=0.007):
    n = 10
    for k in range(n):
        a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
        bd.quad([(x, y, z + 0.0012), (x + r * math.cos(a0), y + r * math.sin(a0), z + 0.0008), (x + r * math.cos(a1), y + r * math.sin(a1), z + 0.0008), (x, y, z + 0.0012)], 'Dark')
for (ua, ub, hh) in ((U0, -COMB, 0.0), (U_TOPCOMB + COMB, U1, RISE)):
    for xx in (-SKIRT + 0.03, SKIRT - 0.03):
        for t in range(4): screw(xx, ua + 0.05 + (ub - ua - 0.1) * t / 3, hh + 0.004)
    for t in range(1, 5): screw(-SKIRT + 0.03 + (2 * SKIRT - 0.06) * t / 5, (ub - 0.03) if hh == 0.0 else (ua + 0.03), hh + 0.004)
for (uc, hh, d) in ((-0.12, 0.0, 1), (U_TOPCOMB + 0.12, RISE, -1)):
    # the emergency stop: a red mushroom on a yellow plate, on the left skirt; the key switch on the right
    bd.box(-SKIRT + 0.001, -SKIRT + 0.004, uc - 0.05, uc + 0.05, hh + 0.13, hh + 0.24, 'StepYellow')
    n = 16
    for k in range(n):
        a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
        bd.quad([(-SKIRT + 0.004, uc + 0.022 * math.cos(a0), hh + 0.185 + 0.022 * math.sin(a0)), (-SKIRT + 0.026, uc + 0.018 * math.cos(a0), hh + 0.185 + 0.018 * math.sin(a0)),
                 (-SKIRT + 0.026, uc + 0.018 * math.cos(a1), hh + 0.185 + 0.018 * math.sin(a1)), (-SKIRT + 0.004, uc + 0.022 * math.cos(a1), hh + 0.185 + 0.022 * math.sin(a1))], 'Red')
    bd.box(SKIRT - 0.004, SKIRT - 0.001, uc - 0.035, uc + 0.035, hh + 0.15, hh + 0.22, 'Stainless')
    bd.box(SKIRT - 0.012, SKIRT - 0.004, uc - 0.012, uc + 0.012, hh + 0.173, hh + 0.197, 'Dark')
    # the direction light low in the skirt: green where you may step on (the bottom), red where you may not (the top)
    for sx in (-1, 1): bd.box(sx * SKIRT - sx * 0.002, sx * SKIRT - sx * 0.0005, uc + d * 0.10, uc + d * 0.16, hh + 0.04, hh + 0.07, 'Green' if hh == 0.0 else 'Red')
# the glass balustrades and the handrail (the new escalator, owner 2026-11-18, approved concept): at each end the
# black handrail runs along the top, round a smooth round newel (a full half circle), back underneath and into a black
# rubber inlet on the decking - at the top exactly as at the bottom; the glass sits inside the handrail all the way,
# its lower edge rising along the flat ends to just over the handrail's return; stainless below the return
HR = 0.95                                  # handrail top above the step line
NEW_R = 0.34                               # the newel's radius (its return at 0.27 m over the step line)
UHO = 0.08 if SHORT else 0.40              # the handrail's straight run past the comb before the newel
UH0, UH1 = -COMB - UHO, U_TOPCOMB + COMB + UHO     # where the handrail's straight run ends at each newel
INSET, RET = 0.06, 0.30                    # the glass inside the handrail; the return's run back to its inlet
def HH(u_): return H(min(max(u_, -COMB), U_TOPCOMB + COMB)) + HR
def ZR(ue): return HH(ue) - 2 * NEW_R      # the return's height
hus = [x_ for x_ in us if UH0 <= x_ <= UH1]
hus = [UH0] + hus + [UH1]
hus = sorted(set([round(x_, 5) for x_ in hus] + [round(UH0 + i * 0.05, 5) for i in range(1, 11)] + [round(UH1 - i * 0.05, 5) for i in range(1, 11)]))
ENDS = ((UH0, -1), (UH1, 1))
GTOP = lambda ue: ZR(ue) + 0.05            # the glass's lower edge at the ends: just over the return
RAMP_BOT, RAMP_TOP = (UH0, u0 + 0.1), (UH1, u3 - 0.1)
def GLB(u_):
    """the glass's lower edge: on the clamp along the slope, rising smoothly along the flat ends to just over the return"""
    low = H(u_) + GL0
    for ue, us_ in (RAMP_BOT, RAMP_TOP):
        span = abs(ue - us_)
        k = max(0.0, 1.0 - abs(ue - u_) / span) if (min(ue, us_) - 1e-6 <= u_ <= max(ue, us_) + 1e-6) else 0.0
        if k > 0: low = low + (GTOP(ue) - (H(ue) + GL0)) * (k * k * (3 - 2 * k))
    return low
for sx in (-1, 1):
    gx = sx * GLX
    for gxx in (gx, gx + sx * 0.012):                                   # the glass (two faces, 12 mm apart)
        bd.strip([(gxx, u_, GLB(u_)) for u_ in hus], [(gxx, u_, HH(u_) - INSET) for u_ in hus], 'Glass')
        for (ue, d) in ENDS:                                             # its round end inside the newel
            cz = HH(ue) - NEW_R
            outline = [(ue, GTOP(ue)), (ue, HH(ue) - INSET)]
            outline += [(ue + d * (NEW_R - INSET) * math.sin(a), cz + (NEW_R - INSET) * math.cos(a)) for a in [k * math.pi / 48 for k in range(1, 48)]]
            outline += [(ue + d * 0.02, GTOP(ue))]
            cu = sum(o[0] for o in outline) / len(outline); cz_ = sum(o[1] for o in outline) / len(outline)
            for k in range(len(outline)):
                a_, b_ = outline[k], outline[(k + 1) % len(outline)]
                bd.quad([(gxx, cu, cz_), (gxx, a_[0], a_[1]), (gxx, b_[0], b_[1]), (gxx, cu, cz_)], 'Glass')
    # the stainless base under the glass's risen edge and under the newel down to the decking, both faces
    for (ue, d), (lo_, hi_) in zip(ENDS, ((UH0, RAMP_BOT[1]), (RAMP_TOP[1], UH1))):
        base_us = [u_ for u_ in hus if lo_ - 1e-6 <= u_ <= hi_ + 1e-6]
        far = [ue + d * t for t in [k * NEW_R / 8 for k in range(9)]]
        for gxx in (gx - sx * 0.004, gx + sx * 0.016):
            bd.strip([(gxx, u_, H(u_) + GL0 * END(u_)) for u_ in base_us], [(gxx, u_, GLB(u_) + 0.002) for u_ in base_us], 'Stainless')
            bd.strip([(gxx, u_, H(min(max(u_, -COMB), U_TOPCOMB + COMB)) + GL0 * END(u_)) for u_ in far], [(gxx, u_, ZR(ue) - 0.035) for u_ in far], 'Stainless')
        # the newel's nose: a rounded stainless cap under the handrail's turn, from the decking up to the return
    # the inlets: black rubber boxes on the decking where the handrail's return goes back in, a stainless lip round the mouth
    for (ue, d) in ENDS:
        iu = ue - d * RET; hz = H(min(max(iu, -COMB), U_TOPCOMB + COMB)) + GL0
        a0, a1 = sorted((iu - d * 0.02, iu - d * 0.20))
        bd.box(gx - 0.065, gx + 0.065, a0, a1, hz, ZR(ue) + 0.06, 'Dark')
        f0 = a1 if d > 0 else a0                                         # its mouth (facing the newel)
        bd.box(gx - 0.07, gx + 0.07, f0 - 0.008, f0 + 0.008, ZR(ue) - 0.05, ZR(ue) + 0.065, 'Stainless')
        for o in (-1, 1): bd.box(gx - 0.05, gx + 0.05, f0 - 0.004 + d * 0.004, f0 + 0.004 + d * 0.004, ZR(ue) + o * 0.035 - 0.004, ZR(ue) + o * 0.035 + 0.004, 'Handrail')   # rubber lips
    # the handrail: a C-profile in black rubber - along the top, round each newel, back underneath into its inlet;
    # UVs along its length (the game runs it)
    def end_path(ue, d):
        cz = HH(ue) - NEW_R
        arc = [(ue + d * NEW_R * math.sin(a), cz + NEW_R * math.cos(a)) for a in [k * math.pi / 64 for k in range(65)]]
        back = [(ue - d * RET * t / 8, ZR(ue)) for t in range(1, 9)]
        return arc + back
    path = end_path(UH0, -1)[::-1] + [(u_, HH(u_)) for u_ in hus[1:-1]] + end_path(UH1, 1)
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
    # the skirt brushes: black bristle strips along both skirts a hand over the steps (keep feet off the skirt)
    bd.strip([(sx * (SKIRT - 0.002), u_, H(u_) + 0.035) for u_ in us if -0.2 <= u_ <= U_TOPCOMB + 0.2], [(sx * (SKIRT - 0.035), u_, H(u_) + 0.04) for u_ in us if -0.2 <= u_ <= U_TOPCOMB + 0.2], 'Dark')
    # anti-slide studs on the outer decking (on the centre deck between a pair they stand side by side), every 1.6 m
    for uu in [1.6 + k * 1.6 for k in range(int((U_TOPCOMB - 2.0) / 1.6))]:
        zz = H(uu) + GL0 * END(uu); xx = sx * (GLX + 0.02 + (CLAD - GLX - 0.02) / 2)
        tanp = math.atan2(H(uu + 0.05) - H(uu - 0.05), 0.1)
        bd.box(xx - 0.06, xx + 0.06, uu - 0.012, uu + 0.012, zz, zz + 0.025, 'Stainless')
body = bd.finish(smooth=('Handrail',))

# ---------- the walking surface ----------
rp = Builder('SM_EscRamp')
rp.strip([(-0.5, u_, H(u_) + 0.005) for u_ in us], [(0.5, u_, H(u_) + 0.005) for u_ in us], 'Truss')
ramp = rp.finish()

for o in (step, stair, body, ramp):
    print('ESC', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
if SHORT:
    body.name = 'SM_EscBodyShort'; ramp.name = 'SM_EscRampShort'
out = os.path.join(HERE, 'EscalatorShort.blend' if SHORT else 'Escalator.blend')
bpy.ops.wm.save_as_mainfile(filepath=out)
for o in ((body, ramp) if SHORT else (step, stair, body, ramp)):
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
print('ESC saved', out)
