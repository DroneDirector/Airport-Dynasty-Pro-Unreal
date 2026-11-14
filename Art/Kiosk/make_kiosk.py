# The self check-in kiosk, rebuilt smooth and to the owner's four photos (Art/Kiosk/References; owner, 2026-11-14:
# "exactly like this, every curvature and height... no weird gaps... use the image as textures"). Our own build (no
# brand: our screen picture and plane logo). Real size in metres, origin on the floor in the middle of the base plate,
# front toward -Y. Measured off the photos (the 1.69 m height and the 64 cm plate as rulers):
#   base plate 64 x 68 cm, 1.2 cm; the column to 95 cm, 49 cm wide at the floor flaring to 53, its front leaning out 4.5 cm,
#   big chamfers front and back; the door 9-71 cm, the bag-tag slot at 75; the counter's bay 89-115.5 cm, 44 cm wide;
#   the head round the screen 50 cm wide at the front (46 at the back), its back almost upright, its front the screen's
#   plane leaning back 20.5 degrees (25 cm deep at the bottom of the screen, 11.5 at the top); the screen 1.19-1.555 m;
#   the handle a hooked loop to 1.69 m; at the back a vent grid 7-22 cm, a lock at 81 cm, the type label.
# Smooth everywhere: rounded corners on every cross-section (8 segments), the shells lofted every 1.5-2 cm, wide
# rounded edges (10 segments) on the counter, the handle a fine round loop; the column, the shoulder and the head are
# one continuous shell (no gaps). Textures from the photos: the device bay (printer, passport scanner, payment terminal)
# and the rear vents; the screen keeps our own picture.
# Writes Art/Kiosk/SM_CheckinKiosk.blend, .glb, .fbx and kiosk_parts.glb (Tools/find_zfight.py).
#   "C:/Program Files/Blender Foundation/Blender 3.3/blender.exe" -b --python Art/Kiosk/make_kiosk.py
import bpy, bmesh, math, os
from mathutils import Vector, Matrix

OUT = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
scn = bpy.context.scene

