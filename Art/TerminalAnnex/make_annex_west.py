# The terminal's new west end (owner, 2026-11-15, docs/Requests/Terminal-Floor2-Sections.md), in the terminal plan's
# metres (x east, z north, y up; Data/terminal-plan.json) and the terminal's own materials (slots named after
# /Game/Terminal/Materials):
#   the escalator room (x 0..36, z 0..30): its floor (no open ground left), a glass front like the Grand Hall's
#   (mullions every 1.78 m), its west wall, its roof at +12, the Floor 2 landing (x 0..21) with a glass guardrail and
#   black trim along its edge (a gap where the escalators arrive), and the Grand Hall's west wall above +12 up to the
#   wave roof (open below: the escalator room's entrance, no wall, no door);
#   the food walk's south glass from x 21 east (the landing opens freely into the link and the food walk);
#   the plant room's shell 6 m narrower (x 6..36);
#   the museum hall (x -36..0, z 0..52, and the strip x 0..6, z 30..52): floor, walls with tall windows, a door opening
#   in its south (kerb side) wall, its roof; its north side is the pier's own wall (flush).
# Blender axes for the game's import: (x, y, z) = (-plan x, -plan z, plan y).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_annex_west.py -- <soffit points json>
import bpy, bmesh, json, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
a = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
SOFF = json.load(open(a[0]))['12892'] if a else []

bpy.ops.wm.read_factory_settings(use_empty=True)
MATS = {}
def mat(name):
    if name not in MATS:
        m = bpy.data.materials.new(name); MATS[name] = m
    return MATS[name]


class Piece:
    def __init__(self, name):
        self.bm = bmesh.new(); self.name = name; self.mats = []
        self.uv = self.bm.loops.layers.uv.new('UVMap')
    def mi(self, m):
        if m not in self.mats: self.mats.append(m)
        return self.mats.index(m)
    def face(self, pts, m):
        vs = [self.bm.verts.new((-x, -z, y)) for (x, y, z) in pts]
        f = self.bm.faces.new(vs); f.material_index = self.mi(m)
        # world-space UVs (the terminal's masters lay their detail in world space anyway)
        for l, (x, y, z) in zip(f.loops, pts): l[self.uv].uv = (x + z, y)
        return f
    def box(self, x0, x1, y0, y1, z0, z1, m, sides='all'):
        c = lambda i, j, k: (x1 if i else x0, y1 if j else y0, z1 if k else z0)
        F = {'-y': [c(0,0,0), c(1,0,0), c(1,0,1), c(0,0,1)], '+y': [c(0,1,0), c(0,1,1), c(1,1,1), c(1,1,0)],
             '-x': [c(0,0,0), c(0,0,1), c(0,1,1), c(0,1,0)], '+x': [c(1,0,0), c(1,1,0), c(1,1,1), c(1,0,1)],
             '-z': [c(0,0,0), c(0,1,0), c(1,1,0), c(1,0,0)], '+z': [c(0,0,1), c(1,0,1), c(1,1,1), c(0,1,1)]}
        for k, f in F.items():
            if sides == 'all' or k in sides: self.face(f, m)
    def finish(self):
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        me = bpy.data.meshes.new(self.name); self.bm.to_mesh(me); self.bm.free()
        for m in self.mats: me.materials.append(mat(m))
        o = bpy.data.objects.new(self.name, me); bpy.context.scene.collection.objects.link(o)
        return o


S = Piece('annex_west_solid'); G = Piece('annex_west_glass')
W, T = 'MI_T_white', 0.2                      # walls 20 cm

def wall_x(x, z0, z1, y0, y1, m_in=W, m_out=W, holes=()):
    """A wall in the plane x (thickness T), from z0 to z1, y0 to y1, with rectangular holes (z0, z1, y0, y1)."""
    cuts_z = sorted({z0, z1} | {h[0] for h in holes} | {h[1] for h in holes})
    for za, zb in zip(cuts_z, cuts_z[1:]):
        spans = [(y0, y1)]
        for h in holes:
            if h[0] <= za and zb <= h[1]:
                spans = [s for sp in spans for s in ((sp[0], min(sp[1], h[2])), (max(sp[0], h[3]), sp[1])) if s[1] - s[0] > 1e-4]
        for ya, yb in spans: S.box(x - T / 2, x + T / 2, ya, yb, za, zb, m_in)

