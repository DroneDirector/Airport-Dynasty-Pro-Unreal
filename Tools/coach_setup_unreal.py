# The airport coach's Unreal side (to-do 144, owner 2026-11-19): everything the parked coaches need beyond the import
#  - every coach part collides by its real shape (the import gave each part one box round it: the body a solid block you
#    couldn't enter; the floor, steps, walls, seats and door leaves must be walkable / solid as they look)
#  - its sounds (Art/Sounds/Coach, CC0 from Freesound - SOURCES.md there) -> /Game/Coach/Sounds; the drive's hum loops
#  - M_CoachLamp: a lamp lens - tinted by Color, glowing by Glow (0 off) - each coach's lenses lit on their own (Coaches.cpp)
#  - M_CoachMirror: a mirror's glass - its capture's picture (Reflection), left-right turned as a mirror shows it, unlit
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/coach_setup_unreal.py" -unattended -nullrhi
import os, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EL = unreal.EditorAssetLibrary; ML = unreal.MaterialEditingLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()

def writable(game_path):
    f = os.path.join(ROOT, 'Content', game_path[len('/Game/'):] + '.uasset')
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)

# 1. collision by shape
n = 0
for p in EL.list_assets('/Game/Babylon/AirportCoach/airportcoach/StaticMeshes', recursive=False):
    sm = unreal.load_asset(p)
    if not isinstance(sm, unreal.StaticMesh): continue
    writable(p.split('.')[0])
    bs = sm.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EL.save_loaded_asset(sm, only_if_is_dirty=False); n += 1
unreal.log('ADP_COACH collision by shape on %d parts' % n)

# 2. sounds
SD = '/Game/Coach/Sounds'
for name, loop in (('CoachDoor', False), ('CoachKneel', False), ('CoachBay', False), ('CoachDrive', True)):
    writable(SD + '/' + name)
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'Sounds', 'Coach', name + '.wav'); t.destination_path = SD
    t.automated = True; t.replace_existing = True; t.save = True
    AT.import_asset_tasks([t])
    w = unreal.load_asset(SD + '/' + name)
    if w: w.set_editor_property('looping', loop); EL.save_loaded_asset(w); unreal.log('ADP_COACH sound %s %.2f s' % (name, w.get_editor_property('duration')))
    else: unreal.log('ADP_COACH sound %s MISSING' % name)

def new_material(path):
    if EL.does_asset_exist(path):
        writable(path); return unreal.load_asset(path)
    folder, name = path.rsplit('/', 1)
    return AT.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

# 3. the lamp lens
m = new_material('/Game/Coach/M_CoachLamp')
ML.delete_all_material_expressions(m)
col = ML.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, 0)
col.set_editor_property('parameter_name', 'Color'); col.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
glow = ML.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 200)
glow.set_editor_property('parameter_name', 'Glow'); glow.set_editor_property('default_value', 0.0)
tint = ML.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, -100)
tint.set_editor_property('const_b', 0.22); ML.connect_material_expressions(col, '', tint, 'A')
emis = ML.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 120)
ML.connect_material_expressions(col, '', emis, 'A'); ML.connect_material_expressions(glow, '', emis, 'B')
rough = ML.create_material_expression(m, unreal.MaterialExpressionConstant, -300, 300); rough.set_editor_property('r', 0.12)
ML.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
ML.connect_material_property(emis, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
ML.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
m.set_editor_property('used_with_instanced_static_meshes', True); m.set_editor_property('used_with_nanite', True)
ML.recompile_material(m); EL.save_loaded_asset(m)
unreal.log('ADP_COACH M_CoachLamp ok')

# 4. the mirror's glass (unlit: the picture as it is; the U turned round, as a mirror shows the world)
m = new_material('/Game/Coach/M_CoachMirror')
ML.delete_all_material_expressions(m)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
uv = ML.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -700, 0)
uv.set_editor_property('u_tiling', -1.0)
tex = ML.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -450, 0)
tex.set_editor_property('parameter_name', 'Reflection')
tex.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/Black'))   # (a default texture: the texture rule)
ML.connect_material_expressions(uv, '', tex, 'UVs')
dim = ML.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 0); dim.set_editor_property('const_b', 0.9)
ML.connect_material_expressions(tex, 'RGB', dim, 'A')
ML.connect_material_property(dim, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
m.set_editor_property('used_with_instanced_static_meshes', True); m.set_editor_property('used_with_nanite', True)
ML.recompile_material(m); EL.save_loaded_asset(m)
unreal.log('ADP_COACH M_CoachMirror ok')