# #region Materials (the game puts them on the terminal's masters: Tools/set_kiosk.py)
def pmat(name, col, rough=0.4, metal=0.0, coat=0.0, emit=None, emit_str=0.0, tex=None, emit_tex=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    if 'Clearcoat' in b.inputs: b.inputs['Clearcoat'].default_value = coat
    if tex:
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(OUT, tex))
        nt.links.new(t.outputs['Color'], b.inputs['Base Color'])
    if emit_tex:
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(OUT, emit_tex))
        nt.links.new(t.outputs['Color'], b.inputs['Emission']); b.inputs['Emission Strength'].default_value = emit_str
    elif emit:
        b.inputs['Emission'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = emit_str
    return m

M = {
    'white': pmat('M_Kiosk_WhiteCoat', (0.82, 0.83, 0.83), 0.32, coat=0.35),
    'grey': pmat('M_Kiosk_HandleGrey', (0.16, 0.17, 0.18), 0.42),
    'black': pmat('M_Kiosk_BlackPlastic', (0.018, 0.018, 0.02), 0.45),
    'glass': pmat('M_Kiosk_BezelGlass', (0.006, 0.006, 0.008), 0.03, coat=1.0),
    'screen': pmat('M_Kiosk_Screen', (0.01, 0.012, 0.02), 0.04, coat=1.0, emit_tex='T_Kiosk_ScreenUI.png', emit_str=2.2),
    'green': pmat('M_Kiosk_GreenLight', (0.1, 0.9, 0.45), 0.3, emit=(0.15, 1.0, 0.45), emit_str=8.0),
    'plate': pmat('M_Kiosk_BasePlate', (0.07, 0.075, 0.08), 0.5, metal=0.6),
    'metal': pmat('M_Kiosk_Steel', (0.7, 0.7, 0.72), 0.25, metal=1.0),
    'label': pmat('M_Kiosk_Label', (1, 1, 1), 0.6, tex='T_Kiosk_Label.png'),
    'vent': pmat('M_Kiosk_Vent', (0.03, 0.03, 0.035), 0.6),
    'bay': pmat('M_Kiosk_Bay', (1, 1, 1), 0.45, tex='T_Kiosk_Bay.png'),           # (from the front photo)
    'ventpanel': pmat('M_Kiosk_VentPanel', (1, 1, 1), 0.4, tex='T_Kiosk_Vents.png'),  # (from the back photo)
}
# #endregion

# #region Helpers
PARTS = []
def link(o):
    if o.name not in scn.collection.objects: scn.collection.objects.link(o)
    return o

def finish(o, mat, smooth=True, angle=40):
    link(o); o.data.materials.clear(); o.data.materials.append(mat)
    if o.type == 'MESH':
        for p in o.data.polygons: p.use_smooth = smooth
        o.data.use_auto_smooth = True; o.data.auto_smooth_angle = math.radians(angle)
    PARTS.append(o); return o

def mesh_obj(name, bm):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    return bpy.data.objects.new(name, me)

def apply_mods(o):
    bpy.context.view_layer.objects.active = o
    for m in list(o.modifiers): bpy.ops.object.modifier_apply(modifier=m.name)

def bevel(o, width, segs=10, angle=30):
    b = o.modifiers.new('B', 'BEVEL'); b.width = width; b.segments = segs; b.limit_method = 'ANGLE'
    b.angle_limit = math.radians(angle); b.profile = 0.5
    apply_mods(o)

def fillet(poly, r, segs=8):
    """a closed polygon [(a, b)...] with every corner rounded (a smooth curve tangent to both sides)"""
    out = []; n = len(poly)
    for i in range(n):
        p0, p1, p2 = Vector(poly[i - 1]), Vector(poly[i]), Vector(poly[(i + 1) % n])
        d0, d1 = (p0 - p1).normalized(), (p2 - p1).normalized()
        ang = math.acos(max(-1.0, min(1.0, d0.dot(d1))))
        t = r / math.tan(ang / 2) if ang > 1e-3 else 0.0
        t = min(t, (p0 - p1).length * 0.45, (p2 - p1).length * 0.45)
        a, b = p1 + d0 * t, p1 + d1 * t
        for k in range(segs + 1):
            s = k / segs; q = a * (1 - s) ** 2 + p1 * 2 * s * (1 - s) + b * s * s
            out.append((q.x, q.y))
    return out

def loft(name, rings, cap_bottom=True, cap_top=True):
    bm = bmesh.new(); vs = [[bm.verts.new(p) for p in r] for r in rings]; n = len(rings[0])
    for a, b in zip(vs, vs[1:]):
        for i in range(n): bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    if cap_bottom: bm.faces.new(list(reversed(vs[0])))
    if cap_top: bm.faces.new(vs[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return mesh_obj(name, bm)

def extrude_x(name, prof, x0, x1):
    bm = bmesh.new(); n = len(prof)
    a = [bm.verts.new((x0, y, z)) for y, z in prof]; b = [bm.verts.new((x1, y, z)) for y, z in prof]
    for i in range(n): bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    bm.faces.new(list(reversed(a))); bm.faces.new(b)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return mesh_obj(name, bm)

def rbox(name, size, loc, mat, r, rot=(0, 0, 0), segs=6, keep=True):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0); bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    o = link(mesh_obj(name, bm))
    if r: bevel(o, r, segs, 40)
    o.location = loc; o.rotation_euler = rot
    return finish(o, mat) if keep else o

def cyl(name, rad, h, loc, mat, rot=(0, 0, 0), verts=48, r=0.0):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=rad, radius2=rad, depth=h)
    o = link(mesh_obj(name, bm))
    if r: bevel(o, r, 4, 40)
    o.location = loc; o.rotation_euler = rot
    return finish(o, mat)

def tube(name, pts, radius, mat, res=48, prof_res=10):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; cu.resolution_u = res
    cu.bevel_mode = 'ROUND'; cu.bevel_depth = radius; cu.bevel_resolution = prof_res; cu.use_fill_caps = True
    sp = cu.splines.new('BEZIER'); sp.bezier_points.add(len(pts) - 1)
    for bp, p in zip(sp.bezier_points, pts): bp.co = p; bp.handle_left_type = bp.handle_right_type = 'AUTO'
    o = link(bpy.data.objects.new(name, cu))
    bpy.ops.object.select_all(action='DESELECT'); bpy.context.view_layer.objects.active = o; o.select_set(True)
    bpy.ops.object.convert(target='MESH'); o.select_set(False)
    return finish(o, mat, angle=70)

def planar_uv(o, u_axis, u0, u1, v_axis, v0, v1, sides=None):
    """UVs projected straight from one side (the photo texture laid on as the photo saw it); faces turned away from that
    side (edge-on to it: the projection would smear) get `sides`, one plain spot of the texture, instead"""
    me = o.data
    if not me.uv_layers: me.uv_layers.new()
    uv = me.uv_layers.active.data; mw = o.matrix_world
    for p in me.polygons:
        n = (mw.to_3x3() @ p.normal).normalized()
        if sides and abs(n[u_axis]) > 0.6:
            for li in p.loop_indices: uv[li].uv = sides
            continue
        for li in p.loop_indices:
            w = mw @ me.vertices[me.loops[li].vertex_index].co
            uv[li].uv = ((w[u_axis] - u0) / (u1 - u0), (w[v_axis] - v0) / (v1 - v0))

def ease(t): t = min(1.0, max(0.0, t)); return t * t * (3 - 2 * t)
# #endregion

# #region Base plate: dark steel, rounded corners, a soft top edge
bm = bmesh.new()
pl = fillet([(-0.32, -0.34), (0.32, -0.34), (0.32, 0.34), (-0.32, 0.34)], 0.05, 12)
bot = [bm.verts.new((x, y, 0.0)) for x, y in pl]; top = [bm.verts.new((x, y, 0.012)) for x, y in pl]
for i in range(len(pl)): bm.faces.new((bot[i], bot[(i + 1) % len(pl)], top[(i + 1) % len(pl)], top[i]))
bm.faces.new(list(reversed(bot))); bm.faces.new(top); bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
o = link(mesh_obj('Kiosk_BasePlate', bm)); bevel(o, 0.004, 4, 40); finish(o, M['plate'])
# #endregion

# #region The white shell: the column (to 95 cm), the shoulder and the head round the screen - one smooth piece
Z0, ZC = 0.011, 0.95           # the column's foot (just into the plate) and top
def col_section(z):
    t = (z - Z0) / (ZC - Z0); e = t ** 1.6
    hw = 0.235 + 0.045 * e; hf = 0.215 + 0.065 * e      # half widths at the back and the front (47 cm at the floor, 56 at the top)
    yf = -0.165 - 0.045 * e; yb = 0.165                  # the front face (leaning out going up) and the back
    c1, c2 = 0.07, 0.065                                 # the chamfers, front and back
    return [(-hf + c1, yf), (hf - c1, yf), (hf, yf + c1), (hw, yb - c2), (hw - c2, yb), (-hw + c2, yb), (-hw, yb - c2), (-hf, yf + c1)]
def front_y(z): t = (z - Z0) / (ZC - Z0); return -0.165 - 0.045 * t ** 1.6
# the head: its back almost upright, its front the screen's plane leaning back 20.5 degrees
SCR_TILT = math.radians(20.5)
def head_back(z): return 0.15 - 0.025 * (z - 0.95) / 0.62
def face_y(z): return -0.10 + (z - 1.19) * math.tan(SCR_TILT)     # the screen's front plane
def head_section(z):
    yf, yb = face_y(z) + 0.004, head_back(z)
    return [(-0.25, yf), (0.25, yf), (0.25, yf + 0.03), (0.23, yb - 0.03), (0.20, yb), (-0.20, yb), (-0.23, yb - 0.03), (-0.25, yf + 0.03)]   # (50 cm at the front, 46 at the back)
# the device deck (owner, 2026-11-14, his photo marked): open at the front between the white side cheeks, sloping back
# up toward the screen (36 degrees), a small wall at its back, the screen overhanging it
DK0, DK1 = (-0.245, 0.905), (-0.03, 1.062)            # the deck's front edge and its top at the back wall (y, z)
DECK_A = math.atan2(DK1[1] - DK0[1], DK1[0] - DK0[0])
def deck_y(z): return DK0[0] + (z - DK0[1]) / math.tan(DECK_A)
def front_limit(z):
    """how far forward the shell may come at z (below the deck's front edge: nowhere held back)"""
    if z < DK0[1]: return -9.0
    if z < DK1[1]: return deck_y(z) + 0.012          # (well under the deck plate: never touching it)
    if z < 1.165: return DK1[0] + 0.004
    w = ease((z - 1.165) / 0.022); return (DK1[0] + 0.004) * (1 - w) + (face_y(z) + 0.004) * w
NSEC = 72   # points round a section (each polygon resampled to the same count, so lofting can blend them)
def resample(poly2d, n=NSEC, r=0.026):
    pts = fillet(poly2d, r, 8); pts.append(pts[0])
    L = [0.0]
    for a, b in zip(pts, pts[1:]): L.append(L[-1] + math.dist(a, b))
    out, j = [], 0
    for k in range(n):
        s = L[-1] * k / n
        while L[j + 1] < s: j += 1
        f = (s - L[j]) / max(L[j + 1] - L[j], 1e-9)
        out.append((pts[j][0] + (pts[j + 1][0] - pts[j][0]) * f, pts[j][1] + (pts[j + 1][1] - pts[j][1]) * f))
    # start every section at its front middle (so the blend doesn't twist)
    i0 = min(range(n), key=lambda i: out[i][1] + 4.0 * abs(out[i][0]))   # (the front-most point near the middle: never the back's)
    return out[i0:] + out[:i0]
rings = []
for k in range(56):                                    # the column, every 1.7 cm
    z = Z0 + (ZC - Z0) * k / 55
    rings.append([(x, max(y, front_limit(z)), z) for x, y in resample(col_section(z))])
ZS, ZTOP = 1.06, 1.585                                 # the shoulder blends into the head by 1.06; the head's top
for k in range(1, 40):                                 # the shoulder and the head, every 1.6 cm
    z = ZC + (ZTOP - ZC) * k / 39
    a = resample(col_section(ZC)); b = resample(head_section(z)); w = ease((z - ZC) / (ZS - ZC))
    rings.append([(pa[0] + (pb[0] - pa[0]) * w, max(pa[1] + (pb[1] - pa[1]) * w, front_limit(z)), z) for pa, pb in zip(a, b)])
# the head's top rounded over (rings drawn in toward its middle)
top = rings[-1]; cx = sum(p[0] for p in top) / len(top); cy = sum(p[1] for p in top) / len(top)
for k in range(1, 7):
    a = k / 6 * math.pi / 2; s = math.cos(a) ** 0.8; dz = 0.022 * math.sin(a)
    rings.append([(cx + (p[0] - cx) * (0.88 + 0.12 * s), cy + (p[1] - cy) * (0.80 + 0.20 * s), ZTOP + dz) for p in top])
shell = loft('Kiosk_Shell', rings); finish(shell, M['white'], angle=35)
# #endregion

# #region The front of the column: the door's thin seam, its lock, the bag-tag slot
def on_front(x, z, out=0.0): return (x, front_y(z) - out, z)
def lean(z, dz=0.01): return math.atan2(front_y(z + dz) - front_y(z - dz), 2 * dz)
for nm, pts in (('L', [(-0.165, 0.09), (-0.165, 0.40), (-0.165, 0.71)]), ('R', [(0.165, 0.09), (0.165, 0.40), (0.165, 0.71)]),
                ('B', [(-0.165, 0.09), (0.0, 0.09), (0.165, 0.09)]), ('T', [(-0.165, 0.71), (0.0, 0.71), (0.165, 0.71)])):
    tube('Kiosk_DoorSeam' + nm, [on_front(x, z, 0.0005) for x, z in pts], 0.0011, M['vent'], res=16, prof_res=2)
cyl('Kiosk_DoorLock', 0.0095, 0.008, on_front(0.02, 0.60, 0.0015), M['metal'], rot=(math.pi / 2 - lean(0.60), 0, 0), r=0.0015)
cyl('Kiosk_DoorLockSlot', 0.0022, 0.004, on_front(0.02, 0.60, 0.0058), M['black'], rot=(math.pi / 2 - lean(0.60), 0, 0), verts=16)
rbox('Kiosk_TagSlotFrame', (0.19, 0.012, 0.03), on_front(-0.03, 0.75, 0.002), M['black'], 0.006, rot=(-lean(0.75), 0, 0))
rbox('Kiosk_TagSlot', (0.16, 0.020, 0.005), on_front(-0.03, 0.75, 0.0035), M['vent'], 0.002, rot=(-lean(0.75), 0, 0))
# #endregion

# #region The counter: the device deck open at the front between white side cheeks, sloping back up to a small wall
# under the screen (owner, 2026-11-14), the deck, the wall and the devices carrying the photo of the real bay
YF0 = front_y(0.84)
# under the deck: white, the column's front running on up to the deck's front edge
# (the kiosk's left, +x, has a wall from the lip up to the screen; its right, -x, is open: the deck seen from the side)
blk = link(extrude_x('Kiosk_DeckBase', fillet([(YF0 + 0.004, 0.84), (DK0[0], 0.885), (DK0[0], DK0[1]), (DK1[0], DK1[1]), (0.03, DK1[1]), (0.03, 0.84)], 0.012, 8), -0.281, 0.236))
bevel(blk, 0.006, 6, 35); finish(blk, M['white'], angle=35)
# the side cheeks: white, from the column's front up the deck's sides to the screen's bottom
for sgn in (1,):
    x0, x1 = 0.234, 0.286
    ch = link(extrude_x('Kiosk_Cheek' + ('R' if sgn > 0 else 'L'), fillet([(YF0 + 0.004, 0.84), (-0.25, 0.885), (-0.255, 1.10), (face_y(1.18) + 0.003, 1.182), (0.02, 1.19), (0.02, 0.84)], 0.016, 8), x0, x1))
    bevel(ch, 0.012, 10, 30); finish(ch, M['white'], angle=35)
BX, BZ0, BZ1 = 0.22, 0.89, 1.155                       # (the photo crop's frame: the bay's half width, bottom, top)
# the deck plate and the wall at its back (6 mm, black, the photo on them)
L = math.dist(DK0, DK1)
deck = rbox('Kiosk_Deck', (0.512, L + 0.004, 0.006), (-0.023, (DK0[0] + DK1[0]) / 2 + 0.0025 * math.sin(DECK_A), (DK0[1] + DK1[1]) / 2 + 0.003 * math.cos(DECK_A)), M['bay'], 0.002, rot=(DECK_A, 0, 0))
wall = rbox('Kiosk_DeckWall', (0.466, 0.006, 1.168 - DK1[1]), (0, DK1[0] + 0.003, (1.168 + DK1[1]) / 2), M['bay'], 0.002)
def on_deck(x, s, h):   # a point s m up the deck from its front edge, h above it
    return (x, DK0[0] + s * math.cos(DECK_A) - h * math.sin(DECK_A), DK0[1] + s * math.sin(DECK_A) + h * math.cos(DECK_A))
devs = [rbox('Kiosk_Printer', (0.13, 0.08, 0.03), on_deck(-0.12, 0.07, 0.021), M['bay'], 0.006, (DECK_A, 0, 0)),
        rbox('Kiosk_Scanner', (0.125, 0.09, 0.04), on_deck(0.015, 0.075, 0.026), M['bay'], 0.008, (DECK_A, 0, 0)),
        rbox('Kiosk_PinPad', (0.11, 0.12, 0.03), on_deck(0.15, 0.085, 0.028), M['bay'], 0.009, (DECK_A + 0.15, 0, 0)),
        rbox('Kiosk_BarcodeHolder', (0.10, 0.06, 0.09), (-0.155, DK1[0] - 0.032, 1.105), M['bay'], 0.008)]   # (2 mm clear of the wall)
for o in [deck, wall] + devs: planar_uv(o, 0, -BX, BX, 2, BZ0, BZ1, sides=(0.5, 0.78))   # (sides: the photo's black back)
# #endregion

# #region The screen: black glass inset in the head's front, the touch screen under it, a white border round it
def scr(p):    # screen space: x across, y out of the screen (toward the passenger), z up its face; at the face's middle
    c = Vector((0, face_y(1.3725), 1.3725)); R = Matrix.Rotation(-SCR_TILT, 4, 'X')
    return c + R @ Vector((p[0], -p[1], p[2]))
glass = rbox('Kiosk_ScreenGlass', (0.468, 0.008, 0.37), scr((0, 0.002, 0)), M['glass'], 0.004, rot=(-SCR_TILT, 0, 0), segs=4)
bm = bmesh.new(); w2, h2 = 0.205, 0.155
vs = [bm.verts.new(scr(p)) for p in ((-w2, 0.0075, -h2 + 0.01), (w2, 0.0075, -h2 + 0.01), (w2, 0.0075, h2 + 0.01), (-w2, 0.0075, h2 + 0.01))]
f = bm.faces.new(vs); uvl = bm.loops.layers.uv.new()
for l, c in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))): l[uvl].uv = c
bmesh.ops.recalc_face_normals(bm, faces=[f])
if f.normal.y > 0: bmesh.ops.reverse_faces(bm, faces=[f])
finish(mesh_obj('Kiosk_Display', bm), M['screen'], smooth=False)
# #endregion

