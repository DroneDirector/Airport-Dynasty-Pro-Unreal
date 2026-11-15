# The Airbus A320 parked at the terminal's gates (owner, 2026-11-15: "use this model, replace all of the custom
# airplanes at the apron"). His model (Art/A320/A320.blend, from Meshy, one mesh, its colour / roughness-metal / normal
# textures packed) made ready for the game, the shape untouched:
#   - real size: 37.57 m nose to tail (the file is 1.90 long)
#   - nose along +y (the game's loader turns a Blender model's +y to the actor's right: ParkedAircraft.cpp turns it)
#   - level on its wheels: the nose tyre and both main tyres touch z 0 (a tiny pitch, if the file sits nose down)
#   - its origin where the nose tyre touches the ground: the stand's stop bar is where that point goes
#   - mirrored left-right (the loader mirrors x back: the lettering reads the right way in the game)
#   - exported as Art/A320/A320.glb (one mesh, one material: Nanite in Unreal; textures embedded)
# Needs Blender 3.6 or later (the file is from Blender 4.x): F:/Tools/Blender-4.2/blender.exe -b Art/A320/A320.blend --python Art/A320/make_a320.py
import bpy, math, os
import numpy as np
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
LENGTH = 37.57                      # A320: 37.57 m overall
o = bpy.data.objects['Mesh_0']
me = o.data
o.name = 'A320'; me.name = 'A320'
co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
k = LENGTH / (co[:, 0].max() - co[:, 0].min())
# nose along -x in the file: turn it to +y, scale, mirror x
M = Matrix.Diagonal((k, k, k, 1.0))
M = Matrix.Rotation(-math.pi / 2, 4, 'Z') @ M          # (-x -> +y)
M = Matrix.Diagonal((-1.0, 1.0, 1.0, 1.0)) @ M         # (the loader's mirror, undone in advance)
me.transform(M); me.flip_normals(); me.update()
co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
# the tyres: the lowest points near the front (nose tyre) and the two groups behind (main tyres, left and right)
zmin = co[:, 2].min()
low = co[co[:, 2] < zmin + 0.35]
nose = low[low[:, 1] > co[:, 1].max() - 0.35 * LENGTH]
mains = low[low[:, 1] < co[:, 1].max() - 0.35 * LENGTH]
nz = nose[:, 2].min(); ny = nose[nose[:, 2] < nz + 0.02][:, 1].mean(); nx = nose[nose[:, 2] < nz + 0.02][:, 0].mean()
ml = mains[mains[:, 0] < 0]; mr = mains[mains[:, 0] > 0]
mz = (ml[:, 2].min() + mr[:, 2].min()) / 2
my = (mains[mains[:, 2] < mz + 0.03][:, 1]).mean()
print('A320: scale %.3f, nose tyre y %.2f z %.3f, main tyres y %.2f z %.3f (left %.3f right %.3f)' % (k, ny, nz, my, mz, ml[:, 2].min(), mr[:, 2].min()))
# level: pitch about x so the nose and main tyres touch the same plane, then the nose tyre's contact point to the origin
pitch = math.atan2(nz - mz, ny - my)
me.transform(Matrix.Rotation(-pitch, 4, 'X'))
co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
nose = co[(co[:, 1] > co[:, 1].max() - 0.35 * LENGTH) & (co[:, 2] < co[:, 2].min() + 0.35)]
nz = nose[:, 2].min(); sel = nose[nose[:, 2] < nz + 0.02]
me.transform(Matrix.Translation(Vector((-sel[:, 0].mean(), -sel[:, 1].mean(), -nz))))
co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
mains = co[(co[:, 1] < -6.0) & (co[:, 2] < 0.35)]
print('A320: pitched %.3f deg; nose tyre at the origin; main tyres lowest z left %.3f right %.3f, at y %.2f; nose tip y %.2f, tail y %.2f, span %.2f, height %.2f'
      % (math.degrees(pitch), mains[mains[:, 0] < 0][:, 2].min(), mains[mains[:, 0] > 0][:, 2].min(),
         mains[mains[:, 2] < 0.05][:, 1].mean(), co[:, 1].max(), co[:, 1].min(), co[:, 0].max() - co[:, 0].min(), co[:, 2].max()))
o.matrix_world = Matrix.Identity(4)
# no empty material slots (material-slot-check rule)
assert all(s.material for s in o.material_slots), 'empty material slot'
# only the aircraft (cameras, lights - none in the file - and anything else stay out)
for x in list(bpy.data.objects):
    if x is not o: bpy.data.objects.remove(x)
bpy.ops.export_scene.gltf(filepath=os.path.join(HERE, 'A320.glb'), export_format='GLB', use_selection=False,
                          export_apply=True, export_texcoords=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO')
print('A320: written', os.path.join(HERE, 'A320.glb'))
print('DONE')
