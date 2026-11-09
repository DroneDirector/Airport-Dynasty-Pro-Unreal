# The second floor's wood floor (the owner's planks, made seamless by Tools/make_wood_floor.py): the maps imported,
# the material M_WoodFloor built, and every second-floor floor surface given it: the long walkway, the link, the
# landing by the stairs, the food court, the bar, the family room, tax-free, the gate rooms and their view areas, and
# the eateries (their floors; their tiled walls keep a dark tile colour). The stairs are their own meshes and keep
# theirs; the slab's underside (the ground floor's ceiling) is another mesh and is not touched; the toilets keep tiles.
# The wood is laid by world position (no stretching anywhere, the planks run along the building), 18 cm planks;
# matte, as oiled wood is: the room's light shows it clearly but nothing mirrors in it (owner, 2026-11-09); a separate
# detail layer of wood fibres (at two sizes) so the light catches the grain's filaments up close.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_wood_floor.py" -unattended -nullrhi
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, 'Art', 'WoodFloor')
DEST = '/Game/Materials/WoodFloor'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()

def tex(name, srgb, comp):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(ART, name + '.png')
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = False
    at.import_asset_tasks([task])
    t = unreal.load_asset(DEST + '/' + name)
    t.set_editor_property('srgb', srgb)
    t.set_editor_property('compression_settings', comp)
    eal.save_loaded_asset(t, only_if_is_dirty=False)
    return t

D = tex('T_WoodFloor_D', True, unreal.TextureCompressionSettings.TC_DEFAULT)
N = tex('T_WoodFloor_N', False, unreal.TextureCompressionSettings.TC_NORMALMAP)
R = tex('T_WoodFloor_R', False, unreal.TextureCompressionSettings.TC_GRAYSCALE)
DN = tex('T_WoodDetail_N', False, unreal.TextureCompressionSettings.TC_NORMALMAP)

path = DEST + '/M_WoodFloor'
if eal.does_asset_exist(path):
    m = unreal.load_asset(path)          # rebuilt in place (the floors keep using it)
    mel.delete_all_material_expressions(m)
else:
    m = at.create_asset('M_WoodFloor', DEST, unreal.Material, unreal.MaterialFactoryNew())
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)

# the planks by world position: across the planks along world Y, along them world X (the building's long way);
# one tile is three 18 cm planks by 70 cm
wp = E(unreal.MaterialExpressionWorldPosition, -1400, 0)
mx = E(unreal.MaterialExpressionComponentMask, -1200, -60); mx.set_editor_property('r', True); mx.set_editor_property('g', False); mx.set_editor_property('b', False); mx.set_editor_property('a', False)
my = E(unreal.MaterialExpressionComponentMask, -1200, 60); my.set_editor_property('r', False); my.set_editor_property('g', True); my.set_editor_property('b', False); my.set_editor_property('a', False)
mel.connect_material_expressions(wp, '', mx, '')
mel.connect_material_expressions(wp, '', my, '')
sx = E(unreal.MaterialExpressionScalarParameter, -1200, -160); sx.set_editor_property('parameter_name', 'TileAlong'); sx.set_editor_property('default_value', 70.0)
sy = E(unreal.MaterialExpressionScalarParameter, -1200, 160); sy.set_editor_property('parameter_name', 'TileAcross'); sy.set_editor_property('default_value', 54.0)
dx = E(unreal.MaterialExpressionDivide, -1000, -60); mel.connect_material_expressions(mx, '', dx, 'A'); mel.connect_material_expressions(sx, '', dx, 'B')
dy = E(unreal.MaterialExpressionDivide, -1000, 60); mel.connect_material_expressions(my, '', dy, 'A'); mel.connect_material_expressions(sy, '', dy, 'B')
uv = E(unreal.MaterialExpressionAppendVector, -850, 0)
mel.connect_material_expressions(dy, '', uv, 'A')
mel.connect_material_expressions(dx, '', uv, 'B')