# #region The handle: a dark grey hooked loop over the head (front and back arms at each side, a bar across the top),
# the green status lamp along the top
HX, HZ = 0.236, 1.662   # (the green lamp's top at 1.69 m)
for s in (-1, 1):
    x = s * HX
    tube('Kiosk_HandleSide' + ('L' if s < 0 else 'R'),
         [(x, face_y(1.535) + 0.012, 1.535), (x, face_y(1.60) + 0.02, 1.62), (x * 0.97, 0.005, HZ - 0.005), (x * 0.94, 0.06, HZ - 0.03), (x * 0.90, 0.11, 1.59), (x * 0.88, head_back(1.555) - 0.02, 1.555)],
         0.021, M['grey'])
tube('Kiosk_HandleTop', [(-HX * 0.97, 0.005, HZ - 0.005), (-HX * 0.80, 0.003, HZ + 0.002), (0, 0.0, HZ + 0.004), (HX * 0.80, 0.003, HZ + 0.002), (HX * 0.97, 0.005, HZ - 0.005)], 0.0215, M['grey'])
# the green lamp follows the whole handle (owner, 2026-11-14): along the top of the top bar, round both corners and down
# the outside of both side hooks to the kiosk, front and back - set out from each tube's middle line by 19.5 mm (the lamp
# standing 4 mm proud of the grey), turned from straight up on the bar to straight out on the hooks, one piece per half
# meeting the bar's half at the same corner point (no gap)
LAMP_OFF, LAMP_R = 0.0195, 0.0062
def lamp_pts(pts, dirs):
    return [(p[0] + LAMP_OFF * d[0], p[1] + LAMP_OFF * d[1], p[2] + LAMP_OFF * d[2]) for p, d in zip(pts, dirs)]
