# The gates' seating, our own design and model (made here in Blender, nothing bought or copied): linked seats on a
# beam, each a black shell seat with a reclined back, standing on cast X-legs whose upper arms make the armrests, a
# black triangular hub where the legs cross, round floor pads; the metal 75% matte chrome steel. Three pieces, placed
# along each row by the game (BabylonModel.cpp ReplaceSeats):
#  - SeatModule: one seat (0.62 m wide, as the Babylon rows' pitch) with its shells, its length of beam and bracket
#  - LegModule:  an X-leg with its armrest, hub and pads (at the rows' ends and every third seat)
#  - ArmModule:  an armrest alone, fixed to the beam (between the other seats)
# Axes as the Babylon model's glTF (so the game places them as it places the rows): Blender x across the row, -y the
# way a seated person faces, z up; metres. Bevelled edges and round tubes throughout (Nanite keeps all of it).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_gate_seat.py
import bpy, bmesh, math, os
from mathutils import Vector, Matrix

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Art', 'GateSeat', 'gateseat.glb')
os.makedirs(os.path.dirname(OUT), exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)

def mat(name, col, metal, rough):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (col, col, col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    return m
SHELL = mat('Shell', 0.028, 0.0, 0.62)       # the seat and back: black, softly matte
METAL = mat('Metal', 0.75, 1.0, 0.36)        # legs, arms, beam: 75% matte chrome steel
BLACK = mat('Black', 0.015, 0.0, 0.5)        # the hubs and floor pads

def new_obj(name, bm, m):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(o)
    o.data.materials.append(m)
    return o

def add_mod(o, kind, **kw):
    md = o.modifiers.new(kind, kind)
    for k, v in kw.items(): setattr(md, k, v)
    return md

def apply_all(o):
    bpy.context.view_layer.objects.active = o
    for md in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=md.name)

def sheet(name, nx, ny, f, thick, m, bevel=0.005):
    """A curved panel: f(u, v) -> point, u, v in 0..1; given thickness; edges rounded."""
    bm = bmesh.new()
    vs = [[bm.verts.new(f(i / nx, j / ny)) for i in range(nx + 1)] for j in range(ny + 1)]
    for j in range(ny):
        for i in range(nx): bm.faces.new((vs[j][i], vs[j][i + 1], vs[j + 1][i + 1], vs[j + 1][i]))
    o = new_obj(name, bm, m)
    add_mod(o, 'SOLIDIFY', thickness=thick, offset=0.0)
    add_mod(o, 'BEVEL', width=bevel, segments=3, limit_method='ANGLE')
    add_mod(o, 'WEIGHTED_NORMAL')
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(40)
    apply_all(o); return o

def box(name, x0, y0, z0, x1, y1, z1, m, bevel=0.005):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts: v.co = Vector(((x0 + x1) / 2 + v.co.x * (x1 - x0), (y0 + y1) / 2 + v.co.y * (y1 - y0), (z0 + z1) / 2 + v.co.z * (z1 - z0)))
    o = new_obj(name, bm, m)
    add_mod(o, 'BEVEL', width=bevel, segments=3)
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(40)
    apply_all(o); return o

def tube(name, a, b, r, m, sides=20):
    a, b = Vector(a), Vector(b); d = b - a
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=sides, radius1=r, radius2=r, depth=d.length)
    rot = d.normalized().to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((a + b) / 2) @ rot, verts=bm.verts)
    o = new_obj(name, bm, m)
    add_mod(o, 'BEVEL', width=r * 0.3, segments=2, limit_method='ANGLE')
    o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(50)
    apply_all(o); return o

def ball(name, at, r, m):
    bm = bmesh.new(); bmesh.ops.create_uvsphere(bm, u_segments=20, v_segments=12, radius=r)
    bmesh.ops.translate(bm, vec=Vector(at), verts=bm.verts)
    o = new_obj(name, bm, m);
    for p in o.data.polygons: p.use_smooth = True
    return o

def pad(name, at, m):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=24, radius1=0.034, radius2=0.028, depth=0.018)
    bmesh.ops.translate(bm, vec=Vector(at) + Vector((0, 0, 0.009)), verts=bm.verts)
    o = new_obj(name, bm, m); add_mod(o, 'BEVEL', width=0.004, segments=2); apply_all(o); return o

