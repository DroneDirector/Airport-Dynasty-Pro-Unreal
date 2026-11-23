# To-do 213 detailed renders: the helpers for build_new.py (materials, a mesh batcher, shops, seats, booths, signs, people).
import bpy, json, math, os, random, bmesh
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
sc = bpy.context.scene; OX = 200.0
EX = bpy.data.collections['EXIST']


def coll(n, par=None):
    c = bpy.data.collections.new(n); (par or sc.collection).children.link(c); return c


LAND = coll('LANDSIDE'); NEWC = coll('NEW'); GOLDC = coll('GOLD')
SUB = {k: coll('NEW_' + k, NEWC) for k in ('ground', 'f2', 'roof', 'out')}
GOLDS = {k: coll('GOLD_' + k, GOLDC) for k in SUB}


def P(x, z, h=0.0): return Vector((x - OX, z, h))


def mat(n, c, r=0.6, m=0.0, e=0.0):
    M = bpy.data.materials.new(n); M.use_nodes = True; b = M.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*c, 1); b.inputs['Roughness'].default_value = r; b.inputs['Metallic'].default_value = m
    if e: b.inputs['Emission'].default_value = (*c, 1); b.inputs['Emission Strength'].default_value = e
    return M


def noisy(n, c1, c2, r, scale, stretch=(1, 1, 1)):
    M = mat(n, c1, r); nt = M.node_tree; b = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeNewGeometry'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = stretch
    nt.links.new(tc.outputs['Position'], mp.inputs['Vector'])
    t = nt.nodes.new('ShaderNodeTexNoise'); t.inputs['Scale'].default_value = scale; t.inputs['Detail'].default_value = 8; t.inputs['Distortion'].default_value = 1.2
    ra = nt.nodes.new('ShaderNodeValToRGB'); ra.color_ramp.elements[0].position = 0.4; ra.color_ramp.elements[1].position = 0.62
    ra.color_ramp.elements[0].color = (*c1, 1); ra.color_ramp.elements[1].color = (*c2, 1)
    nt.links.new(mp.outputs[0], t.inputs['Vector']); nt.links.new(t.outputs['Fac'], ra.inputs[0]); nt.links.new(ra.outputs[0], b.inputs['Base Color'])
    return M


def glassm(n, tint, fac=0.10):
    M = bpy.data.materials.new(n); M.use_nodes = True; nt = M.node_tree; nt.nodes.clear()
    o = nt.nodes.new('ShaderNodeOutputMaterial'); mx = nt.nodes.new('ShaderNodeMixShader'); tr = nt.nodes.new('ShaderNodeBsdfTransparent'); gl = nt.nodes.new('ShaderNodeBsdfGlossy')
    fr = nt.nodes.new('ShaderNodeLayerWeight'); fr.inputs['Blend'].default_value = 0.3
    tr.inputs[0].default_value = (*tint, 1); gl.inputs[0].default_value = (0.9, 0.95, 1, 1); gl.inputs['Roughness'].default_value = 0.01
    ma = nt.nodes.new('ShaderNodeMath'); ma.operation = 'MULTIPLY_ADD'; ma.inputs[1].default_value = 0.4; ma.inputs[2].default_value = fac; ma.use_clamp = True
    nt.links.new(fr.outputs['Fresnel'], ma.inputs[0]); nt.links.new(ma.outputs[0], mx.inputs[0]); nt.links.new(tr.outputs[0], mx.inputs[1]); nt.links.new(gl.outputs[0], mx.inputs[2]); nt.links.new(mx.outputs[0], o.inputs[0]); return M