C45 = math.sqrt(0.5)
for s in (-1, 1):
    x = s * HX
    side = [(x, face_y(1.535) + 0.012, 1.535), (x, face_y(1.60) + 0.02, 1.62), (x * 0.97, 0.005, HZ - 0.005), (x * 0.94, 0.06, HZ - 0.03), (x * 0.90, 0.11, 1.59), (x * 0.88, head_back(1.555) - 0.02, 1.555)]
    out = (s, 0, 0); corner = (s * C45, 0, C45)
    tube('Kiosk_StatusLightSide' + ('L' if s < 0 else 'R'), lamp_pts(side, [out, (s * 0.9, 0, 0.44), corner, (s * 0.9, 0, 0.44), out, out]), LAMP_R, M['green'], res=32, prof_res=6)
top = [(-HX * 0.97, 0.005, HZ - 0.005), (-HX * 0.80, 0.003, HZ + 0.002), (0, 0.0, HZ + 0.004), (HX * 0.80, 0.003, HZ + 0.002), (HX * 0.97, 0.005, HZ - 0.005)]
tube('Kiosk_StatusLight', lamp_pts(top, [(-C45, 0, C45), (-0.3, 0, 0.95), (0, 0, 1), (0.3, 0, 0.95), (C45, 0, C45)]), LAMP_R, M['green'], res=32, prof_res=6)
# #endregion

