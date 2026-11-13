# The fabric library in Unreal (2026-11-13): Art/Fabrics/<name>/{albedo,normal,roughness}.png (Tools/make_fabrics.py)
# imported as /Game/Clothes/Fabrics/<name>/T_*, one master material M_Fabric (the weave tiled over the garment's UVs,
# tinted by Colour, its relief and roughness) and an instance MI_Fabric_<name> per fabric. Garments name their
# material slots Fabric_<name> (Blender) and Tools/make_outfit.py puts MI_Fabric_<name> in them.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_fabrics.py" -unattended -nullrhi
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(ROOT, 'Art', 'Fabrics')
DIR = '/Game/Clothes/Fabrics'
eal = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_FABRIC ' + str(m))

# the fabric -> how many times its tile repeats across the garment's UVs, and the colour it is made in by default
# (garments may use a fabric under another name: cotton_shirt is the jersey's weave, white)
FABRICS = {   # tiles per metre of cloth (garments' UVs are in metres: Art/Clothes/*/make_*.py)
    'wool_suiting': ('wool_suiting', 11.0), 'cotton_shirt': ('cotton_jersey', 22.0), 'cotton_jersey': ('cotton_jersey', 22.0),
    'satin': ('satin', 22.0), 'leather': ('leather', 9.0), 'denim': ('denim', 11.0), 'fleece': ('fleece', 11.0),
    'knit': ('knit', 10.0), 'nylon_shell': ('nylon_shell', 14.0), 'hivis_polyester': ('hivis_polyester', 14.0), 'canvas': ('canvas', 10.0),
}

def texture(folder, kind):
    path = '%s/%s/T_%s_%s' % (DIR, folder, folder, kind)
    if not eal.does_asset_exist(path):
        t = unreal.AssetImportTask(); t.filename = os.path.join(ART, folder, kind + '.png')
        t.destination_path = DIR + '/' + folder; t.destination_name = 'T_%s_%s' % (folder, kind)
        t.automated = True; t.replace_existing = True; t.save = False
        tools.import_asset_tasks([t])
    a = unreal.load_asset(path)
    if kind == 'normal':
        a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); a.set_editor_property('srgb', False)
    elif kind == 'roughness':
        a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS); a.set_editor_property('srgb', False)
    eal.save_loaded_asset(a, only_if_is_dirty=False)
    return a

