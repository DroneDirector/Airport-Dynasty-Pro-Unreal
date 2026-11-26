# Your office, a render of how it could look (owner 2026-11-26): moved 4.7 m west onto the big Grand Hall window (the dead-end
# stretch of the walkway, x 114..118.7, z 0..9, goes into it; Office 2 takes the 4.7 m it leaves) - plan x 114..126.3, z 0..9,
# Floor 2 at +6.01, ceiling +11.32; the west wall the Grand Hall window (3 of its 10 equal panes, floor to ceiling), the south
# wall the forecourt windows (the Alps of the Chablais beyond), the north wall the corridor partition (plaster to 1 m, glass
# to 3.3 m, plaster above) with the door, the east wall plaster to Office 2.
# Inside: your desk facing the Grand Hall (you look out over the hall), your ergonomic chair, the visitor's chair facing you
# (the one where a called-in member of staff sits - in the game a MetaHuman at full quality, the only one in the room), a
# modern laptop (no brand), the desk lamp, phone, radio, the dog's bed with its bowls and football by the forecourt window, a
# sofa corner, the wardrobe, plants, art, rug, pendant lights. All modelled here in Blender, our own; the wood floor and the
# fabrics are the game's own textures (Art/WoodFloor, Art/Fabrics).
# Axes: X = plan x - 114 (east), Y = plan z (north, the airside), Z up from the office floor. Metres.
#   python3 Tools/render_owner_office.py [--samples=96] [--res=2560x1440] [--only=1-from-door,2-your-view,3-room]
import bpy, bmesh, math, os, sys, random
from mathutils import Vector, Matrix, Euler
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Blueprints', 'Terminal', 'Office'); os.makedirs(OUT, exist_ok=True)
arg = lambda k, d: next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--' + k + '=')), d)
SAMPLES = int(arg('samples', '96')); RES = tuple(int(v) for v in arg('res', '2560x1440').split('x'))
ONLY = arg('only', '') and arg('only', '').split(',')
bpy.ops.wm.read_factory_settings(use_empty=True); sc = bpy.context.scene
rng = random.Random(7)
W_, D_, H_ = 12.3, 9.0, 5.31            # the office: width (x), depth (y), height
HALL_Z = -6.01                           # the Grand Hall's floor

# #region ── Materials ──
def mat(name, rgb, rough=0.5, metal=0.0, emit=0.0, alpha=1.0, tex=None, tile=1.0, nrm=None, tint=None, ecol=None):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    if emit:
        b.inputs['Emission Color' if 'Emission Color' in b.inputs else 'Emission'].default_value = (*(ecol or rgb), 1)
        b.inputs['Emission Strength'].default_value = emit
    if alpha < 1:
        b.inputs['Alpha'].default_value = alpha
        if hasattr(m, 'blend_method'): m.blend_method = 'BLEND'
    if tex:
        tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / tile, 1 / tile, 1 / tile)
        nt.links.new(tc.outputs['Object'], mp.inputs['Vector'])
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(ROOT, tex)); t.projection = 'BOX'; t.projection_blend = 0.3
        nt.links.new(mp.outputs[0], t.inputs[0])
        if tint:
            mx = nt.nodes.new('ShaderNodeMixRGB'); mx.blend_type = 'MULTIPLY'; mx.inputs[0].default_value = 1; mx.inputs[2].default_value = (*tint, 1)
            nt.links.new(t.outputs[0], mx.inputs[1]); nt.links.new(mx.outputs[0], b.inputs['Base Color'])
        else: nt.links.new(t.outputs[0], b.inputs['Base Color'])
        if nrm:
            n = nt.nodes.new('ShaderNodeTexImage'); n.image = bpy.data.images.load(os.path.join(ROOT, nrm)); n.image.colorspace_settings.name = 'Non-Color'
            n.projection = 'BOX'; n.projection_blend = 0.3; nt.links.new(mp.outputs[0], n.inputs[0])
            nm = nt.nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = 0.6
            nt.links.new(n.outputs[0], nm.inputs['Color']); nt.links.new(nm.outputs[0], b.inputs['Normal'])
    return m
WOODFLOOR = mat('WoodFloor', (1, 1, 1), 0.35, tex='Art/WoodFloor/T_WoodFloor_D.png', tile=2.4, nrm='Art/WoodFloor/T_WoodFloor_N.png')
WALNUT = mat('Walnut', (1, 1, 1), 0.3, tex='Art/WoodFloor/T_WoodFloor_D.png', tile=1.6, tint=(0.55, 0.42, 0.34))
OAKLEG = mat('Oak', (1, 1, 1), 0.4, tex='Art/WoodFloor/T_WoodFloor_D.png', tile=0.8, tint=(1.3, 1.15, 0.95))
PLASTER = mat('Plaster', (0.86, 0.85, 0.82), 0.85, nrm='Art/Terminal/Detail/T_Detail_plaster_N.jpg', tex=None)
CEIL = mat('Ceiling', (0.9, 0.9, 0.89), 0.9)
GLASS = mat('Glass', (0.02, 0.03, 0.035), 0.02, alpha=0.10)
ALU = mat('Aluminium', (0.42, 0.43, 0.45), 0.3, 0.9)
BLACKSTEEL = mat('BlackSteel', (0.025, 0.025, 0.028), 0.35, 0.7)
CHROME = mat('Chrome', (0.9, 0.9, 0.92), 0.08, 1.0)
POLALU = mat('PolishedAlu', (0.8, 0.8, 0.82), 0.15, 1.0)
FABRIC = mat('ChairFabric', (1, 1, 1), 0.9, tex='Art/Fabrics/wool_suiting/albedo.png', tile=0.25, nrm='Art/Fabrics/wool_suiting/normal.png', tint=(0.9, 0.9, 1.0))
LEATHER = mat('Leather', (1, 1, 1), 0.45, tex='Art/Fabrics/leather/albedo.png', tile=0.3, nrm='Art/Fabrics/leather/normal.png', tint=(3.2, 2.2, 1.6))
SOFAFAB = mat('SofaFabric', (1, 1, 1), 0.9, tex='Art/Fabrics/knit/albedo.png', tile=0.3, nrm='Art/Fabrics/knit/normal.png', tint=(0.85, 0.9, 1.0))
RUG = mat('Rug', (1, 1, 1), 0.95, tex='Art/Fabrics/knit/albedo.png', tile=0.6, tint=(0.75, 0.8, 0.9))
PLASTIC = mat('BlackPlastic', (0.03, 0.03, 0.032), 0.45)
GREYPL = mat('GreyPlastic', (0.18, 0.18, 0.19), 0.5)
SPACEGREY = mat('SpaceGrey', (0.22, 0.225, 0.235), 0.28, 0.85)
KEYS = mat('Keys', (0.03, 0.03, 0.035), 0.55)
LEAF = mat('Leaf', (0.05, 0.2, 0.06), 0.55); STEM = mat('Stem', (0.18, 0.12, 0.06), 0.8)
POT = mat('Pot', (0.75, 0.73, 0.7), 0.6); SOIL = mat('Soil', (0.06, 0.04, 0.03), 0.95)
LAMPGLOW = mat('LampGlow', (1.0, 0.92, 0.8), 0.2, emit=8.0); LEDSTRIP = mat('LED', (1.0, 0.95, 0.88), 0.2, emit=14.0)
WHITE = mat('White', (0.85, 0.85, 0.83), 0.4); CERAMIC = mat('Ceramic', (0.92, 0.92, 0.9), 0.15)
BOWL = mat('Steel', (0.7, 0.71, 0.72), 0.2, 1.0); DOGBED = mat('DogBed', (0.35, 0.28, 0.22), 0.95); DOGBED2 = mat('DogBedIn', (0.62, 0.56, 0.48), 0.95)
PAPER = mat('Paper', (0.92, 0.91, 0.87), 0.8); NOTE = mat('Notebook', (0.08, 0.12, 0.2), 0.6)
TERRAZZO = mat('Terrazzo', (0.78, 0.77, 0.74), 0.18); COUNTER = mat('Counter', (0.88, 0.88, 0.87), 0.35)
ROOFW = mat('WaveRoof', (0.93, 0.9, 0.84), 0.7); SIGN = mat('Sign', (0.03, 0.05, 0.08), 0.4); SIGNLIT = mat('SignLit', (0.95, 0.75, 0.2), 0.3, emit=4.0)
BELT = mat('Belt', (0.05, 0.05, 0.05), 0.8); MOUNT = mat('Mountain', (0.32, 0.36, 0.34), 0.95); SNOW = mat('Snow', (0.92, 0.94, 0.97), 0.6)
ROAD = mat('Road', (0.18, 0.18, 0.19), 0.9); GRASSM = mat('Grass', (0.18, 0.3, 0.1), 0.95)
# #endregion

