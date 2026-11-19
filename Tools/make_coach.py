# The airport express coach (owner's blueprints, 2026-11-19: Art/Reference/From-Phone/2026-11-19/Coach-*.png), our own
# model made here in Blender. Part 1: the exterior. Sizes from the blueprint (metres): 12.90 long, 2.55 wide, 3.70 high
# (3.88 with the roof pods), wheelbase 6.10 (front overhang 2.75, rear 4.05), wheels 295/80 R22.5 (1.04), floor 1.40,
# ground clearance 0.34, windows 2.10-3.22, three luggage bays a side (0.80 high), front + middle doors on the right.
# Axes: x forward (origin on the ground at the coach's middle), y to the left (driver's side), z up.
# The swappable parts (owner: "easily replaceable") are their own models, placed by Data/coach-layout.json:
#   CoachBody (Nanite), CoachGlass (not Nanite), CoachWheel, CoachBayDoor, CoachDoorLeaf / CoachDoorGlass, CoachMirror,
#   CoachSeat (Tools/make_coach_seat.py), and the interior's own parts (part 2).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Tools/make_coach.py [-- --render|--quick]
import bpy, bmesh, math, os, sys, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import coach_kit as K
from coach_kit import rblock, box, cyl, tube, lathe, profile, text, dup, boolean, solidify, ss, mat, tex
from mathutils import Vector, Matrix
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
K.ART = ART = os.path.join(ROOT, 'Art', 'Coach'); os.makedirs(ART, exist_ok=True)
SHOTS = os.path.join(ROOT, 'docs', 'Pictures', 'Coach'); os.makedirs(SHOTS, exist_ok=True)
QUICK = '--quick' in sys.argv; CLASH = '--clash' in sys.argv; RENDER = QUICK or CLASH or '--render' in sys.argv
bpy.ops.wm.read_factory_settings(use_empty=True)

L, W, H, CLR = 12.90, 2.55, 3.70, 0.34
XF, XR = L / 2, -L / 2
def d2x(d): return XF - d                      # distance from the front -> x
AX_F, AX_R, WR = d2x(2.75), d2x(8.85), 0.52    # axles, wheel radius
RIGHT, LEFT = -1, 1                            # y sign of each side (doors on the right)
FLOOR = 1.40

# ---------------------------------------------------------------- materials (our own textures, made here)
xx, yy = K.xx, K.yy
flake = K.blur(K.rng.random((K.N, K.N)), 0) ** 6
T_PAINT = K.tex('T_Coach_Paint', (0.022, 0.075, 0.20), 0.05, 0.6, pattern=flake * 0.4)
T_TEAL = K.tex('T_Coach_Teal', (0.05, 0.55, 0.52), 0.04, 0.6, pattern=flake * 0.3)
T_BLACK = K.tex('T_Coach_GlossBlack', (0.012, 0.013, 0.015), 0.03, 0.5)
T_TRIM = K.tex('T_Coach_TrimBlack', (0.025, 0.026, 0.028), 0.08, 2.5)
T_GLASS = K.tex('T_Coach_Glass', (0.012, 0.018, 0.022), 0.01, 0.2)
T_CHROME = K.tex('T_Coach_Chrome', (0.80, 0.82, 0.84), 0.03, 0.8, True)
T_ALLOY = K.tex('T_Coach_Alloy', (0.62, 0.64, 0.67), 0.05, 1.5, True)
tread = ((np.abs(((xx * 24) % 1) - 0.5) < 0.12) | (np.abs(((yy * 6) % 1) - 0.5) < 0.05)).astype(float)
T_TYRE = K.tex('T_Coach_Tyre', (0.03, 0.03, 0.032), 0.10, 4.0, pattern=1 - tread * 0.6)
T_RUBBER = K.tex('T_Coach_Rubber', (0.035, 0.035, 0.038), 0.10, 3.0)
T_STEEL = K.tex('T_Coach_Chassis', (0.08, 0.085, 0.09), 0.12, 2.5)
T_LENS = K.tex('T_Coach_Lens', (0.85, 0.88, 0.92), 0.02, 0.5)
T_RED = K.tex('T_Coach_RedLens', (0.55, 0.02, 0.02), 0.03, 0.6)
T_AMBER = K.tex('T_Coach_AmberLens', (0.85, 0.40, 0.02), 0.03, 0.6)
grille = (np.abs(((yy * 18) % 1) - 0.5) < 0.22).astype(float)
T_GRILLE = K.tex('T_Coach_Grille', (0.03, 0.035, 0.04), 0.05, 3.0, pattern=grille)
T_WHITE = K.tex('T_Coach_White', (0.90, 0.92, 0.94), 0.02, 0.5)
T_PLATE = K.tex('T_Coach_Plate', (0.85, 0.86, 0.84), 0.02, 0.4)
T_LED = K.tex('T_Coach_LedPanel', (0.02, 0.02, 0.02), 0.05, 0.5, pattern=((np.abs(((xx * 64) % 1) - 0.5) < 0.3) & (np.abs(((yy * 16) % 1) - 0.5) < 0.3)).astype(float))

# the livery (our own design): navy deepening towards the skirt, the teal sweep rising to the back with a white
# pinstripe under it, teal speed-lines over the rear quarter climbing to the roof
LW, LH = 4096, 1176
lu, lv = np.meshgrid(np.linspace(0, 1, LW), np.linspace(0, 1, LH))   # (Blender images: row 0 = the bottom, the ground)
zz = lv * H; dd = lu * L                                             # height (m), distance from the front (m)
def band(c, w, soft=0.004): return np.clip((w / 2 - np.abs(zz - c)) / soft + 0.5, 0, 1)
low, high = np.array((0.010, 0.035, 0.095)), np.array((0.035, 0.115, 0.290))
k = np.clip((zz - 0.3) / 3.0, 0, 1) ** 0.8
liv = low[None, None, :] * (1 - k[..., None]) + high[None, None, :] * k[..., None]
sweep_c = 1.30 + dd / L * 0.64
teal = np.array((0.020, 0.50, 0.46)); teal2 = np.array((0.10, 0.80, 0.72))
tb = band(sweep_c, 0.10); tg = (dd / L)[..., None]
liv = liv * (1 - tb[..., None]) + (teal * (1 - tg) + teal2 * tg)[None, None, :] * tb[..., None] if False else liv * (1 - tb[..., None]) + (teal[None, None, :] * (1 - tg) + teal2[None, None, :] * tg) * tb[..., None]
pin = band(sweep_c - 0.085, 0.014)
liv = liv * (1 - pin[..., None]) + np.array((0.85, 0.90, 0.92))[None, None, :] * pin[..., None]
for i, (a, w) in enumerate(((0.16, 0.05), (0.32, 0.03), (0.46, 0.018))):
    # speed-lines from the sweep at 8.3 m back, curving up towards the rear roof corner
    t = np.clip((dd - 8.3) / (L - 8.3), 0, 1)
    c = sweep_c + a + (H - 0.55 - sweep_c - a) * t ** 2.2 * 0.62
    f = np.clip((dd - 8.3) / 1.2, 0, 1) * band(c, w)
    liv = liv * (1 - f[..., None]) + (teal2 * 0.9)[None, None, :] * f[..., None]
