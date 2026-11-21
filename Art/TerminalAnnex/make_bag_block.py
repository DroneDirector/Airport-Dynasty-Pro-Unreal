# The departure baggage block's room shell (owner 2026-11-21, Blueprints/Terminal/Departure-Baggage-v2.png and its 3D
# concepts): the old office / ops / academy block on Floor 1 (x 114..156, z 0..52) cleared (Tools/terminal_edit.py keeps
# only its facades, slab and ceilings) and rebuilt as
#   the hold-baggage screening room (x 114..141.75, z 0..22.5; two CT lines - the belts are DepartureBelts' own),
#   the level-3 manual search room (x 141.75..150, z 0..15.5), glass-fronted onto the east passage (x 150..156),
#   the early-bag store (x 141.75..150, z 15.5..22.5),
#   the open sorting / make-up hall (x 114..156, z 22.5..40) and the cart lane (x 114..136, z 40..52) with an 8 m
#   ROLLER DOOR onto the baggage hall (x 127.9..135.8 in z 52, 4 m clear: the curtain is ARollerDoor, its guides and
#   hood are here),
#   the staff entrance hall kept (x 136..156, z 40..52: its stair and lift up to the Floor 2 offices) with walls toward
#   the new block (its door to the cart lane where the staff corridor's was),
#   the Grand Hall's east wall (x 114) rebuilt plain (the bags come under it in the tunnel), the tunnel's rise (x 118.9..120.9),
#   and ramp services 13 (x 255.4..283.4, z 68..82) as the departures vehicle passage: its shell rebuilt with a 4 m
#   roller door's opening onto the apron (its opening onto the baggage hall: Art/TerminalAnnex/make_pier_extension.py).
# Also the roller doors' own moving parts (roller_slat, roller_bar: one slat / the bottom bar, 1 m long, for the game's
# instanced curtains). Plan metres (x east, z north, y up), Blender axes (-x, -z, y) as the other annex parts.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_bag_block.py
#   then Tools/import_annex.py annex_bag_solid annex_bag_glass roller_slat roller_bar
import bpy, bmesh, json, math, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
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
        self.want = []        # (face, the way it must face, in Blender's axes): set after Blender's own guess
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
        out = {'-y': (0, 0, -1), '+y': (0, 0, 1), '-x': (1, 0, 0), '+x': (-1, 0, 0), '-z': (0, 1, 0), '+z': (0, -1, 0)}   # (plan -> Blender: x -> -x, z -> -y, y -> z)
        for k, f in F.items():
            if sides == 'all' or k in sides: self.want.append((self.face(f, m), out[k]))
    def finish(self):
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        # every box face out of its box, whatever Blender guessed: a lone floor panel could come out facing down, and you
        # fell through it (owner 2026-11-16: stuck at the escalators' top, the new stair landing)
        flipped = 0
        for f, w in self.want:
            f.normal_update()
            if f.normal.x * w[0] + f.normal.y * w[1] + f.normal.z * w[2] < 0: f.normal_flip(); flipped += 1
        print('ANNEX %s: %d box faces turned the right way round' % (self.name, flipped))
        me = bpy.data.meshes.new(self.name); self.bm.to_mesh(me); self.bm.free()
        for m in self.mats: me.materials.append(mat(m))
        o = bpy.data.objects.new(self.name, me); bpy.context.scene.collection.objects.link(o)
        return o


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


S = Piece('annex_bag_solid'); G = Piece('annex_bag_glass')
PL, CO, ST = 'MI_T_plaster', 'MI_T_concrete', 'MI_T_steel'
F1C = 5.36                                   # the Floor 1 ceilings (the rooms' own, kept)

# ============ the floor (the old rooms' carpets and tiles went with them): epoxy, as the plant room's - open where the
# check-in collector comes up out of its tunnel under the hall (owner 2026-11-21, Checkin-Security-Flow-v1: the tunnel
# 2 m under the hall along z 18.6 rises through the block's floor at about x 120.2 - DepartureBelts.cpp; its tube mouth
# frames the opening) ============
RISE = (118.9, 120.9, 17.8, 19.4)
for (x0, x1, z0, z1) in ((114.1, 156.0, 0.0, RISE[2]), (114.1, 156.0, RISE[3], 40.0), (114.1, RISE[0], RISE[2], RISE[3]),
                         (RISE[1], 156.0, RISE[2], RISE[3]), (114.1, 136.0, 40.0, 51.9)):
    S.box(x0, x1, -0.3, 0.01, z0, z1, 'MI_T_epoxy', sides=('+y',))
