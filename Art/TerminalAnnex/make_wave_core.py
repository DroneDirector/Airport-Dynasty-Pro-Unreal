# The wave roof as a solid body (Tools/wave_roof.py's shape, underside to top, 0.7 m thick), in pieces of one wave (30 m):
# never drawn; Lumen sees it in place of the roof's two thin sheets (SoffitLights.cpp), which it lost at a distance - the panes
# under the roof mirrored the wood only in patches, blue sky between (owner, 2026-11-23).
#   F:/Tools/Blender-4.2/blender.exe -b --python Art/TerminalAnnex/make_wave_core.py ; then Tools/import_annex.py wave_core_m0 ...
# Blender axes as make_wave_roof.py: (-plan x, -plan z, plan y), each bow's middle at x 0.
import bpy, os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(HERE)), 'Tools'))
import wave_roof as W
bpy.ops.wm.read_factory_settings(use_empty=True)
MAT = [bpy.data.materials.new(n) for n in ('MI_T_timber', 'MI_T_roofZinc')]
for name in 'mwe':
    for k in range(3):
        u0, u1 = -45.0 + 30 * k, -15.0 + 30 * k
        if name == 'w' and k == 0: u0 = -51.0
        if name == 'e' and k == 2: u1 = 51.0
        def end(u): return (name == 'w' and u < 0) or (name == 'e' and u > 0)
        n, m = int(round(u1 - u0)), 50; v = []; f = []; mi = []
        for t in (0.0, W.THICK):
            for i in range(n + 1):
                u = u0 + (u1 - u0) * i / n; z0 = W.bow(u, end(u))
                for j in range(m + 1):
                    z = z0 + (W.Z1 - z0) * j / m; v.append((-u, -z, W.h_u(u, z, end(u)) + t))
        N = (n + 1) * (m + 1); q = lambda i, j, t: t * N + i * (m + 1) + j
        for i in range(n):
            for j in range(m):
                f.append((q(i, j, 0), q(i + 1, j, 0), q(i + 1, j + 1, 0), q(i, j + 1, 0))); mi.append(0)
                f.append((q(i, j, 1), q(i, j + 1, 1), q(i + 1, j + 1, 1), q(i + 1, j, 1))); mi.append(1)
        for i in range(n):
            f.append((q(i, 0, 0), q(i, 0, 1), q(i + 1, 0, 1), q(i + 1, 0, 0))); f.append((q(i, m, 0), q(i + 1, m, 0), q(i + 1, m, 1), q(i, m, 1))); mi += [1, 1]
        for j in range(m):
            f.append((q(0, j, 0), q(0, j + 1, 0), q(0, j + 1, 1), q(0, j, 1))); f.append((q(n, j, 0), q(n, j, 1), q(n, j + 1, 1), q(n, j + 1, 0))); mi += [1, 1]
        me = bpy.data.meshes.new('wave_core_%s%d' % (name, k)); me.from_pydata(v, [], f)
        for x in MAT: me.materials.append(x)
        me.polygons.foreach_set('material_index', mi)
        uv = me.uv_layers.new(name='UVMap')
        for l in me.loops: c = me.vertices[l.vertex_index].co; uv.data[l.index].uv = (-c.x * 0.126, -c.y / 8.0)
        o = bpy.data.objects.new(me.name, me); bpy.context.scene.collection.objects.link(o)
        bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
        bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.export_scene.fbx(filepath=os.path.join(HERE, o.name + '.fbx'), use_selection=True, object_types={'MESH'}, apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='OFF', path_mode='STRIP')
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, 'WaveCore.blend'))
print('CORE done', len(bpy.data.meshes))