# #region ── Helpers ──
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
        rings.append([bm.verts.new((c[0] + r * math.cos(a), c[1] + r * math.sin(a), c[2] + z)) for r, z in prof])
    for i in range(n):
        A, B = rings[i], rings[(i + 1) % n]
        for j in range(len(prof) - 1): bm.faces.new((A[j], B[j], B[j + 1], A[j + 1]))
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); o = link(bpy.data.objects.new(name, me)); o.data.materials.append(m)
    for p in o.data.polygons: p.use_smooth = True
    return o
def group(name, objs, loc, rotz):
    e = bpy.data.objects.new(name, None); link(e)
    for o in objs: o.parent = e
    e.location = loc; e.rotation_euler = (0, 0, rotz); return e
def img_mat(name, png, emit=0.0, rough=0.5):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(png); nt.links.new(t.outputs[0], b.inputs['Base Color'])
    if emit:
        nt.links.new(t.outputs[0], b.inputs['Emission Color' if 'Emission Color' in b.inputs else 'Emission']); b.inputs['Emission Strength'].default_value = emit
    b.inputs['Roughness'].default_value = rough; return m
def uv_plane(name, corners, m):                     # a quad with UVs 0..1 (for screens and pictures), corners: ll, lr, ur, ul
    bm = bmesh.new(); uv = bm.loops.layers.uv.new(); f = bm.faces.new([bm.verts.new(v) for v in corners])
    for l, u in zip(f.loops, [(0, 0), (1, 0), (1, 1), (0, 1)]): l[uv].uv = u
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); o = link(bpy.data.objects.new(name, me)); o.data.materials.append(m); return o
# #endregion

# #region ── The room: floor, ceiling, the Grand Hall window, the forecourt windows, the corridor partition, the door ──
box('Floor', (W_ / 2, D_ / 2, -0.1), (W_, D_, 0.2), WOODFLOOR)
box('Ceiling', (W_ / 2, D_ / 2, H_ + 0.1), (W_, D_, 0.2), CEIL)
for k in range(3):                                   # recessed linear lights in the ceiling
    box(f'CeilLight{k}', (2.5 + k * 3.6, D_ / 2, H_ - 0.005), (0.12, 6.0, 0.02), LEDSTRIP)
# west: the Grand Hall window - 3 panes of 3 m, floor to ceiling, slim mullions, a low glass guard and a steel handrail
for k in range(4):
    box(f'HallMullion{k}', (0.0, k * 3.0, H_ / 2), (0.18, 0.08 if 0 < k < 3 else 0.14, H_), ALU)
box('HallWindowHead', (0.0, D_ / 2, H_ - 0.06), (0.2, D_, 0.12), ALU); box('HallWindowSill', (0.0, D_ / 2, 0.03), (0.22, D_, 0.06), ALU)
box('HallGlass', (0.0, D_ / 2, H_ / 2), (0.02, D_, H_), GLASS)
# south: the forecourt windows between plaster piers, a deep sill
box('SouthWallLow', (W_ / 2, -0.15, 0.3), (W_, 0.3, 0.6), PLASTER); box('SouthWallHigh', (W_ / 2, -0.15, (4.6 + H_) / 2), (W_, 0.3, H_ - 4.6), PLASTER)
for x in (0.0, 4.1, 8.2, 12.3):
    box(f'SouthPier{x}', (x, -0.15, 2.6), (0.5 if 0 < x < 12 else 0.25, 0.3, 4.0), PLASTER)
box('SouthGlass', (W_ / 2, -0.12, 2.6), (W_, 0.02, 4.0), GLASS); box('SouthSill', (W_ / 2, 0.02, 0.6), (W_, 0.3, 0.04), WHITE)
for x in (2.05, 6.15, 10.25): box(f'SouthMullion{x}', (x, -0.12, 2.6), (0.06, 0.1, 4.0), ALU)
# east: plaster to Office 2
box('EastWall', (W_ + 0.06, D_ / 2, H_ / 2), (0.12, D_, H_), PLASTER)
# north: the corridor partition (plaster to 1 m, glass to 3.3 m, plaster above) with the door at x 10.5
DX = 10.5
box('NorthLow', (W_ / 2, D_ + 0.06, 0.5), (W_, 0.12, 1.0), PLASTER); box('NorthHigh', (W_ / 2, D_ + 0.06, (3.3 + H_) / 2), (W_, 0.12, H_ - 3.3), PLASTER)
box('NorthGlass', (W_ / 2, D_ + 0.06, 2.15), (W_, 0.02, 2.3), GLASS)
for x in (0.0, 3.0, 6.0, 9.0, DX - 0.7, DX + 0.7, W_): box(f'NorthPost{x}', (x, D_ + 0.06, 2.15), (0.06, 0.1, 2.3), ALU)
box('DoorFrameTop', (DX, D_ + 0.06, 2.35), (1.46, 0.14, 0.08), ALU)
cut_low = bpy.data.objects['NorthLow']                # the doorway through the low plaster
bpy.ops.mesh.primitive_cube_add(location=(DX, D_ + 0.06, 1.15)); cutter = bpy.context.active_object; cutter.scale = (0.66, 0.3, 1.2)
md = cut_low.modifiers.new('cut', 'BOOLEAN'); md.object = cutter; md.operation = 'DIFFERENCE'; cutter.hide_render = True; cutter.hide_viewport = True
door = box('Door', (DX, D_ + 0.06, 1.155), (1.2, 0.05, 2.31), WALNUT, bevel=0.004)
box('DoorGlassStrip', (DX + 0.35, D_ + 0.03, 1.3), (0.12, 0.02, 1.7), GLASS)
for s in (-1, 1): cyl(f'DoorHandle{s}', (DX - 0.48, D_ + 0.06 + s * 0.05, 1.05), 0.012, 0.22, CHROME, axis='X')
box('Skirting', (W_ / 2, D_ - 0.01, 0.04), (W_, 0.02, 0.08), WHITE)
# #endregion

