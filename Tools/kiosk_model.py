# Self check-in kiosk for the terminal's main hall: real size in metres, origin at the floor in the middle of the base
# plate, front towards -Y. Our own design after the owner's reference photos (no brand from the photos).
import bpy, bmesh, math, os
from mathutils import Vector, Matrix

OUT = os.path.dirname(os.path.abspath(__file__))
bpy.ops.wm.read_factory_settings(use_empty=True)
scn = bpy.context.scene
scn.unit_settings.system = 'METRIC'

# #region ── Materials ──
def pmat(name, col, rough=0.4, metal=0.0, coat=0.0, coat_rough=0.03, emit=None, emit_str=0.0, tex=None, emit_tex=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; b = nt.nodes['Principled BSDF']
    b.inputs['Base Color'].default_value = (*col, 1); b.inputs['Roughness'].default_value = rough
    b.inputs['Metallic'].default_value = metal
    b.inputs['Coat Weight'].default_value = coat; b.inputs['Coat Roughness'].default_value = coat_rough
    if tex:
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(OUT, tex))
        nt.links.new(t.outputs['Color'], b.inputs['Base Color'])
    if emit_tex:
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(os.path.join(OUT, emit_tex))
        nt.links.new(t.outputs['Color'], b.inputs['Emission Color']); b.inputs['Emission Strength'].default_value = emit_str
    elif emit:
        b.inputs['Emission Color'].default_value = (*emit, 1); b.inputs['Emission Strength'].default_value = emit_str
    return m

M = {
    'white':  pmat('M_Kiosk_WhiteCoat', (0.82, 0.83, 0.83), 0.32, coat=0.35, coat_rough=0.12),
    'grey':   pmat('M_Kiosk_HandleGrey', (0.16, 0.17, 0.18), 0.42),
    'black':  pmat('M_Kiosk_BlackPlastic', (0.018, 0.018, 0.02), 0.45),
    'glass':  pmat('M_Kiosk_BezelGlass', (0.006, 0.006, 0.008), 0.03, coat=1.0, coat_rough=0.01),
    'screen': pmat('M_Kiosk_Screen', (0.01, 0.012, 0.02), 0.04, coat=1.0, coat_rough=0.01, emit_tex='screen_ui.png', emit_str=2.2),
    'green':  pmat('M_Kiosk_GreenLight', (0.1, 0.9, 0.45), 0.3, emit=(0.15, 1.0, 0.45), emit_str=8.0),
    'plate':  pmat('M_Kiosk_BasePlate', (0.07, 0.075, 0.08), 0.5, metal=0.6),
    'keys':   pmat('M_Kiosk_Keys', (0.32, 0.33, 0.35), 0.5),
    'lcd':    pmat('M_Kiosk_SmallLCD', (0.55, 0.62, 0.6), 0.1, emit=(0.5, 0.6, 0.58), emit_str=0.6),
    'scanglass': pmat('M_Kiosk_ScannerGlass', (0.05, 0.12, 0.1), 0.05, coat=1.0),
    'metal':  pmat('M_Kiosk_Steel', (0.7, 0.7, 0.72), 0.25, metal=1.0),
    'label':  pmat('M_Kiosk_Label', (1, 1, 1), 0.6, tex='label.png'),
    'vent':   pmat('M_Kiosk_Vent', (0.03, 0.03, 0.035), 0.6),
}
# #endregion

# #region ── Helpers ──
def obj_from_bm(name, bm, mat, bevel=0.0, segs=3):
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    o = bpy.data.objects.new(name, me); scn.collection.objects.link(o)
    o.data.materials.append(mat)
    if bevel:
        bv = o.modifiers.new('Bevel', 'BEVEL'); bv.width = bevel; bv.segments = segs; bv.limit_method = 'ANGLE'
        bv.angle_limit = math.radians(30); bv.harden_normals = True
    for p in o.data.polygons: p.use_smooth = True
    return o

