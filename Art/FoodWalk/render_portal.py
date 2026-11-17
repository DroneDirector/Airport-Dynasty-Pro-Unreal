# Food Walk entrance portal - a concept render for the owner (to-do 94, 2026-11-17): the landing's white tiles meeting the
# Food Walk's wood on a brass strip, the portal (walnut pillars with a warm LED line, a slatted wood header with a backlit
# FOOD WALK sign, planters), flush with the old tax free room's wall on the left and the glass to the gate walkway on the
# right, the Food Walk behind it (eateries with lit menu boards, tables, red chairs, plants, pendant lamps).
#   blender -b --python Art/FoodWalk/render_portal.py -- <out dir> [samples] [width height]
import bpy, math, os, sys, random
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
OUT = argv[0] if argv else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'renders')
SAMPLES = int(argv[1]) if len(argv) > 1 else 400
RW, RH = (int(argv[2]), int(argv[3])) if len(argv) > 3 else (2560, 1440)
FONT = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'Content', 'Fonts', 'Audiowide-Regular.ttf')
os.makedirs(OUT, exist_ok=True)
rnd = random.Random(7)

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

# ---------------------------------------------------------------- materials
def mat(name, col, rough=0.5, metal=0.0, emit=None, es=0.0, trans=0.0, ior=1.45, alpha=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1)
    b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    if emit: b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = es
    if trans: b.inputs['Transmission'].default_value = trans; b.inputs['IOR'].default_value = ior
    return m

def tiles_mat():
    # the landing: big polished white stone tiles (60 x 120 cm) with thin grey joints, a soft gloss
    m = bpy.data.materials.new('tiles'); m.use_nodes = True; nt = m.node_tree
    b = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / 1.2, 1 / 0.6, 1)
    br = nt.nodes.new('ShaderNodeTexBrick'); br.offset = 0.5; br.inputs['Scale'].default_value = 1.0
    br.inputs['Mortar Size'].default_value = 0.004; br.inputs['Color1'].default_value = (0.86, 0.86, 0.85, 1); br.inputs['Color2'].default_value = (0.8, 0.8, 0.79, 1)
    br.inputs['Mortar'].default_value = (0.45, 0.45, 0.45, 1)
    nz = nt.nodes.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = 3.0; nz.inputs['Detail'].default_value = 8
    mix = nt.nodes.new('ShaderNodeMixRGB'); mix.blend_type = 'MULTIPLY'; mix.inputs['Fac'].default_value = 0.12
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], br.inputs['Vector'])
    nt.links.new(br.outputs['Color'], mix.inputs['Color1']); nt.links.new(nz.outputs['Color'], mix.inputs['Color2'])
    nt.links.new(mix.outputs['Color'], b.inputs['Base Color'])
    b.inputs['Roughness'].default_value = 0.12
    return m

