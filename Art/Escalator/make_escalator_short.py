# The arrivals escalators (to-do 72, owner 2026-11-16: "the rounded ends continue quite far on the floor ... make it 2/3
# shorter"): the building's escalator (Escalator.blend) with everything beyond the combs - the floor plates, the decking,
# the newels with the handrail's turn - squeezed to a third of its length at both ends; the steps, the slope and the
# handrail's run are untouched. SM_EscBodyShort, SM_EscRampShort (Tools/import_escalator.py imports them too).
#   F:/Tools/Blender-4.2/blender.exe -b Art/Escalator/Escalator.blend --python Art/Escalator/make_escalator_short.py
import bpy, os
HERE = os.path.dirname(os.path.abspath(__file__))
Y0, Y1 = -0.25, 14.01            # the bottom comb plate's teeth line and the top comb's (beyond them: the ends)
for src in ('SM_EscBody', 'SM_EscRamp'):
    o = bpy.data.objects[src]
    c = o.copy(); c.data = o.data.copy(); c.name = src + 'Short'; c.data.name = src + 'Short'; bpy.context.scene.collection.objects.link(c)
    for v in c.data.vertices:
        y = v.co.y
        if y < Y0: v.co.y = Y0 + (y - Y0) / 3.0
        elif y > Y1: v.co.y = Y1 + (y - Y1) / 3.0
    for x in bpy.context.selected_objects: x.select_set(False)
    c.select_set(True); bpy.context.view_layer.objects.active = c
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, c.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    print('ESCSHORT', c.name, [round(min(v.co.y for v in c.data.vertices), 2), round(max(v.co.y for v in c.data.vertices), 2)])
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'EscalatorShort.blend'))
