# Shared builder for the hangar access equipment (to-do 364 part A, batch 1), our own models from reference photos and
# published sizes, no brands. Every model script calls reset(), builds parts with B(), part(), ik(), then finish().
# Conventions: Blender z up, metres, +y = the side that faces the aircraft, origin on the floor under the middle.
# Every moving part is its own object with its origin on its pivot (hinge, axle, swivel, slide start); its "move" text
# says the axis and range. IK points are empties named IK_<what> (hands on handles and rails, feet on steps and decks).
import bpy, bmesh, math, os, json
from mathutils import Vector, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
TEX = os.path.join(ROOT, 'Art', 'GPU', 'Textures')
PARTS, IKS, MATS, CONTROLS = [], [], {}, []
OBJ = {}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    PARTS.clear(); IKS.clear(); MATS.clear(); OBJ.clear(); CONTROLS.clear()


def _srgb(c): return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def _gen_textures():
    """Our own 1 m tiles (1 px = 1 mm), made once, same seed every time: paint with fine grain, faint dirt and light wear
    (small chips down to the primer, more of them low down), and plain yellow/black 45 deg warning stripes (125 mm)."""
    import numpy as np
    d = os.path.join(HERE, 'Textures'); os.makedirs(d, exist_ok=True)
    if os.path.exists(os.path.join(d, 'HQ_WarningStripes_Color.jpg')): return d
    class Image:
        @staticmethod
        def fromarray(a):
            class W:
                def save(self, path, quality=92):
                    im = bpy.data.images.new('tmp', a.shape[1], a.shape[0])
                    rgba = np.concatenate([a[::-1].astype('float32') / 255, np.ones(a.shape[:2] + (1,), 'float32')], 2)
                    im.pixels.foreach_set(rgba.ravel()); im.filepath_raw = path; im.file_format = 'JPEG'
                    bpy.context.scene.render.image_settings.quality = quality; im.save(); bpy.data.images.remove(im)
            return W()
    N = 1024; rng = np.random.default_rng(364)
    def smooth(a, k):
        for _ in range(k): a = (a + np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 5
        return a
    grain = smooth(rng.standard_normal((N, N)), 2); grain /= grain.std()
    dirt = smooth(rng.standard_normal((N, N)), 60); dirt = (dirt - dirt.min()) / (dirt.max() - dirt.min())
    chip = smooth(rng.standard_normal((N, N)), 3); chip = chip > np.quantile(chip, 0.9965)
    chip = smooth(chip.astype(float), 1) > 0.35
    def paint(fname, lin, primer, gk=0.025):
        c = np.array([_srgb(x) for x in lin])[None, None, :] * (1 + gk * grain[..., None]) * (1 - 0.06 * dirt[..., None])
        c = np.where(chip[..., None], np.array(primer)[None, None, :], c)
        Image.fromarray((np.clip(c, 0, 1) * 255).astype('uint8')).save(os.path.join(d, fname), quality=92)
    paint('HQ_PaintYellow_Color.jpg', (0.86, 0.56, 0.015), (0.42, 0.42, 0.40))
    paint('HQ_PaintGrey_Color.jpg', (0.17, 0.18, 0.19), (0.50, 0.50, 0.50))
    paint('HQ_PaintRed_Color.jpg', (0.55, 0.025, 0.02), (0.40, 0.40, 0.40))
    y, x = np.mgrid[0:N, 0:N]; band = ((x + y) // 128) % 2 == 0
    yl = np.array([_srgb(v) for v in (0.86, 0.56, 0.015)]); bk = np.array([0.05, 0.05, 0.055])
    c = np.where(band[..., None], yl, bk) * (1 + 0.02 * grain[..., None]) * (1 - 0.05 * dirt[..., None])
    Image.fromarray((np.clip(c, 0, 1) * 255).astype('uint8')).save(os.path.join(d, 'HQ_WarningStripes_Color.jpg'), quality=92)
    return d


def mat(key, name, col, rough, metal=0.0, tex=None, color_tex=False, img=None):
    """A lifelike material: plain colour (paint keeps its exact shade) with the texture's fine normal grain; galvanised
    steel, tread plate and rubber also take the texture's colour. Every image node gets its picture (no empty param)."""
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    if tex:
        d = os.path.join(TEX, tex)
        if img:
            n = nt.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(img)
            nt.links.new(n.outputs[0], b.inputs['Base Color'])
        elif color_tex:
            n = nt.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(os.path.join(d, '%s_2K-JPG_Color.jpg' % tex))
            nt.links.new(n.outputs[0], b.inputs['Base Color'])
        n = nt.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(os.path.join(d, '%s_2K-JPG_NormalGL.jpg' % tex))
        n.image.colorspace_settings.name = 'Non-Color'
        nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.6 if color_tex else 0.3
        nt.links.new(n.outputs[0], nm.inputs['Color']); nt.links.new(nm.outputs[0], b.inputs['Normal'])
    MATS[key] = m
    return m


def materials(prefix):
    """The equipment palette: industrial yellow, machine grey, light grey, galvanised, tread plate, chrome, rubber..."""
    d = _gen_textures()
    mat('Y', prefix + '_PaintYellow', (0.86, 0.56, 0.015), 0.42, 0.0, 'PaintedMetal016', img=os.path.join(d, 'HQ_PaintYellow_Color.jpg'))
    mat('G', prefix + '_PaintGrey', (0.17, 0.18, 0.19), 0.5, 0.0, 'Paint004', img=os.path.join(d, 'HQ_PaintGrey_Color.jpg'))
    mat('WS', prefix + '_WarningStripes', (0.86, 0.56, 0.015), 0.45, 0.0, 'PaintedMetal016', img=os.path.join(d, 'HQ_WarningStripes_Color.jpg'))
    mat('LG', prefix + '_PaintLightGrey', (0.55, 0.57, 0.58), 0.45, 0.0, 'Paint004')
    mat('ST', prefix + '_Galvanised', (0.55, 0.57, 0.59), 0.42, 1.0, 'Metal011', True)
    mat('PL', prefix + '_TreadPlate', (0.6, 0.6, 0.6), 0.38, 1.0, 'DiamondPlate008A', True)
    mat('CR', prefix + '_Chrome', (0.80, 0.80, 0.82), 0.10, 1.0, 'Metal027')
    mat('RB', prefix + '_Rubber', (0.03, 0.03, 0.032), 0.86, 0.0, 'Rubber004', True)
    mat('BK', prefix + '_BlackPlastic', (0.025, 0.025, 0.027), 0.55, 0.0, 'Plastic006')
    mat('RD', prefix + '_PaintRed', (0.55, 0.025, 0.02), 0.4, 0.0, 'PaintedMetal016', img=os.path.join(d, 'HQ_PaintRed_Color.jpg'))
    mat('OR', prefix + '_GuardOrange', (0.85, 0.22, 0.02), 0.5, 0.0, 'Plastic013A')


def _uv_and_shade(bm, scale=1.0):
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        a, c = [(1, 2), (0, 2), (0, 1)][ax]
        for l in f.loops: l[uv].uv = (l.vert.co[a] * scale, l.vert.co[c] * scale)
        f.smooth = True


class B:
    """Geometry of one part, in world coordinates; mesh(pivot) moves it so the pivot is its origin."""
    def __init__(self): self.bm, self.ms = bmesh.new(), []

    def _take(self, o, m, bevel=0.0):
        if bevel > 0:
            es = [e for e in o.edges if len(e.link_faces) == 2 and e.calc_face_angle(0) > 0.5]
            bmesh.ops.bevel(o, geom=es, offset=bevel, segments=2, profile=0.5, affect='EDGES', clamp_overlap=True)
        bmesh.ops.recalc_face_normals(o, faces=o.faces)
        if m not in self.ms: self.ms.append(m)
        k = self.ms.index(m)
        for f in o.faces: f.material_index = k
        me = bpy.data.meshes.new('t'); o.to_mesh(me); o.free(); self.bm.from_mesh(me); bpy.data.meshes.remove(me)

    def box(self, a, b, m, bevel=0.003):
        o = bmesh.new(); r = bmesh.ops.create_cube(o, size=1.0)
        lo, hi = Vector(a), Vector(b)
        for v in o.verts: v.co = Vector(((lo.x + hi.x) / 2 + v.co.x * abs(hi.x - lo.x), (lo.y + hi.y) / 2 + v.co.y * abs(hi.y - lo.y), (lo.z + hi.z) / 2 + v.co.z * abs(hi.z - lo.z)))
        self._take(o, MATS[m], min(bevel, min(abs(hi[i] - lo[i]) for i in range(3)) * 0.3))

    def bar(self, p0, p1, w, d, m, bevel=0.003, up=(0, 0, 1)):
        """A rectangular bar (square tube, flat bar) from p0 to p1, w wide, d deep."""
        p0, p1 = Vector(p0), Vector(p1); ax = p1 - p0; L = ax.length; z = ax.normalized()
        u = Vector(up); u = u if abs(u.dot(z)) < 0.95 else Vector((1, 0, 0))
        x = u.cross(z).normalized(); y = z.cross(x)
        o = bmesh.new(); bmesh.ops.create_cube(o, size=1.0)
        for v in o.verts:
            c = v.co.copy(); v.co = p0 + x * c.x * w + y * c.y * d + z * (c.z + 0.5) * L
        self._take(o, MATS[m], min(bevel, w * 0.3, d * 0.3))

    def tube(self, p0, p1, r, m, n=14, weld=None):
        """Round tube; painted frame tubes (r >= 15 mm) get a fillet weld bead at both ends where they meet the frame."""
        p0, p1 = Vector(p0), Vector(p1); ax = p1 - p0; L = ax.length
        if weld is None: weld = m in ('Y', 'G', 'RD') and r >= 0.015 and L > 0.08
        if weld:
            z = ax.normalized(); q = z.to_track_quat('Z', 'Y').to_matrix().to_4x4(); bw, bh = 0.0045, 0.004
            for c, sg in ((p0, 1), (p1, -1)):
                prof = [(r - 0.0005, -bw), (r + bh * 0.6, -bw * 0.6), (r + bh, 0.0), (r + bh * 0.6, bw * 0.6), (r - 0.0005, bw)]
                o = bmesh.new(); rings = []
                for rr, h in prof:
                    rings.append([o.verts.new(Vector((rr * math.cos(2 * math.pi * k / n), rr * math.sin(2 * math.pi * k / n), h))) for k in range(n)])
                for i in range(len(rings) - 1):
                    for k in range(n): o.faces.new((rings[i][k], rings[i][(k + 1) % n], rings[i + 1][(k + 1) % n], rings[i + 1][k]))
                bmesh.ops.transform(o, matrix=Matrix.Translation(c + z * sg * (bw + 0.001)) @ q, verts=o.verts)
                self._take(o, MATS[m])
        o = bmesh.new(); bmesh.ops.create_cone(o, cap_ends=True, segments=n, radius1=r, radius2=r, depth=L)
        rot = ax.normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
        bmesh.ops.transform(o, matrix=Matrix.Translation((p0 + p1) / 2) @ rot, verts=o.verts)
        self._take(o, MATS[m])

    def lathe(self, prof, m, centre=(0, 0, 0), axis='z', n=24):
        """Profile [(r, h)] turned about an axis through centre ('x' for wheels, 'z' for columns)."""
        o = bmesh.new(); rings = []
        for r, h in prof:
            ring = []
            for k in range(n):
                a = 2 * math.pi * k / n; c, s = r * math.cos(a), r * math.sin(a)
                p = {'z': (c, s, h), 'x': (h, c, s), 'y': (c, h, s)}[axis]
                ring.append(o.verts.new(Vector(centre) + Vector(p)))
            rings.append(ring)
        for i in range(len(rings) - 1):
            for k in range(n):
                o.faces.new((rings[i][k], rings[i][(k + 1) % n], rings[i + 1][(k + 1) % n], rings[i + 1][k]))
        if prof[0][0] > 1e-4: o.faces.new(list(reversed(rings[0])))
        if prof[-1][0] > 1e-4: o.faces.new(rings[-1])
        self._take(o, MATS[m])

    def mesh(self, name, pivot):
        bmesh.ops.translate(self.bm, vec=-Vector(pivot), verts=self.bm.verts)
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=1e-6)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        _uv_and_shade(self.bm)
        me = bpy.data.meshes.new(name); self.bm.to_mesh(me); self.bm.free()
        for m in self.ms: me.materials.append(m)
        me.use_auto_smooth = True; me.auto_smooth_angle = math.radians(35)
        return me


def part(name, geo, pivot=(0, 0, 0), parent=None, move='fixed', rot_z=0.0, rot_x=0.0):
    """geo: a B (made into this part's own mesh) or an existing mesh (an instance of it, drawn once by the game)."""
    me = geo.mesh(name, pivot) if isinstance(geo, B) else geo
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o)
    pw = Vector(PARTS[[p['name'] for p in PARTS].index(parent)]['pivot']) if parent else Vector()
    o.location = Vector(pivot) - pw; o.rotation_euler = (rot_x, 0, rot_z)
    if parent: o.parent = OBJ[parent]
    OBJ[name] = o
    PARTS.append({'name': name, 'mesh': me.name, 'parent': parent, 'pivot': [round(c, 3) for c in pivot], 'move': move, **({'rot_x_deg': round(math.degrees(rot_x), 2)} if rot_x else {})})
    return o