def wood_mat(name, dark=0.0, plank=(2.0, 0.2), rough=0.45):
    # oak planks: a stretched noise for the grain, planks set apart by the brick pattern
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree
    b = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord')
    mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / plank[0], 1 / plank[1], 1)
    br = nt.nodes.new('ShaderNodeTexBrick'); br.offset = 0.37; br.inputs['Mortar Size'].default_value = 0.006
    br.inputs['Color1'].default_value = (1, 1, 1, 1); br.inputs['Color2'].default_value = (0.82, 0.82, 0.82, 1); br.inputs['Mortar'].default_value = (0.35, 0.35, 0.35, 1)
    mp2 = nt.nodes.new('ShaderNodeMapping'); mp2.inputs['Scale'].default_value = (0.6, 14.0, 1)
    nz = nt.nodes.new('ShaderNodeTexNoise'); nz.inputs['Scale'].default_value = 4.0; nz.inputs['Detail'].default_value = 10; nz.inputs['Distortion'].default_value = 2.0
    ramp = nt.nodes.new('ShaderNodeValToRGB')
    c0 = (0.30 - dark, 0.18 - dark * 0.6, 0.10 - dark * 0.3); c1 = (0.55 - dark, 0.36 - dark * 0.7, 0.21 - dark * 0.4)
    ramp.color_ramp.elements[0].color = (*[max(0.01, v) for v in c0], 1); ramp.color_ramp.elements[1].color = (*[max(0.02, v) for v in c1], 1)
    mix = nt.nodes.new('ShaderNodeMixRGB'); mix.blend_type = 'MULTIPLY'; mix.inputs['Fac'].default_value = 1.0
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector']); nt.links.new(mp.outputs['Vector'], br.inputs['Vector'])
    nt.links.new(tc.outputs['Object'], mp2.inputs['Vector']); nt.links.new(mp2.outputs['Vector'], nz.inputs['Vector'])
    nt.links.new(nz.outputs['Fac'], ramp.inputs['Fac'])
    nt.links.new(ramp.outputs['Color'], mix.inputs['Color1']); nt.links.new(br.outputs['Color'], mix.inputs['Color2'])
    nt.links.new(mix.outputs['Color'], b.inputs['Base Color'])
    b.inputs['Roughness'].default_value = rough
    return m

M = {
    'tiles': tiles_mat(),
    'oak': wood_mat('oak', 0.0, (1.8, 0.19), 0.38),
    'walnut': wood_mat('walnut', 0.21, (2.4, 0.12), 0.3),
    'wall': mat('wall', (0.84, 0.84, 0.82), 0.6),
    'ceil': mat('ceil', (0.78, 0.78, 0.77), 0.7),
    'brass': mat('brass', (0.86, 0.66, 0.36), 0.22, 1.0),
    'led': mat('led', (1, 0.8, 0.5), 0.5, 0, (1.0, 0.72, 0.38), 18.0),
    'ledcool': mat('ledcool', (1, 1, 1), 0.5, 0, (1.0, 0.95, 0.88), 6.0),
    'sign_bg': mat('sign_bg', (0.012, 0.009, 0.007), 0.55),
    'sign_txt': mat('sign_txt', (1, 0.8, 0.5), 0.4, 0, (1.0, 0.70, 0.36), 7.0),
    'glass': mat('glass', (0.92, 0.96, 0.97), 0.02, 0, None, 0, 1.0, 1.5),
    'steel': mat('steel', (0.72, 0.73, 0.75), 0.3, 1.0),
    'black': mat('black', (0.03, 0.03, 0.03), 0.45),
    'pot': mat('pot', (0.08, 0.08, 0.085), 0.55),
    'soil': mat('soil', (0.06, 0.04, 0.03), 0.9),
    'leaf': mat('leaf', (0.05, 0.2, 0.04), 0.45),
    'stem': mat('stem', (0.18, 0.13, 0.07), 0.7),
    'leaf2': mat('leaf2', (0.11, 0.33, 0.08), 0.55),
    'red': mat('red', (0.55, 0.06, 0.04), 0.5),
    'table': mat('table', (0.85, 0.84, 0.8), 0.25),
    'counter': mat('counter', (0.12, 0.12, 0.13), 0.3),
    'quartz': mat('quartz', (0.9, 0.89, 0.86), 0.15),
}
MENU = [mat('menu%d' % i, (1, 1, 1), 0.4, 0, c, 6.0) for i, c in enumerate([(1.0, 0.55, 0.25), (0.4, 0.85, 1.0), (1.0, 0.85, 0.35), (0.55, 1.0, 0.55), (1.0, 0.45, 0.6)])]

