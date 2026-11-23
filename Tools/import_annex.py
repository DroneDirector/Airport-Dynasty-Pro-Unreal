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
# (Git LFS leaves committed .uasset files read-only: Unreal then can't replace them and the import fails silently -
#  the game kept an old copy of the new parts; the flag is cleared first)
import glob, stat
for _f in glob.glob(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Content', 'Terminal/Meshes', '*.uasset')):
    os.chmod(_f, stat.S_IWRITE | stat.S_IREAD)

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
    # (the wave roof's own smooth normals, as made: Unreal's computed ones would differ at each bow's ends and show the joint)
    if name.startswith('wave_'): sd.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'TerminalAnnex', name + '.fbx')
    t.destination_path = '/Game/Terminal/Meshes'; t.automated = True; t.replace_existing = True; t.save = True; t.options = o
    AT.import_asset_tasks([t])
    sm = unreal.load_asset(path)
    mats = []
    for m in sm.get_editor_property('static_materials'):
        slot = str(m.get_editor_property('material_slot_name'))
        # the floors' own masters by name (the walkway's matte oak, the polished marble), else the terminal finish of that name
        EXTRA = {'M_WoodFloor': '/Game/Materials/WoodFloor/M_WoodFloor', 'M_Marble': '/Game/Materials/Marble/M_Marble',
                 'M_PillarStone': '/Game/Materials/PillarStone/M_PillarStone'}
        mi = (unreal.load_asset(EXTRA[slot]) if slot in EXTRA else None) or unreal.load_asset('/Game/Terminal/Materials/' + slot) or unreal.load_asset('/Game/Terminal/Materials/MI_T_white')
        m.set_editor_property('material_interface', mi); mats.append(m)
    sm.set_editor_property('static_materials', mats)
    # (the tax free's floor: its thin curved neon lines and promenade edge stay exact without Nanite - owner 2026-11-17)
    # (2026-11-21: the set_editor_property below had slipped into the comment above it, so the setting never stuck)
    ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', 'glass' not in name and name not in ('tf_floor', 'tf_aurora'))
    # (the wave roof: its Nanite backup mesh in full, as Tools/set_nanite_fallback.py did for the old roof - Unreal cuts it to 3 %, and
    #  Lumen's distance field and lighting are made from it)
    if name.startswith('wave_'):
        ns.set_editor_property('fallback_target', unreal.NaniteFallbackTarget.PERCENT_TRIANGLES); ns.set_editor_property('fallback_percent_triangles', 1.0); ns.set_editor_property('fallback_relative_error', 0.0)
    sm.set_editor_property('nanite_settings', ns)
    bs = sm.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EAL.save_loaded_asset(sm)
    unreal.log('ADP_ANNEX %s slots %s' % (name, [str(m.get_editor_property('material_slot_name')) for m in mats]))
    # the material slot check (memory: material-slot-check): every slot filled, after the save
    empty = [str(m.get_editor_property('material_slot_name')) for m in sm.get_editor_property('static_materials') if not m.get_editor_property('material_interface')]
    if empty: unreal.log_error('ADP_ANNEX %s EMPTY SLOTS %s' % (name, empty))
    unreal.log('ADP_ANNEX %s nanite %s' % (name, sm.get_editor_property('nanite_settings').get_editor_property('enabled')))
unreal.log('ADP_ANNEX done')