clean = low[None, :] * (1 - k[:, :1]) + high[None, :] * k[:, :1]
liv[:, -16:, :] = clean[:, None, :] if clean.ndim == 2 else clean   # (a clean strip for the ends, roof and edges)
T_LIVERY = (K.save_img('T_Coach_Livery', liv), T_PAINT[1])
M = {}
M['paint'] = mat('Coach_Paint', T_LIVERY, 0.0, 0.26, coat=1.0)
# (the flake normal on its own real-size UVs, the second set: the livery's stretch along the coach doesn't touch it)
_nt = M['paint'].node_tree; _uvn = _nt.nodes.new('ShaderNodeUVMap'); _uvn.uv_map = 'Flake'
_nt.links.new(_uvn.outputs['UV'], [n for n in _nt.nodes if n.type == 'TEX_IMAGE' and n.image == T_PAINT[1]][0].inputs['Vector'])
M['teal'] = mat('Coach_LiveryTeal', T_TEAL, 0.0, 0.3, coat=1.0)
M['black'] = mat('Coach_GlossBlack', T_BLACK, 0.0, 0.12, coat=1.0)
M['trim'] = mat('Coach_TrimBlack', T_TRIM, 0.0, 0.6)
M['glass'] = mat('Coach_Glass', T_GLASS, 0.0, 0.015, alpha=0.80)
M['chrome'] = mat('Coach_Chrome', T_CHROME, 1.0, 0.08)
M['alloy'] = mat('Coach_Alloy', T_ALLOY, 1.0, 0.25)
M['tyre'] = mat('Coach_Tyre', T_TYRE, 0.0, 0.85)
M['rubber'] = mat('Coach_Rubber', T_RUBBER, 0.0, 0.8)
M['chassis'] = mat('Coach_Chassis', T_STEEL, 0.6, 0.55)
M['caliper'] = mat('Coach_Caliper', K.tex('T_Coach_Caliper', (0.45, 0.04, 0.03), 0.05, 1.0), 0.2, 0.35, coat=0.6)
M['drlgold'] = mat('Coach_DRLWarm', K.tex('T_Coach_DRLWarm', (0.85, 0.72, 0.45), 0.02, 0.5), 0.0, 0.2, (1.0, 0.85, 0.55), 2.5)
M['lens'] = mat('Coach_HeadLens', T_LENS, 0.0, 0.05, (1.0, 0.98, 0.92), 0.0)
M['drl'] = mat('Coach_DRL', T_WHITE, 0.0, 0.2, (0.9, 0.97, 1.0), 4.0)
M['blade'] = mat('Coach_LightBlade', T_TEAL, 0.0, 0.2, (0.15, 0.95, 0.85), 3.0)
M['red'] = mat('Coach_TailLight', T_RED, 0.0, 0.08, (1.0, 0.04, 0.03), 1.5)
M['amber'] = mat('Coach_Indicator', T_AMBER, 0.0, 0.08, (1.0, 0.45, 0.02), 0.0)
M['marker'] = mat('Coach_SideMarker', T_AMBER, 0.0, 0.1, (1.0, 0.5, 0.05), 1.0)
M['grille'] = mat('Coach_Grille', T_GRILLE, 0.3, 0.5)
M['white'] = mat('Coach_White', T_WHITE, 0.0, 0.35)
M['plate'] = mat('Coach_Plate', T_PLATE, 0.0, 0.3)
M['led'] = mat('Coach_RouteDisplay', T_LED, 0.0, 0.3)
M['ledtext'] = mat('Coach_RouteText', T_AMBER, 0.0, 0.3, (1.0, 0.62, 0.12), 3.0)

PART = {}   # every object -> which game model it goes in
def put(o, part): PART[o.name] = part; return o

# ---------------------------------------------------------------- the body: one rounded volume, the front leaning back
def body_volume(name, grow=0.0):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    hz = (H - CLR) / 2
    for v in bm.verts: v.co = Vector((v.co.x * (L + 2 * grow), v.co.y * (W + 2 * grow), (CLR + hz) + v.co.z * (H - CLR + 2 * grow)))
    bw = bm.edges.layers.bevel_weight.verify()
    for e in bm.edges:
        a, b = e.verts[0].co, e.verts[1].co; d = (b - a).normalized(); mz = (a.z + b.z) / 2
        if abs(d.z) > 0.9: e[bw] = 0.62                              # the four upright corners
        elif mz > 2.0: e[bw] = 1.0 if abs(d.x) > 0.9 else 0.80       # the roof's edges (front / rear top a little tighter)
        else: e[bw] = 0.16                                           # along the bottom
    o = K.link(name, bm)
    md = o.modifiers.new('b', 'BEVEL'); md.width = 0.34; md.segments = 10; md.limit_method = 'WEIGHT'; md.profile = 0.5
    K.apply_mods(o)
    bm = bmesh.new(); bm.from_mesh(o.data)
    bmesh.ops.subdivide_edges(bm, edges=[e for e in bm.edges if (e.verts[0].co - e.verts[1].co).length > 0.6], cuts=3, use_grid_fill=True)
    bm.to_mesh(o.data); bm.free()
    for v in o.data.vertices:
        # the front leans back towards the top (the blueprint: 0.25 m from the bumper to the roof), and its face bows out
        f = ss(XF - 0.55, XF - 0.02, v.co.x)
        v.co.x -= f * 0.25 * ss(1.05, H, v.co.z)
        v.co.x += f * 0.035 * (1 - (v.co.y / (W / 2)) ** 2) * ss(0.8, 1.2, v.co.z)
        # the sides tuck in a hair towards the roof (tumblehome), the skirt a hair in at the bottom
        side = abs(v.co.y) / (W / 2)
        v.co.y -= math.copysign(0.018 * ss(2.4, 3.6, v.co.z) * side, v.co.y)
    return o

outer = body_volume('BodyOuter')
for p in outer.data.polygons: p.use_smooth = True
outer.data.use_auto_smooth = True; outer.data.auto_smooth_angle = math.radians(35)

# openings (each a box straight through one side / the front / the rear)
def side_box(side, d0, d1, z0, z1, depth=0.6):
    y0 = side * (W / 2 - 0.25); y1 = side * (W / 2 + 0.3)
    return box('cut', d2x(d1), d2x(d0), min(y0, y1), max(y0, y1), z0, z1)
WIN_Z = (2.10, 3.22)
# right side (doors): window runs between the front door and the middle door, and after it to the back
R_PANES = [(1.61, 2.17), (2.21, 3.34), (3.38, 4.51), (4.55, 5.68), (5.72, 6.85), (6.89, 8.02), (8.06, 9.60)]
R_PANES += [(10.60, 12.50)]
L_PANES = [(0.55, 1.56), (1.60, 2.75), (2.79, 3.94), (3.98, 5.13), (5.17, 6.32), (6.36, 7.51), (7.55, 8.70), (8.74, 9.84), (9.88, 12.30)]
DOORS = [(0.53, 1.61), (9.60, 10.60)]          # front, middle (right side)
DOOR_Z = (0.40, 3.22)
BAYS = [(3.58, 4.99), (5.09, 6.52), (6.62, 8.05)]
BAY_Z = (0.40, 1.18)
WS_Z = (1.12, 3.37)                            # the windscreen
RW_Z, RW_Y = (2.58, 3.30), 0.74               # the rear window

def opening_boxes(grow=0.0):
    g = grow
    bx = []
    for side, panes in ((RIGHT, R_PANES), (LEFT, L_PANES)):
        for d0, d1 in panes: bx.append(side_box(side, d0 - g, d1 + g, WIN_Z[0] - g, WIN_Z[1] + g))
    for d0, d1 in DOORS: bx.append(side_box(RIGHT, d0 - g, d1 + g, DOOR_Z[0] - g, DOOR_Z[1] + g))
    for side in (RIGHT, LEFT):
        for d0, d1 in BAYS: bx.append(side_box(side, d0 - g, d1 + g, BAY_Z[0] - g, BAY_Z[1] + g))
    bx.append(box('cut', XF - 0.42 - g, XF + 0.4, -(W / 2 - 0.10) - g, (W / 2 - 0.10) + g, WS_Z[0] - g, WS_Z[1] + g))
    bx.append(box('cut', XF - 0.62 - g, XF + 0.4, -(W / 2 - 0.30) - g, (W / 2 - 0.30) + g, WS_Z[1] - 0.2 - g, H + 0.5))
    bx.append(box('cut', XR - 0.4, XR + 0.25, -RW_Y - g, RW_Y + g, RW_Z[0] - g, RW_Z[1] + g))
    bx.append(box('cut', XR - 0.4, XR + 0.45, -(W / 2 - 0.30) - g, (W / 2 - 0.30) + g, RW_Z[1] - 0.1 - g, H + 0.5))
    return bx