def ik(name, p, kind, parent=None):
    """An IK point (empty) at world position p: kind 'hand' / 'foot' / 'stand' / 'push' (a push-handle grip)."""
    e = bpy.data.objects.new('IK_' + name, None); e.empty_display_type = 'ARROWS'; e.empty_display_size = 0.06
    bpy.context.scene.collection.objects.link(e)
    pw = Vector(PARTS[[q['name'] for q in PARTS].index(parent)]['pivot']) if parent else Vector()
    e.location = Vector(p) - pw
    if parent: e.parent = OBJ[parent]
    IKS.append({'name': 'IK_' + name, 'kind': kind, 'parent': parent, 'pos': [round(c, 3) for c in p]})


def ctrl(name, part_name, ik_name, does, by='hand'):
    """A control the mechanic or the player works (E on it): its part (own pivot) and its IK point."""
    assert part_name in OBJ and any(i['name'] == 'IK_' + ik_name for i in IKS), name
    CONTROLS.append({'control': name, 'part': part_name, 'ik': 'IK_' + ik_name, 'by': by, 'does': does})


# ---- shared pieces: a braked swivel castor (swivel, wheel and brake pedal each their own part) ----
_CASTOR = {}


def castor(tag, x, y, root, r=0.10, w=0.05, top=None, fixed=False):
    """A castor whose mounting plate's top is at height top (default 2r + 0.07). Parts: Castor_<tag> (swivel about z,
    fork + plate), Wheel_<tag> (spins about x at the axle), Brake_<tag> (foot pedal, tips about x 0..20 deg)."""
    key = (r, w, fixed)
    h = top if top is not None else 2 * r + 0.07
    off = 0.0 if fixed else r * 0.45            # trailing offset of the axle behind the swivel
    if key not in _CASTOR:
        f = B()
        f.box((-w / 2 - 0.03, -0.07, h - 0.012), (w / 2 + 0.03, 0.07, h), 'ST', 0.003)            # plate
        f.lathe([(0.035, h - 0.04), (0.035, h - 0.013)], 'ST', (0, 0, 0), 'z', 16)               # swivel bearing
        for s in (-1, 1):
            f.bar((s * (w / 2 + 0.012), 0, h - 0.035), (s * (w / 2 + 0.012), -off, r), 0.05, 0.01, 'ST', 0.002, up=(1, 0, 0))
        f.tube((-w / 2 - 0.02, -off, r), (w / 2 + 0.02, -off, r), 0.011, 'CR', 10)                # axle
        fm = f.mesh('Castor_Fork_%d' % len(_CASTOR), (0, 0, 0))
        wh = B()
        wh.lathe([(0.0, -w / 2 + 0.004), (r * 0.62, -w / 2 + 0.004), (r * 0.62, -w / 2), (r - 0.012, -w / 2), (r, -w / 2 + 0.012),
                  (r, w / 2 - 0.012), (r - 0.012, w / 2), (r * 0.62, w / 2), (r * 0.62, w / 2 - 0.004), (0.0, w / 2 - 0.004)], 'RB', (0, 0, 0), 'x', 28)
        wh.lathe([(0.0, -w / 2 - 0.002), (r * 0.6, -w / 2 - 0.002), (r * 0.6, w / 2 + 0.002), (0.0, w / 2 + 0.002)], 'G', (0, 0, 0), 'x', 20)
        wm = wh.mesh('Castor_Wheel_%d' % len(_CASTOR), (0, 0, 0))
        bk = B()
        bk.bar((w / 2 + 0.03, 0.0, 0.0), (w / 2 + 0.03, 0.09, 0.0), 0.012, 0.035, 'ST', 0.002)
        bk.box((w / 2 + 0.012, 0.07, -0.006), (w / 2 + 0.07, 0.115, 0.012), 'RD', 0.003)            # pedal
        bm_ = bk.mesh('Castor_Brake_%d' % len(_CASTOR), (0, 0, 0))
        _CASTOR[key] = (fm, wm, bm_, off, h)
    fm, wm, bm_, off, h = _CASTOR[key]
    part('Castor_' + tag, fm, (x, y, 0), root, 'fixed axle' if fixed else 'swivel about z, free 360 deg')
    part('Wheel_' + tag, wm, (x, y - off, r), 'Castor_' + tag, 'spin about x, rolls r=%.3f m' % r)
    part('Brake_' + tag, bm_, (x, y - off, r), 'Castor_' + tag, 'pedal tips about x 0..20 deg: down = wheel locked')
    ik('Brake_' + tag, (x + w / 2 + 0.04, y - off + 0.09, r + 0.012), 'foot', 'Brake_' + tag)
    ctrl('Castor brake ' + tag, 'Brake_' + tag, 'Brake_' + tag, 'step down = locked, flip up = free', 'foot')


