# The arrivals wing (to-do 72, owner 2026-11-16; design: docs/Baggage-Claim-Concept.md, renders docs/Pictures/BaggageClaim,
# blueprints docs/Baggage-Claim-*Blueprint.png): the museum hall's place and the free ground west of it, flush with the
# end of the gates' pier - x -80..0, z -4..52 - in the terminal plan's metres and the terminal's own materials (slots
# named after /Game/Terminal/Materials; MI_T_basalt and MI_T_cedar made by Tools/make_claim_materials.py). The shell:
#   the claim hall (z 16..52) and the arrivals hall (z -4..8) under oak slats at 5.4 m with LED lines; customs between
#   (z 8..16: the green channel x -44..-36 straight through, the red channel x -54..-48; toilets west, its office east);
#   the dark basalt wall along the east side (x 0) with the store's door; glass fronts to 5.4 m on the kerb side (z -4,
#   the openings for the two sliding doors the layout places) and the west end (x -80); above them zinc cladding to the
#   12 m roof; the escalators' void (x -80..-68.2, z 34.5..52) open up to Floor 2's ceiling (11.4) with its cedar wall
#   on the west, its Floor 2 landing (x -76.1..-68.2 from z 49.76) at the gate walkway's new doorway; the floor with each
#   belt's feed-shaft opening (the belts themselves come in their own models).
# Blender axes for the game's import: (x, y, z) = (-plan x, -plan z, plan y).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_arrivals_wing.py
import bpy, bmesh, json, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
# the west end's own builders (Piece, wall_x, wall_z, slab, glass_front_z) - one set of rules for every new part
src = open(os.path.join(HERE, 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
S = Piece('annex_arrivals_solid'); G = Piece('annex_arrivals_glass')
W, T = 'MI_T_white', 0.2
exec(src[src.index('def wall_x('):src.index('\n', src.index("        S.box(x0, x1, ty - 0.04, ty + 0.04, z - 0.075, z + 0.075, 'MI_T_mullion')")) + 1])

X0, X1, ZK, Z16, ZN = -80.0, 0.0, -4.0, 16.0, 52.0       # the wing's west end, east side, kerb front, claim hall's south wall, north wall
CEIL, F2, ROOF = 5.4, 6.0, 12.0
VOID = (-80.0, -68.2, 34.5, 52.0)                          # the escalators' void (x0, x1, z0, z1)
LAND = 37.46 + 12.3                                        # the Floor 2 landing's edge (the escalators' foot + their top flat)
BELTS = [-60.8, -48.15, -35.5, -22.85, -10.2]; BCZ = 31.0  # the belts' middles (their shafts at x -0.45..+1.4, z +5.4..+6.6)

# ---- the floor: terrazzo, with each belt's feed-shaft opening ----
holes = [(cx - 0.45, cx + 1.4, BCZ + 5.4, BCZ + 6.6) for cx in BELTS]
xs = sorted({X0, X1} | {h[0] for h in holes} | {h[1] for h in holes}); zs = sorted({ZK, ZN} | {h[2] for h in holes} | {h[3] for h in holes})
for xa, xb in zip(xs, xs[1:]):
    for za, zb in zip(zs, zs[1:]):
        if any(h[0] <= xa and xb <= h[1] and h[2] <= za and zb <= h[3] for h in holes): continue
        slab(xa, xb, za, zb, 0.01, top='MI_T_terrazzo', under='MI_T_concrete', thick=0.3)

# ---- the east side (x 0): the basalt wall, full height, with the store's door (x 0..6, z 30..52 behind it) ----
wall_x(-0.1, ZK, ZN, 0.01, ROOF, m_in='MI_T_basalt', holes=[(40.0, 41.4, 0.0, 2.3)])
# ---- the kerb front (z -4): glass to 5.4 m, mullions every 2.5 m, the two door openings (x -46..-41, -39..-34) ----
DOORS = [(-46.0, -41.0), (-39.0, -34.0)]
spans = [(X0, DOORS[0][0]), (DOORS[0][1], DOORS[1][0]), (DOORS[1][1], X1)]
for a_, b_ in spans:
    G.box(a_, b_, 0.01, CEIL, ZK - 0.012, ZK + 0.012, 'MI_T_glass')
    n = max(1, round((b_ - a_) / 2.5)); st = (b_ - a_) / n
    for i in range(n + 1): S.box(a_ + i * st - 0.04, a_ + i * st + 0.04, 0.01, CEIL, ZK - 0.075, ZK + 0.075, 'MI_T_mullion')
for a_, b_ in DOORS:
    G.box(a_, b_, 3.2, CEIL, ZK - 0.012, ZK + 0.012, 'MI_T_glass')          # glass over the doors
    S.box(a_, b_, 3.14, 3.2, ZK - 0.075, ZK + 0.075, 'MI_T_mullion')
S.box(X0, X1, 0.01, 0.06, ZK - 0.075, ZK + 0.075, 'MI_T_mullion'); S.box(X0, X1, CEIL - 0.04, CEIL + 0.04, ZK - 0.075, ZK + 0.075, 'MI_T_mullion')
# ---- the west end (x -80): glass to 5.4 m ----
G.box(X0 - 0.012, X0 + 0.012, 0.01, CEIL, ZK, ZN, 'MI_T_glass')
for i in range(int((ZN - ZK) / 2.5) + 1):
    z = min(ZN, ZK + i * 2.5); S.box(X0 - 0.075, X0 + 0.075, 0.01, CEIL, z - 0.04, z + 0.04, 'MI_T_mullion')
S.box(X0 - 0.075, X0 + 0.075, CEIL - 0.04, CEIL + 0.04, ZK, ZN, 'MI_T_mullion')
# ---- above the glass: zinc cladding to the roof (outside), the void's cedar wall inside ----
S.box(X0 - 0.2, X1, CEIL, ROOF - 0.4, ZK - 0.2, ZK, 'MI_T_roofZinc')
S.box(X0 - 0.2, X0, CEIL, ROOF - 0.4, ZK, ZN, 'MI_T_roofZinc', sides=('-x', '+y', '-y', '-z', '+z'))
S.box(X0, X0 + 0.06, CEIL - 0.15, 11.4, VOID[2], ZN, 'MI_T_cedar', sides=('+x',))
# ---- the roof (12 m) and its edge ----
S.box(X0 - 0.4, X1, ROOF - 0.4, ROOF, ZK - 0.4, ZN, 'MI_T_roofZinc')   # (its top at 12.0: under the wave roof's lowest point, 12.3)
S.box(X0 - 0.42, X1, ROOF - 0.45, ROOF + 0.05, ZK - 0.42, ZK - 0.38, 'MI_T_black'); S.box(X0 - 0.42, X0 - 0.38, ROOF - 0.45, ROOF + 0.05, ZK - 0.4, ZN, 'MI_T_black')

# ---- customs (z 8..16) and the claim hall's south wall (z 16) ----
wall_z(Z16, X0, X1, 0.01, CEIL, m='MI_T_plaster', holes=[(-44.0, -36.0, 0.0, 3.0)])
wall_z(8.0, X0, X1, 0.01, CEIL, m='MI_T_plaster', holes=[(-44.0, -36.0, 0.0, 3.0), (-54.0, -48.0, 0.0, 3.0), (-70.6, -69.4, 0.0, 2.3), (-12.6, -11.4, 0.0, 2.3)])
wall_x(-56.0, 8.0, Z16, 0.01, CEIL, m_in='MI_T_plaster'); wall_x(-24.0, 8.0, Z16, 0.01, CEIL, m_in='MI_T_plaster')
S.box(-44.0, -36.0, 0.01, 0.014, 8.1, 15.9, 'MI_T_l_20FF40')       # the green channel's floor strip
S.box(-54.0, -48.0, 0.01, 0.014, 8.1, 15.9, 'MI_T_l_FF2020')       # the red channel's

# ---- the ceilings at 5.4: oak slats (6 x 12 cm, every 12 cm) over a dark plenum, LED lines every 3 m; none over the void ----
def slats(x0, x1, z0, z1):
    z = z0 + 0.05
    while z + 0.06 <= z1:
        S.box(x0, x1, CEIL - 0.12, CEIL, z, z + 0.06, 'MI_T_oak', sides=('-y', '-z', '+z')); z += 0.12
    S.box(x0, x1, CEIL + 0.3, CEIL + 0.34, z0, z1, 'MI_T_dark', sides=('-y',))
for (x0, x1, z0, z1) in ((X0, X1, ZK, VOID[2]), (VOID[1], X1, VOID[2], ZN)): slats(x0, x1, z0, z1)
for z in [ZK + 3.0 + 3.0 * k for k in range(18)]:
    x0 = X0 if z < VOID[2] else VOID[1]
    S.box(x0, X1, CEIL - 0.13, CEIL - 0.11, z - 0.02, z + 0.02, 'MI_T_l_FFFFFF')
# ---- the void: its east and south walls up to Floor 2's ceiling, the ceiling, the Floor 2 landing ----
S.box(VOID[1], VOID[1] + 0.3, CEIL - 0.15, 11.4, VOID[2], ZN, 'MI_T_white')
S.box(X0, VOID[1] + 0.3, CEIL - 0.15, 11.4, VOID[2] - 0.3, VOID[2], 'MI_T_white')
S.box(X0, VOID[1] + 0.3, 11.4, 11.55, VOID[2], ZN, 'MI_T_offwhite', sides=('-y',))
slab(-76.1, VOID[1], LAND, ZN, F2 + 0.01, top='MI_T_terrazzo', under='MI_T_offwhite', thick=0.6)
slab(X0, -76.1, ZN - 0.1, ZN, F2 + 0.01, top='MI_T_terrazzo', under='MI_T_offwhite', thick=0.6)

so = S.finish(); go = G.finish()
for o in (so, go):
    print('ANNEX', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'ArrivalsWing.blend'))
print('ANNEX saved')