def box(name, size, loc, mat, bevel=0.0, rot=(0, 0, 0), segs=3):
    bm = bmesh.new(); bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    o = obj_from_bm(name, bm, mat, bevel, segs); o.location = loc; o.rotation_euler = rot
    return o

def cyl(name, r, h, loc, mat, rot=(0, 0, 0), verts=32, bevel=0.0):
    bm = bmesh.new(); bmesh.ops.create_cone(bm, cap_ends=True, segments=verts, radius1=r, radius2=r, depth=h)
    o = obj_from_bm(name, bm, mat, bevel); o.location = loc; o.rotation_euler = rot
    return o

def loft(name, rings, mat, bevel=0.006):
    """rings: list of lists of (x,y,z), same vertex count, bottom to top; closed rings, capped."""
    bm = bmesh.new(); vs = [[bm.verts.new(p) for p in r] for r in rings]; n = len(rings[0])
    for a, b in zip(vs, vs[1:]):
        for i in range(n):
            bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    bm.faces.new(list(reversed(vs[0]))); bm.faces.new(vs[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj_from_bm(name, bm, mat, bevel)

def octa(w, d, ch, y0, z, front_extra=0.0, wf=None):
    """chamfered rectangle footprint: half width w/2, back at y0+d, front at y0 - front_extra; wf = front half width."""
    hw = w / 2; hf = (wf or w) / 2; yb = y0 + d; yf = y0 - front_extra
    return [(-hf + ch, yf, z), (hf - ch, yf, z), (hf, yf + ch, z), (hw, yb - ch, z), (hw - ch, yb, z), (-hw + ch, yb, z), (-hw, yb - ch, z), (-hf, yf + ch, z)]

def extrude_profile(name, prof, x0, x1, mat, bevel=0.006):
    """side profile [(y,z)...] (closed, counter-clockwise seen from +X) extruded from x0 to x1."""
    bm = bmesh.new()
    a = [bm.verts.new((x0, y, z)) for y, z in prof]; b = [bm.verts.new((x1, y, z)) for y, z in prof]; n = len(prof)
    for i in range(n): bm.faces.new((a[i], a[(i + 1) % n], b[(i + 1) % n], b[i]))
    bm.faces.new(list(reversed(a))); bm.faces.new(b)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return obj_from_bm(name, bm, mat, bevel)
# #endregion

# #region ── Base plate ──
box('Kiosk_BasePlate', (0.64, 0.68, 0.012), (0, 0.0, 0.006), M['plate'], bevel=0.006)
# #endregion

# #region ── Lower body: a faceted column whose front leans out towards the counter ──
Z0 = 0.012
rings = [octa(0.46, 0.34, 0.055, -0.17, Z0),
         octa(0.46, 0.34, 0.055, -0.17, 0.30, 0.004),
         octa(0.47, 0.34, 0.055, -0.17, 0.56, 0.012),
         octa(0.49, 0.34, 0.06, -0.17, 0.80, 0.045, wf=0.50),
         octa(0.50, 0.34, 0.06, -0.17, 0.955, 0.075, wf=0.52)]
body = loft('Kiosk_Body', rings, M['white'], bevel=0.008)

def front_y(z):   # the front face's position at height z (for parts placed on it)
    pts = [(Z0, -0.17), (0.30, -0.174), (0.56, -0.182), (0.80, -0.215), (0.955, -0.245)]
    for (za, ya), (zb, yb) in zip(pts, pts[1:]):
        if za <= z <= zb: return ya + (yb - ya) * (z - za) / (zb - za)
    return pts[-1][1]

# the front door: a panel with a thin dark gap round it, its lock, and the bag-tag printer slot above it
for nm, sx, sz, lx, lz in (('Kiosk_DoorGapL', 0.003, 0.50, -0.185, 0.30), ('Kiosk_DoorGapR', 0.003, 0.50, 0.185, 0.30),
                           ('Kiosk_DoorGapB', 0.37, 0.003, 0, 0.05), ('Kiosk_DoorGapT', 0.37, 0.003, 0, 0.55)):
    box(nm, (sx, 0.004, sz), (lx, front_y(lz) + 0.0005, lz), M['vent'])
cyl('Kiosk_DoorLock', 0.009, 0.006, (0, front_y(0.48) - 0.002, 0.48), M['metal'], rot=(math.pi / 2, 0, 0))
cyl('Kiosk_DoorLockSlot', 0.0018, 0.008, (0, front_y(0.48) - 0.003, 0.48), M['black'], rot=(math.pi / 2, 0, 0), verts=8)
ang = math.atan2(0.215 - 0.182, 0.80 - 0.56)
box('Kiosk_TagSlotFrame', (0.20, 0.012, 0.03), (0, front_y(0.66) - 0.002, 0.66), M['black'], bevel=0.003, rot=(-ang, 0, 0))
box('Kiosk_TagSlot', (0.17, 0.014, 0.005), (0, front_y(0.66) - 0.004, 0.66), M['vent'], rot=(-ang, 0, 0))
# #endregion

# #region ── Counter: the white head with its black bay of devices under the screen ──
ZC0, ZC1 = 0.955, 1.135
YF = -0.245            # front edge of the counter
# side cheeks, bottom sill and the top lip make a white frame round the black bay
for s in (-1, 1):
    extrude_profile('Kiosk_Cheek' + ('L' if s < 0 else 'R'),
                    [(YF, ZC0), (0.17, ZC0), (0.17, ZC1), (-0.08, ZC1 + 0.02), (YF + 0.03, ZC1 - 0.01)],
                    s * 0.26 - (0.045 if s > 0 else 0), s * 0.26 + (0.0 if s > 0 else 0.045), M['white'], 0.006)
extrude_profile('Kiosk_Sill', [(YF, ZC0), (-0.02, ZC0), (-0.02, ZC0 + 0.03), (YF, ZC0 + 0.022)], -0.215, 0.215, M['white'], 0.005)
extrude_profile('Kiosk_Lip', [(YF + 0.03, ZC1 - 0.03), (-0.06, ZC1 - 0.01), (-0.06, ZC1 + 0.02), (YF + 0.03, ZC1 - 0.005)], -0.215, 0.215, M['white'], 0.005)
# the bay itself: a black tray sloping down towards the passenger
tilt = math.radians(14)
extrude_profile('Kiosk_Bay', [(YF + 0.01, ZC0 + 0.022), (0.05, ZC0 + 0.022), (0.05, ZC1 - 0.012), (YF + 0.04, ZC1 - 0.02)], -0.215, 0.215, M['black'], 0.003)
def on_tray(x, y, h):   # a point on the tray floor, raised by h, tilt so devices sit flat on the slope
    return (x, y, ZC0 + 0.03 + (y - YF) * math.tan(tilt) * 0.35 + h)
# receipt printer (left), document/passport scanner (middle), payment terminal (right), barcode camera (back left)
box('Kiosk_Printer', (0.085, 0.07, 0.035), on_tray(-0.135, -0.17, 0.0175), M['black'], 0.004, (tilt, 0, 0))
box('Kiosk_PrinterSlot', (0.06, 0.004, 0.004), on_tray(-0.135, -0.206, 0.028), M['vent'], 0, (tilt, 0, 0))
box('Kiosk_PrinterFace', (0.07, 0.003, 0.02), on_tray(-0.135, -0.2065, 0.016), M['keys'], 0.001, (tilt, 0, 0))
box('Kiosk_Scanner', (0.10, 0.09, 0.04), on_tray(-0.01, -0.16, 0.02), M['black'], 0.005, (tilt, 0, 0))
box('Kiosk_ScannerWindow', (0.07, 0.05, 0.002), on_tray(-0.01, -0.16, 0.041), M['scanglass'], 0.0, (tilt, 0, 0))
box('Kiosk_ScannerLight', (0.05, 0.003, 0.003), on_tray(-0.01, -0.206, 0.03), M['green'], 0.0, (tilt, 0, 0))
pp = on_tray(0.13, -0.15, 0.022)
box('Kiosk_PinPad', (0.075, 0.115, 0.03), pp, M['black'], 0.006, (tilt + math.radians(6), 0, 0))
box('Kiosk_PinScreen', (0.05, 0.035, 0.002), (pp[0], pp[1] + 0.03, pp[2] + 0.02), M['lcd'], 0.0, (tilt + math.radians(6), 0, 0))
for r in range(4):
    for c in range(3):
        box(f'Kiosk_PinKey_{r}{c}', (0.014, 0.01, 0.004),
            (pp[0] - 0.018 + c * 0.018, pp[1] - 0.003 - r * 0.0135, pp[2] + 0.018 + r * 0.0035 * math.sin(tilt)),
            M['keys'] if not (r == 3 and c != 1) else (M['green'] if c == 2 else M['vent']), 0.0015, (tilt + math.radians(6), 0, 0))
box('Kiosk_PinCardSlot', (0.06, 0.004, 0.004), (pp[0], pp[1] - 0.058, pp[2] - 0.006), M['vent'], 0.0, (tilt, 0, 0))
cyl('Kiosk_BarcodeCam', 0.022, 0.07, (-0.15, -0.05, ZC1 - 0.07), M['black'], verts=24, bevel=0.003)
cyl('Kiosk_BarcodeLens', 0.012, 0.004, (-0.15, -0.05, ZC1 - 0.107), M['scanglass'], verts=24)
# #endregion

# #region ── Upper column: the white spine rising behind the screen to the handle ──
prof = []
N = 24
for i in range(N + 1):                      # back line, bottom to top: a gentle inward curve
    t = i / N; z = 0.955 + t * 0.70
    prof.append((0.17 - 0.045 * math.sin(math.pi * t) - 0.02 * t, z))
for i in range(N, -1, -1):                  # front line, top to bottom, behind the screen
    t = i / N; z = 0.955 + t * 0.70
    prof.append((0.03 - 0.04 * math.sin(math.pi * t * 0.9) + 0.02 * t, z))
spine = extrude_profile('Kiosk_Spine', prof, -0.21, 0.21, M['white'], 0.012)
# rear details: vents low down, the lock and our own type label
for r in range(9):
    for c in range(7):
        box(f'Kiosk_Vent_{r}{c}', (0.032, 0.004, 0.006), (-0.12 + c * 0.04, 0.1705, 0.07 + r * 0.02), M['vent'])
cyl('Kiosk_BackLock', 0.009, 0.006, (0, 0.172, 0.78), M['metal'], rot=(math.pi / 2, 0, 0))
lab = box('Kiosk_Label', (0.10, 0.002, 0.04), (0, 0.168 - 0.045 * math.sin(math.pi * 0.06) - 0.001, 1.0), M['label'])
# #endregion

# #region ── Screen: housing tilted back, black glass front with the touch screen under it ──
SCR_T = math.radians(-11)                  # top leans back (+Y)
sc = Vector((0, -0.11, 1.33))              # screen centre
R = Matrix.Rotation(SCR_T, 4, 'X')
def scr(p):                                 # local screen space (x across, y out of the screen towards -Y, z up)
    return sc + (R @ Vector(p))
hous = box('Kiosk_ScreenHousing', (0.50, 0.07, 0.40), scr((0, 0.025, 0)), M['white'], 0.016)
hous.rotation_euler = (SCR_T, 0, 0)
gl = box('Kiosk_ScreenGlass', (0.47, 0.006, 0.37), scr((0, -0.0135, 0)), M['glass'], 0.004)
gl.rotation_euler = (SCR_T, 0, 0)
# the display: a flat quad just under the glass surface, carrying the UI picture
bm = bmesh.new()
w2, h2 = 0.205, 0.154
vs = [bm.verts.new(scr(p)) for p in ((-w2, -0.0168, -h2 + 0.02), (w2, -0.0168, -h2 + 0.02), (w2, -0.0168, h2 + 0.02), (-w2, -0.0168, h2 + 0.02))]
f = bm.faces.new(vs); uv = bm.loops.layers.uv.new()
for l, c in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))): l[uv].uv = c
bmesh.ops.reverse_faces(bm, faces=[f])
disp = obj_from_bm('Kiosk_Display', bm, M['screen'])
box('Kiosk_ScreenChin', (0.47, 0.004, 0.012), scr((0, -0.0168, -0.17)), M['vent']).rotation_euler = (SCR_T, 0, 0)
# #endregion

