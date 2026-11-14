# The true-Unreal terminal (to-do 42), part 1: what the Babylon-exported terminal is made of, written to
# docs/terminal-inventory.json for the conversion plan: every material (its master, blend, textures, colour factors)
# and every mesh (triangles, material slots, size), so the materials can be sorted into proper Unreal masters and the
# biggest merged meshes found.
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/terminal_inventory.py" -unattended -nullrhi
import unreal, json, os
eal = unreal.EditorAssetLibrary
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = '/Game/Babylon/Terminal/terminal'
out = {'materials': [], 'meshes': []}
for p in eal.list_assets(BASE + '/Materials', recursive=False, include_folder=False):
    m = unreal.load_asset(p)
    if not isinstance(m, unreal.MaterialInstanceConstant): continue
    par = m.get_editor_property('parent')
    tex = {}
    for tp in m.get_editor_property('texture_parameter_values'):
        t = tp.get_editor_property('parameter_value')
        tex[str(tp.get_editor_property('parameter_info').get_editor_property('name'))] = t.get_name() if t else None
    vec = {}
    for vp in m.get_editor_property('vector_parameter_values'):
        c = vp.get_editor_property('parameter_value')
        vec[str(vp.get_editor_property('parameter_info').get_editor_property('name'))] = [round(c.r, 3), round(c.g, 3), round(c.b, 3), round(c.a, 3)]
    sca = {}
    for sp in m.get_editor_property('scalar_parameter_values'):
        sca[str(sp.get_editor_property('parameter_info').get_editor_property('name'))] = round(sp.get_editor_property('parameter_value'), 3)
    out['materials'].append({'name': m.get_name(), 'parent': par.get_name() if par else None, 'tex': tex, 'vec': vec, 'scalar': sca})
sml = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) if hasattr(unreal, 'StaticMeshEditorSubsystem') else None
for p in eal.list_assets(BASE + '/StaticMeshes', recursive=False, include_folder=False):
    s = unreal.load_asset(p)
    if not isinstance(s, unreal.StaticMesh): continue
    try: tris = s.get_num_triangles(0)
    except Exception: tris = -1
    bb = s.get_bounding_box()
    size = bb.max - bb.min
    mats = [sm.get_editor_property('material_interface').get_name() if sm.get_editor_property('material_interface') else None for sm in s.get_editor_property('static_materials')]
    out['meshes'].append({'name': s.get_name(), 'tris': tris, 'size': [round(size.x), round(size.y), round(size.z)], 'mats': mats})
dst = os.path.join(ROOT, 'docs', 'terminal-inventory.json')
json.dump(out, open(dst, 'w'), indent=0)
unreal.log('ADP_INVENTORY %d materials, %d meshes -> %s' % (len(out['materials']), len(out['meshes']), dst))
unreal.SystemLibrary.quit_editor()
