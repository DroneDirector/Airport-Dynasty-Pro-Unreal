# A pet into the game (after Art/Pets/build_pet.sh and export_pet.py): /Game/Pets/<Dogs|Cats>/<Name>/ gets
#   SK_<Name> (skinned mesh + skeleton), A_<Name>_<Move> for every move, T_<Name>_D / T_<Name>_N (colour, surface
#   detail), MI_<Name> on the fur and MI_<Name>_Mouth on the mouth's inside (colours from the mesh, two-sided).
# Nothing else to register: the game finds every SK_ under /Game/Pets/Dogs and /Game/Pets/Cats by itself (the creator's
# dogs and cats sections) and /Game/Pets is always cooked.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_pet.py Dogs GermanShepherd" -unattended -nullrhi
import os, sys
import unreal

KIND, NAME = sys.argv[-2], sys.argv[-1]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Art', 'Pets', 'Models', KIND, NAME)
DEST = '/Game/Pets/%s/%s' % (KIND, NAME)
MATS = '/Game/Pets/Materials'
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
def log(*a): unreal.log('ADP_PET ' + ' '.join(str(x) for x in a))

if EAL.does_directory_exist(DEST):
    EAL.delete_directory(DEST)

def task(path, opts=None, dest=DEST, name=None):
    t = unreal.AssetImportTask()
    t.filename = path; t.destination_path = dest; t.automated = True; t.replace_existing = True; t.save = True
    if name: t.destination_name = name
    if opts: t.options = opts
    AT.import_asset_tasks([t])
    return list(t.imported_object_paths)

# --- the textures
tex = {}
for k in ('D', 'N'):
    got = task(os.path.join(SRC, 'T_%s_%s.png' % (NAME, k)))
    t = unreal.load_asset(got[0]); tex[k] = t
    if k == 'N':
        t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property('srgb', False)
    EAL.save_loaded_asset(t)

# --- the two parent materials (made once for every pet)
def master(name, build):
    path = '%s/%s' % (MATS, name)
    if EAL.does_asset_exist(path): return unreal.load_asset(path)
    m = AT.create_asset(name, MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_skeletal_mesh', True)
    build(m); MEL.recompile_material(m); EAL.save_loaded_asset(m)
    return m
def fur(m):
    c = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    c.set_editor_property('parameter_name', 'Color')
    # (a texture parameter needs a default texture, or the material fails to compile and every pet shows untextured)
    c.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
    n = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, 300)
    n.set_editor_property('parameter_name', 'Normal'); n.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    n.set_editor_property('texture', unreal.load_asset('/Engine/EngineMaterials/DefaultNormal'))
    r = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -300, 200)
    r.set_editor_property('parameter_name', 'Roughness'); r.set_editor_property('default_value', 0.82)
    MEL.connect_material_property(c, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(n, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
def mouth(m):
    m.set_editor_property('two_sided', True)
    v = MEL.create_material_expression(m, unreal.MaterialExpressionVertexColor, -400, 0)
    r = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 200); r.set_editor_property('r', 0.35)
    MEL.connect_material_property(v, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
M_FUR = master('M_PetFur', fur); M_MOUTH = master('M_PetMouth', mouth)
mi = AT.create_asset('MI_%s' % NAME, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
MEL.set_material_instance_parent(mi, M_FUR)
MEL.set_material_instance_texture_parameter_value(mi, 'Color', tex['D'])
MEL.set_material_instance_texture_parameter_value(mi, 'Normal', tex['N'])
EAL.save_loaded_asset(mi)
mim = AT.create_asset('MI_%s_Mouth' % NAME, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
MEL.set_material_instance_parent(mim, M_MOUTH); EAL.save_loaded_asset(mim)

# --- the mesh, skeleton and moves
o = unreal.FbxImportUI()
o.set_editor_property('import_mesh', True); o.set_editor_property('import_as_skeletal', True)
o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
o.set_editor_property('import_animations', True); o.set_editor_property('import_materials', False)
o.set_editor_property('import_textures', False); o.set_editor_property('create_physics_asset', True)
sd = o.get_editor_property('skeletal_mesh_import_data')
sd.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
sd.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
sd.set_editor_property('import_morph_targets', False)
ad = o.get_editor_property('anim_sequence_import_data')
ad.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
got = task(os.path.join(SRC, 'SK_%s.fbx' % NAME), o)
log('imported', got)
sk = None
for p in EAL.list_assets(DEST, recursive=False):
    a = unreal.load_asset(p)
    if isinstance(a, unreal.SkeletalMesh): sk = a
    elif isinstance(a, unreal.AnimSequence):
        move = a.get_name().split('_')[-1]
        new = '%s/A_%s_%s' % (DEST, NAME, move)
        if p.split('.')[0] != new: EAL.rename_asset(p.split('.')[0], new)
assert sk, 'no skeletal mesh came in'
mats = []
for sm in sk.get_editor_property('materials'):
    slot = str(sm.get_editor_property('material_slot_name'))
    sm.set_editor_property('material_interface', mim if slot.endswith('Mouth') else mi)
    mats.append(sm)
sk.set_editor_property('materials', mats)
EAL.save_loaded_asset(sk)
b = sk.get_bounds()
log('mesh', sk.get_path_name(), 'slots', [str(m.get_editor_property('material_slot_name')) for m in mats],
    'bounds extent', b.box_extent, 'origin', b.origin)
log('moves', sorted(x.split('/')[-1].split('.')[0] for x in EAL.list_assets(DEST) if '/A_' in x))
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log('done', NAME)