M = dict(
    GLASS=glassm('N2_glass', (0.86, 0.94, 0.95)), SHOPG=glassm('N2_shopglass', (0.96, 0.98, 0.98), 0.05),
    MARBLE=noisy('N2_marble', (0.72, 0.70, 0.66), (0.6, 0.57, 0.52), 0.16, 0.55), TERR=noisy('N2_paving', (0.40, 0.39, 0.37), (0.34, 0.33, 0.32), 0.8, 0.8),
    DWOOD=noisy('N2_darkwood', (0.15, 0.085, 0.05), (0.09, 0.05, 0.03), 0.35, 2.0, (0.3, 4, 1)), OAK=noisy('N2_oak', (0.45, 0.29, 0.16), (0.36, 0.22, 0.12), 0.5, 2.0, (0.4, 5, 1)),
    MUL=mat('N2_mullion', (0.07, 0.075, 0.085), 0.35, 0.7), PLAST=mat('N2_plaster', (0.70, 0.69, 0.66), 0.8), WHITE=mat('N2_white', (0.72, 0.73, 0.74), 0.3),
    ZINC=mat('N2_zinc', (0.45, 0.47, 0.5), 0.4, 0.7), DESK=mat('N2_desk', (0.66, 0.67, 0.69), 0.25), DARK=mat('N2_dark', (0.03, 0.035, 0.045), 0.4), STEEL=mat('N2_steel', (0.5, 0.51, 0.53), 0.3, 0.9),
    RUBBER=mat('N2_rubber', (0.025, 0.025, 0.028), 0.6), CARPET=noisy('N2_carpet', (0.05, 0.08, 0.14), (0.035, 0.055, 0.1), 0.95, 9), NAVY=mat('N2_navy', (0.02, 0.06, 0.16), 0.6), RUST=mat('N2_rust', (0.35, 0.1, 0.04), 0.7),
    SEAT=mat('N2_seat', (0.06, 0.09, 0.16), 0.55), SCREEN=mat('N2_screen', (0.05, 0.22, 0.55), 0.3, 0, 1.2), GRN=mat('N2_green', (0.02, 0.4, 0.12), 0.4, 0, 1.2), RED=mat('N2_red', (0.6, 0.03, 0.03), 0.4, 0, 1.2),
    YSIGN=mat('N2_yellow', (0.9, 0.6, 0.03), 0.4, 0, 0.6), SIGNBG=mat('N2_signbg', (0.015, 0.02, 0.03), 0.35), SIGNTX=mat('N2_signtext', (1, 1, 1), 0.4, 0, 1.8), SIGNYT=mat('N2_signtext_y', (1, 0.7, 0.08), 0.4, 0, 1.8),
    LAMP=mat('N2_lamp', (1, 0.93, 0.82), 0.4, 0, 5.0), SHOPLT=mat('N2_shoplight', (1, 0.88, 0.7), 0.4, 0, 5.0), LEAF=mat('N2_leaf', (0.05, 0.16, 0.04), 0.8), POT=mat('N2_pot', (0.5, 0.48, 0.45), 0.6),
    ASPH=noisy('L_asphalt', (0.05, 0.05, 0.053), (0.07, 0.07, 0.072), 0.9, 0.7), GRASS=noisy('L_grass', (0.10, 0.125, 0.05), (0.075, 0.10, 0.04), 0.95, 0.15), CONC=noisy('L_concrete', (0.36, 0.355, 0.34), (0.31, 0.305, 0.295), 0.85, 0.2), KERB=mat('L_kerb', (0.5, 0.49, 0.47), 0.8),
    PAINT=mat('L_paint', (0.7, 0.7, 0.7), 0.6), PYEL=mat('L_paintyellow', (0.8, 0.55, 0.03), 0.6), TAXI=mat('N2_taxi', (0.85, 0.58, 0.02), 0.25, 0.2), CARGL=mat('N2_carglass', (0.02, 0.025, 0.03), 0.08), COACH=mat('N2_coach', (0.7, 0.72, 0.75), 0.25, 0.1),
    GOLD=mat('X2_GOLD', (1.0, 0.5, 0.04), 0.4, 0, 2.6))
for i, c in enumerate(((0.55, 0.06, 0.08), (0.7, 0.5, 0.08), (0.08, 0.2, 0.45), (0.1, 0.35, 0.15), (0.6, 0.6, 0.62), (0.35, 0.12, 0.35), (0.8, 0.35, 0.1))): M['P%d' % i] = mat('N2_prod%d' % i, c, 0.45)
for i, c in enumerate(((0.6, 0.6, 0.62), (0.03, 0.03, 0.035), (0.35, 0.03, 0.03), (0.08, 0.12, 0.25), (0.3, 0.31, 0.33), (0.75, 0.75, 0.76))): M['CAR%d' % i] = mat('L_car%d' % i, c, 0.22, 0.3)