def wall_z(z, x0, x1, y0, y1, m=W, holes=()):
    cuts_x = sorted({x0, x1} | {h[0] for h in holes} | {h[1] for h in holes})
    for xa, xb in zip(cuts_x, cuts_x[1:]):
        spans = [(y0, y1)]
        for h in holes:
            if h[0] <= xa and xb <= h[1]:
                spans = [s for sp in spans for s in ((sp[0], min(sp[1], h[2])), (max(sp[0], h[3]), sp[1])) if s[1] - s[0] > 1e-4]
        for ya, yb in spans: S.box(xa, xb, ya, yb, z - T / 2, z + T / 2, m)

def slab(x0, x1, z0, z1, ytop, top='MI_T_terrazzo', under='MI_T_offwhite', thick=0.6):
    S.box(x0, x1, ytop - 0.02, ytop, z0, z1, top)
    S.box(x0, x1, ytop - thick, ytop - 0.02, z0, z1, 'MI_T_concrete', sides=('-x', '+x', '-z', '+z'))
    S.box(x0, x1, ytop - thick - 0.04, ytop - thick, z0, z1, under)

def glass_front_z(z, x0, x1, y0, y1, step=1.78, transoms=(3.0,)):
    """A glass front in the plane z: panes, mullions every `step`, transoms, head and sill (as the Grand Hall's)."""
    G.box(x0, x1, y0, y1, z - 0.012, z + 0.012, 'MI_T_glass')
    n = max(1, round((x1 - x0) / step)); st = (x1 - x0) / n
    for i in range(n + 1):
        x = x0 + i * st
        S.box(max(x0, x - 0.04), min(x1, x + 0.04), y0, y1, z - 0.075, z + 0.075, 'MI_T_mullion')
    for ty in transoms + (y0, y1):
        S.box(x0, x1, ty - 0.04, ty + 0.04, z - 0.075, z + 0.075, 'MI_T_mullion')

# ============ the escalator room (x 0..36, z 0..30) ============
slab(0.0, 36.0, 0.0, 30.0, 0.01, under='MI_T_concrete', thick=0.3)                 # its floor: no open ground
glass_front_z(0.0, 0.1, 35.9, 0.01, 12.0, transoms=(3.0, 5.7, 6.1, 9.0))           # the front, onto the kerb
wall_x(0.0, 0.0, 30.0, 0.01, 12.0)                                                 # west wall (the museum beyond)
wall_z(30.0, 0.0, 6.0, 0.01, 12.0)                                                 # north, beside the museum strip
# the roof at +12 with its ceiling, and its edge (the wave roof floats above its east part as over the kerb)
S.box(0.0, 36.0, 11.4, 12.0, 0.0, 30.0, W)
S.box(0.0, 36.0, 11.32, 11.4, 0.0, 30.0, 'MI_T_offwhite', sides=('-y',))
# the Grand Hall's west wall above +12, up to the wave roof's underside (sampled from the roof itself)
if SOFF:
    import collections
    hz = collections.defaultdict(lambda: 99.0)
    for (x, y, z) in SOFF:
        if abs(x - 36.0) < 0.6 and -0.5 <= z <= 30.5: hz[round(z)] = min(hz[round(z)], y)
    zs = [z for z in range(0, 31)]
    ys = [min(hz[z], 22.0) if hz[z] < 98 else 15.0 for z in zs]
    for za, zb, ya, yb in zip(zs, zs[1:], ys, ys[1:]):
        for xx in (35.9, 36.1):
            S.face([(xx, 12.0, za), (xx, 12.0, zb), (xx, yb + 0.05, zb), (xx, ya + 0.05, za)], W)
    print('ANNEX hall wall above +12 up to the wave roof:', [round(y, 2) for y in ys[::5]])