# #region ── Handle: dark grey loop round the top of the screen with the green status light ──
cu = bpy.data.curves.new('Kiosk_HandleCurve', 'CURVE'); cu.dimensions = '3D'
cu.bevel_mode = 'ROUND'; cu.bevel_depth = 0.021; cu.bevel_resolution = 6; cu.resolution_u = 16
sp = cu.splines.new('BEZIER')
hw, ztop = 0.238, 0.30
pts = [(-hw, 0.05, -0.02), (-hw, 0.055, 0.17), (-hw + 0.06, 0.07, ztop), (hw - 0.06, 0.07, ztop), (hw, 0.055, 0.17), (hw, 0.05, -0.02)]
sp.bezier_points.add(len(pts) - 1)
for bp, p in zip(sp.bezier_points, pts):
    bp.co = scr(p); bp.handle_left_type = bp.handle_right_type = 'AUTO'
ho = bpy.data.objects.new('Kiosk_Handle', cu); scn.collection.objects.link(ho); ho.data.materials.append(M['grey'])
cu.extrude = 0.0
# a second, flatter bar behind joins the loop to the spine's top
box('Kiosk_HandleBridge', (0.40, 0.11, 0.04), scr((0, 0.105, ztop - 0.03)), M['grey'], 0.014).rotation_euler = (SCR_T, 0, 0)
# green light strip along the top of the loop
cu2 = bpy.data.curves.new('Kiosk_LightCurve', 'CURVE'); cu2.dimensions = '3D'; cu2.bevel_depth = 0.006; cu2.bevel_resolution = 3
s2 = cu2.splines.new('POLY'); s2.points.add(1)
s2.points[0].co = (*scr((-hw + 0.08, 0.07, ztop + 0.019)), 1); s2.points[1].co = (*scr((hw - 0.08, 0.07, ztop + 0.019)), 1)
lo = bpy.data.objects.new('Kiosk_StatusLight', cu2); scn.collection.objects.link(lo); lo.data.materials.append(M['green'])
# #endregion

# #region ── Finish: curves to meshes, one object, export ──
bpy.ops.object.select_all(action='DESELECT')
for o in list(scn.collection.objects):
    if o.type == 'CURVE':
        bpy.context.view_layer.objects.active = o; o.select_set(True)
        bpy.ops.object.convert(target='MESH'); o.select_set(False)
for o in scn.collection.objects:
    wn = o.modifiers.new('WN', 'WEIGHTED_NORMAL'); wn.keep_sharp = True
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, 'SM_CheckinKiosk.blend'))
for o in scn.collection.objects: o.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.convert(target='MESH')
bpy.ops.object.join()
k = bpy.context.view_layer.objects.active; k.name = 'SM_CheckinKiosk'
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, 'SM_CheckinKiosk.glb'), export_format='GLB', use_selection=True,
                          export_apply=True, export_yup=True)
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, 'SM_CheckinKiosk.fbx'), use_selection=True, apply_unit_scale=True,
                         axis_forward='-Y', axis_up='Z', path_mode='COPY', embed_textures=True)
dims = k.dimensions; print('DIMS', round(dims.x, 3), round(dims.y, 3), round(dims.z, 3), 'TRIS', sum(len(p.vertices) - 2 for p in k.data.polygons))
# #endregion
