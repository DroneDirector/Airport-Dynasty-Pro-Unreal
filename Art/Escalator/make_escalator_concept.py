# The NEW escalator - concept (artist rendering) for the owner's approval (2026-11-18): modelled on his photos in
# Art/Escalator/Reference/Owner-2026-11-18 (1: the look - a pair with a centre deck, clear glass balustrades with a
# stainless clamp strip, black handrails running round big smooth round newels at BOTH ends and into inlets near the floor,
# stainless outer cladding; 2: top-down - steps with yellow edges, the centre deck with anti-slide studs; 3: the steps'
# grooved treads and risers). Same height as today (6.0 m, flush with both floors). Simplified shapes, rendered with
# outlines like a drawing; the detailed game model follows only after his OK.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Escalator/make_escalator_concept.py
import bpy, bmesh, math, os
from mathutils import Vector
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), '..', 'docs', 'Blueprints', 'EscalatorNew')
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene

# ---------------- the steps' line (as today: 6.0 m rise, 30 degrees, curves 1.0 / 2.6 m, three flat steps each end) ----------------
RISE, ANG, RL, RU, FLAT = 6.0, math.radians(30), 1.0, 2.6, 1.2
u0 = FLAT; u1 = u0 + RL * math.sin(ANG); h1 = RL * (1 - math.cos(ANG))
h2 = RISE - RU * (1 - math.cos(ANG)); u2 = u1 + (h2 - h1) / math.tan(ANG); u3 = u2 + RU * math.sin(ANG); UT = u3 + FLAT
def h(u):
    if u <= u0: return 0.0
    if u <= u1: return RL - math.sqrt(max(0, RL * RL - (u - u0) ** 2))
    if u <= u2: return h1 + (u - u1) * math.tan(ANG)
    if u <= u3: return RISE - RU + math.sqrt(max(0, RU * RU - (u3 - u) ** 2))
    return RISE
US = [-1.4 + i * 0.05 for i in range(int((UT + 2.8) / 0.05) + 1)]

# ---------------- materials (concept colours) ----------------
def mat(name, col, metal=0.0, rough=0.5, alpha=1.0):
    m = bpy.data.materials.new(name); m.use_nodes = True; b = m.node_tree.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Metallic'].default_value = metal; b.inputs['Roughness'].default_value = rough
    if alpha < 1: b.inputs['Alpha'].default_value = alpha; m.blend_method = 'BLEND' if hasattr(m, 'blend_method') else None
    m.diffuse_color = (*col, alpha); return m
M = {'steel': mat('Stainless', (0.78, 0.8, 0.82), 0.25, 0.35), 'step': mat('Step', (0.33, 0.35, 0.37), 0.05, 0.7),
     'yellow': mat('Yellow', (0.95, 0.75, 0.1)), 'rail': mat('Handrail', (0.04, 0.04, 0.045), 0.0, 0.45),
     'glass': mat('Glass', (0.75, 0.88, 0.95), 0.0, 0.05, 0.25), 'dark': mat('Dark', (0.1, 0.1, 0.11)),
     'floor': mat('Floor', (0.55, 0.53, 0.5), 0.0, 0.6), 'red': mat('Red', (0.85, 0.08, 0.06)), 'green': mat('Green', (0.1, 0.8, 0.3))}

def obj(name, me, m):
    o = bpy.data.objects.new(name, me); sc.collection.objects.link(o); o.data.materials.append(M[m]); return o
def mesh_from(verts, faces, name, m):
    me = bpy.data.meshes.new(name); me.from_pydata(verts, [], faces); me.update(); return obj(name, me, m)
def box(name, x0, x1, y0, y1, z0, z1, m):
    v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    return mesh_from(v, [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)], name, m)
def strip(name, a, b, m):
    v = a + b; n = len(a); return mesh_from(v, [(i, i + 1, n + i + 1, n + i) for i in range(n - 1)], name, m)