# the Floor 2 landing (x 0..21) and its edge
slab(0.0, 21.0, 0.0, 30.0, 6.01, thick=0.6)
S.box(20.96, 21.0, 5.37, 6.01, 0.0, 30.0, W, sides=('+x',))
# the landing's guardrail: a stainless shoe, glass, black trim on top (as high as the escalators' handrails); a gap
# where the escalators arrive (z 13.3..16.7)
ESC_GAP = (13.3, 16.7)
for (za, zb) in ((0.15, ESC_GAP[0]), (ESC_GAP[1], 29.85)):
    S.box(20.82, 20.98, 6.01, 6.13, za, zb, 'MI_T_steel')
    G.box(20.894, 20.906, 6.11, 6.94, za, zb, 'MI_T_glass')
    # the trim: a rounded black profile like the handrails (a box with chamfered top)
    S.box(20.87, 20.93, 6.94, 6.985, za, zb, 'MI_T_black')
    S.box(20.88, 20.92, 6.985, 7.0, za, zb, 'MI_T_black')
# ============ the food walk's south glass, from x 21 east (the landing opens freely below x 21) ============
glass_front_z(30.0, 21.0, 56.0, 6.01, 11.4, transoms=())
S.box(6.0, 56.0, 11.4, 12.0, 29.9, 30.1, W)                                        # its head over the opening
# ============ the plant room, 6 m narrower (x 6..36, z 30..52, Floor 1) ============
S.box(6.0, 36.0, -0.3, 0.01, 30.0, 52.0, 'MI_T_epoxy', sides=('+y',))
S.box(6.0, 36.0, 5.36, 5.4, 30.0, 52.0, 'MI_T_concrete', sides=('-y',))
wall_z(30.0, 6.0, 36.0, 0.01, 6.0)                                                 # its south wall (the escalator room's back)
wall_x(6.0, 30.0, 52.0, 0.01, 6.0)                                                 # its west wall (the museum strip)
# ============ the museum hall (x -36..0, z 0..52, and the strip x 0..6, z 30..52) ============
slab(-36.0, 0.0, 0.0, 52.0, 0.01, top='MI_T_terrazzo', under='MI_T_concrete', thick=0.3)
slab(0.0, 6.0, 30.0, 52.0, 0.01, top='MI_T_terrazzo', under='MI_T_concrete', thick=0.3)
# south wall (kerb side): the door opening in the middle (6 m wide, 3.6 high), four tall windows
WIN = [(-33.2, -30.8), (-27.2, -24.8), (-11.2, -8.8), (-5.2, -2.8)]
wall_z(0.0, -36.0, 0.0, 0.01, 12.0, holes=[(-20.6, -15.4, 0.0, 3.14)] + [(w0, w1, 2.5, 9.5) for (w0, w1) in WIN])
for (w0, w1) in WIN: G.box(w0, w1, 2.5, 9.5, -0.012, 0.012, 'MI_T_glass')
# the door itself is the main entrance's double sliding door (Tools/terminal_edit.py); a canopy over it
S.box(-22.0, -14.0, 3.9, 4.1, -2.5, 0.0, W)
# west wall: tall windows every 6.5 m
wall_x(-36.0, 0.0, 52.0, 0.01, 12.0, holes=[(z0, z0 + 2.4, 2.5, 9.5) for z0 in (4.0, 10.5, 17.0, 23.5, 30.0, 36.5, 43.0)])
for z0 in (4.0, 10.5, 17.0, 23.5, 30.0, 36.5, 43.0): G.box(-36.012, -35.988, 2.5, 9.5, z0, z0 + 2.4, 'MI_T_glass')
# the museum strip's south wall above the escalator room's (Floor 2: x 0..6 at z 30 is already the full-height wall)
# the roof at +12 with its ceiling (the hall and the strip)
S.box(-36.0, 0.0, 11.4, 12.0, 0.0, 52.0, W); S.box(-36.0, 0.0, 11.32, 11.4, 0.0, 52.0, 'MI_T_offwhite', sides=('-y',))
S.box(0.0, 6.0, 11.4, 12.0, 30.0, 52.0, W); S.box(0.0, 6.0, 11.32, 11.4, 30.0, 52.0, 'MI_T_offwhite', sides=('-y',))
# the museum strip's west face on Floor 1 and 2 against the link is the link's own wall (x 6, Floor 2) and the plant
# room's new west wall (Floor 1)

