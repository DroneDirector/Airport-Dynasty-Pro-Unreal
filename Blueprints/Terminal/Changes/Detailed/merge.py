# merge the 30 000 real pieces into one mesh per class with numpy (bpy.ops.object.join took over 9 minutes)
import numpy as np, time
t0 = time.time(); bpy.context.view_layer.update(); CL = {}
for o in list(EX.objects) + [q for q in NEWC.objects if q.get('cutcopy')]:
    if o.type != 'MESH' or not o.data.polygons: continue
    a, b = bb(o); g = str(o.get('grp') or '')
    k = 'before' if o.get('before_only') else 'after' if o.get('cutcopy') else 'real'
    top = 'roof' if (g.startswith('ROOF') and (b[0] + 200 > 112 or g == 'ROOF' and o.get('cutcopy'))) or ((o.name.startswith('arm#') or g == 'PILLAR') and b[0] + 200 > 120 and b[2] > 9) or (b[0] + 200 > 160 and a[2] > 10.6 and g in ('F1', 'F2')) else 'body'
    CL.setdefault(k + '_' + top, []).append(o)
CACHE = {}
def arr(m):
    if m.name in CACHE: return CACHE[m.name]
    nv, nl, npo = len(m.vertices), len(m.loops), len(m.polygons)
    co = np.empty(nv * 3, np.float32); m.vertices.foreach_get('co', co); li = np.empty(nl, np.int32); m.loops.foreach_get('vertex_index', li)
    ls = np.empty(npo, np.int32); m.polygons.foreach_get('loop_start', ls); lt = np.empty(npo, np.int32); m.polygons.foreach_get('loop_total', lt)
    mi = np.empty(npo, np.int32); m.polygons.foreach_get('material_index', mi); sm = np.empty(npo, bool); m.polygons.foreach_get('use_smooth', sm)
    uv = np.zeros(nl * 2, np.float32)
    if m.uv_layers: m.uv_layers[0].data.foreach_get('uv', uv)
    CACHE[m.name] = (co.reshape(-1, 3), li, ls, lt, mi, sm, uv, m.use_auto_smooth); return CACHE[m.name]
for k, os_ in CL.items():
    mats, idx = [], {}; parts = [[] for _ in range(7)]; ov = ol = 0
    for o in os_:
        co, li, ls, lt, mi, sm, uv, _ = arr(o.data); mw = np.array(o.matrix_world, np.float32); slots = []
        for q in (o.material_slots or []):
            mm = q.material; key = mm.name if mm else ''
            if key not in idx: idx[key] = len(mats); mats.append(mm)
            slots.append(idx[key])
        if not slots:
            if '' not in idx: idx[''] = len(mats); mats.append(None)
            slots = [idx['']]
        sl = np.array(slots, np.int32); w = co @ mw[:3, :3].T + mw[:3, 3]
        for p, v in zip(parts, (w, li + ov, ls + ol, lt, sl[np.minimum(mi, len(sl) - 1)], sm, uv)): p.append(v)
        ov += len(co); ol += len(li)
    W, li, ls, lt, mi, sm, uv = [np.concatenate(p) for p in parts]; uv = uv.reshape(-1, 2)
    zmin = np.minimum.reduceat(W[li, 2], ls); xmax = np.maximum.reduceat(W[li, 0], ls); ymin = np.minimum.reduceat(W[li, 1], ls); ymax = np.maximum.reduceat(W[li, 1], ls)
    lid = (zmin > 10.7) & (xmax > -35) & (ymin > 36) & (ymax < 84) if k.endswith('body') else np.zeros(len(ls), bool)   # ceilings, vault and gate roofs east
    for kk, sel in ((k, ~lid), (k.replace('body', 'lid'), lid)):
        if not sel.any(): continue
        li2 = li[np.repeat(sel, lt)]; uv2 = uv[np.repeat(sel, lt)]; lt2 = lt[sel]; ls2 = np.concatenate(([0], np.cumsum(lt2)[:-1])).astype(np.int32); used, li2 = np.unique(li2, return_inverse=True); W2_ = W[used]
        me = bpy.data.meshes.new('J_' + kk)
        me.vertices.add(len(W2_)); me.vertices.foreach_set('co', W2_.astype(np.float32).ravel()); me.loops.add(len(li2)); me.loops.foreach_set('vertex_index', li2.astype(np.int32)); me.polygons.add(len(ls2)); me.polygons.foreach_set('loop_start', ls2); me.polygons.foreach_set('loop_total', lt2)
        for q_ in mats: me.materials.append(q_)
        me.polygons.foreach_set('material_index', mi[sel]); me.polygons.foreach_set('use_smooth', sm[sel]); me.uv_layers.new(name='UVMap').data.foreach_set('uv', uv2.ravel()); me.update(calc_edges=True); me.validate()
        me.use_auto_smooth = True; me.auto_smooth_angle = math.radians(40)
        q = bpy.data.objects.new('J_' + kk, me); coll('J_' + kk).objects.link(q); print('MERGED', kk, len(os_), 'pieces', len(ls2), 'faces', round(time.time() - t0), 's', flush=True)
for L in CL.values():                                          # (unlinked, so not saved: deleting 25 000 objects took minutes)
    for o in L:
        for c_ in list(o.users_collection): c_.objects.unlink(o)