def sheet_x(name, x, pts, m, t=0.0):
    """a flat polygon in the plane x (pts in (u, z)); with t>0 a thin solid"""
    bm = bmesh.new(); vs = [bm.verts.new((x, p[0], p[1])) for p in pts]; f = bm.faces.new(vs)
    if t > 0:
        r = bmesh.ops.extrude_face_region(bm, geom=[f]); bmesh.ops.translate(bm, vec=(t, 0, 0), verts=[e for e in r['geom'] if isinstance(e, bmesh.types.BMVert)])
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free(); return obj(name, me, m)
def tube(name, pts, r, m, rect=None):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; sp = cu.splines.new('POLY'); sp.points.add(len(pts) - 1)
    for i, p in enumerate(pts): sp.points[i].co = (*p, 1)
    cu.bevel_depth = r; cu.bevel_resolution = 4; o = bpy.data.objects.new(name, cu); sc.collection.objects.link(o); o.data.materials.append(M[m]); return o

# ---------------- the floors: ground floor and the upper floor at 6.0 (the escalators sit flush in both) ----------------
box('GroundFloor', -6, 6, -8, UT + 6, -0.3, 0.0, 'floor')
box('UpperFloor', -6, 6, u3 + 0.6, UT + 8, RISE - 0.35, RISE, 'floor')
box('UpperFloorEdge', -6, 6, u3 + 0.55, u3 + 0.6, RISE - 0.35, RISE, 'dark')

