# The cargo terminal's sectional (garage) door leaves (step B3, owner: "garage doors look like garage doors"), built in
# Blender for ARollerDoor's sectional mode. For every door size in cargo_doors.rollers() three meshes, each made to the
# door's width (panel = opening - GAP, its ends run in the guide channels the building model puts in each reveal):
#   SM_CargoLeaf_Panel_<w>x<h>   one 42 mm insulated steel panel: chamfered tongue joints, two pressed ribs each face
#   SM_CargoLeaf_Window_<w>x<h>  the same panel with a row of vision windows (black gaskets, glass) - put at eye height
#   SM_CargoLeaf_Rail_<w>x<h>    the bottom rail: rubber seal under the lowest panel, aluminium rail plates, lift handles
# Units metres. x along the door (centred), y across (0 = the curtain's plane), z up; a panel lies from SEAL to
# SEAL + pitch - 3 mm (the visible joint), the rail from 0 up. The game stacks the panels 'pitch' apart from the rail.
#   python Tools/import_model.py Art/CargoTerminal/make_cargo_door_leaves.py Art/CargoTerminal/CargoDoorLeaves.glb CargoDoorLeaves
import bpy, bmesh, os, sys
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import cargo_doors as CD

bpy.ops.wm.read_factory_settings(use_empty=True)
SC = bpy.context.scene
COL = bpy.data.collections.new('CargoDoorLeaves'); SC.collection.children.link(COL)
TEX = os.path.join(HERE, 'Textures')
T2 = 0.021                 # half the panel's thickness (42 mm)
CH = 0.006                 # chamfer at the panel joints
JOINT = 0.003              # the gap between two panels


