# The grass clumps (Tools/make_grass.py) into Unreal with their material M_Grass:
#  - two-sided foliage shading (light through the blades), dark green at the root to a lighter, yellower green at the tip,
#    each clump a slightly different shade (per-instance random), matte with a faint sheen
#  - the wind: each blade swaying from its root (nothing at the root, most at the tip), in gusts that roll across the
#    field (the sway's phase follows the world position), never the whole field in step
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_grass.py" -unattended -nullrhi
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = '/Game/Materials/Grass'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()

# ---- the material ---------------------------------------------------------------------------------------------------
path = DEST + '/M_Grass'
if eal.does_asset_exist(path):
    m = unreal.load_asset(path); mel.delete_all_material_expressions(m)
else:
    m = at.create_asset('M_Grass', DEST, unreal.Material, unreal.MaterialFactoryNew())
y = [0]
def E(cls, x=-800):
    y[0] += 70; return mel.create_material_expression(m, cls, x, y[0])
def op(cls, a, b, ao='', bo=''):
    k = E(cls); mel.connect_material_expressions(a, ao, k, 'A'); mel.connect_material_expressions(b, bo, k, 'B'); return k
def pw(a, b):
    k = E(unreal.MaterialExpressionPower); mel.connect_material_expressions(a, '', k, 'Base'); mel.connect_material_expressions(b, '', k, 'Exponent'); return k
def c1(v):
    k = E(unreal.MaterialExpressionConstant); k.set_editor_property('r', v); return k
def c3(r, g, b):
    k = E(unreal.MaterialExpressionConstant3Vector); k.set_editor_property('constant', unreal.LinearColor(r, g, b, 1)); return k
def mask(src, ch, out=''):
    k = E(unreal.MaterialExpressionComponentMask)
    for c in 'rgba': k.set_editor_property(c, c in ch)
    mel.connect_material_expressions(src, out, k, ''); return k

uv = E(unreal.MaterialExpressionTextureCoordinate, -1400)
v = mask(uv, 'g')                                            # 0 at the root, 1 at the tip
rnd = E(unreal.MaterialExpressionPerInstanceRandom, -1400)
root, tip = c3(0.012, 0.035, 0.006), c3(0.055, 0.12, 0.018)
lerp = E(unreal.MaterialExpressionLinearInterpolate)
mel.connect_material_expressions(root, '', lerp, 'A'); mel.connect_material_expressions(tip, '', lerp, 'B')
mel.connect_material_expressions(pw(v, c1(0.8)), '', lerp, 'Alpha')
# each clump its own shade: a touch greener or drier
dry = c3(1.25, 1.05, 0.55)
shade = E(unreal.MaterialExpressionLinearInterpolate)
mel.connect_material_expressions(lerp, '', shade, 'A'); mel.connect_material_expressions(op(unreal.MaterialExpressionMultiply, lerp, dry), '', shade, 'B')
mel.connect_material_expressions(op(unreal.MaterialExpressionMultiply, rnd, c1(0.25)), '', shade, 'Alpha')
bright = op(unreal.MaterialExpressionMultiply, shade, op(unreal.MaterialExpressionAdd, c1(0.85), op(unreal.MaterialExpressionMultiply, rnd, c1(0.3))))
mel.connect_material_property(bright, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(c1(0.75), '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(c1(0.15), '', unreal.MaterialProperty.MP_SPECULAR)
mel.connect_material_property(op(unreal.MaterialExpressionMultiply, bright, c1(0.45)), '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
# the root darker (shadow inside the turf)
mel.connect_material_property(op(unreal.MaterialExpressionAdd, c1(0.35), op(unreal.MaterialExpressionMultiply, v, c1(0.65))), '', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

# the wind: sway = sin(time * speed + phase from the world position) with a slower gust on top, times v squared
wp = E(unreal.MaterialExpressionWorldPosition, -1600)
wx, wy = mask(wp, 'r'), mask(wp, 'g')
tm = E(unreal.MaterialExpressionTime, -1600)
phase = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, wx, c1(0.0021)), op(unreal.MaterialExpressionMultiply, wy, c1(0.0013)))
s1 = E(unreal.MaterialExpressionSine); mel.connect_material_expressions(op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, tm, c1(1.9)), phase), '', s1, '')
s1.set_editor_property('period', 6.283185)
gust_ph = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, tm, c1(0.35)), op(unreal.MaterialExpressionMultiply, wx, c1(0.0004)))
s2 = E(unreal.MaterialExpressionSine); mel.connect_material_expressions(gust_ph, '', s2, ''); s2.set_editor_property('period', 6.283185)
gust = op(unreal.MaterialExpressionAdd, c1(0.55), op(unreal.MaterialExpressionMultiply, s2, c1(0.45)))
flex = op(unreal.MaterialExpressionMultiply, v, v)
amp = op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, s1, c1(0.6)), c1(0.5)), gust), op(unreal.MaterialExpressionMultiply, flex, c1(6.0)))
wpo = op(unreal.MaterialExpressionAppendVector, op(unreal.MaterialExpressionAppendVector, amp, op(unreal.MaterialExpressionMultiply, amp, c1(0.45))), c1(0.0))
mel.connect_material_property(wpo, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

m.set_editor_property('two_sided', True)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
m.set_editor_property('used_with_instanced_static_meshes', True)
m.set_editor_property('used_with_nanite', True)   # (without it Nanite draws the blades in the engine's plain fallback)
mel.recompile_material(m)
eal.save_loaded_asset(m, only_if_is_dirty=False)

# ---- the clumps -----------------------------------------------------------------------------------------------------
for name in ('SM_GrassNear', 'SM_GrassFar', 'SM_GrassTuft'):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(ROOT, 'Art', 'Grass', name + '.obj')
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = True
    at.import_asset_tasks([task])
    sm = unreal.load_asset(DEST + '/' + name)
    if not sm:
        unreal.log_error('ADP_GRASS: %s not imported' % name); continue
    for k in range(len(sm.get_editor_property('static_materials'))): sm.set_material(k, m)
    # Nanite: the far grass drawn with as few triangles as the distance allows; thin blades keep their area (no shimmer)
    ns = sm.get_editor_property('nanite_settings')
    ns.set_editor_property('enabled', True)
    try: ns.set_editor_property('shape_preservation', unreal.NaniteShapePreservation.PRESERVE_AREA)
    except Exception as e: unreal.log_warning('ADP_GRASS: no shape preservation: %s' % e)
    sm.set_editor_property('nanite_settings', ns)
    b = sm.get_bounding_box()
    unreal.log('ADP_GRASS: %s bounds %s .. %s' % (name, b.min, b.max))
    eal.save_loaded_asset(sm, only_if_is_dirty=False)
unreal.SystemLibrary.quit_editor()
