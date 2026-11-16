# Baggage claim concept renders (to-do 72, owner 2026-11-16): the arrivals wing's claim hall from the blueprint
# (docs/Baggage-Claim-Blueprint.png), to its exact sizes, textured, rendered from four cameras whose from/to points are
# written to Art/BaggageClaim/cameras.json (so the real model is built to match). Not the game model - a look to agree.
# Plan metres: Blender X = plan x (along the building), Blender Y = plan z (north, airside), Z up.
#   claim hall   x -80..0 (flush with the gates' west end; the museum hall removed, owner 2026-11-16), y 16..52, ceiling 5.4 m
#                (the Floor 2 arrivals corridor over y 40..52)
#   belts        3 flat-plate carousels, centres x -31 / -19 / -7, y 31; straight 15 m, centre-line radius 1.8 m:
#                41.3 m of belt each (IATA: about 37 - 40 m of presentation for a large narrow-body); plates 0.92 m
#                wide at 0.45 m; overall 19.9 x 4.9 m; 0.46 m/s
#   feed         each belt's bags rise out of a shaft in its island's north end (a stainless hood with rubber flaps,
#                an inclined belt up to the plates); in the game the bags enter the sorting hall's wall and come up here
#   crew side    the sorting hall behind the north wall: an input conveyor per belt (6 m, 0.75 m high) into the wall
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/BaggageClaim/render_claim_concept.py
import bpy, bmesh, math, os, json, random
from mathutils import Vector
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
SCAN = os.path.join(ROOT, 'Art', 'Ground', 'Scans')
bpy.ops.wm.read_factory_settings(use_empty=True)
S = bpy.context.scene
R = random.Random(72)

# ---------------------------------------------------------------- materials
def node_mat(name):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial'); b = nt.nodes.new('ShaderNodeBsdfPrincipled'); b.location = (-300, 0)
    nt.links.new(b.outputs[0], out.inputs[0]); return m, nt, b
def simple(name, col, rough=0.5, metal=0.0, emit=None, estr=0.0):
    m, nt, b = node_mat(name)
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Roughness'].default_value = rough; b.inputs['Metallic'].default_value = metal
    if emit: b.inputs['Emission Color'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = estr
    return m
def terrazzo():
    m, nt, b = node_mat('Terrazzo'); L = nt.links
    tc = nt.nodes.new('ShaderNodeTexCoord')
    v1 = nt.nodes.new('ShaderNodeTexVoronoi'); v1.inputs['Scale'].default_value = 55; L.new(tc.outputs['Object'], v1.inputs['Vector'])
    v2 = nt.nodes.new('ShaderNodeTexVoronoi'); v2.inputs['Scale'].default_value = 140; L.new(tc.outputs['Object'], v2.inputs['Vector'])
    ra = nt.nodes.new('ShaderNodeValToRGB'); ra.color_ramp.elements[0].position = 0.0; ra.color_ramp.elements[0].color = (0.62, 0.60, 0.56, 1)
    e = ra.color_ramp.elements.new(0.45); e.color = (0.40, 0.42, 0.44, 1); e = ra.color_ramp.elements.new(0.7); e.color = (0.78, 0.76, 0.71, 1)
    ra.color_ramp.elements[-1].color = (0.20, 0.22, 0.25, 1); L.new(v1.outputs['Color'], ra.inputs[0])
    d1 = nt.nodes.new('ShaderNodeMath'); d1.operation = 'LESS_THAN'; d1.inputs[1].default_value = 0.32; L.new(v1.outputs['Distance'], d1.inputs[0])
    d2 = nt.nodes.new('ShaderNodeMath'); d2.operation = 'LESS_THAN'; d2.inputs[1].default_value = 0.18; L.new(v2.outputs['Distance'], d2.inputs[0])
    base = nt.nodes.new('ShaderNodeRGB'); base.outputs[0].default_value = (0.60, 0.58, 0.54, 1)
    mx = nt.nodes.new('ShaderNodeMix'); mx.data_type = 'RGBA'; L.new(d1.outputs[0], mx.inputs['Factor']); L.new(base.outputs[0], mx.inputs['A']); L.new(ra.outputs[0], mx.inputs['B'])
    mx2 = nt.nodes.new('ShaderNodeMix'); mx2.data_type = 'RGBA'; L.new(d2.outputs[0], mx2.inputs['Factor']); L.new(mx.outputs['Result'], mx2.inputs['A']); mx2.inputs['B'].default_value = (0.30, 0.32, 0.34, 1)
    L.new(mx2.outputs['Result'], b.inputs['Base Color']); b.inputs['Roughness'].default_value = 0.18
    # the floor's polished slabs: 1.2 m joints
    br = nt.nodes.new('ShaderNodeTexBrick'); br.inputs['Scale'].default_value = 1.0 / 1.2; br.inputs['Mortar Size'].default_value = 0.004; br.offset = 0.0
    br.inputs['Color1'].default_value = (1, 1, 1, 1); br.inputs['Color2'].default_value = (1, 1, 1, 1); br.inputs['Mortar'].default_value = (0.45, 0.45, 0.45, 1)
    L.new(tc.outputs['Object'], br.inputs['Vector'])
    mul = nt.nodes.new('ShaderNodeMix'); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'; mul.inputs['Factor'].default_value = 1.0
    L.new(mx2.outputs['Result'], mul.inputs['A']); L.new(br.outputs['Color'], mul.inputs['B']); L.new(mul.outputs['Result'], b.inputs['Base Color'])
    return m
def brushed(name, col=(0.72, 0.73, 0.75), rough=0.28):
    m, nt, b = node_mat(name); L = nt.links
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1, 1, 260)
    L.new(tc.outputs['Object'], mp.inputs['Vector'])
    n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = 6; n.inputs['Detail'].default_value = 8; L.new(mp.outputs['Vector'], n.inputs['Vector'])
    bm_ = nt.nodes.new('ShaderNodeBump'); bm_.inputs['Strength'].default_value = 0.05; L.new(n.outputs['Fac'], bm_.inputs['Height']); L.new(bm_.outputs['Normal'], b.inputs['Normal'])
    rr = nt.nodes.new('ShaderNodeMapRange'); rr.inputs['To Min'].default_value = rough - 0.06; rr.inputs['To Max'].default_value = rough + 0.08; L.new(n.outputs['Fac'], rr.inputs['Value']); L.new(rr.outputs['Result'], b.inputs['Roughness'])
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = 1.0
    return m