def mat(name, rgb, rough=0.5, metal=0.0, alpha=1.0, tex=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    if alpha < 1:
        b.inputs['Alpha'].default_value = alpha; m.blend_method = 'BLEND'; m.shadow_method = 'HASHED'
        if 'Specular' in b.inputs: b.inputs['Specular'].default_value = 0.9
    if tex:
        ci = nt.nodes.new('ShaderNodeTexImage'); ci.image = bpy.data.images.load(os.path.join(TEX, tex + '_Color.jpg'), check_existing=True)
        nt.links.new(ci.outputs['Color'], b.inputs['Base Color'])
        ni = nt.nodes.new('ShaderNodeTexImage'); ni.image = bpy.data.images.load(os.path.join(TEX, tex + '_Normal.jpg'), check_existing=True)
        ni.image.colorspace_settings.name = 'Non-Color'
        nm = nt.nodes.new('ShaderNodeNormalMap'); nt.links.new(ni.outputs['Color'], nm.inputs['Color']); nt.links.new(nm.outputs['Normal'], b.inputs['Normal'])
        oi = nt.nodes.new('ShaderNodeTexImage'); oi.image = bpy.data.images.load(os.path.join(TEX, tex + '_ORM.jpg'), check_existing=True)
        oi.image.colorspace_settings.name = 'Non-Color'
        sp = nt.nodes.new('ShaderNodeSeparateColor' if hasattr(bpy.types, 'ShaderNodeSeparateColor') else 'ShaderNodeSeparateRGB')
        nt.links.new(oi.outputs['Color'], sp.inputs[0])
        nt.links.new(sp.outputs[1], b.inputs['Roughness']); nt.links.new(sp.outputs[2], b.inputs['Metallic'])
    m.diffuse_color = (*rgb, alpha)
    return m


M = dict(panel=mat('M_CargoLeaf_Panel', (0.92, 0.50, 0.06), tex='SectionalDoor'),
         gasket=mat('M_CargoLeaf_Gasket', (0.03, 0.03, 0.03), 0.7, tex='Rubber'),
         glass=mat('M_CargoLeaf_Glass', (0.12, 0.15, 0.17), 0.03, 0.0, 0.28),
         rail=mat('M_CargoLeaf_Rail', (0.70, 0.72, 0.74), 0.35, 1.0, tex='Aluminium'),
         seal=mat('M_CargoLeaf_Seal', (0.03, 0.03, 0.03), 0.8, tex='Rubber'),
         steel=mat('M_CargoLeaf_Steel', (0.55, 0.57, 0.59), 0.4, 1.0, tex='Galvanised'))
TILE = {'M_CargoLeaf_Gasket': 1.0, 'M_CargoLeaf_Glass': 1.0, 'M_CargoLeaf_Rail': 1.0, 'M_CargoLeaf_Seal': 1.0, 'M_CargoLeaf_Steel': 1.0}


def add_box(bm, x0, y0, z0, x1, y1, z1, mi, bevel=0.0):
    r = bmesh.ops.create_cube(bm, size=1.0)
    vs = r['verts']
    for v in vs:
        v.co = Vector(((v.co.x + 0.5) * (x1 - x0) + x0, (v.co.y + 0.5) * (y1 - y0) + y0, (v.co.z + 0.5) * (z1 - z0) + z0))
    fs = list({f for v in vs for f in v.link_faces})
    for f in fs: f.material_index = mi
    if bevel > 0:
        es = list({e for v in vs for e in v.link_edges})
        bmesh.ops.bevel(bm, geom=es, offset=min(bevel, 0.3 * min(x1 - x0, y1 - y0, z1 - z0)), segments=1, affect='EDGES')
    return fs


def profile(p, ribs=True):
    """The panel's section in (y, z): chamfered joint edges; two shallow pressed grooves on each face."""
    a, b = CD.SEAL, CD.SEAL + p - JOINT
    out = [(T2, a + CH)]
    zs = [a + p / 3, a + 2 * p / 3] if ribs else []
    for zr in zs:
        out += [(T2, zr - 0.016), (T2 - 0.004, zr - 0.007), (T2 - 0.004, zr + 0.007), (T2, zr + 0.016)]
    out += [(T2, b - CH), (T2 - CH, b), (-T2 + CH, b), (-T2, b - CH)]
    for zr in reversed(zs):
        out += [(-T2, zr + 0.016), (-T2 + 0.004, zr + 0.007), (-T2 + 0.004, zr - 0.007), (-T2, zr - 0.016)]
    out += [(-T2, a + CH), (-T2 + CH, a), (T2 - CH, a)]
    return out


def prism(bm, hw, pts, mi):
    """The section extruded along x from -hw to hw, both ends capped."""
    l = [bm.verts.new((-hw, y, z)) for y, z in pts]
    r = [bm.verts.new((hw, y, z)) for y, z in pts]
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((l[i], l[j], r[j], r[i])).material_index = mi
    bm.faces.new(list(reversed(l))).material_index = mi
    bm.faces.new(r).material_index = mi


def obj(name, bm):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    for k in ('panel', 'gasket', 'glass', 'rail', 'seal', 'steel'): me.materials.append(M[k])
    o = bpy.data.objects.new(name, me); COL.objects.link(o)
    return o


def uvs(o, p):
    """Panel faces: the door texture's 0.6 m section fitted to the panel (its joint line on the panel's joint), u along
    the door in metres / 2.4; every other part box-projected 1 m to a tile."""
    me = o.data; bm = bmesh.new(); bm.from_mesh(me); uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal; ax = max(range(3), key=lambda k: abs(n[k]))
        for lp in f.loops:
            c = lp.vert.co
            if f.material_index == 0:
                u = c.y if ax == 0 else c.x
                lp[uv].uv = (u / 2.4, (c.z - CD.SEAL) / p * 0.25 if ax != 2 else c.y / 2.4)
            else:
                u, v = (c.x, c.y) if ax == 2 else ((c.x, c.z) if ax == 1 else (c.y, c.z))
                lp[uv].uv = (u, v)
        f.smooth = False
    bm.to_mesh(me); bm.free()


def boolean_cut(o, boxes):
    """Cuts the window openings (exact boolean, applied)."""
    bm = bmesh.new()
    for b in boxes: add_box(bm, *b, 0)
    cme = bpy.data.meshes.new('cut'); bm.to_mesh(cme); bm.free(); cme.materials.append(M['panel'])   # the reveals: panel steel
    c = bpy.data.objects.new('cut', cme); COL.objects.link(c)
    md = o.modifiers.new('cut', 'BOOLEAN'); md.operation = 'DIFFERENCE'; md.object = c; md.solver = 'EXACT'
    bpy.context.view_layer.objects.active = o
    with bpy.context.temp_override(object=o, active_object=o):
        bpy.ops.object.modifier_apply(modifier=md.name)
    bpy.data.objects.remove(c); bpy.data.meshes.remove(cme)
    for f in o.data.polygons:                          # the reveals the cut made: panel steel, no slot of their own
        if f.material_index >= len(M): f.material_index = 0
    while len(o.data.materials) > len(M): o.data.materials.pop()


made = []
for w, h in CD.leaf_sizes():
    n, p, tag = CD.leaf(w, h)
    hw = (w - CD.GAP) / 2
    a, b = CD.SEAL, CD.SEAL + p - JOINT
    # 1. the plain panel
    bm = bmesh.new(); prism(bm, hw, profile(p), 0)
    o = obj('SM_CargoLeaf_Panel_' + tag, bm); uvs(o, p); made.append(o)
    # 2. the window panel: a smooth-faced section, a row of 610 x ~300 mm windows (one per metre or so)
    bm = bmesh.new(); prism(bm, hw, profile(p, ribs=False), 0)
    o = obj('SM_CargoLeaf_Window_' + tag, bm)
    nw = max(2, int(w / 1.05)); ww = 0.61; wz0 = a + 0.13; wz1 = b - 0.13
    xs = [-hw + 2 * hw * (k + 0.5) / nw for k in range(nw)]
    boolean_cut(o, [(x - ww / 2, -0.1, wz0, x + ww / 2, 0.1, wz1) for x in xs])
    bm = bmesh.new(); bm.from_mesh(o.data)
    g = 0.022                                                       # the gasket's lip round the opening
    for x in xs:
        x0, x1 = x - ww / 2, x + ww / 2
        yy = (-T2 - 0.005, T2 + 0.005)
        add_box(bm, x0 - g, yy[0], wz0 - g, x0 + 0.012, yy[1], wz1 + g, 1, 0.003)          # left and right, full height
        add_box(bm, x1 - 0.012, yy[0], wz0 - g, x1 + g, yy[1], wz1 + g, 1, 0.003)
        add_box(bm, x0 + 0.012, yy[0], wz0 - g, x1 - 0.012, yy[1], wz0 + 0.012, 1, 0.003)  # bottom and top, between them
        add_box(bm, x0 + 0.012, yy[0], wz1 - 0.012, x1 - 0.012, yy[1], wz1 + g, 1, 0.003)
        add_box(bm, x0 + 0.006, -0.003, wz0 + 0.006, x1 - 0.006, 0.003, wz1 - 0.006, 2)   # the pane, its edge in the gasket
    bm.to_mesh(o.data); bm.free(); uvs(o, p); made.append(o)
    # 3. the bottom rail: rubber seal (under the panel), rail plates on both faces (1 mm off them, clear of the guides),
    #    a lift handle on each face
    bm = bmesh.new()
    add_box(bm, -hw + 0.001, -0.012, 0.0, hw - 0.001, 0.012, CD.SEAL, 4, 0.004)
    rx = hw - 0.065
    for s in (-1, 1):
        y0, y1 = sorted((s * (T2 + 0.001), s * (T2 + 0.006)))
        add_box(bm, -rx, y0, 0.012, rx, y1, 0.075, 3, 0.0015)
        y0, y1 = sorted((s * (T2 + 0.001), s * (T2 + 0.005)))
        add_box(bm, -0.09, y0, 0.17, 0.09, y1, 0.31, 5, 0.0015)                 # mount plate
        for xo in (-0.06, 0.06):                                               # stand-offs
            y0, y1 = sorted((s * (T2 + 0.005), s * (T2 + 0.040)))
            add_box(bm, xo - 0.011, y0, 0.229, xo + 0.011, y1, 0.251, 5, 0.003)
        y0, y1 = sorted((s * (T2 + 0.036), s * (T2 + 0.058)))
        add_box(bm, -0.08, y0, 0.226, 0.08, y1, 0.254, 5, 0.006)               # the grip
    o = obj('SM_CargoLeaf_Rail_' + tag, bm); uvs(o, p); made.append(o)

# every slot filled (the import's rule): slots nobody uses are dropped, none left empty
for o in made:
    used = {f.material_index for f in o.data.polygons}
    keep = sorted(used); remap = {old: i for i, old in enumerate(keep)}
    mats = [o.data.materials[i] for i in keep]
    for f in o.data.polygons: f.material_index = remap[f.material_index]
    o.data.materials.clear()
    for m in mats: o.data.materials.append(m)
    assert all(o.data.materials), ('empty material slot', o.name, keep, [m.name if m else None for m in mats])
# side by side in the file (0.5 m apart across), so the flicker check sees no two parts in one place; the game uses the
# meshes only (their own origin), never this layout
for i, o in enumerate(made): o.location = (0.0, i * 0.5, 0.0)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'CargoDoorLeaves.blend'))
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'CargoDoorLeaves.glb'), export_format='GLB')
print('door leaves:', len(made), 'meshes', flush=True)
