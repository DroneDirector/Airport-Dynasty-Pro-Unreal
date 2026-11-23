# A look at the proposed remote stands before the owner's OK (2026-11-23): the 8 remote stands behind the gates on their
# own concrete pads, with grass strips between every pad and every taxiway edge (taxilane T, links P1-P3, taxiway A), the
# real-world stand markings (yellow lead-in lines and stop bars, red equipment restraint lines, stand numbers, the
# bus road behind the noses with its crossings, the bus depot with chargers), drawn to scale in plan metres from
# Data/airfield.json. Photo-scanned CC0 ground (Poly Haven, Art/Ground/Scans). Writes Blueprints/Airfield/RemoteStands-*.png.
#   python3 Tools/render_remote_stands.py [--samples=32]
import bpy, bmesh, math, os, sys
from mathutils import Vector
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Blueprints', 'Airfield'); os.makedirs(OUT, exist_ok=True)
SAMPLES = int(next((a.split('=')[1] for a in sys.argv if a.startswith('--samples=')), '32'))
bpy.ops.wm.read_factory_settings(use_empty=True); sc = bpy.context.scene
SCANS = os.path.join(ROOT, 'Art', 'Ground', 'Scans')

# #region ── Materials ──
def scan(name, folder, tile, tint=(1, 1, 1), rough=None):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; b = nt.nodes['Principled BSDF']
    tc = nt.nodes.new('ShaderNodeTexCoord'); mp = nt.nodes.new('ShaderNodeMapping'); mp.inputs['Scale'].default_value = (1 / tile, 1 / tile, 1)
    nt.links.new(tc.outputs['Object'], mp.inputs['Vector'])
    f = [x for x in os.listdir(os.path.join(SCANS, folder)) if '_diff_' in x][0]
    t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(SCANS, folder, f)); nt.links.new(mp.outputs[0], t.inputs[0])
    mx = nt.nodes.new('ShaderNodeMixRGB'); mx.blend_type = 'MULTIPLY'; mx.inputs[0].default_value = 1; mx.inputs[2].default_value = (*tint, 1)
    nt.links.new(t.outputs[0], mx.inputs[1]); nt.links.new(mx.outputs[0], b.inputs['Base Color'])
    b.inputs['Roughness'].default_value = rough if rough is not None else 0.85
    return m
def flat(name, rgb, rough=0.6, emit=0.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*rgb, 1); b.inputs['Roughness'].default_value = rough
    return m
GRASS = scan('Grass', 'grass_ground', 3.0, (0.75, 0.85, 0.7))
ASPH = scan('Asphalt', 'asphalt_04', 4.0, (0.55, 0.56, 0.58))
SHOULDER = scan('Shoulder', 'asphalt_02', 4.0, (0.62, 0.62, 0.62))
CONC = scan('StandConcrete', 'concrete_floor_worn_001', 5.0, (0.95, 0.95, 0.95))
CONC2 = scan('ApronConcrete', 'brushed_concrete_04', 5.0, (0.9, 0.9, 0.9))
YEL = flat('MarkYellow', (0.85, 0.55, 0.02), 0.5); RED = flat('MarkRed', (0.65, 0.03, 0.02), 0.5)
WHT = flat('MarkWhite', (0.85, 0.85, 0.82), 0.5); BLK = flat('MarkBlack', (0.02, 0.02, 0.02), 0.6)
AC = flat('Aircraft', (0.85, 0.87, 0.9), 0.35); TERM = flat('Terminal', (0.55, 0.6, 0.66), 0.5); GLASSB = flat('TermGlass', (0.08, 0.12, 0.16), 0.1)
BUSY = flat('Bus', (0.9, 0.56, 0.01), 0.35); CHG = flat('Charger', (0.1, 0.3, 0.6), 0.4)
# #endregion