def windscreen_wrap(grow=0.0):
    # the windscreen wraps round the front corners onto the sides (blueprint 1)
    return [box('cut', XF - 0.42 - grow, XF + 0.4, s * (W / 2 - 0.4), s * (W / 2 + 0.4), WS_Z[0] - grow, WS_Z[1] + grow) for s in (-1, 1)]

# the skin: 35 mm, every opening cut, the wheel arches cut
body = dup(outer, 'CoachSkin', M['paint'])
solidify(body, 0.035)
arches = [cyl('arch', Vector((ax, 0, WR)), 0.62, W + 0.4, 'y', M['paint'], 64) for ax in (AX_F, AX_R)]
boolean(body, opening_boxes() + windscreen_wrap() + arches)
put(body, 'CoachBody')

# glass: the skin's own outer 12 mm where each window / windscreen / door is (so it lies flush with the paint)
skin12 = dup(outer, 'GlassBase', M['glass']); solidify(skin12, 0.012)
def glass_piece(name, cutters):
    g = dup(skin12, name, M['glass'])
    boolean(g, cutters, 'INTERSECT')
    return put(g, 'CoachGlass')
gb = []
for side, panes in ((RIGHT, R_PANES), (LEFT, L_PANES)):
    for i, (d0, d1) in enumerate(panes): gb.append(side_box(side, d0 + 0.002, d1 - 0.002, WIN_Z[0] + 0.002, WIN_Z[1] - 0.002))
glass_piece('SideGlass', gb)
glass_piece('Windscreen', [box('cut', XF - 0.42 + 0.002, XF + 0.4, -(W / 2 + 0.4), W / 2 + 0.4, WS_Z[0] + 0.002, WS_Z[1] - 0.002)])
glass_piece('WindscreenRoof', [box('cut', XF - 0.62 + 0.002, XF + 0.4, -(W / 2 - 0.30) + 0.002, (W / 2 - 0.30) - 0.002, WS_Z[1] - 0.2, H + 0.5)])
glass_piece('RearGlass', [box('cut', XR - 0.4, XR + 0.25, -RW_Y + 0.002, RW_Y - 0.002, RW_Z[0] + 0.002, RW_Z[1] - 0.002)])
glass_piece('RearGlassRoof', [box('cut', XR - 0.4, XR + 0.45 - 0.002, -(W / 2 - 0.30) + 0.002, (W / 2 - 0.30) - 0.002, RW_Z[1] - 0.1, H + 0.5)])
# the black frit round every pane, printed on the inside of the glass (a band 4 cm wide just behind it)
fr = dup(outer, 'FritBase', M['black']); solidify(fr, 0.004, -1.0)
for v in fr.data.vertices: pass
frit_in = [],
def frit(name, cut_outer, cut_inner):
    f = dup(fr, name, M['black'])
    boolean(f, cut_outer, 'INTERSECT')
    boolean(f, cut_inner, 'DIFFERENCE')
    # behind the glass: 12-16 mm in from the paint
    for v in f.data.vertices: pass
    return f
# (the frit sits under the glass: the base is moved 12 mm in along the normals)
fr.data.transform(Matrix.Identity(4))
bm = bmesh.new(); bm.from_mesh(fr.data); bm.normal_update()
for v in bm.verts: v.co -= v.normal * 0.026
bm.to_mesh(fr.data); bm.free()
fo, fi = [], []
for side, panes in ((RIGHT, R_PANES), (LEFT, L_PANES)):
    for d0, d1 in panes:
        fo.append(side_box(side, d0, d1, WIN_Z[0], WIN_Z[1])); fi.append(side_box(side, d0 + 0.05, d1 - 0.05, WIN_Z[0] + 0.05, WIN_Z[1] - 0.04))
put(frit('SideFrit', fo, fi), 'CoachBody')
put(frit('WindscreenFrit', [box('cut', XF - 0.42, XF + 0.4, -(W / 2 + 0.4), W / 2 + 0.4, WS_Z[0], WS_Z[1])],
         [box('cut', XF - 0.36, XF + 0.4, -(W / 2 + 0.4), W / 2 + 0.4, WS_Z[0] + 0.07, WS_Z[1] - 0.06)]), 'CoachBody')
K.ALL.remove(fr); bpy.data.objects.remove(fr); K.ALL.remove(skin12); bpy.data.objects.remove(skin12)
# (no clipping: the glass's own shape cut out of the frit, so the two never share a place)
_wf = bpy.data.objects['WindscreenFrit']; K.ALL.remove(_wf); bpy.data.objects.remove(_wf)   # (it kept meeting the curved glass: the windscreen's own tint does its edge)
for fn_ in ('SideFrit',):
    boolean(bpy.data.objects[fn_], [dup(bpy.data.objects[g], 'cut', M['glass']) for g in ('SideGlass', 'Windscreen', 'WindscreenRoof')])

# ---------------------------------------------------------------- livery: the turquoise sweep, the wordmark
# the wordmark (white), below the windows behind the front axle, both sides; the airport's own name goes on at run time
for side in (RIGHT, LEFT):
    rot = Matrix.Rotation(math.pi / 2, 4, 'X') @ (Matrix.Identity(4) if side == RIGHT else Matrix.Rotation(math.pi, 4, 'Y'))
    rot = Matrix.Rotation(math.pi / 2 if side == RIGHT else -math.pi / 2, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'X') if False else rot
    o = text(f'Wordmark_{side}', 'AIRPORT EXPRESS', Vector((d2x(4.6), side * (W / 2 + 0.0015), 1.80)), 0.21, M['white'],
             (Matrix.Rotation(math.pi / 2, 4, 'X') if side == RIGHT else Matrix.Rotation(math.pi, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'X')))
    put(o, 'CoachBody')
    o2 = text(f'WordSub_{side}', 'electric  ·  zero emission', Vector((d2x(4.6), side * (W / 2 + 0.0015), 1.66)), 0.075, M['teal'],
              (Matrix.Rotation(math.pi / 2, 4, 'X') if side == RIGHT else Matrix.Rotation(math.pi, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'X')))
    put(o2, 'CoachBody')

# ---------------------------------------------------------------- wheel arches: liners, mud flaps; underbody
for ax in (AX_F, AX_R):
    for side in (RIGHT, LEFT):
        yo, yi = side * (W / 2 - 0.035), side * (W / 2 - (0.80 if ax == AX_R else 0.47))
        # the arch liner: a half-pipe round the wheel and a wall on the inside, black plastic
        pr = [(0.60, yi), (0.60, yo)]
        bm = bmesh.new(); rings = []
        for i in range(25):
            t = math.pi * i / 24
            rings.append([bm.verts.new((ax + 0.60 * math.cos(t), y, WR + 0.60 * math.sin(t))) for y in (yi, yo)])
        for i in range(24): bm.faces.new((rings[i][0], rings[i + 1][0], rings[i + 1][1], rings[i][1]))
        inner = [bm.verts.new((ax + 0.60 * math.cos(math.pi * i / 24), yi, WR + 0.60 * math.sin(math.pi * i / 24))) for i in range(25)]
        bm.faces.new(inner)
        o = K.link(f'ArchLiner_{ax:.0f}{side}', bm); solidify(o, 0.008); put(K.finish(o, M['trim']), 'CoachBody')
        put(rblock(f'MudFlap_{ax:.0f}{side}', (ax - 0.66, side * (W / 2 - 0.20), CLR + 0.05), (0.012, 0.34, 0.26), 0.004, M['rubber'], cuts=1), 'CoachBody')
