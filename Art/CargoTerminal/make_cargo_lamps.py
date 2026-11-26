# The cargo terminal's light fittings (step C, owner: the lamps were plain cubes), built in Blender for
# ACargoTerminalLife: four real fittings, each a housing mesh (housing + polished reflector) and its own lens mesh (the
# game gives the lens the lamp system's glowing face; Nanite on both - nothing here is see-through):
#   SM_CargoLamp_HighBay / _HighBayLens   LED high-bay: driver can with cooling fins, eye-bolt, spun aluminium bell
#                                         reflector (430 mm), frosted lens set 6 mm inside its rim. Origin: the rim.
#   SM_CargoLamp_Panel / _PanelLens       recessed linear office panel 1200 x 300: white steel flange, opal diffuser
#                                         set 3 mm up into it. Origin: the ceiling plane (flange below, body above).
#   SM_CargoLamp_Down / _DownLens         canopy downlight 220 mm: white trim ring, faceted reflector cone, lens.
#                                         Origin: the canopy's underside.
#   SM_CargoLamp_Flood / _FloodLens       floodlight head 600 x 420: die-cast body with rear cooling fins, visor,
#                                         toughened front glass, U-yoke. Faces -y, origin on the yoke's pivot.
# Units metres, z up.
#   python Tools/import_model.py Art/CargoTerminal/make_cargo_lamps.py Art/CargoTerminal/CargoLamps.glb CargoLamps
import bpy, bmesh, math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
SC = bpy.context.scene
COL = bpy.data.collections.new('CargoLamps'); SC.collection.children.link(COL)
TEX = os.path.join(HERE, 'Textures')