x0, x1, z0, z1 = RISE                                                      # the opening's concrete sides, down to the slab's foot
S.box(x0, x1, -0.3, 0.01, z0 - 0.02, z0, 'MI_T_concrete', sides=('+z',)); S.box(x0, x1, -0.3, 0.01, z1, z1 + 0.02, 'MI_T_concrete', sides=('-z',))
S.box(x0 - 0.02, x0, -0.3, 0.01, z0, z1, 'MI_T_concrete', sides=('+x',)); S.box(x1, x1 + 0.02, -0.3, 0.01, z0, z1, 'MI_T_concrete', sides=('-x',))

# ============ the Grand Hall's east wall (x 114, 0..6 m), rebuilt plain: the old staff passage's door closed (the bags
# come under it, in the tunnel - no opening: Checkin-Security-Flow-v1 replaced the overhead link) ============
wall_x(114.0, 0.0, 30.0, 0.0, 6.0, m_in='MI_T_white2')

# ============ hold-baggage screening (x 114..141.75, z 0..22.5) ============
# its north wall onto the sorter: its door (D-F1-HBS-1, x 117.2..118.8) and the screened bags' way out (x 139.6..141.4)
wall_z(22.5, 114.1, 141.85, 0.0, F1C, m=PL, holes=[(117.2, 118.8, 0.0, 2.25), (139.6, 141.4, 0.3, 1.8)])
# its east wall: the two CT lines' rejects into the search room (z 7, 13) and the early bags into their store (z 19)
wall_x(141.75, 0.0, 22.6, 0.0, F1C, m_in=PL, holes=[(6.2, 7.8, 0.3, 1.5), (12.2, 13.8, 0.3, 1.5), (18.2, 19.8, 0.3, 1.5)])
# ============ the level-3 search room (x 141.75..150, z 0..15.5) and the early-bag store (z 15.5..22.5) ============
wall_z(15.5, 141.85, 150.0, 0.0, F1C, m=PL)
wall_x(150.0, 15.4, 22.6, 0.0, F1C, m_in=PL, holes=[(18.5, 19.5, 0.0, 2.25)])     # the store's door (D-F1-EBS-1)
# the search room's glass front onto the east passage: panes to 3 m, its door (D-F1-L3-1, z 12.6..13.6), plaster above
GD = (12.6, 13.6)
for (za, zb) in ((0.0, GD[0]), (GD[1], 15.4)):
    G.box(149.988, 150.012, 0.0, 3.0, za, zb, 'MI_T_glass')
G.box(149.988, 150.012, 2.29, 3.0, GD[0], GD[1], 'MI_T_glass')
S.box(149.9, 150.1, 3.0, F1C, 0.0, 15.4, PL)
for z in [0.06 + i * (GD[0] - 0.1) / 6 for i in range(7)] + [GD[1] + 0.04, 15.36]:
    S.box(149.925, 150.075, 0.0, 3.0, z - 0.04, z + 0.04, 'MI_T_mullion')
S.box(149.925, 150.075, 0.0, 0.08, 0.0, GD[0], 'MI_T_mullion'); S.box(149.925, 150.075, 0.0, 0.08, GD[1], 15.4, 'MI_T_mullion')
S.box(149.925, 150.075, 2.92, 3.0, 0.0, 15.4, 'MI_T_mullion'); S.box(149.925, 150.075, 2.25, 2.29, GD[0], GD[1], 'MI_T_mullion')

# ============ the staff entrance hall (x 136..156, z 40..52) kept: its walls toward the block (its door to the cart
# lane, D-F1-STAFF-ENTR-1, where the staff corridor's was) ============
wall_z(40.0, 136.0, 156.0, 0.0, F1C, m=PL)
wall_x(136.0, 40.1, 51.9, 0.0, F1C, m_in=PL, holes=[(45.2, 46.8, 0.0, 2.25)])

# ============ the cart lane's wall onto the baggage hall (z 52) and its 8 m roller door ============
RD = (127.9, 135.8, 4.1)                                                   # its opening (x) and height
wall_z(52.0, 114.1, 136.0, 0.0, 5.4, m=CO, holes=[(RD[0], RD[1], 0.0, RD[2])])
def roller_frame(x0, x1, h, z, inward):
    """A roller door's two side guides and its coil hood, on the side `inward` (+1 / -1 in z) of the wall face at z."""
    zi = z + inward * 0.25
    za, zb = min(z, zi), max(z, zi)
    S.box(x0 - 0.2, x0, 0.0, h, za, zb, ST); S.box(x1, x1 + 0.2, 0.0, h, za, zb, ST)
    hz = z + inward * 0.7
    S.box(x0 - 0.25, x1 + 0.25, h, h + 0.75, min(z, hz), max(z, hz), ST)