def tri_hub(name, at, size, m):
    """The black triangular cap where the legs cross (a rounded triangle, a little thicker than the tubes)."""
    bm = bmesh.new(); pts = []
    for k in range(3):
        a = math.radians(90 + k * 120)
        pts.append(bm.verts.new(Vector(at) + Vector((0, math.cos(a) * size, math.sin(a) * size))))
    f = bm.faces.new(pts)
    bmesh.ops.solidify(bm, geom=[f], thickness=0.06)
    bmesh.ops.translate(bm, vec=Vector((-0.03, 0, 0)), verts=bm.verts)
    o = new_obj(name, bm, m); add_mod(o, 'BEVEL', width=0.012, segments=4); apply_all(o)
    for p in o.data.polygons: p.use_smooth = True
    return o

def join(name, objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join(); objs[0].name = name; objs[0].data.name = name
    return objs[0]

HW = 0.275   # half the seat's width
# ---- the seat ----
def seat_pan(u, v):
    x = -HW + 2 * HW * u
    y = 0.20 - 0.47 * v                                      # rear (y 0.20) to front (y -0.27)
    z = 0.44 + 0.012 * (x / HW) ** 2 - 0.02 * max(0, v - 0.82) / 0.18 - 0.012 * (1 - v)   # dished, the front rolled down
    return Vector((x, y, z))
def seat_back(u, v):
    x = -HW + 2 * HW * u
    a = math.radians(16)
    s = 0.64 * v
    bulge = 0.022 * math.sin(math.pi * min(1, v * 1.4))       # the lumbar curve
    y = 0.225 + math.sin(a) * s - bulge * math.cos(a)
    z = 0.50 + math.cos(a) * s
    return Vector((x + 0.0, y, z - 0.0 * (x / HW) ** 2))
parts = [sheet('pan', 16, 12, seat_pan, 0.022, SHELL), sheet('back', 16, 14, seat_back, 0.02, SHELL)]
parts.append(box('beam', -0.31, -0.01, 0.30, 0.31, 0.07, 0.36, METAL, 0.008))
parts.append(box('bracket', -0.07, -0.02, 0.36, 0.07, 0.15, 0.418, METAL, 0.006))
parts.append(box('backarm', -0.035, 0.07, 0.32, 0.035, 0.23, 0.52, METAL, 0.006))
seat = join('SeatModule', parts)

# ---- the legs: an X seen from the side, its upper arms the armrest ----
HUB = (0.0, 0.03, 0.30)
FF, FB = (0.0, -0.36, 0.0), (0.0, 0.42, 0.0)
AF, AB = (0.0, -0.26, 0.63), (0.0, 0.20, 0.655)
R = 0.017
leg = [tube('lf', HUB, (FF[0], FF[1], 0.018), R, METAL), tube('lb', HUB, (FB[0], FB[1], 0.018), R, METAL),
       tube('uf', HUB, AF, R, METAL), tube('ub', HUB, AB, R, METAL), tube('bar', AB, AF, 0.019, METAL),
       ball('j1', AF, 0.021, METAL), ball('j2', AB, 0.021, METAL), ball('j3', HUB, 0.03, METAL),
       pad('pf', FF, BLACK), pad('pb', FB, BLACK), tri_hub('hub', (0.03, 0.03, 0.30), 0.06, BLACK)]
join('LegModule', leg)
# ---- an armrest alone, on the beam ----
arm = [tube('uf2', HUB, AF, R, METAL), tube('ub2', HUB, AB, R, METAL), tube('bar2', AB, AF, 0.019, METAL),
       ball('k1', AF, 0.021, METAL), ball('k2', AB, 0.021, METAL), ball('k3', HUB, 0.03, METAL)]
join('ArmModule', arm)

bpy.ops.export_scene.gltf(filepath=OUT, export_format='GLB', export_apply=True, export_yup=True)
print('GATESEAT written', OUT)
# the Blender file next to the model (owner's rule: every in-game object's .blend in Art/<Object>/, kept up to date)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(os.path.dirname(OUT), 'GateSeat.blend'))
print('SAVED', os.path.join(os.path.dirname(OUT), 'GateSeat.blend'))