def rubber():
    m, nt, b = node_mat('BeltRubber'); L = nt.links
    tc = nt.nodes.new('ShaderNodeTexCoord'); n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = 220; L.new(tc.outputs['Object'], n.inputs['Vector'])
    bm_ = nt.nodes.new('ShaderNodeBump'); bm_.inputs['Strength'].default_value = 0.15; L.new(n.outputs['Fac'], bm_.inputs['Height']); L.new(bm_.outputs['Normal'], b.inputs['Normal'])
    b.inputs['Base Color'].default_value = (0.03, 0.03, 0.035, 1); b.inputs['Roughness'].default_value = 0.62
    return m
def oak():
    m, nt, b = node_mat('OakSlat'); L = nt.links
    tc = nt.nodes.new('ShaderNodeTexCoord'); w = nt.nodes.new('ShaderNodeTexWave'); w.wave_type = 'BANDS'; w.inputs['Scale'].default_value = 3; w.inputs['Distortion'].default_value = 6; w.inputs['Detail'].default_value = 4
    mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (0.08, 40, 40); L.new(tc.outputs['Object'], mp.inputs['Vector']); L.new(mp.outputs['Vector'], w.inputs['Vector'])
    ra = nt.nodes.new('ShaderNodeValToRGB'); ra.color_ramp.elements[0].color = (0.42, 0.26, 0.13, 1); ra.color_ramp.elements[1].color = (0.70, 0.50, 0.30, 1)
    L.new(w.outputs['Fac'], ra.inputs[0]); L.new(ra.outputs[0], b.inputs['Base Color']); b.inputs['Roughness'].default_value = 0.45
    return m
def basalt():
    m, nt, b = node_mat('BasaltWall'); L = nt.links
    tc = nt.nodes.new('ShaderNodeTexCoord'); v = nt.nodes.new('ShaderNodeTexVoronoi'); v.inputs['Scale'].default_value = 1.4; v.feature = 'DISTANCE_TO_EDGE'
    L.new(tc.outputs['Object'], v.inputs['Vector'])
    n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = 25; n.inputs['Detail'].default_value = 10; L.new(tc.outputs['Object'], n.inputs['Vector'])
    ra = nt.nodes.new('ShaderNodeValToRGB'); ra.color_ramp.elements[0].position = 0.0; ra.color_ramp.elements[0].color = (0.01, 0.01, 0.012, 1)
    ra.color_ramp.elements[1].position = 0.04; ra.color_ramp.elements[1].color = (0.075, 0.075, 0.08, 1); L.new(v.outputs['Distance'], ra.inputs[0])
    mx = nt.nodes.new('ShaderNodeMix'); mx.data_type = 'RGBA'; mx.blend_type = 'MULTIPLY'; mx.inputs['Factor'].default_value = 0.5
    L.new(ra.outputs[0], mx.inputs['A']); L.new(n.outputs['Color'], mx.inputs['B']); L.new(mx.outputs['Result'], b.inputs['Base Color'])
    bm_ = nt.nodes.new('ShaderNodeBump'); bm_.inputs['Strength'].default_value = 0.6; L.new(v.outputs['Distance'], bm_.inputs['Height']); L.new(bm_.outputs['Normal'], b.inputs['Normal'])
    b.inputs['Roughness'].default_value = 0.55
    return m
def scan(name, sid, size=3.0):
    m, nt, b = node_mat(name); L = nt.links
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / size, 1 / size, 1 / size)
    L.new(tc.outputs['Object'], mp.inputs['Vector'])
    def img(f, col):
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(SCAN, sid, f)); t.projection = 'BOX'
        if not col: t.image.colorspace_settings.name = 'Non-Color'
        L.new(mp.outputs['Vector'], t.inputs['Vector']); return t
    d = img(sid + '_diff_4k.jpg', True); L.new(d.outputs[0], b.inputs['Base Color'])
    nm = img(sid + '_nor_dx_4k.jpg', False); nmap = nt.nodes.new('ShaderNodeNormalMap'); L.new(nm.outputs[0], nmap.inputs['Color']); L.new(nmap.outputs[0], b.inputs['Normal'])
    b.inputs['Roughness'].default_value = 0.7
    return m
def picture(name, file, strength=6.0):
    m, nt, b = node_mat(name); L = nt.links
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(HERE, file))
    L.new(t.outputs[0], b.inputs['Emission Color']); b.inputs['Emission Strength'].default_value = strength
    b.inputs['Base Color'].default_value = (0, 0, 0, 1); b.inputs['Roughness'].default_value = 0.15
    return m
M = dict(floor=terrazzo(), steel=brushed('Steel'), dark=brushed('DarkSteel', (0.08, 0.085, 0.09), 0.4), rubber=rubber(), oak=oak(), basalt=basalt(),
         plaster=simple('Plaster', (0.86, 0.85, 0.82), 0.8), plenum=simple('Plenum', (0.02, 0.02, 0.02), 0.9),
         led=simple('LED', (1, 1, 1), 0.5, emit=(1.0, 0.93, 0.84), estr=18.0), glass=None, flap=simple('Flap', (0.02, 0.02, 0.02), 0.7),
         concrete=scan('SortFloor', 'concrete_floor_worn_001', 3.0), wallc=scan('SortWall', 'brushed_concrete_04', 4.0),
         yellow=simple('SafetyYellow', (0.85, 0.62, 0.02), 0.5), top=simple('IslandTop', (0.10, 0.11, 0.12), 0.35, 0.2),
         grass=simple('Grass', (0.10, 0.20, 0.06), 0.9))
g = bpy.data.materials.new('Glass'); g.use_nodes = True; gn = g.node_tree.nodes['Principled BSDF']
gn.inputs['Transmission Weight'].default_value = 1.0; gn.inputs['Roughness'].default_value = 0.0; gn.inputs['IOR'].default_value = 1.45; gn.inputs['Base Color'].default_value = (0.9, 0.97, 0.98, 1); M['glass'] = g

# ---------------------------------------------------------------- geometry helpers
def box(x0, y0, z0, x1, y1, z1, mat, name='box'):
    bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    o = bpy.context.object; o.scale = (abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)); o.name = name
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True); o.data.materials.append(mat); return o
def prism(pts, z0, z1, mat, name='prism'):
    me = bpy.data.meshes.new(name); bm = bmesh.new()
    lo = [bm.verts.new((p[0], p[1], z0)) for p in pts]; hi = [bm.verts.new((p[0], p[1], z1)) for p in pts]
    bm.faces.new(lo[::-1]); bm.faces.new(hi)
    n = len(pts)
    for i in range(n): bm.faces.new((lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i]))
    bm.normal_update(); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); S.collection.objects.link(o); me.materials.append(mat); return o
