# Where on a MetaHuman's skin pictures each place of its body lies (owner, 2026-11-15: tattoos): every MetaHuman body
# (and head) shares one UV layout, so a tattoo laid into that layout fits all of them. From the player's exported body
# and head (F:/ADP-Export/tattoo/*.fbx, Unreal's FBX export), this writes Art/Tattoos/skin_map_<part>.npz: for every
# texel of a RES x RES map the 3D point of the skin it shows (cm, the mesh's own space; NaN where no skin) and its
# surface normal, plus the skeleton's joints - the tattoo tool places designs with it.
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/Tattoos/map_skin.py
import bpy, os, sys
import numpy as np

RES = 2048
SRC = 'F:/ADP-Export/tattoo'
HERE = os.path.dirname(os.path.abspath(__file__))

for part in ('body', 'face'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=SRC + '/' + part + '.fbx', global_scale=1.0)
    arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    # LOD0 only (the export may hold several)
    meshes.sort(key=lambda o: -len(o.data.vertices))
    mo = meshes[0]
    dg = bpy.context.evaluated_depsgraph_get()
    me = mo.evaluated_get(dg).to_mesh()
    me.calc_loop_triangles()
    W = np.array(mo.matrix_world)
    co = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('co', co); co = co.reshape(-1, 3)
    co = (np.c_[co, np.ones(len(co))] @ W.T)[:, :3] * 100.0     # (Blender m -> cm)
    nv = np.empty(len(me.vertices) * 3); me.vertices.foreach_get('normal', nv); nv = nv.reshape(-1, 3) @ W[:3, :3].T
    nv /= np.linalg.norm(nv, axis=1, keepdims=True) + 1e-9
    uvl = me.uv_layers[0].data
    uv = np.empty(len(uvl) * 2); uvl.foreach_get('uv', uv); uv = uv.reshape(-1, 2)
    uv = uv - np.floor(uv.min(0))          # (MetaHuman's body lies in the second UV tile, u 1-2: the material wraps it)
    tris = me.loop_triangles
    lt = np.empty(len(tris) * 3, int); tris.foreach_get('loops', lt); lt = lt.reshape(-1, 3)
    vt = np.empty(len(tris) * 3, int); tris.foreach_get('vertices', vt); vt = vt.reshape(-1, 3)
    # only the skin: the head mesh also holds the eyes, teeth, lashes etc., whose UVs lie over the skin's and would
    # overwrite it (the neck came out empty)
    mi = np.empty(len(tris), int); tris.foreach_get('material_index', mi)
    # (the FBX import does not keep the slot names in order - the skin is the slot holding most of the triangles)
    cnt = np.bincount(mi); skin = [int(np.argmax(cnt))]
    print('SLOTS %s: triangles per slot %s -> skin %s' % (part, cnt.tolist(), skin))
    if skin:
        keep = np.isin(mi, skin); lt = lt[keep]; vt = vt[keep]
    P = np.full((RES, RES, 3), np.nan, np.float32); Nn = np.zeros((RES, RES, 3), np.float32)
    # rasterise every triangle in UV space (texel centres), barycentric 3D position and normal
    for t in range(len(lt)):
        a, b, c = uv[lt[t]] * RES
        a = np.array([a[0], RES - a[1]]); b = np.array([b[0], RES - b[1]]); c = np.array([c[0], RES - c[1]])   # (row 0 at the top)
        x0, x1 = int(max(0, np.floor(min(a[0], b[0], c[0])))), int(min(RES - 1, np.ceil(max(a[0], b[0], c[0]))))
        y0, y1 = int(max(0, np.floor(min(a[1], b[1], c[1])))), int(min(RES - 1, np.ceil(max(a[1], b[1], c[1]))))
        if x1 < x0 or y1 < y0: continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(d) < 1e-12: continue
        l1 = ((b[1] - c[1]) * (gx - c[0]) + (c[0] - b[0]) * (gy - c[1])) / d
        l2 = ((c[1] - a[1]) * (gx - c[0]) + (a[0] - c[0]) * (gy - c[1])) / d
        l3 = 1 - l1 - l2
        m = (l1 >= -0.01) & (l2 >= -0.01) & (l3 >= -0.01)
        if not m.any(): continue
        va, vb, vc = vt[t]
        pos = l1[..., None] * co[va] + l2[..., None] * co[vb] + l3[..., None] * co[vc]
        nor = l1[..., None] * nv[va] + l2[..., None] * nv[vb] + l3[..., None] * nv[vc]
        ys, xs = np.nonzero(m)
        P[y0 + ys, x0 + xs] = pos[ys, xs]; Nn[y0 + ys, x0 + xs] = nor[ys, xs]
    bones = {}
    if arm:
        for bn in arm.data.bones:
            h = np.array(arm.matrix_world @ bn.head_local) * 100.0; tl = np.array(arm.matrix_world @ bn.tail_local) * 100.0
            bones[bn.name] = np.r_[h, tl]
    np.savez_compressed(os.path.join(HERE, 'skin_map_%s.npz' % part), P=P, N=Nn, bone_names=np.array(list(bones.keys())), bone_pts=np.array(list(bones.values())) if bones else np.zeros((0, 6)))
    print('MAP %s: %d verts, %d tris, %d texels with skin, %d bones; bounds %s %s' % (part, len(co), len(lt), int(np.isfinite(P[..., 0]).sum()), len(bones), co.min(0).round(1), co.max(0).round(1)))
print('DONE')
