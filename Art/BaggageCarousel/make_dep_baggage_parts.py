# The DEPARTURE baggage system's game parts (owner 2026-11-21, blueprint Blueprints/Terminal/Departure-Baggage-v2.png),
# in the same family as make_baggage_parts.py (the arrivals' belts): each part its own FBX for
# Tools/import_dep_baggage.py, material slots named after the terminal's finishes (/Game/Terminal/Materials) so they
# match the building and the arrivals' belts. Blender X = plan x, Blender Y = plan z (north), Z up; metres.
# Real kit (reference: Vanderlande / Beumer / Siemens Logistics check-in, CT screening and make-up equipment):
#   SM_DepConv      a 2 m powered belt conveyor module along +Y (y 0..2), the rubber top at z 0: black frame with
#                   steel side channels, return rollers underneath (the game lays them end to end, turned and pitched)
#   SM_DepGuard     a 2 m side guard along +Y on two posts, its foot at the origin (the conveyor's top edge)
#   SM_DepCorner    a 90-degree transfer corner (1.0 x 1.0, top at z 0) with its roller table look
#   SM_DepLeg       a pair of legs with feet and a cross brace, 1 m tall (the game scales it to the conveyor's height)
#   SM_WeighBelt    a check-in desk's weigh + tag belt (0.58 wide), from y 0 (the passenger) to y 2.15 (the collector):
#                   the stainless weighing scale with its rubber top at z 0.40, the dispatch belt with white side
#                   panels, its arch, and the passenger's weight display (green digits) on a post
#   SM_CTScanner    a hold-baggage CT scanner's housing (EU Standard 3 EDS), 4.4 m along +Y, the tunnel for a belt at
#                   0.75 m (the conveyor runs through, the curtains are SM_Flap instances), the gantry, the stripe,
#                   the panel seams, the operator's panel and its status lamp
#   SM_DiverterPost the paddle diverter's motor housing on its post (origin: the pivot, at the floor)
#   SM_DiverterArm  the paddle (1.3 m along +Y from the pivot, yellow with its rubber face), at belt height
#   SM_MakeupBody   a make-up carousel's still parts (11 m straight along X, 2.45 m outer radius), as the claim belts'
#   SM_ChuteHood    the steel arch over a chute's end with its hinge bar (the flaps are SM_Flap instances)
#   SM_LinkTube     a 2 m module of the overhead link's enclosure round a conveyor (cladding, glass strip, ribs, roof)
#   SM_TubeMouth    the enclosure's mouth frame where the belt goes in / comes out
#   SM_LinkPost     a steel column 1 m tall (scaled) with its base plate; SM_LinkYoke the cross beam on top
#   SM_SearchTable  the level-3 search room's stainless bench with a monitor and the trace detector
#   SM_EndStop      the yellow end buffer at a belt's end
# Every part is checked for fighting surfaces (Tools/find_zfight.py on each part's glb, its pieces as nodes) - 0 pairs.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/BaggageCarousel/make_dep_baggage_parts.py
import bpy, bmesh, math, os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
ZF = os.path.join(ROOT, 'Saved', 'ZFight', 'DepBaggage'); os.makedirs(ZF, exist_ok=True)   # (the checker's working files)
bpy.ops.wm.read_factory_settings(use_empty=True)
def mat(n): return bpy.data.materials.get(n) or bpy.data.materials.new(n)
FIGHTS = []
PY = os.environ.get('ADP_PYTHON', 'python')   # (the system python with numpy, for Tools/find_zfight.py)
class Part:
    def __init__(s, name): s.name = name; s.objs = []
    def _n(s, o): o.name = '%s_%02d' % (s.name, len(s.objs)); s.objs.append(o); return o
    # (the flicker rule, built in: pieces that only touch share a face plane - the checker counts that as a fight, and the
    #  game would flicker where they show. Every piece grows by its own few tenths of a millimetre more than the one
    #  before, so touching pieces sink into each other and no two pieces keep a face on the same plane)
    def eps(s): return 0.0011 * (1 + len(s.objs) % 11)
    def box(s, x0, y0, z0, x1, y1, z1, m):
        e = s.eps(); x0, x1 = min(x0, x1) - e, max(x0, x1) + e; y0, y1 = min(y0, y1) - e, max(y0, y1) + e; z0, z1 = min(z0, z1) - e, max(z0, z1) + e
        bpy.ops.mesh.primitive_cube_add(size=1, location=((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)); o = bpy.context.object
        o.scale = (abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)); bpy.ops.object.transform_apply(scale=True); o.data.materials.append(mat(m)); return s._n(o)
    def prism(s, pts, z0, z1, m, cap=True):
        e = s.eps(); z0 -= e; z1 += e
        me = bpy.data.meshes.new('p'); bm = bmesh.new()
        lo = [bm.verts.new((p[0], p[1], z0)) for p in pts]; hi = [bm.verts.new((p[0], p[1], z1)) for p in pts]
        if cap: bm.faces.new(lo[::-1]); bm.faces.new(hi)
        n = len(pts)
        for i in range(n): bm.faces.new((lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i]))
        bm.normal_update(); bm.to_mesh(me); bm.free()
        o = bpy.data.objects.new('p', me); bpy.context.scene.collection.objects.link(o); me.materials.append(mat(m)); return s._n(o)
    def cyl(s, r, d, loc, rot, m, v=24):
        e = s.eps(); r += e; d += 2 * e
        bpy.ops.mesh.primitive_cylinder_add(vertices=v, radius=r, depth=d, location=loc, rotation=rot); o = bpy.context.object; o.data.materials.append(mat(m)); return s._n(o)
    def bevel(s, o, w, seg=2):
        b = o.modifiers.new('bev', 'BEVEL'); b.width = w; b.segments = seg; b.limit_method = 'ANGLE'; return o
    def finish(s):
        for x in bpy.context.selected_objects: x.select_set(False)
        for o in s.objs:
            o.select_set(True); bpy.context.view_layer.objects.active = o
            for md in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=md.name)
            o.select_set(False)
        # every face mapped at real size by the axis it faces (1 m = one tile), as the arrivals' parts (to-do 102)
        for o in s.objs:
            me = o.data; M = o.matrix_world
            if not me.uv_layers: me.uv_layers.new(name='UVMap')
            uv = me.uv_layers.active.data
            for poly in me.polygons:
                n = poly.normal; ax = max(range(3), key=lambda i: abs(n[i]))
                for li in poly.loop_indices:
                    w = M @ me.vertices[me.loops[li].vertex_index].co
                    uv[li].uv = (w.x, w.y) if ax == 2 else (w.y, w.z) if ax == 0 else (w.x, w.z)
        # the flicker check on the pieces before they are joined (each piece its own node)
        for o in s.objs: o.select_set(True)
        g = os.path.join(ZF, s.name + '.glb')
        bpy.ops.export_scene.gltf(filepath=g, use_selection=True, export_format='GLB', export_apply=True)
        r = subprocess.run([PY, os.path.join(ROOT, 'Tools', 'find_zfight.py'), g, os.path.join(ZF, s.name + '.json')], capture_output=True, text=True)
        last = (r.stdout.strip().splitlines() or ['? pairs fight'])[-1]
        try: n = int(last.split()[0])
        except Exception: n = -1
        print('ZFIGHT', s.name, last)
        if n != 0: FIGHTS.append((s.name, last, r.stdout[-1500:] + r.stderr[-500:]))
        bpy.context.view_layer.objects.active = s.objs[0]; bpy.ops.object.join()
        ob = bpy.context.object; ob.name = s.name
        me = ob.data; bm = bmesh.new(); bm.from_mesh(me); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me); bm.free()
        assert all(sl.material for sl in ob.material_slots), s.name + ': empty material slot'
        bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, s.name + '.fbx'), use_selection=True, object_types={'MESH'},
                                 apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
        print('PART', s.name, len(me.polygons), [m.name for m in me.materials])
        coll = bpy.data.collections.get('Done') or bpy.data.collections.new('Done')
        if coll.name not in bpy.context.scene.collection.children: bpy.context.scene.collection.children.link(coll)
        for c in list(ob.users_collection): c.objects.unlink(ob)
        coll.objects.link(ob); ob.hide_set(True); ob.location.x += 30 * len(coll.objects)   # (kept in the .blend side by side)