# ============ tax free and the airside landing, their west 16 m (x 56..72) ============
S.box(56.0, 72.0, 5.99, 6.01, 30.0, 42.0, 'MI_T_tile', sides=('+y',))
S.box(56.0, 72.0, 5.63, 5.99, 30.0, 52.0, 'MI_T_concrete', sides=('-y',))
S.box(56.0, 72.0, 5.99, 6.01, 42.0, 52.0, 'MI_T_terrazzo', sides=('+y',))
S.box(56.0, 72.0, 11.32, 11.4, 30.0, 52.0, 'MI_T_offwhite', sides=('-y',))
S.box(56.0, 72.0, 11.4, 12.0, 30.0, 52.0, W)
glass_front_z(30.0, 56.0, 72.0, 6.01, 11.4, transoms=())                            # tax free's glass onto the Grand Hall
# between tax free and the landing: a wall with a wide open doorway (x 60..66)
wall_z(42.0, 56.0, 72.0, 6.01, 11.4, m='MI_T_plaster', holes=[(60.0, 66.0, 6.0, 9.6)])

# ============ the offices on Floor 2 over the staff wing (x 114..156, z 0..52) ============
OX0, OX1, OZ0, OZ1 = 114.0, 156.0, 0.0, 52.0
STAIR = (140.0, 143.0, 43.0, 51.0)            # the staff stairs coming up (S-STAFF)
SHAFT = (152.4, 155.8, 47.6, 51.4)            # the lift, in the staff entrance hall's north-east corner
def floor_with_holes(x0, x1, z0, z1, holes, top, y):
    xs = sorted({x0, x1} | {h[0] for h in holes} | {h[1] for h in holes})
    zs = sorted({z0, z1} | {h[2] for h in holes} | {h[3] for h in holes})
    for xa, xb in zip(xs, xs[1:]):
        for za, zb in zip(zs, zs[1:]):
            if any(h[0] <= xa and xb <= h[1] and h[2] <= za and zb <= h[3] for h in holes): continue
            S.box(xa, xb, y - 0.02, y, za, zb, top, sides=('+y',))
            S.box(xa, xb, y - 0.38, y - 0.02, za, zb, 'MI_T_concrete', sides=('-y',))
floor_with_holes(OX0, OX1, OZ0, OZ1, [STAIR, SHAFT], 'MI_T_carpetGrey', 6.01)
for h in (STAIR, SHAFT):                       # the openings' edges
    S.box(h[0], h[1], 5.63, 6.01, h[2] - 0.02, h[2], W); S.box(h[0], h[1], 5.63, 6.01, h[3], h[3] + 0.02, W)
    S.box(h[0] - 0.02, h[0], 5.63, 6.01, h[2], h[3], W); S.box(h[1], h[1] + 0.02, 5.63, 6.01, h[2], h[3], W)
S.box(OX0, OX1, 11.32, 11.4, OZ0, OZ1, 'MI_T_offwhite', sides=('-y',))
S.box(OX0, OX1, 11.4, 12.0, OZ0, OZ1, W)
# the big window onto the Grand Hall (owner: almost the whole wall, ~35 cm of wall at each end, the terminal's glass,
# frames like the main hall's big windows, every pane the same width)
E_, MW, NP = 0.35, 0.12, 10
pane = (30.0 - 2 * E_ - (NP - 1) * MW) / NP
G.box(113.988, 114.012, 6.01, 11.4, E_, 30.0 - E_, 'MI_T_glass')
z = E_
for i in range(NP - 1):
    z += pane; S.box(113.925, 114.075, 6.01, 11.4, z, z + MW, 'MI_T_mullion'); z += MW
