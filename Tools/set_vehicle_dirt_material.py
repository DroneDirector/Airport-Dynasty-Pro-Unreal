# M_VehicleDirt: the fire truck's paint (M_TruckPaint: road dirt and spray from the bottom, streaks, wear chips, wetness,
# heavy grime past 1, the wash hose's runs, beads of water and the wet sheen - Tools/set_truck_materials.py) for every
# other vehicle (owner 2026-11-21: the pushback tug, the GPU, the baggage tractor and cart, and every vehicle after them),
# keeping what the imported glTF materials have that the truck's did not: their normal map (relief) and their
# metal / roughness map. Tyres get tread, shoulder blocks and lettered sidewalls (Tools/make_tyre_textures.py draws the
# height maps: run it first), rims brake dust and scuffs (to-do 261). VehicleDirt.cpp puts it on each vehicle's parts, copying each original's colours and textures.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_vehicle_dirt_material.py" -unattended
import os
import unreal

# the truck material script's helpers and shader code (everything before it builds its own materials)
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'set_truck_materials.py')
text = open(SRC, encoding='utf-8').read()
head = text.split("m = mat('M_TruckPaint')")[0]
NS = {}
exec(compile(head, SRC, 'exec'), NS)
eal, mel, log = NS['eal'], NS['mel'], NS['log']
params, custom, wire, mat = NS['params'], NS['custom'], NS['wire'], NS['mat']
TYRE, WHITE, MPC = NS['TYRE'], NS['WHITE'], NS['MPC']
O = unreal.CustomMaterialOutputType
ST = unreal.MaterialSamplerType


def sampler_for(tex):
    cs = tex.get_editor_property('compression_settings')
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP: return ST.SAMPLERTYPE_NORMAL
    if cs == unreal.TextureCompressionSettings.TC_MASKS: return ST.SAMPLERTYPE_MASKS
    if cs == unreal.TextureCompressionSettings.TC_GRAYSCALE: return ST.SAMPLERTYPE_GRAYSCALE if tex.get_editor_property('srgb') else ST.SAMPLERTYPE_LINEAR_GRAYSCALE
    return ST.SAMPLERTYPE_COLOR if tex.get_editor_property('srgb') else ST.SAMPLERTYPE_LINEAR_COLOR


def sample_texture(word):
    # one of the vehicles' own imported textures of that kind, as the parameter's default (its kind sets the sampler:
    # VehicleDirt.cpp only puts in textures of the same kind)
    for folder in ('/Game/Babylon/BaggageTractor', '/Game/Babylon/PushbackTug', '/Game/Babylon/GPU', '/Game/Babylon/BaggageCart'):
        if not eal.does_directory_exist(folder): continue
        for p in eal.list_assets(folder, recursive=True, include_folder=False):
            if word not in p.lower(): continue
            a = unreal.load_asset(p)
            if isinstance(a, unreal.Texture2D): return a
    return None


def height_texture(name):
    # the tyres' height maps (Tools/make_tyre_textures.py draws them): plain linear grey
    src = os.path.abspath(os.path.join(os.path.dirname(SRC), '..', 'Art', 'Tyre', name + '.png'))
    path = '/Game/Truck/' + name
    if os.path.isfile(src):
        task = unreal.AssetImportTask()
        for k, v in (('filename', src), ('destination_path', '/Game/Truck'), ('destination_name', name), ('automated', True), ('replace_existing', True), ('save', False)):
            task.set_editor_property(k, v)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    else:
        log('WARNING: %s is missing (run python Tools/make_tyre_textures.py first)' % src)
    t = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if not t:
        log('WARNING: no %s - the tyres stay plain' % name)
        return WHITE
    t.set_editor_property('srgb', False)
    t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
    eal.save_loaded_asset(t, only_if_is_dirty=False)
    return t


tread, side = height_texture('T_TyreTread'), height_texture('T_TyreSide')
m = mat('M_VehicleDirt')
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_CLEAR_COAT)
m.set_editor_property('two_sided', True)
m.set_editor_property('use_material_attributes', True)
P = params(m, [('R', 0.5), ('M', 0.0), ('CC', 0.0), ('CCR', 0.12), ('GroundZ', 0.0), ('Dirt', 0.5), ('Wear', 0.3), ('Blow', 0.0), ('UseN', 0.0), ('UseORM', 0.0), ('NS', 1.0)] + NS['WHEEL_S'],
           [('BC', (1, 1, 1, 1)), ('OS', (0, 0, 1, 1)), ('NOS', (0, 0, 1, 1)), ('MOS', (0, 0, 1, 1)), ('Origin', (0, 0, 0, 0)), ('AxisF', (1, 0, 0, 0)),
            ('Z0', (1, 1, 1, 1)), ('Z1', (1, 1, 1, 1)), ('Z2', (1, 1, 1, 1)), ('B0', (0, 0, 0, 0)), ('B1', (0, 0, 0, 0)), ('B2', (0, 0, 0, 0))] + NS['WHEEL_V'], [('Base', WHITE)])
wet = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, -900, 900)
wet.set_editor_property('collection', MPC); wet.set_editor_property('parameter_name', 'Wetness'); P['Wet'] = (wet, '')