def track(cx, cy, r, L=15.0, seg=28):
    pts = []
    for i in range(seg + 1): a = math.pi * i / seg; pts.append((cx + r * math.cos(a), cy + L / 2 + r * math.sin(a)))
    for i in range(seg + 1): a = math.pi + math.pi * i / seg; pts.append((cx + r * math.cos(a), cy - L / 2 + r * math.sin(a)))
    return pts
def path_at(cx, cy, s, r=1.8, L=15.0):
    # along the centre line, anticlockwise from the east side's south end
    per = 2 * L + 2 * math.pi * r; s %= per
    if s < L: return (cx + r, cy - L / 2 + s, 0), (0, 1)
    s -= L
    if s < math.pi * r: a = s / r; return (cx + r * math.cos(a), cy + L / 2 + r * math.sin(a), 0), (-math.sin(a), math.cos(a))
    s -= math.pi * r
    if s < L: return (cx - r, cy + L / 2 - s, 0), (0, -1)
    s -= L; a = math.pi + s / r; return (cx + r * math.cos(a), cy - L / 2 + r * math.sin(a), 0), (-math.sin(a), math.cos(a))
BAGCOL = [(0.55, 0.05, 0.05), (0.02, 0.05, 0.12), (0.05, 0.05, 0.05), (0.12, 0.25, 0.45), (0.65, 0.55, 0.40), (0.30, 0.32, 0.34), (0.05, 0.25, 0.15), (0.75, 0.35, 0.05)]
BAGM = [simple('Bag%d' % i, c, 0.35 + 0.1 * (i % 3)) for i, c in enumerate(BAGCOL)]
def bag(x, y, z, yaw, kind):
    w, d, h = [(0.70, 0.45, 0.28), (0.55, 0.38, 0.24), (0.80, 0.50, 0.32), (0.50, 0.32, 0.30)][kind % 4]
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z + h / 2)); o = bpy.context.object; o.scale = (w, d, h); bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    o.rotation_euler = (0, 0, yaw); mod = o.modifiers.new('bev', 'BEVEL'); mod.width = 0.04; mod.segments = 4
    o.data.materials.append(BAGM[R.randrange(len(BAGM))])
    bpy.ops.mesh.primitive_cylinder_add(radius=0.014, depth=0.2, location=(x, y, z + h + 0.025), rotation=(0, math.pi / 2, yaw)); bpy.context.object.data.materials.append(M['dark'])
    return o

# ---------------------------------------------------------------- the claim hall
X0, X1, Y0, Y1, CEIL = -80.0, 0.0, 16.0, 52.0, 5.4
box(X0, Y0, -0.2, X1, Y1, 0.0, M['floor'], 'ClaimFloor')
box(X0 - 80, -120, -0.25, X1 + 160, Y1 + 60, -0.2, M['grass'], 'Ground')
# walls: south (customs, plaster with its opening), east (the museum's side: a dark basalt feature wall), north (plaster)
box(X0, Y0 - 0.3, 0, -44, Y0, CEIL, M['plaster']); box(-36, Y0 - 0.3, 0, X1, Y0, CEIL, M['plaster']); box(-44, Y0 - 0.3, 3.0, -36, Y0, CEIL, M['plaster'])
box(X1, Y0, 0, X1 + 0.3, Y1, CEIL, M['basalt'], 'BasaltWall')
box(X0, Y1, 0, X1, Y1 + 0.3, 6.0, M['plaster'], 'NorthWall')
# west: the glass wall onto the airfield side, mullions every 2.5 m
box(X0 - 0.06, Y0, 0, X0, Y1, CEIL, M['glass'], 'WestGlass')
for y in [Y0 + 2.5 * k for k in range(15)]: box(X0 - 0.12, y - 0.05, 0, X0, y + 0.05, CEIL, M['dark'])
box(X0 - 0.12, Y0, 0, X0, Y1, 0.12, M['dark']); box(X0 - 0.12, Y0, CEIL - 0.15, X0, Y1, CEIL, M['dark'])
# the ceiling: oak slats over a dark plenum, LED lines between them; the Floor 2 corridor's slab over y 40..52 (its void
# over the escalators open)
box(-68.2, Y0, CEIL + 0.3, X1, Y1, CEIL + 0.35, M['plenum'], 'Plenum'); box(X0, Y0, CEIL + 0.3, -68.2, 34.5, CEIL + 0.35, M['plenum'])
slat = box(-68.2, Y0 + 0.05, CEIL - 0.12, X1, Y0 + 0.11, CEIL, M['oak'], 'Slat')
slatw = box(X0, Y0 + 0.05, CEIL - 0.12, -68.2, Y0 + 0.11, CEIL, M['oak'], 'SlatW')   # (west of the void: only up to it)
arw = slatw.modifiers.new('arr', 'ARRAY'); arw.count = 154; arw.use_relative_offset = False; arw.use_constant_offset = True; arw.constant_offset_displace = (0, 0.12, 0)
ar = slat.modifiers.new('arr', 'ARRAY'); ar.count = 300; ar.use_relative_offset = False; ar.use_constant_offset = True; ar.constant_offset_displace = (0, 0.12, 0)
for y in [Y0 + 3 + 3 * k for k in range(11)]: box(X0 if y < 34.5 else -68.2, y - 0.02, CEIL - 0.13, X1, y + 0.02, CEIL - 0.11, M['led'], 'LedLine')
# the void's own walls up to Floor 2's ceiling (11.4), its ceiling, the west glass carried up
box(-68.2, 34.5, CEIL - 0.15, -67.9, Y1, 11.4, M['plaster'], 'VoidEast'); box(X0, 34.2, CEIL - 0.15, -67.9, 34.5, 11.4, M['plaster'], 'VoidSouth')
box(X0, 34.5, 11.4, -67.9, Y1, 11.6, M['plaster'], 'VoidCeiling')
for y in (40.0, 44.0, 48.0): box(-79.5, y - 0.3, 11.3, -68.7, y + 0.3, 11.38, M['led'])
box(X0 - 0.06, 34.5, CEIL, X0, Y1, 11.4, M['glass'])
for y in (37.0, 39.5, 42.0, 44.5, 47.0, 49.5, 52.0): box(X0 - 0.12, y - 0.05, CEIL, X0, y + 0.05, 11.4, M['dark'])
# the Floor 2 top landing (2.5 m deep) in front of the doorway, the escalators, stair and lift arriving on it
box(-76.1, 48.89, CEIL, -68.2, Y1, 6.0, M['plaster'], 'LandingSlab'); box(-76.1, 48.89, 5.99, -68.2, Y1, 6.0, M['floor'], 'LandingFloor')
box(X0, 51.9, CEIL, -76.1, Y1, 6.0, M['floor'])   # (behind the escalators' top plates)
box(-74.0, 48.84, 6.0, -71.1, 48.89, 7.1, M['glass'], 'LandingRail'); box(-74.0, 48.79, 7.05, -71.1, 48.94, 7.12, M['steel'])