def finish(name, kind, size_note, footprint, ref):
    """Checks (no empty material slot, sizes), saves Art/HangarEquipment/<Name>/<Name>.blend, the .glb and the parts list."""
    d = os.path.join(HERE, name); os.makedirs(d, exist_ok=True)
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    for o in meshes:
        assert o.data.materials and all(o.data.materials), 'empty material slot on ' + o.name
        assert all(p.material_index < len(o.data.materials) for p in o.data.polygons), o.name
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    lo = [round(min(p[i] for p in pts), 3) for i in range(3)]; hi = [round(max(p[i] for p in pts), 3) for i in range(3)]
    info = {'asset': name, 'kind': kind, 'blend': 'Art/HangarEquipment/%s/%s.blend' % (name, name),
            'glb': 'Art/HangarEquipment/%s/%s.glb' % (name, name.lower()), 'sizes': size_note,
            'bounds_min': lo, 'bounds_max': hi, 'size_m': [round(hi[i] - lo[i], 3) for i in range(3)],
            'footprint': footprint, 'reference': ref,
            'tris': sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in meshes),
            'parts': PARTS, 'ik': IKS, 'controls': CONTROLS}
    json.dump(info, open(os.path.join(d, '%s-parts.json' % name.lower()), 'w'), indent=1)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(d, name + '.blend'))
    bpy.ops.export_scene.gltf(filepath=os.path.join(d, name.lower() + '.glb'), export_format='GLB', export_apply=True, export_yup=True)
    print('DONE', name, info['size_m'], len(PARTS), 'parts', len(IKS), 'IK', info['tris'], 'tris')