# the underbody: a flat black undertray between the arches, the floor's beams across, the side skirts' lower lip
for i, (x0, x1) in enumerate(((XR + 0.25, AX_R - 0.66), (AX_R + 0.66, AX_F - 0.66), (AX_F + 0.66, XF - 0.25))):
    put(box(f'UnderTray_{i}', x0, x1, -(W / 2 - 0.05), W / 2 - 0.05, CLR - 0.002, CLR + 0.004, M['trim']), 'CoachBody')

# ---------------------------------------------------------------- the wheels (one model, four times)
def wheel():
    # tyre 295/80 R22.5: 1.04 across, 0.295 wide; rim 22.5" (0.572), ten spokes, a brake disc behind
    tw, rr = 0.295, 0.286
    prof = [(rr + 0.01, -tw / 2 + 0.01), (0.33, -tw / 2 - 0.004), (0.42, -tw / 2 - 0.006), (0.49, -tw / 2 + 0.01), (0.515, -tw / 2 + 0.04),
            (0.52, -0.10), (0.52, 0.10), (0.515, tw / 2 - 0.04), (0.49, tw / 2 - 0.01), (0.42, tw / 2 + 0.006), (0.33, tw / 2 + 0.004), (rr + 0.01, tw / 2 - 0.01)]
    ty = lathe('Tyre', prof, M['tyre'], 96, 'y'); put(ty, 'CoachWheel')
    # tread grooves: three rings cut in
    for z in (-0.06, 0.0, 0.06):
        g = lathe('TreadGroove', [(0.505, z - 0.006), (0.505, z + 0.006)], M['rubber'], 96, 'y'); put(g, 'CoachWheel')
    # sidewall lettering ring
    rim = lathe('Rim', [(0.17, -0.11), (rr, -0.11), (rr, -0.13), (rr + 0.012, -0.135), (rr + 0.012, -0.145), (0.27, -0.145), (0.24, -0.12), (0.16, -0.12), (0.10, -0.13), (0.06, -0.13)], M['alloy'], 72, 'y')
    put(rim, 'CoachWheel')
    # ten spokes: each a swept blade from the hub to the rim
    for i in range(10):
        a = 2 * math.pi * i / 10
        o = rblock(f'Spoke_{i}', (0.0, -0.128, 0.165), (0.032, 0.022, 0.21), 0.008, M['alloy'], cuts=2, segs=3)
        for v in o.data.vertices:
            t = (v.co.z - 0.06) / 0.21
            v.co.x *= 1.0 + 0.6 * t; v.co.y += -0.012 * t
        o.data.transform(Matrix.Rotation(a, 4, 'Y')); put(o, 'CoachWheel')
    put(lathe('Hub', [(0.0, -0.165), (0.06, -0.165), (0.075, -0.15), (0.08, -0.12), (0.10, -0.11)], M['chrome'], 48, 'y'), 'CoachWheel')
    for i in range(10):
        a = 2 * math.pi * i / 10
        put(cyl(f'Nut_{i}', Vector((0.11 * math.cos(a), -0.128, 0.11 * math.sin(a))), 0.013, 0.025, 'y', M['chrome'], 6, 0.002), 'CoachWheel')
    put(lathe('BrakeDisc', [(0.12, 0.0), (0.215, 0.0), (0.215, 0.045), (0.12, 0.045)], M['chassis'], 64, 'y'), 'CoachWheel')
wheel()

# ---------------------------------------------------------------- suspension (owner: several kinds, with their details)
def bellow(name, c, r=0.13, h=0.26):
    prof = [(r * 0.75, -h / 2), (r, -h / 2 + 0.03), (r * 1.04, -h / 4), (r * 0.93, 0), (r * 1.04, h / 4), (r, h / 2 - 0.03), (r * 0.75, h / 2)]
    o = lathe(name, prof, M['rubber'], 40, 'z', c); put(o, 'CoachBody')
    put(cyl(name + 'Top', Vector((c[0], c[1], c[2] + h / 2 + 0.01)), r * 0.85, 0.02, 'z', M['chassis'], 32), 'CoachBody')
    put(cyl(name + 'Piston', Vector((c[0], c[1], c[2] - h / 2 - 0.03)), r * 0.6, 0.06, 'z', M['chassis'], 32), 'CoachBody')
def damper(name, a, b, r=0.035):
    a, b = Vector(a), Vector(b); m_ = (a + b) / 2
    put(tube(name + 'Body', [a, a.lerp(b, 0.55)], r, M['chassis'], 2, bez=False), 'CoachBody')
    put(tube(name + 'Rod', [a.lerp(b, 0.5), b], r * 0.45, M['chrome'], 2, bez=False), 'CoachBody')
    put(tube(name + 'Boot', [a.lerp(b, 0.55), a.lerp(b, 0.85)], r * 0.75, M['rubber'], 2, bez=False), 'CoachBody')
    for p in (a, b): put(cyl(name + 'Eye', p, r * 0.9, 0.05, 'y', M['rubber'], 16), 'CoachBody')
# front: independent double wishbones (a coach's front), an air bellow and a damper each side, the steering rack and track rods
for side in (RIGHT, LEFT):
    yk = side * (W / 2 - 0.42)                        # the knuckle, inside the wheel
    put(rblock(f'Knuckle_{side}', (AX_F, yk, WR), (0.14, 0.10, 0.34), 0.03, M['chassis'], cuts=1), 'CoachBody')
    put(rblock(f'Caliper_{side}F', (AX_F + 0.17, side * (W / 2 - 0.30), WR + 0.06), (0.10, 0.09, 0.16), 0.02, M['caliper'], cuts=1), 'CoachBody')
    for zz, nm in ((WR - 0.16, 'Low'), (WR + 0.20, 'Up')):
        put(tube(f'Wishbone{nm}A_{side}', [(AX_F, yk, zz), (AX_F + 0.25, side * 0.45, zz + 0.04)], 0.032, M['chassis'], 2, bez=False), 'CoachBody')
        put(tube(f'Wishbone{nm}B_{side}', [(AX_F, yk, zz), (AX_F - 0.25, side * 0.45, zz + 0.04)], 0.032, M['chassis'], 2, bez=False), 'CoachBody')
    bellow(f'FrontBellow_{side}', (AX_F - 0.05, side * (W / 2 - 0.62), WR + 0.42))
    damper(f'FrontDamper_{side}', (AX_F + 0.12, side * (W / 2 - 0.55), WR - 0.12), (AX_F + 0.16, side * (W / 2 - 0.68), WR + 0.52))
    put(tube(f'TrackRod_{side}', [(AX_F - 0.16, yk, WR - 0.02), (AX_F - 0.20, side * 0.20, WR - 0.02)], 0.018, M['chassis'], 2, bez=False), 'CoachBody')
    put(cyl(f'BallJoint_{side}', Vector((AX_F - 0.16, yk, WR - 0.02)), 0.03, 0.05, 'z', M['rubber'], 16), 'CoachBody')
