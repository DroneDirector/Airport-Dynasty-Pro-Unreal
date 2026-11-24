import bpy,sys,math,os
from mathutils import Vector
a=sys.argv[sys.argv.index('--')+1:]; out=a[0]; W=int(a[1]); S=int(a[2]); views=a[3].split(',')
sc=bpy.context.scene
for o in list(sc.objects):
    if o.type in('LIGHT','CAMERA'): bpy.data.objects.remove(o)
obs=[o for o in sc.objects if o.type=='MESH' and not o.hide_render]
lo=Vector((1e9,)*3);hi=Vector((-1e9,)*3)
for o in obs:
    for c in o.bound_box:
        w=o.matrix_world@Vector(c); lo=Vector(map(min,lo,w));hi=Vector(map(max,hi,w))
c=(lo+hi)/2
# ground
bpy.ops.mesh.primitive_plane_add(size=160,location=(c.x,c.y,lo.z-0.002)); g=bpy.context.object
m=bpy.data.materials.new('G'); m.use_nodes=True; nt=m.node_tree; b=nt.nodes['Principled BSDF']; b.inputs['Roughness'].default_value=.85
n=nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value=40; n.inputs['Detail'].default_value=12
tc=nt.nodes.new('ShaderNodeTexCoord'); nt.links.new(tc.outputs['Object'],n.inputs['Vector'])
r=nt.nodes.new('ShaderNodeValToRGB'); r.color_ramp.elements[0].color=(.17,.168,.16,1); r.color_ramp.elements[1].color=(.30,.295,.285,1)
nt.links.new(n.outputs['Fac'],r.inputs['Fac'])
# slab joints
br=nt.nodes.new('ShaderNodeTexBrick'); br.offset=0; br.inputs['Scale'].default_value=1; br.inputs['Brick Width'].default_value=5; br.inputs['Row Height'].default_value=5; br.inputs['Mortar Size'].default_value=.012
br.inputs['Color1'].default_value=(1,1,1,1);br.inputs['Color2'].default_value=(.93,.93,.93,1);br.inputs['Mortar'].default_value=(.25,.25,.25,1)
nt.links.new(tc.outputs['Object'],br.inputs['Vector'])
mx=nt.nodes.new('ShaderNodeMixRGB'); mx.blend_type='MULTIPLY'; mx.inputs[0].default_value=1
nt.links.new(r.outputs[0],mx.inputs[1]); nt.links.new(br.outputs[0],mx.inputs[2]); nt.links.new(mx.outputs[0],b.inputs['Base Color'])
bp=nt.nodes.new('ShaderNodeBump'); bp.inputs['Strength'].default_value=.12; nt.links.new(n.outputs['Fac'],bp.inputs['Height']); nt.links.new(bp.outputs[0],b.inputs['Normal'])
g.data.materials.append(m)
w=bpy.data.worlds.new('W'); sc.world=w; w.use_nodes=True; nt=w.node_tree; bg=nt.nodes['Background']
sky=nt.nodes.new('ShaderNodeTexSky'); sky.sky_type='NISHITA'; sky.sun_elevation=math.radians(38); sky.sun_rotation=math.radians(40); sky.sun_size=math.radians(1.2); sky.air_density=1; sky.dust_density=1.5
nt.links.new(sky.outputs[0],bg.inputs[0]); bg.inputs[1].default_value=.16
sc.render.engine='CYCLES'
p=bpy.context.preferences.addons['cycles'].preferences; p.compute_device_type='OPTIX'; p.get_devices()
for d in p.devices: d.use=(d.type=='OPTIX')
sc.cycles.device='GPU'; sc.cycles.samples=S; sc.cycles.use_denoising=True; sc.cycles.denoiser='OPENIMAGEDENOISE'; sc.cycles.use_adaptive_sampling=True; sc.cycles.adaptive_threshold=.015
sc.render.resolution_x=W; sc.render.resolution_y=W*9//16; sc.render.resolution_percentage=100
sc.render.image_settings.file_format='PNG'; sc.render.image_settings.color_mode='RGB'; sc.render.film_transparent=False
sc.view_settings.view_transform='Filmic'; sc.view_settings.look='Medium High Contrast'
sc.render.use_persistent_data=True
sc.view_settings.exposure=-1.75
for mt in bpy.data.materials:
    if mt.name.split('.')[0].endswith('_Glass'):
        mt.use_nodes=True; t=mt.node_tree; t.nodes.clear(); o=t.nodes.new('ShaderNodeOutputMaterial'); mix=t.nodes.new('ShaderNodeMixShader'); f=t.nodes.new('ShaderNodeFresnel'); f.inputs[0].default_value=1.45
        tr=t.nodes.new('ShaderNodeBsdfTransparent'); tr.inputs[0].default_value=(.88,.92,.92,1); gl=t.nodes.new('ShaderNodeBsdfGlossy'); gl.inputs['Roughness'].default_value=.01
        t.links.new(f.outputs[0],mix.inputs[0]);t.links.new(tr.outputs[0],mix.inputs[1]);t.links.new(gl.outputs[0],mix.inputs[2]);t.links.new(mix.outputs[0],o.inputs[0])
cam=bpy.data.cameras.new('C'); co=bpy.data.objects.new('C',cam); sc.collection.objects.link(co); sc.camera=co; cam.lens=50; cam.clip_end=500
V={'A':(35,1.0),'B':(-35,1.0),'C':(145,1.0),'D':(215,1.0),'S':(90,.95)}
pts=[o.matrix_world@Vector(k) for o in obs for k in o.bound_box]
from bpy_extras.object_utils import world_to_camera_view as wcv
for v in views:
    az,_=V[v]; az=math.radians(az); d=6
    while True:
        e=Vector((c.x+d*math.cos(az),c.y+d*math.sin(az),lo.z+ 1.75+ .06*d)); co.location=e
        t=Vector((c.x,c.y,c.z-.05)); co.rotation_euler=(t-e).to_track_quat('-Z','Y').to_euler(); bpy.context.view_layer.update()
        q=[wcv(sc,co,x) for x in pts]
        if min(min(k.x for k in q),1-max(k.x for k in q),min(k.y for k in q),1-max(k.y for k in q))>.06 and min(k.z for k in q)>0: break
        d+=.2
    sc.render.filepath=out+'_'+v+'.png'; bpy.ops.render.render(write_still=True); print('DONE',out,v,d)
