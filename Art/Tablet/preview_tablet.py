# Preview pictures of the tablet for the owner's OK (to-do 117): front straight on, front at an angle, the back - a
# blue wave picture on the screen like his photo's (only for the preview; the game draws its own UI there).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b Art/Tablet/Tablet.blend --python Art/Tablet/preview_tablet.py
import bpy, math, os
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', 'docs', 'Pictures', 'Tablet')
os.makedirs(OUT, exist_ok=True)
sc = bpy.context.scene
sc.render.engine = 'CYCLES'; sc.cycles.samples = 96; sc.cycles.use_denoising = True
sc.render.resolution_x, sc.render.resolution_y = 3840, 2160
sc.render.film_transparent = False
w = bpy.data.worlds.new('W'); sc.world = w; w.use_nodes = True
bg = w.node_tree.nodes['Background']; bg.inputs[0].default_value = (0.16, 0.17, 0.18, 1); bg.inputs[1].default_value = 0.6   # (a dark studio: the black glass and graphite read true)
# the screen: his own drone photo as the wallpaper (Art/Tablet/Wallpapers/owner-drone-sea-stack.jpg), lit as a screen is
m = bpy.data.materials['TabletScreen']; nt = m.node_tree; nt.nodes.clear()
out = nt.nodes.new('ShaderNodeOutputMaterial'); em = nt.nodes.new('ShaderNodeEmission'); em.inputs[1].default_value = 1.0
img = nt.nodes.new('ShaderNodeTexImage'); img.image = bpy.data.images.load(os.path.join(HERE, 'Wallpapers', 'owner-drone-sea-stack.jpg'))
tc = nt.nodes.new('ShaderNodeTexCoord'); nt.links.new(tc.outputs['UV'], img.inputs['Vector'])
nt.links.new(img.outputs['Color'], em.inputs['Color']); nt.links.new(em.outputs[0], out.inputs[0])
# light: a big soft key and a fill
for nm, loc, en, size in (('Key', (0.35, -0.3, 0.6), 30, 0.6), ('Fill', (-0.4, 0.2, 0.4), 10, 0.8), ('Rim', (0.0, 0.5, -0.3), 18, 0.5), ('Under', (-0.2, 0.3, -0.5), 14, 0.7)):
    ld = bpy.data.lights.new(nm, 'AREA'); ld.energy = en; ld.size = size
    lo = bpy.data.objects.new(nm, ld); sc.collection.objects.link(lo); lo.location = loc
    d = lo.location.normalized() * -1; lo.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
cd = bpy.data.cameras.new('Cam'); cd.lens = 60; cam = bpy.data.objects.new('Cam', cd); sc.collection.objects.link(cam); sc.camera = cam
def shot(name, loc, look):
    from mathutils import Vector
    cam.location = loc; d = (Vector(look) - Vector(loc)); cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
    sc.render.filepath = os.path.join(OUT, name); bpy.ops.render.render(write_still=True); print('SHOT', name)
shot('tablet-preview-1-front.png', (0.0, -0.01, 0.62), (0, 0, 0.0035))
shot('tablet-preview-2-angle.png', (0.34, -0.50, 0.30), (0, 0, 0.0035))
shot('tablet-preview-3-back.png', (-0.30, 0.40, -0.42), (0, 0, 0.0))
