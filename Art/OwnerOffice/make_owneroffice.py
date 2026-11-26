# Your office's furniture as a game model (owner 2026-11-26: "exactly as the render", Tools/render_owner_office.py and
# Blueprints/Terminal/Office/YourOffice-*.png). The render's own builders, the same sizes and places; only the room itself
# (floor, walls, windows, door, ceiling) and the wardrobe (AWardrobe, Offices.cpp) are the game's already.
# Axes as the render: X = plan x - 114 (east), Y = plan z (north), Z up from Floor 2's floor (+6.01). Metres.
# Placed by Offices.cpp as ABabylonModel "OfficeOwner" at plan (114, 6.01, 0), yaw 180: the model's (X, Y) is then the
# plan's (114 + X, Y) exactly.
# Named parts for the next steps (empties, exported as nodes, also written to Data/owner-office-points.json in plan metres):
#   OfficeChair: OC_Base (5-star base, stays) and OC_Swivel (its pivot on the gas column's axis: seat, back, arms turn),
#     OC_SitPoint (the seat's top, middle), OC_BackIK (the back's face at the lumbar), OC_ArmIK_L / _R (the arm pads' tops)
#   VisitorChair (cognac leather, in front of the desk on the rug, facing yours): VC_SitPoint, VC_BackIK, VC_ArmIK_L / _R
#   DogBed: DB_LiePoint (the cushion's top, middle)
#   the room door's handles (TerminalDoors' swing door D-F2-OFFICE-1-1): Door_HandleIn / Door_HandleOut, in the JSON only
# The laptop's parts are named laptop~* (the office's laptop opens the office device, UI/Devices.cpp).
# Two steps, one file:
#   python Art/OwnerOffice/make_owneroffice.py          -> its pictures (tinted wood/leather/fabric, the screen, the art)
#   python Tools/import_model.py Art/OwnerOffice/make_owneroffice.py Art/OwnerOffice/officeowner.glb OfficeOwner
import os, sys, math, json, random
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
FY = 6.01

