# The A320's lamp places for Data/aircraft-lights.json (2026-11-28): FlightGear A320-common.xml offsets mapped as make_a320_fg.py
# places the model, checked against the nearest lens part of Art/A320/A320.glb, plus marker renders.
#   blender -b --factory-startup --python Art/A320/place_a320_lamps.py   (writes docs/Pictures/a320-lamps/: lamps.json, side/top/nose.png)
import bpy, math, json, mathutils, bmesh, sys
from mathutils import Matrix, Vector
D = 'C:/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal/docs/Pictures/a320-lamps/'
# --- 1. the transform make_a320_fg.py applies to body-fixed points (steps 1, 2, 2b)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath='F:/FlightGear-Aircraft/out/A320-200-CFM.glb')
obs = [o for o in bpy.data.objects if o.type == 'MESH']
for o in obs:
    o.data.transform(o.matrix_world); o.matrix_world = Matrix.Identity(4)
O = {o.name: o for o in obs}
def bbox(ns):
    vs = [v.co for n in ns for v in O[n].data.vertices]
    return Vector([min(v[i] for v in vs) for i in range(3)]), Vector([max(v[i] for v in vs) for i in range(3)])
def app(M, ns):
    for n in ns: O[n].data.transform(M)
K = ['NLGWheels', 'MLGWheelsL', 'MLGWheelsR']
M = Matrix.Rotation(math.radians(-90), 4, 'Z'); app(M, K)
lo, hi = bbox(['NLGWheels']); T = Matrix.Translation(Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))); app(T, K); M = T @ M
lm, hm = bbox(['MLGWheelsL', 'MLGWheelsR']); ang = math.atan2(-lm.z, -(lm.y + hm.y) / 2)
R = Matrix.Rotation(-ang, 4, 'X'); M = R @ M
M = Matrix.Translation(Vector((0, 0, -0.30))) @ M
print('LAMPS pitch', math.degrees(ang))
# FlightGear A320-common.xml offsets (x aft, y right, z up) and its compositor light directions
FG = {
 'nav_l': (1.89364, -16.6504, 0.317012), 'nav_r': (1.89364, 16.648, 0.317018), 'nav_tail': (18.8029, 0.0495674, 0.670205),
 'strobe_l': (2.03967, -16.7246, 0.319212), 'strobe_r': (2.03967, 16.7222, 0.319218), 'strobe_tail': (18.8029, -0.0495674, 0.670205),
 'beacon_top': (-3.58595, -0.001033, 1.94988), 'beacon_belly': (-3.97011, -0.001032, -2.50394),
 'landing_l': (-2.080, -2.22016, -1.69034), 'landing_r': (-2.080, 2.22016, -1.69034),
 'takeoff': (-13.5119, -0.185175, -2.30747), 'taxi': (-13.461, 0.20414, -2.28819),
 'turnoff_l': (-13.5034, -0.20993, -2.83921), 'turnoff_r': (-13.5034, 0.208994, -2.83921),
 'wingscan_l': (-9.99483, -1.97792, -0.202625), 'wingscan_r': (-9.99483, 1.97792, -0.202625),
}
FGD = {'landing_l': (-1, -0.175, -0.095), 'landing_r': (-1, 0.175, -0.095), 'taxi': (-1, 0, -0.2), 'takeoff': (-1, 0, -0.15),
       'turnoff_l': (-1, -0.571953125, -0.15), 'turnoff_r': (-1, 0.571953125, -0.15)}
P = {k: M @ Vector(v) for k, v in FG.items()}
Dr = {k: (M.to_3x3() @ Vector(v)).normalized() for k, v in FGD.items()}
# --- 2. the game model
for o in list(bpy.data.objects): bpy.data.objects.remove(o)
bpy.ops.import_scene.gltf(filepath='C:/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal/Art/A320/A320.glb')
ms = [o for o in bpy.data.objects if o.type == 'MESH']
TR = {}
for o in ms:
    bm = bmesh.new(); bm.from_mesh(o.data); bm.transform(o.matrix_world)
    TR[o.name] = (mathutils.bvhtree.BVHTree.FromBMesh(bm), [v.co.copy() for v in bm.verts]); bm.free()
def near(p, only=None):
    r = []
    for n, (t, _) in TR.items():
        if only and not any(n.startswith(s) for s in only): continue
        h = t.find_nearest(p)
        if h[0] is not None: r.append((h[3], n))
    r.sort(); return r[:2]
def ray(p, d, n):
    return TR[n][0].ray_cast(p, d)[0]
