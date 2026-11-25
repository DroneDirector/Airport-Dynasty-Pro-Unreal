# The apron bus's plain materials given real surfaces (to-do 277), after Tools/import_model.py:
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/set_apronbus_materials.py" -unattended -nullrhi
# Grain, weave and wear from pictures already in the project (the coach's, the taxi bus's); its lenses, light strips and
# glass get the fire truck's at run time (ApronBus.cpp). Its screens are dark: it is parked, switched off.
# Says ADP_ABMAT for each, and how many material slots of its meshes are empty (must be 0).
import os, stat
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EAL = unreal.EditorAssetLibrary; MEL = unreal.MaterialEditingLibrary
D = '/Game/Babylon/ApronBus/apronbus'; C = '/Game/Babylon/TaxiBus/taxibus/Textures/'; A = D + '/Textures/'
P_N = A + 'Plastic006_2K-JPG_NormalGL'; P_R = A + 'Plastic006_2K-JPG_Roughness'
M_C = A + 'Metal027_2K-JPG_Color'; M_N = A + 'Metal027_2K-JPG_NormalGL'; M_R = A + 'Metal027_2K-JPG_Metalness-Metal027_2K-JPG_Roughness'
L_N = A + 'Paint004_2K-JPG_NormalGL'; L_R = A + 'Paint004_2K-JPG_Roughness'
# material: colour picture, relief, roughness, repeats a metre, colour over it (None: its own), strength of the relief, roughness
SET = {
    'CeilingLining': (None, C + 'T_Coach_Shell_N', P_R, 2, None, 0.6, 1),
    'BatteryPod': (None, L_N, L_R, 2, None, 1, 1),
    'SeatShell': (None, C + 'T_Coach_Shell_N', P_R, 3, None, 1, 1),
    'Stainless': (M_C, M_N, M_R, 2, None, 0.5, 0.7),
    'BlackPlastic': (None, C + 'T_Coach_BlackPlastic_N', P_R, 4, None, 1, 1),
    'GreenButton': (None, P_N, P_R, 6, None, 0.4, 0.7),
    'YellowGrip': (None, P_N, P_R, 5, None, 0.5, 0.9),
    'RedPaint': (None, L_N, L_R, 2, None, 1, 1),
    'DriverSeat': (None, C + 'T_Coach_Moquette_N', None, 4, None, 1, 0.75),
    'FloorVinyl': (C + 'T_Coach_FloorVinyl', C + 'T_Coach_FloorVinyl_N', None, 1.5, (1, 1, 1), 1, 0.65),
    'SeatFabric': (C + 'T_Coach_Moquette', C + 'T_Coach_Moquette_N', None, 3, (1, 1, 1), 1, 0.85),
}
DARK = ('Screen', 'DestinationLED', 'LEDStrip')
def log(m): unreal.log('ADP_ABMAT ' + m)
def free(p):
    f = os.path.join(ROOT, 'Content', *p[len('/Game/'):].split('/')) + '.uasset'
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
for k, (bc, n, r, rep, col, ns, rf) in SET.items():
    p = D + '/Materials/AB_' + k; m = unreal.load_asset(p)
    if not m: log('%s MISSING' % k); continue
    free(p)
    names = {str(x) for x in MEL.get_texture_parameter_names(m)}; sn = {str(x) for x in MEL.get_scalar_parameter_names(m)}; vn = {str(x) for x in MEL.get_vector_parameter_names(m)}
    bad = []
    for par, t in (('BaseColorTexture', bc), ('NormalTexture', n), ('MetallicRoughnessTexture', r)):
        if not t: continue
        tx = unreal.load_asset(t)
        if not tx or par not in names: bad.append('%s (%s)' % (par, 'no picture' if not tx else 'no parameter')); continue
        MEL.set_material_instance_texture_parameter_value(m, par, tx)
        if MEL.get_material_instance_texture_parameter_value(m, par) != tx: bad.append(par + ' not taken')
        if par + '_OffsetScale' in vn: MEL.set_material_instance_vector_parameter_value(m, par + '_OffsetScale', unreal.LinearColor(0, 0, rep, rep))
        else: bad.append(par + '_OffsetScale missing')
    if col: MEL.set_material_instance_vector_parameter_value(m, 'BaseColorFactor', unreal.LinearColor(col[0], col[1], col[2], 1))
    for par, v in (('NormalScale', ns), ('RoughnessFactor', rf)):
        if par in sn: MEL.set_material_instance_scalar_parameter_value(m, par, v)
        else: bad.append(par + ' missing')
    MEL.update_material_instance(m); EAL.save_loaded_asset(m); log('%s %s' % (k, 'ok' if not bad else 'PROBLEM ' + ', '.join(bad)))
for k in DARK:
    p = D + '/Materials/AB_' + k; m = unreal.load_asset(p)
    if not m: log('%s MISSING' % k); continue
    free(p); vn = {str(x) for x in MEL.get_vector_parameter_names(m)}
    if 'EmissiveFactor' in vn: MEL.set_material_instance_vector_parameter_value(m, 'EmissiveFactor', unreal.LinearColor(0, 0, 0, 1))
    MEL.update_material_instance(m); EAL.save_loaded_asset(m); log('%s dark %s' % (k, 'ok' if 'EmissiveFactor' in vn else 'PROBLEM no EmissiveFactor'))
empty = meshes = 0; used = {}
for a in EAL.list_assets(D + '/StaticMeshes', False):
    s = unreal.load_asset(a.split('.')[0])
    if not isinstance(s, unreal.StaticMesh): continue
    meshes += 1
    for sm in s.static_materials:
        mi = sm.material_interface
        if not mi: empty += 1; log('EMPTY ' + a)
        else: used[mi.get_name()] = used.get(mi.get_name(), 0) + 1
log('%d meshes, %d empty slots, materials used %s' % (meshes, empty, ' '.join('%s:%d' % x for x in sorted(used.items()))))
