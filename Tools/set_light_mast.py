# The apron light mast (Art/LightMast/make_lightmast.py -> lightmast.glb) into Unreal: the mesh to /Game/LightMast
# (Nanite on, its own shape as the collision), the textures and materials to /Game/Materials/LightMast:
#   galvanised steel (the tileable metal maps; satin, not mirror-like, not white), the guardrail (the same steel with
#   the yellow / black bands every other 3 m panel), the LED faces (the floodlight picture; their glow is set by the
#   game at night, parameter "Glow"), and the plain parts (bolts, aluminium, concrete, copper, plastic, the red light).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_light_mast.py" -unattended -nullrhi
import os
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(ROOT, 'Art', 'LightMast')
MESH_DIR, MAT_DIR = '/Game/LightMast', '/Game/Materials/LightMast'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def log(m): unreal.log('ADP_MAST ' + m)

# ---- the textures ----
def import_tex(file, name, srgb=True, comp=None):
    t = unreal.AssetImportTask()
    t.filename = os.path.join(ART, file); t.destination_path = MAT_DIR; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    tools.import_asset_tasks([t])
    tex = unreal.load_asset(MAT_DIR + '/' + name)
    tex.set_editor_property('srgb', srgb)
    if comp is not None: tex.set_editor_property('compression_settings', comp)
    eal.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex
T_BASE = import_tex('metal_basecolor.png', 'T_LM_Metal_BaseColor')
T_ROUGH = import_tex('metal_roughness.png', 'T_LM_Metal_Roughness', False, unreal.TextureCompressionSettings.TC_MASKS)
T_NORM = import_tex('metal_normal_dx.png', 'T_LM_Metal_Normal', False, unreal.TextureCompressionSettings.TC_NORMALMAP)
T_LED = import_tex('led_panel.png', 'T_LM_LEDFace')
log('textures imported')

# ---- the mesh ----
manager = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters(); params.is_automated = True
if not eal.does_directory_exist(MESH_DIR + '/lightmast'): manager.import_asset(MESH_DIR, unreal.InterchangeManager.create_source_data(os.path.join(ART, 'lightmast.glb')), params)
manager.wait_until_all_tasks_done(False)
meshes = []
for p in eal.list_assets(MESH_DIR, recursive=True):
    a = unreal.load_asset(p)
    if isinstance(a, unreal.StaticMesh): meshes.append(a); log('mesh ' + p)
if not meshes: raise RuntimeError('no mesh imported')

# ---- the materials ----
def new_material(name):
    path = MAT_DIR + '/' + name
    if eal.does_asset_exist(path): eal.delete_asset(path)
    m = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_nanite', True); m.set_editor_property('used_with_instanced_static_meshes', True)
    return m
