# New terminal parts (Art/TerminalAnnex/*.fbx, made in the plan's metres) into the terminal's piece set:
# /Game/Terminal/Meshes/<name>, every slot the terminal material of the same name (/Game/Terminal/Materials), Nanite on
# for the solid parts (not glass), their own shape as their collision. The layout places them (Tools/terminal_edit.py,
# a node with "asset": <name>).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_annex.py annex_west_solid annex_west_glass" -unattended -nullrhi
import os, sys
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
names = [a for a in sys.argv[1:] if not a.endswith('.py')]
for name in names:
    path = '/Game/Terminal/Meshes/' + name
    if EAL.does_asset_exist(path): EAL.delete_asset(path)
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', True); o.set_editor_property('import_as_skeletal', False)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property('import_materials', False); o.set_editor_property('import_textures', False)
    sd = o.get_editor_property('static_mesh_import_data')
    sd.set_editor_property('combine_meshes', True); sd.set_editor_property('generate_lightmap_u_vs', False)
    sd.set_editor_property('auto_generate_collision', False)
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'TerminalAnnex', name + '.fbx')
    t.destination_path = '/Game/Terminal/Meshes'; t.automated = True; t.replace_existing = True; t.save = True; t.options = o
    AT.import_asset_tasks([t])
    sm = unreal.load_asset(path)
    mats = []
    for m in sm.get_editor_property('static_materials'):
        slot = str(m.get_editor_property('material_slot_name'))
        mi = unreal.load_asset('/Game/Terminal/Materials/' + slot) or unreal.load_asset('/Game/Terminal/Materials/MI_T_white')
        m.set_editor_property('material_interface', mi); mats.append(m)
    sm.set_editor_property('static_materials', mats)
    ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', 'glass' not in name); sm.set_editor_property('nanite_settings', ns)
    bs = sm.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EAL.save_loaded_asset(sm)
    unreal.log('ADP_ANNEX %s slots %s' % (name, [str(m.get_editor_property('material_slot_name')) for m in mats]))
unreal.log('ADP_ANNEX done')
