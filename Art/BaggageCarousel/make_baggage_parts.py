# The baggage claim's game parts (to-do 72; the look agreed in Art/BaggageClaim/render_claim_concept.py), each its own
# FBX for Tools/import_baggage.py; material slots named after the terminal's own finishes (/Game/Terminal/Materials) so
# they match the building. Blender X = plan x, Blender Y = plan z (north), Z up; metres.
#   SM_BeltBody     one carousel's still parts, centred on its middle: the stainless skirt and black kick, the trim under
#                   the plates' edge, the island (its feed-shaft opening at x -0.45..1.4, y 5.4..6.6), the shaft's dark
#                   walls down 0.8 m, the incline frame, the stainless hood with its top and the screen's hanger
#   SM_BeltPlate    one rubber plate (0.92 x 0.285 m, 3 cm), its middle at the origin (the game moves them round)
#   SM_BeltIncline  the feed incline's rubber surface (still: the bags ride up it)
#   SM_Flap         one rubber flap strip (0.12 m wide, 0.47 m long), hung from the origin (its hinge) downwards
#   SM_BeltButton   the green push button (5.5 cm radius, 4 cm deep), facing +X; SM_ButtonPlate its dark plate + ring
#   SM_CrewBox      the crew's yellow control box on its post (the button faces +Y), its foot at the origin
#   SM_InputConv    a belt-input conveyor, 1 m long along +Y (the game stretches it): frame, rubber top, side rails
#   SM_WallFrame    the yellow frame round a belt input's wall opening (1.6 x 1.85 m), in the plane y 0
#   SM_Bag0..3      two hard-shell cases, a soft case and a duffel (their bottoms at z 0); SM_BeltScreen the screens' dark frame
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/BaggageCarousel/make_baggage_parts.py
import bpy, bmesh, math, os
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
L, RC = 15.0, 1.8
def mat(n): return bpy.data.materials.get(n) or bpy.data.materials.new(n)
class Part:
    def __init__(s, name): s.name = name; s.objs = []
    def box(s, x0, y0, z0, x1, y1, z1, m):
        bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)); o = bpy.context.object
        o.scale = (abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)); bpy.ops.object.transform_apply(scale=True); o.data.materials.append(mat(m)); s.objs.append(o); return o
    def prism(s, pts, z0, z1, m, cap=True):
        me = bpy.data.meshes.new('p'); bm = bmesh.new()
        lo = [bm.verts.new((p[0], p[1], z0)) for p in pts]; hi = [bm.verts.new((p[0], p[1], z1)) for p in pts]
        if cap: bm.faces.new(lo[::-1]); bm.faces.new(hi)
        n = len(pts)
        for i in range(n): bm.faces.new((lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i]))
        bm.normal_update(); bm.to_mesh(me); bm.free()
        o = bpy.data.objects.new('p', me); bpy.context.scene.collection.objects.link(o); me.materials.append(mat(m)); s.objs.append(o); return o
    def cyl(s, r, d, loc, rot, m, v=24):
        bpy.ops.mesh.primitive_cylinder_add(vertices=v, radius=r, depth=d, location=loc, rotation=rot); o = bpy.context.object; o.data.materials.append(mat(m)); s.objs.append(o); return o
    def finish(s):
        for x in bpy.context.selected_objects: x.select_set(False)
        for o in s.objs:
            o.select_set(True); bpy.context.view_layer.objects.active = o
            for md in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=md.name)
            o.select_set(False)
        # (owner 2026-11-18, to-do 102: the brushed metal was stretched - every box face had the whole picture, however long)
        # every face mapped at real size, by the axis it faces: 1 m of surface = one tile, the brushing running sideways;
        # seamless across faces facing the same way
        for o in s.objs:
            me = o.data; M = o.matrix_world
            if not me.uv_layers: me.uv_layers.new(name='UVMap')
            uv = me.uv_layers.active.data
            for poly in me.polygons:
                n = poly.normal; ax = max(range(3), key=lambda i: abs(n[i]))
                for li in poly.loop_indices:
                    w = M @ me.vertices[me.loops[li].vertex_index].co
                    if ax == 2: uv[li].uv = (w.x, w.y)        # up/down faces: plan x, plan z
                    elif ax == 0: uv[li].uv = (w.y, w.z)      # faces looking along x: along y, up
                    else: uv[li].uv = (w.x, w.z)              # faces looking along y: along x, up
        for o in s.objs: o.select_set(True)
        bpy.context.view_layer.objects.active = s.objs[0]; bpy.ops.object.join()
        ob = bpy.context.object; ob.name = s.name
        me = ob.data; bm = bmesh.new(); bm.from_mesh(me); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me); bm.free()
        assert all(sl.material for sl in ob.material_slots)
        bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, s.name + '.fbx'), use_selection=True, object_types={'MESH'},
                                 apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
        print('PART', s.name, len(me.polygons), [m.name for m in me.materials])
        bpy.data.objects.remove(ob, do_unlink=True)
