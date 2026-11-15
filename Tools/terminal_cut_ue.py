import os
# Cuts boxes out of terminal pieces inside Unreal (Tools/terminal_cut.py writes the job): each job piece is copied to
# /Game/Terminal/Meshes/<piece>_cut<n>, and every face whose middle lies in one of the job's boxes (the plan's metres)
# is taken out of the copy; materials, Nanite and collision as the original.
import json, os, sys
import unreal
JOB = sys.argv[-1]
job = json.load(open(JOB))
EAL = unreal.EditorAssetLibrary
# (Git LFS leaves committed .uasset files read-only: Unreal then can't replace them and the import fails silently -
#  the game kept an old copy of the new parts; the flag is cleared first)
import glob, stat
for _f in glob.glob(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'Content', 'Terminal/Meshes', '*.uasset')):
    os.chmod(_f, stat.S_IWRITE | stat.S_IREAD)

for j in job:
    src = '/Game/Terminal/Meshes/' + j['piece']; dst = '/Game/Terminal/Meshes/' + j['new']
    if EAL.does_asset_exist(dst): EAL.delete_asset(dst)
    EAL.duplicate_asset(src, dst)
    sm = unreal.load_asset(dst)
    M = j['m']; boxes = j['boxes']
    desc = sm.get_static_mesh_description(0)
    gone = 0
    polys = [unreal.PolygonID(p) for p in range(desc.get_polygon_count() + 64)]
    kill = []
    for p in polys:
        if not desc.is_polygon_valid(p): continue
        vs = desc.get_polygon_vertices(p)
        if not vs: continue
        cx = cy = cz = 0.0
        for v in vs:
            q = desc.get_vertex_position(v); cx += q.x; cy += q.y; cz += q.z
        n = len(vs); cx /= n; cy /= n; cz /= n
        px = M[0][0] * cx + M[0][1] * cy + M[0][2] * cz + M[0][3]
        py = M[1][0] * cx + M[1][1] * cy + M[1][2] * cz + M[1][3]
        pz = M[2][0] * cx + M[2][1] * cy + M[2][2] * cz + M[2][3]
        for b in boxes:
            if b[0] <= px <= b[3] and b[1] <= py <= b[4] and b[2] <= pz <= b[5]: kill.append(p); break
    for p in kill: desc.delete_polygon(p)
    sm.build_from_static_mesh_descriptions([desc])
    EAL.save_loaded_asset(sm)
    unreal.log('ADP_CUT %s -> %s: %d faces out' % (j['piece'], j['new'], len(kill)))
unreal.log('ADP_CUT done')