S.box(113.925, 114.075, 6.01, 6.09, E_, 30.0 - E_, 'MI_T_mullion'); S.box(113.925, 114.075, 11.32, 11.4, E_, 30.0 - E_, 'MI_T_mullion')
S.box(113.9, 114.1, 11.4, 12.0, 0.0, 30.0, W)
S.box(113.9, 114.1, 6.01, 11.4, 0.0, E_, W); S.box(113.9, 114.1, 6.01, 11.4, 30.0 - E_, 30.0, W)
print('ANNEX the offices window: %d panes of %.3f m, %d frames of %d cm, %.2f m ends' % (NP, pane, NP - 1, MW * 100, E_))
# the kerb-side wall with ten windows, the east wall with six, the wall onto the gate walkway (no way through: staff)
OW = [(OX0 + 1.6 + i * 4.08, OX0 + 1.6 + i * 4.08 + 2.2) for i in range(10)]
wall_z(OZ0, OX0, OX1, 6.01, 12.0, holes=[(w0, w1, 7.3, 9.9) for (w0, w1) in OW])
for (w0, w1) in OW: G.box(w0, w1, 7.3, 9.9, -0.012, 0.012, 'MI_T_glass')
EW = [(3.0 + i * 6.5, 5.2 + i * 6.5) for i in range(6)]
wall_x(OX1, OZ0, OZ1, 6.01, 12.0, holes=[(z0, z1, 7.3, 9.9) for (z0, z1) in EW])
for (z0, z1) in EW: G.box(OX1 - 0.012, OX1 + 0.012, 7.3, 9.9, z0, z1, 'MI_T_glass')
wall_z(OZ1, OX0, OX1, 6.01, 12.0, m='MI_T_plaster')
# the staff stairs' open end on Floor 2: a glass rail with the black trim
S.box(STAIR[0], STAIR[1], 6.01, 6.13, STAIR[3] - 0.16, STAIR[3] - 0.02, 'MI_T_steel')
G.box(STAIR[0], STAIR[1], 6.11, 6.94, STAIR[3] - 0.096, STAIR[3] - 0.084, 'MI_T_glass')
S.box(STAIR[0], STAIR[1], 6.94, 6.99, STAIR[3] - 0.12, STAIR[3] - 0.06, 'MI_T_black')
# the lift shaft: from the staff entrance hall up through the offices, its head above the roof (to +13.2); a door on
# each floor on its west side
LD = (48.9, 50.1)
for (xa, xb, za, zb) in ((SHAFT[0], SHAFT[1], SHAFT[2] - 0.1, SHAFT[2] + 0.1), (SHAFT[0], SHAFT[1], SHAFT[3] - 0.1, SHAFT[3] + 0.1),
                         (SHAFT[1] - 0.1, SHAFT[1] + 0.1, SHAFT[2], SHAFT[3])):
    S.box(xa, xb, 0.01, 13.2, za, zb, 'MI_T_liftBrushed' if False else W)
wall_x(SHAFT[0], SHAFT[2], SHAFT[3], 0.01, 13.2, holes=[(LD[0], LD[1], 0.0, 2.3), (LD[0], LD[1], 6.0, 8.3)])
S.box(SHAFT[0] - 0.1, SHAFT[1] + 0.1, 13.0, 13.2, SHAFT[2] - 0.1, SHAFT[3] + 0.1, W)
for y0 in (0.01, 6.01):                        # stainless door frames
    S.box(SHAFT[0] - 0.14, SHAFT[0] - 0.1, y0, y0 + 2.45, LD[0] - 0.15, LD[0], 'MI_T_steel')
    S.box(SHAFT[0] - 0.14, SHAFT[0] - 0.1, y0, y0 + 2.45, LD[1], LD[1] + 0.15, 'MI_T_steel')
    S.box(SHAFT[0] - 0.14, SHAFT[0] - 0.1, y0 + 2.3, y0 + 2.45, LD[0], LD[1], 'MI_T_steel')

so = S.finish(); go = G.finish()
for o in (so, go):
    print('ANNEX', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'TerminalAnnexWest.blend'))
print('ANNEX saved')