# ---- the belt conveyor module: 2 m, the rubber top at 0 ----
p = Part('SM_DepConv')
p.box(-0.5, 0.0, -0.28, 0.5, 2.0, -0.03, 'MI_T_black')                 # the bed (closed box frame)
p.box(-0.45, 0.0, -0.03, 0.45, 2.0, 0.0, 'MI_T_jwRubber')              # the belt's top run
for sx in (-1, 1):
    p.box(sx * 0.5, 0.0, -0.27, sx * 0.535, 2.0, 0.02, 'MI_T_steel')     # the side channels
    p.box(sx * 0.5 + sx * 0.035, 0.05, -0.2, sx * 0.5 + sx * 0.05, 1.95, -0.1, 'MI_T_steel')   # the cable tray lip
for yy in (0.5, 1.5): p.cyl(0.04, 0.88, (0, yy, -0.32), (0, math.pi / 2, 0), 'MI_T_steel', 16)   # the return rollers
for yy in (0.5, 1.5):
    for sx in (-1, 1): p.box(sx * 0.46, yy - 0.03, -0.36, sx * 0.49, yy + 0.03, -0.28, 'MI_T_steel')   # their hangers
p.finish()
p = Part('SM_DepGuard')
p.box(-0.02, 0.0, 0.12, 0.02, 2.0, 0.18, 'MI_T_steel')                 # the rail
for yy in (0.25, 1.75): p.box(-0.015, yy - 0.02, 0.0, 0.015, yy + 0.02, 0.12, 'MI_T_steel')
p.box(-0.025, 0.0, 0.135, -0.02, 2.0, 0.165, 'MI_T_black')               # the rubber strip on its inside
p.finish()
p = Part('SM_DepCorner')
p.box(-0.5, -0.5, -0.28, 0.5, 0.5, -0.03, 'MI_T_black')
p.box(-0.45, -0.45, -0.03, 0.45, 0.45, 0.0, 'MI_T_jwRubber')                 # the turning belt's top
p.box(-0.535, 0.465, -0.27, 0.5, 0.5, 0.18, 'MI_T_steel'); p.box(0.5, -0.5, -0.27, 0.535, 0.5, 0.18, 'MI_T_steel')   # the outer guard walls
p.finish()
p = Part('SM_DepLeg')
for sx in (-1, 1):
    p.box(sx * 0.42 - 0.03, -0.03, 0.0, sx * 0.42 + 0.03, 0.03, 1.0, 'MI_T_steel')
    p.box(sx * 0.42 - 0.07, -0.07, 0.0, sx * 0.42 + 0.07, 0.07, 0.012, 'MI_T_black')   # the foot plates