BAT = {}
def bat(col, m): return BAT.setdefault((col.name, m), (col, [], []))
CUB = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
G = SUB['ground']; F = SUB['f2']; RF = SUB['roof']; O = SUB['out']


def box(x0, z0, x1, z1, h0, h1, m, col=None):
    c, V, Fc = bat(col or G, m); b = len(V)
    V += [P(x, z, h) for h in (h0, h1) for (x, z) in ((x0, z0), (x1, z0), (x1, z1), (x0, z1))]; Fc += [tuple(b + i for i in f) for f in CUB]


def quad(pts, m, col=None):
    c, V, Fc = bat(col or G, m); b = len(V); V += pts; Fc.append(tuple(range(b, b + len(pts))))


def cyl(a, b, r, m, col=None, n=12, r2=None):
    a, b = Vector(a), Vector(b); d = (b - a).normalized(); u = d.orthogonal().normalized(); w = d.cross(u); r2 = r if r2 is None else r2
    c, V, Fc = bat(col or G, m); k = len(V)
    for (q, rr) in ((a, r), (b, r2)): V += [q + (u * math.cos(2 * math.pi * i / n) + w * math.sin(2 * math.pi * i / n)) * rr for i in range(n)]
    Fc += [(k + i, k + (i + 1) % n, k + n + (i + 1) % n, k + n + i) for i in range(n)] + [tuple(k + i for i in reversed(range(n))), tuple(k + n + i for i in range(n))]


def gold(x0, z0, x1, z1, h0, h1, tag='ground', w=0.065, base=False):
    """the gold line round a new part (12 edges; base: only the floor outline)"""
    g = GOLDS[tag]
    for h in ((h0,) if base else (h0, h1)):
        for z in (z0, z1): box(x0 - w, z - w, x1 + w, z + w, h - w, h + w, 'GOLD', g)
        for x in (x0, x1): box(x - w, z0 - w, x + w, z1 + w, h - w, h + w, 'GOLD', g)
    if not base:
        for x in (x0, x1):
            for z in (z0, z1): box(x - w, z - w, x + w, z + w, h0, h1, 'GOLD', g)


FONT = bpy.data.fonts.load('C:/Windows/Fonts/segoeuib.ttf')


def text(s, x, z, h, size, m='SIGNTX', col=None, face='s'):
    """lettering standing upright, read by someone standing on the landside ('s'), airside ('n'), west ('w') or east ('e') of it"""
    cu = bpy.data.curves.new('T', 'FONT'); cu.body = s; cu.font = FONT; cu.size = size; cu.align_x = 'CENTER'; cu.align_y = 'CENTER'; cu.extrude = 0.01; cu.materials.append(M[m])
    o = bpy.data.objects.new('Sign ' + s, cu); (col or G).objects.link(o); o.location = P(x, z, h)
    o.rotation_euler = (math.radians(90), 0, {'s': 0, 'n': math.pi, 'w': -math.pi / 2, 'e': math.pi / 2}[face])


def sign(s, x, z, h, w, hh, face='s', col=None, tx='SIGNTX', bg='SIGNBG'):
    if face in 'sn':
        box(x - w / 2, z - 0.08, x + w / 2, z + 0.08, h - hh / 2, h + hh / 2, bg, col); text(s, x, z + (-0.1 if face == 's' else 0.1), h, hh * 0.62, tx, col, face)
    else:
        box(x - 0.08, z - w / 2, x + 0.08, z + w / 2, h - hh / 2, h + hh / 2, bg, col); text(s, x + (-0.1 if face == 'w' else 0.1), z, h, hh * 0.62, tx, col, face)


# ---------------------------------------------------------------- the real wave soffit (ray-cast)
bpy.context.view_layer.update()
VV, FF = [], []
for o in EX.objects:
    if o.type == 'MESH' and 'WAVE-SOFFIT' in o.name:
        b = len(VV); VV += [o.matrix_world @ v.co for v in o.data.vertices]; FF += [tuple(b + i for i in p.vertices) for p in o.data.polygons]
