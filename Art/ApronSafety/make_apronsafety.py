# Wheel chocks and apron safety cones (to-do 212, 2026-11-23), our own models.
# Sizes from published data: large aircraft chock 24 x 8 x 6 in (0.60 x 0.20 x 0.15 m), about 7.7 kg each, a pair joined by
# a 44 in (1.12 m) rope, reflective strips on both sides; apron cone 0.75 m on a 0.40 m rubber base, two reflective bands.
# No brand, no lettering. One mesh per part (the game instances them: Private/TugChocks.cpp):
#   Chock        origin on the ground under its middle, its length along y, the rope's ring at +y
#   RopeNose / RopeMain  the rope of a pair set at the nose / main wheel (chocks at x -+D, rope on the ground at +y)
#   RopeStow     of a pair lying side by side on the rack (chocks at x -+0.115)
#   RopeCarry    of a pair hanging from a hand: origin in the fist, the rings at (-+0.104, 0, -0.17)
#   Cone, Rack   origin on the ground
import bpy, bmesh, math, os, json
from mathutils import Vector
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ART = os.path.join(ROOT, 'Art', 'ApronSafety')
bpy.ops.wm.read_factory_settings(use_empty=True)
L, W, H, TOP = 0.60, 0.20, 0.15, 0.056
TYRE = {'Nose': 0.455, 'Main': 0.645}          # (Art/A320: the tyres' radius)
D = {k: round(math.sqrt(2 * r * H - H * H) + TOP / 2 + 0.012, 3) for k, r in TYRE.items()}   # chock middle from the wheel's


