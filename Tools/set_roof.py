# The roof tops in the owner's standing-seam zinc (owner, 2026-11-14: Art/Roof/roof_zinc_seams_source.jpg, tileable): on
# top of the terminal's wave roof (ROOF/WAVE-TOP, terminal_mesh_997) and the car rental's big roof
# (RENTAL/STRUCTURE-rentalRoofTop, terminal_mesh_2726) - never the undersides. Used as is, never stretched: the photo is
# 22 panels of about 50 cm, so one tile is 11 x 6 m on the roof; the roofs get UVs in metres measured along the roof
# itself (an angle-true unwrap, every piece at its true size, the seams running down the slope: within 5% everywhere on
# the doubly curved terminal roof, 1% on the rental's), the material scaling metres to tiles.
#   Tools/make_roof.sh runs it all:
#   1. this, "dump"   - the two meshes' corner positions to Art/Roof/roof_positions.json
#   2. Tools/roof_uvs_arc.py (python), Tools/roof_unwrap.py (Blender) - Art/Roof/roof_uvs.json
#   3. this, "apply"  - the texture, MI_T_roofZinc, the UVs into the meshes, the material on them
import os, sys, json, stat
import unreal
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ART = os.path.join(ROOT, 'Art', 'Roof')
MESHES = ('terminal_mesh_997', 'terminal_mesh_2726')
TILE_W, TILE_H = 11.0, 6.0     # metres one tile covers on the roof (22 panels x 0.5 m; the photo's 1408 x 768 aspect)
MODE = 'apply' if 'apply' in ' '.join(sys.argv) else 'dump'
def log(m): unreal.log('ADP_ROOF ' + m)

def writable(*parts):
    """the repo's Git LFS 'lockable' leaves committed .uasset files read-only: saves would fail quietly"""
    p = os.path.join(ROOT, 'Content', *parts)
    if os.path.exists(p): os.chmod(p, stat.S_IWRITE | stat.S_IREAD)

def vid(i):
    for f in (lambda: unreal.VertexInstanceID(i), lambda: unreal.VertexInstanceID(id_value=i)):
        try:
            v = f()
            if v.id_value == i: return v
        except Exception: pass
    raise RuntimeError('VertexInstanceID %d' % i)

if MODE == 'dump':
    out = {}
    for name in MESHES:
        d = unreal.load_asset('/Game/Terminal/Meshes/' + name).get_static_mesh_description(0)
        out[name] = [[p.x, p.y, p.z] for p in (d.get_vertex_position(d.get_vertex_instance_vertex(vid(i))) for i in range(d.get_vertex_instance_count()))]
        log('%s: %d corners' % (name, len(out[name])))
    json.dump(out, open(os.path.join(ART, 'roof_positions.json'), 'w'))
else:
    UV = json.load(open(os.path.join(ART, 'roof_uvs.json')))
    writable('Terminal', 'Textures', 'T_Roof_Zinc.uasset'); writable('Terminal', 'Materials', 'MI_T_roofZinc.uasset')
    t = unreal.AssetImportTask(); t.filename = os.path.join(ART, 'T_Roof_Zinc.png'); t.destination_path = '/Game/Terminal/Textures'
    t.destination_name = 'T_Roof_Zinc'; t.automated = True; t.replace_existing = True; t.save = False
    tools.import_asset_tasks([t])
    tex = unreal.load_asset('/Game/Terminal/Textures/T_Roof_Zinc'); tex.set_editor_property('srgb', True)
    eal.save_loaded_asset(tex, only_if_is_dirty=False)
    P = '/Game/Terminal/Materials/MI_T_roofZinc'
    mi = unreal.load_asset(P) if eal.does_asset_exist(P) else tools.create_asset('MI_T_roofZinc', '/Game/Terminal/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, unreal.load_asset('/Game/Terminal/Masters/M_TSurface'))
    mel.set_material_instance_texture_parameter_value(mi, 'Base', tex)
    mel.set_material_instance_vector_parameter_value(mi, 'BC', unreal.LinearColor(1, 1, 1, 1))          # (the photo as is)
    mel.set_material_instance_vector_parameter_value(mi, 'OS', unreal.LinearColor(0, 0, 1.0 / TILE_W, 1.0 / TILE_H))
    mel.set_material_instance_scalar_parameter_value(mi, 'R', 0.5)       # weathered zinc: a soft sheen
    mel.set_material_instance_scalar_parameter_value(mi, 'M', 0.0)
    mel.update_material_instance(mi); eal.save_loaded_asset(mi, only_if_is_dirty=False)
    for name in MESHES:
        writable('Terminal', 'Meshes', name + '.uasset')
        m = unreal.load_asset('/Game/Terminal/Meshes/' + name)
        d = m.get_static_mesh_description(0); uv = UV[name]
        if d.get_vertex_instance_count() != len(uv): raise RuntimeError('%s: %d corners, %d UVs - run the dump again' % (name, d.get_vertex_instance_count(), len(uv)))
        for i, (u, v) in enumerate(uv): d.set_vertex_instance_uv(vid(i), unreal.Vector2D(u, v), 0)
        m.build_from_static_mesh_descriptions([d])
        m.set_material(0, mi)
        eal.save_loaded_asset(m, only_if_is_dirty=False)
        f = os.path.join(ROOT, 'Content', 'Terminal', 'Meshes', name + '.uasset')
        if os.path.getmtime(f) < os.path.getmtime(os.path.join(ART, 'roof_uvs.json')): unreal.log_error('ADP_ROOF CHECK FAILED: %s not saved' % name)
        else: log('%s: roof zinc on, %d corners' % (name, len(uv)))
unreal.SystemLibrary.quit_editor()
