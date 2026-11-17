# Jetway pictures from Blender (owner, 2026-11-17): a wireframe of each section and a grey-box (clay) render of the whole
# jetway, 4K PNG, from Art/Jetway/Jetway.blend (Tools/make_jetway_blend.py). Out: docs/Pictures/Jetway-Final/.
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b Art/Jetway/Jetway.blend --python Tools/render_jetway_sheets.py
import bpy, os, math
from mathutils import Vector

OUT = os.path.join(os.path.dirname(bpy.data.filepath), '..', '..', 'docs', 'Pictures', 'Jetway-Final')
os.makedirs(OUT, exist_ok=True)
sc = bpy.context.scene
sc.render.engine = 'BLENDER_WORKBENCH'
sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = 3840, 2160, 100
sc.render.film_transparent = False
sh = sc.display.shading
sc.display.render_aa = '16'
meshes = [o for o in sc.objects if o.type == 'MESH']

def group_of(o):
    """The section an object belongs to, from its own name or its parents' (J-1/TUNNEL-1, J-1/CAB, ...)."""
    p = o
    while p:
        n = p.name.split(':')[0].split('.')[0]
        for key in ('TUNNEL-1', 'TUNNEL-2', 'TUNNEL-3', 'CAB', 'ROTUNDA', 'DRIVE-UNIT', 'CORRIDOR'):
            if n.endswith('/' + key) or n.endswith('/' + key + '-SLEEVE') and key == 'CAB': return key
        p = p.parent
    return None

def bounds(objs):
    lo = Vector((1e9, 1e9, 1e9)); hi = -lo
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c); lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    return lo, hi

def camera_on(objs, direction, lens=50.0, margin=1.25):
    lo, hi = bounds(objs); mid = (lo + hi) / 2; size = (hi - lo).length
    cam = bpy.data.objects.get('SheetCam') or bpy.data.objects.new('SheetCam', bpy.data.cameras.new('SheetCam'))
    if cam.name not in sc.collection.objects: sc.collection.objects.link(cam)
    cam.data.lens = lens; cam.data.clip_end = 2000
    d = Vector(direction).normalized()
    dist = size * margin * lens / 36.0
    cam.location = mid + d * dist
    cam.rotation_euler = (mid - cam.location).to_track_quat('-Z', 'Y').to_euler()
    sc.camera = cam

def show_only(objs):
    keep = set(objs)
    for o in meshes: o.hide_render = o not in keep

def shot(name):
    sc.render.filepath = os.path.join(OUT, name + '.png'); bpy.ops.render.render(write_still=True); print('RENDER', sc.render.filepath)

# ---- wireframes: each section on its own, every edge as a thin dark line (a wireframe modifier), from the side and above
try: sc.display.shading.type = 'SOLID'
except Exception: pass
sh.light = 'FLAT'; sh.color_type = 'SINGLE'; sh.single_color = (0.06, 0.07, 0.09)
sh.show_cavity = False; sh.show_shadows = False; sh.show_xray = False
world = sc.world or bpy.data.worlds.new('W'); sc.world = world; world.color = (0.93, 0.94, 0.96)
groups = {}
for o in meshes:
    gname = group_of(o)
    if gname: groups.setdefault(gname, []).append(o)
for gname, objs in groups.items():
    show_only(objs)
    mods = []
    for o in objs:
        m = o.modifiers.new('SheetWire', 'WIREFRAME'); m.thickness = 0.006; m.use_replace = True; m.use_even_offset = True; mods.append((o, m))
    camera_on(objs, (0.55, -1.0, 0.45))
    shot('Wireframe-%s' % gname)
    for o, m in mods: o.modifiers.remove(m)

# ---- grey box: the whole jetway in plain clay grey, soft shadows and cavity
for o in meshes: o.hide_render = False; o.show_wire = False
try: sc.display.shading.type = 'SOLID'
except Exception: pass
sh.light = 'STUDIO'; sh.color_type = 'SINGLE'; sh.single_color = (0.72, 0.72, 0.72)
sh.show_cavity = True; sh.cavity_type = 'BOTH'; sh.show_shadows = True; sh.shadow_intensity = 0.45
sh.show_specular_highlight = False
camera_on(meshes, (0.8, -1.0, 0.55), lens=40.0, margin=1.05); shot('GreyBox-whole-front')
camera_on(meshes, (-0.9, -0.6, 0.35), lens=40.0, margin=1.05); shot('GreyBox-whole-back')
print('SHEETS', sorted(groups), OUT)
