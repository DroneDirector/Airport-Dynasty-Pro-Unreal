"""Step 1 of the cab window outlines (Blender 3.3, headless):
   blender -b <precab.blend: the Meshy cab as imported> --python trace_cab_windows_probe.py
classifies the Meshy texture colour under a ray grid on both doors and the windscreen -> grid.json.
Step 2: python trace_cab_windows.py grid.json cab_windows.json"""
import bpy, bmesh, numpy as np, sys
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
sys.path.insert(0,'C:/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal/Art/Lorry'); sys.path.insert(0,'C:/Users/Simon/Documents/Airport-Dynasty-Pro-Unreal/Art/CargoForklift')
import lorry_cab as LC
sc=bpy.context.scene
names=[o.name for o in sc.objects if o.type=='MESH']
print('OBJS',len(names),[n for n in names if 'Door' in n or 'Glass' in n or 'Tractor' in n or 'Cab' in n])
objs=[sc.objects[n] for n in ('Tractor','Door_L','Door_R','Door_L_Glass','Door_R_Glass','Glass_Windscreen') if n in sc.objects]
data=[]
imcache={}
def img(o,mi):
    m=o.data.materials[mi] if o.data.materials else None
    if m is None or not m.use_nodes: return None,m
    ims=[n.image for n in m.node_tree.nodes if n.type=='TEX_IMAGE' and n.image]
    b=next((i for i in ims if 'base' in i.filepath.lower() or 'color' in i.filepath.lower()), ims[0] if ims else None)
    if b is None: return None,m
    if b.name not in imcache:
        w,h=b.size; imcache[b.name]=np.array(b.pixels[:],dtype=np.float32).reshape(h,w,4)[:,:,:3]
    return imcache[b.name],m
for o in objs:
    bm=bmesh.new(); bm.from_mesh(o.data); bmesh.ops.transform(bm,matrix=o.matrix_world,verts=bm.verts); bm.faces.ensure_lookup_table()
    data.append((o,bm,BVHTree.FromBMesh(bm),bm.loops.layers.uv.active))
    print('O',o.name,len(bm.faces),[m.name if m else None for m in o.data.materials])
def cls(F,a,b):
    nrm=(F.to_3x3()@Vector((0,0,1))).normalized(); best=None
    for o,bm,T,uv in data:
        h=T.ray_cast(F@Vector((a,b,0.5)),-nrm,1.2)
        if h[0] is not None and (best is None or h[3]<best[0]): best=(h[3],o,bm,h[2],uv)
    if best is None: return ' '
    d,o,bm,fi,uv=best; f=bm.faces[fi]
    if 'Glass' in o.name: return 'g'
    A,m=img(o,f.material_index)
    if A is None or uv is None: return 'x'
    U=sum((l[uv].uv for l in f.loops),Vector((0,0)))/len(f.loops); hh,ww=A.shape[:2]
    c=A[int(U.y%1*hh)%hh,int(U.x%1*ww)%ww]; L=c.mean()
    ch='B' if c[2]-c[0]>0.12 and c[2]>0.2 else ('#' if L<0.12 else ('-' if L<0.35 else 'o'))
    if 'Door' in o.name and ch=='B': ch='b'
    return ch
import json
def grid(F,A,B):
    return [[cls(F,a,b) for a in A] for b in B]
G={}
xs=np.arange(1.70,3.70,0.01); zs=np.arange(1.90,2.95,0.01)
G['L']=grid(LC.FS(1),xs,zs); G['R']=grid(LC.FS(-1),xs,zs); G['side_ax']=[xs.tolist(),zs.tolist()]
As=np.arange(-1.40,1.401,0.01); Bs=np.arange(-0.20,0.95,0.01)
G['WS']=grid(LC.F_WS,As,Bs); G['ws_ax']=[As.tolist(),Bs.tolist()]
json.dump(G,open((sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'grid.json'), 'w')); print('GRID OK')