def mat(name, col, rough, metal=0.0, tex=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    if tex:
        d = os.path.join(ROOT, 'Art', 'GPU', 'Textures', tex)
        for f, sock in (('Color', 'Base Color'), ('NormalGL', 'Normal')):
            n = m.node_tree.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(os.path.join(d, '%s_2K-JPG_%s.jpg' % (tex, f)))
            if f == 'Color': m.node_tree.links.new(n.outputs[0], b.inputs[sock])
            else:
                n.image.colorspace_settings.name = 'Non-Color'
                nm = m.node_tree.nodes.new('ShaderNodeNormalMap'); m.node_tree.links.new(n.outputs[0], nm.inputs['Color']); m.node_tree.links.new(nm.outputs[0], b.inputs['Normal'])
    return m


RUB = mat('Chk_Rubber', (0.028, 0.028, 0.03), 0.85, 0, 'Rubber004')
STRIP = mat('Chk_ReflectYellow', (0.90, 0.62, 0.03), 0.32)
WHITE = mat('Chk_ReflectWhite', (0.86, 0.87, 0.88), 0.28)
ROPE = mat('Chk_Rope', (0.82, 0.60, 0.05), 0.8)
GRIP = mat('Chk_Grip', (0.45, 0.02, 0.015), 0.6)
STEEL = mat('Chk_Galvanised', (0.55, 0.57, 0.59), 0.45, 1.0, 'Metal011')
ORANGE = mat('Chk_ConeOrange', (0.86, 0.13, 0.015), 0.45)
OBJ = []


class B:
    """A mesh in several materials, built with bmesh."""
    def __init__(self, name): self.n, self.bm, self.ms = name, bmesh.new(), []
    def mi(self, m):
        if m not in self.ms: self.ms.append(m)
        return self.ms.index(m)
    def take(self, other, m, bevel=0.0, seg=2):
        if bevel > 0:
            es = [e for e in other.edges if len(e.link_faces) == 2 and e.calc_face_angle(0) > 0.5]
            bmesh.ops.bevel(other, geom=es, offset=bevel, segments=seg, profile=0.5, affect='EDGES', clamp_overlap=True)
        bmesh.ops.recalc_face_normals(other, faces=other.faces)
        k = self.mi(m)
        for f in other.faces: f.material_index = k; f.smooth = True
        me = bpy.data.meshes.new('t'); other.to_mesh(me); other.free(); self.bm.from_mesh(me); bpy.data.meshes.remove(me)
    def prism(self, pts, y0, y1, m, bevel=0.0, seg=2):
        b = bmesh.new()
        a = [b.verts.new((x, y0, z)) for x, z in pts]; c = [b.verts.new((x, y1, z)) for x, z in pts]
        b.faces.new(a); b.faces.new(c[::-1])
        for i in range(len(pts)):
            j = (i + 1) % len(pts); b.faces.new((a[i], a[j], c[j], c[i]))
        self.take(b, m, bevel, seg)
    def box(self, lo, hi, m, bevel=0.0): self.prism([(lo[0], lo[2]), (hi[0], lo[2]), (hi[0], hi[2]), (lo[0], hi[2])], lo[1], hi[1], m, bevel)
    def tube(self, pts, r, m, n=10, closed=False):
        ps = [Vector(p) for p in pts]; b = bmesh.new(); rings = []
        N = len(ps); up = None
        for i, p in enumerate(ps):
            t = ((ps[(i + 1) % N] - ps[i - 1]) if closed or 0 < i < N - 1 else (ps[1] - ps[0] if i == 0 else ps[-1] - ps[-2])).normalized()
            if up is None: up = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
            u = (up - t * up.dot(t)).normalized(); up = u; v = t.cross(u)
            rings.append([b.verts.new(p + (u * math.cos(a) + v * math.sin(a)) * r) for a in [2 * math.pi * k / n for k in range(n)]])
        for i in range(N if closed else N - 1):
            A, C = rings[i], rings[(i + 1) % N]
            for k in range(n): b.faces.new((A[k], A[(k + 1) % n], C[(k + 1) % n], C[k]))
        if not closed: b.faces.new(rings[0][::-1]); b.faces.new(rings[-1])
        self.take(b, m)
    def lathe(self, prof, m, n=48, cy=0.0, closed=True):
        b = bmesh.new(); rs = [[b.verts.new((r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n), z)) for k in range(n)] for r, z in prof]
        for i in range(len(prof) if closed else len(prof) - 1):
            A, C = rs[i], rs[(i + 1) % len(prof)]
            for k in range(n): b.faces.new((A[k], A[(k + 1) % n], C[(k + 1) % n], C[k]))
        if not closed: b.faces.new(rs[0][::-1]); b.faces.new(rs[-1])
        self.take(b, m)
    def done(self):
        bm = self.bm; bm.normal_update()
        uv = bm.loops.layers.uv.new('UVMap')
        for f in bm.faces:                                        # box projection, 1 m = 2 tiles, nothing stretched
            n = f.normal; ax = max(range(3), key=lambda i: abs(n[i])); a, c = [(1, 2), (0, 2), (0, 1)][ax]
            for l in f.loops: l[uv].uv = (l.vert.co[a] * 2, l.vert.co[c] * 2)
        me = bpy.data.meshes.new(self.n); bm.to_mesh(me); bm.free()
        for m in self.ms: me.materials.append(m)
        me.use_auto_smooth = True; me.auto_smooth_angle = math.radians(38)
        o = bpy.data.objects.new(self.n, me); bpy.context.scene.collection.objects.link(o); OBJ.append(o)
        wn = o.modifiers.new('wn', 'WEIGHTED_NORMAL'); wn.keep_sharp = True
        return o


def spline(pts, n=10):
    P = [Vector(p) for p in pts]; P = [2 * P[0] - P[1]] + P + [2 * P[-1] - P[-2]]; out = []
    for i in range(1, len(P) - 2):
        a, b, c, d = P[i - 1:i + 3]
        for k in range(n):
            t = k / n
            out.append(0.5 * (2 * b + (c - a) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (3 * b - a - 3 * c + d) * t ** 3))
    return out + [P[-2]]


def circle(n): return [2 * math.pi * k / n for k in range(n)]