box(-72.0, Y1, 6.0, X1, Y1 + 0.3, 11.4, M['plaster']); box(X0, Y1, 9.6, -72.0, Y1 + 0.3, 11.4, M['plaster'])   # (the 8 m doorway into the gates' walkway, 3.6 m high)
# the gates' walkway beyond (Floor 2, z 52.3..68): terrazzo, a white ceiling with LED lines, its glass onto the apron
box(X0, Y1 + 0.3, 5.4, -30.0, 68.0, 6.0, M['floor'], 'WalkFloor'); box(X0, Y1 + 0.3, 11.4, -30.0, 68.0, 11.6, M['plaster'], 'WalkCeiling')
for x in range(-78, -30, 4): box(x - 0.3, Y1 + 1.0, 11.3, x + 0.3, 67.3, 11.38, M['led'])
box(X0, 67.9, 6.0, -30.0, 68.0, 11.4, M['glass'], 'WalkGlass'); box(X0 - 0.3, Y1 + 0.3, 6.0, X0, 68.0, 11.4, M['plaster'])
for x in range(-80, -29, 3): box(x - 0.05, 67.8, 6.0, x + 0.05, 68.0, 11.4, M['dark'])

# ---------------------------------------------------------------- escalators, stair and lift (down from the corridor)
# the building's own escalator (Art/Escalator/Escalator.blend: the same as at the escalator room - rounded newels at
# both ends, the handrail running 0.75 m past each comb and turning round its 0.32 m radius, glass curved under it), its
# foot at the south, rising north 6 m to the Floor 2 landing; its steps placed along it
ESC = os.path.join(ROOT, 'Art', 'Escalator', 'Escalator.blend')
with bpy.data.libraries.load(ESC) as (src, dst): dst.objects = [n for n in src.objects if n in ('SM_EscBody', 'SM_EscRamp', 'SM_EscStep')]
EY0 = 36.59                                   # its foot (the bottom comb's teeth), so its top floor plate ends 0.1 m short of the wall
def zat(u): return 0.0 if u < 1.275 else 6.0 if u > 12.3 else (u - 1.275) * 6.0 / (12.3 - 1.275)
for xc in (-78.8, -77.1):
    for o in dst.objects:
        if o.name.startswith('SM_EscStep'): continue
        c = o.copy(); S.collection.objects.link(c); c.location = (xc, EY0, 0.0)
    stp = [o for o in dst.objects if o.name.startswith('SM_EscStep')][0]
    u = 0.0
    while u < 12.2:
        c = stp.copy(); S.collection.objects.link(c); c.location = (xc, EY0 + u + 0.4, zat(u + 0.2)); u += 0.4
LAND = EY0 + 12.3                             # the Floor 2 landing's edge: where the escalators' top flat begins
for k in range(30): box(-76.1, LAND - 11.4 + k * 0.38, 0, -74.1, LAND - 11.0 + k * 0.38, 0.2 * (k + 1), M['floor'])   # the stair
box(-71.0, 46.4, 0, -68.6, 49.2, 9.0, M['glass'], 'LiftShaft'); box(-71.0, 46.4, 9.0, -68.6, 49.2, 9.15, M['dark'])   # (its door opens north onto the landing)
box(-70.6, 49.18, 0.0, -69.0, 49.22, 2.2, M['steel']); box(-70.6, 49.18, 6.0, -69.0, 49.22, 8.2, M['steel'])

# ---------------------------------------------------------------- the three belts
BELTS = [(-60.8, 31.0, 1), (-48.15, 31.0, 2), (-35.5, 31.0, 3), (-22.85, 31.0, 4), (-10.2, 31.0, 5)]   # (7.75 m walking room between and round them)
L_, RC = 15.0, 1.8
for cx, cy, n in BELTS:
    prism(track(cx, cy, 2.45), 0, 0.40, M['steel'], 'Skirt%d' % n)
    prism(track(cx, cy, 2.47), 0, 0.06, M['dark'], 'Kick%d' % n)
    prism(track(cx, cy, 2.30), 0.40, 0.415, M['dark'])
    # the plates: 0.92 m wide, 0.3 m along, a hair apart
    per = 2 * L_ + 2 * math.pi * RC; np_ = int(per / 0.3)
    for k in range(np_):
        (px, py, _), (tx, ty) = path_at(cx, cy, k * per / np_)
        bpy.ops.mesh.primitive_cube_add(size=1, location=(px, py, 0.435)); o = bpy.context.object; o.scale = (0.92, 0.285, 0.03)
        o.rotation_euler = (0, 0, math.atan2(ty, tx) - math.pi / 2); o.data.materials.append(M['rubber'])
    # the island in the middle, its stainless sides and dark top, with the feed shaft's opening near its north end
    isl = prism(track(cx, cy, 1.30, L_ - 0.2), 0, 0.62, M['steel'], 'Island%d' % n)
    top = prism(track(cx, cy, 1.25, L_ - 0.3), 0.62, 0.64, M['top'], 'IslandTop%d' % n)
    cut = box(cx - 0.45, cy + 5.4, -0.1, cx + 1.4, cy + 6.6, 1.0, M['dark'], 'cut')
    for o in (isl, top):
        bo = o.modifiers.new('hole', 'BOOLEAN'); bo.object = cut; bo.operation = 'DIFFERENCE'
    cut.hide_render = True; cut.hide_viewport = True
    # the feed: an inclined belt rising out of the shaft to the plates, a stainless hood over it, rubber flaps at its mouth
    inc = box(cx - 0.4, cy + 5.55, -0.02, cx + 1.45, cy + 6.45, 0.02, M['rubber'], 'Incline%d' % n)
    inc.rotation_euler = (0, -math.radians(18), 0); inc.location = (cx + 0.55, cy + 6.0, 0.33)
    box(cx - 0.5, cy + 5.4, 0.64, cx + 0.75, cy + 5.46, 1.15, M['steel']); box(cx - 0.5, cy + 6.54, 0.64, cx + 0.75, cy + 6.6, 1.15, M['steel'])
    box(cx - 0.5, cy + 5.4, 1.15, cx + 0.75, cy + 6.6, 1.2, M['steel'], 'Hood%d' % n); box(cx - 0.56, cy + 5.4, 0.64, cx - 0.5, cy + 6.6, 1.2, M['steel'])
    for k in range(8):
        f = box(cx + 0.72, cy + 5.47 + k * 0.135, 0.68, cx + 0.74, cy + 5.59 + k * 0.135, 1.15, M['flap']); f.rotation_euler = (0, math.radians(-6 if k % 2 else 4), 0)
    box(cx - 0.5, cy + 5.38, 1.2, cx + 0.75, cy + 6.62, 1.23, M['rubber'])
    # bags: one coming up out of the shaft through the flaps, others riding round
    bag(cx + 0.95, cy + 6.0, 0.52, 0.0, n)
    for k in range(9 + n * 2):
        s = R.uniform(0, per)
        if abs(s - (L_ + math.pi * RC / 2)) < 2.0: continue
        (px, py, _), (tx, ty) = path_at(cx, cy, s)
        bag(px + R.uniform(-0.12, 0.12), py, 0.45, math.atan2(ty, tx) + R.uniform(-0.5, 0.5), R.randrange(4))
    # its screen, hung over the north end, facing the hall
    box(cx - 1.0, cy + 10.4, 3.0, cx + 1.0, cy + 10.5, 4.125, M['dark'], 'Screen%d' % n)
    # (the screen's front face: a plane with its own UVs, so the picture sits right)
    bpy.ops.mesh.primitive_plane_add(size=1, location=(cx, cy + 10.39, 3.5625), rotation=(math.pi / 2, 0, 0)); pl = bpy.context.object
    pl.scale = (1.96, 1.09, 1); pl.data.materials.append(picture('ScreenFace%d' % n, 'screen_belt%d.png' % n, 4.0))
    box(cx - 1.06, cy + 10.5, 3.0, cx + 1.06, cy + 10.6, 4.19, M['dark']); box(cx - 0.03, cy + 10.5, 4.19, cx + 0.03, cy + 10.56, CEIL, M['steel'])
