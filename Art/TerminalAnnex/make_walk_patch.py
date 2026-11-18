# The gate walkway's south wall rebuilt round the arrivals escalators' opening (owner, 2026-11-18: "the wall in front of
# the escalator needs to go ... sit flush, no openings"). The terminal's own wall there was one long face pair from x -80
# to x 6 (plaster 52.03..52.15 on the walkway side, white 51.92..52.0 on the other); Data/terminal-cuts.json takes those
# faces out ("touch") and this piece puts the wall back from x -68.2 to 6, the same planes and finishes, with a plaster
# jamb at the opening, the strip over the opening up to the vault's foot (11.4..12.0) and a terrazzo threshold flush with
# the landing and the walkway floor. Plan metres (x east, y up, z north).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_walk_patch.py
import bpy, bmesh, os
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(HERE, 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
S = Piece('walk_patch_solid')
OX0, OX1 = -80.0, -68.2                 # the opening
X1 = 6.0                                # the old faces' east end
F2 = 6.0
# the wall east of the opening: white face (south) at 51.92, plaster face (walkway) at 52.15, the core between
S.box(OX1, X1, F2, 12.0, 51.92, 52.0, 'MI_T_white', sides=('-z',))
S.box(OX1, X1, F2, 11.4, 52.03, 52.15, 'MI_T_plaster', sides=('+z',))
S.box(OX1, OX1 + 0.01, F2, 12.0, 51.92, 52.15, 'MI_T_plaster', sides=('-x',))        # the jamb, facing into the opening
# over the opening: the strip from the ceilings (11.4) up to the vault's foot (12.0), both faces
S.box(OX0, OX1, 11.4, 12.0, 51.92, 52.0, 'MI_T_white', sides=('-z', '-y'))
S.box(OX0, OX1, 11.4, 11.42, 52.0, 52.15, 'MI_T_plaster', sides=('-y',))
# the threshold: the landing's terrazzo carried across where the wall stood, flush with the floors either side
S.box(OX0, OX1, F2 - 0.02, F2 + 0.01, 51.9, 52.2, 'MI_T_terrazzo', sides=('+y',))
o = S.finish()
print('PATCH', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
for x in bpy.context.selected_objects: x.select_set(False)
o.select_set(True); bpy.context.view_layer.objects.active = o
bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                         apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
print('PATCH done')