# #region ── The desk (walnut top, black steel sled legs, drawer pedestal), facing the Grand Hall ──
DCX, DCY = 5.2, 4.3                                   # desk centre; you sit on its east side looking west over the hall
desk = []
desk.append(box('DeskTop', (DCX, DCY, 0.735), (0.9, 2.0, 0.04), WALNUT, bevel=0.006))
for s in (-1, 1):
    y = DCY + s * 0.88
    for x in (DCX - 0.36, DCX + 0.36): desk.append(box(f'DeskPost{s}{x}', (x, y, 0.36), (0.04, 0.04, 0.72), BLACKSTEEL, bevel=0.004))
    desk.append(box(f'DeskSledTop{s}', (DCX, y, 0.70), (0.76, 0.04, 0.03), BLACKSTEEL, bevel=0.004))
    desk.append(box(f'DeskSledFoot{s}', (DCX, y, 0.015), (0.8, 0.05, 0.03), BLACKSTEEL, bevel=0.004))
desk.append(box('DeskRail', (DCX + 0.3, DCY, 0.66), (0.03, 1.72, 0.06), BLACKSTEEL))
desk.append(box('CableTray', (DCX + 0.1, DCY, 0.64), (0.2, 1.2, 0.05), BLACKSTEEL))
ped = box('Pedestal', (DCX + 0.12, DCY + 0.62, 0.31), (0.55, 0.42, 0.58), WALNUT, bevel=0.004)
for k in range(3): box(f'PedDrawerLine{k}', (DCX + 0.395, DCY + 0.62, 0.13 + k * 0.19), (0.004, 0.40, 0.006), BLACKSTEEL); box(f'PedPull{k}', (DCX + 0.4, DCY + 0.62, 0.2 + k * 0.19), (0.01, 0.18, 0.012), BLACKSTEEL)
# #endregion

# #region ── Your chair (ergonomic, 5-star polished base, mesh back, headrest, arms) and the visitor's chair facing you ──
def exec_chair(name):
    P = []
    P.append(cyl(name + 'Hub', (0, 0, 0.1), 0.055, 0.09, POLALU, bevel=0.01))
    for k in range(5):
        a = k * 2 * math.pi / 5
        leg = box(name + f'Leg{k}', (0.16 * math.cos(a), 0.16 * math.sin(a), 0.085), (0.32, 0.045, 0.04), POLALU, bevel=0.012, rot=(0, -0.08, a)); P.append(leg)
        cx, cy = 0.33 * math.cos(a), 0.33 * math.sin(a)
        P.append(box(name + f'CasterFork{k}', (cx, cy, 0.06), (0.05, 0.03, 0.04), PLASTIC, bevel=0.008, rot=(0, 0, a)))
        for s in (-1, 1):
            P.append(cyl(name + f'Wheel{k}{s}', (cx + s * 0.014 * math.cos(a + math.pi / 2), cy + s * 0.014 * math.sin(a + math.pi / 2), 0.03), 0.03, 0.014, PLASTIC,
                         rot=(math.pi / 2, 0, a + math.pi / 2), bevel=0.004))
    P.append(cyl(name + 'GasCover', (0, 0, 0.22), 0.032, 0.18, PLASTIC)); P.append(cyl(name + 'Gas', (0, 0, 0.35), 0.022, 0.1, CHROME))
    P.append(box(name + 'Mech', (0.02, 0, 0.42), (0.26, 0.22, 0.06), PLASTIC, bevel=0.01))
    P.append(box(name + 'SeatPan', (0, 0, 0.46), (0.5, 0.5, 0.03), PLASTIC, bevel=0.01))
    P.append(box(name + 'Seat', (-0.01, 0, 0.515), (0.5, 0.5, 0.085), FABRIC, bevel=0.035, segs=4))
    # back: a curved frame with a fabric-mesh panel, tilted back, and the spine bar from the seat
    P.append(box(name + 'Spine', (0.25, 0, 0.55), (0.05, 0.08, 0.2), PLASTIC, bevel=0.01, rot=(0, -0.25, 0)))
    bk = box(name + 'Back', (0.29, 0, 0.92), (0.05, 0.47, 0.62), FABRIC, bevel=0.022, segs=4, rot=(0, -0.17, 0)); P.append(bk)
    P.append(box(name + 'BackFrame', (0.315, 0, 0.92), (0.03, 0.5, 0.66), PLASTIC, bevel=0.015, segs=3, rot=(0, -0.17, 0)))
    P.append(box(name + 'Lumbar', (0.265, 0, 0.72), (0.03, 0.34, 0.09), PLASTIC, bevel=0.012, rot=(0, -0.17, 0)))
    P.append(box(name + 'HeadStem', (0.36, 0, 1.27), (0.025, 0.05, 0.14), PLASTIC, rot=(0, -0.17, 0)))
    P.append(box(name + 'Headrest', (0.37, 0, 1.36), (0.06, 0.3, 0.13), FABRIC, bevel=0.03, segs=4, rot=(0, -0.17, 0)))
    for s in (-1, 1):
        P.append(box(name + f'ArmPost{s}', (0.06, s * 0.27, 0.6), (0.05, 0.03, 0.2), PLASTIC, bevel=0.008))
        P.append(box(name + f'ArmBracket{s}', (0.1, s * 0.255, 0.5), (0.18, 0.04, 0.025), PLASTIC, bevel=0.006))
        P.append(box(name + f'ArmPad{s}', (0.03, s * 0.27, 0.7), (0.26, 0.085, 0.03), PLASTIC, bevel=0.012, segs=3))
    return P