# the hall's overhead sign, seen coming off the escalators
bpy.ops.mesh.primitive_plane_add(size=1, location=(-76.0, 33.02, 3.4), rotation=(math.pi / 2, 0, math.pi)); sg = bpy.context.object   # (its north face, for those coming down)
sg.scale = (6.0, 0.75, 1); sg.data.materials.append(picture('SignClaim', 'sign_claim.png', 3.0))
bpy.ops.mesh.primitive_plane_add(size=1, location=(-76.0, 32.88, 3.4), rotation=(math.pi / 2, 0, 0)); sg2 = bpy.context.object   # (its other face, for those walking back north)
sg2.scale = (6.0, 0.75, 1); sg2.data.materials.append(picture('SignClaim2', 'sign_claim.png', 3.0))
box(-79.0, 32.92, 3.0, -73.0, 33.0, 3.8, M['dark']); box(-77, 32.94, 3.8, -76.95, 32.98, CEIL, M['steel']); box(-75.05, 32.94, 3.8, -75.0, 32.98, CEIL, M['steel'])
# customs: its doorway's sign over the opening
box(-44, Y0 - 0.32, 2.4, -36, Y0 - 0.3, 3.0, simple('CustomsGreen', (0.02, 0.35, 0.12), 0.4, emit=(0.05, 0.6, 0.2), estr=1.5))
# a few seats along the glass
# seats: along the west glass well clear of the escalators' feet (y 18..27), and along the basalt wall in the east
for k in range(3): box(-79.2, 18 + k * 3.2, 0.42, -78.6, 20.2 + k * 3.2, 0.48, M['oak']); box(-79.0, 18.5 + k * 3.2, 0, -78.8, 19.7 + k * 3.2, 0.42, M['dark'])
for k in range(3): box(-0.9, 20 + k * 4.0, 0.42, -0.3, 22.2 + k * 4.0, 0.48, M['oak']); box(-0.7, 20.5 + k * 4.0, 0, -0.5, 21.7 + k * 4.0, 0.42, M['dark'])

# ---------------------------------------------------------------- the crew side: the sorting hall behind the north wall
SY0, SY1 = Y1 + 0.3, 68.0
box(X0, SY0, -0.2, X1 + 20, SY1, 0.0, M['concrete'], 'SortFloor')
box(X0, SY1, 0, X1 + 20, SY1 + 0.3, CEIL, M['wallc']); box(X0 - 0.3, SY0, 0, X0, SY1, CEIL, M['wallc'])
box(X0, SY0, CEIL, X1 + 20, SY1, CEIL + 0.2, M['wallc'], 'SortCeiling')
for x in [X0 + 4 + 6 * k for k in range(16)]: box(x - 0.3, SY0, CEIL - 0.15, x + 0.3, SY1, CEIL - 0.1, M['led'])
for x in [X0 + 6 * k for k in range(17)]: box(x - 0.15, SY0, CEIL - 0.7, x + 0.15, SY1, CEIL, M['dark'])
for cx, cy, n in BELTS:
    # its input conveyor: 6 m from the loading point into the wall, 0.75 m high, flaps and a yellow frame at the wall
    box(cx - 0.55, SY0, 0, cx + 0.55, SY0 + 6.0, 0.70, M['dark'], 'InFrame%d' % n)
    box(cx - 0.5, SY0, 0.70, cx + 0.5, SY0 + 6.0, 0.75, M['rubber'], 'InBelt%d' % n)
    for xs in (cx - 0.58, cx + 0.5): box(xs, SY0, 0.75, xs + 0.08, SY0 + 6.0, 0.95, M['steel'])
    box(cx - 0.8, SY0, 0.70, cx - 0.7, SY0 + 0.1, 1.85, M['yellow']); box(cx + 0.7, SY0, 0.70, cx + 0.8, SY0 + 0.1, 1.85, M['yellow']); box(cx - 0.8, SY0, 1.75, cx + 0.8, SY0 + 0.1, 1.85, M['yellow'])
    for k in range(10): box(cx - 0.68 + k * 0.137, SY0 + 0.1, 0.76, cx - 0.56 + k * 0.137, SY0 + 0.12, 1.74, M['flap'])
    for k in range(3): bag(cx + R.uniform(-0.1, 0.1), SY0 + 1.5 + k * 1.6, 0.75, R.uniform(-0.3, 0.3), R.randrange(4))
    bpy.ops.mesh.primitive_plane_add(size=1, location=(cx, SY0 + 0.12, 2.3), rotation=(math.pi / 2, 0, math.pi)); sp = bpy.context.object
    sp.scale = (1.6, 0.4, 1); sp.data.materials.append(picture('SignIn%d' % n, 'sign_input.png', 1.5))
    # a baggage cart (dolly) parked at the loading point, bags on it
    box(cx + 1.2, SY0 + 5.0, 0.35, cx + 2.6, SY0 + 7.8, 0.42, M['dark']); box(cx + 1.2, SY0 + 5.0, 0.42, cx + 1.25, SY0 + 7.8, 1.6, M['yellow'])
    for k in range(4): bag(cx + 1.9, SY0 + 5.5 + k * 0.6, 0.42, 1.57, R.randrange(4))
    for wx in (cx + 1.3, cx + 2.5):
        for wy in (SY0 + 5.3, SY0 + 7.5):
            bpy.ops.mesh.primitive_cylinder_add(radius=0.18, depth=0.1, location=(wx, wy, 0.18), rotation=(0, math.pi / 2, 0)); bpy.context.object.data.materials.append(M['rubber'])
