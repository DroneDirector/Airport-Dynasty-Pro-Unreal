"""Meshy model intake (2026-11-26): import each unzipped OBJ, report faces, raw bounding box,
up axis, and render one textured 3/4 front preview.

Run headless (Blender 3.3):
  blender -b --factory-startup -P Art/MeshyIntake/meshy_intake.py -- F:/ADP-Meshy/2026-11-26 [Name ...]
Writes <root>/previews/<Name>.png (1024x576, Workbench, texture colour) and <root>/intake.json.
"""
import bpy, sys, os, glob, json, math
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
ROOT = argv[0] if argv else 'F:/ADP-Meshy/2026-11-26'
ONLY = set(argv[1:])
NAMES = ['Lorry', 'CargoForklift', 'CrateFetcher', 'XWayMover', 'VehicleD3abbc', 'Vehicle1b0cf6',
         'B757', 'B747', 'A380', 'A320neo', 'A321']
# Camera direction per model (Blender axes after Y-up -> Z-up conversion). Meshy faces +Z in the
# file, which lands on -Y here; the 3/4 front view looks from front-left, slightly above.
CAM_DIR = {}
DEFAULT_DIR = Vector((-0.75, -1.0, 0.45))

def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.images, bpy.data.cameras, bpy.data.lights):
        for d in list(coll):
            coll.remove(d)

def bbox(objs):
    lo = Vector((1e18,) * 3); hi = Vector((-1e18,) * 3)
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    return lo, hi

def setup_render(path):
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sh = sc.display.shading
    sh.light = 'STUDIO'; sh.color_type = 'TEXTURE'
    sh.show_shadows = False; sh.show_cavity = True; sh.cavity_type = 'WORLD'
    sh.background_type = 'VIEWPORT'; sh.background_color = (0.08, 0.08, 0.09)
    sc.render.resolution_x, sc.render.resolution_y = 1024, 576
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = 'PNG'
    sc.render.filepath = path

def run(name):
    files = glob.glob(os.path.join(ROOT, name, '**', '*.obj'), recursive=True)
    if not files:
        return {'name': name, 'error': 'no obj'}
    clear()
    obj = files[0]
    # Raw import (no axis change) for the raw file bounding box.
    bpy.ops.wm.obj_import(filepath=obj, forward_axis='Y', up_axis='Z')
    objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    rlo, rhi = bbox(objs)
    raw = [round(v, 4) for v in (rhi - rlo)]
    faces = sum(len(o.data.polygons) for o in objs)
    verts = sum(len(o.data.vertices) for o in objs)
    mats = sorted({s.material.name for o in objs for s in o.material_slots if s.material})
    empty_slots = sum(1 for o in objs for s in o.material_slots if s.material is None)
    imgs = sorted(i.name for i in bpy.data.images if i.source == 'FILE')
    # Up axis: Meshy writes Y-up (glTF style). Convert to Blender Z-up for the preview;
    # the preview shows whether that is right.
    for o in objs:
        o.rotation_euler = (math.radians(90), 0, 0)
    bpy.context.view_layer.update()
    lo, hi = bbox(objs)
    ctr = (lo + hi) / 2; size = hi - lo
    radius = size.length / 2
    cam_d = bpy.data.cameras.new('Cam'); cam_d.lens = 50
    cam = bpy.data.objects.new('Cam', cam_d); bpy.context.scene.collection.objects.link(cam)
    d = CAM_DIR.get(name, DEFAULT_DIR).normalized()
    # Fit the bounding sphere into the vertical field of view (16:9, sensor fit auto = horizontal 36 mm).
    vfov = 2 * math.atan((36 * 576 / 1024) / 2 / 50)
    dist = radius / math.sin(vfov / 2) * 0.85
    cam.location = ctr + d * dist
    cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    cam_d.clip_start = dist * 0.01; cam_d.clip_end = dist * 10
    bpy.context.scene.camera = cam
    os.makedirs(os.path.join(ROOT, 'previews'), exist_ok=True)
    png = os.path.join(ROOT, 'previews', name + '.png')
    setup_render(png)
    bpy.ops.render.render(write_still=True)
    return {'name': name, 'obj': os.path.relpath(obj, ROOT), 'objects': len(objs), 'faces': faces,
            'verts': verts, 'raw_size_xyz': raw, 'raw_min': [round(v, 4) for v in rlo],
            'up_axis_file': 'Y (assumed, check preview)', 'zup_size_xyz': [round(v, 4) for v in size],
            'materials': mats, 'empty_slots': empty_slots, 'images': imgs, 'preview': png}

out = []
for n in NAMES:
    if ONLY and n not in ONLY:
        continue
    try:
        r = run(n)
    except Exception as e:
        r = {'name': n, 'error': repr(e)}
    print('INTAKE', json.dumps(r))
    out.append(r)
jp = os.path.join(ROOT, 'intake.json' if not ONLY else 'intake-partial.json')
with open(jp, 'w') as f:
    json.dump(out, f, indent=1)
