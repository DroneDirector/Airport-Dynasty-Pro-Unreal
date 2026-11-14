# The land along the roads into Unreal as Nanite meshes (owner, 2026-11-14: "change it into Nanite, for performance"):
# Art/Land/land_corridor.glb (Tools/land_nanite.py, from the shaped land the game wrote with -DumpLand) imported to
# /Game/Land/Corridor/LC_<block> - Nanite on, its own shape as the collision (walkable, drivable) - and the band's own
# ground material /Game/Land/M_LandCorridor: the globe's picture (a parameter the game sets per Settings > Imagery) on
# a fully rough, unreflective surface (the terminal's material it used gave a sky sheen at low angles, so the band
# looked lighter than the globe round it). LC_Ready marks the set complete for ALandCorridor.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_land_corridor.py" -unattended
import os, stat
import unreal
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
DIR = '/Game/Land'; COR = DIR + '/Corridor'
def log(m): unreal.log('ADP_LAND ' + m)
# (Git LFS 'lockable' leaves committed assets read-only: saves would fail quietly)
for d, _, fs in os.walk(os.path.join(ROOT, 'Content', 'Land')):
    for f in fs: os.chmod(os.path.join(d, f), stat.S_IWRITE | stat.S_IREAD)

# ---- the ground material ----
if not eal.does_directory_exist(DIR): eal.make_directory(DIR)
MP = DIR + '/M_LandCorridor'
if eal.does_asset_exist(MP): eal.delete_asset(MP)
m = tools.create_asset('M_LandCorridor', DIR, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property('two_sided', True)
tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -600, 0)
tex.set_editor_property('parameter_name', 'Base')
tex.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
bc = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, 250)
bc.set_editor_property('parameter_name', 'BC'); bc.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 100)
mel.connect_material_expressions(tex, 'RGB', mul, 'A'); mel.connect_material_expressions(bc, '', mul, 'B')
mel.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 300); rough.set_editor_property('r', 1.0)
mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
spec = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 400); spec.set_editor_property('r', 0.12)
mel.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
log('material ready')

# ---- the meshes ----
if eal.does_directory_exist(COR): eal.delete_directory(COR)
eal.make_directory(COR)
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters(); params.is_automated = True
manager.import_asset(COR + '/Import', unreal.InterchangeManager.create_source_data(os.path.join(ROOT, 'Art', 'Land', 'land_corridor.glb')), params)
manager.wait_until_all_tasks_done(False)
n = 0
for p in eal.list_assets(COR + '/Import', recursive=True):
    a = unreal.load_asset(p)
    if not isinstance(a, unreal.StaticMesh): continue
    name = a.get_name()
    if not name.startswith('LC_'): continue
    ns = a.get_editor_property('nanite_settings'); ns.enabled = True
    # every triangle kept in its fallback too (collision, ray tracing and the far view use it): the default fallback
    # smoothed away the 30 cm trough the land keeps under each road, so the land stood through the asphalt in places
    # (Main Street, 2026-11-15)
    for k, v in (('fallback_relative_error', 0.0), ('fallback_percent_triangles', 1.0), ('keep_percent_triangles', 1.0), ('trim_relative_error', 0.0)):
        try: ns.set_editor_property(k, v)
        except Exception as e: log('nanite setting %s: %s' % (k, e))
    try: ns.set_editor_property('fallback_target', unreal.NaniteFallbackTarget.RELATIVE_ERROR)
    except Exception as e: log('nanite fallback target: %s' % e)
    a.set_editor_property('nanite_settings', ns)
    bs = a.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    a.set_material(0, m)
    eal.save_loaded_asset(a, only_if_is_dirty=False)
    eal.rename_asset(a.get_path_name(), COR + '/' + name + '.' + name)
    n += 1
eal.delete_directory(COR + '/Import')
mk = tools.create_asset('LC_Ready', COR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
eal.save_loaded_asset(mk, only_if_is_dirty=False)
eal.save_directory(COR, only_if_is_dirty=False, recursive=True)
log('%d Nanite land blocks ready' % n)
unreal.SystemLibrary.quit_editor()