# #region The back: the vent grid (textured from the back photo), the lock, our type label
vp = rbox('Kiosk_VentPanel', (0.25, 0.001, 0.15), (0, 0.165 + 0.0017, 0.145), M['ventpanel'], 0.0)
planar_uv(vp, 0, 0.125, -0.125, 2, 0.07, 0.22)
cyl('Kiosk_BackLock', 0.0095, 0.008, (0, 0.165 + 0.0055, 0.81), M['metal'], rot=(math.pi / 2, 0, 0), r=0.0015)
lab = rbox('Kiosk_Label', (0.11, 0.001, 0.035), (0, head_back(1.10) + 0.0035, 1.10), M['label'], 0.0)   # (the back leans: 3.5 mm out, clear of it top to bottom)
planar_uv(lab, 0, 0.055, -0.055, 2, 1.10 - 0.0175, 1.10 + 0.0175)
# #endregion

# #region Finish: the parts file (flicker check), one object, export
for o in PARTS:
    if o.type == 'MESH' and not o.data.uv_layers: o.data.uv_layers.new()
bpy.ops.object.select_all(action='DESELECT')
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, 'kiosk_parts.glb'), export_format='GLB', use_selection=False, export_apply=True)
for o in PARTS: o.select_set(True)
bpy.context.view_layer.objects.active = shell
bpy.ops.object.join()
k = bpy.context.view_layer.objects.active; k.name = 'SM_CheckinKiosk'; k.data.name = 'SM_CheckinKiosk'
assert all(s.material for s in k.material_slots), 'an empty material slot'
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'SM_CheckinKiosk.blend'))
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, 'SM_CheckinKiosk.glb'), export_format='GLB', use_selection=True, export_apply=True)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, 'SM_CheckinKiosk.fbx'), use_selection=True, apply_unit_scale=True, axis_forward='-Y', axis_up='Z', path_mode='COPY', embed_textures=True)
d = k.dimensions
print('KIOSK DONE dims %.3f %.3f %.3f tris %d slots %s' % (d.x, d.y, d.z, sum(len(p.vertices) - 2 for p in k.data.polygons), [s.material.name for s in k.material_slots]))
# #endregion