# ------------------------------------------------------------ the chock
RING = Vector((0, L / 2 + 0.028, 0.062))
c = B('Chock')
c.prism([(-W / 2, 0), (-0.035, 0), (-0.028, 0.008), (0.028, 0.008), (0.035, 0), (W / 2, 0), (W / 2, 0.022), (TOP / 2, H), (-TOP / 2, H), (-W / 2, 0.022)], -L / 2, L / 2, RUB, 0.007, 3)
sl = Vector((W / 2 - TOP / 2, 0, -(H - 0.022))); ln = sl.length; sl.normalize(); nr = Vector((-sl.z, 0, sl.x))
for s in (-1, 1):
    def q(t, o):
        v = Vector((TOP / 2, 0, H)) + sl * t + nr * o
        return (s * v.x, v.z)
    # grip ribs on the lower face, the reflective strip above them
    for k in range(3):
        t = ln * (0.56 + 0.13 * k)
        c.prism([q(t - .007, -.004), q(t - .004, .0045), q(t + .004, .0045), q(t + .007, -.004)], -L / 2 + .03, L / 2 - .03, RUB, 0.0015, 1)
    c.prism([q(ln * .13, -.002), q(ln * .13, .0022), q(ln * .45, .0022), q(ln * .45, -.002)], -L / 2 + .045, L / 2 - .045, STRIP)
# the moulded rope lug at each end, the steel ring at the +y one
c.box((-0.03, L / 2 - 0.012, 0.034), (0.03, L / 2 + 0.016, 0.09), RUB, 0.006)
c.box((-0.03, -L / 2 - 0.016, 0.034), (0.03, -L / 2 + 0.012, 0.09), RUB, 0.006)
c.tube([(0, RING.y - 0.004 + 0.022 * math.cos(a), RING.z + 0.022 * math.sin(a)) for a in circle(20)], 0.0045, STEEL, 8, True)
c.done()


def grip(b, pts):
    i = len(pts) // 2
    b.tube(pts[i - 4:i + 5], 0.0155, GRIP, 12)


# ------------------------------------------------------------ its ropes
RY = RING.y + 0.016
for k in ('Nose', 'Main'):
    d = D[k]; b = B('Rope' + k)
    pts = spline([(-d, RY, RING.z), (-d + .015, RY + .035, .02), (-d * .5, RY + .13, .0115), (0, RY + .10, .0155), (d * .5, RY + .15, .0115), (d - .015, RY + .035, .02), (d, RY, RING.z)])
    b.tube(pts, 0.010, ROPE); grip(b, pts); b.done()
b = B('RopeStow')
pts = spline([(-.115, RY, RING.z), (-.11, RY + .03, .12), (-.06, L / 2 - .02, .185), (-.05, .06, .171), (0, 0, .172), (.05, .06, .171), (.06, L / 2 - .02, .185), (.11, RY + .03, .12), (.115, RY, RING.z)])
b.tube(pts, 0.010, ROPE); grip(b, pts); b.done()
b = B('RopeCarry')     # the rope's middle is the grip in the fist, lying along y; the slack hangs in a loop
pts = spline([(-.104, 0, -.165), (-.07, -.05, -.08), (-.014, -.075, -.012), (0, -.04, 0), (0, 0, 0), (0, .04, 0), (.014, .075, -.012), (.07, .05, -.08), (.104, 0, -.165)])
b.tube(pts, 0.010, ROPE); grip(b, pts)
b.tube(spline([(-.016, -.084, -.02), (-.03, -.10, -.14), (0, -.10, -.22), (.03, -.10, -.14), (.012, -.088, -.03)]), 0.010, ROPE); b.done()

# ------------------------------------------------------------ the cone (hollow: they stack, 6 cm a cone)
R0, R1, Z0, Z1, h, N = 0.150, 0.029, 0.034, 0.75, 0.20, 48
rad = lambda z: R0 + (R1 - R0) * (z - Z0) / (Z1 - Z0)
c = B('Cone'); bm = bmesh.new(); HOLE = 0.1435
def ringv(r, z): return [bm.verts.new((r * math.cos(a), r * math.sin(a), z)) for a in circle(N)]
def sqv(r, z):
    out = []
    for a in circle(N):
        ca, sa = math.cos(a), math.sin(a); e = (abs(ca) ** 14 + abs(sa) ** 14) ** (-1 / 14); out.append(bm.verts.new((r * e * ca, r * e * sa, z)))
    return out