# logo lamps: on the tailplane top, 2.0 m out, a third back along its chord there, aimed at the fin
vs = TR['Hstabs'][1]
for k, sg in (('logo_l', -1), ('logo_r', 1)):
    hy = [i * 0.02 for i in range(-2000, -1000) if ray(Vector((sg * 2.0, i * 0.02, 20)), Vector((0, 0, -1)), 'Hstabs') is not None]
    y0 = max(hy); y1 = min(hy)   # (nose +y: the leading edge is the larger y)
    y = y0 - (y0 - y1) * 0.33
    top = ray(Vector((sg * 2.0, y, 20)), Vector((0, 0, -1)), 'Hstabs')
    P[k] = top + Vector((0, 0, 0.03))
    aim = Vector((0, y - 1.2, P[k].z + 2.4)); Dr[k] = (aim - P[k]).normalized()
# wing lights: aimed at the slat leading edge about 9 m out; the engine-scan beam from the same lamp at the intake lip
wv = TR['Wings'][1]
for k, e, sg in (('wingscan_l', 'enginescan_l', -1), ('wingscan_r', 'enginescan_r', 1)):
    hy = [i * 0.01 for i in range(-2000, -500) if ray(Vector((sg * 9.0, i * 0.01, 20)), Vector((0, 0, -1)), 'Wings') is not None]
    yl = max(hy); le = Vector((sg * 9.0, yl, 0)); le.z = ray(Vector((sg * 9.0, yl - 0.03, 20)), Vector((0, 0, -1)), 'Wings').z - 0.03
    Dr[k] = (le - P[k]).normalized()
    en = 'EngineCFM56' + ('L' if sg < 0 else 'R'); ev = TR[en][1]
    yf = max(v.y for v in ev); lip = [v for v in ev if v.y > yf - 0.05]; c = sum(lip, Vector()) / len(lip)
    P[e] = P[k].copy(); Dr[e] = (c - P[e]).normalized()
Dr['beacon_top'] = Vector((0, 0, 1)); Dr['beacon_belly'] = Vector((0, 0, -1))
LENS = {'nav_l': ['navglass'], 'nav_r': ['navglass'], 'strobe_l': ['navglass'], 'strobe_r': ['navglass'],
        'landing_l': ['LightWingL'], 'landing_r': ['LightWingR'], 'takeoff': ['NoseLegUpper'], 'taxi': ['NoseLegUpper'],
        'turnoff_l': ['NoseLegUpper'], 'turnoff_r': ['NoseLegUpper']}
res = {}
for k, p in P.items():
    res[k] = {'at': [round(p.x * 100, 1), round(-p.y * 100, 1), round(p.z * 100, 1)],
              'dir': [round(Dr[k].x, 3), round(-Dr[k].y, 3), round(Dr[k].z, 3)] if k in Dr else None,
              'near': [(round(d * 100, 1), n) for d, n in near(p)],
              'lens': [(round(d * 100, 1), n) for d, n in near(p, LENS.get(k))] if k in LENS else None}
json.dump(res, open(D + 'lamps.json', 'w'), indent=0)
# --- 3. markers and renders (workbench, x-ray so the far side shows)
COL = {'nav_l': (1, 0, 0), 'nav_r': (0, 1, 0), 'nav_tail': (1, 1, 1), 'strobe': (0.7, 0.9, 1), 'beacon': (1, 0.3, 0),
       'logo': (1, 0, 1), 'landing': (1, 1, 0), 'takeoff': (0, 1, 1), 'taxi': (0, 0.5, 1), 'turnoff': (0.5, 0, 1),
       'wingscan': (1, 0.6, 0.2), 'enginescan': (0.6, 0.3, 0.1)}
for o in ms: o.color = (0.55, 0.55, 0.58, 1)
for k, p in P.items():
    c = COL.get(k) or COL[k.split('_')[0]]
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.14, location=p); s = bpy.context.object; s.color = (*c, 1)
    if k in Dr and not k.startswith('beacon'):
        d = Dr[k]; L = 1.5
        bpy.ops.mesh.primitive_cylinder_add(radius=0.035, depth=L, location=p + d * L / 2)
        cy = bpy.context.object; cy.rotation_euler = Vector((0, 0, 1)).rotation_difference(d).to_euler(); cy.color = (*c, 1)
sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'
sh = sc.display.shading; sh.color_type = 'OBJECT'; sh.light = 'STUDIO'; sh.show_xray = True; sh.xray_alpha = 0.55
sc.world = bpy.data.worlds.new('w'); sc.world.color = (0.02, 0.02, 0.02)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = 'ORTHO'
def shot(loc, rot, scale, w, h, name):
    cam.location = loc; cam.rotation_euler = rot; cam.data.ortho_scale = scale
    sc.render.resolution_x = w; sc.render.resolution_y = h; sc.render.filepath = D + name; bpy.ops.render.render(write_still=True)
shot((-40, -16, 3.5), (math.radians(90), 0, math.radians(-90)), 40, 2400, 900, 'side.png')      # from the left: nose to the left
shot((0, -16, 40), (0, 0, 0), 38, 1800, 1900, 'top.png')
shot((-8, 0.3, 1.8), (math.radians(90), 0, math.radians(-90)), 3.2, 1000, 900, 'nose.png')
print('LAMPS done')
