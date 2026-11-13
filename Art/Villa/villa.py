# The player's own home: a one-storey modern villa at the end of its own cul-de-sac in the home town (town plan
# marker 8), its glass living wing facing north to the glacier. L-shaped: living wing (glass to the north, terrace and
# hot tub), bedroom wing to the south-east, double garage with an electric car charger on the west, the entrance in
# the sheltered courtyard between them, a long drive-in from the south. Dark charred-timber boards, a basalt plinth,
# flat roofs with thin deep overhangs. Real size in metres, north = +Y.
import sys, math; sys.path.insert(0, '/home/claude/bld')
import bpy
from lib import *

reset()
view = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'garden'
samples = int(sys.argv[sys.argv.index('--') + 2]) if '--' in sys.argv and len(sys.argv) > sys.argv.index('--') + 2 else 128

# ── materials ──
timber = mat('charred_timber', (0.035, 0.03, 0.028), 0.6, wave='X', bump=0.35)          # vertical boards
timber_l = mat('larch', (0.36, 0.22, 0.12), 0.55, wave='X', bump=0.25)                   # warm soffits and the entrance
basalt = mat('basalt', (0.06, 0.06, 0.065), 0.75, bump=0.6, bump_scale=6, var=0.25)
roof_edge = mat('roof_edge', (0.04, 0.04, 0.045), 0.35, 0.7)
glass = mat('glass', (0.92, 0.96, 0.97), 0.0, glass=True)
frame = mat('frame', (0.03, 0.03, 0.035), 0.3, 0.8)
deck = mat('deck', (0.32, 0.2, 0.11), 0.6, wave='Y', bump=0.3)
plinth = img_mat('plinth', 'concrete_col.png', 'concrete_h.png', 1.5, 0.7, 0.15)
asph = img_mat('asphalt', 'asphalt_col.png', 'asphalt_h.png', 3.0, 0.88, 0.25, box=False)
gravel = img_mat('gravel', 'gravel_col.png', 'gravel_h.png', 1.6, 0.85, 0.8, box=False)
lawn = img_mat('lawn', 'lawn_col.png', 'lawn_h.png', 2.5, 0.9, 0.4, box=False)
oak = mat('oak_floor', (0.42, 0.28, 0.16), 0.35, wave='Y', bump=0.05)
white = mat('white', (0.86, 0.86, 0.85), 0.75)
sofa_f = mat('sofa', (0.55, 0.52, 0.47), 0.9, bump=0.2, bump_scale=80)
rug = mat('rug', (0.3, 0.27, 0.24), 0.95, bump=0.3, bump_scale=120)
black = mat('black', (0.02, 0.02, 0.02), 0.4)
fire = mat('fire', (1, 0.5, 0.1), 0.5, emit=(1.0, 0.45, 0.12), emit_str=18)
warm = mat('warm_light', (1, 1, 1), 0.2, emit=(1.0, 0.78, 0.5), emit_str=12)
water = mat('tub_water', (0.2, 0.45, 0.5), 0.02, glass=True)
birch_bark = mat('birch_bark', (0.75, 0.73, 0.68), 0.7, bump=0.4, bump_scale=20, var=0.3)
leaf = mat('birch_leaf', (0.16, 0.3, 0.06), 0.7, bump=0.8, bump_scale=12, var=0.35)
lava = mat('lava_rock', (0.05, 0.045, 0.045), 0.9, bump=1.5, bump_scale=4, var=0.3)
charger = mat('charger', (0.9, 0.9, 0.9), 0.3, coat=0.5)
charge_led = mat('charge_led', (0.1, 0.6, 1.0), 0.2, emit=(0.1, 0.6, 1.0), emit_str=6)