def visitor_chair(name):                               # an upholstered shell on four splayed oak legs, cognac leather
    P = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            P.append(cyl(name + f'Leg{sx}{sy}', (sx * 0.19, sy * 0.19, 0.22), 0.018, 0.46, OAKLEG, r2=0.012, rot=(sy * -0.07, sx * 0.07, 0)))
    P.append(box(name + 'Rail', (0, 0, 0.42), (0.42, 0.42, 0.03), OAKLEG, bevel=0.006))
    P.append(box(name + 'Seat', (0, 0, 0.47), (0.5, 0.48, 0.08), LEATHER, bevel=0.03, segs=4))
    # the back: a curved shell round the back of the seat (an arc of 110 degrees), padded
    bm = bmesh.new(); prof = []
    R = 0.29; n = 16
    for i in range(n + 1):
        a = math.radians(-55 + 110 * i / n); prof.append((R * math.cos(a), R * math.sin(a)))
    th = 0.045; top, bot = 0.86, 0.5
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
    me = bpy.data.meshes.new(name + 'Back'); bm.to_mesh(me); bm.free(); bk = link(bpy.data.objects.new(name + 'Back', me)); bk.data.materials.append(LEATHER)
    bk.location = (-0.02, 0, 0); bev(bk, 0.015, 3); sd = bk.modifiers.new('S', 'SUBSURF'); sd.levels = 1; sd.render_levels = 2
    for p in bk.data.polygons: p.use_smooth = True
    P.append(bk)
    for s in (-1, 1): P.append(box(name + f'BackPost{s}', (0.2, s * 0.2, 0.55), (0.03, 0.03, 0.14), OAKLEG, bevel=0.005))
    return P
group('YourChair', exec_chair('YC_'), (DCX + 0.95, DCY - 0.1, 0), math.radians(8))          # faces -x: the desk and the hall
group('VisitorChair', visitor_chair('VC_'), (DCX - 0.95, DCY, 0), math.pi)                   # faces +x: you
# #endregion

# #region ── On the desk: the laptop (no brand), lamp, phone, radio, notebook, mug, a small plant ──
from PIL import Image, ImageDraw, ImageFont
def screen_png():
    im = Image.new('RGB', (1600, 1000), (12, 22, 38)); d = ImageDraw.Draw(im)
    FB = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'; FR = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
    d.rectangle((0, 0, 1600, 70), fill=(20, 40, 70)); d.text((30, 18), 'CHABLAIS AIRPORT  ·  Operations', font=ImageFont.truetype(FB, 32), fill=(230, 240, 255))
    for i, (lab, val, col) in enumerate((('Passengers today', '18 420', (110, 230, 140)), ('On-time', '94 %', (120, 200, 255)), ('Revenue', 'CHF 412 k', (255, 196, 70)))):
        x = 30 + i * 520; d.rounded_rectangle((x, 100, x + 490, 260), 16, fill=(22, 38, 62)); d.text((x + 24, 120), lab, font=ImageFont.truetype(FR, 28), fill=(150, 175, 210))
        d.text((x + 24, 165), val, font=ImageFont.truetype(FB, 60), fill=col)
    d.rounded_rectangle((30, 290, 1010, 960), 16, fill=(22, 38, 62)); d.text((54, 305), 'Departures', font=ImageFont.truetype(FB, 30), fill=(230, 240, 255))
    rows = [('08:40', 'LX 1640', 'Zurich', 'Gate 3', 'Boarding'), ('09:05', 'EZS 1452', 'London', 'R4', 'Bus'), ('09:20', 'LH 5730', 'Frankfurt', 'Gate 1', 'On time'),
            ('09:55', 'AF 1143', 'Paris', 'Gate 11', 'On time'), ('10:10', 'TP 943', 'Lisbon', 'R6', 'Delayed'), ('10:35', 'SN 2852', 'Brussels', 'Gate 2', 'On time')]
    for i, r in enumerate(rows):
        y = 360 + i * 95; d.line((54, y - 12, 990, y - 12), fill=(40, 60, 90), width=2)
        for j, (t, x) in enumerate(zip(r, (54, 190, 370, 610, 790))):
            col = (255, 196, 70) if t in ('Delayed', 'Bus') else (110, 230, 140) if t == 'Boarding' else (225, 235, 250)
            d.text((x, y + 10), t, font=ImageFont.truetype(FB if j == 1 else FR, 32), fill=col)
    d.rounded_rectangle((1040, 290, 1570, 960), 16, fill=(22, 38, 62)); d.text((1064, 305), 'Staff', font=ImageFont.truetype(FB, 30), fill=(230, 240, 255))
    for i, (t, v) in enumerate((('On shift', '212'), ('Apron buses', '3 / 3'), ('Mood', 'Good'), ('Messages', '2 new'))):
        d.text((1064, 380 + i * 130), t, font=ImageFont.truetype(FR, 30), fill=(150, 175, 210)); d.text((1064, 420 + i * 130), v, font=ImageFont.truetype(FB, 44), fill=(230, 240, 255))
    p = os.path.join(OUT, '_laptop_screen.png'); im.save(p); return p
SCR = img_mat('LaptopScreen', screen_png(), emit=1.6, rough=0.15)
LX, LY = DCX + 0.12, DCY - 0.05                       # the laptop, facing you (+x side)
lap = []
lap.append(box('LaptopBase', (LX, LY, 0.763), (0.215, 0.31, 0.014), SPACEGREY, bevel=0.004))
for r in range(5):
    for k in range(13):
        lap.append(box(f'Key{r}_{k}', (LX - 0.045 + r * 0.019, LY - 0.12 + k * 0.02, 0.7705), (0.016, 0.017, 0.002), KEYS))
lap.append(box('Trackpad', (LX + 0.065, LY, 0.7703), (0.065, 0.11, 0.0008), mat('Trackpad', (0.3, 0.305, 0.315), 0.25, 0.6)))
hinge = Vector((LX - 0.1075, LY, 0.77)); ang = math.radians(108)
lid = box('LaptopLid', (0, 0, 0), (0.006, 0.31, 0.215), SPACEGREY, bevel=0.003)
lid.location = hinge + Vector((-math.cos(ang - math.pi / 2) * 0.0 - 0.003, 0, 0)) + Vector((0, 0, 0)); lid.rotation_euler = (0, -(ang - math.pi / 2), 0)
lid.location = hinge + Vector((-0.1075 * math.sin(ang - math.pi / 2) - 0.003, 0, 0.1075 * math.cos(ang - math.pi / 2)))
t_ = ang - math.pi / 2; ux, uz = -math.sin(t_), math.cos(t_)          # up along the lid
cx, cz = hinge.x + ux * 0.1075 + 0.0035 * math.cos(t_), hinge.z + uz * 0.1075 + 0.0035 * math.sin(t_)
def lidpt(u, v):                                       # u across (y), v up the lid from its middle
    return (cx + ux * v + 0.0002, LY + u, cz + uz * v)