# the yellow walking lines on the sorting hall's floor
for cx, cy, n in BELTS: box(cx - 1.0, SY0 + 0.4, 0.001, cx - 0.9, SY1 - 0.5, 0.004, M['yellow'])

# ---------------------------------------------------------------- the belts' start / stop buttons (owner, 2026-11-16)
# Passenger side: on each belt's skirt (east side, by the feed shaft), a recessed steel plate with one big green
# illuminated push button - press: the belt starts, press again: it stops. Crew side: at each input conveyor's loading
# end, a yellow control box on a post with the same green button facing the crew. (In the game: E/X, the glow outline,
# the button pressed in and back out; to-do 72.)
BTN = simple('ButtonGreen', (0.05, 0.5, 0.15), 0.25, emit=(0.15, 1.0, 0.35), estr=2.5)
for cx, cy, n in BELTS:
    box(cx + 2.45, cy + 2.9, 0.12, cx + 2.49, cy + 3.5, 0.36, M['dark'], 'BtnPlate%d' % n)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.055, depth=0.04, location=(cx + 2.51, cy + 3.2, 0.24), rotation=(0, math.pi / 2, 0)); bpy.context.object.data.materials.append(BTN); bpy.context.object.name = 'Button%d' % n
    bpy.ops.mesh.primitive_cylinder_add(radius=0.075, depth=0.012, location=(cx + 2.495, cy + 3.2, 0.24), rotation=(0, math.pi / 2, 0)); bpy.context.object.data.materials.append(M['steel'])
    # the crew's control box at the input's loading end
    box(cx - 1.32, SY0 + 5.2, 0, cx - 1.22, SY0 + 5.3, 1.0, M['yellow'], 'BtnPost%d' % n)
    box(cx - 1.45, SY0 + 5.1, 1.0, cx - 1.09, SY0 + 5.4, 1.32, M['yellow'], 'BtnBox%d' % n)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.06, depth=0.04, location=(cx - 1.27, SY0 + 5.42, 1.16), rotation=(math.pi / 2, 0, 0)); bpy.context.object.data.materials.append(BTN); bpy.context.object.name = 'CrewButton%d' % n
    bpy.ops.mesh.primitive_cylinder_add(radius=0.08, depth=0.012, location=(cx - 1.27, SY0 + 5.405, 1.16), rotation=(math.pi / 2, 0, 0)); bpy.context.object.data.materials.append(M['dark'])
# ---------------------------------------------------------------- the way out: customs, the arrivals hall, the doors
def sign(file, x, y, z, w, h, facing_south=True, strength=3.0):
    bpy.ops.mesh.primitive_plane_add(size=1, location=(x, y, z), rotation=(math.pi / 2, 0, 0 if facing_south else math.pi)); o = bpy.context.object
    o.scale = (w, h, 1); o.data.materials.append(picture('Sign_' + file, file, strength)); return o
AY0 = -4.0
box(X0, AY0, -0.2, X1, Y0, 0.0, M['floor'], 'ArrFloor')
box(X1, AY0, 0, X1 + 0.3, Y0, CEIL, M['basalt'], 'ArrEastWall')   # (owner: the arrivals hall's east end closed - the escalator room lies behind it)
# the arrivals part's ceiling: the same oak slats and LED lines
sl2 = box(X0, AY0 + 0.05, CEIL - 0.12, X1, AY0 + 0.11, CEIL, M['oak'], 'Slat2')
ar2 = sl2.modifiers.new('arr', 'ARRAY'); ar2.count = 166; ar2.use_relative_offset = False; ar2.use_constant_offset = True; ar2.constant_offset_displace = (0, 0.12, 0)
box(X0, AY0, CEIL + 0.3, X1, Y0, CEIL + 0.35, M['plenum'])
for y in (-1.0, 2.0, 5.0, 8.0, 11.0, 14.0): box(X0, y - 0.02, CEIL - 0.13, X1, y + 0.02, CEIL - 0.11, M['led'])
# customs (z 8..16): the hall x -56..-24 between the toilets (west) and the customs office (east); the green channel
# straight through (x -44..-36), the red channel to the desk (x -54..-48)
box(-56.3, 8.0, 0, -56.0, 16.0, CEIL, M['plaster']); box(-24.0, 8.0, 0, -23.7, 16.0, CEIL, M['plaster'])
box(X0, 7.7, 0, -54, 8.0, CEIL, M['plaster']); box(-48, 7.7, 0, -44, 8.0, CEIL, M['plaster']); box(-36, 7.7, 0, X1, 8.0, CEIL, M['plaster'])
box(-54, 7.7, 3.0, -48, 8.0, CEIL, M['plaster']); box(-44, 7.7, 3.0, -36, 8.0, CEIL, M['plaster'])
box(-44, 8.3, 0.0, -36, 15.7, 0.004, simple('GreenLane', (0.05, 0.30, 0.12), 0.4), 'GreenLane')
box(-54, 8.3, 0.0, -48, 15.7, 0.004, simple('RedLane', (0.40, 0.05, 0.04), 0.4), 'RedLane')
box(-54.5, 11.0, 0, -52.5, 13.5, 0.95, M['steel']); box(-54.6, 10.9, 0.95, -52.4, 13.6, 1.0, M['dark'])
sign('sign_exit.png', -40.0, 7.6, 3.6, 7.6, 1.4, True, 2.5)
box(-44, 15.6, 3.1, -36, 15.65, 3.9, simple('GreenSign', (0.02, 0.2, 0.06), 0.4, emit=(0.1, 0.9, 0.3), estr=3.0))
box(-54, 15.6, 3.1, -48, 15.65, 3.9, simple('RedSign', (0.2, 0.02, 0.02), 0.4, emit=(0.95, 0.1, 0.05), estr=3.0))
# the arrivals hall (z -4..8): the meeting rail round the customs exit, desks along the west, a cafe in the east
for x0, x1 in ((-62, -46), (-34, -18)):
    box(x0, 3.6, 0.95, x1, 3.7, 1.0, M['steel'])
    for x in range(int(x0), int(x1) + 1, 2): box(x - 0.03, 3.62, 0, x + 0.03, 3.68, 0.95, M['steel'])