H, RZ = 3.5, 3.9              # wall height, roof top
OVH = 1.6                     # roof overhang
def wing(name, x0, y0, x1, y1, glass_sides=(), solid=timber):
    """A wing: basalt plinth, timber walls (or full-height glass on the listed sides: 'n','s','e','w'), flat roof."""
    box(name + '_plinth', (x1 - x0 + 0.3, y1 - y0 + 0.3, 0.45), ((x0 + x1) / 2, (y0 + y1) / 2, 0.225), basalt)
    box(name + '_floor', (x1 - x0 - 0.3, y1 - y0 - 0.3, 0.02), ((x0 + x1) / 2, (y0 + y1) / 2, 0.46), oak)
    t = 0.3
    for side in 'nsew':
        if side in 'ns':
            y = y1 - t / 2 if side == 'n' else y0 + t / 2; L = x1 - x0; cx = (x0 + x1) / 2
            if side in glass_sides:
                n = int(L / 2.4)
                for i in range(n):
                    xx = x0 + (i + 0.5) * L / n
                    box(name + '_glass', (L / n - 0.06, 0.03, H - 0.1), (xx, y, 0.45 + (H - 0.1) / 2), glass)
                    box(name + '_mull', (0.07, 0.12, H), (x0 + i * L / n, y, 0.45 + H / 2), frame)
                box(name + '_mull', (0.07, 0.12, H), (x1, y, 0.45 + H / 2), frame)
            else:
                box(name + '_wall', (L, t, H), (cx, y, 0.45 + H / 2), solid)
        else:
            x = x1 - t / 2 if side == 'e' else x0 + t / 2; L = y1 - y0; cy = (y0 + y1) / 2
            if side in glass_sides:
                n = int(L / 2.4)
                for i in range(n):
                    yy = y0 + (i + 0.5) * L / n
                    box(name + '_glass', (0.03, L / n - 0.06, H - 0.1), (x, yy, 0.45 + (H - 0.1) / 2), glass)
                    box(name + '_mull', (0.12, 0.07, H), (x, y0 + i * L / n, 0.45 + H / 2), frame)
            else:
                box(name + '_wall', (t, L, H), (x, cy, 0.45 + H / 2), solid)
    # the roof: a thin dark edge, warm larch soffit underneath the overhang
    box(name + '_roof', (x1 - x0 + 2 * OVH, y1 - y0 + 2 * OVH, 0.32), ((x0 + x1) / 2, (y0 + y1) / 2, 0.45 + H + 0.16), roof_edge)
    box(name + '_soffit', (x1 - x0 + 2 * OVH - 0.1, y1 - y0 + 2 * OVH - 0.1, 0.02), ((x0 + x1) / 2, (y0 + y1) / 2, 0.45 + H - 0.005), timber_l)
    box(name + '_ceiling', (x1 - x0 - 0.6, y1 - y0 - 0.6, 0.02), ((x0 + x1) / 2, (y0 + y1) / 2, 0.45 + H - 0.03), white)

# living wing (glass to the north and west), bedroom wing (south-east), garage (south-west)
wing('living', 0, 0, 26, 11, glass_sides=('n', 'w'))
wing('bedrooms', 17, -17, 28, 0, glass_sides=('e',))
wing('garage', -1, -9, 8, 0)
# the garage's two doors (south), its charger on the wall by the drive
for x in (1.6, 5.4):
    box('garage_door', (3.2, 0.12, 2.5), (x, -9.08, 0.45 + 1.25), mat('garage_door', (0.05, 0.05, 0.055), 0.35, 0.6, wave='Z', bump=0.2))
box('charger', (0.35, 0.15, 0.5), (7.6, -9.25, 1.6), charger, bevel=0.03); box('charger_led', (0.12, 0.02, 0.03), (7.6, -9.33, 1.75), charge_led)
cyl('charger_cable', 0.02, 1.2, (7.6, -9.3, 1.0), black, 8)
# the entrance: a larch-lined recess in the courtyard, a tall pivot door, a light
box('entry_wall', (2.6, 0.32, H), (12.5, 0.16, 0.45 + H / 2), timber_l)
box('front_door', (1.3, 0.1, 2.6), (12.5, -0.05, 0.45 + 1.3), mat('door', (0.03, 0.025, 0.02), 0.4, 0.2))
box('door_pull', (0.04, 0.06, 1.4), (12.0, -0.13, 1.6), mat('brass', (0.8, 0.6, 0.3), 0.25, 1.0))
box('entry_canopy', (9.0, 9.0, 0.3), (12.5, -4.5, 0.45 + H + 0.15), roof_edge)
box('entry_path', (2.0, 9.0, 0.06), (12.5, -4.5, 0.03), plinth)
area_light('entry_light', (12.5, -2.0, H + 0.3), 1.0, 60, color=(1, 0.8, 0.55), rot=(0, 0, 0))
# courtyard: gravel, a few lava rocks, one birch
box('courtyard', (9.0, 8.8, 0.05), (12.5, -4.6, 0.02), gravel)

# ── inside the living wing (seen through the glass) ──
box('rug', (5.0, 3.5, 0.02), (6.0, 6.5, 0.47), rug)
box('sofa_a', (4.2, 1.0, 0.45), (6.0, 4.3, 0.7), sofa_f, bevel=0.08); box('sofa_a_back', (4.2, 0.25, 0.5), (6.0, 3.8, 1.15), sofa_f, bevel=0.08)
box('sofa_b', (1.0, 2.6, 0.45), (3.6, 6.0, 0.7), sofa_f, bevel=0.08); box('sofa_b_back', (0.25, 2.6, 0.5), (3.1, 6.0, 1.15), sofa_f, bevel=0.08)
box('coffee_table', (1.6, 1.0, 0.06), (6.5, 6.4, 0.85), oak, bevel=0.01); box('ct_base', (1.2, 0.6, 0.35), (6.5, 6.4, 0.62), black)
box('fireplace', (2.4, 0.6, 1.0), (6.0, 0.9, 1.2), black); box('fire', (1.6, 0.1, 0.25), (6.0, 1.22, 1.1), fire)
box('chimney_wall', (3.2, 0.5, H), (6.0, 0.55, 0.45 + H / 2), basalt)
box('dining_table', (3.0, 1.1, 0.06), (14.0, 6.5, 1.2), oak, bevel=0.01); box('dt_leg', (2.6, 0.2, 0.7), (14.0, 6.5, 0.82), black)
for k in range(3):
    for s in (-1, 1): box('chair', (0.48, 0.48, 0.48), (13.0 + k * 1.0, 6.5 + s * 0.85, 0.7), sofa_f, bevel=0.05)
