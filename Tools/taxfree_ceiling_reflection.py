# The tax free's ceiling reflections (owner, 2026-11-19: "casting a whole lot ... put the ceiling's reflection down to
# 15 %"): its dark walnut slats were lacquered (clear coat 0.7, metallic 0.15, roughness 0.32) and the dark ceiling over
# them metallic 0.6 - both now about 15 %: no lacquer, metallic 0, roughness 0.6 (the scale used for the escalators:
# 0.95 ~ 1 %, 0.9 ~ 3 %, 0.6 ~ 15 %). MI_T_tf_walnut is the tax free's own; the shared MI_T_dark gets a tax free copy,
# MI_T_tf_dark, put on the tax free's meshes (tf_*) only.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/taxfree_ceiling_reflection.py" -unattended -nullrhi
import unreal, os, stat
EL = unreal.EditorAssetLibrary; ML = unreal.MaterialEditingLibrary
TM = '/Game/Terminal/Materials/'
def log(*a): unreal.log('ADP_TFREF ' + ' '.join(str(x) for x in a))
def writable(path):
    f = os.path.join(unreal.Paths.project_content_dir(), path.replace('/Game/', '') + '.uasset')
    if os.path.exists(f): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
def low(mi):
    for n, v in (('M', 0.0), ('R', 0.6), ('CC', 0.0)): ML.set_material_instance_scalar_parameter_value(mi, n, v)
    ML.update_material_instance(mi); EL.save_loaded_asset(mi, only_if_is_dirty=False); log(mi.get_name(), 'M 0 R 0.6 CC 0')
writable(TM + 'MI_T_tf_walnut'); low(unreal.load_asset(TM + 'MI_T_tf_walnut'))
if not EL.does_asset_exist(TM + 'MI_T_tf_dark'): EL.duplicate_asset(TM + 'MI_T_dark', TM + 'MI_T_tf_dark')
writable(TM + 'MI_T_tf_dark'); dark = unreal.load_asset(TM + 'MI_T_tf_dark'); low(dark)
for a in EL.list_assets('/Game/Terminal/Meshes', recursive=False):
    nm = a.split('/')[-1].split('.')[0]
    if not nm.startswith('tf_'): continue
    sm = unreal.load_asset(a)
    if not isinstance(sm, unreal.StaticMesh): continue
    mats = list(sm.get_editor_property('static_materials')); hit = False
    for m in mats:
        mi = m.get_editor_property('material_interface')
        if mi and mi.get_name() == 'MI_T_dark': m.set_editor_property('material_interface', dark); hit = True
    if hit:
        writable(a.split('.')[0]); sm.set_editor_property('static_materials', mats); EL.save_loaded_asset(sm, only_if_is_dirty=False); log(nm, 'dark -> MI_T_tf_dark')
log('done')