# #region ── Helpers: paved rectangles (with a little height order), painted lines, text ──
def quad(name, pts, z, m):
    bm = bmesh.new(); f = bm.faces.new([bm.verts.new((x, y, z)) for x, y in pts]); bmesh.ops.recalc_face_normals(bm, faces=[f])
    if f.normal.z < 0: f.normal_flip()
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); o.data.materials.append(m); return o
def rect(name, x0, y0, x1, y1, z, m): return quad(name, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], z, m)
def line(name, pts, w, m, z=0.06, dash=None):
    segs = []
    for (ax_, ay), (bx, by) in zip(pts, pts[1:]):
        L = math.hypot(bx - ax_, by - ay)
        if dash:
            on, off = dash; t = 0.0
            while t < L:
                t1 = min(L, t + on); segs.append(((ax_ + (bx - ax_) * t / L, ay + (by - ay) * t / L), (ax_ + (bx - ax_) * t1 / L, ay + (by - ay) * t1 / L))); t += on + off
        else: segs.append(((ax_, ay), (bx, by)))
    bm = bmesh.new()
    for (ax_, ay), (bx, by) in segs:
        dx, dy = bx - ax_, by - ay; L = math.hypot(dx, dy) or 1; nx, ny = -dy / L * w / 2, dx / L * w / 2
        bm.faces.new([bm.verts.new(v) for v in ((ax_ + nx, ay + ny, z), (bx + nx, by + ny, z), (bx - nx, by - ny, z), (ax_ - nx, ay - ny, z))])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); o.data.materials.append(m); return o
def box_outline(name, x0, y0, x1, y1, w, m, z=0.06, dash=None): return line(name, [(x0, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0)], w, m, z, dash)
def text(t, x, y, size, m, rot=0.0, z=0.07):
    cu = bpy.data.curves.new('T', 'FONT'); cu.body = t; cu.size = size; cu.align_x = 'CENTER'; cu.align_y = 'CENTER'
    o = bpy.data.objects.new('Text_' + t, cu); sc.collection.objects.link(o); o.location = (x, y, z); o.rotation_euler = (0, 0, math.radians(rot)); o.data.materials.append(m)
def solid(name, c, s, m):
    bpy.ops.mesh.primitive_cube_add(location=c); o = bpy.context.active_object; o.name = name; o.scale = (s[0] / 2, s[1] / 2, s[2] / 2); o.data.materials.append(m); return o
# #endregion

# #region ── Ground, the existing apron and taxiways ──
rect('Grass', -340, 20, 640, 440, 0.0, GRASS)
rect('ContactApron', -265, 82, 570, 238, 0.02, CONC2)
solid('Terminal', (155, 56, 7), (792, 52, 14), TERM); solid('TerminalGlass', (155, 82.1, 6.5), (780, 0.3, 8), GLASSB)
rect('T', -255, 205.5, 560, 230.5, 0.03, ASPH)
rect('A_shoulders', -340, 366, 640, 406, 0.02, SHOULDER); rect('A', -340, 373.5, 640, 398.5, 0.03, ASPH)
for nm, x in (('P1', -130), ('P2', 150), ('P3', 362)):
    rect(nm + '_shoulders', x - 20, 238, x + 20, 366, 0.02, SHOULDER); rect(nm, x - 12.5, 230.5, x + 12.5, 373.5, 0.03, ASPH)
    line(nm + '_cl', [(x, 218), (x, 386)], 0.3, YEL, 0.05)
    for s in (-1, 1): line(nm + f'_edge{s}', [(x + s * 12.6, 238), (x + s * 12.6, 366)], 0.15, YEL, 0.05)
line('T_cl', [(-255, 218), (560, 218)], 0.3, YEL, 0.05); line('A_cl', [(-340, 386), (640, 386)], 0.3, YEL, 0.05)
for s in (-1, 1): line(f'A_edge{s}', [(-340, 386 + s * 12.6), (640, 386 + s * 12.6)], 0.15, YEL, 0.05)
line('ServiceLane', [(-255, 176), (560, 176)], 0.15, WHT, 0.05, dash=(3, 3))
# the gate stands (context): their lead-in lines and red restraint lines
import json
D = json.load(open(os.path.join(ROOT, 'Data', 'airfield.json'), encoding='utf-8'))
for s in D['stands']:
    if s.get('cargo'): continue
    x, n, sp, L = s['x'], s['nose'], s['span'], s['len']
    line('GateLead' + s['id'], [(x, 218), (x, n)], 0.15, YEL, 0.05)
    box_outline('GateERA' + s['id'], x - sp / 2 - 1.5, n - 3, x + sp / 2 + 1.5, n + L + 1.5, 0.2, RED, 0.05)
# #endregion