BVH = BVHTree.FromPolygons(VV, FF)


def soffit(x, z=0.0):
    h = BVH.ray_cast(P(x, z, 2.0), Vector((0, 0, 1)))[0]; return h.z if h else 12.0


def mkperson(i, CLO, SKIN, HAIR):
    bm = bmesh.new()

    def paint(ret, mi):
        for v in ret['verts']:
            for f in v.link_faces: f.material_index = mi

    def c(a, b, r1, r2, mi, n=10):
        a, b = Vector(a), Vector(b); d = b - a
        paint(bmesh.ops.create_cone(bm, cap_ends=True, segments=n, radius1=r1, radius2=r2, depth=d.length, matrix=Matrix.Translation((a + b) / 2) @ d.to_track_quat('Z', 'Y').to_matrix().to_4x4()), mi)
    st = 0.12 + 0.05 * (i % 3)
    c((-0.09, st, 0), (-0.09, 0, 0.88), 0.06, 0.1, 1); c((0.09, -st, 0), (0.09, 0, 0.88), 0.06, 0.1, 1)
    c((0, 0, 0.84), (0, 0, 1.2), 0.17, 0.18, 0); c((0, 0, 1.2), (0, 0, 1.46), 0.19, 0.1, 0); c((0, 0, 1.46), (0, 0, 1.56), 0.06, 0.06, 2)
    c((-0.22, 0, 1.4), (-0.25, -0.08, 0.88), 0.055, 0.04, 0); c((0.22, 0, 1.4), (0.25, 0.1, 0.88), 0.055, 0.04, 0)
    paint(bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.115, matrix=Matrix.Translation((0, 0, 1.64))), 2)
    paint(bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.122, matrix=Matrix.Translation((0, 0.02, 1.67))), 3)
    if i % 2: paint(bmesh.ops.create_cube(bm, size=1, matrix=Matrix.Translation((0.42, 0.15, 0.32)) @ Matrix.Diagonal((0.25, 0.4, 0.58, 1))), 4)
    me = bpy.data.meshes.new('Person%d' % i); bm.to_mesh(me); bm.free()
    for m in (CLO[i % 8], CLO[(i * 3 + 4) % 8], SKIN, HAIR, CLO[(i + 5) % 8]): me.materials.append(m)
    for p in me.polygons: p.use_smooth = True
    return me


CLO = [mat('C2_%d' % i, c, 0.8) for i, c in enumerate(((0.05, 0.07, 0.15), (0.3, 0.05, 0.05), (0.06, 0.18, 0.1), (0.25, 0.2, 0.13), (0.1, 0.1, 0.11), (0.5, 0.42, 0.1), (0.5, 0.5, 0.5), (0.1, 0.2, 0.35)))]
PPL = [mkperson(i, CLO, mat('C2_skin', (0.55, 0.36, 0.26), 0.6), mat('C2_hair', (0.04, 0.025, 0.015), 0.8)) for i in range(12)]
random.seed(21)


def people(xa, xb, za, zb, n, h=0.0, col=None):
    for _ in range(n):
        o = bpy.data.objects.new('Person', random.choice(PPL)); (col or G).objects.link(o); o.location = P(random.uniform(xa, xb), random.uniform(za, zb), h); o.rotation_euler = (0, 0, random.uniform(0, 6.28)); s = random.uniform(0.94, 1.04); o.scale = (s, s, s)


def plant(x, z, h=0.0, col=None, s=1.0):
    cyl(P(x, z, h), P(x, z, h + 0.6 * s), 0.35 * s, 'POT', col, 14, 0.45 * s)
    for k in range(9):
        a = k * 2.4; cyl(P(x, z, h + 0.5 * s), P(x + math.cos(a) * 0.6 * s, z + math.sin(a) * 0.6 * s, h + (1.4 + 0.12 * k) * s), 0.16 * s, 'LEAF', col, 6, 0.02)


