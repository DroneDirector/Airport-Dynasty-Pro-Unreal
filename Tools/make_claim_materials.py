# The arrivals wing's two new terminal finishes (to-do 72, owner 2026-11-16), instances of the terminal's own surface
# master (/Game/Terminal/Masters/M_TSurface, Tools/set_terminal_materials.py) so they behave as every other terminal
# surface; the pictures are CC0 scans from Poly Haven at 2K (Art/BaggageClaim/textures):
#   MI_T_cedar   vertical Japanese cedar planks (the escalators' void, west wall), one tile 1.13 m, a touch deeper
#   MI_T_basalt  dark stacked stone (the east wall), "Japanese Stone Wall" darkened to a basalt grey, one tile 1.9 m
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_claim_materials.py" -unattended -nullrhi
import os, glob, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools(); MEL = unreal.MaterialEditingLibrary
T = '/Game/Terminal'; TX = os.path.join(ROOT, 'Art', 'BaggageClaim', 'textures')
def log(*a): unreal.log('ADP_CMAT ' + ' '.join(str(x) for x in a))
for d in ('Materials', 'Textures'):
    for f in glob.glob(os.path.join(ROOT, 'Content', 'Terminal', d, '*.uasset')): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
def tex(path, name, normal):
    dst = T + '/Textures/' + name
    t = unreal.AssetImportTask(); t.filename = path; t.destination_path = T + '/Textures'; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    AT.import_asset_tasks([t]); a = unreal.load_asset(dst)
    if normal:
        a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); a.set_editor_property('srgb', False)
    a.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_loaded_asset(a, only_if_is_dirty=False); return a
def mi(name, base, nrm, bc, tile, rough, ns):
    path = T + '/Materials/' + name
    m = unreal.load_asset(path) or AT.create_asset(name, T + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(m, unreal.load_asset(T + '/Masters/M_TSurface'))
    MEL.set_material_instance_vector_parameter_value(m, 'BC', unreal.LinearColor(*bc))
    MEL.set_material_instance_vector_parameter_value(m, 'OS', unreal.LinearColor(0, 0, 1.0 / tile, 1.0 / tile))
    MEL.set_material_instance_scalar_parameter_value(m, 'R', rough); MEL.set_material_instance_scalar_parameter_value(m, 'M', 0.0)
    MEL.set_material_instance_scalar_parameter_value(m, 'NS', ns)
    vals = []
    for n_, t_ in (('Base', base), ('Nrm', nrm)):
        v = unreal.TextureParameterValue(); info = unreal.MaterialParameterInfo(); info.set_editor_property('name', n_)
        v.set_editor_property('parameter_info', info); v.set_editor_property('parameter_value', t_); vals.append(v)
    m.set_editor_property('texture_parameter_values', vals)
    MEL.update_material_instance(m); EAL.save_loaded_asset(m, only_if_is_dirty=False); log(name, 'made')
c = os.path.join(TX, 'japanese_cedar_planks_2k'); s = os.path.join(TX, 'japanese_stone_wall')
mi('MI_T_cedar', tex(os.path.join(c, 'japanese_cedar_planks_diff_2k.jpg'), 'T_cedar_diff', False), tex(os.path.join(c, 'japanese_cedar_planks_nor_dx_2k.jpg'), 'T_cedar_nrm', True),
   (0.72, 0.68, 0.66, 1), 1.13, 0.55, 1.0)
mi('MI_T_basalt', tex(os.path.join(s, 'japanese_stone_wall_diff_2k.jpg'), 'T_basaltwall_diff', False), tex(os.path.join(s, 'japanese_stone_wall_nor_dx_2k.jpg'), 'T_basaltwall_nrm', True),
   (0.32, 0.32, 0.34, 1), 1.9, 0.6, 1.2)
unreal.SystemLibrary.quit_editor()