# #region ── The remote stands: pads with grass round them, throats to taxilane T, markings ──
STANDS = [('R1', -68.5, 'C'), ('R2', -28.0, 'C'), ('R3', 35.75, 'E'), ('R4', 231.75, 'E'), ('R5', 292.5, 'C'), ('R6', 445.5, 'E'), ('R7', 518.0, 'E')]
STANDS = [('R1', -68.5, 'C'), ('R2', -28.0, 'C'), ('R3', 35.75, 'E'), ('R4', 231.75, 'E'), ('R5', 292.5, 'C'), ('R6', 445.5, 'E'), ('R7', 518.0, 'E'), ('R8', 0, '')]
STANDS = STANDS[:7]
PADS = [(-96.5, 84.0), (183.5, 320.5), (403.0, 560.5)]
PY0, PY1 = 244.0, 342.5                       # the pads: 5.5 m of grass from T's paved edge, 23.5 m short of taxiway A's shoulder
for i, (x0, x1) in enumerate(PADS): rect(f'RemotePad{i}', x0, PY0, x1, PY1, 0.025, CONC)
DIM = {'C': (35.8, 44.5, 272.0, 'A321'), 'E': (64.8, 73.9, 258.0, 'A350 / 777')}
for nm, x, code in STANDS:
    span, L, tail, lab = DIM[code]; nose = tail + L
    rect(nm + '_throat', x - (16 if code == 'C' else 26), 237.9, x + (16 if code == 'C' else 26), 244.1, 0.024, CONC)   # paved way in from T
    r = 34.0                                                                     # lead-in: off T's centre line in a curve
    arc = [(x - r + r * math.sin(a), 218 + r - r * math.cos(a)) for a in [k * math.pi / 2 / 12 for k in range(13)]]
    line(nm + '_lead', arc + [(x, nose - 1.0)], 0.3, YEL, 0.065)
    line(nm + '_stop', [(x - 3.0, nose - 1.0), (x + 3.0, nose - 1.0)], 0.9, YEL, 0.066)
    box_outline(nm + '_ERA', x - span / 2 - 3, tail - 3, x + span / 2 + 3, nose + 2, 0.25, RED, 0.064)
    rect(nm + '_numbox', x - 5, tail - 12, x + 5, tail - 6, 0.062, YEL); text(nm, x, tail - 9, 4.2, BLK, z=0.068)
    text(lab, x + 9, nose - 3, 1.6, YEL, z=0.068)
    for k in (-1, 1):                                                            # GSE staging boxes beside the tail, outside the red line
        box_outline(nm + f'_gse{k}', x + k * (span / 2 + 4) - 2.5, tail - 2, x + k * (span / 2 + 4) + 2.5, tail + 10, 0.15, WHT, 0.063)
# the bus road behind the noses (two lanes, 7 m), crossing the links with ladder crossings and stop lines
RY0, RY1 = 335.0, 342.0
for i, (x0, x1) in enumerate([(-245, 560.5)]):
    rect('BusRoad', x0, RY0, x1, RY1, 0.03, ASPH)
    for s in (RY0 + 0.2, RY1 - 0.2): line(f'BusRoadEdge{s}', [(x0, s), (x1, s)], 0.15, WHT, 0.07)
    line('BusRoadCL', [(x0, (RY0 + RY1) / 2), (x1, (RY0 + RY1) / 2)], 0.15, WHT, 0.07, dash=(3, 6))
for x in (-130, 150, 362):
    for k in range(-10, 11, 2): rect(f'Zebra{x}_{k}', x + k - 0.5, RY0 + 0.3, x + k + 0.5, RY1 - 0.3, 0.075, WHT)
    for s in (-1, 1): line(f'StopLine{x}{s}', [(x + s * 22, RY0), (x + s * 22, RY1)], 0.5, WHT, 0.075); text('STOP', x + s * 26, (RY0 + RY1) / 2, 2.2, WHT, rot=90 * s, z=0.076)
# the bus depot (west end): 6 charging bays, 3 buses parked
rect('Depot', -240, 244, -160, 334.9, 0.025, CONC); rect('DepotLink', -250, 230.4, -240, 342, 0.03, ASPH)
line('Crossing', [(-245, 176), (-245, 244)], 0.15, WHT, 0.07, dash=(2, 2))
for k in range(6):
    xb = -232 + k * 12
    box_outline(f'Bay{k}', xb - 2, 262, xb + 2, 278, 0.15, WHT, 0.07); solid(f'Charger{k}', (xb, 259.5, 1.0), (0.8, 0.5, 2.0), CHG)
    if k < 3: solid(f'Bus{k}', (xb, 270, 1.55), (3.0, 13.9, 3.1), BUSY)
text('BUS DEPOT', -200, 300, 5, WHT)
# #endregion

