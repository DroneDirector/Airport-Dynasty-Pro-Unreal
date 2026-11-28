# The house livery "Airport Dynasty Pro" on the A320 as glossy airliner paint (owner 2026-11-28: livery approved, "give it
# some reflection like a real fuselage"). Makes NEW assets only; the plain white material instances and textures of the
# import (/Game/Babylon/A320/A320/...) stay untouched and are the backup.
#   /Game/Babylon/A320/Livery/T_A320_Livery_ADP_Fuse / _Engine       Art/A320/Livery/ADP-*.png: sRGB, mips, BC7, no LOD bias
#   /Game/Babylon/A320/Livery/T_A320_Livery_ADP_Fuse_MR / _Engine_MR  Art/A320/Livery/ADP-*-MR.png (make_livery_mr.py):
#                                                                     linear mask, G roughness 0.30, B metallic (gold 0.40)
#   /Game/Babylon/A320/Livery/MI_A320_Livery_ADP_Fuse / _Engine       on MI_ClearCoat_Opaque_DS (two-sided glTF clear-coat
#                                                                     paint): clear coat 1.0, clear-coat roughness 0.08, IOR
#                                                                     1.5 (F0 4 %: mild Fresnel, the sky and sun at grazing
#                                                                     angles, no mirror); normal/occlusion of the white kept
# The game puts them on every A320 by Data/babylon-a320-livery.json ("livery": "adp" or "white"; ABabylonModel::ApplyLivery).
# import_model.py runs this after the two-sided step for A320, so a re-import keeps it.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_a320_livery.py" -unattended -nullrhi
import os
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
LIV = os.path.join(ROOT, 'Art', 'A320', 'Livery')
DEST = '/Game/Babylon/A320/Livery'
WHITE = '/Game/Babylon/A320/A320/Materials/'
PARENT = '/Game/Babylon/Masters/MI_ClearCoat_Opaque_DS'
# livery MI: (white MI it replaces, colour picture, gloss map)
SETS = {'MI_A320_Livery_ADP_Fuse': ('Fuse-Main_Fuse-Main', 'ADP-Fuse-Main.png', 'ADP-Fuse-Main-MR.png', 'Fuse'),
        'MI_A320_Livery_ADP_Engine': ('engine-CFM56_engine-CFM56', 'ADP-engine-CFM56.png', 'ADP-engine-CFM56-MR.png', 'Engine')}
SCALARS = {'MetallicFactor': 1.0, 'RoughnessFactor': 1.0, 'ClearCoatFactor': 1.0, 'ClearCoatRoughnessFactor': 0.08,
           'IOR': 1.5, 'SpecularFactor': 1.0, 'EmissiveStrength': 1.0}
KEEP_TEX = ('NormalTexture', 'OcclusionTexture')        # the white's panel detail, if it has any
KEEP_SCALAR = ('NormalScale', 'OcclusionStrength')


def writable(asset_path):
    # LFS uassets are read-only after a commit: the import would silently keep the old one
    f = os.path.join(ROOT, 'Content', asset_path[len('/Game/'):] + '.uasset')
    if os.path.exists(f):
        os.chmod(f, 0o666)
    return f


def import_texture(png, name, colour):
    path = '%s/%s' % (DEST, name)
    writable(path)
    t = unreal.AssetImportTask()
    t.set_editor_property('filename', os.path.join(LIV, png))
    t.set_editor_property('destination_path', DEST)
    t.set_editor_property('destination_name', name)
    t.set_editor_property('replace_existing', True)
    t.set_editor_property('automated', True)
    t.set_editor_property('save', False)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    tex = unreal.load_asset(path)
    if tex is None:
        raise RuntimeError('texture not imported: ' + png)
    tex.set_editor_property('srgb', colour)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7 if colour else unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property('compression_quality', unreal.TextureCompressionQuality.TCQ_HIGHEST)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    tex.set_editor_property('lod_bias', 0)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    eal.save_loaded_asset(tex, False)
    return tex


try:
    parent = unreal.load_asset(PARENT)
    if parent is None:
        raise RuntimeError(PARENT + ' missing (run fix_babylon_materials.py first)')
    try:
        o = parent.get_editor_property('base_property_overrides')
        ds = o.get_editor_property('two_sided') if o.get_editor_property('override_two_sided') else parent.get_base_material().get_editor_property('two_sided')
    except Exception as e:
        ds = 'unknown (%s)' % e
    done = []
    for mi_name, (white, png, mr_png, short) in SETS.items():
        w = unreal.load_asset(WHITE + white)
        if w is None:
            raise RuntimeError('white material missing: ' + white)
        col = import_texture(png, 'T_A320_Livery_ADP_' + short, True)
        mr = import_texture(mr_png, 'T_A320_Livery_ADP_%s_MR' % short, False)
        path = '%s/%s' % (DEST, mi_name)
        writable(path)
        mi = unreal.load_asset(path)
        if mi is None:
            mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(mi_name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, parent)
        for n in KEEP_TEX:
            v = mel.get_material_instance_texture_parameter_value(w, n)
            if v: mel.set_material_instance_texture_parameter_value(mi, n, v)
        for n in KEEP_SCALAR:
            mel.set_material_instance_scalar_parameter_value(mi, n, mel.get_material_instance_scalar_parameter_value(w, n))
        mel.set_material_instance_vector_parameter_value(mi, 'BaseColorFactor', mel.get_material_instance_vector_parameter_value(w, 'BaseColorFactor'))
        mel.set_material_instance_texture_parameter_value(mi, 'BaseColorTexture', col)
        mel.set_material_instance_texture_parameter_value(mi, 'MetallicRoughnessTexture', mr)
        for n, v in SCALARS.items():
            mel.set_material_instance_scalar_parameter_value(mi, n, v)
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        got = [round(mel.get_material_instance_scalar_parameter_value(mi, n), 3) for n in ('ClearCoatFactor', 'ClearCoatRoughnessFactor')]
        done.append('%s(cc %s, rough %s, base %s)' % (mi_name, got[0], got[1], col.get_name()))
    unreal.log('ADP_LIVERY: two-sided parent %s; %s' % (ds, ', '.join(done)))
except Exception as e:
    unreal.log_error('ADP_LIVERY: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