# ---------------------------------------------------------------- helpers
def box(name, x0, x1, y0, y1, z0, z1, m, bevel=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    o = bpy.context.object; o.name = name; o.scale = (x1 - x0, y1 - y0, z1 - z0)
    bpy.ops.object.transform_apply(scale=True)
    o.data.materials.append(m)
    if bevel:
        bv = o.modifiers.new('bev', 'BEVEL'); bv.width = bevel; bv.segments = 3
    return o

def cyl(name, x, y, z0, z1, r, m, verts=48):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=z1 - z0, location=(x, y, (z0 + z1) / 2))
    o = bpy.context.object; o.name = name; o.data.materials.append(m)
    bpy.ops.object.shade_smooth()
    return o

def sphere(name, x, y, z, r, m, sx=1, sy=1, sz=1):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=r, location=(x, y, z))
    o = bpy.context.object; o.name = name; o.scale = (sx, sy, sz); o.data.materials.append(m)
    bpy.ops.object.shade_smooth()
    return o

def text(name, s, x, y, z, size, m, rot=(math.pi / 2, 0, 0), depth=0.01):
    bpy.ops.object.text_add(location=(x, y, z), rotation=rot)
    o = bpy.context.object; o.name = name; o.data.body = s; o.data.size = size; o.data.extrude = depth
    o.data.align_x = 'CENTER'; o.data.align_y = 'CENTER'
    if os.path.exists(FONT): o.data.font = bpy.data.fonts.load(FONT)
    o.data.space_character = 1.12
    o.data.materials.append(m)
    return o

def plant(x, y, h=1.0, s=1.0):
    # a tall pot with a leafy shrub (many leaf blobs)
    cyl('pot', x, y, 0, 0.8 * s, 0.42 * s, M['pot'])
    cyl('soil', x, y, 0.78 * s, 0.8 * s, 0.39 * s, M['soil'])
    leaves = []
    for st in range(7):
        a0 = st * math.tau / 7 + rnd.uniform(-0.3, 0.3); lean = rnd.uniform(0.08, 0.28)
        top = Vector((x + math.cos(a0) * lean * s, y + math.sin(a0) * lean * s, (1.5 + rnd.uniform(0, 0.8) * h) * s))
        base = Vector((x, y, 0.8 * s))
        d = top - base
        bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.012 * s, depth=d.length, location=(base + top) / 2)
        o = bpy.context.object; o.rotation_euler = d.to_track_quat('Z', 'Y').to_euler(); o.data.materials.append(M['stem'])
        for k in range(26):
            t = rnd.uniform(0.25, 1.0); p = base + d * t
            a = rnd.uniform(0, math.tau); r = rnd.uniform(0.04, 0.22) * s * (1.2 - t * 0.5)
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1, location=(p.x + math.cos(a) * r, p.y + math.sin(a) * r, p.z + rnd.uniform(-0.05, 0.05)))
            o = bpy.context.object; L = rnd.uniform(0.09, 0.15) * s
            o.scale = (L, L * 0.45, 0.012 * s)
            o.rotation_euler = (rnd.uniform(-0.9, 0.9), rnd.uniform(-0.6, 0.6), a + rnd.uniform(-0.5, 0.5))
            o.data.materials.append(M['leaf' if k % 3 else 'leaf2']); bpy.ops.object.shade_smooth()
            leaves.append(o)
    bpy.ops.object.select_all(action='DESELECT')
    for o in leaves: o.select_set(True)
    bpy.context.view_layer.objects.active = leaves[0]; bpy.ops.object.join()

# ---------------------------------------------------------------- the space (metres; y = into the Food Walk, x across, z up)
H = 5.4                 # floor to ceiling
X0, X1 = 0.0, 10.0      # the opening: the old tax free room's wall end (left) to the glass (right)
# floors: the landing (y < 0) and the Food Walk (y > 0), the brass strip under the portal
box('landing', -14, 24, -22, 0, -0.1, 0, M['tiles'])
box('foodwalk', -14, 24, 0, 30, -0.1, 0, M['oak'])
box('strip', X0, X1, -0.04, 0.04, 0, 0.004, M['brass'])
# ceilings: landing plaster with a long cove of light, Food Walk wood slats over a dark plenum with LED lines
box('ceil_land', -14, 24, -22, 0, H, H + 0.2, M['ceil'])
for k in range(5):
    box('cove', -10 + k * 0.0, 22, -18 + k * 4.0, -17.92 + k * 4.0, H - 0.02, H, M['ledcool'])
