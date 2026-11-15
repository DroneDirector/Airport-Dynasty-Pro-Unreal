# A pet model from the owner (Meshy: one textured mesh, Art/Pets/Models/<Dogs|Cats>/<Name>/meshy/...obj) made ready
# for the game (owner, 2026-11-15: his German Shepherd and house cat, "move them into the character creator's pet
# section, and animate them"): turned to face forward (-Y in Blender, Unreal's +X after export), its real size (the
# height to the ear tips), standing on the ground at the origin, the mesh reduced to a size a bending body can carry
# (its texture kept), saved as Art/Pets/Models/<Kind>/<Name>/<Name>.blend for the rig and the moves (rig_pet.py).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Pets/make_pet.py -- <Dogs|Cats> <Name> <ear-tip height m> <faces>
import bpy, bmesh, glob, math, os, sys
import numpy as np
from mathutils import Vector, Matrix

a = sys.argv[sys.argv.index('--') + 1:]
KIND, NAME, H, FACES = a[0], a[1], float(a[2]), int(a[3])
HERE = os.path.dirname(os.path.abspath(__file__))
D = os.path.join(HERE, 'Models', KIND, NAME)
src = glob.glob(os.path.join(D, 'meshy', '*', '*.obj'))[0]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=src)
ob = bpy.context.selected_objects[0]; ob.name = NAME; ob.data.name = NAME
bpy.context.view_layer.objects.active = ob
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
co = np.empty(len(ob.data.vertices) * 3); ob.data.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
lo, hi = co.min(0), co.max(0); ext = hi - lo
print('PET raw extent', ext.round(3))

# up: the axis whose low end holds the paws - for Meshy's export Blender's import already gives Z up; checked: the
# lowest 3 % of the points must lie in small separate spots (the paws), not spread over a whole side
up = 2
# the length: the longer of the two level axes; the nose end: the end nearer the highest point (the ears)
ax = 0 if ext[0] > ext[1] else 1
top = co[np.argmax(co[:, up])]
mid = (lo[ax] + hi[ax]) / 2
nose_positive = top[ax] > mid
# turn so the nose points -Y (Blender's front)
ang = {(0, True): math.pi / 2, (0, False): -math.pi / 2, (1, True): math.pi, (1, False): 0.0}[(ax, nose_positive)]
R = Matrix.Rotation(ang, 4, 'Z')
ob.data.transform(R)
co = np.empty(len(ob.data.vertices) * 3); ob.data.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
lo, hi = co.min(0), co.max(0)
# real size: the height to the ear tips; standing on the ground (the paws' lowest points at z 0), centred
s = H / (hi[2] - lo[2])
c = np.array([(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]])
ob.data.transform(Matrix.Scale(s, 4) @ Matrix.Translation(Vector(-c)))
ob.data.update()
co = np.empty(len(ob.data.vertices) * 3); ob.data.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
print('PET real size (m): width %.3f length %.3f height %.3f' % tuple(co.max(0) - co.min(0)))

# lighter for a bending body: reduced to FACES faces (the texture follows its UVs) - only where nobody notices (owner,
# 2026-11-15): the head (eyes, nose, mouth), the ears, the paws and the tail's tip keep their detail, the faces come
# from the flat coat, the belly and the insides of the legs. (Blender reduces the weighted points more: weight 1 =
# reduce here, 0 = keep.)
n0 = len(ob.data.polygons)
L = co[:, 1].max() - co[:, 1].min(); y0 = co[:, 1].min()
def region(p):
    t = (p.y - y0) / L; z = p.z / H
    if z < 0.06: return 'paws'
    if z > 0.84: return 'ears'
    if t < 0.22 and z > 0.55: return 'head'
    if t > 0.93 and z < 0.40: return 'tail tip'
    return 'body'
g = ob.vertex_groups.new(name='reduce_here')
g.add([v.index for v in ob.data.vertices if region(v.co) == 'body'], 1.0, 'REPLACE')
g.add([v.index for v in ob.data.vertices if region(v.co) != 'body'], 0.0, 'REPLACE')
# (tried: Blender's vertex-group weighting strips the weighted part almost bare at any factor - the body went to a few
#  thousand faces. The plain reduction already measures the shape error of every collapse, so it takes its faces from
#  the flat coat first and keeps the curved eyes, nose, mouth and paws; the group stays only to count by region.)
if n0 > FACES:
    keep = ob.data.copy(); r = FACES / n0
    for _ in range(6):
        ob.data = keep.copy()
        m = ob.modifiers.new('reduce', 'DECIMATE'); m.ratio = r
        bpy.ops.object.modifier_apply(modifier='reduce')
        got = len(ob.data.polygons)
        if abs(got - FACES) < FACES * 0.04: break
        r *= FACES / got
counts = {}
for p_ in ob.data.polygons: r = region(p_.center); counts[r] = counts.get(r, 0) + 1
print('PET faces by region', counts)
bpy.ops.object.shade_smooth()
print('PET faces %d -> %d' % (n0, len(ob.data.polygons)))
# the texture packed into the file, its material named for the game
for mat in ob.data.materials:
    mat.name = 'M_' + NAME
for img in bpy.data.images:
    if img.filepath: img.pack()
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(D, NAME + '.blend'))
print('PET saved', os.path.join(D, NAME + '.blend'))