put(cyl('SteeringRack', Vector((AX_F - 0.20, 0, WR - 0.02)), 0.04, 0.42, 'y', M['chassis'], 24), 'CoachBody')
put(cyl('FrontAntiRoll', Vector((AX_F + 0.30, 0, WR - 0.10)), 0.022, W - 0.9, 'y', M['chassis'], 16), 'CoachBody')
put(box('FrontCrossmember', AX_F - 0.32, AX_F + 0.32, -0.55, 0.55, WR + 0.02, WR + 0.18, M['chassis'], 0.02), 'CoachBody')
# rear: a rigid drive axle with the electric motor at its middle (rear drive), four bellows, four dampers, trailing arms,
# a V-rod on top and an anti-roll bar
put(cyl('RearAxleBeam', Vector((AX_R, 0, WR)), 0.075, 1.16, 'y', M['chassis'], 32), 'CoachBody')   # (to the inner twin wheels' hubs)
put(lathe('RearMotor', [(0.0, -0.32), (0.20, -0.32), (0.26, -0.26), (0.27, 0.18), (0.22, 0.30), (0.0, 0.30)], M['chassis'], 48, 'y', (AX_R, 0, WR)), 'CoachBody')
for i in range(10): put(cyl(f'MotorFin_{i}', Vector((AX_R, -0.22 + i * 0.045, WR)), 0.285, 0.012, 'y', M['chassis'], 48), 'CoachBody')
for side in (RIGHT, LEFT):
    for dx in (-0.36, 0.36):
        bellow(f'RearBellow_{side}{dx > 0}', (AX_R + dx, side * (W / 2 - 0.86), WR + 0.40), 0.13, 0.28)
    damper(f'RearDamperA_{side}', (AX_R + 0.12, side * (W / 2 - 0.84), WR - 0.08), (AX_R + 0.20, side * (W / 2 - 0.86), WR + 0.55))
    damper(f'RearDamperB_{side}', (AX_R - 0.12, side * (W / 2 - 0.84), WR - 0.08), (AX_R - 0.20, side * (W / 2 - 0.86), WR + 0.55))
    put(box(f'BellowBeam_{side}', AX_R - 0.55, AX_R + 0.55, side * (W / 2 - 0.98), side * (W / 2 - 0.80), WR + 0.04, WR + 0.20, M['chassis'], 0.02), 'CoachBody')
    put(tube(f'TrailingArm_{side}', [(AX_R + 0.85, side * (W / 2 - 0.92), WR + 0.22), (AX_R, side * (W / 2 - 0.92), WR - 0.02)], 0.04, M['chassis'], 2, bez=False), 'CoachBody')
    put(rblock(f'Caliper_{side}R', (AX_R + 0.17, side * (W / 2 - 0.36), WR + 0.06), (0.10, 0.09, 0.16), 0.02, M['caliper'], cuts=1), 'CoachBody')
    put(tube(f'VRod_{side}', [(AX_R + 0.05, side * 0.10, WR + 0.12), (AX_R + 0.75, side * 0.55, WR + 0.30)], 0.03, M['chassis'], 2, bez=False), 'CoachBody')
    put(tube(f'AntiRollLink_{side}', [(AX_R - 0.40, side * (W / 2 - 0.85), WR - 0.05), (AX_R - 0.40, side * (W / 2 - 0.85), WR + 0.35)], 0.016, M['chassis'], 2, bez=False), 'CoachBody')
put(cyl('RearAntiRoll', Vector((AX_R - 0.40, 0, WR - 0.05)), 0.026, W - 1.7, 'y', M['chassis'], 16), 'CoachBody')
# the chassis rails between the axles and to the ends, under the floor
for side in (RIGHT, LEFT):
    put(box(f'ChassisRail_{side}', XR + 0.4, XF - 0.5, side * 0.38, side * 0.50, WR + 0.20, WR + 0.42, M['chassis'], 0.01), 'CoachBody')

# ---------------------------------------------------------------- doors (the leaves are their own model: they'll open)
for i, (d0, d1) in enumerate(DOORS):
    mid = (d0 + d1) / 2
    for k, (a, b) in enumerate(((d0 + 0.012, mid - 0.006), (mid + 0.006, d1 - 0.012))):
        x0, x1 = d2x(b), d2x(a)
        y = RIGHT * (W / 2 - 0.012)
        put(box(f'DoorLeaf_{i}{k}Frame', x0, x1, y - 0.03, y + 0.0, DOOR_Z[0] + 0.01, DOOR_Z[1] - 0.01, M['black'], 0.01), 'CoachDoorLeaf')
        put(box(f'DoorLeaf_{i}{k}Glass', x0 + 0.045, x1 - 0.045, y - 0.016, y - 0.004, DOOR_Z[0] + 0.12, DOOR_Z[1] - 0.06, M['glass']), 'CoachDoorGlass')
        put(box(f'DoorLeaf_{i}{k}Kick', x0 + 0.02, x1 - 0.02, y - 0.004, y + 0.002, DOOR_Z[0] + 0.02, DOOR_Z[0] + 0.11, M['chrome'], 0.004), 'CoachDoorLeaf')
        put(box(f'DoorLeaf_{i}{k}Seal', x1 - 0.012 if k == 0 else x0, x1 if k == 0 else x0 + 0.012, y - 0.03, y + 0.004, DOOR_Z[0] + 0.01, DOOR_Z[1] - 0.01, M['rubber'], 0.004), 'CoachDoorLeaf')
    # the door buttons / hand rail outside, the step's lit edge
    put(rblock(f'DoorButton_{i}', (d2x(d1 + 0.06), RIGHT * (W / 2 + 0.004), 1.15), (0.06, 0.012, 0.10), 0.006, M['black'], cuts=1), 'CoachBody')
    put(cyl(f'DoorButtonLed_{i}', Vector((d2x(d1 + 0.06), RIGHT * (W / 2 + 0.011), 1.15)), 0.018, 0.004, 'y', M['blade'], 24), 'CoachBody')

# ---------------------------------------------------------------- luggage bay doors (one model, six times; they lift up)
bd0, bd1 = BAYS[0]
bay = dup(outer, 'BayDoorBase', M['paint']); solidify(bay, 0.03)
boolean(bay, [side_box(RIGHT, bd0 + 0.008, bd1 - 0.008, BAY_Z[0] + 0.008, BAY_Z[1] - 0.008)], 'INTERSECT')
bay.name = 'BayDoor'; put(bay, 'CoachBayDoor')
put(rblock('BayHandle', (d2x((bd0 + bd1) / 2), RIGHT * (W / 2 + 0.006), BAY_Z[1] - 0.10), (0.22, 0.014, 0.035), 0.006, M['chrome'], cuts=2), 'CoachBayDoor')
put(rblock('BayHandleRecess', (d2x((bd0 + bd1) / 2), RIGHT * (W / 2 - 0.002), BAY_Z[1] - 0.10), (0.26, 0.012, 0.06), 0.012, M['trim'], cuts=2), 'CoachBayDoor')
put(rblock('BayLock', (d2x((bd0 + bd1) / 2) + 0.20, RIGHT * (W / 2 + 0.003), BAY_Z[1] - 0.10), (0.025, 0.008, 0.025), 0.006, M['chrome'], cuts=1), 'CoachBayDoor')
# the bays themselves (through-loading, both sides): a lined box under the floor, lit when open
for i, (d0, d1) in enumerate(BAYS):
    put(box(f'BayFloor_{i}', d2x(d1) + 0.02, d2x(d0) - 0.02, -(W / 2 - 0.04), W / 2 - 0.04, BAY_Z[0] - 0.02, BAY_Z[0] + 0.01, M['trim']), 'CoachBody')
    put(box(f'BayRoof_{i}', d2x(d1) + 0.02, d2x(d0) - 0.02, -(W / 2 - 0.04), W / 2 - 0.04, BAY_Z[1] + 0.0, BAY_Z[1] + 0.03, M['trim']), 'CoachBody')
    for x in (d2x(d1) - 0.01, d2x(d0) + 0.01):
        put(box(f'BayWall_{i}{x > 0}', x - 0.015, x + 0.015, -(W / 2 - 0.04), W / 2 - 0.04, BAY_Z[0], BAY_Z[1], M['trim']), 'CoachBody')
    for k in range(6):
        put(box(f'BayRail_{i}{k}', d2x(d1) + 0.05, d2x(d0) - 0.05, -0.9 + k * 0.36 - 0.015, -0.9 + k * 0.36 + 0.015, BAY_Z[0] + 0.01, BAY_Z[0] + 0.025, M['alloy'], 0.004), 'CoachBody')