box('plenum', -14, 24, 0, 30, H + 0.4, H + 0.6, M['black'])
for k in range(130):
    xx = -10 + k * 0.25
    box('slat', xx, xx + 0.08, 0.3, 30, H - 0.25, H + 0.05, M['oak'])
for k in range(6):
    box('ledline', -10, 22, 3 + k * 4.4, 3.06 + k * 4.4, H - 0.27, H - 0.25, M['ledcool'])
# the left: the old tax free room wall (landing side) and the Food Walk's wall beyond
box('wall_left', -14, X0, -22, 0, 0, H, M['wall'])
box('land_end', -14, X1 + 16, -22.3, -22, 0, H + 3, M['wall'])
for k in range(6): box('land_door', 1.5 + k * 1.2, 2.5 + k * 1.2, -21.99, -21.95, 0, 2.4, M['steel'])
box('wall_left_fw', -14, X0 - 6, 0, 30, 0, H, M['wall'])
# the right: the glass to the gate walkway, mullions every 1.5 m, a brushed rail at 1.1 m
for yy in [y * 1.5 for y in range(-14, 21)]:
    box('mullion', X1, X1 + 0.08, yy - 0.04, yy + 0.04, 0, H, M['steel'])
box('glass', X1 + 0.02, X1 + 0.04, -22, 30, 0, H, M['glass'])
box('rail', X1 - 0.08, X1 - 0.02, -22, 30, 1.08, 1.12, M['steel'], 0.01)
# the gate walkway beyond the glass: its floor and far wall, softly lit
box('walk_floor', X1 + 0.1, X1 + 16, -22, 30, -0.1, 0, M['tiles'])
box('walk_wall', X1 + 16, X1 + 16.2, -22, 30, 0, H + 3, M['wall'])
box('walk_ceil', X1 + 0.1, X1 + 16, -22, 30, H + 3, H + 3.2, M['ceil'])

# ---------------------------------------------------------------- the portal
PW, PD = 0.6, 0.7     # pillar width and depth
for x0 in (X0, X1 - PW):
    box('pillar', x0, x0 + PW, -PD / 2, PD / 2, 0, H - 1.3, M['walnut'], 0.01)
    # the warm LED line up the face of each pillar (both faces), in a brass reveal
    for yf in (-PD / 2 - 0.005, PD / 2 - 0.01):
        box('reveal', x0 + PW / 2 - 0.05, x0 + PW / 2 + 0.05, yf, yf + 0.015, 0.12, H - 1.45, M['brass'])
        box('pled', x0 + PW / 2 - 0.02, x0 + PW / 2 + 0.02, yf - 0.002, yf + 0.017, 0.15, H - 1.48, M['led'])
    box('plinth', x0 - 0.02, x0 + PW + 0.02, -PD / 2 - 0.02, PD / 2 + 0.02, 0, 0.1, M['brass'], 0.005)
# the header: walnut slats over a dark core, the full width, 1.3 m tall
box('header_core', X0, X1, -PD / 2 + 0.05, PD / 2 - 0.05, H - 1.3, H, M['black'])
n = int((X1 - X0) / 0.09)
for k in range(n):
    xx = X0 + 0.02 + k * 0.09
    for yf in (-PD / 2, PD / 2 - 0.06):
        box('hslat', xx, xx + 0.05, yf, yf + 0.06, H - 1.3, H, M['walnut'])
# a soft cove line under the header on both sides
for yf in (-PD / 2 - 0.02, PD / 2 - 0.02):
    box('cove_h', X0 + PW, X1 - PW, yf, yf + 0.04, H - 1.34, H - 1.31, M['led'])
