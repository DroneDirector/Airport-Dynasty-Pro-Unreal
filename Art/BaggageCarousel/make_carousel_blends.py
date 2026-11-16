# The baggage claim machinery as their own Blender files (to-do 72; owner 2026-11-16: "commit the conveyor belts and
# the package claim machinery to the Blender folder"), taken out of the concept scene (Art/BaggageClaim/
# BaggageClaim_concept.blend, made by render_claim_concept.py) so they always match it:
#   Art/BaggageCarousel/BaggageCarousel.blend  one flat-plate carousel: skirt and kick plate, the plates, the centre
#                                              island with its feed shaft, the inclined feed belt, the stainless hood with
#                                              its rubber flaps, its flight screen - centred on the origin, floor at 0,
#                                              the feed shaft at its +Y (north) end; a few bags in their own collection
#   Art/BaggageInput/BaggageInput.blend        one crew input conveyor (frame, belt, side rails, the wall's yellow frame
#                                              and flaps, its sign) and its baggage cart, the wall line at Y 0, the
#                                              conveyor running to -Y (into the wall)
# Plan metres (X along the building, Y north, Z up), as the concept.
#   F:/Tools/Blender-4.2/blender.exe -b Art/BaggageClaim/BaggageClaim_concept.blend --python Art/BaggageCarousel/make_carousel_blends.py
import bpy, os
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CX, CY = -35.5, 31.0          # belt 3, the middle one
WALL = 52.3                   # the sorting hall's side of the north wall
def bbox(o):
    from mathutils import Vector
    pts = [o.matrix_world @ Vector(c) for c in o.bound_box]
    return (min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts), max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts))
def inside(o, x0, y0, z0, x1, y1, z1):
    b = bbox(o)
    return b[0] >= x0 and b[1] >= y0 and b[2] >= z0 and b[3] <= x1 and b[4] <= y1 and b[5] <= z1
def is_bag(o): return any(m and m.name.startswith('Bag') for m in o.data.materials) if o.type == 'MESH' else False
def export(name, keep, origin, folder):
    # a copy of the scene with only these objects, moved so `origin` sits at (0, 0, 0)
    # their modifiers made real first (the island's shaft hole is cut by a box that is not kept; the bags' bevels)
    for o in keep:
        if not o.modifiers: continue
        for x in bpy.context.selected_objects: x.select_set(False)
        o.hide_viewport = False; o.select_set(True); bpy.context.view_layer.objects.active = o
        for m in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)
    scene_objs = [o for o in bpy.data.objects if o.type == 'MESH']
    keep_set = set(keep)
    for o in scene_objs:
        if o not in keep_set: bpy.data.objects.remove(o, do_unlink=True)
    for o in [o for o in bpy.data.objects if o.type != 'MESH']: bpy.data.objects.remove(o, do_unlink=True)
    bags = bpy.data.collections.new('Bags (examples)'); bpy.context.scene.collection.children.link(bags)
    for o in keep:
        o.hide_render = False; o.hide_viewport = False
        o.location = (o.location.x - origin[0], o.location.y - origin[1], o.location.z - origin[2])
        if is_bag(o):
            for c in list(o.users_collection): c.objects.unlink(o)
            bags.objects.link(o)
    # (the bags' handles: the thin dark cylinders sitting on them go with them)
    out = os.path.join(ROOT, 'Art', folder); os.makedirs(out, exist_ok=True)
    bpy.ops.outliner.orphans_purge(do_recursive=True)
    bpy.ops.file.pack_all()   # (the textures in the file: it opens textured anywhere)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, name + '.blend'))
    print('BLEND', name, len(keep), 'objects ->', os.path.join(out, name + '.blend'))

src = bpy.data.filepath
mesh = [o for o in bpy.data.objects if o.type == 'MESH']
# the carousel: everything within its footprint (plus its screen and the screen's hanger), no floor, wall or ceiling
car = [o for o in mesh if inside(o, CX - 2.6, CY - 10.1, -0.05, CX + 2.6, CY + 10.7, 5.45) and o.name not in ('ClaimFloor', 'Plenum', 'F2Slab') and not o.name.startswith(('Slat', 'LedLine'))]
car += [o for o in mesh if o.name.startswith(('Island3', 'IslandTop3', 'Skirt3', 'Kick3')) and o not in car]
export('BaggageCarousel', car, (CX, CY, 0.0), 'BaggageCarousel')
bpy.ops.wm.open_mainfile(filepath=src)
mesh = [o for o in bpy.data.objects if o.type == 'MESH']
inp = [o for o in mesh if inside(o, CX - 1.0, WALL - 0.05, -0.05, CX + 2.9, WALL + 8.2, 2.6) and o.name not in ('SortFloor',)]
export('BaggageInput', inp, (CX, WALL, 0.0), 'BaggageInput')
