# An aircraft lamp's light on its own skin and on what is right round it (to-do 243, owner 2026-11-24): a deferred light
# decal - no light source - so every aircraft at any distance lights itself up: the beacon's red sweep over the fuselage and
# the apron, the strobe's flash on the wingtip, the nav lights' tint, the logo light on the fin, the landing / taxi beam on
# the nacelle, the nose gear and the ground ahead. AircraftLights.cpp sets per lamp (from Data/aircraft-lights.json):
#   LampPos  xyz the lamp in the world (cm), a its reach (cm)
#   LampDir  xyz where it shines (0 = all round), a the cosine of the beam's half angle (-1 all round)
#   Color, Strength
# The light: (1 - distance / reach)^2, times the beam (cosine between the edge and the middle), additive (emissive only;
# nothing else of the surface is written). Run once (and again only to change the look):
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs path>/Tools/make_aircraft_lamp_light.py"
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
DIR, NAME = '/Game/Materials/Lamps', 'M_AircraftLampLight'
path = DIR + '/' + NAME


def node(cls, x, y):
    return mel.create_material_expression(m, cls, x, y)


def con(a, ao, b, bi):
    if not mel.connect_material_expressions(a, ao, b, bi):
        raise RuntimeError('could not connect %s.%s -> %s.%s' % (a.get_name(), ao, b.get_name(), bi))


def op(cls, a, b, x, y, ao='', bo=''):
    n = node(cls, x, y); con(a, ao, n, 'A'); con(b, bo, n, 'B'); return n


def one(cls, a, x, y, ao=''):
    n = node(cls, x, y); con(a, ao, n, ''); return n


def const(v, x, y):
    n = node(unreal.MaterialExpressionConstant, x, y); n.set_editor_property('r', v); return n


try:
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else at.create_asset(NAME, DIR, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    # blend first: a decal domain on an opaque material fails its in-between compile
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
    pos = node(unreal.MaterialExpressionVectorParameter, -1400, -200)
    pos.set_editor_property('parameter_name', 'LampPos'); pos.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, 1000))
    ldir = node(unreal.MaterialExpressionVectorParameter, -1400, 150)
    ldir.set_editor_property('parameter_name', 'LampDir'); ldir.set_editor_property('default_value', unreal.LinearColor(0, 0, 0, -1))
    col = node(unreal.MaterialExpressionVectorParameter, -400, -450)
    col.set_editor_property('parameter_name', 'Color'); col.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    stren = node(unreal.MaterialExpressionScalarParameter, -400, -300)
    stren.set_editor_property('parameter_name', 'Strength'); stren.set_editor_property('default_value', 0.0)
    wp = node(unreal.MaterialExpressionWorldPosition, -1400, -400)
    # distance falloff: (1 - d / reach)^2
    dist = op(unreal.MaterialExpressionDistance, wp, pos, -1150, -350)
    frac = op(unreal.MaterialExpressionDivide, dist, pos, -950, -350, bo='A')
    near = one(unreal.MaterialExpressionOneMinus, frac, -800, -350)
    near = one(unreal.MaterialExpressionSaturate, near, -680, -350)
    fall = op(unreal.MaterialExpressionMultiply, near, near, -560, -350)
    # the beam: the direction from the lamp against where it shines, 0 at the edge, 1 in the middle (all round: a flat half)
    rel = op(unreal.MaterialExpressionSubtract, wp, pos, -1150, -100)
    nrm = one(unreal.MaterialExpressionNormalize, rel, -1000, -100)
    cosv = op(unreal.MaterialExpressionDotProduct, nrm, ldir, -850, -50)
    num = op(unreal.MaterialExpressionSubtract, cosv, ldir, -700, 0, bo='A')
    den = op(unreal.MaterialExpressionSubtract, const(1.0001, -850, 200), ldir, -700, 150, bo='A')
    beam = one(unreal.MaterialExpressionSaturate, op(unreal.MaterialExpressionDivide, num, den, -560, 50), -440, 50)
    mask = op(unreal.MaterialExpressionMultiply, fall, beam, -300, -150)
    tint = op(unreal.MaterialExpressionMultiply, col, stren, -200, -400)
    emis = op(unreal.MaterialExpressionMultiply, tint, mask, -50, -250)
    opac = one(unreal.MaterialExpressionSaturate, op(unreal.MaterialExpressionMultiply, mask, const(50.0, -300, 0), -150, -50), -30, -50)
    mel.connect_material_property(emis, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opac, '', unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(m)
    eal.save_asset(path)
    unreal.log('ADP_LAMP: ' + path + ' made')
except Exception as e:
    unreal.log_error('ADP_LAMP: failed ' + str(e))
unreal.SystemLibrary.quit_editor()