uv_plane('LaptopScreen', [lidpt(-0.145, -0.095), lidpt(0.145, -0.095), lidpt(0.145, 0.095), lidpt(-0.145, 0.095)][::-1] if False else
         [lidpt(-0.145, -0.095), lidpt(0.145, -0.095), lidpt(0.145, 0.095), lidpt(-0.145, 0.095)], SCR)   # u from your left to right
# desk lamp: weighted base, two arms, a shade with its LED
lb = (DCX - 0.25, DCY + 0.78, 0.755)
cyl('LampBase', lb, 0.08, 0.025, BLACKSTEEL, bevel=0.006)
a1 = cyl('LampArm1', (lb[0] + 0.05, lb[1], lb[2] + 0.2), 0.008, 0.42, BLACKSTEEL, rot=(0, math.radians(15), 0))
a2 = cyl('LampArm2', (lb[0] + 0.2, lb[1] - 0.05, lb[2] + 0.42), 0.008, 0.36, BLACKSTEEL, rot=(math.radians(18), math.radians(70), 0))
cyl('LampShade', (lb[0] + 0.36, lb[1] - 0.12, lb[2] + 0.45), 0.065, 0.12, BLACKSTEEL, r2=0.03, rot=(0, math.radians(160), 0))
cyl('LampLED', (lb[0] + 0.375, lb[1] - 0.12, lb[2] + 0.395), 0.05, 0.004, LAMPGLOW, rot=(0, math.radians(160), 0))
# desk phone
box('PhoneBody', (DCX + 0.15, DCY - 0.72, 0.775), (0.2, 0.22, 0.045), GREYPL, bevel=0.01, rot=(0, -0.15, 0.3))
box('PhoneHandset', (DCX + 0.13, DCY - 0.79, 0.808), (0.05, 0.2, 0.03), PLASTIC, bevel=0.012, rot=(0, -0.15, 0.3))
box('PhoneScreen', (DCX + 0.18, DCY - 0.66, 0.799), (0.07, 0.05, 0.002), mat('PhoneLCD', (0.3, 0.5, 0.6), 0.2, emit=0.6), rot=(0, -0.15, 0.3))
# the radio by the computer (owner 2026-11-12): wood case, speaker grille, dial
box('Radio', (DCX - 0.22, DCY - 0.6, 0.83), (0.12, 0.26, 0.15), WALNUT, bevel=0.01)
box('RadioGrille', (DCX - 0.159, DCY - 0.66, 0.83), (0.004, 0.11, 0.11), mat('Grille', (0.12, 0.12, 0.12), 0.7))
cyl('RadioDial', (DCX - 0.158, DCY - 0.53, 0.84), 0.022, 0.012, POLALU, axis='X', bevel=0.003)
box('RadioWindow', (DCX - 0.159, DCY - 0.53, 0.885), (0.003, 0.08, 0.02), mat('RadioLit', (1.0, 0.7, 0.3), 0.3, emit=2.0))
# notebook and pen, a mug of coffee, a small plant
box('Notebook', (DCX + 0.2, DCY + 0.32, 0.759), (0.21, 0.15, 0.014), NOTE, bevel=0.003, rot=(0, 0, 0.12))
cyl('Pen', (DCX + 0.2, DCY + 0.45, 0.762), 0.004, 0.14, CHROME, rot=(math.pi / 2, 0, 0.4))
lathe('Mug', [(0.0, 0.0), (0.04, 0.0), (0.042, 0.005), (0.042, 0.095), (0.037, 0.095), (0.037, 0.008), (0.0, 0.008)], (DCX + 0.3, DCY - 0.38, 0.755), CERAMIC)
cyl('Coffee', (DCX + 0.3, DCY - 0.38, 0.84), 0.036, 0.004, mat('Coffee', (0.08, 0.04, 0.02), 0.1))
tor('MugHandle', (DCX + 0.344, DCY - 0.38, 0.805), 0.022, 0.006, CERAMIC, rot=(math.pi / 2, 0, 0))
def plant(name, c, pot_r, pot_h, h, nleaves, leaf_len, spread):
    lathe(name + 'Pot', [(0.0, 0.0), (pot_r * 0.8, 0.0), (pot_r, pot_h), (pot_r * 0.92, pot_h), (pot_r * 0.92 - 0.005, pot_h * 0.9), (0.0, pot_h * 0.9)], c, POT)
    cyl(name + 'Soil', (c[0], c[1], c[2] + pot_h * 0.88), pot_r * 0.9, 0.01, SOIL)
    for i in range(nleaves):
        a = rng.uniform(0, 2 * math.pi); t = rng.uniform(0.25, 1.0)
        z = c[2] + pot_h + h * t; r = spread * (0.3 + 0.7 * rng.random())
        cyl(name + f'Stem{i}', ((c[0] + r * 0.5 * math.cos(a)), c[1] + r * 0.5 * math.sin(a), (c[2] + pot_h + z) / 2), 0.004 + h * 0.003, z - c[2] - pot_h, STEM,
            rot=(r * 0.6 * math.sin(a) / max(0.1, h), -r * 0.6 * math.cos(a) / max(0.1, h), 0), verts=8)
        lf = sph(name + f'Leaf{i}', (c[0] + r * math.cos(a), c[1] + r * math.sin(a), z), leaf_len / 2, LEAF, scale=(1.0, 0.42, 0.06), segs=12)
        lf.rotation_euler = (rng.uniform(-0.4, 0.4), rng.uniform(-0.7, 0.2), a)
plant('DeskPlant', (DCX - 0.3, DCY - 0.85, 0.755), 0.06, 0.09, 0.18, 14, 0.07, 0.1)
plant('BigPlant', (0.75, 8.1, 0.0), 0.28, 0.5, 1.6, 46, 0.32, 0.55)
plant('CornerPlant', (11.6, 0.8, 0.0), 0.22, 0.45, 1.1, 34, 0.26, 0.42)
# #endregion

# #region ── The rest of the room: rug, sofa corner, wardrobe, art, the dog's bed with its bowls and football, pendants ──
box('Rug', (DCX, DCY, 0.006), (3.4, 2.8, 0.012), RUG, bevel=0.004)
# sofa against the east wall, a round coffee table
SX = W_ - 0.55   # (the coffee table stands between the sofa and the armchair)
box('SofaBase', (SX, 4.3, 0.22), (0.9, 2.3, 0.3), SOFAFAB, bevel=0.03); box('SofaBack', (SX + 0.33, 4.3, 0.6), (0.24, 2.3, 0.55), SOFAFAB, bevel=0.06, segs=4)
for s in (-1, 1): box(f'SofaArm{s}', (SX, 4.3 + s * 1.1, 0.42), (0.9, 0.2, 0.42), SOFAFAB, bevel=0.06, segs=4)
for k in (-1, 1): box(f'SofaCushion{k}', (SX - 0.07, 4.3 + k * 0.5, 0.42), (0.72, 0.98, 0.14), SOFAFAB, bevel=0.05, segs=4)
for k in (-1, 1): box(f'SofaBackCushion{k}', (SX + 0.2, 4.3 + k * 0.5, 0.68), (0.2, 0.96, 0.42), SOFAFAB, bevel=0.07, segs=4, rot=(0, -0.15, 0))
for s in (-1, 1):
    for t in (-1, 1): cyl(f'SofaFoot{s}{t}', (SX + s * 0.35, 4.3 + t * 1.05, 0.035), 0.02, 0.07, OAKLEG)