# ---------------- one escalator, centred on x = cx ----------------
W, SK, GL, CL = 0.5, 0.506, 0.62, 0.80        # step half width, skirt, glass, outer cladding (half widths)
HR, NR = 0.95, 0.34                            # handrail height over the step line; the newels' radius (round, its return at 0.27 m - into the inlet)
UE0, UE1 = -0.55, UT + 0.55                    # where the handrail's straight run ends at each end
def escalator(cx, tag, outer_sides):
    # the steps: treads (grooved, dark, ~5 % reflective) with a yellow front edge and yellow side edges; risers
    k = 0; s = -0.8
    while s < UT + 0.8:
        a, b = s, s + 0.40; ha, hb = h(a), h(b)
        top = max(ha, hb) if hb - ha < 0.05 else hb
        box(f'{tag}Tread{k}', cx - W + 0.04, cx + W - 0.04, a, b - 0.01, top - 0.03, top, 'step')
        box(f'{tag}TreadEdge{k}', cx - W + 0.04, cx + W - 0.04, b - 0.05, b - 0.01, top + 0.0005, top + 0.002, 'yellow')
        for sx in (-1, 1): box(f'{tag}TreadSide{k}{sx}', cx + sx * (W - 0.06) - 0.02, cx + sx * (W - 0.06) + 0.02, a, b - 0.01, top + 0.0005, top + 0.002, 'yellow')
        if hb - ha > 0.05: box(f'{tag}Riser{k}', cx - W + 0.04, cx + W - 0.04, b - 0.012, b - 0.002, ha, hb, 'step')
        s = b; k += 1
    # the comb plates (yellow) and the ribbed stainless floor plates, flush with each floor
    for (ua, ub, z) in ((-1.35, -0.25, 0.0), (UT + 0.25, UT + 1.35, RISE)):
        box(f'{tag}FloorPlate{ua}', cx - SK, cx + SK, ua, ub, z - 0.01, z + 0.004, 'steel')
        for r in range(12): box(f'{tag}Rib{ua}{r}', cx - SK + 0.02, cx + SK - 0.02, ua + 0.05 + r * 0.085, ua + 0.07 + r * 0.085, z + 0.004, z + 0.007, 'steel')
    for (ua, ub, z) in ((-0.25, 0.0, 0.0), (UT, UT + 0.25, RISE)): box(f'{tag}Comb{ua}', cx - SK, cx + SK, ua, ub, z - 0.005, z + 0.01, 'yellow')
    for sx in (-1, 1):
        # the skirt (stainless) and the inner decking up to the glass clamp, along the whole length
        strip(f'{tag}Skirt{sx}', [(cx + sx * SK, u, h(min(max(u, 0), UT)) - 0.25) for u in US], [(cx + sx * SK, u, h(min(max(u, 0), UT)) + 0.10) for u in US], 'steel')
        strip(f'{tag}Deck{sx}', [(cx + sx * SK, u, h(min(max(u, 0), UT)) + 0.10) for u in US], [(cx + sx * (GL - 0.03), u, h(min(max(u, 0), UT)) + 0.14) for u in US], 'steel')
        # the glass clamp strip (stainless profile under the glass)
        strip(f'{tag}Clamp{sx}', [(cx + sx * (GL - 0.03), u, h(min(max(u, 0), UT)) + 0.14) for u in US], [(cx + sx * (GL + 0.03), u, h(min(max(u, 0), UT)) + 0.14) for u in US], 'steel')
        # the glass: inside the handrail all the way round both newels (D-shaped ends), down to the clamp
        def HH(u): return h(min(max(u, 0), UT)) + HR
        pts = [(u, HH(u) - 0.06) for u in US if UE0 <= u <= UE1]
        c1 = (UE1, HH(UE1) - NR); pts += [(UE1 + (NR - 0.06) * math.sin(a), c1[1] + (NR - 0.06) * math.cos(a)) for a in [i * math.pi / 40 for i in range(1, 41)]]
        pts += [(u, h(min(max(u, 0), UT)) + 0.14) for u in reversed(US) if UE0 - 0.1 <= u <= UE1 + 0.1]
        c0 = (UE0, HH(UE0) - NR); pts += [(UE0 - (NR - 0.06) * math.sin(a), c0[1] - (NR - 0.06) * math.cos(a)) for a in [i * math.pi / 40 for i in range(0, 40)]]
        sheet_x(f'{tag}Glass{sx}', cx + sx * GL, pts, 'glass', 0.012)
        # the handrail: a continuous loop - over the top, round each newel, back underneath into the inlet by the floor
        path = [(cx + sx * GL, u, HH(u)) for u in US if UE0 <= u <= UE1]
        path += [(cx + sx * GL, UE1 + NR * math.sin(a), c1[1] + NR * math.cos(a)) for a in [i * math.pi / 48 for i in range(1, 49)]]
        path += [(cx + sx * GL, UE1 - 0.30 * t, c1[1] - NR) for t in [i / 8 for i in range(1, 9)]]
        tube(f'{tag}RailTop{sx}', path, 0.04, 'rail')
        path0 = [(cx + sx * GL, UE0 - NR * math.sin(a), c0[1] + NR * math.cos(a)) for a in [i * math.pi / 48 for i in range(0, 49)]]
        path0 += [(cx + sx * GL, UE0 + 0.30 * t, c0[1] - NR) for t in [i / 8 for i in range(1, 9)]]
        tube(f'{tag}RailBottom{sx}', path0, 0.04, 'rail')
        # the inlets: black rubber boxes on the decking where the handrail goes back in (each newel's foot)
        for (ue, z) in ((UE1 - 0.30, RISE), (UE0 + 0.30, 0.0)):        # (the handrail's return runs into its mouth)
            box(f'{tag}Inlet{ue}{sx}', cx + sx * GL - 0.065, cx + sx * GL + 0.065, ue - 0.10, ue + 0.10, z + 0.14, z + 0.36, 'dark')
            box(f'{tag}InletLip{ue}{sx}', cx + sx * GL - 0.07, cx + sx * GL + 0.07, ue - 0.11, ue + 0.11, z + 0.355, z + 0.37, 'steel')
        # the outer cladding (stainless panels) on the outer sides, from the decking down to under the truss
        if sx in outer_sides:
            xo = cx + sx * CL
            strip(f'{tag}OuterDeck{sx}', [(cx + sx * (GL + 0.03), u, h(min(max(u, 0), UT)) + 0.14) for u in US], [(xo, u, h(min(max(u, 0), UT)) + 0.14) for u in US], 'steel')
            strip(f'{tag}Clad{sx}', [(xo, u, h(min(max(u, 0), UT)) + 0.14) for u in US if 0.0 <= u <= UT], [(xo, u, h(min(max(u, 0), UT)) - 1.05) for u in US if 0.0 <= u <= UT], 'steel')
            for u in [i * 1.2 for i in range(1, int(UT / 1.2))]:      # the panel joints
                strip(f'{tag}Joint{u}{sx}', [(xo + sx * 0.002, u, h(u) + 0.1)], [(xo + sx * 0.002, u, h(u) - 1.0)], 'dark') if False else None
    # the emergency stop and the key switch on the skirt at each end, a direction light
    for (u, z) in ((-0.15, 0.0), (UT + 0.15, RISE)):
        box(f'{tag}EStop{u}', cx - SK - 0.005, cx - SK + 0.0, u - 0.04, u + 0.04, z + 0.16, z + 0.24, 'red')
        box(f'{tag}Key{u}', cx + SK, cx + SK + 0.005, u - 0.03, u + 0.03, z + 0.18, z + 0.22, 'dark')
        box(f'{tag}Light{u}', cx - 0.15, cx + 0.15, u - 1.40, u - 1.36 if u < 0 else u + 1.40, z + 0.004, z + 0.012, 'green') if False else None