rs = [ringv(HOLE, 0.006), sqv(h - .035, 0.006), sqv(h - .030, 0.0), sqv(h - .004, 0.0), sqv(h, 0.004), sqv(h, 0.022), sqv(h - .008, 0.030), ringv(R0 + .012, 0.034), ringv(R0 - .002, 0.036), ringv(HOLE, 0.031)]
for i in range(len(rs)):
    A, C = rs[i], rs[(i + 1) % len(rs)]
    for k in range(N): bm.faces.new((A[k], A[(k + 1) % N], C[(k + 1) % N], C[k]))
c.take(bm, RUB)
t = 0.0035
c.lathe([(R0 + .004, Z0 - .003), (rad(Z0 + .02), Z0 + .02), (rad(Z1 - .012), Z1 - .012), (R1 - .001, Z1 - .004), (R1 - .004, Z1), (R1 - .009, Z1 - .003), (rad(Z1 - .012) - t, Z1 - .02), (rad(Z0 + .02) - t, Z0 + .02), (R0 - t - .0015, Z0 - .0015)], ORANGE)
for z0, z1 in ((0.285, 0.415), (0.505, 0.605)):
    c.lathe([(rad(z0) - .001, z0), (rad(z0) + .0016, z0 + .001), (rad(z1) + .0016, z1 - .001), (rad(z1) - .001, z1)], WHITE)
c.done()

# ------------------------------------------------------------ the rack: the shelf for three pairs, the tray for the cones
b = B('Rack'); X, Y0, Y1, ZS, T = 0.34, -0.98, 0.50, 0.26, 0.04
for x in (-X, X - T):
    for y in (Y0 + .001, Y1 - T - .001):
        b.box((x + .001, y, 0.010), (x + T - .001, y + T, ZS - T + .003), STEEL, 0.003)
        b.box((x - .015, y - .015, 0), (x + T + .015, y + T + .015, 0.012), STEEL, 0.002)
    b.box((x, Y0, ZS - T), (x + T, Y1, ZS), STEEL, 0.004)
for k, y in enumerate((Y0 + .0015, Y1 - T - .0015, Y0 + .49, Y0 + .98)): b.box((-X + T - .003, y, ZS - T + .0015), (X - T + .003, y + T, ZS - .0015), STEEL, 0.003)
for k in range(9): b.box((-X + T + .05 + k * .062, Y0 + T - .003, ZS - .025), (-X + T + .062 + k * .062, Y1 - T + .003, ZS - .006), STEEL)
for y in (Y0 + .005, Y1 - .017): b.box((-X + .01, y, ZS - .003), (X - .01, y + .012, ZS + .06), STRIP, 0.002)
b.box((-.24, Y1 + .04, .008), (.24, Y1 + .50, .02), STEEL, .002)
b.box((-.02, Y1 - .003, .10), (.02, Y1 + .06, .13), STEEL, .003)
b.box((-.0195, Y1 + .042, .017), (.0195, Y1 + .0605, .1285), STEEL, .002)
for s in (-1, 1): b.box((s * .232 - .008, Y1 + .065, .017), (s * .232 + .008, Y1 + .495, .05), STRIP, .002)
b.lathe([(.032, .017), (.032, .028), (.016, .034), (.016, .60), (.008, .615)], STEEL, 20, Y1 + .27, False)
b.done()

for i, o in enumerate(OBJ): o.location.x = i * 2.0     # (apart from each other: the flicker check; each mesh keeps its own origin)
for o in OBJ:
    assert o.data.materials and all(o.data.materials) and all(p.material_index < len(o.data.materials) for p in o.data.polygons), o.name
json.dump({'D': D, 'tris': {o.name: sum(len(p.vertices) - 2 for p in o.data.polygons) for o in OBJ}}, open(os.path.join(ART, 'sizes.json'), 'w'))
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ART, 'ApronSafety.blend'))
bpy.ops.export_scene.gltf(filepath=os.path.join(ART, 'apronsafety.glb'), export_format='GLB', export_apply=True, export_yup=True)
print('DONE', json.dumps(D))