roller_frame(RD[0], RD[1] - 0.1, RD[2], 51.9, -1)

# ============ ramp services 13 (x 255.4..283.4, z 68..82): the departures vehicle passage, its shell as before with a
# 4 m roller door's opening onto the apron (x 267.4..271.4) in line with its new door onto the baggage hall ============
RX = (267.4, 271.4, 4.1)
S.box(255.4, 283.4, -0.3, 0.01, 68.0, 82.0, CO, sides=('+y',))
S.box(255.4, 283.4, F1C, F1C + 0.04, 68.0, 82.0, CO, sides=('-y',))
S.box(255.43, 255.55, 0.0, 5.4, 68.0, 82.0, CO)                              # west wall, and its outside
S.box(255.32, 255.40, 0.0, 6.0, 68.0, 82.0, 'MI_T_white2')
S.box(283.3, 283.5, 0.0, 5.4, 68.0, 82.0, CO)                                # east wall (onto ramp services 13 B)
for (x0, x1) in ((255.55, RX[0]), (RX[1], 283.3)):
    S.box(x0, x1, 0.0, 5.4, 81.85, 81.97, CO)                               # the apron wall: inside
for (x0, x1) in ((255.4, RX[0]), (RX[1], 283.4)):
    S.box(x0, x1, 0.0, 6.0, 82.0, 82.08, 'MI_T_white2')                     # and its white outside
S.box(RX[0], RX[1], RX[2], 5.4, 81.85, 81.97, CO); S.box(RX[0], RX[1], RX[2], 6.0, 82.0, 82.08, 'MI_T_white2')
roller_frame(RX[0], RX[1], RX[2], 81.85, -1)
roller_frame(RX[0], RX[1], RX[2], 68.1, +1)

# every flat face, for Tools/check_floors.py
FLAT = []
for P_ in (S, G):
    for f in P_.bm.faces:
        ys_ = [v.co.z for v in f.verts]
        if max(ys_) - min(ys_) < 1e-4:
            xs_ = [-v.co.x for v in f.verts]; zs_ = [-v.co.y for v in f.verts]
            FLAT.append([P_.name, round(min(xs_), 3), round(max(xs_), 3), round(ys_[0], 3), round(min(zs_), 3), round(max(zs_), 3), P_.mats[f.material_index]])
os.makedirs(os.path.join(HERE, '..', '..', 'Saved'), exist_ok=True)
json.dump(FLAT, open(os.path.join(HERE, '..', '..', 'Saved', 'annex-flat-annex_bag.json'), 'w'))
objs = [S.finish(), G.finish()]

# ============ the roller door's moving parts (one slat, the bottom bar), 1 m long along x, from y 0 up, centred in
# their depth: the game stretches them to the door's width (ARollerDoor) ============
def prism(name, prof, m):
    """A bar along Blender x (-0.5..0.5) with the profile [(depth, up)] (Blender y, z)."""
    bm = bmesh.new()
    L = [bm.verts.new((-0.5, d, u)) for (d, u) in prof]; R = [bm.verts.new((0.5, d, u)) for (d, u) in prof]
    n = len(prof)
    for i in range(n): bm.faces.new((L[i], L[(i + 1) % n], R[(i + 1) % n], R[i]))
    bm.faces.new(L[::-1]); bm.faces.new(R)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    uv = bm.loops.layers.uv.new('UVMap')
    for f in bm.faces:
        for l in f.loops: l[uv].uv = (l.vert.co.x + l.vert.co.y, l.vert.co.z)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.materials.append(mat(m))
    o = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(o); return o
# a slat: a curved steel strip 10 cm high, 8 mm thick, bulging 6 mm outward, its interlocking hook at the top
objs.append(prism('roller_slat', [(0.004, 0.0), (0.010, 0.03), (0.010, 0.07), (0.004, 0.092), (0.004, 0.1), (-0.004, 0.1),
                                  (-0.004, 0.092), (-0.002, 0.07), (-0.002, 0.03), (-0.004, 0.0)], 'MI_T_steel'))
# the bottom bar: an aluminium T, 6 cm high, 6 cm across its foot
objs.append(prism('roller_bar', [(0.03, 0.0), (0.03, 0.02), (0.012, 0.03), (0.012, 0.06), (-0.012, 0.06), (-0.012, 0.03),
                                 (-0.03, 0.02), (-0.03, 0.0)], 'MI_T_black'))
for o in objs:
    print('BAGBLOCK', o.name, len(o.data.polygons), 'faces', [m.name for m in o.data.materials])
    for x in bpy.context.selected_objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'},
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='FACE', path_mode='STRIP')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'BagBlock.blend'))
print('BAGBLOCK saved')