for file, x in (('desk_vega.png', -76.0), ('desk_vega.png', -70.0), ('desk_taxi.png', -64.0)):
    box(x - 2.4, 4.2, 0, x + 2.4, 5.0, 1.05, M['oak']); box(x - 2.5, 4.15, 1.05, x + 2.5, 5.05, 1.1, M['top'])
    box(x - 2.5, 7.4, 0, x + 2.5, 7.7, 3.2, M['basalt'])
    sign(file, x, 7.38, 2.6, 4.4, 0.85, True, 2.0)
box(-14, 4.0, 0, -2, 5.0, 1.05, M['oak']); box(-14.1, 3.95, 1.05, -1.9, 5.05, 1.1, M['top']); sign('desk_cafe.png', -8.0, 7.6, 2.8, 5.0, 0.95, True, 2.0)
for k in range(6):
    tx, ty = -16 + (k % 3) * 4.5, -1.5 + (k // 3) * 3.0
    bpy.ops.mesh.primitive_cylinder_add(radius=0.45, depth=0.04, location=(tx, ty, 0.74)); bpy.context.object.data.materials.append(M['top'])
    bpy.ops.mesh.primitive_cylinder_add(radius=0.04, depth=0.72, location=(tx, ty, 0.37)); bpy.context.object.data.materials.append(M['steel'])
# the front onto the kerb (z -4): glass the whole way, mullions every 2.5 m; the doors (x -48..-32: four sliding doors,
# open) under a canopy; above the glass the cladding band with ARRIVALS
for x0, x1 in ((X0, -48.0), (-32.0, X1)):
    box(x0, AY0 - 0.06, 0, x1, AY0, CEIL, M['glass'])
for x in [X0 + 2.5 * k for k in range(33)]: box(x - 0.05, AY0 - 0.12, 0, x + 0.05, AY0, CEIL, M['dark'])
box(-48, AY0 - 0.06, 3.0, -32, AY0, CEIL, M['glass'])
for x in (-48.0, -44.0, -40.0, -36.0, -32.0): box(x - 0.08, AY0 - 0.15, 0, x + 0.08, AY0 + 0.05, 3.0, M['dark'])
box(-48, AY0 - 0.15, 2.95, -32, AY0 + 0.05, 3.05, M['dark'])
for x0 in (-47.6, -43.6, -39.6, -35.6): box(x0, AY0 - 0.04, 0, x0 + 0.9, AY0 - 0.02, 2.95, M['glass'])          # the doors, slid aside
# the west end (x -80): glass to 5.4 m here too
box(X0 - 0.06, AY0, 0, X0, Y0, CEIL, M['glass'])
for y in (-1.5, 1.0, 3.5, 6.0, 8.5, 11.0, 13.5): box(X0 - 0.12, y - 0.05, 0, X0, y + 0.05, CEIL, M['dark'])

# ---------------------------------------------------------------- the outside: cladding, roof, canopy, the kerb
clad, cnt, cb = node_mat('Cladding')
tcx = cnt.nodes.new('ShaderNodeTexCoord'); brk = cnt.nodes.new('ShaderNodeTexBrick'); brk.inputs['Scale'].default_value = 0.6; brk.inputs['Mortar Size'].default_value = 0.01; brk.offset = 0.0; brk.squash = 0.3
brk.inputs['Color1'].default_value = (0.20, 0.21, 0.22, 1); brk.inputs['Color2'].default_value = (0.24, 0.25, 0.26, 1); brk.inputs['Mortar'].default_value = (0.05, 0.05, 0.05, 1)
cnt.links.new(tcx.outputs['Object'], brk.inputs['Vector']); cnt.links.new(brk.outputs['Color'], cb.inputs['Base Color']); cb.inputs['Metallic'].default_value = 0.6; cb.inputs['Roughness'].default_value = 0.35
TOP = 12.0
box(X0 - 0.3, AY0 - 0.3, CEIL, X1, AY0, TOP, clad, 'CladSouth'); box(X0 - 0.3, AY0, CEIL, X0, Y1, TOP, clad, 'CladWest')
box(X0 - 0.6, AY0 - 0.6, TOP, X1, Y1 + 16.3, TOP + 0.6, M['dark'], 'Roof')
bpy.ops.mesh.primitive_plane_add(size=1, location=(-40.0, AY0 - 0.35, 8.6), rotation=(math.pi / 2, 0, 0)); lt = bpy.context.object
lt.scale = (16.0, 3.1, 1); lt.data.materials.append(picture('ArrivalsLetters', 'sign_arrivals.png', 6.0))
# the canopy over the doors: a slim dark roof 9 m out, oak underside, on four columns
box(-56, AY0 - 9.0, 4.2, -24, AY0, 4.5, M['dark'], 'Canopy'); box(-56, AY0 - 9.0, 4.15, -24, AY0, 4.2, M['oak'])
for x in (-55.0, -25.0):
    for y in (AY0 - 8.5, AY0 - 4.5): box(x - 0.12, y - 0.12, 0, x + 0.12, y + 0.12, 4.2, M['dark'])
# the kerb: a paved forecourt, the kerb line, the road beyond with its lane line, street lamps, a few cars
box(X0 - 30, -43.0, -0.2, X1 + 40, AY0, 0.01, M['concrete'], 'Forecourt')
box(X0 - 30, -43.3, 0.01, X1 + 40, -43.0, 0.15, M['steel'])
box(X0 - 30, -60.0, -0.2, X1 + 40, -43.3, 0.0, simple('Asphalt', (0.04, 0.04, 0.045), 0.8), 'Road')
for k in range(14): box(-110 + k * 12, -51.7, 0.0, -104 + k * 12, -51.55, 0.005, M['yellow'])
for x in range(-76, 40, 12):
    box(x - 0.08, -41.5, 0, x + 0.08, -41.3, 7.0, M['dark']); box(x - 0.1, -41.9, 7.0, x + 0.1, -40.6, 7.1, M['dark'])
for x, col in ((-50, (0.85, 0.65, 0.05)), (-42, (0.9, 0.9, 0.92)), (-30, (0.05, 0.05, 0.06))):
    box(x - 2.3, -48.0, 0.25, x + 2.3, -46.2, 1.15, simple('Car%d' % (x + 100), col, 0.25, 0.3)); box(x - 1.4, -47.9, 1.15, x + 1.0, -46.3, 1.55, M['glass'])
# the neighbours, for context: the terminal east of the wing (glass front) and the gates' pier north of it
box(X1, AY0 + 4.0, 0, X1 + 114, Y1, TOP, M['plaster'], 'TerminalEast'); box(X1, AY0 + 3.94, 0, X1 + 114, AY0 + 4.0, 11.0, M['glass'])
for x in range(0, 115, 3): box(x - 0.05, AY0 + 3.85, 0, x + 0.05, AY0 + 4.0, 11.0, M['dark'])
box(-30.0, Y1 + 0.3, 5.4, X1 + 114, Y1 + 16, TOP, clad, 'Pier'); box(X0 - 0.3, Y1 + 0.3, TOP - 0.6, -30.0, Y1 + 16.3, TOP, clad)   # (the pier's outside; its west end cut open here: the walkway is modelled inside it)
# the Floor 2 arrivals corridor's top landing, straight from the gates' walkway onto the escalators

# ---------------------------------------------------------------- light, world, cameras
S.render.engine = 'CYCLES'
try:
    pr = bpy.context.preferences.addons['cycles'].preferences; pr.compute_device_type = 'OPTIX'; pr.get_devices()
    for d in pr.devices: d.use = True
    S.cycles.device = 'GPU'
except Exception as e: print('GPU', e)
S.cycles.samples = 128; S.cycles.use_denoising = True; S.cycles.max_bounces = 8
S.render.resolution_x, S.render.resolution_y = 1920, 1080
S.view_settings.view_transform = 'AgX'; S.view_settings.look = 'AgX - Medium High Contrast'; S.view_settings.exposure = -0.3
W = bpy.data.worlds.new('Sky'); S.world = W; W.use_nodes = True; wn = W.node_tree.nodes; sky = wn.new('ShaderNodeTexSky'); sky.sky_type = 'NISHITA'
sky.sun_elevation = math.radians(22); sky.sun_rotation = math.radians(250); W.node_tree.links.new(sky.outputs[0], wn['Background'].inputs[0]); wn['Background'].inputs[1].default_value = 0.35
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 3.0; so = bpy.data.objects.new('Sun', sun); S.collection.objects.link(so); so.rotation_euler = (math.radians(68), 0, math.radians(-110))
for x in range(-76, 0, 8):
    for y in range(20, 52, 8):
        l = bpy.data.lights.new('fill', 'AREA'); l.energy = 180; l.size = 4; l.color = (1.0, 0.94, 0.86)
        lo = bpy.data.objects.new('fill', l); S.collection.objects.link(lo); lo.location = (x, y, CEIL - 0.25)
for x in range(-76, 18, 10):
    l = bpy.data.lights.new('sfill', 'AREA'); l.energy = 300; l.size = 4; lo = bpy.data.objects.new('sfill', l); S.collection.objects.link(lo); lo.location = (x, 60, CEIL - 0.8)
CAMS = [
    ('1-from-the-stairs', (-75.2, 41.6, 3.8), (-40.0, 24.0, 0.4), 18, 'Halfway down the stair beside the escalators (west end): the claim hall, all five belts'),
    ('2-passenger-from-customs', (-36.0, 18.2, 1.65), (-44.0, 38.0, 1.1), 22, 'By the customs doorway looking back north: belts 2, 3 and 4, their screens, the escalators beyond'),
    ('3-belt-3-feed-shaft', (-32.1, 35.3, 1.55), (-34.9, 37.1, 0.75), 30, 'Close up at belt 3: a bag rising out of the feed shaft through the flaps onto the plates'),
    ('4-crew-input-sorting-hall', (-31.7, 60.8, 1.75), (-35.9, 52.6, 0.95), 24, 'Crew side, in the sorting hall: belt 3 and 4 input conveyors into the wall, a baggage cart at each'),
    ('5-escalators-from-below', (-72.5, 29.0, 1.65), (-77.0, 48.0, 5.0), 20, 'The escalators, stair and lift from the claim hall floor, up through the open void to the Floor 2 landing and the doorway into the gates walkway'),
    ('10-escalator-foot-newels', (-73.4, 32.4, 1.5), (-78.0, 35.6, 0.8), 28, 'At the foot of the escalators: the handrails running out past the combs and turning round their rounded newels'),
    ('11-escalator-top-newels', (-72.4, 51.2, 7.6), (-77.6, 48.6, 6.5), 26, 'On the Floor 2 landing: the escalators arriving, their handrails turning round the rounded newels at the top'),
    ('12-passenger-button', (-31.4, 32.4, 1.2), (-33.05, 34.2, 0.3), 32, 'The passengers start / stop button on belt 3s skirt, by the feed shaft'),
    ('9-from-the-gates-walkway', (-66.0, 62.0, 7.7), (-76.0, 47.0, 4.0), 20, 'Floor 2, in the gates walkway: through the 8 m doorway onto the landing and down the escalators into the claim hall'),
    ('6-arrivals-hall', (-70.0, -2.0, 1.7), (-40.0, 8.0, 1.6), 20, 'The arrivals hall: the customs exit (green channel), the meeting rail, car rental and taxi desks, the doors out'),
    ('7-outside-arrivals-doors', (-40.0, -34.0, 1.7), (-40.0, -4.0, 4.0), 24, 'Outside on the forecourt: the arrivals doors under the canopy, ARRIVALS over the glass'),
    ('8-outside-whole-wing', (-125.0, -75.0, 32.0), (-38.0, 14.0, 4.0), 28, 'From above the forecourt (south-west): the whole arrivals wing, flush with the end of the gates pier'),
]
out = []
ONLY = os.environ.get('CLAIM_ONLY')
for name, eye, at, lens, what in CAMS:
    if ONLY and not any(name.startswith(k) for k in ONLY.split(',')): continue
    cd = bpy.data.cameras.new(name); cd.lens = lens; co = bpy.data.objects.new(name, cd); S.collection.objects.link(co); co.location = eye
    co.rotation_euler = (Vector(at) - Vector(eye)).to_track_quat('-Z', 'Y').to_euler()
    S.camera = co; S.render.filepath = os.path.join(ROOT, 'docs', 'Pictures', 'BaggageClaim', 'Claim-%s.png' % name)
    bpy.ops.render.render(write_still=True)
    out.append({'name': name, 'from_plan_m': {'x': eye[0], 'z': eye[1], 'height': eye[2]}, 'to_plan_m': {'x': at[0], 'z': at[1], 'height': at[2]}, 'lens_mm': lens, 'what': what})
    print('RENDERED', name)
if not ONLY: json.dump(out, open(os.path.join(HERE, 'cameras.json'), 'w'), indent=1)
if not ONLY or os.environ.get('CLAIM_SAVE'):
    bpy.ops.file.pack_all()   # (its textures packed in: it opens with them on anywhere)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'BaggageClaim_concept.blend'))
print('CLAIM DONE')