# ---------------------------------------------------------------- front: headlight clusters, light blade, grille, route display
FX = lambda z, y=0.0: XF - 0.25 * ss(1.05, H, z) + 0.035 * (1 - (y / (W / 2)) ** 2) * ss(0.8, 1.2, z)   # the front face's x at z
for side in (-1, 1):
    yc = side * 0.92
    put(rblock(f'HeadHousing_{side}', (FX(0.76, yc) - 0.03, yc, 0.76), (0.08, 0.52, 0.17), 0.04, M['black'], cuts=2, segs=4), 'CoachBody')
    for k in range(3):
        y = yc - side * (0.14 - k * 0.12)
        put(lathe(f'HeadLens_{side}{k}', [(0.0, 0.0), (0.045, 0.0), (0.048, -0.012), (0.03, -0.02), (0.0, -0.022)], M['lens'], 32, 'x', (FX(0.76, y) + 0.012, y, 0.765)), 'CoachBody')
        put(lathe(f'HeadReflector_{side}{k}', [(0.048, 0.0), (0.05, -0.005), (0.05, -0.002)], M['chrome'], 32, 'x', (FX(0.76, y) + 0.012, y, 0.765)), 'CoachBody')
    put(rblock(f'DrlBar_{side}', (FX(0.86, yc) + 0.004, yc + side * 0.04, 0.865), (0.03, 0.60, 0.03), 0.008, M['drlgold'], cuts=2), 'CoachBody')
    put(rblock(f'DrlFin_{side}', (FX(0.78, side * 1.20) - 0.06, side * 1.215, 0.76), (0.16, 0.03, 0.20), 0.01, M['drlgold'], cuts=2,
               deform=lambda co, u, v, w: co + Vector((-0.05 * w, 0, 0))), 'CoachBody')
    put(rblock(f'Indicator_{side}', (FX(0.62, side * 1.12) - 0.05, side * 1.13, 0.62), (0.07, 0.16, 0.05), 0.02, M['amber'], cuts=1), 'CoachBody')
put(rblock('LightBlade', (FX(0.98) + 0.004, 0, 0.98), (0.012, W - 0.28, 0.035), 0.012, M['blade'], cuts=6,
           deform=lambda co, u, v, w: co + Vector((0.035 * (1 - u * u) - 0.012, 0, 0))), 'CoachBody')
put(rblock('FrontGrille', (FX(0.66) - 0.01, 0, 0.66), (0.03, 0.86, 0.22), 0.03, M['grille'], cuts=4), 'CoachBody')
for k in range(5): put(box(f'GrilleSlat_{k}', FX(0.58) + 0.005, FX(0.58) + 0.02, -0.40, 0.40, 0.58 + k * 0.04, 0.59 + k * 0.04, M['chrome'], 0.003), 'CoachBody')
put(rblock('FrontPlate', (FX(0.45) + 0.01, 0, 0.45), (0.01, 0.52, 0.11), 0.006, M['plate'], cuts=1), 'CoachBody')
put(rblock('Bumper', (XF - 0.02, 0, 0.40), (0.10, W - 0.06, 0.12), 0.05, M['trim'], cuts=4,
           deform=lambda co, u, v, w: co + Vector((0.03 * (1 - v * v), 0, 0))), 'CoachBody')
put(rblock('TowCover', (FX(0.52) + 0.01, 0.6, 0.52), (0.012, 0.08, 0.06), 0.01, M['paint'], cuts=1), 'CoachBody')
put(rblock('RouteDisplay', (FX(3.47) - 0.16, 0, 3.475), (0.04, 1.40, 0.15), 0.02, M['led'], cuts=2), 'CoachBody')   # (behind the glass)
put(text('RouteText', 'AIRPORT EXPRESS', Vector((FX(3.47) - 0.138, 0, 3.475)), 0.10, M['ledtext'],
         Matrix.Rotation(math.pi / 2, 4, 'Z') @ Matrix.Rotation(math.pi / 2, 4, 'X') @ Matrix.Rotation(-0.10, 4, 'X')), 'CoachBody')
# the wipers, resting at the windscreen's foot
for side in (-1, 1):
    put(tube(f'WiperArm_{side}', [(FX(1.20) + 0.03, side * 0.05, 1.18), (FX(1.30) + 0.03, side * 0.55, 1.26), (FX(1.40) + 0.025, side * 1.0, 1.33)], 0.008, M['black'], 6), 'CoachBody')
    put(cyl(f'WiperPivot_{side}', Vector((FX(1.18) + 0.02, side * 0.05, 1.18)), 0.02, 0.03, 'x', M['black'], 16), 'CoachBody')

# ---------------------------------------------------------------- mirrors on arms (one model, mirrored)
def mirror(side):
    base = Vector((XF - 0.30, side * (W / 2 - 0.02), 3.32))
    pts = [base, base + Vector((0.18, side * 0.22, 0.04)), base + Vector((0.42, side * 0.25, -0.05)), base + Vector((0.50, side * 0.22, -0.30))]
    put(tube(f'MirrorArm_{side}', pts, 0.022, M['black'], 10), 'CoachMirror')
    head = base + Vector((0.50, side * 0.22, -0.55))
    put(rblock(f'MirrorHead_{side}', tuple(head), (0.10, 0.20, 0.40), 0.05, M['black'], cuts=3, segs=4, sub=1), 'CoachMirror')
    put(rblock(f'MirrorGlass_{side}', (head.x - 0.051, head.y, head.z), (0.004, 0.17, 0.36), 0.03, M['chrome'], cuts=2), 'CoachMirror')
    put(rblock(f'MirrorRepeater_{side}', (head.x + 0.03, head.y + side * 0.101, head.z + 0.12), (0.06, 0.004, 0.02), 0.002, M['amber'], cuts=1), 'CoachMirror')
mirror(RIGHT); mirror(LEFT)

# ---------------------------------------------------------------- rear: light columns, window camera, cooling grille
for side in (-1, 1):
    y = side * (W / 2 - 0.12)
    put(rblock(f'TailColumn_{side}', (XR - 0.006, y, 2.03), (0.03, 0.19, 2.42), 0.03, M['red'], cuts=8), 'CoachBody')
    for k in range(16): put(box(f'TailLed_{side}{k}', XR - 0.024, XR - 0.020, y - 0.07, y + 0.07, 0.90 + k * 0.145, 0.93 + k * 0.145, M['drl'] if k == 2 else M['red']), 'CoachBody')
    put(rblock(f'Reverse_{side}', (XR - 0.006, side * 0.85, 0.95), (0.02, 0.14, 0.06), 0.01, M['white'], cuts=1), 'CoachBody')
for s_ in (-1, 1): put(rblock(f'RearRedBar_{s_}', (XR - 0.006, s_ * 0.44, 2.47), (0.02, 0.80, 0.05), 0.01, M['red'], cuts=4), 'CoachBody')
put(rblock('NamePlate', (XR - 0.004, 0, 2.34), (0.012, 1.40, 0.11), 0.01, M['plate'], cuts=2), 'CoachBody')   # the airport's name: at run time
put(rblock('RearGrille', (XR - 0.004, 0, 1.62), (0.02, 1.90, 0.84), 0.04, M['grille'], cuts=4), 'CoachBody')
put(box('RearGrilleMullion', XR - 0.03, XR - 0.008, -0.02, 0.02, 1.22, 2.02, M['trim'], 0.004), 'CoachBody')
for k in range(12): put(box(f'RearLouvre_{k}', XR - 0.028, XR - 0.012, -0.90, 0.90, 1.24 + k * 0.066, 1.255 + k * 0.066, M['trim'], 0.004), 'CoachBody')
put(rblock('RearPlate', (XR - 0.005, 0, 0.62), (0.01, 0.52, 0.11), 0.006, M['plate'], cuts=1), 'CoachBody')
put(rblock('RearBumper', (XR + 0.02, 0, 0.42), (0.10, W - 0.06, 0.14), 0.05, M['trim'], cuts=4), 'CoachBody')
put(rblock('RearCamera', (XR + 0.10, 0, 3.30), (0.06, 0.10, 0.05), 0.02, M['black'], cuts=1), 'CoachBody')   # (behind the rear glass)
put(cyl('RearCameraLens', Vector((XR + 0.068, 0, 3.30)), 0.015, 0.008, 'x', M['black'], 24), 'CoachBody')