def make_pictures():
    from PIL import Image, ImageDraw, ImageFont, ImageOps
    def tinted(src, out, tint, size=1024):
        im = Image.open(os.path.join(ROOT, src)).convert('RGB').resize((size, size), Image.LANCZOS)
        ch = [c.point(lambda v, k=k: min(255, int(v * k))) for c, k in zip(im.split(), tint)]
        Image.merge('RGB', ch).save(os.path.join(HERE, out))
    tinted('Art/WoodFloor/T_WoodFloor_D.png', 'T_Walnut.png', (0.55, 0.42, 0.34))
    tinted('Art/WoodFloor/T_WoodFloor_D.png', 'T_Oak.png', (1.3, 1.15, 0.95))
    tinted('Art/Fabrics/wool_suiting/albedo.png', 'T_ChairFabric.png', (0.9, 0.9, 1.0))
    tinted('Art/Fabrics/leather/albedo.png', 'T_Leather.png', (3.2, 2.2, 1.6))
    tinted('Art/Fabrics/knit/albedo.png', 'T_SofaFabric.png', (0.85, 0.9, 1.0))
    tinted('Art/Fabrics/knit/albedo.png', 'T_Rug.png', (0.75, 0.8, 0.9))
    for src, out in (('Art/WoodFloor/T_WoodFloor_N.png', 'T_Wood_N.png'), ('Art/Fabrics/wool_suiting/normal.png', 'T_ChairFabric_N.png'),
                     ('Art/Fabrics/leather/normal.png', 'T_Leather_N.png'), ('Art/Fabrics/knit/normal.png', 'T_Knit_N.png')):
        Image.open(os.path.join(ROOT, src)).convert('RGB').resize((1024, 1024), Image.LANCZOS).save(os.path.join(HERE, out))
    # the laptop's screen: operations at a glance, no airport name (it is never baked) and no real airlines; drawn mirrored
    # (the loader mirrors a Blender model's x, Tools/import_model.py)
    FB, FR = 'C:/Windows/Fonts/arialbd.ttf', 'C:/Windows/Fonts/arial.ttf'
    im = Image.new('RGB', (1600, 1000), (12, 22, 38)); d = ImageDraw.Draw(im)
    d.rectangle((0, 0, 1600, 70), fill=(20, 40, 70)); d.text((30, 16), 'Operations', font=ImageFont.truetype(FB, 34), fill=(230, 240, 255))
    for i, (lab, val, col) in enumerate((('Passengers today', '18 420', (110, 230, 140)), ('On-time', '94 %', (120, 200, 255)), ('Revenue', 'CHF 412 k', (255, 196, 70)))):
        x = 30 + i * 520; d.rounded_rectangle((x, 100, x + 490, 260), 16, fill=(22, 38, 62)); d.text((x + 24, 120), lab, font=ImageFont.truetype(FR, 28), fill=(150, 175, 210))
        d.text((x + 24, 165), val, font=ImageFont.truetype(FB, 60), fill=col)
    d.rounded_rectangle((30, 290, 1010, 960), 16, fill=(22, 38, 62)); d.text((54, 305), 'Departures', font=ImageFont.truetype(FB, 30), fill=(230, 240, 255))
    rows = [('08:40', 'Zurich', 'Gate 3', 'Boarding'), ('09:05', 'London', 'R4', 'Bus'), ('09:20', 'Frankfurt', 'Gate 1', 'On time'),
            ('09:55', 'Paris', 'Gate 11', 'On time'), ('10:10', 'Lisbon', 'R6', 'Delayed'), ('10:35', 'Brussels', 'Gate 2', 'On time')]
    for i, r in enumerate(rows):
        y = 360 + i * 95; d.line((54, y - 12, 990, y - 12), fill=(40, 60, 90), width=2)
        for j, (t, x) in enumerate(zip(r, (54, 220, 520, 760))):
            col = (255, 196, 70) if t in ('Delayed', 'Bus') else (110, 230, 140) if t == 'Boarding' else (225, 235, 250)
            d.text((x, y + 10), t, font=ImageFont.truetype(FR, 32), fill=col)
    d.rounded_rectangle((1040, 290, 1570, 960), 16, fill=(22, 38, 62)); d.text((1064, 305), 'Staff', font=ImageFont.truetype(FB, 30), fill=(230, 240, 255))
    for i, (t, v) in enumerate((('On shift', '212'), ('Apron buses', '3 / 3'), ('Mood', 'Good'), ('Messages', '2 new'))):
        d.text((1064, 380 + i * 130), t, font=ImageFont.truetype(FR, 30), fill=(150, 175, 210)); d.text((1064, 420 + i * 130), v, font=ImageFont.truetype(FB, 44), fill=(230, 240, 255))
    ImageOps.mirror(im).save(os.path.join(HERE, 'T_LaptopScreen.png'))
    for seed in (11, 23):                                   # our own abstract canvases (as the render)
        r = random.Random(seed); w, h = 900, 1100; im = Image.new('RGB', (w, h), (232, 226, 214)); d = ImageDraw.Draw(im)
        pal = [(28, 52, 92), (196, 120, 52), (220, 186, 120), (90, 120, 110), (180, 70, 50)]
        for i in range(9):
            x, y = r.randint(-100, w), r.randint(-100, h); s = r.randint(120, 420)
            d.ellipse((x, y, x + s, y + s * r.uniform(0.6, 1.4)), fill=pal[i % len(pal)]) if i % 2 else d.rectangle((x, y, x + s, y + s // 3), fill=pal[i % len(pal)])
        im.save(os.path.join(HERE, 'T_Art%d.png' % seed))
    print('pictures made in', HERE)

try:
    import bpy, bmesh
    from mathutils import Vector, Matrix
except ImportError:
    make_pictures(); sys.exit(0)

bpy.ops.wm.read_factory_settings(use_empty=True); sc = bpy.context.scene
rng = random.Random(7)
W_, D_, H_ = 12.3, 9.0, 5.31
TILE = {}                                              # material -> metres one picture covers

# #region ── Materials (PBR; every textured one a picture of its own, tint baked in, normals where the render had them) ──
def mat(name, rgb, rough=0.5, metal=0.0, emit=0.0, alpha=1.0, tex=None, tile=1.0, nrm=None):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    if emit:
        b.inputs['Emission'].default_value = (*rgb, 1); b.inputs['Emission Strength'].default_value = emit
    if alpha < 1:
        b.inputs['Alpha'].default_value = alpha; m.blend_method = 'BLEND'
    if tex:
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(HERE, tex)); nt.links.new(t.outputs[0], b.inputs['Base Color'])
        if emit: nt.links.new(t.outputs[0], b.inputs['Emission'])
    if nrm:
        n = nt.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(os.path.join(HERE, nrm)); n.image.colorspace_settings.name = 'Non-Color'
        nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.6
        nt.links.new(n.outputs[0], nm.inputs['Color']); nt.links.new(nm.outputs[0], b.inputs['Normal'])
    TILE[name] = tile
    return m
WALNUT = mat('OO_Walnut', (1, 1, 1), 0.3, tex='T_Walnut.png', tile=1.6, nrm='T_Wood_N.png')
OAKLEG = mat('OO_Oak', (1, 1, 1), 0.4, tex='T_Oak.png', tile=0.8, nrm='T_Wood_N.png')
BLACKSTEEL = mat('OO_BlackSteel', (0.025, 0.025, 0.028), 0.35, 0.7)
CHROME = mat('OO_Chrome', (0.9, 0.9, 0.92), 0.08, 1.0)
POLALU = mat('OO_PolishedAlu', (0.8, 0.8, 0.82), 0.15, 1.0)
FABRIC = mat('OO_ChairFabric', (1, 1, 1), 0.9, tex='T_ChairFabric.png', tile=0.25, nrm='T_ChairFabric_N.png')
LEATHER = mat('OO_Leather', (1, 1, 1), 0.45, tex='T_Leather.png', tile=0.3, nrm='T_Leather_N.png')
SOFAFAB = mat('OO_SofaFabric', (1, 1, 1), 0.9, tex='T_SofaFabric.png', tile=0.3, nrm='T_Knit_N.png')
RUG = mat('OO_Rug', (1, 1, 1), 0.95, tex='T_Rug.png', tile=0.6, nrm='T_Knit_N.png')
PLASTIC = mat('OO_BlackPlastic', (0.03, 0.03, 0.032), 0.45)
GREYPL = mat('OO_GreyPlastic', (0.18, 0.18, 0.19), 0.5)
SPACEGREY = mat('OO_SpaceGrey', (0.22, 0.225, 0.235), 0.28, 0.85)
KEYS = mat('OO_Keys', (0.03, 0.03, 0.035), 0.55)
LEAF = mat('OO_Leaf', (0.05, 0.2, 0.06), 0.55); STEM = mat('OO_Stem', (0.18, 0.12, 0.06), 0.8)
POT = mat('OO_Pot', (0.75, 0.73, 0.7), 0.6); SOIL = mat('OO_Soil', (0.06, 0.04, 0.03), 0.95)
LAMPGLOW = mat('OO_LampGlow', (1.0, 0.92, 0.8), 0.2, emit=8.0); LEDSTRIP = mat('OO_LED', (1.0, 0.95, 0.88), 0.2, emit=14.0)
WHITE = mat('OO_White', (0.85, 0.85, 0.83), 0.4); CERAMIC = mat('OO_Ceramic', (0.92, 0.92, 0.9), 0.15)
BOWL = mat('OO_Steel', (0.7, 0.71, 0.72), 0.2, 1.0); DOGBED = mat('OO_DogBed', (0.35, 0.28, 0.22), 0.95); DOGBED2 = mat('OO_DogBedIn', (0.62, 0.56, 0.48), 0.95)
PAPER = mat('OO_Paper', (0.92, 0.91, 0.87), 0.8); NOTE = mat('OO_Notebook', (0.08, 0.12, 0.2), 0.6)
SCR = mat('OO_LaptopScreen', (1, 1, 1), 0.15, emit=1.6, tex='T_LaptopScreen.png')
TRACK = mat('OO_Trackpad', (0.3, 0.305, 0.315), 0.25, 0.6); LCD = mat('OO_PhoneLCD', (0.3, 0.5, 0.6), 0.2, emit=0.6)
GRILLE = mat('OO_Grille', (0.12, 0.12, 0.12), 0.7); RADIOLIT = mat('OO_RadioLit', (1.0, 0.7, 0.3), 0.3, emit=2.0)
COFFEE = mat('OO_Coffee', (0.08, 0.04, 0.02), 0.1); WATER = mat('OO_Water', (0.6, 0.7, 0.75), 0.02, alpha=0.4)
TAILBLUE = mat('OO_TailBlue', (0.05, 0.2, 0.5), 0.4)
BOOKS = [mat(f'OO_Book{k}', c, 0.6) for k, c in enumerate([(0.5, 0.1, 0.08), (0.1, 0.2, 0.4), (0.85, 0.8, 0.7), (0.15, 0.3, 0.2)])]
ARTM = [mat(f'OO_Art{s}', (1, 1, 1), 0.8, tex=f'T_Art{s}.png') for s in (11, 23)]
# #endregion

# #region ── Helpers (the render's) ──
def link(o):
    sc.collection.objects.link(o); return o
def bev(o, w, segs=3):
    if w > 0:
        m = o.modifiers.new('B', 'BEVEL'); m.width = w; m.segments = segs; m.limit_method = 'ANGLE'
    return o
def box(name, c, s, m, bevel=0.0, segs=3, rot=None):
    bpy.ops.mesh.primitive_cube_add(location=c); o = bpy.context.active_object; o.name = name; o.scale = (s[0] / 2, s[1] / 2, s[2] / 2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if rot: o.rotation_euler = rot
    o.data.materials.append(m); bev(o, bevel, segs)
    for p in o.data.polygons: p.use_smooth = bevel > 0
    return o
def cyl(name, c, r, d, m, axis='Z', verts=32, bevel=0.0, rot=None, r2=None):
    if r2 is None: bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=d, vertices=verts, location=c)
    else: bpy.ops.mesh.primitive_cone_add(radius1=r, radius2=r2, depth=d, vertices=verts, location=c)
    o = bpy.context.active_object; o.name = name
    if axis == 'X': o.rotation_euler = (0, math.pi / 2, 0)
    elif axis == 'Y': o.rotation_euler = (math.pi / 2, 0, 0)
    if rot: o.rotation_euler = rot
    o.data.materials.append(m); bev(o, bevel, 2)
    for p in o.data.polygons: p.use_smooth = True
    sm = o.modifiers.new('E', 'EDGE_SPLIT'); sm.split_angle = math.radians(40)    # (smooth round, crisp caps)
    return o
def sph(name, c, r, m, scale=(1, 1, 1), segs=24):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r, location=c, segments=segs, ring_count=segs // 2); o = bpy.context.active_object; o.name = name
    o.scale = scale; o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    return o
def tor(name, c, R, r, m, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_torus_add(major_radius=R, minor_radius=r, location=c, rotation=rot, major_segments=48, minor_segments=16)
    o = bpy.context.active_object; o.name = name; o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    return o
def lathe(name, prof, c, m, n=48):
    bm = bmesh.new(); rings = []
    for i in range(n):
        a = 2 * math.pi * i / n
        rings.append([bm.verts.new((r * math.cos(a), r * math.sin(a), z)) for r, z in prof])
    for i in range(n):
        A, B = rings[i], rings[(i + 1) % n]
        for j in range(len(prof) - 1): bm.faces.new((A[j], B[j], B[j + 1], A[j + 1]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); o = link(bpy.data.objects.new(name, me)); o.data.materials.append(m); o.location = c
    for p in o.data.polygons: p.use_smooth = True
    sm = o.modifiers.new('E', 'EDGE_SPLIT'); sm.split_angle = math.radians(50)
    return o
def empty(name, loc, parent=None):
    e = link(bpy.data.objects.new(name, None)); e.empty_display_size = 0.05; e.location = loc
    if parent: e.parent = parent
    return e
def group(name, objs, loc=(0, 0, 0), rotz=0.0):
    e = empty(name, (0, 0, 0))
    for o in objs: o.parent = e
    e.location = loc; e.rotation_euler = (0, 0, rotz); return e
def uv_plane(name, corners, m):                     # a quad with UVs 0..1 (screens, pictures), corners: ll, lr, ur, ul
    bm = bmesh.new(); uv = bm.loops.layers.uv.new(); f = bm.faces.new([bm.verts.new(v) for v in corners])
    for l, u in zip(f.loops, [(0, 0), (1, 0), (1, 1), (0, 1)]): l[uv].uv = u
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); o = link(bpy.data.objects.new(name, me)); o.data.materials.append(m); o['keep_uv'] = 1; return o
def clone(objs, prefix_from, prefix_to):           # linked copies (one mesh, drawn instanced in the game)
    out = []
    for o in objs:
        c = o.copy(); c.name = o.name.replace(prefix_from, prefix_to, 1); link(c); out.append(c)
    return out
# #endregion

# #region ── The desk (walnut top, black steel sled legs, drawer pedestal), facing the Grand Hall ──
DCX, DCY = 5.2, 4.3
desk = [box('DK_Top', (DCX, DCY, 0.733), (0.9, 2.0, 0.04), WALNUT, bevel=0.006)]
for s in (-1, 1):
    y = DCY + s * 0.88
    for i, x in enumerate((DCX - 0.36, DCX + 0.36)): desk.append(box(f'DK_Post{s}{i}', (x, y, 0.3575), (0.04, 0.04, 0.695), BLACKSTEEL, bevel=0.004))
    desk.append(box(f'DK_SledTop{s}', (DCX, y, 0.7), (0.78, 0.04, 0.03), BLACKSTEEL, bevel=0.004))
    desk.append(box(f'DK_SledFoot{s}', (DCX, y, 0.015), (0.8, 0.05, 0.03), BLACKSTEEL, bevel=0.004))
desk.append(box('DK_Rail', (DCX + 0.3, DCY, 0.66), (0.03, 1.72, 0.06), BLACKSTEEL, bevel=0.003))
desk.append(box('DK_CableTray', (DCX + 0.1, DCY, 0.64), (0.2, 1.2, 0.05), BLACKSTEEL, bevel=0.003))
desk.append(box('DK_Pedestal', (DCX + 0.12, DCY + 0.62, 0.31), (0.55, 0.42, 0.58), WALNUT, bevel=0.004))
for k in range(3):
    desk.append(box(f'DK_DrawerLine{k}', (DCX + 0.396, DCY + 0.62, 0.13 + k * 0.19), (0.004, 0.40, 0.006), BLACKSTEEL))
    desk.append(box(f'DK_Pull{k}', (DCX + 0.4, DCY + 0.62, 0.2 + k * 0.19), (0.008, 0.18, 0.012), BLACKSTEEL, bevel=0.002))
group('Desk', desk)
# #endregion

# #region ── Your chair (swivel) and the visitor's chair facing you ──
def exec_chair(p):
    B, S = [], []
    B.append(cyl(p + 'Hub', (0, 0, 0.1), 0.055, 0.09, POLALU, bevel=0.01))
    for k in range(5):
        a = k * 2 * math.pi / 5
        B.append(box(p + f'Leg{k}', (0.16 * math.cos(a), 0.16 * math.sin(a), 0.085), (0.32, 0.045, 0.04), POLALU, bevel=0.012, rot=(0, -0.08, a)))
        cx, cy = 0.33 * math.cos(a), 0.33 * math.sin(a)
        B.append(box(p + f'CasterFork{k}', (cx, cy, 0.06), (0.05, 0.03, 0.04), PLASTIC, bevel=0.008, rot=(0, 0, a)))
        for s in (-1, 1):
            B.append(cyl(p + f'Wheel{k}{s}', (cx + s * 0.0145 * math.cos(a + math.pi / 2), cy + s * 0.0145 * math.sin(a + math.pi / 2), 0.03), 0.03, 0.013, PLASTIC,
                         rot=(math.pi / 2, 0, a), bevel=0.004))
    B.append(cyl(p + 'GasCover', (0, 0, 0.22), 0.032, 0.18, PLASTIC))
    S.append(cyl(p + 'Gas', (0, 0, 0.35), 0.022, 0.1, CHROME))
    S.append(box(p + 'Mech', (0.02, 0, 0.42), (0.26, 0.22, 0.06), PLASTIC, bevel=0.01))
    S.append(box(p + 'SeatPan', (0, 0, 0.46), (0.5, 0.5, 0.03), PLASTIC, bevel=0.01))
    S.append(box(p + 'Seat', (-0.01, 0, 0.515), (0.5, 0.5, 0.085), FABRIC, bevel=0.035, segs=4))
    S.append(box(p + 'Spine', (0.25, 0, 0.55), (0.05, 0.08, 0.2), PLASTIC, bevel=0.01, rot=(0, -0.25, 0)))
    S.append(box(p + 'Back', (0.29, 0, 0.92), (0.05, 0.47, 0.62), FABRIC, bevel=0.022, segs=4, rot=(0, -0.17, 0)))
    S.append(box(p + 'BackFrame', (0.316, 0, 0.92), (0.03, 0.5, 0.66), PLASTIC, bevel=0.015, segs=3, rot=(0, -0.17, 0)))
    S.append(box(p + 'Lumbar', (0.26, 0, 0.72), (0.03, 0.34, 0.09), PLASTIC, bevel=0.012, rot=(0, -0.17, 0)))
    S.append(box(p + 'HeadStem', (0.36, 0, 1.27), (0.025, 0.05, 0.14), PLASTIC, bevel=0.004, rot=(0, -0.17, 0)))
    S.append(box(p + 'Headrest', (0.37, 0, 1.36), (0.06, 0.3, 0.13), FABRIC, bevel=0.03, segs=4, rot=(0, -0.17, 0)))
    for s in (-1, 1):
        S.append(box(p + f'ArmPost{s}', (0.06, s * 0.27, 0.6), (0.05, 0.03, 0.2), PLASTIC, bevel=0.008))
        S.append(box(p + f'ArmBracket{s}', (0.1, s * 0.255, 0.5), (0.18, 0.04, 0.025), PLASTIC, bevel=0.006))
        S.append(box(p + f'ArmPad{s}', (0.03, s * 0.27, 0.7), (0.26, 0.085, 0.03), PLASTIC, bevel=0.012, segs=3))
    return B, S
def visitor_chair(p):                                  # an upholstered shell on four splayed oak legs, cognac leather
    P = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            P.append(cyl(p + f'Leg{sx}{sy}', (sx * 0.19, sy * 0.19, 0.22), 0.018, 0.44, OAKLEG, r2=0.012, rot=(sy * -0.07, sx * 0.07, 0)))
    P.append(box(p + 'Rail', (0, 0, 0.42), (0.42, 0.42, 0.03), OAKLEG, bevel=0.006))
    P.append(box(p + 'Seat', (0, 0, 0.47), (0.5, 0.48, 0.08), LEATHER, bevel=0.03, segs=4))
    bm = bmesh.new(); prof = []; R = 0.29; n = 16
    for i in range(n + 1):
        a = math.radians(-55 + 110 * i / n); prof.append((R * math.cos(a), R * math.sin(a)))
    th = 0.045; top, bot = 0.86, 0.512
    outer = [(x * (1 + th / R), y * (1 + th / R)) for x, y in prof]
    vs = {}
    for zi, z in enumerate((bot, top)):
        vs[('i', zi)] = [bm.verts.new((x, y, z)) for x, y in prof]; vs[('o', zi)] = [bm.verts.new((x, y, z)) for x, y in outer]
    for i in range(n):
        bm.faces.new((vs[('i', 0)][i], vs[('i', 0)][i + 1], vs[('i', 1)][i + 1], vs[('i', 1)][i]))
        bm.faces.new((vs[('o', 0)][i + 1], vs[('o', 0)][i], vs[('o', 1)][i], vs[('o', 1)][i + 1]))
        bm.faces.new((vs[('i', 1)][i], vs[('i', 1)][i + 1], vs[('o', 1)][i + 1], vs[('o', 1)][i]))
        bm.faces.new((vs[('o', 0)][i], vs[('o', 0)][i + 1], vs[('i', 0)][i + 1], vs[('i', 0)][i]))
    for e in (0, n):
        bm.faces.new((vs[('i', 0)][e], vs[('i', 1)][e], vs[('o', 1)][e], vs[('o', 0)][e]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(p + 'Back'); bm.to_mesh(me); bm.free(); bk = link(bpy.data.objects.new(p + 'Back', me)); bk.data.materials.append(LEATHER)
    bk.location = (-0.02, 0, 0); bev(bk, 0.015, 3); sd = bk.modifiers.new('S', 'SUBSURF'); sd.levels = 1
    for q in bk.data.polygons: q.use_smooth = True
    P.append(bk)
    for s in (-1, 1): P.append(box(p + f'BackPost{s}', (0.2, s * 0.2, 0.51), (0.03, 0.03, 0.14), OAKLEG, bevel=0.005))
    return P
cb, cs = exec_chair('OC_')
oc = group('OfficeChair', [], (DCX + 0.95, DCY - 0.1, 0), math.radians(8))          # faces -x: the desk and the hall
base = group('OC_Base', cb); base.parent = oc
swiv = group('OC_Swivel', [], (0, 0, 0.3)); swiv.parent = oc
for o in cs: o.parent = swiv; o.location.z -= 0.3                                   # (the swivel's pivot: the gas column's axis)
empty('OC_SitPoint', (-0.01, 0, 0.5575 - 0.3), swiv)                                # the seat's top, middle
empty('OC_BackIK', (0.24, 0, 0.82 - 0.3), swiv)                                     # the back's face at the lumbar
for s, side in ((-1, 'L'), (1, 'R')): empty(f'OC_ArmIK_{side}', (0.03, s * 0.27, 0.715 - 0.3), swiv)   # the arm pads' tops
VC = visitor_chair('VC_')
vc = group('VisitorChair', VC, (DCX - 0.95, DCY, 0), math.pi)                     # faces +x: you
empty('VC_SitPoint', (0, 0, 0.51), vc)
empty('VC_BackIK', (0.245, 0, 0.7), vc)                                             # the shell's inside, the small of the back
a52 = math.radians(52)
for s, side in ((-1, 'L'), (1, 'R')): empty(f'VC_ArmIK_{side}', (-0.02 + 0.3125 * math.cos(a52), s * 0.3125 * math.sin(a52), 0.86), vc)   # the shell's ends, on top
# #endregion

# #region ── On the desk: the laptop (no brand), lamp, phone, radio, notebook, mug, a small plant ──
LX, LY = DCX + 0.12, DCY - 0.05
lap = [box('laptop~Base', (LX, LY, 0.762), (0.215, 0.31, 0.014), SPACEGREY, bevel=0.004)]
key0 = box('laptop~Key0_0', (LX - 0.045, LY - 0.12, 0.7695), (0.016, 0.017, 0.003), KEYS); lap.append(key0)
for r in range(5):
    for k in range(13):
        if r == 0 and k == 0: continue
        c = key0.copy(); c.name = f'laptop~Key{r}_{k}'; link(c); c.location = (LX - 0.045 + r * 0.019, LY - 0.12 + k * 0.02, 0.7695); lap.append(c)
lap.append(box('laptop~Trackpad', (LX + 0.074, LY, 0.769), (0.055, 0.11, 0.002), TRACK))
hinge = Vector((LX - 0.1075, LY, 0.769)); t_ = math.radians(108) - math.pi / 2
lid = box('laptop~Lid', (0, 0, 0), (0.006, 0.31, 0.215), SPACEGREY, bevel=0.003); lid.rotation_euler = (0, -t_, 0)
lid.location = hinge + Vector((-0.1075 * math.sin(t_) - 0.003, 0, 0.1075 * math.cos(t_))); lap.append(lid)
ux, uz = -math.sin(t_), math.cos(t_)
cx, cz = hinge.x + ux * 0.1075 + 0.0035 * math.cos(t_) - 0.003 + 0.0012, hinge.z + uz * 0.1075 + 0.0035 * math.sin(t_)
def lidpt(u, v): return (cx + ux * v, LY + u, cz + uz * v)
lap.append(uv_plane('laptop~Screen', [lidpt(-0.145, -0.095), lidpt(0.145, -0.095), lidpt(0.145, 0.095), lidpt(-0.145, 0.095)], SCR))
group('Laptop', lap)
lb = (DCX - 0.25, DCY + 0.78, 0.755)                  # desk lamp: weighted base, two arms, a shade with its LED
dl = [cyl('DL_Base', (lb[0], lb[1], lb[2] + 0.0125), 0.08, 0.025, BLACKSTEEL, bevel=0.006),
      cyl('DL_Arm1', (lb[0] + 0.05, lb[1], lb[2] + 0.2), 0.008, 0.42, BLACKSTEEL, rot=(0, math.radians(15), 0)),
      cyl('DL_Arm2', (lb[0] + 0.2, lb[1] - 0.05, lb[2] + 0.42), 0.008, 0.36, BLACKSTEEL, rot=(math.radians(18), math.radians(70), 0)),
      cyl('DL_Shade', (lb[0] + 0.36, lb[1] - 0.12, lb[2] + 0.45), 0.065, 0.12, BLACKSTEEL, r2=0.03, rot=(0, math.radians(160), 0)),
      cyl('DL_LED', (lb[0] + 0.378, lb[1] - 0.12, lb[2] + 0.392), 0.05, 0.004, LAMPGLOW, rot=(0, math.radians(160), 0))]
group('DeskLamp', dl)
DESKLAMP = (lb[0] + 0.378, lb[1] - 0.12, lb[2] + 0.38); DESKLAMP_DIR = (math.sin(math.radians(160)), 0, math.cos(math.radians(160)))
ph = [box('PH_Body', (DCX + 0.15, DCY - 0.72, 0.777), (0.2, 0.22, 0.045), GREYPL, bevel=0.01, rot=(0, 0, 0.3)),
      box('PH_Handset', (DCX + 0.13, DCY - 0.79, 0.816), (0.05, 0.2, 0.03), PLASTIC, bevel=0.012, rot=(0, 0, 0.3)),
      box('PH_Screen', (DCX + 0.185, DCY - 0.66, 0.802), (0.07, 0.05, 0.002), LCD, rot=(0, 0, 0.3))]
group('DeskPhone', ph)
rd = [box('RD_Case', (DCX - 0.22, DCY - 0.6, 0.83), (0.12, 0.26, 0.15), WALNUT, bevel=0.01),
      box('RD_Grille', (DCX - 0.157, DCY - 0.66, 0.83), (0.004, 0.11, 0.11), GRILLE),
      cyl('RD_Dial', (DCX - 0.155, DCY - 0.53, 0.84), 0.022, 0.012, POLALU, axis='X', bevel=0.003),
      box('RD_Window', (DCX - 0.1575, DCY - 0.53, 0.885), (0.003, 0.08, 0.02), RADIOLIT)]
group('Radio', rd)
mug = lathe('MG_Mug', [(0.0, 0.0), (0.04, 0.0), (0.042, 0.005), (0.042, 0.095), (0.037, 0.095), (0.037, 0.008), (0.0, 0.008)], (DCX + 0.3, DCY - 0.38, 0.755), CERAMIC)
dk = [box('NB_Notebook', (DCX + 0.2, DCY + 0.32, 0.762), (0.21, 0.15, 0.014), NOTE, bevel=0.003, rot=(0, 0, 0.12)),
      cyl('NB_Pen', (DCX + 0.2, DCY + 0.45, 0.759), 0.004, 0.14, CHROME, rot=(math.pi / 2, 0, 0.4)),
      mug, cyl('MG_Coffee', (DCX + 0.3, DCY - 0.38, 0.84), 0.036, 0.004, COFFEE),
      tor('MG_Handle', (DCX + 0.347, DCY - 0.38, 0.805), 0.022, 0.006, CERAMIC, rot=(math.pi / 2, 0, 0))]
group('DeskThings', dk)
def plant(name, c, pot_r, pot_h, h, nleaves, leaf_len, spread):
    P = [lathe(name + 'Pot', [(0.0, 0.0), (pot_r * 0.8, 0.0), (pot_r, pot_h), (pot_r * 0.92, pot_h), (pot_r * 0.92 - 0.005, pot_h * 0.9), (0.0, pot_h * 0.9)], c, POT),
         cyl(name + 'Soil', (c[0], c[1], c[2] + pot_h * 0.88), pot_r * 0.9, 0.01, SOIL)]
    leaf0 = None; used = []; dirs = []
    for i in range(nleaves):
        for _ in range(30):                             # (no leaf on another's place: they would flicker)
            a = rng.uniform(0, 2 * math.pi); t = rng.uniform(0.25, 1.0)
            z = c[2] + pot_h + h * t; r = spread * (0.3 + 0.7 * rng.random())
            q = Vector((c[0] + r * math.cos(a), c[1] + r * math.sin(a), z))
            dv = Vector((r * 0.5 * math.cos(a), r * 0.5 * math.sin(a), max(0.01, z - c[2] - pot_h))).normalized()   # (nor a stem along another's)
            if all((q - u).length > leaf_len * 0.85 for u in used) and all(dv.dot(w) < 0.996 for w in dirs): break
        used.append(q); dirs.append(dv); sk = 0.004 + 0.002 * (i % 12)   # (the stems' feet in the soil at their own depths)
        P.append(cyl(name + f'Stem{i}', ((c[0] + r * 0.5 * math.cos(a)), c[1] + r * 0.5 * math.sin(a), (c[2] + pot_h - sk + z) / 2), 0.004 + h * 0.003 + i * 0.00025, z - c[2] - pot_h + sk, STEM,
                     rot=(r * 0.6 * math.sin(a) / max(0.1, h), -r * 0.6 * math.cos(a) / max(0.1, h), 0), verts=8))
        if leaf0 is None: lf = leaf0 = sph(name + f'Leaf{i}', (0, 0, 0), leaf_len / 2, LEAF, scale=(1.0, 0.42, 0.06), segs=12)
        else: lf = leaf0.copy(); lf.name = name + f'Leaf{i}'; link(lf)
        lf.location = (c[0] + r * math.cos(a), c[1] + r * math.sin(a), z); lf.rotation_euler = (rng.uniform(-0.4, 0.4), rng.uniform(-0.7, 0.2), a)
        P.append(lf)
    # (its stems one part: thin stems crossing at shallow angles near the soil are one surface, never two fighting)
    st = [o for o in P if 'Stem' in o.name]; P = [o for o in P if 'Stem' not in o.name] + [st[0]]
    bpy.ops.object.select_all(action='DESELECT')
    for o in st: o.select_set(True)
    bpy.context.view_layer.objects.active = st[0]; bpy.ops.object.join(); st[0].name = name + 'Stems'
    return group(name, P)
plant('PL_Desk', (DCX - 0.3, DCY - 0.85, 0.755), 0.06, 0.09, 0.18, 14, 0.07, 0.1)
plant('PL_Big', (0.85, 8.45, 0.0), 0.28, 0.5, 1.6, 46, 0.32, 0.3)                    # (clear of the glass guard at x 0.36)
plant('PL_Corner', (11.6, 0.9, 0.0), 0.22, 0.45, 1.1, 34, 0.26, 0.42)
# #endregion

# #region ── The rest of the room: rug, sofa corner, art, the dog's bed with its bowls and football, meeting table, sideboard, pendants ──
group('Rug', [box('RG_Rug', (DCX, DCY, 0.0075), (3.4, 2.8, 0.012), RUG, bevel=0.004)])
SX = W_ - 0.58                                         # (its back 4 cm clear of the east partition's face, x 12.24)
sf = [box('SF_Base', (SX, 4.3, 0.22), (0.9, 2.3, 0.3), SOFAFAB, bevel=0.03), box('SF_Back', (SX + 0.33, 4.3, 0.6), (0.24, 2.3, 0.55), SOFAFAB, bevel=0.06, segs=4)]
for s in (-1, 1): sf.append(box(f'SF_Arm{s}', (SX, 4.3 + s * 1.1, 0.42), (0.9, 0.2, 0.42), SOFAFAB, bevel=0.06, segs=4))
for k in (-1, 1): sf.append(box(f'SF_Cushion{k}', (SX - 0.07, 4.3 + k * 0.5, 0.42), (0.72, 0.98, 0.14), SOFAFAB, bevel=0.05, segs=4))
for k in (-1, 1): sf.append(box(f'SF_BackCushion{k}', (SX + 0.19, 4.3 + k * 0.5, 0.68), (0.2, 0.96, 0.42), SOFAFAB, bevel=0.07, segs=4, rot=(0, -0.15, 0)))
for s in (-1, 1):
    for t in (-1, 1): sf.append(cyl(f'SF_Foot{s}{t}', (SX + s * 0.35, 4.3 + t * 1.05, 0.0375), 0.02, 0.075, OAKLEG))
group('Sofa', sf)
group('CoffeeTable', [cyl('CT_Top', (SX - 1.3, 4.3, 0.42), 0.45, 0.03, WALNUT, verts=64, bevel=0.006), cyl('CT_Stem', (SX - 1.3, 4.3, 0.21), 0.05, 0.4, BLACKSTEEL),
                      cyl('CT_Foot', (SX - 1.3, 4.3, 0.01), 0.25, 0.02, BLACKSTEEL, bevel=0.004), box('CT_Magazine', (SX - 1.25, 4.2, 0.4395), (0.22, 0.3, 0.006), PAPER, rot=(0, 0, 0.3))])
AX = W_ - 3.1                                           # an armchair facing the sofa across the coffee table
ac = [box('AC_Base', (AX, 4.3, 0.22), (0.85, 0.85, 0.3), SOFAFAB, bevel=0.03), box('AC_Back', (AX - 0.33, 4.3, 0.6), (0.22, 0.85, 0.55), SOFAFAB, bevel=0.06, segs=4)]
for s_ in (-1, 1): ac.append(box(f'AC_Arm{s_}', (AX, 4.3 + s_ * 0.38, 0.42), (0.85, 0.18, 0.4), SOFAFAB, bevel=0.06, segs=4))
ac.append(box('AC_Cushion', (AX + 0.05, 4.3, 0.42), (0.7, 0.57, 0.14), SOFAFAB, bevel=0.05, segs=4))
for s_ in (-1, 1):
    for t in (-1, 1): ac.append(cyl(f'AC_Foot{s_}{t}', (AX + s_ * 0.35, 4.3 + t * 0.35, 0.0375), 0.02, 0.075, OAKLEG))
group('Armchair', ac)
art = []
for k, y in enumerate((3.55, 5.05)):                    # on the east wall over the sofa (its face at x 12.24)
    art.append(box(f'AR_Frame{k}', (W_ - 0.075, y, 1.75), (0.03, 1.2, 1.5), BLACKSTEEL, bevel=0.003))
    art.append(uv_plane(f'AR_Canvas{k}', [(W_ - 0.0915, y + 0.57, 1.04), (W_ - 0.0915, y - 0.57, 1.04), (W_ - 0.0915, y - 0.57, 2.46), (W_ - 0.0915, y + 0.57, 2.46)], ARTM[k]))
group('Art', art)
BX, BY = 7.0, 1.3                                       # the dog's bed by the forecourt window, its bowls on a mat, its football
db = [lathe('DB_Base', [(0.0, 0.0), (0.55, 0.0), (0.58, 0.04), (0.58, 0.08), (0.0, 0.08)], (BX, BY, 0.0), DOGBED),
      tor('DB_Bolster', (BX, BY, 0.17), 0.46, 0.12, DOGBED),
      lathe('DB_Cushion', [(0.0, 0.0), (0.36, 0.0), (0.4, 0.04), (0.36, 0.08), (0.0, 0.08)], (BX, BY, 0.0815), DOGBED2)]
dbg = group('DogBed', db)
empty('DB_LiePoint', (BX, BY, 0.16), dbg)
bw = [box('BW_Mat', (BX + 1.2, 0.6, 0.004), (0.6, 0.32, 0.008), PLASTIC, bevel=0.002)]
for k in (-1, 1): bw.append(lathe(f'BW_Bowl{k}', [(0.0, 0.0), (0.075, 0.0), (0.11, 0.07), (0.105, 0.072), (0.07, 0.008), (0.0, 0.008)], (BX + 1.2 + k * 0.14, 0.6, 0.0095), BOWL))
bw.append(cyl('BW_Water', (BX + 1.06, 0.6, 0.06), 0.094, 0.004, WATER))
group('DogBowls', bw)
fbl = [sph('FB_Ball', (BX - 0.85, 1.9, 0.11), 0.11, WHITE, segs=32)]
for k in range(12):
    a, b = rng.uniform(0, 2 * math.pi), math.acos(rng.uniform(-1, 1))
    p_ = sph(f'FB_Patch{k}', (BX - 0.85 + 0.1 * math.sin(b) * math.cos(a), 1.9 + 0.1 * math.sin(b) * math.sin(a), 0.11 + 0.1 * math.cos(b)), 0.028, PLASTIC, scale=(1, 1, 0.5), segs=10)
    p_.rotation_euler = (b, 0, a); fbl.append(p_)
group('Football', fbl)
MX, MY = 2.4, 7.0                                       # the round meeting table with four chairs (north-west corner)
group('MeetTable', [cyl('MT_Top', (MX, MY, 0.735), 0.7, 0.035, WALNUT, verts=64, bevel=0.006), cyl('MT_Stem', (MX, MY, 0.3695), 0.06, 0.7, BLACKSTEEL),
                    cyl('MT_Foot', (MX, MY, 0.012), 0.38, 0.024, BLACKSTEEL, verts=48, bevel=0.005),
                    box('MT_Pad', (MX + 0.2, MY - 0.1, 0.757), (0.3, 0.21, 0.006), PAPER, rot=(0, 0, 0.4)), box('MT_Pad2', (MX - 0.25, MY + 0.2, 0.757), (0.3, 0.21, 0.006), PAPER, rot=(0, 0, -0.3))])
for k in range(4):
    a_ = k * math.pi / 2 + math.pi / 4
    group(f'MeetChair{k}', clone(VC, 'VC_', f'M{k}_'), (MX + 1.0 * math.cos(a_), MY + 1.0 * math.sin(a_), 0), a_)
SBY = 0.6                                               # the low sideboard under the forecourt windows (back 0.38 clear)
sb = [box('SB_Body', (3.2, SBY, 0.29), (3.6, 0.45, 0.5), WALNUT, bevel=0.006)]
for k in range(3): sb.append(box(f'SB_Line{k}', (1.4 + 0.9 * (k + 1), SBY + 0.225, 0.29), (0.004, 0.004, 0.46), BLACKSTEEL))
for k in range(4): sb.append(box(f'SB_Foot{k}', (1.55 + k * 1.15, SBY, 0.0195), (0.04, 0.38, 0.039), BLACKSTEEL))
for k in range(9):
    h = 0.2 + 0.06 * rng.random(); sb.append(box(f'SB_Book{k}', (1.7 + k * 0.045, SBY, 0.542 + h / 2), (0.035, 0.22, h), BOOKS[k % 4], bevel=0.002))
fx, fy, fz = 3.4, SBY, 0.68
sb += [cyl('SB_ModelStand', (fx, fy, 0.547), 0.07, 0.01, BLACKSTEEL), cyl('SB_ModelRod', (fx, fy, 0.608), 0.004, 0.116, CHROME),
       cyl('SB_ModelFuse', (fx, fy, fz), 0.02, 0.42, WHITE, axis='Y', verts=20), sph('SB_ModelNose', (fx, fy + 0.21, fz), 0.0185, WHITE, scale=(1, 1.6, 1)),
       box('SB_ModelWing', (fx, fy - 0.01, fz - 0.008), (0.40, 0.07, 0.006), WHITE), box('SB_ModelTail', (fx, fy - 0.19, fz + 0.04), (0.006, 0.05, 0.07), TAILBLUE),
       box('SB_ModelStab', (fx, fy - 0.2, fz + 0.005), (0.14, 0.035, 0.004), WHITE)]
for s_ in (-1, 1): sb.append(cyl(f'SB_ModelEngine{s_}', (fx + s_ * 0.08, fy + 0.02, fz - 0.025), 0.012, 0.05, GREYPL, axis='Y', verts=16))
sb += [box('SB_PhotoFrame', (4.6, SBY - 0.04, 0.63), (0.24, 0.02, 0.18), BLACKSTEEL, bevel=0.002, rot=(-0.15, 0, 0)),
       lathe('SB_Bowl', [(0.0, 0.0), (0.06, 0.0), (0.16, 0.08), (0.155, 0.083), (0.055, 0.006), (0.0, 0.006)], (2.4, SBY, 0.542), CERAMIC)]
group('Sideboard', sb)
PEND = []                                               # pendant lights over the desk: two linear bars on thin cables
pd = []
for k in (-1, 1):
    y = DCY + k * 0.45
    pd.append(box(f'PD_Bar{k}', (DCX, y, 2.55), (0.06, 0.8, 0.05), BLACKSTEEL, bevel=0.006))
    pd.append(box(f'PD_LED{k}', (DCX, y, 2.525), (0.04, 0.76, 0.004), LEDSTRIP)); PEND.append((DCX, y, 2.5))
    for s in (-1, 1):
        pd.append(cyl(f'PD_Cable{k}{s}', (DCX, y + s * 0.35, (2.575 + H_ - 0.02) / 2), 0.002, H_ - 0.02 - 2.575, BLACKSTEEL, verts=6))
        pd.append(cyl(f'PD_Canopy{k}{s}', (DCX, y + s * 0.35, H_ - 0.0105), 0.03, 0.02, BLACKSTEEL, bevel=0.003))
group('Pendants', pd)
# #endregion

# #region ── Finish: modifiers applied, box-projected UVs in real metres, export, points, a footprint check ──
bpy.context.view_layer.update()
meshes = [o for o in sc.objects if o.type == 'MESH']
done = set()
for o in meshes:
    if o.data.name in done: continue
    done.add(o.data.name)
    dg = bpy.context.evaluated_depsgraph_get(); ev = o.evaluated_get(dg)
    me = bpy.data.meshes.new_from_object(ev); old = o.data
    for u in [x for x in meshes if x.data == old]: u.data = me
    me.name = old.name
for o in meshes: o.modifiers.clear()
for me in {o.data for o in meshes}:
    if any(o.get('keep_uv') for o in meshes if o.data == me): continue
    tile = TILE.get(me.materials[0].name, 1.0) if me.materials else 1.0
    bm = bmesh.new(); bm.from_mesh(me); uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal; ax = max(range(3), key=lambda i: abs(n[i]))
        for l in f.loops:
            v = l.vert.co; a, b = [(1, 2), (0, 2), (0, 1)][ax]
            l[uv].uv = (v[a] / tile, v[b] / tile)
    bm.to_mesh(me); bm.free()
empties_named = {o.name: o for o in sc.objects if o.type == 'EMPTY'}
for o in meshes:
    assert o.data.materials and all(m is not None for m in o.data.materials), 'empty material slot on ' + o.name
os.makedirs(HERE, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'OwnerOffice.blend'))
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'officeowner.glb'), export_format='GLB', export_apply=False, export_cameras=False, export_lights=False)
# the points, in plan metres (x, height over the terrain plan's 0, z)
def plan(v): return [round(114 + v.x, 4), round(FY + v.z, 4), round(v.y, 4)]
pts = {n: plan(empties_named[n].matrix_world.translation) for n in empties_named if any(k in n for k in ('SitPoint', 'IK', 'LiePoint'))}
pts['OC_SwivelPivot'] = plan(empties_named['OC_Swivel'].matrix_world.translation)
# the room door (TerminalDoors, swing, D-F2-OFFICE-1-1 at x 124.5, z 9, 1.2 wide, hinged at x 123.9): its chrome handle
# 1.08 m from the hinge, 1.0 m up, 0.16 m through the leaf - a hand on each end
pts['Door_HandleIn'] = [123.9 + 1.08, FY + 1.0, 9.0 - 0.07]; pts['Door_HandleOut'] = [123.9 + 1.08, FY + 1.0, 9.0 + 0.07]
pts['DeskLamp'] = plan(Vector(DESKLAMP)); pts['DeskLampDir'] = [round(DESKLAMP_DIR[0], 4), round(DESKLAMP_DIR[2], 4), round(DESKLAMP_DIR[1], 4)]
for i, p in enumerate(PEND): pts[f'Pendant{i}'] = plan(Vector(p))
with open(os.path.join(ROOT, 'Data', 'owner-office-points.json'), 'w') as f: json.dump(pts, f, indent=1)
# footprints (plan x, z) of the pieces standing on the floor: none may overlap but what stands on or over the rug
tops = [o for o in sc.objects if o.parent is None]
def bounds(e):
    lo, hi = Vector((1e9,) * 3), Vector((-1e9,) * 3)
    for o in [e] + list(e.children_recursive):
        if o.type != 'MESH': continue
        for v in o.data.vertices:
            w = o.matrix_world @ v.co; lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    return lo, hi
B = {e.name: bounds(e) for e in tops}
fixed = {'Wardrobe': (Vector((11.61, 6.1, 0)), Vector((12.21, 7.3, 2.2))), 'WardrobeDoorsOpen': (Vector((11.0, 6.1, 0)), Vector((11.61, 7.3, 2.2))),
         'DoorSwing': (Vector((9.9, 7.8, 0)), Vector((11.1, 9.0, 2.3))), 'GlassGuard': (Vector((-1, 0, 0)), Vector((0.36, 9, 1.0))),
         'NorthWall': (Vector((0, 8.94, 0)), Vector((12.3, 10, 5.31))), 'EastWall': (Vector((12.24, 0, 0)), Vector((13, 9, 5.31))), 'SouthWall': (Vector((0, -1, 0)), Vector((12.3, 0.3, 1.0)))}
B.update(fixed)
onrug = {'Rug', 'Desk', 'OfficeChair', 'VisitorChair', 'Laptop', 'DeskLamp', 'DeskPhone', 'Radio', 'DeskThings', 'PL_Desk', 'Pendants'}
ondesk = {'Laptop', 'DeskLamp', 'DeskPhone', 'Radio', 'DeskThings', 'PL_Desk'}
bad = []
names = sorted(B)
for i, a in enumerate(names):
    for b in names[i + 1:]:
        if (a in onrug and b in onrug and 'Rug' in (a, b)) or (a in ondesk and b == 'Desk') or (b in ondesk and a == 'Desk') or (a in ondesk and b in ondesk) or (a in fixed and b in fixed): continue
        (la, ha), (lb_, hb) = B[a], B[b]
        if all(la[k] < hb[k] - 0.005 and lb_[k] < ha[k] - 0.005 for k in range(3)): bad.append(f'{a} x {b}')
print('OVERLAPS:', bad or 'none')
print('BOUNDS:', {n: [[round(c, 2) for c in B[n][0]], [round(c, 2) for c in B[n][1]]] for n in names if n not in fixed})
print('POINTS:', pts)
print('DONE')
# #endregion
