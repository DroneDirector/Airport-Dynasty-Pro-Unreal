"""The trees round Chablais Airport (to-do 200; owner, 2026-11-20 night: trees where the real ones are, seasonal), modelled
in Blender our own way (owner's rule: every object made in Blender; reference: the Chablais forests in the 10 cm photo -
Norway spruce and silver fir on the slopes, beech, ash and oak on the plain and the lower slopes).

Solid geometry only (no see-through leaf cards): Nanite draws dense meshes cheaply, and nothing is masked. Each tree
stands on its origin, 20 m tall (the game scales each one to its real height from the surface model), +Z up.
  Spruce_A, Spruce_B - Norway spruce (Picea abies): a narrow cone of drooping tiers, bare trunk at the foot
  Fir_A              - silver fir (Abies alba): flatter, wider tiers, a rounder top
  Beech_A            - European beech (Fagus sylvatica) in leaf: a grey trunk, main limbs, a dome of leaf clumps
  BeechBare_A        - the same beech in winter: limbs and twigs, no leaves (the game swaps in the bare ones Dec-Mar)
Materials (one per part, filled in Unreal): M_Tree_BarkConifer, M_Tree_Needles, M_Tree_BarkBeech, M_Tree_Leaves.
  blender -b --python Art/Trees/make_trees.py   ->  Art/Trees/Trees.blend, Art/Trees/trees.glb"""
import bpy, bmesh, math, random
from mathutils import Vector, noise
from pathlib import Path

OUT = Path(__file__).resolve().parent
H = 20.0


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def mat(name, rgb):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get('Principled BSDF')
    if b: b.inputs['Base Color'].default_value = (*rgb, 1.0); b.inputs['Roughness'].default_value = 0.85
    return m


def mesh_obj(name, bm, material):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.materials.append(material)
    o = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(o)
    for p in me.polygons: p.use_smooth = True
    return o


def tapered(bm, a, b, r0, r1, seg=10, wob=0.0, seed=0):
    """A tapered tube from a to b (a branch or a trunk), capped at the base."""
    a, b = Vector(a), Vector(b); d = (b - a); L = d.length; d.normalize()
    up = Vector((0, 0, 1)) if abs(d.z) < 0.95 else Vector((1, 0, 0))
    u = d.cross(up).normalized(); v = d.cross(u).normalized()
    rings = max(2, int(L / 0.8) + 2); prev = None; first = None
    for i in range(rings):
        t = i / (rings - 1); c = a + d * (L * t); r = r0 + (r1 - r0) * t
        ring = []
        for k in range(seg):
            ang = 2 * math.pi * k / seg
            rr = r * (1 + wob * noise.noise(Vector((math.cos(ang) * 2 + seed, math.sin(ang) * 2, t * 4))))
            ring.append(bm.verts.new(c + u * math.cos(ang) * rr + v * math.sin(ang) * rr))
        if prev:
            for k in range(seg): bm.faces.new((prev[k], prev[(k + 1) % seg], ring[(k + 1) % seg], ring[k]))
        else: first = ring
        prev = ring
    bm.faces.new(list(reversed(first)))
    bm.faces.new(prev)


