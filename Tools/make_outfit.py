# A garment from Blender into the MetaHuman wardrobe (2026-11-13). For a garment made on the player's body
# (Art/Clothes/<Name>/<Name>.fbx, a static mesh in the body's own space):
#   1. the mesh imported as /Game/Clothes/<Name>/SM_<Name>
#   2. a cloth dataflow DF_<Name>: import the mesh (render only: fitted clothes are skinned, not simulated), skin it from the body it was made on, terminal
#      -> cloth asset CA_<Name>
#   3. an outfit OA_<Name> made by Epic's resizable-outfit template (a copy kept at /Game/Clothes/Common) from one
#      size: CA_<Name> on that body; the MetaHuman pipeline resizes it to anybody who wears it
#   4. the wardrobe item WI_<Name> (Epic's default garment's item, copied), for the Outfits slot: pointing at CA_<Name>
#      (made on the player's body, no resizing), or at OA_<Name> with 'resize' (other bodies; not working yet)
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/make_outfit.py <Name>" -unattended -RenderOffscreen
import os, sys
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
NAME = [a for a in sys.argv[1:] if not a.startswith('-')][0] if len(sys.argv) > 1 else 'TestShirt'
DIR = '/Game/Clothes/' + NAME
COMMON = '/Game/Clothes/Common'
BODY = '/Game/People/Built/MH_900_player/Body/SKM_MH_900_player_BodyMesh'
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
DFL = unreal.DataflowEditorBlueprintLibrary
T = unreal.ADPClothTools
def log(m): unreal.log('ADP_OUTFIT ' + str(m))

def fresh(path):
    if eal.does_asset_exist(path): eal.delete_asset(path)

try:
    # 1. the mesh
    fbx = os.path.join(ROOT, 'Art', 'Clothes', NAME, NAME + '.fbx')
    t = unreal.AssetImportTask(); t.filename = fbx; t.destination_path = DIR; t.destination_name = 'SM_' + NAME
    t.automated = True; t.replace_existing = True; t.save = True
    o = unreal.FbxImportUI(); o.import_mesh = True; o.import_as_skeletal = False; o.import_materials = True; o.import_textures = False
    o.static_mesh_import_data.combine_meshes = True; o.static_mesh_import_data.generate_lightmap_u_vs = False
    o.static_mesh_import_data.auto_generate_collision = False
    t.options = o
    tools.import_asset_tasks([t])
    sm = unreal.load_asset(DIR + '/SM_' + NAME)
    log('mesh %s bounds %s' % (sm, sm.get_bounds() if sm else None))
    body = unreal.load_asset(BODY)

    # 2. the cloth dataflow and the cloth asset
    fresh(DIR + '/DF_' + NAME)
    df = tools.create_asset('DF_' + NAME, DIR, unreal.Dataflow, unreal.DataflowAssetFactory())
    imp = DFL.add_dataflow_node(df, 'FChaosClothAssetStaticMeshImportNode_v2', 'Import', unreal.Vector2D(0, 0))
    skin = DFL.add_dataflow_node(df, 'FChaosClothAssetTransferSkinWeightsNode', 'Skin', unreal.Vector2D(300, 0))
    term = DFL.add_dataflow_node(df, 'FChaosClothAssetTerminalNode_v2', 'ClothTerminal', unreal.Vector2D(600, 0))
    log('nodes %s %s %s' % (imp, skin, term))
    log('set mesh %s' % DFL.set_dataflow_node_property(df, imp, 'StaticMesh', sm.get_path_name()))
    log('set body %s' % DFL.set_dataflow_node_property(df, skin, 'SkeletalMesh', body.get_path_name()))
    # fitted clothes are skinned only, not simulated (a simulated shirt with nothing pinning it flew apart): the render
    # mesh alone, its weights from the body itself
    log('no sim %s' % DFL.set_dataflow_node_property(df, imp, 'bImportSimMesh', 'False'))
    log('render only %s' % DFL.set_dataflow_node_property(df, skin, 'TargetMeshType', 'Render'))
    log('weights from body %s' % DFL.set_dataflow_node_property(df, skin, 'RenderMeshSourceType', 'SkeletalMesh'))
    log('wire imp->skin %s' % DFL.connect_dataflow_nodes(df, imp, 'Collection', skin, 'Collection'))
    ok = False
    for pin in ('CollectionLods[0]', 'Collection LOD 0', 'CollectionLod0', 'Collection'):
        if DFL.connect_dataflow_nodes(df, skin, 'Collection', term, pin):
            log('wire skin->terminal via %s' % pin); ok = True; break
    if not ok: log('could not wire the terminal')
    eal.save_loaded_asset(df, only_if_is_dirty=False)

    fresh(DIR + '/CA_' + NAME)
    ca = tools.create_asset('CA_' + NAME, DIR, unreal.ChaosClothAsset, unreal.ChaosClothAssetFactory())
    log('cloth dataflow %s' % T.set_dataflow(ca, df, term))
    log('cloth made %s' % T.regenerate(ca))
    log('cloth %s' % T.describe_dataflow(ca))
    eal.save_loaded_asset(ca, only_if_is_dirty=False)

    # 3. the outfit
    tmpl = COMMON + '/DF_MakeResizableOutfit'
    if not eal.does_asset_exist(tmpl):
        eal.duplicate_asset('/ChaosOutfitAsset/MakeResizableOutfitTemplate', tmpl)
    odf = unreal.load_asset(tmpl)
    fresh(DIR + '/OA_' + NAME)
    oa = tools.create_asset('OA_' + NAME, DIR, unreal.ChaosOutfitAsset, unreal.ChaosOutfitAssetFactory())
    log('outfit dataflow %s' % T.set_dataflow(oa, odf, 'OutfitAssetTerminal'))
    log('outfit vars before %s' % T.describe_dataflow(oa))
    src = unreal.ChaosSizedOutfitSource()
    src.set_editor_property('source_asset', ca)
    src.set_editor_property('source_body_parts', [body])
    src.set_editor_property('size_name', 'player')
    log('outfit sources %s' % T.set_sized_outfit_sources(oa, 'SizedOutfitSource', [src]))
    log('outfit made %s' % T.regenerate(oa))
    log('outfit vars after %s' % T.describe_dataflow(oa))
    eal.save_loaded_asset(oa, only_if_is_dirty=False)

    # 4. the wardrobe item
    fresh(DIR + '/WI_' + NAME)
    wi = eal.duplicate_asset('/MetaHumanCharacter/Optional/Clothing/WI_DefaultGarment', DIR + '/WI_' + NAME)
    # the player's own clothes are tailored on the player's body and need no resizing: the item wears the cloth asset
    # itself (tested 2026-11-13: clean). The resizable outfit (OA_) is made too, for the crew's uniforms on other
    # bodies later, but Epic's resizing still throws this garment apart: to fix before the uniform refit pass.
    RESIZE = '-resize' in sys.argv or any(a == 'resize' for a in sys.argv)
    wi.set_editor_property('principal_asset', oa if RESIZE else ca)
    eal.save_loaded_asset(wi, only_if_is_dirty=False)
    log('item %s -> %s' % (wi.get_path_name(), wi.get_editor_property('principal_asset')))
    # how Epic's own outfit is set up, to compare
    log('epic outfit %s' % T.describe_dataflow(unreal.load_asset('/MetaHumanCharacter/Optional/Clothing/OA_DefaultGarment')))
    log('DONE')
except Exception as e:
    unreal.log_error('ADP_OUTFIT failed: %s' % e)
unreal.SystemLibrary.quit_editor()
