# The gates' curved windows (Art/GateArc/*.fbx, made in the gate's own frame) into the terminal's piece set, as
# Tools/import_east.py: /Game/Terminal/Meshes/<name>, every slot the terminal material of the same name, Nanite on for the
# solid parts (not the glass), smooth normals as made, their own shape as their collision; the floor's and the roof's distance
# fields two-sided (thin shells: one-sided, Lumen can take the space under them for solid - Tools/import_annex.py, the wave roof).
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/import_gatearc.py [name ...]" -unattended -nullrhi
import glob, os, stat, sys, time
import unreal
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EAL = unreal.EditorAssetLibrary; AT = unreal.AssetToolsHelpers.get_asset_tools()
names = [a for a in sys.argv[1:] if not a.endswith('.py')] or sorted(os.path.basename(f)[:-4] for f in glob.glob(os.path.join(ROOT, 'Art', 'GateArc', 'gatearc_*.fbx')))
# (Git LFS leaves committed .uasset files read-only: Unreal then can't replace them and the import fails silently)
for f in glob.glob(os.path.join(ROOT, 'Content', 'Terminal', 'Meshes', 'gatearc_*.uasset')): os.chmod(f, stat.S_IWRITE | stat.S_IREAD)
EXTRA = {'M_WoodFloor': '/Game/Materials/WoodFloor/M_WoodFloor'}
for name in names:
    path = '/Game/Terminal/Meshes/' + name
    if EAL.does_asset_exist(path): EAL.delete_asset(path)
    o = unreal.FbxImportUI()
    o.set_editor_property('import_mesh', True); o.set_editor_property('import_as_skeletal', False)
    o.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property('import_materials', False); o.set_editor_property('import_textures', False)
    sd = o.get_editor_property('static_mesh_import_data')
    sd.set_editor_property('combine_meshes', True); sd.set_editor_property('generate_lightmap_u_vs', False); sd.set_editor_property('auto_generate_collision', False)
    sd.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t = unreal.AssetImportTask(); t.filename = os.path.join(ROOT, 'Art', 'GateArc', name + '.fbx')
    t.destination_path = '/Game/Terminal/Meshes'; t.automated = True; t.replace_existing = True; t.save = True; t.options = o
    AT.import_asset_tasks([t])
    sm = unreal.load_asset(path); mats = []
    for m in sm.get_editor_property('static_materials'):
        slot = str(m.get_editor_property('material_slot_name'))
        mi = unreal.load_asset(EXTRA[slot]) if slot in EXTRA else unreal.load_asset('/Game/Terminal/Materials/' + slot)
        if not mi: unreal.log_error('ADP_GATEARC %s NO MATERIAL %s' % (name, slot))
        m.set_editor_property('material_interface', mi); mats.append(m)
    sm.set_editor_property('static_materials', mats)
    ns = sm.get_editor_property('nanite_settings'); ns.set_editor_property('enabled', 'glass' not in name)
    # (its backup mesh in full - the collision and Lumen use it: Tools/set_nanite_fallback.py's setting)
    ns.set_editor_property('fallback_target', unreal.NaniteFallbackTarget.PERCENT_TRIANGLES); ns.set_editor_property('fallback_percent_triangles', 1.0); ns.set_editor_property('fallback_relative_error', 0.0)
    sm.set_editor_property('nanite_settings', ns)
    bs = sm.get_editor_property('body_setup')
    if bs: bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    if 'roof' in name or 'floor' in name:
        _sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem); _bs = _sub.get_lod_build_settings(sm, 0)
        _bs.set_editor_property('generate_distance_field_as_if_two_sided', True); _bs.set_editor_property('distance_field_resolution_scale', 2.0); _sub.set_lod_build_settings(sm, 0, _bs)
    EAL.save_loaded_asset(sm)
    # the material slot check (memory: material-slot-check): every slot filled, after the save; the file really new
    empty = [str(m.get_editor_property('material_slot_name')) for m in sm.get_editor_property('static_materials') if not m.get_editor_property('material_interface')]
    age = time.time() - os.path.getmtime(os.path.join(ROOT, 'Content', 'Terminal', 'Meshes', name + '.uasset'))
    b = sm.get_bounding_box()
    unreal.log('ADP_GATEARC %s slots %s empty %s nanite %s, saved %.0f s ago, box %s %s' % (name, [str(m.get_editor_property('material_slot_name')) for m in mats], empty,
               sm.get_editor_property('nanite_settings').get_editor_property('enabled'), age, b.min, b.max))
    if empty or age > 120: unreal.log_error('ADP_GATEARC %s BAD' % name)
unreal.log('ADP_GATEARC done %d' % len(names))
# (what the room's own pieces wear, for the record: the new parts wear the same)
for m in (920, 925, 927, 928, 929, 930):
    sm = unreal.load_asset('/Game/Terminal/Meshes/terminal_mesh_%d' % m)
    unreal.log('ADP_GATEARC room piece %d: %s' % (m, [s.get_editor_property('material_interface').get_path_name().split('.')[-1] for s in sm.get_editor_property('static_materials')]))