# the relief: the model's normal map (a flat one by default), its strength, faded where thick dirt fills it
nrm_def = sample_texture('normal') or unreal.load_asset('/Engine/EngineMaterials/DefaultNormal')
nobj = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -1200, 1000)
nobj.set_editor_property('parameter_name', 'Nrm'); nobj.set_editor_property('texture', nrm_def); nobj.set_editor_property('sampler_type', sampler_for(nrm_def))
# the metal / roughness map (glTF: roughness in green, metal in blue)
orm_def = sample_texture('metal') or sample_texture('rough') or WHITE
oobj = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -1200, 1150)
oobj.set_editor_property('parameter_name', 'ORM'); oobj.set_editor_property('texture', orm_def); oobj.set_editor_property('sampler_type', sampler_for(orm_def))
log('M_VehicleDirt defaults: normal %s (%s), metal/roughness %s (%s)' % (nrm_def.get_name(), sampler_for(nrm_def), orm_def.get_name(), sampler_for(orm_def)))

uvn = custom(m, 'return UV * NOS.zw + NOS.xy;', O.CMOT_FLOAT2, ['UV', 'NOS'], 1000); wire(m, uvn, P, ['UV', 'NOS'])
uvm = custom(m, 'return UV * MOS.zw + MOS.xy;', O.CMOT_FLOAT2, ['UV', 'MOS'], 1100); wire(m, uvm, P, ['UV', 'MOS'])
def sampler_node(obj, uv, tex, y):
    s = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -600, y)
    s.set_editor_property('sampler_type', sampler_for(tex))
    if not (mel.connect_material_expressions(obj, '', s, 'Tex') or mel.connect_material_expressions(obj, '', s, 'TextureObject')): log('WIRE FAILED texture object')
    if not mel.connect_material_expressions(uv, '', s, 'UVs'): log('WIRE FAILED uvs')
    return s
ns = sampler_node(nobj, uvn, nrm_def, 1000)
os_ = sampler_node(oobj, uvm, orm_def, 1150)
P['NT'] = (ns, 'RGB'); P['MT'] = (os_, 'RGB')

# roughness and metal from the map where there is one
reff = custom(m, 'return lerp(R, R * MT.g, UseORM);', O.CMOT_FLOAT1, ['R', 'MT', 'UseORM'], 1250); wire(m, reff, P, ['R', 'MT', 'UseORM'])
meff = custom(m, 'return lerp(M, M * MT.b, UseORM);', O.CMOT_FLOAT1, ['M', 'MT', 'UseORM'], 1350); wire(m, meff, P, ['M', 'MT', 'UseORM'])
# the tyres' relief: its slope worked out in the wheel's own frame, then turned into the surface's
NS['local_place'](m, P)
for n, tex, y in (('TreadH', tread, 1600), ('SideH', side, 1700)):
    x = mel.create_material_expression(m, unreal.MaterialExpressionTextureObjectParameter, -1200, y)
    x.set_editor_property('parameter_name', n); x.set_editor_property('texture', tex); x.set_editor_property('sampler_type', sampler_for(tex))
    P[n] = (x, '')
TYRE_IN = ['LP', 'LPX', 'LPY', 'TyreC', 'TyreS', 'Rim', 'TreadH', 'SideH']
ty = custom(m, TYRE, O.CMOT_FLOAT4, TYRE_IN, 1600); wire(m, ty, P, TYRE_IN)
P['TY'] = (ty, '')
gl = custom(m, 'return TY.xyz;', O.CMOT_FLOAT3, ['TY'], 1750); wire(m, gl, P, ['TY'])
tr = mel.create_material_expression(m, unreal.MaterialExpressionTransform, -100, 1750)
tr.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_INSTANCE)
tr.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_TANGENT)
if not mel.connect_material_expressions(gl, '', tr, ''): log('WIRE FAILED tyre slope')
P['TG'] = (tr, '')
NOUT_IN = ['NT', 'NS', 'UseN', 'TG']
nout = custom(m, 'float3 n = NT; n.xy *= NS; n = normalize(float3(n.xy, max(n.z, 0.05))); n = normalize(lerp(float3(0, 0, 1), n, UseN)); return normalize(float3(n.xy - TG.xy, n.z));',
              O.CMOT_FLOAT3, NOUT_IN, 1450); wire(m, nout, P, NOUT_IN)

mma = mel.create_material_expression(m, unreal.MaterialExpressionMakeMaterialAttributes, 200, 0)
P2 = dict(P); P2['R'] = (reff, '')
NS['paint_node'](m, P2, mma)
mel.connect_material_expressions(P['CCR'][0], '', mma, 'ClearCoatRoughness')
mel.connect_material_expressions(meff, '', mma, 'Metallic')
mel.connect_material_expressions(nout, '', mma, 'Normal')
mel.connect_material_property(mma, '', unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES)
m.set_editor_property('used_with_nanite', True); m.set_editor_property('used_with_instanced_static_meshes', True)
mel.recompile_material(m); eal.save_loaded_asset(m, only_if_is_dirty=False)
log('ready: M_VehicleDirt')
unreal.SystemLibrary.quit_editor()