p.box(-0.39, -0.02, 0.3, 0.39, 0.02, 0.34, 'MI_T_steel')
p.finish()

# ---- the check-in weigh + tag belt ----
p = Part('SM_WeighBelt')
p.box(-0.29, 0.0, 0.0, 0.29, 0.9, 0.36, 'MI_T_steel')                    # the scale's stainless body
p.box(-0.29, -0.015, 0.0, 0.29, 0.0, 0.12, 'MI_T_black')                  # its kick plate
p.box(-0.26, 0.02, 0.36, 0.26, 0.88, 0.40, 'MI_T_jwRubber')               # the weighing belt
p.box(-0.29, 0.9, 0.08, 0.29, 2.15, 0.36, 'MI_T_black')                   # the dispatch belt's frame
p.box(-0.26, 0.92, 0.36, 0.26, 2.15, 0.40, 'MI_T_jwRubber')
for sx in (-1, 1):
    p.box(sx * 0.27, 0.9, 0.40, sx * 0.29, 2.15, 0.78, 'MI_T_white')     # the side panels (bags can't fall off)
    p.box(sx * 0.265, 0.9, 0.78, sx * 0.295, 2.15, 0.81, 'MI_T_steel')    # their capping
    p.box(sx * 0.27, 0.0, 0.40, sx * 0.29, 0.9, 0.5, 'MI_T_steel')        # the scale's low edges
    p.box(sx * 0.29, 2.0, 0.0, sx * 0.31, 2.06, 0.08, 'MI_T_steel')       # the dispatch belt's feet