# #region ── A few aircraft for scale ──
def aircraft(name, x, nose, span, L, fus, north=True):
    s = 1 if north else -1; cx = x; cy = nose - s * L / 2
    bpy.ops.mesh.primitive_cylinder_add(radius=fus / 2, depth=L - fus, location=(cx, cy, 3.0 + fus / 2), rotation=(math.pi / 2, 0, 0)); o = bpy.context.active_object; o.data.materials.append(AC)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=fus / 2, location=(cx, nose - s * fus / 2, 3.0 + fus / 2)); o = bpy.context.active_object; o.scale = (1, 1.4, 1); o.data.materials.append(AC)
    wy = nose - s * 0.45 * L
    bm = bmesh.new(); z = 2.6 + fus * 0.3
    pts = [(cx - span / 2, wy - s * 0.17 * L), (cx, wy + s * 0.05 * L), (cx + span / 2, wy - s * 0.17 * L), (cx + span / 2, wy - s * 0.21 * L), (cx, wy - s * 0.10 * L), (cx - span / 2, wy - s * 0.21 * L)]
    bm.faces.new([bm.verts.new((px, py, z)) for px, py in pts])
    me = bpy.data.meshes.new(name + 'Wing'); bm.to_mesh(me); bm.free(); w = bpy.data.objects.new(name + 'Wing', me); sc.collection.objects.link(w); w.data.materials.append(AC)
    sd = w.modifiers.new('s', 'SOLIDIFY'); sd.thickness = 0.5
    solid(name + 'Fin', (cx, nose - s * (L - 4), 3 + fus + 4), (0.4, 7, 8), AC)
    solid(name + 'Stab', (cx, nose - s * (L - 3), 3 + fus * 0.8), (span * 0.34, 3.5, 0.3), AC)
aircraft('B777onR3', 35.75, 258 + 73.9, 64.8, 73.9, 6.2); aircraft('A321onR1', -68.5, 318, 35.8, 44.5, 4.0)
aircraft('A350onR6', 445.5, 258 + 66.8, 64.75, 66.8, 6.0); aircraft('A321onR5', 292.5, 318, 35.8, 44.5, 4.0)
solid('BusAtR3', (35.75 - 9, 300, 1.55), (3.0, 13.9, 3.1), BUSY)
# #endregion

# #region ── Sky, sun, cameras, render ──
w = bpy.data.worlds.new('Sky'); w.use_nodes = True; sc.world = w; nt = w.node_tree; bg = nt.nodes['Background']
sky = nt.nodes.new('ShaderNodeTexSky')
for t in ('NISHITA', 'MULTIPLE_SCATTERING', 'SINGLE_SCATTERING', 'PREETHAM', 'HOSEK_WILKIE'):
    try: sky.sky_type = t; break
    except Exception: pass
for k, v in (('sun_elevation', math.radians(35)), ('sun_rotation', math.radians(200))):
    if hasattr(sky, k): setattr(sky, k, v)
nt.links.new(sky.outputs[0], bg.inputs['Color']); bg.inputs['Strength'].default_value = 0.15
sd = bpy.data.lights.new('Sun', 'SUN'); sd.energy = 3.2; sd.angle = math.radians(1.5)
so = bpy.data.objects.new('Sun', sd); sc.collection.objects.link(so); so.rotation_euler = (math.radians(55), 0, math.radians(200))
sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = SAMPLES; sc.cycles.use_denoising = True
sc.view_settings.view_transform = 'Standard'; sc.view_settings.exposure = -0.9
cam_d = bpy.data.cameras.new('Cam'); cam = bpy.data.objects.new('Cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam; cam_d.clip_end = 3000
def shoot(name, loc, at, lens=35, ortho=0, res=(2560, 1440)):
    cam.location = loc; cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    cam_d.type = 'ORTHO' if ortho else 'PERSP'; cam_d.lens = lens; cam_d.ortho_scale = ortho or 1
    sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = os.path.join(OUT, f'RemoteStands-{name}.png')
    bpy.ops.render.render(write_still=True); print('rendered', name, flush=True)
shoot('Aerial', (150, 40, 230), (190, 300, 0), 22)
shoot('Closeup', (-150, 190, 60), (-20, 300, 0), 28)
cam.rotation_euler = (0, 0, 0); cam.location = (230, 290, 600); cam_d.type = 'ORTHO'; cam_d.ortho_scale = 900
sc.render.resolution_x, sc.render.resolution_y = (3000, 1300); sc.render.filepath = os.path.join(OUT, 'RemoteStands-Top.png')
bpy.ops.render.render(write_still=True); print('rendered Top', flush=True)
# #endregion