# the master
MP = DIR + '/M_Fabric'
if eal.does_asset_exist(MP): eal.delete_asset(MP)
m = tools.create_asset('M_Fabric', DIR, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('two_sided', True)
# garments are skeletal meshes made by the cloth pipeline: without these the game draws Unreal's grey default instead
for flag in ('used_with_skeletal_mesh', 'used_with_clothing', 'used_with_morph_targets'):
    try: m.set_editor_property(flag, True)
    except Exception as e: log('no flag %s: %s' % (flag, e))
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
uv = E(unreal.MaterialExpressionTextureCoordinate, -1100, 0)
tiling = E(unreal.MaterialExpressionScalarParameter, -1100, 150); tiling.set_editor_property('parameter_name', 'Tiling'); tiling.set_editor_property('default_value', 20.0)
mul = E(unreal.MaterialExpressionMultiply, -900, 50); mel.connect_material_expressions(uv, '', mul, 'A'); mel.connect_material_expressions(tiling, '', mul, 'B')
# the master's samplers need default pictures of their own kind (an empty one gets the engine's colour default, which
# fails to compile in a normal-map or mask sampler: the game then drew Unreal's grey default on every garment)
DEFAULT = {'Albedo': texture('wool_suiting', 'albedo'), 'Normal': texture('wool_suiting', 'normal'), 'Roughness': texture('wool_suiting', 'roughness')}
def sampler(name, x, y, stype):
    s = E(unreal.MaterialExpressionTextureSampleParameter2D, x, y); s.set_editor_property('parameter_name', name); s.set_editor_property('texture', DEFAULT[name]); s.set_editor_property('sampler_type', stype)
    mel.connect_material_expressions(mul, '', s, 'UVs'); return s
alb = sampler('Albedo', -650, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
nrm = sampler('Normal', -650, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
rgh = sampler('Roughness', -650, 400, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
col = E(unreal.MaterialExpressionVectorParameter, -650, -400); col.set_editor_property('parameter_name', 'Colour'); col.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
tint = E(unreal.MaterialExpressionMultiply, -350, -300); mel.connect_material_expressions(alb, 'RGB', tint, 'A'); mel.connect_material_expressions(col, '', tint, 'B')
rs = E(unreal.MaterialExpressionScalarParameter, -650, 650); rs.set_editor_property('parameter_name', 'RoughnessScale'); rs.set_editor_property('default_value', 1.0)
rmul = E(unreal.MaterialExpressionMultiply, -350, 450); mel.connect_material_expressions(rgh, 'R', rmul, 'A'); mel.connect_material_expressions(rs, '', rmul, 'B')
mel.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(nrm, 'RGB', unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(rmul, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
log('master ' + m.get_path_name())

# the print master (owner: a print keeps its exact colours and shows the same knit as the shirt): the fabric's own
# weave (relief and roughness, tiled per metre on UV1... here the print UVs carry the picture) under a printed picture -
# the picture's colours unchanged where its ink is, the fabric's colour round it; the knit's relief runs through both
PP = DIR + '/M_FabricPrint'
if eal.does_asset_exist(PP): eal.delete_asset(PP)
pmat = tools.create_asset('M_FabricPrint', DIR, unreal.Material, unreal.MaterialFactoryNew())
pmat.set_editor_property('two_sided', True)
for flag in ('used_with_skeletal_mesh', 'used_with_clothing', 'used_with_morph_targets'):
    try: pmat.set_editor_property(flag, True)
    except Exception as e: log('no flag %s: %s' % (flag, e))
PE = lambda cls, x, y: mel.create_material_expression(pmat, cls, x, y)
puv = PE(unreal.MaterialExpressionTextureCoordinate, -1300, 0)
ptile = PE(unreal.MaterialExpressionScalarParameter, -1300, 150); ptile.set_editor_property('parameter_name', 'Tiling'); ptile.set_editor_property('default_value', 6.6)   # the cotton's 22 tiles a metre over the print's 30 cm
pmul = PE(unreal.MaterialExpressionMultiply, -1100, 50); mel.connect_material_expressions(puv, '', pmul, 'A'); mel.connect_material_expressions(ptile, '', pmul, 'B')
def psamp(name, x, y, stype, uvsrc, tex):
    s_ = PE(unreal.MaterialExpressionTextureSampleParameter2D, x, y); s_.set_editor_property('parameter_name', name); s_.set_editor_property('texture', tex); s_.set_editor_property('sampler_type', stype)
    mel.connect_material_expressions(uvsrc, '', s_, 'UVs'); return s_
palb = psamp('Albedo', -850, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, pmul, texture('cotton_jersey', 'albedo'))
pnrm = psamp('Normal', -850, 100, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, pmul, texture('cotton_jersey', 'normal'))
prgh = psamp('Roughness', -850, 400, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, pmul, texture('cotton_jersey', 'roughness'))
pprint = psamp('Print', -850, -650, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, puv, texture('cotton_jersey', 'albedo'))
pcol = PE(unreal.MaterialExpressionVectorParameter, -850, -850); pcol.set_editor_property('parameter_name', 'Colour'); pcol.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
ptint = PE(unreal.MaterialExpressionMultiply, -550, -350); mel.connect_material_expressions(palb, 'RGB', ptint, 'A'); mel.connect_material_expressions(pcol, '', ptint, 'B')
# the knit's light and shade (albedo over its average) also falls on the ink, a little
pshade = PE(unreal.MaterialExpressionMultiply, -550, -650); mel.connect_material_expressions(pprint, 'RGB', pshade, 'A')
pk = PE(unreal.MaterialExpressionLinearInterpolate, -700, -500); c1 = PE(unreal.MaterialExpressionConstant, -850, -480); c1.set_editor_property('r', 1.0)
mel.connect_material_expressions(c1, '', pk, 'A'); mel.connect_material_expressions(palb, 'R', pk, 'B')
ck = PE(unreal.MaterialExpressionConstant, -850, -440); ck.set_editor_property('r', 0.35); mel.connect_material_expressions(ck, '', pk, 'Alpha')
mel.connect_material_expressions(pk, '', pshade, 'B')
plerp = PE(unreal.MaterialExpressionLinearInterpolate, -300, -400)
mel.connect_material_expressions(ptint, '', plerp, 'A'); mel.connect_material_expressions(pshade, '', plerp, 'B'); mel.connect_material_expressions(pprint, 'A', plerp, 'Alpha')
mel.connect_material_property(plerp, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(pnrm, 'RGB', unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(prgh, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(pmat); eal.save_loaded_asset(pmat, only_if_is_dirty=False)
log('print master ' + pmat.get_path_name())

for name, (folder, tile) in FABRICS.items():
    if not os.path.isdir(os.path.join(ART, folder)): log('no pictures for ' + folder); continue
    a, n, r = texture(folder, 'albedo'), texture(folder, 'normal'), texture(folder, 'roughness')
    ip = '%s/MI_Fabric_%s' % (DIR, name)
    if eal.does_asset_exist(ip): eal.delete_asset(ip)
    mi = tools.create_asset('MI_Fabric_' + name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, m)
    mel.set_material_instance_texture_parameter_value(mi, 'Albedo', a)
    mel.set_material_instance_texture_parameter_value(mi, 'Normal', n)
    mel.set_material_instance_texture_parameter_value(mi, 'Roughness', r)
    mel.set_material_instance_scalar_parameter_value(mi, 'Tiling', tile)
    if name == 'leather': mel.set_material_instance_scalar_parameter_value(mi, 'RoughnessScale', 0.55)   # polished shoes
    eal.save_loaded_asset(mi, only_if_is_dirty=False)
    log('instance ' + mi.get_path_name())
log('DONE')
unreal.SystemLibrary.quit_editor()