p.box(-0.29, 0.92, 0.81, 0.29, 0.98, 0.86, 'MI_T_steel')                  # the arch over the dispatch belt
p.box(-0.2, 0.93, 0.86, 0.2, 0.97, 0.94, 'MI_T_yellow')                    # its sign plate ('MAX 32 kg' strip)
p.box(0.31, 0.04, 0.0, 0.35, 0.08, 0.92, 'MI_T_steel')                    # the display's post (passenger side, east)
p.box(0.26, 0.01, 0.92, 0.40, 0.09, 1.04, 'MI_T_black')                   # the weight display
p.box(0.275, 0.005, 0.95, 0.385, 0.01, 1.01, 'MI_T_l_20FF40')             # its digits (glow by material)
p.box(0.31, 0.03, 0.0, 0.37, 0.09, 0.01, 'MI_T_black')                    # the post's foot
p.finish()

# ---- the CT scanner's housing ----
p = Part('SM_CTScanner')
L2 = 2.2; TX, TZ0, TZ1 = 0.55, 0.45, 1.35
p.bevel(p.box(-0.95, -L2, 0.0, 0.95, L2, TZ0, 'MI_T_white'), 0.02)           # the plinth under the tunnel
for sx in (-1, 1):
    o = p.box(sx * TX, -L2, TZ0 + 0.001, sx * 0.95, L2, TZ1 - 0.001, 'MI_T_white')  # the sides of the tunnel
o = p.bevel(p.box(-0.95, -L2, TZ1, 0.95, L2, 2.05, 'MI_T_white'), 0.04)     # the top
# the gantry (the CT's ring housing) in the middle, bigger than the body
p.bevel(p.box(-1.05, -0.8, 0.01, 1.05, 0.8, 0.43, 'MI_T_whiteGloss'), 0.03)
for sx in (-1, 1): p.box(sx * 0.6, -0.8, 0.43, sx * 1.05, 0.8, 1.41, 'MI_T_whiteGloss')
p.bevel(p.box(-1.05, -0.8, 1.41, 1.05, 0.8, 2.25, 'MI_T_whiteGloss'), 0.05)
for sx in (-1, 1):
    p.box(sx * 1.05, -0.8, 1.55, sx * 1.058, 0.8, 1.68, 'MI_T_tealUI')       # the stripe on the gantry
    for yy in (-L2, L2 - 1.4):
        p.box(sx * 0.95, yy, 1.6, sx * 0.958, yy + 1.4, 1.7, 'MI_T_tealUI')  # and along the body
    for yy in (-1.5, 1.5): p.box(sx * 0.95, yy - 0.004, 0.06, sx * 0.964, yy + 0.004, 2.0, 'MI_T_black')   # panel seams
# the tunnel's mouths: dark trim frames at both ends
for sy in (-1, 1):
    y0, y1 = (L2, L2 + 0.04) if sy > 0 else (-L2 - 0.04, -L2)
    p.box(-TX - 0.08, y0, TZ1, TX + 0.08, y1, TZ1 + 0.08, 'MI_T_black')
    for sx in (-1, 1): p.box(sx * TX, y0, TZ0, sx * (TX + 0.08), y1, TZ1, 'MI_T_black')
    p.box(-TX - 0.08, y0, TZ0 - 0.06, TX + 0.08, y1, TZ0, 'MI_T_black')
    p.box(-0.6, y0, 1.75, 0.6, y1, 1.95, 'MI_T_yellow')                        # the X-ray warning plate
