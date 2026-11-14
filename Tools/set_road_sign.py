# The roadside airport sign (Art/RoadSign/make_roadsign.py -> roadsign.glb) into Unreal: the mesh to /Game/RoadSign
# (Nanite on, its own shape as the collision), its face's base picture and the letters (Art/RoadSign/
# make_roadsign_texture.py), the face material M_RoadSignFace (texture parameter "Face": the game gives each sign its own
# picture with the airport's name and distance written on - RoadSigns.cpp), the steel and the plain parts on the light
# mast's own materials. Checks no material slot is left empty.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_road_sign.py" -unattended
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(ROOT, 'Art', 'RoadSign')
DIR, LM = '/Game/RoadSign', '/Game/Materials/LightMast/'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
def log(m): unreal.log('ADP_ROADSIGN ' + m)

if eal.does_directory_exist(DIR): eal.delete_directory(DIR)
def tex(file, name, srgb=True, ui=False):
    t = unreal.AssetImportTask(); t.filename = os.path.join(ART, file); t.destination_path = DIR; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    tools.import_asset_tasks([t]); a = unreal.load_asset(DIR + '/' + name)
    a.set_editor_property('srgb', srgb)
    if ui:   # the letters: read on the CPU side by the canvas, exact (no compression blur), no mips
        a.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        a.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        a.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    eal.save_loaded_asset(a, only_if_is_dirty=False); return a
T_BASE = tex('T_RoadSign_AirportBase.png', 'T_RoadSign_AirportBase', ui=True)
T_GLYPH = tex('T_SignGlyphs.png', 'T_SignGlyphs', ui=True)
log('textures in')

# the face: the picture (a parameter: each sign's own), reflective sheeting - a little glossy
m = tools.create_asset('M_RoadSignFace', DIR, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('used_with_nanite', True); m.set_editor_property('used_with_instanced_static_meshes', True)
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
uv = E(unreal.MaterialExpressionTextureCoordinate, -900, 0)
t = E(unreal.MaterialExpressionTextureSampleParameter2D, -600, 0); t.set_editor_property('parameter_name', 'Face'); t.set_editor_property('texture', T_BASE)
mel.connect_material_expressions(uv, '', t, 'UVs')
mel.connect_material_property(t, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
r = E(unreal.MaterialExpressionConstant, -300, 200); r.set_editor_property('r', 0.38); mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
sp = E(unreal.MaterialExpressionConstant, -300, 280); sp.set_editor_property('r', 0.6); mel.connect_material_property(sp, '', unreal.MaterialProperty.MP_SPECULAR)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
log('face material ready')

manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters(); params.is_automated = True
manager.import_asset(DIR, unreal.InterchangeManager.create_source_data(os.path.join(ART, 'roadsign.glb')), params)
manager.wait_until_all_tasks_done(False)
meshes = [a for a in (unreal.load_asset(p) for p in eal.list_assets(DIR, recursive=True)) if isinstance(a, unreal.StaticMesh)]
if not meshes: raise RuntimeError('no mesh imported')
SLOTS = {'SignFace': DIR + '/M_RoadSignFace', 'Galvanised': LM + 'M_LM_Galvanised', 'Aluminium': LM + 'MI_LM_Aluminium',
         'Concrete': LM + 'MI_LM_Concrete', 'Bolt': LM + 'MI_LM_Bolt', 'DarkMetal': LM + 'MI_LM_DarkMetal'}
for mesh in meshes:
    for i, s in enumerate(mesh.get_editor_property('static_materials')):
        slot = str(s.get_editor_property('material_slot_name'))
        key = next((k for k in SLOTS if k.lower() in slot.lower()), None)
        if key: mesh.set_material(i, unreal.load_asset(SLOTS[key]))
        log('%s slot %s -> %s' % (mesh.get_name(), slot, key))
    ns = mesh.get_editor_property('nanite_settings'); ns.enabled = True; mesh.set_editor_property('nanite_settings', ns)
    bs = mesh.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    log('mesh ready: ' + mesh.get_path_name())
bad = [('%s/%s' % (x.get_name(), s.get_editor_property('material_slot_name'))) for x in (unreal.load_asset(mm.get_path_name()) for mm in meshes)
       for s in x.get_editor_property('static_materials') if not s.get_editor_property('material_interface')]
if bad: unreal.log_error('ADP_ROADSIGN CHECK FAILED: empty material slots ' + ', '.join(bad))
else: log('CHECK passed: every material slot filled')
unreal.SystemLibrary.quit_editor()