def track(r, l=L, seg=28):
    pts = [(r * math.cos(math.pi * i / seg), l / 2 + r * math.sin(math.pi * i / seg)) for i in range(seg + 1)]
    pts += [(r * math.cos(math.pi + math.pi * i / seg), -l / 2 + r * math.sin(math.pi + math.pi * i / seg)) for i in range(seg + 1)]
    return pts

# ---- the carousel's still parts ----
b = Part('SM_BeltBody')
b.prism(track(2.45), 0, 0.40, 'MI_T_steel'); b.prism(track(2.47), 0, 0.06, 'MI_T_black')
b.prism(track(2.30), 0.40, 0.415, 'MI_T_black')
isl = b.prism(track(1.30, L - 0.2), 0, 0.62, 'MI_T_steel'); top = b.prism(track(1.25, L - 0.3), 0.62, 0.64, 'MI_T_marbleDark')
bpy.ops.mesh.primitive_cube_add(size=1, location=(0.475, 6.0, 0.3)); cut = bpy.context.object; cut.scale = (1.85, 1.2, 1.2)
for o in (isl, top):
    bo = o.modifiers.new('hole', 'BOOLEAN'); bo.object = cut; bo.operation = 'DIFFERENCE'
for o in (isl, top):
    bpy.context.view_layer.objects.active = o; o.select_set(True); bpy.ops.object.modifier_apply(modifier='hole'); o.select_set(False)
