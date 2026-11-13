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
def sampler(name, x, y, stype):
    s = E(unreal.MaterialExpressionTextureSampleParameter2D, x, y); s.set_editor_property('parameter_name', name); s.set_editor_property('sampler_type', stype)
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
    eal.save_loaded_asset(mi, only_if_is_dirty=False)
    log('instance ' + mi.get_path_name())
log('DONE')
unreal.SystemLibrary.quit_editor()