cyl('CoffeeTable', (SX - 1.3, 4.3, 0.42), 0.45, 0.03, WALNUT, verts=64, bevel=0.006); cyl('CoffeeTableBase', (SX - 1.3, 4.3, 0.2), 0.05, 0.4, BLACKSTEEL); cyl('CoffeeTableFoot', (SX - 1.3, 4.3, 0.01), 0.25, 0.02, BLACKSTEEL, bevel=0.004)
box('Magazine', (SX - 1.25, 4.2, 0.44), (0.22, 0.3, 0.006), PAPER, rot=(0, 0, 0.3))
# the wardrobe (owner 2026-11-12: in every office), against the east wall by the corridor
box('Wardrobe', (W_ - 0.31, 7.9, 1.1), (0.6, 1.6, 2.2), WALNUT, bevel=0.006)
box('WardrobeSplit', (W_ - 0.61, 7.9, 1.1), (0.004, 0.006, 2.16), BLACKSTEEL)
for s in (-1, 1): box(f'WardrobePull{s}', (W_ - 0.62, 7.9 + s * 0.06, 1.1), (0.015, 0.015, 0.4), BLACKSTEEL)
# art on the east wall over the sofa: two of our own abstract canvases
def art_png(seed, w, h):
    r = random.Random(seed); im = Image.new('RGB', (w, h), (232, 226, 214)); d = ImageDraw.Draw(im)
    pal = [(28, 52, 92), (196, 120, 52), (220, 186, 120), (90, 120, 110), (180, 70, 50)]
    for i in range(9):
        x, y = r.randint(-100, w), r.randint(-100, h); s = r.randint(120, 420)
        d.ellipse((x, y, x + s, y + s * r.uniform(0.6, 1.4)), fill=pal[i % len(pal)]) if i % 2 else d.rectangle((x, y, x + s, y + s // 3), fill=pal[i % len(pal)])
    p = os.path.join(OUT, f'_art{seed}.png'); im.save(p); return p
for k, (y, seed) in enumerate(((3.55, 11), (5.05, 23))):
    box(f'ArtFrame{k}', (W_ - 0.02, y, 1.75), (0.03, 1.2, 1.5), BLACKSTEEL)
    uv_plane(f'Art{k}', [(W_ - 0.04, y + 0.57, 1.04), (W_ - 0.04, y - 0.57, 1.04), (W_ - 0.04, y - 0.57, 2.46), (W_ - 0.04, y + 0.57, 2.46)], img_mat(f'ArtM{k}', art_png(seed, 900, 1100), rough=0.8))
# the dog's bed by the forecourt window, its bowls on a mat, its football
BX, BY = 7.0, 1.3
lathe('DogBedBase', [(0.0, 0.0), (0.55, 0.0), (0.58, 0.04), (0.58, 0.08), (0.0, 0.08)], (BX, BY, 0.0), DOGBED)
tor('DogBedBolster', (BX, BY, 0.17), 0.46, 0.12, DOGBED, rot=(0, 0, 0))
lathe('DogBedCushion', [(0.0, 0.08), (0.36, 0.08), (0.4, 0.12), (0.36, 0.16), (0.0, 0.16)], (BX, BY, 0.0), DOGBED2)
box('BowlMat', (BX + 1.2, 0.6, 0.004), (0.6, 0.32, 0.008), PLASTIC, bevel=0.002)
for k in (-1, 1): lathe(f'Bowl{k}', [(0.0, 0.0), (0.075, 0.0), (0.11, 0.07), (0.105, 0.072), (0.07, 0.008), (0.0, 0.008)], (BX + 1.2 + k * 0.14, 0.6, 0.008), BOWL)
cyl('Water', (BX + 1.06, 0.6, 0.06), 0.095, 0.004, mat('Water', (0.6, 0.7, 0.75), 0.02, alpha=0.4))
fb = sph('Football', (BX - 0.85, 1.9, 0.11), 0.11, WHITE, segs=32)
for k in range(12):                                   # its black patches
    a, b = rng.uniform(0, 2 * math.pi), math.acos(rng.uniform(-1, 1))
    sph(f'FootballPatch{k}', (BX - 0.85 + 0.1 * math.sin(b) * math.cos(a), 1.9 + 0.1 * math.sin(b) * math.sin(a), 0.11 + 0.1 * math.cos(b)), 0.028, PLASTIC, scale=(1, 1, 0.5), segs=10).rotation_euler = (b, 0, a)
# a round meeting table with four chairs by the Grand Hall window (north-west corner)
MX, MY = 2.4, 7.0
cyl('MeetTable', (MX, MY, 0.735), 0.7, 0.035, WALNUT, verts=64, bevel=0.006); cyl('MeetStem', (MX, MY, 0.37), 0.06, 0.7, BLACKSTEEL)
cyl('MeetFoot', (MX, MY, 0.012), 0.38, 0.024, BLACKSTEEL, verts=48, bevel=0.005)
for k in range(4):
    a_ = k * math.pi / 2 + math.pi / 4
    group(f'MeetChair{k}', visitor_chair(f'MC{k}_'), (MX + 1.0 * math.cos(a_), MY + 1.0 * math.sin(a_), 0), a_)
box('MeetPad', (MX + 0.2, MY - 0.1, 0.756), (0.3, 0.21, 0.006), PAPER, rot=(0, 0, 0.4)); box('MeetPad2', (MX - 0.25, MY + 0.2, 0.756), (0.3, 0.21, 0.006), PAPER, rot=(0, 0, -0.3))
# a low sideboard under the forecourt windows: books, a model aircraft (generic twin-jet), a framed photo, a bowl
box('Sideboard', (3.2, 0.42, 0.29), (3.6, 0.45, 0.5), WALNUT, bevel=0.006)
for k in range(4): box(f'SideboardLine{k}', (1.4 + 0.9 * (k + 1) - 0.0, 0.645, 0.29), (0.004, 0.004, 0.46), BLACKSTEEL) if k < 3 else None
for k in range(4): box(f'SideboardFoot{k}', (1.55 + k * 1.15, 0.42, 0.02), (0.04, 0.38, 0.04), BLACKSTEEL)
for k in range(9):
    h = 0.2 + 0.06 * rng.random(); box(f'Book{k}', (1.7 + k * 0.045, 0.42, 0.54 + h / 2), (0.035, 0.22, h), mat(f'BookM{k}', [(0.5, 0.1, 0.08), (0.1, 0.2, 0.4), (0.85, 0.8, 0.7), (0.15, 0.3, 0.2)][k % 4], 0.6))
fx, fy, fz = 3.4, 0.42, 0.68                           # the model aircraft on a stand
cyl('ModelStand', (fx, fy, 0.555), 0.07, 0.01, BLACKSTEEL); cyl('ModelRod', (fx, fy, 0.61), 0.004, 0.1, CHROME)
cyl('ModelFuse', (fx, fy, fz), 0.02, 0.42, WHITE, axis='Y', verts=20); sph('ModelNose', (fx, fy + 0.21, fz), 0.02, WHITE, scale=(1, 1.6, 1))
box('ModelWing', (fx, fy - 0.01, fz - 0.008), (0.40, 0.07, 0.006), WHITE, rot=(0, 0, 0)); box('ModelTail', (fx, fy - 0.19, fz + 0.04), (0.006, 0.05, 0.07), mat('TailBlue', (0.05, 0.2, 0.5), 0.4))
box('ModelStab', (fx, fy - 0.2, fz), (0.14, 0.035, 0.004), WHITE)
for s_ in (-1, 1): cyl(f'ModelEngine{s_}', (fx + s_ * 0.08, fy + 0.02, fz - 0.025), 0.012, 0.05, GREYPL, axis='Y', verts=16)
box('PhotoFrame', (4.6, 0.38, 0.66), (0.24, 0.02, 0.18), BLACKSTEEL, rot=(-0.15, 0, 0))
lathe('Bowl', [(0.0, 0.0), (0.06, 0.0), (0.16, 0.08), (0.155, 0.083), (0.055, 0.006), (0.0, 0.006)], (2.4, 0.42, 0.54), CERAMIC)
# an armchair facing the sofa across the coffee table
AX = W_ - 3.1
box('ArmchairBase', (AX, 4.3, 0.22), (0.85, 0.85, 0.3), SOFAFAB, bevel=0.03); box('ArmchairBack', (AX - 0.33, 4.3, 0.6), (0.22, 0.85, 0.55), SOFAFAB, bevel=0.06, segs=4)
for s_ in (-1, 1): box(f'ArmchairArm{s_}', (AX, 4.3 + s_ * 0.38, 0.42), (0.85, 0.18, 0.4), SOFAFAB, bevel=0.06, segs=4)
box('ArmchairCushion', (AX + 0.05, 4.3, 0.42), (0.7, 0.58, 0.14), SOFAFAB, bevel=0.05, segs=4)
for s_ in (-1, 1):
    for t in (-1, 1): cyl(f'ArmchairFoot{s_}{t}', (AX + s_ * 0.35, 4.3 + t * 0.35, 0.035), 0.02, 0.07, OAKLEG)
# pendant lights over the desk: two linear bars on thin cables
for k in (-1, 1):
    y = DCY + k * 0.45
    box(f'Pendant{k}', (DCX, y, 2.55), (0.06, 0.8, 0.05), BLACKSTEEL, bevel=0.006); box(f'PendantLED{k}', (DCX, y, 2.523), (0.04, 0.76, 0.004), LEDSTRIP)
    for s in (-1, 1): cyl(f'PendantCable{k}{s}', (DCX, y + s * 0.35, (2.58 + H_) / 2), 0.002, H_ - 2.58, BLACKSTEEL, verts=6)
# #endregion

# #region ── Beyond the glass: the Grand Hall below, and the forecourt with the Alps ──
HX0 = -72.0
box('HallFloor', ((HX0) / 2, 15, HALL_Z - 0.1), (-HX0, 40, 0.2), TERRAZZO)
box('HallBackWall', (HX0 - 0.2, 15, HALL_Z + 9), (0.4, 40, 18), PLASTER)
box('HallNorthWall', (HX0 / 2, 30.2, HALL_Z + 9), (-HX0, 0.4, 18), PLASTER)
for k in range(10): box(f'HallShop{k}', (HX0 + 4 + k * 6.6, 29.95, HALL_Z + 2.0), (5.8, 0.1, 3.6), mat(f'Shop{k}', (0.9, 0.85, 0.7) if k % 2 else (0.25, 0.3, 0.38), 0.3, emit=0.6))
box('HallFacadeGlass', (HX0 / 2, -0.25, HALL_Z + 8), (-HX0, 0.04, 16), GLASS)
for k in range(13): box(f'HallFacadeMullion{k}', (HX0 + k * 6, -0.25, HALL_Z + 8), (0.15, 0.25, 16), ALU)
for i in range(4):                                    # check-in islands: desks both sides, belts, signs overhead
    x = -12 - i * 14
    box(f'Island{i}', (x, 15, HALL_Z + 0.5), (2.4, 20, 1.0), COUNTER, bevel=0.03)
    box(f'IslandBelt{i}', (x, 15, HALL_Z + 0.5), (0.7, 20.2, 1.02), BELT)
    for k in range(8):
        for s in (-1, 1): box(f'Desk{i}_{k}{s}', (x + s * 1.9, 6.5 + k * 2.4, HALL_Z + 0.55), (0.9, 1.6, 1.1), COUNTER, bevel=0.02)
    box(f'IslandSign{i}', (x, 15, HALL_Z + 5.2), (0.3, 18, 0.9), SIGN, bevel=0.02)
    for s in (-1, 1): box(f'IslandSignLit{i}{s}', (x + s * 0.16, 15, HALL_Z + 5.2), (0.01, 17, 0.6), SIGNLIT)
for i in range(3):                                    # tree columns holding the wave roof
    x = -18 - i * 21
    for y in (8.0, 22.0):
        cyl(f'Trunk{i}{y}', (x, y, HALL_Z + 6), 0.45, 12, WHITE, verts=24)
        for k in range(4):
            a = k * math.pi / 2 + math.pi / 4
            cyl(f'Arm{i}{y}{k}', (x + 2.4 * math.cos(a), y + 2.4 * math.sin(a), HALL_Z + 13.5), 0.18, 6.0, WHITE, rot=(math.radians(-50) * math.sin(a), math.radians(50) * math.cos(a), 0), verts=12)
bm = bmesh.new(); NX, NY = 72, 12                     # the wave roof: a ceiling of light timber slats over the hall
grid = [[bm.verts.new((HX0 + (-HX0 + 2) * i / NX, -1 + 32 * j / NY, HALL_Z + 17.0 + 1.6 * math.sin(i / NX * 2 * math.pi * 2.5))) for j in range(NY + 1)] for i in range(NX + 1)]
for i in range(NX):
    for j in range(NY): bm.faces.new((grid[i][j], grid[i][j + 1], grid[i + 1][j + 1], grid[i + 1][j]))
me = bpy.data.meshes.new('WaveRoof'); bm.to_mesh(me); bm.free(); wr = link(bpy.data.objects.new('WaveRoof', me)); wr.data.materials.append(ROOFW)
for p in wr.data.polygons: p.use_smooth = True
for k in range(6): box(f'Skylight{k}', (HX0 + 6 + k * 12, 15, HALL_Z + 16.2 + 1.6 * math.sin((6 + k * 12) / -HX0 * 2 * math.pi * 2.5)), (1.2, 30, 0.05), mat(f'Sky{k}', (0.85, 0.9, 1.0), 0.2, emit=3.0))
# the forecourt: road, kerb, grass, and the mountains of the Chablais beyond
box('Forecourt', (0, -60, HALL_Z - 0.1), (600, 120, 0.2), ROAD); box('Verge', (0, -300, HALL_Z - 0.15), (2000, 360, 0.2), GRASSM)
bm = bmesh.new(); NX_, NY_ = 360, 120                # the Alps of the Chablais: a terrain 4-9 km off, snow on the tops
def hgt(x, y):
    d = (-y - 4000) / 5000.0
    base = 300 + 1700 * min(1.0, max(0.0, d + 0.35))
    return base * (0.55 + 0.25 * math.sin(x / 900 + 0.3 * math.sin(y / 700)) + 0.14 * math.sin(x / 380 + y / 450) + 0.05 * math.sin(x / 170 - y / 210))
V = [[bm.verts.new((-9000 + 18000 * i / NX_, -3800 - 6000 * j / NY_, hgt(-9000 + 18000 * i / NX_, -3800 - 6000 * j / NY_))) for j in range(NY_ + 1)] for i in range(NX_ + 1)]
for i in range(NX_):
    for j in range(NY_): bm.faces.new((V[i][j], V[i + 1][j], V[i + 1][j + 1], V[i][j + 1]))
me = bpy.data.meshes.new('Alps'); bm.to_mesh(me); bm.free(); alps = link(bpy.data.objects.new('Alps', me))
for p_ in alps.data.polygons: p_.use_smooth = True
am = bpy.data.materials.new('AlpsMat'); am.use_nodes = True; nt = am.node_tree; b = nt.nodes['Principled BSDF']
geo = nt.nodes.new('ShaderNodeNewGeometry'); sep = nt.nodes.new('ShaderNodeSeparateXYZ'); ramp = nt.nodes.new('ShaderNodeValToRGB')
mr = nt.nodes.new('ShaderNodeMapRange'); mr.inputs['From Min'].default_value = 300; mr.inputs['From Max'].default_value = 1900
nt.links.new(geo.outputs['Position'], sep.inputs[0]); nt.links.new(sep.outputs['Z'], mr.inputs['Value']); nt.links.new(mr.outputs[0], ramp.inputs[0])
ramp.color_ramp.elements[0].color = (0.07, 0.12, 0.06, 1); ramp.color_ramp.elements[1].position = 0.62; ramp.color_ramp.elements[1].color = (0.22, 0.23, 0.22, 1)
e = ramp.color_ramp.elements.new(0.75); e.color = (0.9, 0.92, 0.96, 1)
hz = nt.nodes.new('ShaderNodeMapRange'); hz.inputs['From Min'].default_value = -4000; hz.inputs['From Max'].default_value = -9500
nt.links.new(sep.outputs['Y'], hz.inputs['Value']); mixh = nt.nodes.new('ShaderNodeMixRGB'); mixh.inputs[2].default_value = (0.62, 0.7, 0.8, 1)
nt.links.new(hz.outputs[0], mixh.inputs[0]); nt.links.new(ramp.outputs[0], mixh.inputs[1])
nt.links.new(mixh.outputs[0], b.inputs['Base Color']); b.inputs['Roughness'].default_value = 0.9; alps.data.materials.append(am)
box('Town', (0, -900, HALL_Z - 0.12), (6000, 1200, 0.2), mat('Fields', (0.12, 0.2, 0.08), 0.95))
# #endregion

# #region ── Light, sky, cameras, render ──
w = bpy.data.worlds.new('Sky'); w.use_nodes = True; sc.world = w; nt = w.node_tree; bg = nt.nodes['Background']
tcw = nt.nodes.new('ShaderNodeTexCoord'); sep = nt.nodes.new('ShaderNodeSeparateXYZ'); ramp = nt.nodes.new('ShaderNodeValToRGB')
nt.links.new(tcw.outputs['Generated'], sep.inputs[0]); nt.links.new(sep.outputs['Z'], ramp.inputs[0])
ramp.color_ramp.elements[0].color = (0.85, 0.9, 0.97, 1); ramp.color_ramp.elements[1].color = (0.32, 0.55, 0.92, 1)
nt.links.new(ramp.outputs[0], bg.inputs['Color']); bg.inputs['Strength'].default_value = 1.2
sd = bpy.data.lights.new('Sun', 'SUN'); sd.energy = 3.5; sd.angle = math.radians(1.0)
so = link(bpy.data.objects.new('Sun', sd)); so.rotation_euler = (math.radians(52), 0, math.radians(200))
for k, (x, y) in enumerate(((DCX, DCY), (9.5, 4.3), (3.0, 7.5))):         # the ceiling lights' glow into the room
    ld = bpy.data.lights.new(f'Room{k}', 'AREA'); ld.shape = 'RECTANGLE'; ld.size = 1.2; ld.size_y = 4.0; ld.energy = 900; ld.color = (1.0, 0.94, 0.86)
    lo = link(bpy.data.objects.new(f'Room{k}', ld)); lo.location = (x, y, H_ - 0.05)
ld = bpy.data.lights.new('HallFill', 'AREA'); ld.size = 60; ld.energy = 60000; lo = link(bpy.data.objects.new('HallFill', ld)); lo.location = (-36, 15, HALL_Z + 15)
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
sc.cycles.max_bounces = 6; sc.render.resolution_x, sc.render.resolution_y = RES
sc.view_settings.view_transform = 'AgX' if 'AgX' in [i.identifier for i in sc.view_settings.bl_rna.properties['view_transform'].enum_items] else 'Filmic'
sc.view_settings.exposure = -0.35
cam_d = bpy.data.cameras.new('Cam'); cam = link(bpy.data.objects.new('Cam', cam_d)); sc.camera = cam; cam_d.clip_end = 10000
VIEWS = [
    ('1-from-door', (10.4, 8.3, 1.7), (2.6, 3.8, 1.0), 20),
    ('2-your-view', (6.12, 4.2, 1.22), (2.0, 4.3, 0.85), 24),
    ('3-room', (0.9, 1.3, 1.85), (9.0, 6.0, 0.7), 18),
]
for name, loc, at, lens in VIEWS:
    if ONLY and name not in ONLY: continue
    cam.location = loc; cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cam_d.lens = lens
    sc.render.filepath = os.path.join(OUT, f'YourOffice-{name}.png'); bpy.ops.render.render(write_still=True); print('rendered', name, flush=True)
# #endregion