# ---------------------------------------------------------------- the side: markers, charging flap, drive hatch, emblem
for side in (RIGHT, LEFT):
    for d in (0.9, 2.2, 3.4, 8.2, 10.9, 12.4):   # (none on a bay door)
        put(rblock(f'Marker_{side}{d}', (d2x(d), side * (W / 2 + 0.004), 0.48), (0.07, 0.012, 0.025), 0.006, M['marker'], cuts=1), 'CoachBody')
put(rblock('ChargeFlap', (d2x(9.88), LEFT * (W / 2 + 0.002), 1.62), (0.26, 0.008, 0.26), 0.02, M['teal'], cuts=2), 'CoachBody')
put(rblock('SideRouteDisplay', (d2x(1.86), RIGHT * (W / 2 - 0.06), 2.52), (0.24, 0.03, 0.40), 0.01, M['led'], cuts=1), 'CoachBody')
put(cyl('ChargeFlapLed', Vector((d2x(9.88) + 0.12, LEFT * (W / 2 + 0.007), 1.68)), 0.008, 0.003, 'y', M['blade'], 16), 'CoachBody')
put(rblock('DriveHatch', (d2x(11.96), LEFT * (W / 2 + 0.002), 0.89), (1.26, 0.008, 0.70), 0.02, M['paint'], cuts=2), 'CoachBody')
for k in range(7): put(box(f'DriveHatchSlot_{k}', d2x(11.5 + k * 0.13) - 0.02, d2x(11.5 + k * 0.13) + 0.02, LEFT * (W / 2 + 0.004), LEFT * (W / 2 + 0.008), 0.66, 1.12, M['trim'], 0.006), 'CoachBody')

# ---------------------------------------------------------------- roof: battery packs, climate unit, escape hatches
put(rblock('RoofBatteries', (d2x(8.95), 0, H + 0.07), (3.90, 1.95, 0.16), 0.06, M['paint'], cuts=4, segs=4), 'CoachBody')
for k in range(3): put(box(f'BatterySeam_{k}', d2x(7.0 + 0.98 * (k + 1)) - 0.006, d2x(7.0 + 0.98 * (k + 1)) + 0.006, -0.96, 0.96, H + 0.145, H + 0.152, M['trim']), 'CoachBody')
put(rblock('RoofClimate', (d2x(5.0), 0, H + 0.07), (3.60, 1.80, 0.16), 0.08, M['paint'], cuts=4, segs=4), 'CoachBody')
for k, d in enumerate((4.3, 5.7)):
    put(cyl(f'ClimateFan_{k}', Vector((d2x(d), 0, H + 0.152)), 0.28, 0.006, 'z', M['grille'], 48), 'CoachBody')
    put(lathe(f'FanRing_{k}', [(0.28, 0.0), (0.30, 0.004), (0.30, 0.012), (0.28, 0.014)], M['trim'], 48, 'z', (d2x(d), 0, H + 0.148)), 'CoachBody')
for d in (2.6, 11.8):
    put(rblock(f'Hatch_{d}', (d2x(d), 0, H + 0.02), (0.58, 0.58, 0.05), 0.03, M['teal'], cuts=2, segs=3), 'CoachBody')
    put(rblock(f'HatchVent_{d}', (d2x(d), 0, H + 0.05), (0.40, 0.40, 0.02), 0.01, M['grille'], cuts=1), 'CoachBody')

exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'coach_interior.py'), encoding='utf-8').read())
bpy.data.objects.remove(outer, do_unlink=True)   # (the uncut volume the cuts were made from)
# ---------------------------------------------------------------- finish: UVs, checks, the layout, files
K.box_uv(K.ALL, 0.5)
for o in K.ALL:
    if not o.data.materials or o.data.materials[0] != M['paint']: continue
    me = o.data
    me.uv_layers.active.name = 'UVMap'
    fl = me.uv_layers.new(name='Flake'); src = me.uv_layers['UVMap'].data
    for i in range(len(src)): fl.data[i].uv = src[i].uv
    uv = me.uv_layers['UVMap'].data
    for p in me.polygons:
        side = abs(p.normal.y) > 0.55
        for li in p.loop_indices:
            co = me.vertices[me.loops[li].vertex_index].co
            u = min(max((XF - co.x) / L, 0.0), 0.996) if side else 0.9995
            uv[li].uv = (u, min(max(co.z / H, 0.0), 1.0))
# the paint's sides: one livery tile along the coach (a real-size map)
K.check_slots(K.ALL)
cols = {}
for o in K.ALL:
    part = PART.get(o.name, 'CoachBody')
    if part not in cols: cols[part] = bpy.data.collections.new(part); bpy.context.scene.collection.children.link(cols[part])
    for c in o.users_collection: c.objects.unlink(o)
    cols[part].objects.link(o)
tris = {p: sum(sum(len(q.vertices) - 2 for q in o.data.polygons) for o in c.objects) for p, c in cols.items()}
print('COACH:', json.dumps(tris))
layout = {'wheels': [[AX_F, RIGHT * (W / 2 - 0.20), WR, 0], [AX_F, LEFT * (W / 2 - 0.20), WR, 180], [AX_R, RIGHT * (W / 2 - 0.20), WR, 0], [AX_R, LEFT * (W / 2 - 0.20), WR, 180],
                     [AX_R, RIGHT * (W / 2 - 0.515), WR, 180], [AX_R, LEFT * (W / 2 - 0.515), WR, 0]],
          'bays': [[d2x((a + b) / 2), s, BAY_Z[1]] for s in (RIGHT, LEFT) for a, b in BAYS], 'doors': DOORS, 'floor': FLOOR}
layout.update(INTERIOR_LAYOUT)
json.dump(layout, open(os.path.join(ROOT, 'Data', 'coach-layout.json'), 'w'), indent=1)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(ART, 'Coach.blend'))
for p, c in cols.items():
    for o in bpy.context.selected_objects: o.select_set(False)
    for o in c.objects: o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(ART, p.lower() + '.glb'), export_format='GLB', use_selection=True, export_apply=True)

