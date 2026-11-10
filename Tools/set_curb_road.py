# The road outside the terminal (SITE/ROAD), the car parks (SITE/CARPARK, the rental centre's) and the road's curb (SITE/KERB-EDGE) in the textures made by
# Tools/make_curb_road.py. Only those two meshes: the kerb's tiles, the car park and everything else keep theirs.
#  - M_Road: light grey asphalt laid flat by world position, 2 m tiles, a normal map for the stones, matte.
#  - M_Curb: the curb stones along the curb (1 m stones), on its top with the dark joint on the road side (the road
#    is on the curb's +Y side in the world: the curb runs along x at plan z -20, world y 20 m), and down its upright
#    faces; laid from three sides and blended by which way each face looks, so the sides never stretch.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_curb_road.py" -unattended -nullrhi
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, 'Art', 'CurbRoad')
DEST = '/Game/Materials/CurbRoad'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()

def tex(name, srgb, comp):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(ART, name + '.png'); task.destination_path = DEST
    task.automated = True; task.replace_existing = True
    at.import_asset_tasks([task])
    t = unreal.load_asset(DEST + '/' + name)
    t.set_editor_property('srgb', srgb); t.set_editor_property('compression_settings', comp)
    eal.save_loaded_asset(t, only_if_is_dirty=False)
    return t

RD = tex('T_Road_D', True, unreal.TextureCompressionSettings.TC_DEFAULT)
RN = tex('T_Road_N', False, unreal.TextureCompressionSettings.TC_NORMALMAP)
CD = tex('T_Curb_D', True, unreal.TextureCompressionSettings.TC_DEFAULT)
CN = tex('T_Curb_N', False, unreal.TextureCompressionSettings.TC_NORMALMAP)

def material(name):
    path = DEST + '/' + name
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path); mel.delete_all_material_expressions(m); return m
    return at.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())

class G:
    """A small helper for building a material graph."""
    def __init__(self, m): self.m = m; self.y = 0
    def e(self, cls, x=-800):
        self.y += 60; return mel.create_material_expression(self.m, cls, x, self.y)
    def mask(self, src, ch, out=''):
        k = self.e(unreal.MaterialExpressionComponentMask)
        for c in 'rgba': k.set_editor_property(c, c in ch)
        mel.connect_material_expressions(src, out, k, ''); return k
    def op(self, cls, a, b):
        k = self.e(cls); mel.connect_material_expressions(a, '', k, 'A'); mel.connect_material_expressions(b, '', k, 'B'); return k
    def c(self, v):
        k = self.e(unreal.MaterialExpressionConstant); k.set_editor_property('r', v); return k
    def sample(self, t, uv, kind):
        s = self.e(unreal.MaterialExpressionTextureSample, -400); s.set_editor_property('texture', t); s.set_editor_property('sampler_type', kind)
        mel.connect_material_expressions(uv, '', s, 'UVs'); return s
    def append(self, a, b): return self.op(unreal.MaterialExpressionAppendVector, a, b)
    def div(self, a, b): return self.op(unreal.MaterialExpressionDivide, a, b)
    def mul(self, a, b): return self.op(unreal.MaterialExpressionMultiply, a, b)
    def add(self, a, b): return self.op(unreal.MaterialExpressionAdd, a, b)
    def sub(self, a, b): return self.op(unreal.MaterialExpressionSubtract, a, b)

COLOR, NORMAL = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL

# ---- the road -----------------------------------------------------------------------------------------------------
m = material('M_Road'); g = G(m)
wp = g.e(unreal.MaterialExpressionWorldPosition, -1200)
uv = g.append(g.div(g.mask(wp, 'r'), g.c(200.0)), g.div(g.mask(wp, 'g'), g.c(200.0)))
mel.connect_material_property(g.sample(RD, uv, COLOR), 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(g.sample(RN, uv, NORMAL), 'RGB', unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(g.c(0.86), '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(g.c(0.3), '', unreal.MaterialProperty.MP_SPECULAR)
m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
M_ROAD = m

# ---- the curb -----------------------------------------------------------------------------------------------------
m = material('M_Curb'); g = G(m)
wp = g.e(unreal.MaterialExpressionWorldPosition, -1400)
X, Y, Z = g.mask(wp, 'r'), g.mask(wp, 'g'), g.mask(wp, 'b')
along_x = g.div(X, g.c(100.0))                            # 1 m stones along the curb
across_top = g.div(g.sub(g.c(2015.0), Y), g.c(30.0))      # 0 on the road side (the joint), 1 on the kerb side
down_side = g.add(g.c(0.15), g.div(g.sub(g.c(16.0), Z), g.c(20.0)))   # the upright faces: the concrete below the joint
top = g.sample(CD, g.append(along_x, across_top), COLOR)
side = g.sample(CD, g.append(along_x, down_side), COLOR)
ends = g.sample(CD, g.append(g.div(Y, g.c(100.0)), down_side), COLOR)
vn = g.e(unreal.MaterialExpressionVertexNormalWS, -1400)
ab = g.e(unreal.MaterialExpressionAbs); mel.connect_material_expressions(vn, '', ab, '')
p4 = g.mul(g.mul(ab, ab), g.mul(ab, ab))
wx, wy, wz = g.mask(p4, 'r'), g.mask(p4, 'g'), g.mask(p4, 'b')
acc = g.add(g.add(g.mul(g.mask(ends, 'rgb'), wx), g.mul(g.mask(side, 'rgb'), wy)), g.mul(g.mask(top, 'rgb'), wz))
col = g.div(acc, g.add(g.add(wx, wy), wz))
mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(g.c(0.82), '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(g.c(0.35), '', unreal.MaterialProperty.MP_SPECULAR)
m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
M_CURB = m

done = []
# the road, the car park (SITE/CARPARK) and the rental centre's car park (rentalAsphalt) in the same asphalt; the curb
for idx, mat in ((2613, M_ROAD), (2615, M_ROAD), (2714, M_ROAD), (2617, M_CURB)):
    p = '/Game/Babylon/Terminal/terminal/StaticMeshes/terminal_mesh_%d' % idx
    if not eal.does_asset_exist(p): continue
    sm = unreal.load_asset(p)
    for k in range(len(sm.get_editor_property('static_materials'))): sm.set_material(k, mat)
    eal.save_loaded_asset(sm, only_if_is_dirty=False)
    done.append(idx)
unreal.log('ADP_CURBROAD: road and curb on meshes %s' % done)
unreal.SystemLibrary.quit_editor()