# the operator's panel on the -X side, its screen and the status lamp on top
p.box(-1.12, -1.9, 0.9, -0.95, -1.2, 1.3, 'MI_T_black'); p.box(-1.125, -1.85, 1.0, -1.12, -1.25, 1.25, 'MI_T_screenBlack')
p.cyl(0.06, 0.1, (0.7, -1.9, 2.1), (0, 0, 0), 'MI_T_black', 16); p.cyl(0.05, 0.12, (0.7, -1.9, 2.21), (0, 0, 0), 'MI_T_l_20FF40', 16)
p.box(-0.4, -L2 + 0.3, 2.05, 0.4, -L2 + 0.9, 2.15, 'MI_T_steel')            # the cooling vent box
p.finish()

# ---- the paddle diverter ----
p = Part('SM_DiverterPost')
p.cyl(0.07, 0.95, (0, 0, 0.475), (0, 0, 0), 'MI_T_steel', 20)
p.bevel(p.box(-0.16, -0.16, 0.95, 0.16, 0.16, 1.25, 'MI_T_yellow'), 0.02)   # the motor + gearbox housing
p.box(-0.18, -0.18, 0.0, 0.18, 0.18, 0.015, 'MI_T_black')
p.cyl(0.05, 0.04, (0, 0, 1.27), (0, 0, 0), 'MI_T_black', 16)
p.finish()
p = Part('SM_DiverterArm')
p.cyl(0.045, 0.34, (0, 0, 0.95), (0, 0, 0), 'MI_T_steel', 16)               # the hub on the pivot
p.bevel(p.box(-0.03, 0.04, 0.80, 0.03, 1.3, 1.10, 'MI_T_yellow'), 0.01)   # the paddle blade
p.box(-0.045, 0.06, 0.82, -0.03, 1.28, 1.08, 'MI_T_jwRubber'); p.box(0.03, 0.06, 0.82, 0.045, 1.28, 1.08, 'MI_T_jwRubber')
p.box(-0.02, 0.2, 1.10, 0.02, 1.2, 1.13, 'MI_T_black')                      # the stiffener
p.finish()

# ---- the make-up carousel's still parts (as the claim belts', 11 m straight along X) ----
LM = 11.0
def track(r, l=LM, seg=28):
    pts = [(l / 2 + r * math.sin(math.pi * i / seg), r * math.cos(math.pi * i / seg)) for i in range(seg + 1)]
    pts += [(-l / 2 - r * math.sin(math.pi * i / seg), -r * math.cos(math.pi * i / seg)) for i in range(seg + 1)]
    return pts[::-1]
b = Part('SM_MakeupBody')
b.prism(track(2.45), 0, 0.40, 'MI_T_steel'); b.prism(track(2.47), 0, 0.06, 'MI_T_black')
b.prism(track(2.30), 0.40, 0.415, 'MI_T_black')
b.prism(track(1.30, LM - 0.2), 0, 0.62, 'MI_T_steel'); b.prism(track(1.25, LM - 0.3), 0.62, 0.64, 'MI_T_marbleDark')
b.box(-LM / 2 + 0.2, -0.05, 0.64, LM / 2 - 0.2, 0.05, 0.9, 'MI_T_steel')       # the island's middle rail (bags lean on it)
b.box(-0.03, -0.03, 0.9, 0.03, 0.03, 3.2, 'MI_T_steel')                        # the screen's post
b.finish()
p = Part('SM_ChuteHood')
for sx in (-1, 1): p.box(sx * 0.6, -0.04, 0.0, sx * 0.66, 0.04, 1.0, 'MI_T_steel')
p.box(-0.66, -0.04, 1.0, 0.66, 0.04, 1.08, 'MI_T_steel'); p.box(-0.6, 0.04, 0.93, 0.6, 0.08, 0.97, 'MI_T_steel')   # the hinge bar
p.box(-0.66, -0.045, 1.08, 0.66, 0.045, 1.2, 'MI_T_yellow')
p.finish()

