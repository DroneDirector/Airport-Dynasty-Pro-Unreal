# The tax free's glowing finishes (owner 2026-11-17): the neon sign and the promenade's floor lines (bright teal), and the
# aurora's ribbons (teal, lilac). Copies of the terminal's unlit light MI_T_l_FFFFFF (M_TUnlit: (Base * BC + Emi * EC) * EB,
# so the glow is the colour BC times the brightness EB - above 1 it blooms as neon does).
import unreal
EAL = unreal.EditorAssetLibrary; MEL = unreal.MaterialEditingLibrary; M = '/Game/Terminal/Materials/'
for name, col, eb in (('MI_T_neonTeal', (0.25, 1.0, 0.82), 9.0), ('MI_T_neonPink', (1.0, 0.04, 0.5), 6.0), ('MI_T_neonPink50', (1.0, 0.04, 0.5), 3.0), ('MI_T_neonPink35', (1.0, 0.04, 0.5), 2.1),   # (the flicker's dips)
                         # neon pink: the sign and the promenade's lines (owner 2026-11-17: 'very poppy pink')
                       ('MI_T_auroraTeal', (0.25, 1.0, 0.82), 3.5), ('MI_T_auroraLilac', (0.7, 0.55, 1.0), 3.5)):
    if EAL.does_asset_exist(M + name): EAL.delete_asset(M + name)
    m = EAL.duplicate_asset(M + 'MI_T_l_FFFFFF', M + name)
    MEL.set_material_instance_vector_parameter_value(m, 'BC', unreal.LinearColor(col[0], col[1], col[2], 1.0))
    MEL.set_material_instance_scalar_parameter_value(m, 'EB', eb)
    MEL.update_material_instance(m); EAL.save_loaded_asset(m, only_if_is_dirty=False); unreal.log('ADP_NEON ' + name)
unreal.SystemLibrary.quit_editor()