bpy.data.objects.remove(cut, do_unlink=True)
b.box(-0.45, 5.4, -0.8, 1.4, 5.42, 0.0, 'MI_T_black'); b.box(-0.45, 6.58, -0.8, 1.4, 6.6, 0.0, 'MI_T_black')   # the shaft's walls
b.box(-0.47, 5.4, -0.8, -0.45, 6.6, 0.0, 'MI_T_black'); b.box(-0.45, 5.4, -0.82, 1.4, 6.6, -0.8, 'MI_T_black')
for yy in (5.45, 6.51): b.box(-0.4, yy, -0.1, 1.45, yy + 0.04, 0.2, 'MI_T_steel')                       # the incline's side frames
# the hood: its side walls and back go all the way down to the incline and into the shaft (owner 2026-11-18, to-do 100:
# they stopped at the island's top with a gap under them), so the bags come out of a closed housing under its strips
b.box(-0.5, 5.4, -0.8, 0.75, 5.46, 1.15, 'MI_T_steel'); b.box(-0.5, 6.54, -0.8, 0.75, 6.6, 1.15, 'MI_T_steel')
b.box(-0.5, 5.4, 1.15, 0.75, 6.6, 1.2, 'MI_T_steel'); b.box(-0.56, 5.4, -0.8, -0.5, 6.6, 1.2, 'MI_T_steel')
b.box(-0.5, 5.38, 1.2, 0.75, 6.62, 1.23, 'MI_T_black')
b.box(0.7, 5.42, 1.12, 0.78, 6.58, 1.16, 'MI_T_steel')                                                     # the flaps' hinge bar
b.box(-0.03, 10.45, 4.19, 0.03, 10.51, 5.4, 'MI_T_steel')                                                  # the screen's hanger
b.finish()
p = Part('SM_BeltPlate'); p.box(-0.46, -0.1425, -0.015, 0.46, 0.1425, 0.015, 'MI_T_jwRubber'); p.finish()
p = Part('SM_BeltIncline'); o = p.box(-0.4, 5.55, -0.02, 1.45, 6.45, 0.02, 'MI_T_jwRubber'); p.finish()
p = Part('SM_Flap'); p.box(-0.06, -0.006, -0.47, 0.06, 0.006, 0.0, 'MI_T_jwRubber'); p.finish()
p = Part('SM_BeltButton'); p.cyl(0.055, 0.04, (0.02, 0, 0), (0, math.pi / 2, 0), 'MI_T_l_20FF40'); p.finish()
p = Part('SM_ButtonPlate'); p.box(-0.04, -0.3, -0.12, 0.0, 0.3, 0.12, 'MI_T_black'); p.cyl(0.075, 0.012, (0.004, 0, 0), (0, math.pi / 2, 0), 'MI_T_steel'); p.finish()
p = Part('SM_CrewBox'); p.box(-0.05, -0.05, 0, 0.05, 0.05, 1.0, 'MI_T_yellow'); p.box(-0.18, -0.15, 1.0, 0.18, 0.15, 1.32, 'MI_T_yellow')
p.cyl(0.08, 0.012, (0, 0.156, 1.16), (math.pi / 2, 0, 0), 'MI_T_black'); p.finish()
p = Part('SM_InputConv'); p.box(-0.55, 0, 0, 0.55, 1, 0.70, 'MI_T_black'); p.box(-0.5, 0, 0.70, 0.5, 1, 0.75, 'MI_T_jwRubber')
p.box(-0.58, 0, 0.75, -0.5, 1, 0.95, 'MI_T_steel'); p.box(0.5, 0, 0.75, 0.58, 1, 0.95, 'MI_T_steel'); p.finish()
p = Part('SM_WallFrame'); p.box(-0.8, -0.1, 0.70, -0.7, 0.0, 1.85, 'MI_T_yellow'); p.box(0.7, -0.1, 0.70, 0.8, 0.0, 1.85, 'MI_T_yellow')
p.box(-0.8, -0.1, 1.75, 0.8, 0.0, 1.85, 'MI_T_yellow'); p.box(-0.72, -0.08, 1.70, 0.72, 0.0, 1.76, 'MI_T_steel')
# (the crew side's detail, 2026-11-17 - the wall was bare; +y faces the crew, the wall's face is at y -0.1): the belt's
#  sign above (the game writes its number), yellow-and-black hazard panels either side at the foot, steel bumper rails,
#  the input's status lights (green: running, red: stopped) and an emergency stop
p.box(-0.75, -0.1, 2.0, 0.75, 0.03, 2.42, 'MI_T_black'); p.box(-0.77, -0.1, 1.98, 0.77, 0.035, 2.0, 'MI_T_yellow')
def plate2d(pts, y0, y1, m_i, bm):
    # a closed flat slab of a polygon in the wall's plane (x, z) from y0 to y1 (closed, so its normals come out right)
    lo = [bm.verts.new((x, y0, z)) for (x, z) in pts]; hi = [bm.verts.new((x, y1, z)) for (x, z) in pts]
    fs = [bm.faces.new(lo), bm.faces.new(hi[::-1])]
    n = len(pts)
    for k in range(n): fs.append(bm.faces.new((lo[k], hi[k], hi[(k + 1) % n], lo[(k + 1) % n])))
    for f in fs: f.material_index = m_i
