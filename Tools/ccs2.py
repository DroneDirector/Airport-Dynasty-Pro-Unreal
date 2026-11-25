# The CCS2 coupling (IEC 62196-3 configuration FF) for both sides: the charger's plug (Art/BusCharger/make_buscharger.py) and the
# bus's inlet (Tools/apronbus_inlet.py), so that they really fit. Frame: origin on the coupling plane (the plug's flange on the
# inlet's face), half way between the AC part's centre and the DC part's; x = the way the plug goes in, y left, z up; metres.
import math
import bmesh
import vehicle_kit as K
ZA, ZD = 0.0245, -0.0245                    # centre of the AC part (earth pin) and of the DC pins
RA, FLAT = 0.0268, 0.0200                   # the AC part's radius and its flat top above its centre
DW, DH = 0.0135, 0.0158                     # DC: half the pins' distance, the oval's radius
NOSE, CLEAR = 0.0355, 0.0012                # how far the plug's nose goes in; room round it
PINS = [('CP', 0.0081, 0.0111, 0.0025), ('PP', -0.0081, 0.0111, 0.0025), ('PE', 0.0, 0.0, 0.0030), ('N', 0.0161, 0.0, 0.0030), ('L1', -0.0161, 0.0, 0.0030),
        ('L3', 0.0081, -0.0141, 0.0030), ('L2', -0.0081, -0.0141, 0.0030)]
DCP = [(DW, 0.0040), (-DW, 0.0040)]


def ac(g=0.0, n=40):
    """The AC part's outline in (y, z), grown by g: a circle with a flat top."""
    r, a0 = RA + g, math.asin((FLAT + g) / (RA + g))
    return [(r * math.cos(a), ZA + r * math.sin(a)) for a in (math.pi - a0 + (2 * a0 + math.pi) * i / n for i in range(n + 1))]


def dc(g=0.0, n=14):
    r = DH + g
    return [(DW + r * math.cos(a), ZD + r * math.sin(a)) for a in (-math.pi / 2 + math.pi * i / n for i in range(n + 1))] + \
           [(-DW + r * math.cos(a), ZD + r * math.sin(a)) for a in (math.pi / 2 + math.pi * i / n for i in range(n + 1))]


def extr(name, yz, x0, x1, m, bevel=0.0):
    """An outline (y, z) pushed along x."""
    bm = bmesh.new()
    a = [bm.verts.new((x0, y, z)) for y, z in yz]; b = [bm.verts.new((x1, y, z)) for y, z in yz]
    bm.faces.new(a); bm.faces.new(b[::-1]); n = len(yz)
    for i in range(n): bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    o = K.link(name, bm)
    if bevel: md = o.modifiers.new('b', 'BEVEL'); md.width = bevel; md.segments = 2; md.limit_method = 'ANGLE'; md.angle_limit = math.radians(50); K.apply_mods(o)
    return K.finish(o, m, 35)


def join(name, objs, mat=None):
    """One part of several (its materials kept as slots), moved by the matrix mat."""
    import bpy
    for x in bpy.context.selected_objects: x.select_set(False)
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1: bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active; o.select_set(False)
    for p in objs[1:]: K.ALL.remove(p)
    if mat is not None: o.data.transform(mat); o.data.update()
    o.name = name; o.data.name = name
    return o


def inlet(m_body, m_pin, back=0.041, w=0.088, h=0.128):
    """The vehicle's side: the parts, in the coupling frame (their face at x = 0, into the vehicle +x)."""
    f = K.box('Inl_Block', (back / 2, 0, -0.002), (back, w, h), m_body, bevel=0.004)
    K.cut(f, extr('c', ac(CLEAR), -0.02, NOSE + 0.0015, m_body)); K.cut(f, extr('c', dc(CLEAR), -0.02, NOSE + 0.0015, m_body))
    out = [f]
    for n, y, z, r in PINS: out.append(K.cyl('Inl_' + n, (0.0225, y, ZA + z), r, 0.030, 'x', m_pin, 20, bevel=0.0008))
    for i, (y, r) in enumerate(DCP): out.append(K.cyl('Inl_D%d' % i, (0.0215, y, ZD), r, 0.032, 'x', m_pin, 24, bevel=0.001))
    return out


def nose(m_ins, m_pin):
    """The plug's two grey insulators with their holes, and the contacts deep in them."""
    out = []
    a = extr('Nose_A', ac(0), -0.004, NOSE, m_ins, bevel=0.0012); d = extr('Nose_D', dc(0), -0.004, NOSE, m_ins, bevel=0.0012)
    for n, y, z, r in PINS: K.cut(a, K.cyl('c', (NOSE, y, ZA + z), r + 0.0010, 0.064, 'x', m_ins, 20))
    for i, (y, r) in enumerate(DCP): K.cut(d, K.cyl('c', (NOSE, y, ZD), r + 0.0020, 0.064, 'x', m_ins, 24))
    return [a, d]