# the sign: a dark panel framed in brass on both faces, FOOD WALK in warm light, little halo of light behind it
for side, yf in ((-1, -PD / 2 - 0.06), (1, PD / 2 + 0.06)):
    box('sign_frame', X0 + 2.3, X1 - 2.3, yf - 0.03, yf + 0.03, H - 1.12, H - 0.22, M['brass'], 0.01)
    ya, yb = sorted((yf + side * 0.03, yf + side * 0.04))
    box('sign_bg', X0 + 2.36, X1 - 2.36, ya, yb, H - 1.06, H - 0.28, M['sign_bg'])
    t = text('sign', 'FOOD WALK', (X0 + X1) / 2, yf + side * 0.042, H - 0.67, 0.46, M['sign_txt'], (math.pi / 2, 0, 0 if side < 0 else math.pi), 0.004)
# the planters inside the opening, 7 m clear between them
plant(X0 + PW + 0.55, 0.0, 1.0, 1.0)
plant(X1 - PW - 0.55, 0.0, 1.0, 1.0)

# ---------------------------------------------------------------- the Food Walk behind it
# five eateries along the left: counters, a lit menu board each, the eatery's sign
YB = 25.0
box('far_wall', -14, 24, YB, YB + 0.3, 0, H + 0.6, M['wall'])
NAMES = ['NORDIC GRILL', 'NOODLE BAR', 'BAKERY', 'FRESH BOWL', 'BURGERS']
for i in range(5):
    x0 = -5.6 + i * 3.1; x1 = x0 + 2.8
    box('eat_back', x0, x1, YB - 0.1, YB, 0, H, M['walnut'])
    box('eat_div', x1, x1 + 0.3, YB - 3.0, YB, 0, 3.4, M['wall'])
    box('eat_counter', x0 + 0.1, x1 - 0.1, YB - 2.6, YB - 1.9, 0, 1.05, M['counter'], 0.01)
    box('eat_top', x0 + 0.05, x1 - 0.05, YB - 2.65, YB - 1.85, 1.05, 1.1, M['quartz'], 0.01)
    box('eat_menu', x0 + 0.25, x1 - 0.25, YB - 0.14, YB - 0.1, 1.9, 3.0, MENU[i % len(MENU)])
    box('eat_sign', x0 + 0.2, x1 - 0.2, YB - 3.05, YB - 2.95, 3.5, 4.0, M['sign_bg'])
    text('eat_name', NAMES[i], (x0 + x1) / 2, YB - 3.07, 3.75, 0.24, MENU[i % len(MENU)], (math.pi / 2, 0, 0), 0.004)
    for k in range(4): cyl('food', x0 + 0.5 + k * 0.6, YB - 2.25, 1.1, 1.2, 0.12, MENU[(i + k) % len(MENU)], 24)
# tables with red chairs, pendant lamps over them
for r in range(4):
    for c in range(3):
        tx, ty = 0.8 + c * 3.0, 5 + r * 5.0
        cyl('tbl_top', tx, ty, 0.74, 0.77, 0.45, M['table'])
        cyl('tbl_leg', tx, ty, 0, 0.74, 0.04, M['steel'], 24)
        cyl('tbl_base', tx, ty, 0, 0.02, 0.25, M['steel'], 32)
        for a in (0, math.pi / 2, math.pi, 3 * math.pi / 2):
            cx, cy = tx + math.cos(a) * 0.72, ty + math.sin(a) * 0.72
            box('seat', cx - 0.22, cx + 0.22, cy - 0.22, cy + 0.22, 0.45, 0.5, M['red'], 0.02)
            bx, by = tx + math.cos(a) * 0.93, ty + math.sin(a) * 0.93
            if abs(math.cos(a)) > 0.5: box('back', bx - 0.03, bx + 0.03, by - 0.21, by + 0.21, 0.5, 0.9, M['red'], 0.02)
            else: box('back', bx - 0.21, bx + 0.21, by - 0.03, by + 0.03, 0.5, 0.9, M['red'], 0.02)
            for dx in (-0.18, 0.18):
                for dy in (-0.18, 0.18): cyl('cleg', cx + dx, cy + dy, 0, 0.45, 0.015, M['steel'], 12)
        cyl('pend_cord', tx, ty, 3.2, H - 0.25, 0.006, M['black'], 8)
        cyl('pend', tx, ty, 2.95, 3.2, 0.22, M['brass'])
        cyl('pend_glow', tx, ty, 2.94, 2.95, 0.19, M['led'])