def seats(x, z, h=0.0, n=4, col=None):
    box(x, z - 0.02, x + n * 0.62, z + 0.02, h + 0.3, h + 0.36, 'STEEL', col)
    for i in range(n):
        xs = x + i * 0.62 + 0.04; box(xs, z - 0.25, xs + 0.54, z + 0.25, h + 0.38, h + 0.48, 'SEAT', col); box(xs, z + 0.21, xs + 0.54, z + 0.29, h + 0.48, h + 0.95, 'SEAT', col)
    for xl in (x + 0.1, x + n * 0.62 - 0.1): box(xl - 0.04, z - 0.2, xl + 0.04, z + 0.2, h, h + 0.32, 'STEEL', col)


def table(x, z, h, col):
    cyl(P(x, z, h), P(x, z, h + 0.72), 0.06, 'STEEL', col); cyl(P(x, z, h + 0.72), P(x, z, h + 0.76), 0.45, 'OAK', col, 18)
    for (dx, dz) in ((-0.75, 0), (0.75, 0), (0, 0.75), (0, -0.75)):
        box(x + dx - 0.2, z + dz - 0.2, x + dx + 0.2, z + dz + 0.2, h + 0.42, h + 0.47, 'RUST', col); cyl(P(x + dx, z + dz, h), P(x + dx, z + dz, h + 0.42), 0.04, 'STEEL', col, 8)


def shelf(x, z, h, col, rnd):
    box(x, z, x + 1.9, z + 0.8, h, h + 0.12, 'DARK', col); box(x, z + 0.38, x + 1.9, z + 0.42, h, h + 1.35, 'OAK', col)
    for sh in (0.35, 0.75, 1.15):
        box(x, z + 0.05, x + 1.9, z + 0.75, h + sh, h + sh + 0.03, 'WHITE', col)
        for q in range(5):
            pm = 'P%d' % rnd.randrange(7); box(x + 0.08 + q * 0.36, z + 0.1, x + 0.3 + q * 0.36, z + 0.32, h + sh + 0.03, h + sh + 0.2 + 0.1 * rnd.random(), pm, col); box(x + 0.08 + q * 0.36, z + 0.48, x + 0.3 + q * 0.36, z + 0.7, h + sh + 0.03, h + sh + 0.2 + 0.1 * rnd.random(), pm, col)


def shop(x0, z0, x1, z1, h, top, name, col=None, front='s', kind='shelf', floor='MARBLE', tsize=0.62):
    """a shop unit: back and side walls, a glass front with an opening and a lit sign band, shelves / tables, lights"""
    col = col or G; t = 0.12
    box(x0, z0, x1, z1, h + 0.005, h + 0.02, floor, col)
    sides = dict(s=(x0, z0, x1, z0 + t), n=(x0, z1 - t, x1, z1), w=(x0, z0, x0 + t, z1), e=(x1 - t, z0, x1, z1))
    for k, (a, b, c, d) in sides.items():
        if k not in front: box(a, b, c, d, h, top, 'PLAST', col)
    for fr in front:
        a, b, c, d = sides[fr]; L = (c - a) if fr in 'sn' else (d - b); box(a, b, c, d, top - 0.9, top, 'SIGNBG', col); gap = min(6.0, L * 0.4)
        if fr in 'sn':
            mx = (a + c) / 2
            for (xa, xb) in ((a, mx - gap / 2), (mx + gap / 2, c)): box(xa, (b + d) / 2 - 0.02, xb, (b + d) / 2 + 0.02, h, top - 0.9, 'SHOPG', col)
            text(name, mx, (b - 0.04) if fr == 's' else (d + 0.04), top - 0.45, tsize, 'SIGNTX', col, fr)
        else:
            mz = (b + d) / 2
            for (za, zb) in ((b, mz - gap / 2), (mz + gap / 2, d)): box((a + c) / 2 - 0.02, za, (a + c) / 2 + 0.02, zb, h, top - 0.9, 'SHOPG', col)
            text(name, (a - 0.04) if fr == 'w' else (c + 0.04), mz, top - 0.45, tsize, 'SIGNTX', col, fr)
    rnd = random.Random(int(x0 * 7 + z0))
    for i in range(max(int((x1 - x0 - 1) / 3.2), 1)):
        for j in range(max(int((z1 - z0 - 1) / 3.0), 1)):
            x = x0 + 1.0 + i * 3.2; z = z0 + 1.4 + j * 3.0
            if x + 1.9 > x1 - 0.3 or z + 0.8 > z1 - 0.3: continue
            if kind == 'shelf': shelf(x, z, h, col, rnd)
            elif kind == 'table': table(x + 0.9, z + 0.4, h, col)
            elif kind == 'lounge':
                box(x, z, x + 1.8, z + 0.8, h + 0.15, h + 0.45, 'NAVY', col); box(x, z + 0.65, x + 1.8, z + 0.85, h + 0.15, h + 0.85, 'NAVY', col); cyl(P(x + 0.9, z - 0.6, h), P(x + 0.9, z - 0.6, h + 0.42), 0.35, 'OAK', col, 16)
    for i in range(int((x1 - x0) / 4)):
        for zl in ([(z0 + z1) / 2] if z1 - z0 < 10 else [z0 + 4, z1 - 4]): box(x0 + 1.5 + i * 4, zl - 0.15, x0 + 3 + i * 4, zl + 0.15, top - 0.25, top - 0.2, 'SHOPLT', col)