# ---- the overhead link's enclosure (round the belt's top at z 0) ----
p = Part('SM_LinkTube')
W2 = 0.8
p.box(-W2, 0.0, -0.36, W2, 2.0, -0.33, 'MI_T_steel')                       # the floor plate (walkway for the fitters)
for sx in (-1, 1):
    p.box(sx * (W2 - 0.04), 0.0, -0.33, sx * W2, 2.0, 0.45, 'MI_T_white')     # the cladding below the window
    p.box(sx * (W2 - 0.025), 0.0, 0.45, sx * (W2 - 0.015), 2.0, 0.85, 'MI_T_glass')   # the window strip
    p.box(sx * (W2 - 0.04), 0.0, 0.85, sx * W2, 2.0, 1.2, 'MI_T_white')       # above it
    p.box(sx * W2, 0.0, -0.4, sx * (W2 + 0.05), 0.08, 1.26, 'MI_T_steel')     # the rib
    p.box(sx * W2, 0.08, 0.44, sx * (W2 + 0.01), 2.0, 0.46, 'MI_T_steel'); p.box(sx * W2, 0.08, 0.84, sx * (W2 + 0.01), 2.0, 0.86, 'MI_T_steel')
p.box(-W2 - 0.05, 0.0, 1.26, W2 + 0.05, 0.08, 1.3, 'MI_T_steel')            # the rib over the roof
p.box(-W2 - 0.03, 0.08, 1.2, W2 + 0.03, 2.0, 1.25, 'MI_T_roofZinc')          # the roof
p.box(-W2, 0.0, -0.4, W2, 0.08, -0.36, 'MI_T_steel')                         # the rib under the floor
p.finish()
p = Part('SM_TubeMouth')
p.box(-0.86, -0.06, -0.42, 0.86, 0.0, -0.28, 'MI_T_steel'); p.box(-0.86, -0.06, 1.12, 0.86, 0.0, 1.32, 'MI_T_steel')
for sx in (-1, 1): p.box(sx * 0.62, -0.06, -0.28, sx * 0.86, 0.0, 1.12, 'MI_T_steel')
p.box(-0.62, -0.05, 0.95, 0.62, -0.01, 1.12, 'MI_T_black')                   # the flap curtain's hinge board
p.finish()
p = Part('SM_LinkPost')
p.box(-0.1, -0.1, 0.006, 0.1, 0.1, 1.0, 'MI_T_steel'); p.box(-0.2, -0.2, 0.0, 0.2, 0.2, 0.015, 'MI_T_black')
p.finish()
p = Part('SM_LinkYoke')
p.box(-0.95, -0.1, -0.16, 0.95, 0.1, 0.0, 'MI_T_steel')
p.finish()

# ---- the search room's bench, the end buffer ----
p = Part('SM_SearchTable')
p.box(-1.0, -0.45, 0.86, 1.0, 0.45, 0.9, 'MI_T_steel')
for sx in (-1, 1):
    for sy in (-1, 1): p.box(sx * 0.93 - 0.03, sy * 0.38 - 0.03, 0.0, sx * 0.93 + 0.03, sy * 0.38 + 0.03, 0.86, 'MI_T_steel')
p.box(-0.95, -0.4, 0.2, 0.95, 0.4, 0.23, 'MI_T_steel')                        # the shelf
p.box(0.45, 0.2, 0.9, 0.85, 0.4, 1.15, 'MI_T_white'); p.box(0.5, 0.195, 0.95, 0.8, 0.2, 1.1, 'MI_T_screenBlack')   # the trace detector
p.box(-0.85, 0.3, 0.9, -0.81, 0.34, 1.2, 'MI_T_black'); p.box(-1.0, 0.25, 1.2, -0.6, 0.29, 1.45, 'MI_T_black')   # the monitor
p.box(-0.98, 0.245, 1.22, -0.62, 0.25, 1.43, 'MI_T_screenBlack')
p.finish()
p = Part('SM_EndStop')
p.box(-0.5, -0.06, 0.0, 0.5, 0.0, 0.2, 'MI_T_yellow'); p.box(-0.5, -0.07, 0.04, 0.5, -0.06, 0.16, 'MI_T_black')
p.finish()

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'DepBaggageParts.blend'))
for f in FIGHTS: print('FIGHT', f[0], f[1], '\n', f[2])
print('PARTS done', 'fights: %d' % len(FIGHTS))
