# The A320's moving gear parts (owner, 2026-11-21: "the nose wheel of the airplane should also turn a little bit when it's
# being pushed back ... and when the airplane is driving on its own"). His model (made ready by make_a320.py, kept as
# Art/A320/A320-whole.glb) is one mesh of many separate pieces; the pieces of the nose gear below the steering collar and
# the wheels are lifted out, untouched, into their own parts so the game can turn and roll them:
#   NoseLeg        - the lower nose gear leg, torque links and axle (turns with the steering)
#   NoseWheels     - the two nose tyres and hubs (turn with the steering and roll)
#   MainWheels_L/R - each main gear's tyres and hubs (roll)
#   (to-do 380 A) NoseLegUpper, MainLegUpper_L/R, SideBraceUp/Lo_L/R, NoseDoor_L/R, MainDoor_L/R, MainBayDoor_L/R and the
#   dark bays: made by Art/A320/a320_retract_parts.py (run from here, at the end) - pivots at the hinges, see its header.
# Everything else stays the A320 part. Prints the pivots (Blender metres) for SimTraffic.cpp.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/A320/make_a320_parts.py
import bpy, bmesh, os, shutil
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'A320-whole.glb')
if not os.path.exists(SRC): shutil.copy(os.path.join(HERE, 'A320.glb'), SRC)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)
o = [x for x in bpy.context.scene.objects if x.type == 'MESH'][0]
o.name = 'A320'
bpy.context.view_layer.objects.active = o
bm = bmesh.new(); bm.from_mesh(o.data); bm.faces.ensure_lookup_table()
# the pieces (islands of linked faces) and their boxes
seen = set(); isl = []
for f in bm.faces:
    if f.index in seen: continue
    stack = [f]; comp = []; seen.add(f.index)
    while stack:
        g = stack.pop(); comp.append(g.index)
        for e in g.edges:
            for h in e.link_faces:
                if h.index not in seen: seen.add(h.index); stack.append(h)
    vs = np.array([v.co[:] for i in comp for v in bm.faces[i].verts])
    isl.append((comp, vs.min(0), vs.max(0)))
def pick(test): return [c for c, lo, hi in isl if test(lo, hi)]
nose_wheels = pick(lambda lo, hi: abs(lo[1]) < 1.0 and abs(hi[1]) < 1.0 and hi[2] <= 0.95 and abs(lo[0]) < 0.8 and abs(hi[0]) < 0.8)
nose_leg = pick(lambda lo, hi: abs(lo[1]) < 1.0 and abs(hi[1]) < 1.0 and hi[2] <= 1.55 and hi[2] > 0.95 and lo[2] > 0.5 and abs(lo[0]) < 0.8 and abs(hi[0]) < 0.8)
main_l = pick(lambda lo, hi: hi[1] < -9.0 and lo[1] > -16.0 and hi[2] <= 1.3 and hi[0] < -2.0 and lo[0] > -6.5)
main_r = pick(lambda lo, hi: hi[1] < -9.0 and lo[1] > -16.0 and hi[2] <= 1.3 and lo[0] > 2.0 and hi[0] < 6.5)
print('PARTS nose wheels %d pieces, nose leg %d, main L %d, main R %d' % (len(nose_wheels), len(nose_leg), len(main_l), len(main_r)))
bm.free()

def split(groups, name):
    faces = set(i for c in groups for i in c)
    if not faces: return None
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.mode_set(mode='EDIT')
    eb = bmesh.from_edit_mesh(o.data); eb.faces.ensure_lookup_table()
    for v in eb.verts: v.select_set(False)
    for e in eb.edges: e.select_set(False)
    for f in eb.faces: f.select_set(False)
    for i in faces: eb.faces[i].select_set(True)
    eb.select_flush_mode(); bmesh.update_edit_mesh(o.data)
    bpy.ops.mesh.separate(type='SELECTED'); bpy.ops.object.mode_set(mode='OBJECT')
    new = [x for x in bpy.context.selected_objects if x is not o][-1]
    new.name = name; new.data.name = name
    v = np.array([q.co[:] for q in new.data.vertices]); lo, hi = v.min(0), v.max(0)
    print('PIVOT %s centre %.4f %.4f %.4f  box %s %s' % (name, *(lo + hi) / 2, lo.round(3), hi.round(3)))
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    return new
# (the face indices shift after each separate: work out the islands again for each part from their boxes)
def redo(test, name):
    b = bmesh.new(); b.from_mesh(o.data); b.faces.ensure_lookup_table()
    sn = set(); grp = []
    for f in b.faces:
        if f.index in sn: continue
        st = [f]; comp = []; sn.add(f.index)
        while st:
            g = st.pop(); comp.append(g.index)
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in sn: sn.add(h.index); st.append(h)
        vs = np.array([v.co[:] for i in comp for v in b.faces[i].verts])
        if test(vs.min(0), vs.max(0)): grp.append(comp)
    b.free()
    return split(grp, name)
redo(lambda lo, hi: abs(lo[1]) < 1.0 and abs(hi[1]) < 1.0 and hi[2] <= 0.95 and abs(lo[0]) < 0.8 and abs(hi[0]) < 0.8, 'NoseWheels')
redo(lambda lo, hi: abs(lo[1]) < 1.0 and abs(hi[1]) < 1.0 and hi[2] <= 1.55 and hi[2] > 0.95 and lo[2] > 0.5 and abs(lo[0]) < 0.8 and abs(hi[0]) < 0.8, 'NoseLeg')
redo(lambda lo, hi: hi[1] < -11.9 and lo[1] > -14.0 and hi[2] <= 1.3 and hi[0] < -2.0 and lo[0] > -6.5, 'MainWheels_L')
redo(lambda lo, hi: hi[1] < -11.9 and lo[1] > -14.0 and hi[2] <= 1.3 and lo[0] > 2.0 and hi[0] < 6.5, 'MainWheels_R')
# the main gear's lower legs (the sliding piston, the axle beam and links between the tyres): they move with the wheels as
# the struts compress on touchdown (the cylinders above stay in the wing)
redo(lambda lo, hi: hi[1] < -11.9 and lo[1] > -14.0 and lo[2] > 0.4 and hi[2] <= 2.4 and hi[0] < -2.6 and lo[0] > -5.2, 'MainLeg_L')
redo(lambda lo, hi: hi[1] < -11.9 and lo[1] > -14.0 and lo[2] > 0.5 and hi[2] <= 2.0 and lo[0] > 2.9 and hi[0] < 4.9, 'MainLeg_R')
if os.path.basename(globals().get('__file__', '')) == 'make_a320_parts.py':     # (not when make_a320_clean/_panels run it)
    exec(compile(open(os.path.join(HERE, 'a320_retract_parts.py')).read(), 'a320_retract_parts.py', 'exec'))
for x in bpy.data.objects:
    if x.type == 'MESH': assert all(s.material for s in x.material_slots), 'empty material slot on ' + x.name
bpy.ops.export_scene.gltf(filepath=os.environ.get('ADP_PARTS_GLB') or os.path.join(HERE, 'A320.glb'), export_format='GLB', use_selection=False,
                          export_apply=True, export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO')
print('A320: written with its gear parts')
print('DONE')