def stripes(x0, x1, z0, z1):
    me = bpy.data.meshes.new('hz'); bm = bmesh.new()
    plate2d([(x0, z0), (x1, z0), (x1, z1), (x0, z1)], -0.1, 0.02, 0, bm)
    w = 0.12; h = z1 - z0; k = x0 - h
    while k < x1:
        cl = []
        for (qx, qz) in ((k, z0), (k + w, z0), (k + w + h, z1), (k + h, z1)):
            if qx < x0: qz += x0 - qx; qx = x0
            if qx > x1: qz -= qx - x1; qx = x1
            cl.append((round(qx, 5), round(min(max(qz, z0), z1), 5)))
        cl = list(dict.fromkeys(cl))
        if len(cl) >= 3:
            ar = sum(cl[a][0] * cl[(a + 1) % len(cl)][1] - cl[(a + 1) % len(cl)][0] * cl[a][1] for a in range(len(cl)))
            if abs(ar) > 1e-5: plate2d(cl, 0.02, 0.026, 1, bm)
        k += 2 * w
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new('hz', me); bpy.context.scene.collection.objects.link(o)
    me.materials.append(mat('MI_T_yellow')); me.materials.append(mat('MI_T_black')); p.objs.append(o)
for (x0, x1) in ((-1.7, -0.85), (0.85, 1.7)):
    stripes(x0, x1, 0.02, 0.62)
for (x0, x1) in ((-3.0, -0.85), (0.85, 3.0)):
    for zz in (0.28, 0.82):
        p.box(x0, 0.1, zz, x1, 0.16, zz + 0.14, 'MI_T_steel')
    for xx in (x0 + 0.1, (x0 + x1) / 2, x1 - 0.1): p.box(xx - 0.03, -0.1, 0.25, xx + 0.03, 0.1, 1.0, 'MI_T_steel')
p.box(0.86, -0.1, 1.55, 1.02, 0.08, 1.85, 'MI_T_black')
p.cyl(0.035, 0.03, (0.94, 0.09, 1.78), (math.pi / 2, 0, 0), 'MI_T_l_20FF40', 16)
p.cyl(0.035, 0.03, (0.94, 0.09, 1.64), (math.pi / 2, 0, 0), 'MI_T_l_FF2A10', 16)
p.box(0.88, -0.1, 1.1, 1.08, 0.1, 1.32, 'MI_T_yellow'); p.cyl(0.045, 0.05, (0.98, 0.12, 1.21), (math.pi / 2, 0, 0), 'MI_T_red', 16)
p.finish()
p = Part('SM_BeltScreen'); p.box(-1.06, -0.06, -0.6, 1.06, 0.04, 0.6, 'MI_T_black'); p.finish()
# the bags lie flat as on a real carousel (2026-11-17: more than plain boxes - they read as luggage from across the hall):
#   0, 1  hard-shell cases: rounded shell, ribs along the top, the zip seam round the middle, two spinner wheels and the
#         trolley handle's housing at one end, the carry handle on the long side, a luggage tag
#   2     a soft case: fabric body, two front pockets with zips, piping, wheels and a handle
#   3     a duffel bag: a rounded roll with end caps, two straps over the top and a shoulder strap
def wheels(p, w, d, h):
    for yy in (-d / 2 + 0.06, d / 2 - 0.06):
        p.cyl(0.035, 0.03, (-w / 2 - 0.02, yy, 0.05), (math.pi / 2, 0, 0), 'MI_T_black', 12)
        p.box(-w / 2 - 0.03, yy - 0.025, 0.06, -w / 2 + 0.01, yy + 0.025, 0.11, 'MI_T_black')
    p.box(-w / 2 - 0.025, -0.12, h * 0.25, -w / 2, 0.12, h * 0.85, 'MI_T_black')                   # the trolley handle's housing
def carry(p, w, d, h):
    p.cyl(0.014, 0.18, (0, d / 2 + 0.03, h * 0.6), (0, math.pi / 2, 0), 'MI_T_black', 12)
    for xx in (-0.09, 0.09): p.box(xx - 0.012, d / 2 - 0.005, h * 0.6 - 0.015, xx + 0.012, d / 2 + 0.035, h * 0.6 + 0.015, 'MI_T_black')
def tag(p, w, d, h):
    p.box(0.03, d / 2 + 0.03, h * 0.35, 0.10, d / 2 + 0.034, h * 0.6, 'MI_T_yellow')