for k in range(3): cyl('pendant', 0.18, 0.25, (13.0 + k * 1.0, 6.5, 2.9), warm, 20); cyl('pendant_cord', 0.005, 1.0, (13.0 + k * 1.0, 6.5, 3.5), black, 6)
box('kitchen_island', (3.6, 1.1, 0.95), (20.5, 6.0, 0.93), mat('stone_top', (0.85, 0.84, 0.82), 0.2, coat=0.6), bevel=0.02)
box('kitchen_back', (7.0, 0.65, 0.95), (21.5, 1.0, 0.93), black); box('kitchen_tall', (3.0, 0.65, 2.6), (24.5, 1.0, 1.75), oak)
for x in (4.0, 10.0, 16.0, 22.0): area_light('inside', (x, 5.5, H + 0.3), 3.0, 140, color=(1, 0.82, 0.6))

# ── terrace to the north: a deck, the hot tub, an outdoor sofa, a fire pit ──
box('deck', (30.0, 7.0, 0.3), (12.0, 14.5, 0.15), deck)
cyl('tub', 1.25, 0.9, (23.5, 15.0, 0.45), mat('tub_shell', (0.25, 0.2, 0.15), 0.6, wave='Z', bump=0.2), 48)
cyl('tub_water', 1.12, 0.05, (23.5, 15.0, 0.85), water, 48)
area_light('tub_glow', (23.5, 15.0, 0.6), 1.5, 25, color=(0.4, 0.9, 1.0), rot=(math.pi, 0, 0))
box('out_sofa', (3.0, 0.9, 0.4), (8.0, 15.5, 0.5), sofa_f, bevel=0.06); box('out_sofa_back', (3.0, 0.25, 0.45), (8.0, 15.95, 0.9), sofa_f, bevel=0.06)
cyl('fire_pit', 0.6, 0.4, (8.0, 13.6, 0.5), basalt, 32); cyl('fire_pit_flame', 0.35, 0.1, (8.0, 13.6, 0.75), fire, 16)
for x in range(-2, 28, 4): cyl('deck_light', 0.05, 0.02, (x, 17.95, 0.31), warm, 10)

# ── the site: lawn, the drive, birches, lava rocks, the mountains and glacier to the north ──
box('lawn', (90000, 90000, 0.08), (0, 0, -0.04), lawn, c='Site')
box('drive', (6.5, 70.0, 0.06), (3.5, -44.0, 0.03), asph, c='Site'); box('drive_apron', (10.0, 6.0, 0.06), (3.5, -12.0, 0.03), asph, c='Site')
for y in range(-75, -12, 10):
    cyl('drive_lamp', 0.07, 0.8, (7.3, y, 0.4), roof_edge, 12, c='Site'); cyl('drive_lamp_l', 0.07, 0.08, (7.3, y, 0.84), warm, 12, c='Site')
car2('my_car', (3.0, -17.0, 0.0), math.pi / 2, (0.06, 0.07, 0.08), kind='suv')
import random; random.seed(4)
def birch(x, y, s=1.0):
    h = 7 * s
    cyl('birch_trunk', 0.14 * s, h, (x, y, h / 2), birch_bark, 12, c='Site')
    for k in range(14):
        a = random.uniform(0, 2 * math.pi); r = random.uniform(0.3, 1.3) * s
        bpy.ops.mesh.primitive_ico_sphere_add(radius=random.uniform(0.55, 0.95) * s, subdivisions=3,
                                              location=(x + r * math.cos(a), y + r * math.sin(a), h * random.uniform(0.5, 1.0)))
        o = bpy.context.active_object; o.scale = (1, 1, 1.3); o.data.materials.append(leaf); [setattr(p, 'use_smooth', True) for p in o.data.polygons]
        md = o.modifiers.new('d', 'DISPLACE'); tx = bpy.data.textures.new('leafn', 'CLOUDS'); tx.noise_scale = 0.4; md.texture = tx; md.strength = 0.5
        for cc in o.users_collection: cc.objects.unlink(o)
        col('Site').objects.link(o)
