# The crown lights over the escalators (owner, 2026-11-18: "more stunning ... hanging by wires, cable, and a real big
# nice crown light above them"): a 3.2 m brushed-stainless crown ring with 20 tapered crown points, a warm glowing LED
# band round its inside and glowing tips, four spokes to a hub, hung on four thin steel cables from ceiling roses. The
# light itself comes from the terminal's own lamp under the hub (a LAMP node in the layout: Tools/terminal_edit.py) -
# its glowing diffuser disc is the crown's underside. One piece per place (the cables' length differs).
# Plan metres, origin = the ring's middle at its bottom edge (x east, y up, z north).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_crown_light.py
import bpy, bmesh, os, math
HERE = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
src = open(os.path.join(HERE, 'make_annex_west.py'), encoding='utf-8').read()
exec(src[src.index('MATS = {}'):src.index("S = Piece('annex_west_solid')")])
R, BAND, N_PTS, PT_H = 1.6, 0.36, 20, 0.42
STEEL, GLOW, WARM, DARK = 'MI_T_liftBrushed', 'MI_T_l_FFFFFF', 'MI_T_l_FFF4D8', 'MI_T_black'

def quad(P, a, b, c, d, m, out):
    """a quad facing 'out' (plan axes) whatever the corner order"""
    P.want.append((P.face([a, b, c, d], m), (-out[0], -out[2], out[1])))
def ring_band(P, r, y0, y1, m, outward, n=96):
    for k in range(n):
        a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n; am = (a0 + a1) / 2
        o = (math.cos(am) * outward, 0.0, math.sin(am) * outward)
        quad(P, (r * math.cos(a0), y0, r * math.sin(a0)), (r * math.cos(a1), y0, r * math.sin(a1)), (r * math.cos(a1), y1, r * math.sin(a1)), (r * math.cos(a0), y1, r * math.sin(a0)), m, o)
def annulus(P, r0, r1, y, m, up, n=96):
    for k in range(n):
        a0, a1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n
        quad(P, (r0 * math.cos(a0), y, r0 * math.sin(a0)), (r1 * math.cos(a0), y, r1 * math.sin(a0)), (r1 * math.cos(a1), y, r1 * math.sin(a1)), (r0 * math.cos(a1), y, r0 * math.sin(a1)), m, (0, up, 0))
def rod(P, a, b, r, m, n=8):
    """a round rod from a to b (plan points)"""
    ax = [b[i] - a[i] for i in range(3)]; L = math.sqrt(sum(c * c for c in ax)); t = [c / L for c in ax]
    up = (0, 1, 0) if abs(t[1]) < 0.9 else (1, 0, 0)
    u = [up[1] * t[2] - up[2] * t[1], up[2] * t[0] - up[0] * t[2], up[0] * t[1] - up[1] * t[0]]; ul = math.sqrt(sum(c * c for c in u)); u = [c / ul for c in u]
    v = [t[1] * u[2] - t[2] * u[1], t[2] * u[0] - t[0] * u[2], t[0] * u[1] - t[1] * u[0]]
    for k in range(n):
        c0, c1 = 2 * math.pi * k / n, 2 * math.pi * (k + 1) / n; cm = (c0 + c1) / 2
        p = lambda base, c: tuple(base[i] + r * (math.cos(c) * u[i] + math.sin(c) * v[i]) for i in range(3))
        quad(P, p(a, c0), p(b, c0), p(b, c1), p(a, c1), m, tuple(math.cos(cm) * u[i] + math.sin(cm) * v[i] for i in range(3)))

def crown(name, cable):
    P = Piece(name)
    # the ring: a brushed band, its rolled lips top and bottom, a warm LED band round its inside
    ring_band(P, R, 0.0, BAND, STEEL, 1); ring_band(P, R - 0.03, 0.0, BAND, STEEL, -1)
    annulus(P, R - 0.03, R, 0.0, STEEL, -1); annulus(P, R - 0.03, R, BAND, STEEL, 1)
    ring_band(P, R + 0.012, BAND - 0.02, BAND + 0.02, STEEL, 1); ring_band(P, R + 0.012, -0.02, 0.02, STEEL, 1)
    ring_band(P, R - 0.035, 0.07, BAND - 0.07, WARM, -1)
    # the crown points: tapered fins standing on the ring, glowing at their tips (both faces)
    for k in range(N_PTS):
        a = 2 * math.pi * (k + 0.5) / N_PTS; ca, sa = math.cos(a), math.sin(a); w = 0.11
        tx, tz = -sa, ca                                    # along the ring
        h = PT_H * (1.0 if k % 2 == 0 else 0.7)             # tall and short points by turns, as a crown
        base0 = (R * ca - tx * w, BAND, R * sa - tz * w); base1 = (R * ca + tx * w, BAND, R * sa + tz * w)
        mid0 = (R * ca - tx * w * 0.35, BAND + h * 0.8, R * sa - tz * w * 0.35); mid1 = (R * ca + tx * w * 0.35, BAND + h * 0.8, R * sa + tz * w * 0.35)
        tip = (R * ca, BAND + h, R * sa)
        for o in (1, -1):
            quad(P, base0, base1, mid1, mid0, STEEL, (ca * o, 0, sa * o))
            quad(P, mid0, mid1, tip, tip, GLOW, (ca * o, 0, sa * o))
    # the hub and its four spokes (the lamp's diffuser under the hub comes from the layout's LAMP)
    ring_band(P, 0.36, 0.05, 0.22, STEEL, 1); annulus(P, 0.0, 0.36, 0.22, STEEL, 1); annulus(P, 0.30, 0.36, 0.05, STEEL, -1)
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2
        rod(P, (0.36 * math.cos(a), 0.15, 0.36 * math.sin(a)), ((R - 0.03) * math.cos(a), 0.18, (R - 0.03) * math.sin(a)), 0.022, STEEL)
    # the cables: four thin steel cables from the ring's top straight up to their ceiling roses, a clamp at each end
    for k in range(4):
        a = math.pi / 4 + k * math.pi / 2; x, z = (R - 0.015) * math.cos(a), (R - 0.015) * math.sin(a)
        rod(P, (x, BAND + 0.02, z), (x, BAND + cable, z), 0.004, STEEL, 6)
        rod(P, (x, BAND + 0.02, z), (x, BAND + 0.08, z), 0.012, STEEL)              # the clamp on the ring
        rod(P, (x, BAND + cable - 0.06, z), (x, BAND + cable - 0.005, z), 0.045, STEEL, 16)   # the ceiling rose
    o = P.finish()
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
    print('CROWN', name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
# the Grand Hall's escalators: the ring's bottom at 8.6 m, the ceiling at 11.4; the arrivals escalators' void: 8.0 / 11.4
crown('crown_light_hall', 11.4 - 8.6 - BAND)
crown('crown_light_claim', 11.4 - 8.0 - BAND)
print('CROWN done')