def E(m, cls, x, y): return mel.create_material_expression(m, cls, x, y)
def scalar(m, name, v, x, y):
    e = E(m, unreal.MaterialExpressionScalarParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', v); return e
def vector(m, name, c, x, y):
    e = E(m, unreal.MaterialExpressionVectorParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', unreal.LinearColor(*c)); return e
def const(m, v, x, y):
    e = E(m, unreal.MaterialExpressionConstant, x, y); e.set_editor_property('r', v); return e
def tex(m, name, t, x, y, st, uv):
    e = E(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property('parameter_name', name); e.set_editor_property('texture', t); e.set_editor_property('sampler_type', st)
    mel.connect_material_expressions(uv, '', e, 'UVs'); return e
def op(m, cls, a, b, x, y, an='', bn=''):
    e = E(m, cls, x, y); mel.connect_material_expressions(a, an, e, 'A'); mel.connect_material_expressions(b, bn, e, 'B'); return e

def steel(name, stripes=False):
    """Galvanised steel from the tileable maps; with stripes, the painted bands along the guardrail (UV x = metres)."""
    m = new_material(name)
    uv = E(m, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    base = tex(m, 'BaseTex', T_BASE, -1100, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, uv)
    rough = tex(m, 'RoughTex', T_ROUGH, -1100, 0, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, uv)
    norm = tex(m, 'NormalTex', T_NORM, -1100, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, uv)
    tint = vector(m, 'Tint', (1.25, 1.26, 1.28, 1), -1100, -500)
    col = op(m, unreal.MaterialExpressionMultiply, base, tint, -800, -350, 'RGB')
    rmul = scalar(m, 'RoughnessScale', 1.6, -1100, 150)
    r = op(m, unreal.MaterialExpressionMultiply, rough, rmul, -800, 0, 'R')
    metal = scalar(m, 'Metallic', 0.3, -800, -150)
    # a flatter normal: the coating's relief only just catching the light
    nflat = E(m, unreal.MaterialExpressionLinearInterpolate, -800, 300)
    up = E(m, unreal.MaterialExpressionConstant3Vector, -1100, 500); up.set_editor_property('constant', unreal.LinearColor(0, 0, 1, 1))
    mel.connect_material_expressions(norm, 'RGB', nflat, 'A'); mel.connect_material_expressions(up, '', nflat, 'B')
    mel.connect_material_expressions(scalar(m, 'NormalFlatten', 0.35, -1100, 450), '', nflat, 'Alpha')
    if stripes:
        u = E(m, unreal.MaterialExpressionComponentMask, -1100, 650); u.set_editor_property('r', True); u.set_editor_property('g', False)
        mel.connect_material_expressions(uv, '', u, '')
        def band(period, x, y):
            """1 in the first half of each period along the rail, 0 in the second (frac, floor, one-minus)."""
            d = op(m, unreal.MaterialExpressionDivide, u, const(m, period, x - 200, y + 60), x, y)
            fr = E(m, unreal.MaterialExpressionFrac, x + 150, y); mel.connect_material_expressions(d, '', fr, '')
            tw = op(m, unreal.MaterialExpressionMultiply, fr, const(m, 2.0, x + 150, y + 80), x + 300, y)
            fl = E(m, unreal.MaterialExpressionFloor, x + 450, y); mel.connect_material_expressions(tw, '', fl, '')
            om = E(m, unreal.MaterialExpressionOneMinus, x + 600, y); mel.connect_material_expressions(fl, '', om, '')
            return om
        fmod_lt = lambda period, half, x, y: band(period, x, y)
        painted = fmod_lt(3.0, 1.5, -800, 650)          # every other 3 m panel
        yellow = fmod_lt(0.5, 0.25, -800, 1050)         # 0.25 m bands
        paint = E(m, unreal.MaterialExpressionLinearInterpolate, -450, 900)
        mel.connect_material_expressions(vector(m, 'Black', (0.02, 0.02, 0.02, 1), -700, 1400), '', paint, 'A')
        mel.connect_material_expressions(vector(m, 'Yellow', (0.85, 0.62, 0.02, 1), -700, 1550), '', paint, 'B')
        mel.connect_material_expressions(yellow, '', paint, 'Alpha')
        c2 = E(m, unreal.MaterialExpressionLinearInterpolate, -250, -300)
        mel.connect_material_expressions(col, '', c2, 'A'); mel.connect_material_expressions(paint, '', c2, 'B'); mel.connect_material_expressions(painted, '', c2, 'Alpha')
        m2 = E(m, unreal.MaterialExpressionLinearInterpolate, -250, -150)
        mel.connect_material_expressions(metal, '', m2, 'A'); mel.connect_material_expressions(const(m, 0.0, -450, -100), '', m2, 'B'); mel.connect_material_expressions(painted, '', m2, 'Alpha')
        r2 = E(m, unreal.MaterialExpressionLinearInterpolate, -250, 0)
        mel.connect_material_expressions(r, '', r2, 'A'); mel.connect_material_expressions(const(m, 0.55, -450, 50), '', r2, 'B'); mel.connect_material_expressions(painted, '', r2, 'Alpha')
        col, metal, r = c2, m2, r2
    mel.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(nflat, '', unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
    return m

M_STEEL = steel('M_LM_Galvanised')
M_RAIL = steel('M_LM_Guardrail', stripes=True)

# the LED faces: the floodlight picture; its glow (night) by the parameter Glow
m = new_material('M_LM_LEDFace')
uv = E(m, unreal.MaterialExpressionTextureCoordinate, -1000, 0)
led = tex(m, 'FaceTex', T_LED, -700, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, uv)
mel.connect_material_property(led, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(const(m, 0.5, -300, 150), '', unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(const(m, 0.3, -300, 220), '', unreal.MaterialProperty.MP_ROUGHNESS)
# only the lenses' bright centres glow: the picture's brightness pushed up steeply
pw = E(m, unreal.MaterialExpressionPower, -450, 350)
mel.connect_material_expressions(led, 'R', pw, 'Base'); mel.connect_material_expressions(const(m, 4.0, -650, 450), '', pw, 'Exponent')
warm = op(m, unreal.MaterialExpressionMultiply, pw, vector(m, 'GlowColor', (1.0, 0.95, 0.88, 1), -650, 550), -250, 350)
glow = op(m, unreal.MaterialExpressionMultiply, warm, scalar(m, 'Glow', 0.0, -450, 650), -100, 450)
mel.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
M_LED = m

# the plain parts: one material, an instance each
m = new_material('M_LM_Plain')
mel.connect_material_property(vector(m, 'BaseColor', (0.5, 0.5, 0.5, 1), -400, 0), '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(scalar(m, 'Metallic', 0.0, -400, 150), '', unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(scalar(m, 'Roughness', 0.5, -400, 250), '', unreal.MaterialProperty.MP_ROUGHNESS)
em = op(m, unreal.MaterialExpressionMultiply, vector(m, 'EmissiveColor', (0, 0, 0, 1), -600, 400), scalar(m, 'EmissiveStrength', 0.0, -600, 550), -300, 450)
mel.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
M_PLAIN = m
def plain(name, col, metal, rough, emit=None, strength=0.0):
    path = MAT_DIR + '/' + name
    if eal.does_asset_exist(path): eal.delete_asset(path)
    mi = tools.create_asset(name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, M_PLAIN)
    mel.set_material_instance_vector_parameter_value(mi, 'BaseColor', unreal.LinearColor(*col, 1))
    mel.set_material_instance_scalar_parameter_value(mi, 'Metallic', metal)
    mel.set_material_instance_scalar_parameter_value(mi, 'Roughness', rough)
    if emit:
        mel.set_material_instance_vector_parameter_value(mi, 'EmissiveColor', unreal.LinearColor(*emit, 1))
        mel.set_material_instance_scalar_parameter_value(mi, 'EmissiveStrength', strength)
    eal.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi
SLOTS = {
    'Galvanised': M_STEEL, 'Guardrail': M_RAIL, 'LEDFace': M_LED,
    'Bolt': plain('MI_LM_Bolt', (0.55, 0.56, 0.58), 0.9, 0.35),
    'Aluminium': plain('MI_LM_Aluminium', (0.62, 0.64, 0.66), 0.85, 0.45),
    'DarkMetal': plain('MI_LM_DarkMetal', (0.1, 0.11, 0.12), 0.7, 0.5),
    'Plastic': plain('MI_LM_Plastic', (0.78, 0.78, 0.76), 0.0, 0.55),
    'Concrete': plain('MI_LM_Concrete', (0.36, 0.35, 0.33), 0.0, 0.9),
    'Copper': plain('MI_LM_Copper', (0.72, 0.42, 0.25), 0.9, 0.4),
    'ObstructionLight': plain('MI_LM_ObstructionLight', (0.5, 0.02, 0.02), 0.0, 0.3, (1.0, 0.05, 0.02), 6.0),
}

# ---- the meshes: materials by slot, Nanite on the main one (not the thin parts), their own shape as the collision ----
for mesh in meshes:
    thin = 'thin' in mesh.get_name().lower()
    for i, sm in enumerate(mesh.get_editor_property('static_materials')):
        slot = str(sm.get_editor_property('material_slot_name'))
        key = next((k for k in SLOTS if k.lower() in slot.lower()), None)
        if key: mesh.set_material(i, SLOTS[key])
        log('%s slot %s -> %s' % (mesh.get_name(), slot, key))
    ns = mesh.get_editor_property('nanite_settings'); ns.enabled = not thin; mesh.set_editor_property('nanite_settings', ns)
    bs = mesh.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh, only_if_is_dirty=False)
    log('%s check: %s' % (mesh.get_name(), [str(x.get_editor_property('material_interface').get_name()) if x.get_editor_property('material_interface') else None for x in mesh.get_editor_property('static_materials')]))
    log('mesh ready: %s (Nanite %s)' % (mesh.get_path_name(), 'off' if thin else 'on'))
# CHECK (owner's rule): the saved meshes reloaded from disk; no material slot may be empty
bad = []
for mesh in meshes:
    fresh = unreal.load_asset(mesh.get_path_name())
    for x in fresh.get_editor_property('static_materials'):
        if not x.get_editor_property('material_interface'): bad.append('%s/%s' % (fresh.get_name(), x.get_editor_property('material_slot_name')))
if bad: unreal.log_error('ADP_MAST CHECK FAILED: empty material slots ' + ', '.join(bad))
else: log('CHECK passed: every material slot filled in the saved meshes')
unreal.SystemLibrary.quit_editor()