def egate(x, z, h=0.0, col=None, L=1.8, along='z'):
    if along == 'z':
        box(x, z, x + 0.28, z + L, h, h + 1.05, 'STEEL', col); box(x, z + 0.1, x + 0.28, z + L - 0.1, h + 1.05, h + 1.09, 'GRN', col); box(x + 0.28, z + L / 2 - 0.02, x + 0.8, z + L / 2 + 0.02, h + 0.15, h + 1.5, 'SHOPG', col)
    else:
        box(x, z, x + L, z + 0.28, h, h + 1.05, 'STEEL', col); box(x + 0.1, z, x + L - 0.1, z + 0.28, h + 1.05, h + 1.09, 'GRN', col); box(x + L / 2 - 0.02, z + 0.28, x + L / 2 + 0.02, z + 1.0, h + 0.15, h + 1.6, 'SHOPG', col)


def booth(x0, z0, x1, z1, h=0.0, col=None):
    box(x0, z0, x1, z1, h, h + 1.05, 'DWOOD', col); box(x0, z0, x1, z1, h + 2.3, h + 2.42, 'SIGNBG', col)
    for (a, b, c, d) in ((x0, z0, x1, z0 + 0.04), (x0, z1 - 0.04, x1, z1), (x0, z0, x0 + 0.04, z1), (x1 - 0.04, z0, x1, z1)): box(a, b, c, d, h + 1.05, h + 2.3, 'SHOPG', col)
    box((x0 + x1) / 2 - 0.3, (z0 + z1) / 2 - 0.1, (x0 + x1) / 2 + 0.3, (z0 + z1) / 2, h + 1.05, h + 1.5, 'SCREEN', col)


def kiosk(x, z, col=None, h=0.0):
    box(x, z, x + 0.6, z + 0.5, h, h + 1.0, 'DARK', col); box(x, z + 0.1, x + 0.6, z + 0.4, h + 1.0, h + 1.65, 'DARK', col); box(x + 0.05, z + 0.08, x + 0.55, z + 0.1, h + 1.05, h + 1.6, 'SCREEN', col)


def trolleys(x, z, n, col=None):
    for i in range(n):
        a = x + i * 0.35
        for (za, zb, ha, hb) in ((0, 0.6, 0.12, 0.3), (0, 0.6, 0.94, 1.0), (0, 0.04, 0.2, 1.0), (0.56, 0.6, 0.2, 1.0)): box(a, z + za, a + 0.05, z + zb, ha, hb, 'STEEL', col)
    box(x - 0.2, z - 0.1, x + n * 0.35 + 0.4, z - 0.04, 0.4, 0.5, 'STEEL', col)