escalator(-1.05, 'A', (-1,))
escalator(1.05, 'B', (1,))
# the centre deck between the pair: one stainless slope with anti-slide studs (his top-down photo)
CD = (-1.05 + GL + 0.03, 1.05 - GL - 0.03)
strip('CentreDeck', [(CD[0], u, h(min(max(u, 0), UT)) + 0.14) for u in US], [(CD[1], u, h(min(max(u, 0), UT)) + 0.14) for u in US], 'steel')
for u in [1.5 + i * 1.6 for i in range(int((UT - 2) / 1.6))]:
    z = h(u) + 0.14
    box(f'Stud{u}', -0.1, 0.1, u - 0.02, u + 0.02, z, z + 0.03, 'steel')

# ---------------- the drawing look: outlines (Freestyle), soft light, white paper ----------------
sc.render.engine = 'BLENDER_EEVEE_NEXT' if 'BLENDER_EEVEE_NEXT' in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE'
sc.render.use_freestyle = True; sc.render.line_thickness = 1.3
for _l in list(sc.view_layers[0].freestyle_settings.linesets): sc.view_layers[0].freestyle_settings.linesets.remove(_l)
ls = sc.view_layers[0].freestyle_settings.linesets.new('Lines'); ls.linestyle = bpy.data.linestyles.new('Ink'); ls.linestyle.color = (0.08, 0.09, 0.1); ls.select_by_visibility = True
ls.select_crease = True; ls.select_silhouette = True; ls.select_border = True
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Color'].default_value = (0.92, 0.93, 0.95, 1); w.node_tree.nodes['Background'].inputs['Strength'].default_value = 1.3
sun = bpy.data.objects.new('Sun', bpy.data.lights.new('Sun', 'SUN')); sc.collection.objects.link(sun); sun.rotation_euler = (math.radians(50), 0, math.radians(30)); sun.data.energy = 3.0
sc.view_settings.view_transform = 'Standard'
sc.render.resolution_x, sc.render.resolution_y = 3200, 1800; sc.render.film_transparent = False
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
def shot(name, loc, at, lens=35, ortho=None):
    cam.location = loc; cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    if ortho: cam.data.type = 'ORTHO'; cam.data.ortho_scale = ortho
    else: cam.data.type = 'PERSP'; cam.data.lens = lens
    sc.render.filepath = os.path.join(OUT, name + '.png'); bpy.ops.render.render(write_still=True); print('SHOT', sc.render.filepath)
shot('1-front', (0.0, -7.5, 1.7), (0.0, 2.0, 1.6), 30)                          # like his first photo
shot('2-side', (12.0, UT / 2, RISE / 2 + 0.3), (0.0, UT / 2, RISE / 2 + 0.3), ortho=UT + 6.5)
shot('3-newel-bottom', (-3.2, -2.4, 1.3), (-1.4, -0.3, 0.6), 35)
shot('4-newel-top', (-3.4, UT + 3.4, RISE + 1.6), (-1.4, UT + 0.3, RISE + 0.6), 35)
shot('5-from-above', (0.0, -2.0, RISE + 4.5), (0.0, UT / 2, RISE / 2), 28)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'EscalatorConcept.blend'))
print('CONCEPT done')