# ---------------------------------------------------------------- previews
if RENDER:
    # the wheels for the pictures: the one wheel set at its four places
    wheel_objs = list(cols['CoachWheel'].objects)
    for (x, y, z, yaw) in layout['wheels']:
        for o in wheel_objs:
            n = o.copy(); n.data = o.data; bpy.context.scene.collection.objects.link(n)
            n.matrix_world = Matrix.Translation((x, y, z)) @ Matrix.Rotation(math.radians(yaw), 4, 'Z')
    for o in wheel_objs: o.hide_render = True
    bay_objs = list(cols['CoachBayDoor'].objects); xc0 = d2x((BAYS[0][0] + BAYS[0][1]) / 2)
    for (xc, side, _z) in layout['bays']:
        for o in bay_objs:
            n = o.copy(); n.data = o.data; bpy.context.scene.collection.objects.link(n)
            n.matrix_world = Matrix.Translation((xc - xc0, 0, 0)) if side == RIGHT else Matrix.Translation((xc, 0, 0)) @ Matrix.Rotation(math.pi, 4, 'Z') @ Matrix.Translation((-xc0, 0, 0))
    for o in bay_objs: o.hide_render = True
    def place(part_objs, M_):
        for o in part_objs:
            n = o.copy(); n.data = o.data; bpy.context.scene.collection.objects.link(n); n.matrix_world = M_
    def at(e, tilt=0.0):
        return Matrix.Translation(e[:3]) @ Matrix.Rotation(math.radians(e[3]), 4, 'Z') @ Matrix.Diagonal((e[4], 1, 1, 1)) @ Matrix.Rotation(math.radians(tilt), 4, 'X')
    for part, key in (('CoachDriverSeat', 'driver_seat'), ('CoachGuideSeat', 'guide_seat')):
        objs = list(cols[part].objects); place(objs, at(layout[key])); [setattr(o, 'hide_render', True) for o in objs]
    objs = list(cols['CoachFoldSeat'].objects)
    for e in layout['fold_seats']: place(objs, at(e))
    for o in objs: o.hide_render = True
    sw = layout['steering_wheel']; objs = list(cols['CoachSteeringWheel'].objects)
    place(objs, Matrix.Translation(sw[:3]) @ Matrix.Rotation(math.radians(-sw[5]), 4, 'Y')); [setattr(o, 'hide_render', True) for o in objs]
    # the approved seat pair (Art/CoachSeat/coachseat.glb) at every row; the back bench for now as two pairs + a seat
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=os.path.join(ROOT, 'Art', 'CoachSeat', 'coachseat.glb'))
    seat_objs = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    for o in seat_objs: o.matrix_world = Matrix.Identity(4)
    for e in layout['seat_pairs']: place(seat_objs, at(e))
    for o in seat_objs: o.hide_render = True
    sc = bpy.context.scene; sc.render.engine = 'BLENDER_EEVEE'
    ee = sc.eevee; ee.use_gtao = True; ee.use_ssr = True; ee.use_ssr_refraction = True; ee.use_soft_shadows = True; ee.shadow_cube_size = '2048'
    ee.shadow_cascade_size = '4096'; ee.taa_render_samples = 16 if QUICK else 96; ee.use_bloom = True; ee.bloom_intensity = 0.03
    sc.view_settings.view_transform = 'Filmic'; sc.view_settings.look = 'Medium High Contrast'
    w = bpy.data.worlds.new('W'); w.use_nodes = True; bg = w.node_tree.nodes['Background']
    sky = w.node_tree.nodes.new('ShaderNodeTexSky'); sky.sky_type = 'NISHITA'; sky.sun_elevation = math.radians(28); sky.sun_rotation = math.radians(210)
    w.node_tree.links.new(sky.outputs['Color'], bg.inputs['Color']); bg.inputs['Strength'].default_value = 0.9; sc.world = w
    sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 4.5; sun.angle = math.radians(1.5)
    so = bpy.data.objects.new('Sun', sun); sc.collection.objects.link(so); so.rotation_euler = (math.radians(55), 0, math.radians(-150))
    gt = K.tex('T_Preview_Asphalt', (0.10, 0.10, 0.105), 0.25, 3.0)
    gm = mat('Preview_Asphalt', gt, 0, 0.8); g = box('Ground', -30, 30, -30, 30, -0.02, 0.0, gm); K.box_uv([g], 1.0)
    cam_d = bpy.data.cameras.new('Cam'); cam = bpy.data.objects.new('Cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
    def shoot(name, loc, at, lens=35, res=(3840, 2160)):
        if CLASH: return
        cam.location = loc; cam.rotation_euler = (Vector(at) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler(); cam_d.lens = lens
        if QUICK: res = (res[0] // 3, res[1] // 3)
        sc.render.resolution_x, sc.render.resolution_y = res; sc.render.filepath = os.path.join(SHOTS, name + '.png')
        bpy.ops.render.render(write_still=True); print('COACH: picture', name)
    shoot('Coach-1-front-right', (14.0, -10.5, 2.6), (0.5, 0, 1.6), 35)
    shoot('Coach-2-rear-left', (-13.5, 9.5, 3.2), (-0.5, 0, 1.6), 35)
    shoot('Coach-3-right-side', (0, -19, 1.9), (0, 0, 1.9), 40)
    shoot('Coach-4-front', (19, -1.5, 1.9), (0, 0, 1.9), 50)
    shoot('Coach-5-wheel-suspension', (AX_R + 1.6, -3.2, 0.55), (AX_R, -0.9, 0.55), 30)
    shoot('Coach-6-rear', (-19, 1.5, 1.9), (0, 0, 1.9), 50)
    shoot('Coach-7-top', (0.0, -0.01, 30), (0, 0, 0), 30)
    # the interior: lit inside, views from inside, and cutaways (the camera's near clip cuts the roof / a wall away)
    for k, x in enumerate((XF - 2.5, 0.0, XR + 2.5)):
        L_ = bpy.data.lights.new(f'Cabin{k}', 'AREA'); L_.energy = 260; L_.size = 2.0; L_.color = (1, 0.95, 0.88)
        o = bpy.data.objects.new(f'Cabin{k}', L_); sc.collection.objects.link(o); o.location = (x, 0, FLOOR + 1.85)
    def shoot_in(name, loc, at, lens=20, clip=0.05):
        cam_d.clip_start = clip; shoot(name, loc, at, lens); cam_d.clip_start = 0.05
    shoot_in('Coach-8-cabin-forward', (XR + 1.4, 0.0, FLOOR + 1.62), (XF, 0.0, FLOOR + 1.2), 18)
    shoot_in('Coach-9-cabin-back', (XF - 1.9, -0.05, FLOOR + 1.62), (XR, 0.0, FLOOR + 1.0), 18)
    shoot_in('Coach-10-driver', (d2x(2.05), -0.20, FLOOR + 1.45), (d2x(0.50), 0.55, 1.70), 22)
    shoot_in('Coach-10b-dashboard', (d2x(1.32), 0.62, 1.00 + 1.30), (d2x(0.55), 0.55, 1.78), 26)
    shoot_in('Coach-11-cutaway-top', (0.0, -0.01, 12.0), (0, 0, 0), 30, 12.0 - ROOF_IN + 0.05)
    shoot_in('Coach-12-cutaway-side', (0.0, -11.0, 2.4), (0, 0, 2.3), 30, 11.0 - (W / 2 - 0.12))

# ---------------------------------------------------------------- the clash check (owner: "some geometry is clipping into
# each other"): the coach assembled as the game will place it, every part tested triangle by triangle against every
# other model; each pair that cuts into each other printed with how many triangles cross and how deep it goes
if CLASH:
    from mathutils.bvhtree import BVHTree
    dg = bpy.context.evaluated_depsgraph_get()
    def part_of(o):
        if o.name in PART: return PART[o.name]
        n = o.name.rsplit('.', 1)[0] if o.name.rsplit('.', 1)[-1].isdigit() else o.name
        return PART.get(n, 'CoachSeat')
    items = []
    for o in bpy.context.scene.objects:
        if o.type != 'MESH' or o.hide_render or o.name in ('Ground',): continue
        me = o.evaluated_get(dg).to_mesh()
        vs = [o.matrix_world @ v.co for v in me.vertices]
        if not vs: continue
        tree = BVHTree.FromPolygons(vs, [tuple(p.vertices) for p in me.polygons])
        lo = Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs))); hi = Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
        items.append((o, part_of(o), tree, lo, hi, vs))
    found = []
    for i in range(len(items)):
        a = items[i]
        for j in range(i + 1, len(items)):
            b = items[j]
            if a[1] == b[1] and a[1] not in ('CoachSeat', 'CoachDashboard', 'CoachInterior'): continue   # (one model's own pieces - but the dashboard's and the fit-out's are checked too)
            if a[1] == 'CoachSeat' and b[1] == 'CoachSeat' and (a[0].name.split('.')[0] == b[0].name.split('.')[0]): pass
            if any(a[3][k] > b[4][k] or b[3][k] > a[4][k] for k in range(3)): continue
            hits = a[2].overlap(b[2])
            if hits: found.append((len(hits), a[0].name, a[1], b[0].name, b[1]))
    found.sort(reverse=True)
    print('CLASH: %d pairs' % len(found))
    for f in found: print('CLASH: %5d  %s [%s]  x  %s [%s]' % f)
