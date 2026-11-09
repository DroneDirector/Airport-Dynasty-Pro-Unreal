# The hall's eight tree columns (six in the Grand Hall, two under the roof at the kerb: trunk, base and five arms each)
# in the owner's white stone (made seamless by Tools/make_pillar_stone.py). The columns are round and their arms
# curve, so the stone is laid from three sides at once and blended by which way each bit of surface faces (no
# stretching anywhere); a stone tile is 1.2 m wide and 2.4 m tall. Polished: a soft sheen, never a mirror.
# Only these meshes (used by nothing else in the model).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_pillar_stone.py" -unattended -nullrhi
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = '/Game/Materials/PillarStone'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()

task = unreal.AssetImportTask()
task.filename = os.path.join(ROOT, 'Art', 'PillarStone', 'T_PillarStone_D.png')
task.destination_path = DEST
task.automated = True
task.replace_existing = True
at.import_asset_tasks([task])
T = unreal.load_asset(DEST + '/T_PillarStone_D')
eal.save_loaded_asset(T, only_if_is_dirty=False)

path = DEST + '/M_PillarStone'
if eal.does_asset_exist(path):
    m = unreal.load_asset(path); mel.delete_all_material_expressions(m)
else:
    m = at.create_asset('M_PillarStone', DEST, unreal.Material, unreal.MaterialFactoryNew())
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)

def mask(src, ch, x, y, out=''):
    k = E(unreal.MaterialExpressionComponentMask, x, y)
    for c in 'rgba':
        k.set_editor_property(c, c in ch)
    mel.connect_material_expressions(src, out, k, '')
    return k

def op(cls, a, b, x, y):
    k = E(cls, x, y)
    mel.connect_material_expressions(a, '', k, 'A'); mel.connect_material_expressions(b, '', k, 'B')
    return k

def const(v, x, y):
    k = E(unreal.MaterialExpressionConstant, x, y); k.set_editor_property('r', v); return k

wp = E(unreal.MaterialExpressionWorldPosition, -1800, 0)
X, Y, Z = mask(wp, 'r', -1650, -100), mask(wp, 'g', -1650, 0), mask(wp, 'b', -1650, 100)
tw, th = const(120.0, -1650, 220), const(-240.0, -1650, 300)
ux, uy, vz = op(unreal.MaterialExpressionDivide, X, tw, -1500, -100), op(unreal.MaterialExpressionDivide, Y, tw, -1500, 0), op(unreal.MaterialExpressionDivide, Z, th, -1500, 100)
vy = op(unreal.MaterialExpressionDivide, Y, th, -1500, 200)

def sample(u, v, y):
    uv = op(unreal.MaterialExpressionAppendVector, u, v, -1300, y)
    s = E(unreal.MaterialExpressionTextureSample, -1150, y); s.set_editor_property('texture', T)
    mel.connect_material_expressions(uv, '', s, 'UVs')
    return s

s_xz = sample(ux, vz, -300)    # seen from the front or back (facing Y)
s_yz = sample(uy, vz, 0)       # seen from the sides (facing X)
s_xy = sample(ux, vy, 300)     # seen from above or below

# blend weights: how much the surface faces each way (sharpened), from the vertex normal (either sign)
vn = E(unreal.MaterialExpressionVertexNormalWS, -1300, 600)
ab = E(unreal.MaterialExpressionAbs, -1150, 600); mel.connect_material_expressions(vn, '', ab, '')
p2 = op(unreal.MaterialExpressionMultiply, ab, ab, -1000, 600)
p4 = op(unreal.MaterialExpressionMultiply, p2, p2, -850, 600)
wx, wy, wz = mask(p4, 'r', -700, 520), mask(p4, 'g', -700, 600), mask(p4, 'b', -700, 680)
sumw = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionAdd, wx, wy, -550, 560), wz, -420, 600)
acc = op(unreal.MaterialExpressionAdd,
         op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, mask(s_yz, 'rgb', -950, 0, ''), wx, -800, 0),
            op(unreal.MaterialExpressionMultiply, mask(s_xz, 'rgb', -950, -300, ''), wy, -800, -300), -650, -150),
         op(unreal.MaterialExpressionMultiply, mask(s_xy, 'rgb', -950, 300, ''), wz, -800, 300), -500, 0)
col = op(unreal.MaterialExpressionDivide, acc, sumw, -350, 0)
bright = op(unreal.MaterialExpressionMultiply, col, const(0.92, -500, 150), -220, 0)
mel.connect_material_property(bright, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(const(0.3, -220, 200), '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(const(0.5, -220, 280), '', unreal.MaterialProperty.MP_SPECULAR)
m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)

PILLARS = [83, 84] + list(range(85, 115)) + list(range(1005, 1015))   # trunks, bases, the arms of all eight
done = 0
for i in PILLARS:
    p = '/Game/Babylon/Terminal/terminal/StaticMeshes/terminal_mesh_%d' % i
    if not eal.does_asset_exist(p):
        continue
    sm = unreal.load_asset(p)
    for k in range(len(sm.get_editor_property('static_materials'))):
        sm.set_material(k, m)
    eal.save_loaded_asset(sm, only_if_is_dirty=False)
    done += 1
unreal.log('ADP_PILLAR: stone on %d column meshes' % done)
unreal.SystemLibrary.quit_editor()