def conifer(name, seed, tiers, width, droop, top_round):
    """Spruce / fir: the trunk, then tiers of drooping needle skirts with ragged edges, narrowing to the top."""
    random.seed(seed)
    bark = mat('M_Tree_BarkConifer', (0.20, 0.13, 0.09)); needles = mat('M_Tree_Needles', (0.06, 0.16, 0.07))
    bm = bmesh.new(); tapered(bm, (0, 0, -0.4), (0, 0, H * 0.96), 0.30, 0.03, seg=12, wob=0.06, seed=seed)
    trunk = mesh_obj(name + '_trunk', bm, bark)
    bm = bmesh.new()
    base = H * 0.18   # (the lowest third of a forest spruce has lost its branches)
    for i in range(tiers):
        t = i / (tiers - 1)
        z = base + (H * 0.98 - base) * t
        R = width * (1 - t) ** (0.9 if not top_round else 0.75) + 0.15
        spokes = 16 + int(12 * (1 - t)); turn = random.random() * 2 * math.pi
        hub = bm.verts.new((0, 0, z + 0.25))
        rim = []
        for k in range(spokes):
            ang = turn + 2 * math.pi * (k + random.random() * 0.4) / spokes
            r = R * (0.75 + 0.4 * random.random()) * (1.0 if k % 2 == 0 else 0.78)   # (ragged: branch tips and gaps)
            dz = -droop * r * (0.8 + 0.4 * random.random())
            rim.append(bm.verts.new((math.cos(ang) * r, math.sin(ang) * r, z + dz)))
        mid = []
        for k in range(spokes):
            ang = turn + 2 * math.pi * (k + 0.5) / spokes
            r = R * 0.55
            mid.append(bm.verts.new((math.cos(ang) * r, math.sin(ang) * r, z - droop * r * 0.4 + 0.15)))
        for k in range(spokes):
            n = (k + 1) % spokes
            bm.faces.new((hub, mid[k], rim[k])); bm.faces.new((hub, rim[n], mid[k])); bm.faces.new((mid[k], rim[n], rim[k]))
        # the underside (seen from below and from the side): a shallow cone back to the trunk
        under = bm.verts.new((0, 0, z - droop * R * 0.9 - 0.3))
        for k in range(spokes): bm.faces.new((under, rim[(k + 1) % spokes], rim[k]))
    # the leader at the very top
    tip = bm.verts.new((0, 0, H)); ring = [bm.verts.new((math.cos(a) * 0.2, math.sin(a) * 0.2, H * 0.94)) for a in [2 * math.pi * k / 6 for k in range(6)]]
    for k in range(6): bm.faces.new((tip, ring[k], ring[(k + 1) % 6]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    crown = mesh_obj(name + '_crown', bm, needles)
    join(name, [trunk, crown])


def broadleaf(name, seed, bare):
    """Beech: a grey trunk forking into limbs; in leaf a dome of lumpy leaf clumps, bare: limbs and twigs only."""
    random.seed(seed)
    bark = mat('M_Tree_BarkBeech', (0.33, 0.32, 0.30)); leaves = mat('M_Tree_Leaves', (0.18, 0.32, 0.08))
    bm = bmesh.new(); tapered(bm, (0, 0, -0.4), (0, 0, H * 0.40), 0.40, 0.26, seg=12, wob=0.08, seed=seed)
    tips = []
    limbs = 6
    for i in range(limbs):
        ang = 2 * math.pi * i / limbs + random.random() * 0.6
        a = Vector((0, 0, H * (0.36 + 0.06 * random.random())))
        b = a + Vector((math.cos(ang) * H * 0.20, math.sin(ang) * H * 0.20, H * (0.24 + 0.12 * random.random())))
        tapered(bm, a, b, 0.20, 0.07, seg=8, wob=0.05, seed=seed + i)
        for j in range(3 if not bare else 5):
            a2 = a.lerp(b, 0.5 + 0.5 * random.random())
            d2 = Vector((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(0.3, 1.2))).normalized()
            b2 = a2 + d2 * H * (0.10 + 0.08 * random.random())
            tapered(bm, a2, b2, 0.07, 0.015, seg=6, seed=seed + i * 7 + j)
            tips.append(b2)
            if bare:   # (twigs: the fine winter outline)
                for k in range(3):
                    a3 = a2.lerp(b2, 0.4 + 0.6 * random.random()); b3 = a3 + Vector((random.uniform(-1, 1), random.uniform(-1, 1), random.uniform(0.2, 1))).normalized() * H * 0.05
                    tapered(bm, a3, b3, 0.02, 0.005, seg=4, seed=seed + k)
        tips.append(b)
    wood = mesh_obj(name + '_wood', bm, bark)
    parts = [wood]
    if not bare:
        bm = bmesh.new()
        # leaf clumps round every limb tip and filling the dome between them
        centres = list(tips)
        # (a full dome from about 40% of the height to the top, wider than tall, as an open-grown beech)
        for k in range(220):
            th = random.random() * 2 * math.pi; ph = math.acos(random.uniform(-0.75, 1.0)); rr = random.uniform(0.55, 1.0)
            centres.append(Vector((math.cos(th) * math.sin(ph) * H * 0.34 * rr, math.sin(th) * math.sin(ph) * H * 0.34 * rr, H * 0.63 + math.cos(ph) * H * 0.31 * rr)))
        # many small, rough leaf clusters (not a few smooth balls): each a lumpy, flattened cluster turned at random,
        # its surface broken by two scales of noise - the crown reads as foliage, its outline ragged against the sky
        for c in centres:
            r = H * (0.032 + 0.026 * random.random())
            ret = bmesh.ops.create_icosphere(bm, subdivisions=2, radius=r)
            rot = Vector((random.random(), random.random(), random.random())) * 6.28
            for v in ret['verts']:
                p = v.co.copy(); p.rotate(__import__('mathutils').Euler(rot))
                n1 = noise.noise(p * (3.0 / r) + Vector((c.x, c.y, c.z)) * 0.37)
                n2 = noise.noise(p * (9.0 / r) + Vector((c.y, c.z, c.x)) * 0.71)
                v.co = c + p * (1 + 0.45 * n1 + 0.22 * n2) * Vector((1, 1, 0.72))
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        parts.append(mesh_obj(name + '_leaves', bm, leaves))
    join(name, parts)


def join(name, objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    objs[0].name = name; objs[0].data.name = name
    # no empty material slots (material-slot-check rule)
    assert all(s.material for s in objs[0].material_slots), name + ': empty material slot'


def main():
    clear()
    conifer('Spruce_A', 11, tiers=38, width=3.4, droop=0.75, top_round=False)
    conifer('Spruce_B', 23, tiers=34, width=3.9, droop=0.85, top_round=False)
    conifer('Fir_A', 37, tiers=28, width=4.3, droop=0.45, top_round=True)
    broadleaf('Beech_A', 51, bare=False)
    broadleaf('BeechBare_A', 51, bare=True)
    # side by side for a look in the .blend (each is placed by the game on its own)
    for i, o in enumerate(bpy.data.objects): o.location.x = i * 14.0
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'Trees.blend'))
    # (exported side by side too: each is its own mesh at its own origin in the game - the flicker check sees them apart)
    bpy.ops.export_scene.gltf(filepath=str(OUT / 'trees.glb'), export_format='GLB', export_apply=True)
    for o in bpy.data.objects: print('TREE', o.name, len(o.data.polygons), 'faces')


main()
