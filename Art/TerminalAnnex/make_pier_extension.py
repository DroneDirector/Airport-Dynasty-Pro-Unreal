# The 16 gates (owner, 2026-11-17), part 1: the walls of the lengthened gate walkway (Floor 2) and baggage hall (Floor 1)
# at both ends - west x -241.1..-80, east x 242..551.2 - in the terminal plan's metres and the terminal's own materials,
# made as today's: Floor 2 walls 20 cm, white outside and plaster inside, from the floor (6.0) to the ceiling (11.4);
# Floor 1 walls white outside, concrete inside, to 6.0, the hall's concrete ceiling at 5.4; the floor of the baggage hall.
# Openings as today's gates have them (Data/terminal-plan.json doors): each gate room 8 m wide at its x0 + 12, each view
# bay 10 m at its x0 + 17, 3.6 m high; each ramp room's 2 m door into the baggage hall at its x0 + 14. On the walkway's
# south side in the west: the tax free's glass shop front with its way IN (east) and OUT (west), 10 m each; in the east
# the east wing's wall (empty for now). The new ends closed. Everything meets today's walls edge to edge (no overlap).
# The vault, slabs and gate rooms are copies of today's pieces (Tools/terminal_16gates.py).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_pier_extension.py
import bpy, bmesh, os
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(HERE, 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
S = Piece('pier_ext_solid'); G = Piece('pier_ext_glass')
W, T = 'MI_T_white', 0.2
exec(src[src.index('def wall_x('):src.index('# ============ the escalator room')])
MOD = 80.55
W_END, E_END = -80.0 - 2 * MOD, 551.2
WB_X = [289.4, 369.2, 441.5, 513.8]
F2, F2C, F1C = 6.0, 11.4, 5.4

def zwall(z, x0, x1, y0, y1, inside, m_in, m_out, holes=()):
    """A wall in the plane z (20 cm), inside face toward +z or -z; holes (x0, x1, y0, y1)."""
    cx = sorted({x0, x1} | {max(x0, min(x1, h[0])) for h in holes} | {max(x0, min(x1, h[1])) for h in holes})
    for xa, xb in zip(cx, cx[1:]):
        spans = [(y0, y1)]
        for h in holes:
            if h[0] <= xa + 1e-6 and xb <= h[1] + 1e-6:
                spans = [s for sp in spans for s in ((sp[0], min(sp[1], h[2])), (max(sp[0], h[3]), sp[1])) if s[1] - s[0] > 1e-4]
        for ya, yb in spans:
            sin, sout = ('+z', '-z') if inside > 0 else ('-z', '+z')
            S.box(xa, xb, ya, yb, z - T / 2, z + T / 2, m_out, sides=(sout, '+y', '-y'))
            S.box(xa, xb, ya, yb, z - T / 2, z + T / 2, m_in, sides=(sin,))
            for hx in (xa, xb):                                   # the reveals of an opening
                if any(abs(hx - h[0]) < 1e-6 or abs(hx - h[1]) < 1e-6 for h in holes) and hx not in (x0, x1):
                    S.box(hx, hx, ya, yb, z - T / 2, z + T / 2, m_out, sides=('+x',) if any(abs(hx - h[1]) < 1e-6 for h in holes) else ('-x',))

def xwall(x, z0, z1, y0, y1, inside, m_in, m_out):
    S.box(x - T / 2, x + T / 2, y0, y1, z0, z1, m_out, sides=('-x' if inside > 0 else '+x', '+y', '-y'))
    S.box(x - T / 2, x + T / 2, y0, y1, z0, z1, m_in, sides=('+x' if inside > 0 else '-x',))

def opening_x(x0_room, off, w):
    c = x0_room + off; return (c - w / 2, c + w / 2)

# the openings per end
west_gate_x0 = [-72.6 - k * MOD for k in (2, 1)]                    # F2 gate rooms (and F1 ramp rooms) at these x0
east_gate_x0 = [169.05 + (sx - 203.05) for sx in WB_X]
F2N = {'W': [], 'E': []}; F1N = {'W': [], 'E': []}
for side, x0s in (('W', west_gate_x0), ('E', east_gate_x0)):
    for x0 in x0s:
        a, b = opening_x(x0, 12.0, 8.0); F2N[side].append((a, b, F2, F2 + 3.6))          # gate room
        a, b = opening_x(x0 + 34.0, 17.0, 10.0); F2N[side].append((a, b, F2, F2 + 3.6))  # view bay
        if side == 'E' and abs(x0 - 255.4) < 0.01:
            # ramp services 13: the departures vehicle passage (owner 2026-11-21, Departure-Baggage-v2) - a 4 m roller
            # door's opening (ARollerDoor; its guides and hood: Art/TerminalAnnex/make_bag_block.py)
            a, b = opening_x(x0, 14.0, 4.0); F1N[side].append((a, b, 0.0, 4.1))
        else:
            a, b = opening_x(x0, 14.0, 2.0); F1N[side].append((a, b, 0.0, 2.3))          # ramp room door

for side, (x0, x1) in (('W', (W_END, -80.0)), ('E', (242.0, E_END))):
    # ---- Floor 2: the walkway ----
    zwall(68.0, x0, x1, F2, F2C, -1, 'MI_T_plaster', 'MI_T_white', F2N[side])          # north, onto the gate rooms
    if side == 'W':
        # the tax free's shop front: glass, way IN at its east end, way OUT at its west end
        # (owner, 2026-11-18: the tax free ends at x -119.15 now - its way IN moved to its new east end; the old way in
        #  (x -100..-90) stays open as the ESCALATOR HALL's way onto the walkway, from the arrivals escalators' landing)
        IN, OUT, HALL = (-135.09, -125.09), (W_END + 8.0, W_END + 18.0), (-100.0, -90.0)
        for a, b in ((W_END + 0.1, OUT[0]), (OUT[1], IN[0]), (IN[1], HALL[0]), (HALL[1], x1)):
            glass_front_z(52.0, a, b, F2, F2C, step=2.5, transoms=(F2 + 3.6,))
        for a, b in (IN, OUT, HALL):
            S.box(a, b, F2 + 3.6, F2C, 51.9, 52.1, 'MI_T_white')                       # the wall over each way through
    else:
        # the east wing's side, opened onto the Floor 2 concourse (terminal part 2a, 2026-11-23: Art/TerminalEast/conc_geom.py)
        import sys; sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'TerminalEast')); import conc_geom as CG_
        zwall(52.0, x0, x1, F2, F2C, +1, 'MI_T_plaster', 'MI_T_white', [(a_, b_, F2, h_) for a_, b_, h_ in CG_.holes() if a_ > x0])
    # ---- Floor 1: the baggage hall ----
    zwall(68.0, x0, x1, 0.0, F2, -1, 'MI_T_concrete', 'MI_T_white', F1N[side])
    zwall(52.0, x0, x1, 0.0, F2, +1, 'MI_T_concrete', 'MI_T_white')
    S.box(x0, x1, -0.3, 0.0, 52.1, 67.9, 'MI_T_concrete')                                # the floor
    S.box(x0, x1, F1C, F1C + 0.02, 52.1, 67.9, 'MI_T_concrete', sides=('-y',))           # its ceiling
# the new ends
for x, inside in ((W_END, +1), (E_END, -1)):
    xwall(x, 51.9, 68.1, 0.0, 12.2, inside, 'MI_T_plaster', 'MI_T_white')     # up to the vault's foot (12.17)
so = S.finish(); go = G.finish()
for o in (so, go):
    print('PIER', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'PierExtension.blend'))
print('PIER done')