def mat(name, rgb, rough=0.5, metal=0.0, tex=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
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
    m.diffuse_color = (*rgb, 1)
    return m


M = dict(body=mat('M_CargoLamp_Body', (0.30, 0.31, 0.32), 0.45, 1.0, tex='Galvanised'),
         refl=mat('M_CargoLamp_Reflector', (0.85, 0.86, 0.88), 0.12, 1.0, tex='Aluminium'),
         white=mat('M_CargoLamp_White', (0.90, 0.90, 0.88), 0.4, 0.0, tex='PaintWhite'),
         lens=mat('M_CargoLamp_Lens', (0.96, 0.96, 0.93), 0.25, 0.0))
ORDER = ('body', 'refl', 'white', 'lens')


def ring(bm, r, z, n=32):
    return [bm.verts.new((r * math.cos(2 * math.pi * i / n), r * math.sin(2 * math.pi * i / n), z)) for i in range(n)]


def lathe(bm, prof, mi, n=32, cap0=False, cap1=False):
    """A turned part: prof = [(r, z), ...] round the z axis; caps close the first / last ring."""
    rs = [ring(bm, r, z, n) for r, z in prof]
    for a, b in zip(rs, rs[1:]):
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((a[i], a[j], b[j], b[i])).material_index = mi
    if cap0: bm.faces.new(list(reversed(rs[0]))).material_index = mi
    if cap1: bm.faces.new(rs[-1]).material_index = mi


def box(bm, x0, y0, z0, x1, y1, z1, mi, bev=0.0):
    r = bmesh.ops.create_cube(bm, size=1.0)
    for v in r['verts']:
        v.co.x = x0 if v.co.x < 0 else x1; v.co.y = y0 if v.co.y < 0 else y1; v.co.z = z0 if v.co.z < 0 else z1
    for f in {f for v in r['verts'] for f in v.link_faces}: f.material_index = mi
    if bev > 0:
        es = list({e for v in r['verts'] for e in v.link_edges})
        bmesh.ops.bevel(bm, geom=es, offset=bev, segments=2, profile=0.5, affect='EDGES')


def obj(name, bm, at):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:   # box-projected, 1 m to a tile (texel density as the building's)
        n = f.normal; ax = max(range(3), key=lambda k: abs(n[k]))
        for lp in f.loops:
            c = lp.vert.co
            lp[uv].uv = (c.x, c.y) if ax == 2 else ((c.x, c.z) if ax == 1 else (c.y, c.z))
        f.smooth = True
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.use_auto_smooth = True; me.auto_smooth_angle = math.radians(35)
    used = sorted({p.material_index for p in me.polygons})
    keys = [ORDER[i] for i in used]; remap = {old: i for i, old in enumerate(used)}
    for p in me.polygons: p.material_index = remap[p.material_index]
    for k in keys: me.materials.append(M[k])
    assert all(me.materials), ('empty material slot', name)
    o = bpy.data.objects.new(name, me); COL.objects.link(o); o.location = at
    return o


B, R, W, LN = 0, 1, 2, 3
made = []

# ── LED high-bay: eye-bolt, driver can with fins, bell reflector (rim at z 0) ──
bm = bmesh.new()
lathe(bm, [(0.215, 0.0), (0.218, 0.004), (0.205, 0.10), (0.17, 0.20), (0.12, 0.26), (0.105, 0.27)], R)        # bell outside
lathe(bm, [(0.10, 0.27), (0.105, 0.27)], B)                                                                  # the bell's top ledge
lathe(bm, [(0.105, 0.27), (0.115, 0.258), (0.163, 0.198), (0.198, 0.10), (0.21, 0.004), (0.215, 0.0)], R)    # bell inside (polished)
lathe(bm, [(0.10, 0.27), (0.10, 0.40), (0.085, 0.42), (0.03, 0.42)], B, cap1=True)                            # driver can
for i in range(12):                                                                                          # cooling fins
    a = 2 * math.pi * i / 12; c, s = math.cos(a), math.sin(a)
    f = bmesh.ops.create_cube(bm, size=1.0)
    for v in f['verts']:
        x = 0.098 + (0.035 if v.co.x > 0 else 0.0); y = 0.003 * (1 if v.co.y > 0 else -1); z = 0.285 if v.co.z < 0 else 0.395
        v.co.x, v.co.y, v.co.z = x * c - y * s, x * s + y * c, z
    for fc in {fc for v in f['verts'] for fc in v.link_faces}: fc.material_index = B
lathe(bm, [(0.008, 0.42), (0.008, 0.45)], B)                                                                  # eye-bolt shank
lathe(bm, [(0.03, 0.45), (0.03, 0.458), (0.008, 0.458)], B, cap0=True, cap1=True)                                       # its collar
made.append(obj('SM_CargoLamp_HighBay', bm, (0, 0, 0)))
bm = bmesh.new(); lathe(bm, [(0.207, 0.006), (0.12, 0.009)], LN, cap1=True)                               # frosted lens, 6 mm in
made.append(obj('SM_CargoLamp_HighBayLens', bm, (0, 0, 0)))

# ── recessed linear office panel 1200 x 300 (ceiling plane z 0; flange 12 mm below it, body above) ──
bm = bmesh.new()
for s in (-1, 1):                                                    # flange: a white frame round the diffuser
    box(bm, -0.60, min(s * 0.13, s * 0.15), -0.012, 0.60, max(s * 0.13, s * 0.15), 0.002, W, 0.002)
    box(bm, min(s * 0.58, s * 0.60), -0.131, -0.012, max(s * 0.58, s * 0.60), 0.131, 0.002, W, 0.002)
box(bm, -0.585, -0.135, -0.004, 0.585, 0.135, 0.0, W)                # the white tray behind the diffuser
box(bm, -0.57, -0.12, 0.004, 0.57, 0.12, 0.07, B)                    # the driver body up in the ceiling (unseen)
made.append(obj('SM_CargoLamp_Panel', bm, (1.0, 0, 0)))
bm = bmesh.new(); box(bm, -0.575, -0.125, -0.009, 0.575, 0.125, -0.006, LN)   # opal diffuser 3 mm up into the flange
made.append(obj('SM_CargoLamp_PanelLens', bm, (1.0, 0, 0)))

# ── canopy downlight 220 mm (canopy underside z 0) ──
bm = bmesh.new()
lathe(bm, [(0.08, -0.004), (0.11, -0.004), (0.12, -0.010), (0.125, -0.016), (0.13, -0.016)], W)              # trim ring
lathe(bm, [(0.13, -0.016), (0.13, 0.003), (0.085, 0.003)], W)
lathe(bm, [(0.08, -0.004), (0.07, 0.03), (0.05, 0.06), (0.045, 0.062)], R)                                     # reflector cone
lathe(bm, [(0.09, 0.005), (0.09, 0.13), (0.06, 0.15), (0.0, 0.15)], B)                                         # can above
made.append(obj('SM_CargoLamp_Down', bm, (2.0, 0, 0)))
bm = bmesh.new(); lathe(bm, [(0.062, 0.035), (0.03, 0.036)], LN, cap1=True)
made.append(obj('SM_CargoLamp_DownLens', bm, (2.0, 0, 0)))

# ── floodlight head 600 x 420, facing -y, pivot on the yoke (origin) ──
bm = bmesh.new()
box(bm, -0.30, -0.10, -0.21, 0.30, 0.12, 0.21, B, 0.02)               # die-cast body
for i in range(9):                                                    # rear cooling fins
    x = -0.24 + i * 0.06
    box(bm, x - 0.006, 0.115, -0.18, x + 0.006, 0.19, 0.18, B)
box(bm, -0.31, -0.19, 0.20, 0.31, -0.095, 0.215, B)                    # visor
box(bm, -0.27, -0.102, -0.18, 0.27, -0.09, 0.18, R)                   # reflector face inside the glass
for s in (-1, 1):                                                     # U-yoke and its pivot bosses
    box(bm, min(s * 0.295, s * 0.34), -0.03, -0.03, max(s * 0.295, s * 0.34), 0.03, 0.03, B)
    box(bm, s * 0.345 - 0.008, -0.025, -0.035, s * 0.345 + 0.008, 0.025, 0.31, B)
box(bm, -0.353, -0.025, 0.30, 0.353, 0.025, 0.32, B)
made.append(obj('SM_CargoLamp_Flood', bm, (3.0, 0, 0)))
bm = bmesh.new(); box(bm, -0.28, -0.112, -0.19, 0.28, -0.106, 0.19, LN)   # toughened front glass (frosted face)
made.append(obj('SM_CargoLamp_FloodLens', bm, (3.0, 0, 0)))

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'CargoLamps.blend'))
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'CargoLamps.glb'), export_format='GLB')
print('cargo lamps:', len(made), 'meshes', flush=True)