def sample(t, x, y, kind):
    s = E(unreal.MaterialExpressionTextureSample, x, y)
    s.set_editor_property('texture', t)
    s.set_editor_property('sampler_type', kind)
    return s

sd = sample(D, -600, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
sn = sample(N, -600, 0, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
sr = sample(R, -600, 300, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
for s in (sd, sn, sr):
    mel.connect_material_expressions(uv, '', s, 'UVs')
# the fine detail (pores, fibres), seven times smaller, added to the planks' normal
k7 = E(unreal.MaterialExpressionMultiply, -800, 150); k7.set_editor_property('const_b', 7.0)
mel.connect_material_expressions(uv, '', k7, 'A')
sdn = sample(DN, -600, 150, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
mel.connect_material_expressions(k7, '', sdn, 'UVs')
half = E(unreal.MaterialExpressionMultiply, -400, 150)
c3 = E(unreal.MaterialExpressionConstant3Vector, -600, 260); c3.set_editor_property('constant', unreal.LinearColor(1.3, 1.3, 0.0, 0.0))
mel.connect_material_expressions(sdn, '', half, 'A'); mel.connect_material_expressions(c3, '', half, 'B')
# the same fibres a second time, larger (two and a half times the pores' size), turned a little so they never line up
k2 = E(unreal.MaterialExpressionMultiply, -800, 330); k2.set_editor_property('const_b', 2.7)
mel.connect_material_expressions(uv, '', k2, 'A')
sdn2 = sample(DN, -600, 400, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
mel.connect_material_expressions(k2, '', sdn2, 'UVs')
half2 = E(unreal.MaterialExpressionMultiply, -400, 330)
c32 = E(unreal.MaterialExpressionConstant3Vector, -600, 500); c32.set_editor_property('constant', unreal.LinearColor(0.9, 0.9, 0.0, 0.0))
mel.connect_material_expressions(sdn2, '', half2, 'A'); mel.connect_material_expressions(c32, '', half2, 'B')
nsum0 = E(unreal.MaterialExpressionAdd, -320, 50)
mel.connect_material_expressions(sn, '', nsum0, 'A'); mel.connect_material_expressions(half, '', nsum0, 'B')
nsum = E(unreal.MaterialExpressionAdd, -250, 50)
mel.connect_material_expressions(nsum0, '', nsum, 'A'); mel.connect_material_expressions(half2, '', nsum, 'B')
nn = E(unreal.MaterialExpressionNormalize, -120, 50)
mel.connect_material_expressions(nsum, '', nn, '')

# only the faces that look up are wood; any other face of the same mesh (an eatery's tiled wall) a plain tile colour
vn = E(unreal.MaterialExpressionVertexNormalWS, -600, -520)
vz = E(unreal.MaterialExpressionComponentMask, -450, -520); vz.set_editor_property('r', False); vz.set_editor_property('g', False); vz.set_editor_property('b', True); vz.set_editor_property('a', False)
mel.connect_material_expressions(vn, '', vz, '')
# (either way up: the imported model's instances are mirrored, which turns the normal over)
vabs = E(unreal.MaterialExpressionAbs, -380, -520)
mel.connect_material_expressions(vz, '', vabs, '')
up = E(unreal.MaterialExpressionSmoothStep, -300, -520)
mel.connect_material_expressions(vabs, '', up, 'Value')
up.set_editor_property('const_min', 0.6); up.set_editor_property('const_max', 0.8)
side = E(unreal.MaterialExpressionVectorParameter, -450, -420); side.set_editor_property('parameter_name', 'SideColor'); side.set_editor_property('default_value', unreal.LinearColor(0.05, 0.05, 0.055, 1))
# a touch of colour control (the owner may want it lighter or darker later)
tint = E(unreal.MaterialExpressionScalarParameter, -450, -300); tint.set_editor_property('parameter_name', 'Brightness'); tint.set_editor_property('default_value', 1.0)
bc = E(unreal.MaterialExpressionMultiply, -300, -300)
mel.connect_material_expressions(sd, 'RGB', bc, 'A'); mel.connect_material_expressions(tint, '', bc, 'B')
lerp = E(unreal.MaterialExpressionLinearInterpolate, -120, -400)
side3 = E(unreal.MaterialExpressionComponentMask, -300, -420); side3.set_editor_property('r', True); side3.set_editor_property('g', True); side3.set_editor_property('b', True); side3.set_editor_property('a', False)
mel.connect_material_expressions(side, '', side3, '')
mel.connect_material_expressions(side3, '', lerp, 'A'); mel.connect_material_expressions(bc, '', lerp, 'B'); mel.connect_material_expressions(up, '', lerp, 'Alpha')
mel.connect_material_property(lerp, '', unreal.MaterialProperty.MP_BASE_COLOR)
# never the floor's pattern on a wall: any face that isn't the floor (looking up) gets a flat normal and a plain satin
# finish (owner, 2026-11-09: 'the wall should not have the same stuff as the floor, ever')
flat = E(unreal.MaterialExpressionConstant3Vector, -120, 160); flat.set_editor_property('constant', unreal.LinearColor(0, 0, 1, 0))
nup = E(unreal.MaterialExpressionLinearInterpolate, 0, 80)
mel.connect_material_expressions(flat, '', nup, 'A'); mel.connect_material_expressions(nn, '', nup, 'B'); mel.connect_material_expressions(up, '', nup, 'Alpha')
mel.connect_material_property(nup, '', unreal.MaterialProperty.MP_NORMAL)
# matte: roughness 0.78 to 0.9 (the map's grain and gaps still vary it), a low specular: no mirror image of anything
rs = E(unreal.MaterialExpressionMultiply, -300, 300); rs.set_editor_property('const_b', 0.35)
mel.connect_material_expressions(sr, 'R', rs, 'A')
ra = E(unreal.MaterialExpressionAdd, -180, 300); ra.set_editor_property('const_b', 0.6)
mel.connect_material_expressions(rs, '', ra, 'A')
rside = E(unreal.MaterialExpressionConstant, -120, 300); rside.set_editor_property('r', 0.6)
rup = E(unreal.MaterialExpressionLinearInterpolate, 0, 300)
mel.connect_material_expressions(rside, '', rup, 'A'); mel.connect_material_expressions(ra, '', rup, 'B'); mel.connect_material_expressions(up, '', rup, 'Alpha')
mel.connect_material_property(rup, '', unreal.MaterialProperty.MP_ROUGHNESS)
spec = E(unreal.MaterialExpressionConstant, -120, 380); spec.set_editor_property('r', 0.25)
mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)

# every second-floor floor (by the mesh that holds it)
FLOORS = [758, 776, 886, 768,                           # link, landing, long walkway, food court (terrazzo)
          925, 944, 963, 982, 933, 952, 971, 990,       # gate rooms 1-4 and their view areas (blue carpet)
          819, 804, 786,                                # bar (herringbone), family room (carpet), tax-free (tiles)
          892, 898, 904, 910, 916]                      # the eateries 1-5 (dark tiles)
done = 0
for i in FLOORS:
    p = '/Game/Babylon/Terminal/terminal/StaticMeshes/terminal_mesh_%d' % i
    if not eal.does_asset_exist(p):
        unreal.log_warning('ADP_WOOD: no mesh %d' % i); continue
    sm = unreal.load_asset(p)
    for k in range(len(sm.get_editor_property('static_materials'))):
        sm.set_material(k, m)
    sm.modify()
    eal.save_loaded_asset(sm, only_if_is_dirty=False)
    if sm.get_material(0) == m: done += 1
    else: unreal.log_warning('ADP_WOOD: mesh %d kept %s' % (i, sm.get_material(0).get_name()))
unreal.log('ADP_WOOD: wood floor on %d second-floor floors' % done)
unreal.SystemLibrary.quit_editor()