plant(8.6, 9.5, 1.2, 1.1); plant(8.6, 19.5, 1.2, 1.1)

# ---------------------------------------------------------------- lights and world
def area(name, loc, size, energy, col=(1, 0.95, 0.88), rot=(0, 0, 0)):
    d = bpy.data.lights.new(name, 'AREA'); d.size = size[0]; d.shape = 'RECTANGLE'; d.size_y = size[1]
    d.energy = energy; d.color = col
    o = bpy.data.objects.new(name, d); o.location = loc; o.rotation_euler = rot; sc.collection.objects.link(o)
for k in range(5): area('land', (5, -18 + k * 4, H - 0.05), (16, 1.2), 450)
for k in range(6): area('fw', (2, 3 + k * 4.4, H - 0.3), (20, 1.0), 520, (1, 0.9, 0.78))
area('walk', (X1 + 8, 4, H + 2.9), (14, 50), 6000, (0.9, 0.95, 1.0))
area('sign_glow', ((X0 + X1) / 2, -0.9, H - 1.5), (5, 0.3), 120, (1, 0.75, 0.42))
w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True
w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.55, 0.62, 0.72, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.35

# ---------------------------------------------------------------- render
sc.render.engine = 'CYCLES'
prefs = bpy.context.preferences.addons['cycles'].preferences
for t in ('OPTIX', 'CUDA'):
    try:
        prefs.compute_device_type = t; prefs.get_devices()
        if any(d.type == t for d in prefs.devices): break
    except Exception: pass
for d in prefs.devices: d.use = d.type != 'CPU'
sc.cycles.device = 'GPU'
sc.cycles.samples = SAMPLES; sc.cycles.use_adaptive_sampling = True; sc.cycles.adaptive_threshold = 0.01
sc.cycles.use_denoising = True
try: sc.cycles.denoiser = 'OPTIX'
except Exception: pass
sc.cycles.max_bounces = 10; sc.cycles.glossy_bounces = 6; sc.cycles.transmission_bounces = 8
sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'; sc.view_settings.exposure = -0.35
sc.render.resolution_x, sc.render.resolution_y = RW, RH
sc.render.image_settings.file_format = 'PNG'

def shoot(name, loc, target, lens=24):
    cd = bpy.data.cameras.new(name); cd.lens = lens
    cd.dof.use_dof = True; cd.dof.aperture_fstop = 8.0
    o = bpy.data.objects.new(name, cd); sc.collection.objects.link(o); o.location = loc
    d = Vector(target) - Vector(loc); o.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    cd.dof.focus_distance = d.length
    sc.camera = o
    sc.render.filepath = os.path.join(OUT, name + '.png')
    bpy.ops.render.render(write_still=True)
    print('RENDERED', sc.render.filepath)

shoot('FoodWalk-Portal-1-from-landing', (4.2, -13.5, 1.7), (5.0, 0.0, 2.6), 22)
shoot('FoodWalk-Portal-2-close', (2.0, -5.5, 1.65), (5.6, 0.0, 3.4), 18)
shoot('FoodWalk-Portal-3-from-inside', (6.0, 14.0, 1.7), (5.0, 0.0, 2.7), 22)