def shell(i, w, d, h, m):
    p = Part('SM_Bag%d' % i)
    o = p.box(-w / 2, -d / 2, 0, w / 2, d / 2, h, m); bv = o.modifiers.new('bev', 'BEVEL'); bv.width = 0.05; bv.segments = 4
    p.box(-w / 2 - 0.004, -d / 2 - 0.004, h * 0.48, w / 2 + 0.004, d / 2 + 0.004, h * 0.52, 'MI_T_black')   # the zip seam
    for k in range(5):
        yy = -d / 2 + 0.08 + k * (d - 0.16) / 4
        r = p.box(-w / 2 + 0.07, yy - 0.012, h - 0.004, w / 2 - 0.07, yy + 0.012, h + 0.012, m)              # the shell's ribs
    for (sx, sy) in ((-1, -1), (-1, 1), (1, -1), (1, 1)):                                             # corner guards
        p.box(sx * w / 2 - 0.045 * (sx > 0) - 0.005 * (sx < 0), sy * d / 2 - 0.045 * (sy > 0) - 0.005 * (sy < 0), h * 0.3,
              sx * w / 2 + 0.005 * (sx > 0) + 0.045 * (sx < 0), sy * d / 2 + 0.005 * (sy > 0) + 0.045 * (sy < 0), h * 0.7, 'MI_T_black')
    wheels(p, w, d, h); carry(p, w, d, h); tag(p, w, d, h); p.finish()
shell(0, 0.70, 0.45, 0.28, 'MI_T_red'); shell(1, 0.55, 0.38, 0.24, 'MI_T_navy')
# the soft case
p = Part('SM_Bag2'); w, d, h = 0.80, 0.50, 0.32
o = p.box(-w / 2, -d / 2, 0, w / 2, d / 2, h, 'MI_T_fabricGrey'); bv = o.modifiers.new('bev', 'BEVEL'); bv.width = 0.07; bv.segments = 4
for (x0, x1) in ((-0.33, -0.02), (0.04, 0.33)):
    o = p.box(x0, -0.19, h - 0.01, x1, 0.19, h + 0.05, 'MI_T_fabricGrey'); bv = o.modifiers.new('bev', 'BEVEL'); bv.width = 0.02; bv.segments = 2
    p.box(x0 + 0.02, 0.15, h + 0.05, x1 - 0.02, 0.165, h + 0.056, 'MI_T_black')                       # the pockets' zips
p.box(-w / 2 - 0.004, -d / 2 - 0.004, h * 0.5 - 0.01, w / 2 + 0.004, d / 2 + 0.004, h * 0.5 + 0.01, 'MI_T_black')   # piping
wheels(p, w, d, h); carry(p, w, d, h); tag(p, w, d, h); p.finish()
# the duffel bag
p = Part('SM_Bag3'); L3, R3 = 0.62, 0.17
o = p.cyl(R3, L3, (0, 0, R3), (0, math.pi / 2, 0), 'MI_T_fabricTeal', 20); o.scale = (1.0, 1.0, 1.0)
for xx in (-L3 / 2, L3 / 2): p.cyl(R3 + 0.008, 0.02, (xx, 0, R3), (0, math.pi / 2, 0), 'MI_T_black', 20)   # the end caps' piping
p.box(-L3 / 2 + 0.05, -0.012, 2 * R3 - 0.01, L3 / 2 - 0.05, 0.012, 2 * R3 + 0.006, 'MI_T_black')         # the zip along the top
for xx in (-0.14, 0.14):                                                                           # two carry straps over the top
    pts = [(xx, R3 * math.cos(a) * 1.04, R3 + R3 * math.sin(a) * 1.04) for a in [math.pi * k / 10 for k in range(11)]]
    for a, b2 in zip(pts, pts[1:]): p.box(xx - 0.02, min(a[1], b2[1]) - 0.004, min(a[2], b2[2]) - 0.004, xx + 0.02, max(a[1], b2[1]) + 0.004, max(a[2], b2[2]) + 0.004, 'MI_T_black')
p.cyl(0.012, 0.5, (0, -R3 - 0.02, 0.03), (0, math.pi / 2, 0), 'MI_T_black', 8)                        # the shoulder strap lying by it
tag(p, L3, 2 * R3, 2 * R3); p.finish()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'BaggageParts.blend'))
print('PARTS done')