def belt(cx, z0, z1, hw):
    for (a, b, c, d) in ((cx - hw, z0, cx - hw + 1.3, z1), (cx + hw - 1.3, z0, cx + hw, z1), (cx - hw, z0, cx + hw, z0 + 1.3), (cx - hw, z1 - 1.3, cx + hw, z1)):
        box(a, b, c, d, 0.35, 0.45, 'RUBBER'); box(a - 0.1, b - 0.1, c + 0.1, d + 0.1, 0, 0.36, 'STEEL')
    zm = (z0 + z1) / 2
    box(cx - hw + 1.4, z0 + 1.4, cx + hw - 1.4, z1 - 1.4, 0, 0.5, 'STEEL'); cyl(P(cx, zm, 0.5), P(cx, zm, 3.4), 0.12, 'STEEL'); box(cx - 1.6, zm - 0.1, cx + 1.6, zm + 0.1, 2.3, 3.4, 'DARK'); box(cx - 1.5, zm - 0.13, cx + 1.5, zm + 0.13, 2.4, 3.3, 'SCREEN')
    for k in range(26):
        z = random.uniform(z0 + 1.5, z1 - 1.5); x = cx + random.choice((-1, 1)) * (hw - 0.65); box(x - 0.25, z - 0.35, x + 0.25, z + 0.35, 0.45, 0.45 + random.uniform(0.25, 0.5), 'P%d' % random.randrange(7))


def fids(x, z, h, w, n, face='s', col=None, title='DEPARTURES'):
    box(x - w / 2, z - 0.1, x + w / 2, z + 0.1, h, h + n * 0.32 + 0.8, 'DARK', col); zz = z + (-0.12 if face == 's' else 0.12)
    text(title, x, zz, h + n * 0.32 + 0.45, 0.45, 'SIGNYT', col, face)
    for i in range(n): box(x - w / 2 + 0.3, min(zz, z), x + w / 2 - 0.3, max(zz, z), h + 0.2 + i * 0.32, h + 0.38 + i * 0.32, 'SCREEN' if i % 2 else 'NAVY', col)


def lamps(xa, xb, zs, h, col, step=6.0):
    x = xa
    while x < xb:
        for z in zs: box(x, z - 0.12, x + 2.2, z + 0.12, h, h + 0.04, 'LAMP', col)
        x += step


def car(x, z, m, yaw90, col):
    L, W = 4.4, 1.8; a, b = (L, W) if yaw90 else (W, L)
    box(x, z, x + a, z + b, 0.3, 0.85, m, col)
    if yaw90: box(x + 0.9, z + 0.12, x + 3.6, z + b - 0.12, 0.85, 1.4, 'CARGL', col); box(x + 1.2, z + 0.15, x + 3.2, z + b - 0.15, 1.4, 1.46, m, col)
    else: box(x + 0.12, z + 0.9, x + a - 0.12, z + 3.6, 0.85, 1.4, 'CARGL', col); box(x + 0.15, z + 1.2, x + a - 0.15, z + 3.2, 1.4, 1.46, m, col)
    for (dx, dz) in ((0.7, 0.05), (3.6, 0.05), (0.7, 1.75), (3.6, 1.75)):
        wx, wz = (x + dx, z + dz) if yaw90 else (x + dz, z + dx); e = Vector((0, 0.1, 0)) if yaw90 else Vector((0.1, 0, 0)); cyl(P(wx, wz, 0.32) - e, P(wx, wz, 0.32) + e, 0.32, 'RUBBER', col, 12)


def coach(x, z, col, L=12.0):
    box(x, z, x + L, z + 2.5, 0.35, 1.5, 'COACH', col); box(x, z, x + L, z + 2.5, 2.9, 3.5, 'COACH', col); box(x + 0.05, z + 0.05, x + L - 0.05, z + 2.45, 1.5, 2.9, 'CARGL', col)
    for wx in (2.2, 8.6):
        for wz in (0.02, 2.48): cyl(P(x + wx, z + wz - 0.12, 0.5), P(x + wx, z + wz + 0.12, 0.5), 0.5, 'RUBBER', col, 16)


def flush():
    for (cn, m), (c, V, Fc) in BAT.items():
        me = bpy.data.meshes.new(cn + '_' + m); me.from_pydata(V, [], Fc); me.materials.append(M[m]); me.update()
        o = bpy.data.objects.new(cn + '_' + m, me); c.objects.link(o)
        if m == 'GOLD': o.visible_shadow = False; o.visible_diffuse = False; o.visible_glossy = False
