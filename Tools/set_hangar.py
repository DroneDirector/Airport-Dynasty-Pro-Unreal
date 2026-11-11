# The hangars' materials: the textures made from the owner's hangar photos (Tools/make_hangar_textures.py,
# Art/Hangar) laid on by world position from three sides at once and blended by which way each bit of surface faces,
# so the engine's scaled boxes the hangars are built from never stretch them (a rib stays a rib on a 90 m wall and on a
# 1 m door). Colour and relief (the normal map, in world space) from the same three projections. Matte enough never to
# glare: the white cladding a touch below the photo's white, as the terminal's own whites.
# Makes /Game/Materials/Hangar/M_Hangar<Name> for each texture.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/set_hangar.py" -unattended -nullrhi
import os
import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = '/Game/Materials/Hangar'
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()

# name: (tile width m, tile height m, brightness, roughness, metallic, relief strength)
SETS = {
    'Clad':       (1.5, 1.5, 0.82, 0.42, 0.25, 1.0),
    'Liner':      (1.0, 1.0, 0.86, 0.6, 0.0, 0.6),
    'Floor':      (6.0, 6.0, 1.0, 0.28, 0.0, 0.5),
    'Asphalt':    (4.0, 4.0, 1.0, 0.9, 0.0, 0.8),
    'Wood':       (0.8, 0.8, 0.95, 0.45, 0.0, 0.4),
    'Slats':      (1.2, 2.4, 0.9, 0.45, 0.5, 1.0),
    'Deck':       (1.5, 1.5, 1.0, 0.55, 0.35, 1.0),
    'Corrugated': (1.52, 1.52, 0.9, 0.38, 0.45, 1.0),
}


def tex(path, normal):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    at.import_asset_tasks([task])
    name = os.path.splitext(os.path.basename(path))[0]
    t = unreal.load_asset(DEST + '/' + name)
    if normal:
        t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        t.set_editor_property('srgb', False)
    eal.save_loaded_asset(t, only_if_is_dirty=False)
    return t