for (x, y, s) in [(-12, 6, 1.0), (-15, 14, 0.8), (-9, 20, 1.1), (36, 4, 0.9), (40, 12, 1.0), (34, -14, 1.0), 
                  (-20, -4, 1.0), (44, -4, 0.8), (16, -12, 0.7), (-24, 26, 1.0), (48, 24, 1.1)]:
    birch(x, y, s)
for (x, y, r) in [(15, -6, 0.6), (10.5, -7.5, 0.4), (-5, 24, 0.7), (2, 20, 0.5)]:
    bpy.ops.mesh.primitive_ico_sphere_add(radius=r, subdivisions=2, location=(x, y, r * 0.4)); o = bpy.context.active_object
    o.scale = (1.3, 1.0, 0.7); o.data.materials.append(lava)
    md = o.modifiers.new('d', 'DISPLACE'); tx = bpy.data.textures.new('rockn', 'CLOUDS'); tx.noise_scale = 0.3; md.texture = tx; md.strength = 0.3
    for cc in o.users_collection: cc.objects.unlink(o)
    col('Site').objects.link(o)
# the glacier to the north: a long mountain range with an ice cap, far away
bpy.ops.mesh.primitive_grid_add(x_subdivisions=400, y_subdivisions=60, size=1, location=(0, 26000, 0))
mt = bpy.context.active_object; mt.scale = (90000, 12000, 1); bpy.ops.object.transform_apply(scale=True)
for v in mt.data.vertices:
    yy = (v.co.y - 20000) / 12000; xx = v.co.x
    ridge = max(0.0, math.sin(min(max(yy, 0.0), 1.0) * math.pi)) ** 1.5
    v.co.z = ridge * (1250 + 380 * math.sin(xx / 6000) + 220 * math.sin(xx / 2100 + 1.3) + 80 * math.sin(xx / 700))
m_mt = bpy.data.materials.new('mountain'); m_mt.use_nodes = True; nt = m_mt.node_tree; b = nt.nodes['Principled BSDF']
geo = nt.nodes.new('ShaderNodeNewGeometry'); sep = nt.nodes.new('ShaderNodeSeparateXYZ'); nt.links.new(geo.outputs['Position'], sep.inputs[0])
ramp = nt.nodes.new('ShaderNodeValToRGB'); mr = nt.nodes.new('ShaderNodeMapRange'); mr.inputs['From Max'].default_value = 1500
nt.links.new(sep.outputs[2], mr.inputs['Value']); nt.links.new(mr.outputs['Result'], ramp.inputs['Fac'])
ramp.color_ramp.elements[0].color = (0.05, 0.06, 0.05, 1); ramp.color_ramp.elements[1].position = 0.55; ramp.color_ramp.elements[1].color = (0.9, 0.93, 0.97, 1)
e = ramp.color_ramp.elements.new(0.45); e.color = (0.12, 0.12, 0.12, 1)
nt.links.new(ramp.outputs['Color'], b.inputs['Base Color']); b.inputs['Roughness'].default_value = 0.8
mt.data.materials.append(m_mt); [p.__setattr__('use_smooth', True) for p in mt.data.polygons]
for cc in mt.users_collection: cc.objects.unlink(mt)
col('Site').objects.link(mt)

# ── light: a late summer evening, the sun low in the north-west; camera ──
sky(sun_elev=7, sun_rot=0, strength=1.0)
sun = bpy.data.objects['sun']; az, el = math.radians(305), math.radians(7)
d = -Vector((math.cos(el) * math.sin(az), math.cos(el) * math.cos(az), math.sin(el)))
sun.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler(); sun.data.color = (1.0, 0.78, 0.55)
if view == 'garden':
    camera((33.0, 27.0, 2.0), (12.0, 5.0, 2.2), lens=24); out = f'{OUT}/villa_garden.png'
elif view == 'drive':
    camera((-14.0, -36.0, 3.2), (9.0, -4.0, 2.3), lens=26); out = f'{OUT}/villa_drive.png'
else:
    camera((33.0, 27.0, 2.0), (12.0, 5.0, 2.2), lens=24); out = f'{OUT}/villa_check.png'
if view == 'checkd': camera((-14.0, -36.0, 3.2), (9.0, -4.0, 2.3), lens=26); out = f'{OUT}/villa_checkd.png'
if view == 'export':
    bpy.ops.file.pack_all(); bpy.ops.wm.save_as_mainfile(filepath=f'{OUT}/Villa.blend')
    for o in bpy.context.scene.objects: o.select_set(False)
    for o in col('Building').all_objects: o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=f'{OUT}/Villa.glb', use_selection=True, export_apply=True)
else:
    render(out, res=(1920, 1080) if view in ('garden', 'drive') else (640, 360), samples=samples)
print('DONE', view)