def make(name, tw, th, bright, rough, metal, relief):
    D = tex(os.path.join(ROOT, 'Art', 'Hangar', 'T_Hangar%s_D.png' % name), False)
    N = tex(os.path.join(ROOT, 'Art', 'Hangar', 'T_Hangar%s_N.png' % name), True)
    path = DEST + '/M_Hangar' + name
    if eal.does_asset_exist(path):
        m = unreal.load_asset(path); mel.delete_all_material_expressions(m)
    else:
        m = at.create_asset('M_Hangar' + name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)

    def mask(src, ch, x, y):
        k = E(unreal.MaterialExpressionComponentMask, x, y)
        for c in 'rgba':
            k.set_editor_property(c, c in ch)
        mel.connect_material_expressions(src, '', k, '')
        return k

    def op(cls, a, b, x, y):
        k = E(cls, x, y)
        mel.connect_material_expressions(a, '', k, 'A'); mel.connect_material_expressions(b, '', k, 'B')
        return k

    def const(v, x, y):
        k = E(unreal.MaterialExpressionConstant, x, y); k.set_editor_property('r', v); return k

    def app(a, b, x, y): return op(unreal.MaterialExpressionAppendVector, a, b, x, y)

    wp = E(unreal.MaterialExpressionWorldPosition, -2400, 0)
    X, Y, Z = mask(wp, 'r', -2250, -100), mask(wp, 'g', -2250, 0), mask(wp, 'b', -2250, 100)
    cw, ch = const(tw * 100.0, -2250, 220), const(-th * 100.0, -2250, 300)   # (cm; v runs down the picture)
    ux, uy = op(unreal.MaterialExpressionDivide, X, cw, -2100, -100), op(unreal.MaterialExpressionDivide, Y, cw, -2100, 0)
    vz, vy = op(unreal.MaterialExpressionDivide, Z, ch, -2100, 100), op(unreal.MaterialExpressionDivide, Y, ch, -2100, 200)

    def sample(t, u, v, y, normal):
        uv = app(u, v, -1900, y)
        s = E(unreal.MaterialExpressionTextureSample, -1750, y); s.set_editor_property('texture', t)
        if normal:
            s.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_expressions(uv, '', s, 'UVs')
        return s

    # the three projections: seen along X (walls facing east/west), along Y (facing north/south), from above
    cx, cy, cz = sample(D, uy, vz, -600, False), sample(D, ux, vz, -300, False), sample(D, ux, vy, 0, False)
    nx, ny, nz = sample(N, uy, vz, 300, True), sample(N, ux, vz, 600, True), sample(N, ux, vy, 900, True)

    # blend weights: how much the surface faces each way (sharpened), from the vertex normal
    vn = E(unreal.MaterialExpressionVertexNormalWS, -1900, 1300)
    ab = E(unreal.MaterialExpressionAbs, -1750, 1300); mel.connect_material_expressions(vn, '', ab, '')
    p2 = op(unreal.MaterialExpressionMultiply, ab, ab, -1600, 1300)
    p4 = op(unreal.MaterialExpressionMultiply, p2, p2, -1450, 1300)
    wx, wy, wz = mask(p4, 'r', -1300, 1220), mask(p4, 'g', -1300, 1300), mask(p4, 'b', -1300, 1380)
    sumw = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionAdd, wx, wy, -1150, 1260), wz, -1000, 1300)

    def blend3(a, b, c, y):
        s = op(unreal.MaterialExpressionAdd,
               op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, a, wx, -1300, y - 100),
                  op(unreal.MaterialExpressionMultiply, b, wy, -1300, y), -1150, y - 50),
               op(unreal.MaterialExpressionMultiply, c, wz, -1300, y + 100), -1000, y)
        return op(unreal.MaterialExpressionDivide, s, sumw, -850, y)

    col = blend3(mask(cx, 'rgb', -1500, -600), mask(cy, 'rgb', -1500, -300), mask(cz, 'rgb', -1500, 0), -300)
    colB = op(unreal.MaterialExpressionMultiply, col, const(bright, -1000, -150), -700, -300)
    mel.connect_material_property(colB, '', unreal.MaterialProperty.MP_BASE_COLOR)

    # relief: each projection's tangent normal turned into world space (its picture's u, v and the face's own axis),
    # the face's side taken from the vertex normal's sign; blended and normalised
    sgn = E(unreal.MaterialExpressionSign, -1750, 1500); mel.connect_material_expressions(vn, '', sgn, '')
    sx_, sy_, sz_ = mask(sgn, 'r', -1600, 1450), mask(sgn, 'g', -1600, 1500), mask(sgn, 'b', -1600, 1550)
    rel = const(relief, -1600, 1650)

    def comp(n, c, y): return op(unreal.MaterialExpressionMultiply, mask(n, c, -1500, y), rel, -1400, y)
    def zc(n, s, y): return op(unreal.MaterialExpressionMultiply, mask(n, 'b', -1500, y), s, -1400, y)
    # along X: picture u = world Y, v = world -Z, out = world X
    wnx = app(app(zc(nx, sx_, 300), comp(nx, 'r', 340), -1250, 320), op(unreal.MaterialExpressionMultiply, comp(nx, 'g', 380), const(-1.0, -1500, 420), -1300, 380), -1100, 340)
    # along Y: u = world X, v = world -Z, out = world Y
    wny = app(app(comp(ny, 'r', 600), zc(ny, sy_, 640), -1250, 620), op(unreal.MaterialExpressionMultiply, comp(ny, 'g', 680), const(-1.0, -1500, 720), -1300, 680), -1100, 640)
    # from above: u = world X, v = world -Y, out = world Z
    wnz = app(app(comp(nz, 'r', 900), op(unreal.MaterialExpressionMultiply, comp(nz, 'g', 940), const(-1.0, -1500, 980), -1300, 940), -1250, 920), zc(nz, sz_, 1000), -1100, 940)
    nb = blend3(wnx, wny, wnz, 640)
    nn = E(unreal.MaterialExpressionNormalize, -700, 640); mel.connect_material_expressions(nb, '', nn, '')
    mel.connect_material_property(nn, '', unreal.MaterialProperty.MP_NORMAL)
    m.set_editor_property('tangent_space_normal', False)

    mel.connect_material_property(const(rough, -700, 900), '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const(metal, -700, 980), '', unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(const(0.4, -700, 1060), '', unreal.MaterialProperty.MP_SPECULAR)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m, only_if_is_dirty=False)
    unreal.log('ADP_HANGAR: M_Hangar%s' % name)


for k, v in SETS.items():
    make(k, *v)
unreal.log('ADP_HANGAR: materials done')
