"""Bakes one light merged skeletal mesh per built person, for use beyond 20 m (to-do 205, step A).

  UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="Tools/make_people_layers.py <names|all> [force]"
      -EnablePlugins=GeometryScripting -unattended -nosplash -RenderOffscreen -dx12

Reads /Game/People/Built/<name> (never changed): the components of its blueprint say what is worn. Writes
/Game/People/Layers/<name>/SKM_<name>_Layer on the skeleton of the person's own body, three levels of detail, the
person's own baked materials. A name may be short: "000" finds MH_000_passenger.
Skipped when the layer is newer than every file of the person (force = bake anyway). A person that fails is an error
line and the run ends with an error; the others are still baked. Summary: Saved/PeopleLayers.json, a line per person.
"""
import glob
import json
import math
import os
import re
import sys
import time
import traceback
import unreal

AU = unreal.GeometryScript_AssetUtils
NA = unreal.GeometryScript_NewAssetUtils
BW = unreal.GeometryScript_BoneWeights
ED = unreal.GeometryScript_MeshEdits
MQ = unreal.GeometryScript_MeshQueries
MM = unreal.GeometryScript_Materials
SP = unreal.GeometryScript_MeshSpatial
SI = unreal.GeometryScript_MeshSimplification
LI = unreal.GeometryScript_List
eal = unreal.EditorAssetLibrary
ses = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)

BUILT, LAYERS = '/Game/People/Built', '/Game/People/Layers'
CONTENT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
DISK = os.path.realpath(os.path.join(CONTENT, 'People'))
JSON = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'PeopleLayers.json')

# triangles wanted per level of detail, by kind of part
WANT = [{'face': 2800, 'body': 2600, 'clothes': 6400, 'hair': 1600, 'small': 250},
        {'face': 750, 'body': 700, 'clothes': 1500, 'hair': 500, 'small': 0},
        {'face': 200, 'body': 200, 'clothes': 400, 'hair': 180, 'small': 0}]
# screen size where each starts (a 1.8 m person, 90 degrees wide): level 0 to about 60 m, 1 to about 120 m, then 2
SCREEN = [1.0, 0.028, 0.014]
COVER = 6.0        # cm: skin with cloth this near in front of it is hidden
# the bones kept on each level (the others hand their weight to the nearest parent kept): the fewer, the cheaper
CORE = {'root', 'pelvis', 'head', 'neck_01', 'neck_02'} | {'spine_0%d' % i for i in range(1, 6)} | {
    '%s_%s' % (b, s) for b in ('clavicle', 'upperarm', 'lowerarm', 'hand', 'thigh', 'calf', 'foot', 'ball') for s in 'lr'}
FINE = {'%s_0%d_%s' % (f, n, s) for f in ('thumb', 'index', 'middle', 'ring', 'pinky') for n in (1, 2) for s in 'lr'} | {
    '%s_twist_0%d_%s' % (b, n, s) for b in ('upperarm', 'lowerarm', 'thigh', 'calf') for n in (1, 2) for s in 'lr'}


class Fail(Exception):
    pass


def log(m):
    unreal.log('ADP_LAYER: ' + m)


def out(r):
    """What a Geometry Script call gives besides the mesh itself."""
    return [x for x in r if not isinstance(x, unreal.DynamicMesh)] if isinstance(r, tuple) else [r]


def ints(il):
    return list(LI.convert_index_list_to_array(il))


def keep_bone(n, level):
    n = n.lower()
    return n in CORE or (level == 0 and n in FINE)


def base_of(m):
    while m and not isinstance(m, unreal.Material):
        m = m.get_editor_property('parent')
    return m


# ---- what the person wears
def worn(name):
    bp = unreal.load_asset('%s/%s/BP_%s' % (BUILT, name, name))
    if not bp:
        raise Fail('no BP_' + name)
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    body = face = None
    clothes, grooms, seen = [], [], set()
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        o = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(sub.k2_find_subobject_data_from_handle(h))
        if not o or o.get_path_name() in seen:
            continue
        seen.add(o.get_path_name())
        n = o.get_name()
        if isinstance(o, unreal.SkeletalMeshComponent):
            sk = o.get_editor_property('skeletal_mesh_asset')
            if not sk:
                continue
            e = (sk, list(o.get_editor_property('override_materials')))
            if n.startswith('Body'):
                body = e
            elif n.startswith('Face'):
                face = e
            else:
                clothes.append(e)
        elif isinstance(o, unreal.GroomComponent):
            g = o.get_editor_property('groom_asset')
            if g:
                grooms.append((n.split('_GEN')[0], g, list(o.get_editor_property('override_materials'))))
    if not body or not face:
        raise Fail('no Body or no Face in the blueprint')
    return body, face, clothes, grooms


# ---- reading
def read_skel(sk, level):
    for t in (unreal.GeometryScriptLODType.SOURCE_MODEL, unreal.GeometryScriptLODType.RENDER_DATA):
        r = unreal.GeometryScriptMeshReadLOD()
        r.set_editor_property('lod_type', t)
        r.set_editor_property('lod_index', level)
        dm = unreal.DynamicMesh()
        AU.copy_mesh_from_skeletal_mesh(sk, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), r)
        if dm.get_triangle_count() > 0:
            return dm
    raise Fail('%s level %d cannot be read' % (sk.get_name(), level))


def read_static(sm):
    r = unreal.GeometryScriptMeshReadLOD()
    r.set_editor_property('lod_type', unreal.GeometryScriptLODType.RENDER_DATA)
    dm = unreal.DynamicMesh()
    AU.copy_mesh_from_static_mesh(sm, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), r)
    if dm.get_triangle_count() < 1:
        raise Fail('%s cannot be read' % sm.get_name())
    return dm


def pick_lod(sk, want, least=0):
    """The lightest level that still has the triangles wanted (never a finer one than 'least')."""
    for l in range(ses.get_lod_count(sk) - 1, least - 1, -1):
        dm = read_skel(sk, l)
        if dm.get_triangle_count() >= want:
            break
    return l, dm


def mat_ids(dm):
    return ints(out(MM.get_all_triangle_material_i_ds(dm))[0])


def tris_of(dm):
    return [(t.x, t.y, t.z) for t in LI.convert_triangle_list_to_array(out(MQ.get_all_triangle_indices(dm, False))[0])]


def points_of(dm):
    return list(LI.convert_vector_list_to_array(out(MQ.get_all_vertex_positions(dm, False))[0]))


def vert_ids(dm):
    return ints(out(MQ.get_all_vertex_i_ds(dm))[0])


def delete_tris(dm, ids):
    if ids:
        ED.delete_triangles_from_mesh(dm, LI.convert_array_to_index_list(ids, unreal.GeometryScriptIndexType.TRIANGLE))
        unreal.GeometryScript_MeshRepair.compact_mesh(dm)


# ---- bones
def bones_of(dm):
    return [(str(b.get_editor_property('name')), b.get_editor_property('parent_index')) for b in out(BW.get_all_bones_info(dm))[0]]


def put_weights(dm, new):
    for v, w in new.items():
        top = sorted(w.items(), key=lambda kv: -kv[1])[:4]
        s = sum(x for _, x in top) or 1.0
        l = []
        for k, x in top:
            b = unreal.GeometryScriptBoneWeight()
            b.set_editor_property('bone_index', k)
            b.set_editor_property('weight', x / s)
            l.append(b)
        BW.set_vertex_bone_weights(dm, v, l)


def rebone(dm, body, index, level):
    """The weights moved to the body's bones, only those kept on this level."""
    own = bones_of(dm)
    to = {}
    for i in range(len(own)):
        j = i
        while j >= 0 and not (own[j][0] in index and keep_bone(own[j][0], level)):
            j = own[j][1]
        if j < 0:
            raise Fail('bone %s has no parent on the body' % own[i][0])
        to[i] = index[own[j][0]]
    new = {}
    for v in vert_ids(dm):
        w = {}
        for x in out(BW.get_vertex_bone_weights(dm, v))[0]:
            k = to.get(x.get_editor_property('bone_index'))
            if k is not None:
                w[k] = w.get(k, 0.0) + x.get_editor_property('weight')
        if not w:
            raise Fail('a vertex without bone weights')
        new[v] = w
    BW.copy_bones_from_mesh(body, dm, unreal.GeometryScriptCopyBonesFromMeshOptions())
    put_weights(dm, new)


def rigid(dm, body, bone):
    BW.copy_bones_from_mesh(body, dm, unreal.GeometryScriptCopyBonesFromMeshOptions())
    BW.mesh_create_bone_weights(dm, False)
    put_weights(dm, {v: {bone: 1.0} for v in vert_ids(dm)})


# ---- hidden skin
def drop_hidden(dm, clothes):
    """Skin with cloth close in front of it at all its corners, and at all corners of its neighbours, is dropped."""
    if not clothes:
        return 0
    allc = unreal.DynamicMesh()
    for c in clothes:
        ED.append_mesh(allc, c, unreal.Transform())
    bvh = out(SP.build_bvh_for_mesh(allc))[0]
    pts, tris = points_of(dm), tris_of(dm)
    P = [(p.x, p.y, p.z) for p in pts]
    key = [tuple(round(c, 2) for c in p) for p in P]   # (by place: the skin's seams split its corners)
    nrm, vol = {}, 0.0
    for t in tris:
        a, b, c = (P[v] for v in t)
        u = [b[i] - a[i] for i in range(3)]
        w = [c[i] - a[i] for i in range(3)]
        n = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
        vol += n[0] * (a[0]) + n[1] * (a[1]) + n[2] * (a[2] - 90.0)
        for v in t:
            m = nrm.setdefault(key[v], [0.0, 0.0, 0.0])
            for i in range(3):
                m[i] += n[i]
    side = 1.0 if vol > 0 else -1.0   # (which way the triangles turn)
    o = unreal.GeometryScriptSpatialQueryOptions()
    o.set_editor_property('max_distance', COVER)
    covered = {}
    for p, k in zip(P, key):
        if k in covered or k not in nrm:
            continue
        n = [x * side for x in nrm[k]]
        ln = sum(x * x for x in n) ** 0.5
        if ln < 1e-6:
            covered[k] = False
            continue
        n = unreal.Vector(n[0] / ln, n[1] / ln, n[2] / ln)
        r = out(SP.find_nearest_ray_intersection_with_mesh(allc, bvh, unreal.Vector(p[0] - n.x * .6, p[1] - n.y * .6, p[2] - n.z * .6), n, o))[0]
        covered[k] = bool(r.get_editor_property('hit')) and r.get_editor_property('ray_parameter') <= COVER + .6
    bad = set()
    for t in tris:
        ks = [key[v] for v in t]
        if not all(covered.get(k, False) for k in ks):
            bad.update(ks)
    gone = [i for i, t in enumerate(tris) if not any(key[v] in bad for v in t)]
    delete_tris(dm, gone)
    return len(gone)


def simplify(dm, want):
    if want and dm.get_triangle_count() > want * 1.05:
        o = unreal.GeometryScriptSimplifyMeshOptions()
        SI.apply_simplify_to_triangle_count(dm, int(want), o)
    return dm


# ---- hair, beard: the groom's own solid meshes (fitted to this person's head when it was built)
def helmets(name, g):
    """The groom's solid meshes, finest first (3 levels: the last is used again if there are fewer)."""
    found = {}
    for path in eal.list_assets('%s/%s/Grooms' % (BUILT, name), recursive=False, include_folder=False):
        m = re.match(re.escape(g.get_name()) + r'_(?:Mesh|Helmet)(?:_Group\d+)?_LOD(\d+)$', path.split('/')[-1].split('.')[0])
        if m:
            found.setdefault(int(m.group(1)), []).append(unreal.load_asset(path))
    l = [found[k] for k in sorted(found)]
    return [l[min(i, len(l) - 1)] for i in range(3)] if l else []


def hair_colour(mi):
    """The colour the hair material mixes from melanin, redness, dye and grey (Unreal's own hair formula)."""
    names = {str(n).lower(): n for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mi)}
    vn = {str(n).lower(): n for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(mi)}

    def num(keys, default):
        for k in keys:
            if k in names:
                return unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi, names[k])
        return default
    mel = num(('hairmelanin', 'melanin'), None)
    if mel is None:
        raise Fail('%s has no melanin, only %s' % (mi.get_name(), sorted(names)))
    red = num(('hairredness', 'redness'), 0.0)
    grey = min(1.0, max(0.0, num(('whiteamount', 'grayness', 'greyness', 'greyamount'), 0.0)))
    q = -math.log(max(1.0 - min(1.0, mel), 0.0001))
    eu, ph = q * (1.0 - red), q * red
    ab = [eu * a + ph * b for a, b in zip((0.506, 0.841, 1.653), (0.343, 0.733, 1.924))]
    c = [math.exp(-math.sqrt(max(0.0, a)) * 3.0) for a in ab]          # (one hair is 5.2; a whole head of hair is lighter, matched against the strands)
    dye = next((vn[k] for k in ('hairdye', 'dyecolor', 'dye') if k in vn), None)
    if dye:
        d = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(mi, dye)
        c = [x * y for x, y in zip(c, (d.r, d.g, d.b))]
    c = [x + (0.45 - x) * grey * grey for x in c]
    c = [max(0.004, x) for x in c]      # (never pure black)
    return unreal.LinearColor(c[0], c[1], c[2], 1.0), {'melanin': round(mel, 3), 'redness': round(red, 3), 'grey': round(grey, 3)}


def hair_master():
    """Our own light hair material: Epic's is made for hair strands only, it may not be put on a skinned mesh."""
    path = LAYERS + '/Common/M_LayerHair'
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    L = unreal.MaterialEditingLibrary
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_LayerHair', LAYERS + '/Common', unreal.Material, unreal.MaterialFactoryNew())
    if not m:
        raise Fail('M_LayerHair cannot be made')
    c = L.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -400, 0)
    c.set_editor_property('parameter_name', 'HairColour')
    c.set_editor_property('default_value', unreal.LinearColor(0.04, 0.025, 0.015, 1.0))
    L.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    for v, prop, y in ((0.5, unreal.MaterialProperty.MP_ROUGHNESS, 200), (0.3, unreal.MaterialProperty.MP_SPECULAR, 300)):
        k = L.create_material_expression(m, unreal.MaterialExpressionConstant, -400, y)
        k.set_editor_property('r', v)
        L.connect_material_property(k, '', prop)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('used_with_skeletal_mesh', True)
    L.recompile_material(m)
    if not eal.save_asset(m.get_path_name(), False):
        raise Fail('M_LayerHair cannot be saved')
    return m


def hair_material(name, part, mi):
    col, why = hair_colour(mi)
    folder, n = '%s/%s' % (LAYERS, name), 'MI_%s_Layer%s' % (name, part)
    a = unreal.load_asset('%s/%s' % (folder, n)) if eal.does_asset_exist('%s/%s' % (folder, n)) else None
    if not a:
        a = unreal.AssetToolsHelpers.get_asset_tools().create_asset(n, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(a, hair_master())
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(a, 'HairColour', col)
    unreal.MaterialEditingLibrary.update_material_instance(a)
    if not eal.save_asset(a.get_path_name(), False):
        raise Fail(n + ' cannot be saved')
    return a, why


def lod_settings():
    """The screen sizes of the levels, one asset for everybody (a mesh's own list cannot be written from here)."""
    path = LAYERS + '/Common/LS_Layer'
    d = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if not d:
        f = unreal.DataAssetFactory()
        f.set_editor_property('data_asset_class', unreal.SkeletalMeshLODSettings)
        d = unreal.AssetToolsHelpers.get_asset_tools().create_asset('LS_Layer', LAYERS + '/Common', unreal.SkeletalMeshLODSettings, f)
        if not d:
            raise Fail('LS_Layer cannot be made')
    groups = []
    for size in SCREEN:
        g = unreal.SkeletalMeshLODGroupSettings()
        p = g.get_editor_property('screen_size')
        p.set_editor_property('default', size)
        g.set_editor_property('screen_size', p)
        r = g.get_editor_property('reduction_settings')
        r.set_editor_property('num_of_triangles_percentage', 1.0)  # (never make our levels smaller itself)
        r.set_editor_property('num_of_vert_percentage', 1.0)
        g.set_editor_property('reduction_settings', r)
        groups.append(g)
    d.set_editor_property('lod_groups', groups)
    if [round(g.get_editor_property('screen_size').get_editor_property('default'), 4) for g in d.get_editor_property('lod_groups')] != SCREEN:
        raise Fail('LS_Layer did not take the screen sizes')
    if not eal.save_asset(path, False):
        raise Fail('LS_Layer cannot be saved')
    return d


# ---- one person
class Palette:
    """The sections: one per really different material."""

    def __init__(self):
        self.mats, self.sigs, self.kinds = [], [], []

    @staticmethod
    def sig(m):
        L = unreal.MaterialEditingLibrary
        if not isinstance(m, unreal.MaterialInstanceConstant):
            return m.get_path_name()
        s = [base_of(m).get_path_name()]
        for n in L.get_scalar_parameter_names(m):
            s.append((str(n), round(L.get_material_instance_scalar_parameter_value(m, n), 4)))
        for n in L.get_vector_parameter_names(m):
            v = L.get_material_instance_vector_parameter_value(m, n)
            s.append((str(n), round(v.r, 4), round(v.g, 4), round(v.b, 4)))
        for n in L.get_texture_parameter_names(m):
            t = L.get_material_instance_texture_parameter_value(m, n)
            s.append((str(n), t.get_path_name() if t else ''))
        return repr(s)

    def add(self, m, kind):
        g = self.sig(m)
        if g in self.sigs:
            return self.sigs.index(g)
        self.mats.append(m)
        self.sigs.append(g)
        self.kinds.append(kind)
        return len(self.mats) - 1


def part_materials(sk, over):
    """The material of every slot; the blueprint's overrides go by the slot's name (each level has slots of its own)."""
    slots = [(str(s.get_editor_property('material_slot_name')), s.get_editor_property('material_interface')) for s in sk.get_editor_property('materials')]
    by = {}
    for (n, _), o in zip(slots, over):
        if o:
            by.setdefault(n, o)
    return [by.get(n, m) for n, m in slots]


def set_ids(dm, to):
    """Material ids moved over (from -> to), all at once."""
    ids = mat_ids(dm)
    for a in set(ids):
        MM.remap_material_i_ds(dm, a, 1000 + to[a])
    for b in set(to.values()):
        MM.remap_material_i_ds(dm, 1000 + b, b)


def bake(name):
    t0 = time.time()
    (bsk, bover), (fsk, fover), clothes, grooms = worn(name)
    notes, pal = [], Palette()
    ref = read_skel(bsk, 0)                                        # the body's bones, with this person's own proportions
    index = {n: i for i, (n, _) in enumerate(bones_of(ref))}
    if 'head' not in index:
        raise Fail('the body has no head bone')
    info = {str(b.get_editor_property('name')): b for b in out(BW.get_all_bones_info(ref))[0]}
    head_z = info['head'].get_editor_property('world_transform').translation.z
    back = next((b for b in ('spine_05', 'spine_04', 'spine_03') if b in index), 'head')

    def adopt(sk, over, lod_dm, level):
        mats = part_materials(sk, over)
        lod = unreal.GeometryScriptMeshReadLOD()
        lod.set_editor_property('lod_type', unreal.GeometryScriptLODType.RENDER_DATA)
        lod.set_editor_property('lod_index', level)
        r = AU.get_lod_material_list_from_skeletal_mesh(sk, lod)   # (a level's material ids are its sections: this says which slot)
        slot = list(r[1])
        to = {}
        for i in set(mat_ids(lod_dm)):
            i0 = i
            if i < 0 or i >= len(slot) or not 0 <= slot[i] < len(mats) or not mats[slot[i]]:
                raise Fail('%s level %d: section %d has no material' % (sk.get_name(), level, i))
            i = slot[i]
            to[i0] = (mats[i], str(sk.get_editor_property('materials')[i].get_editor_property('material_slot_name')).lower())
        return to

    # the face: teeth away, both eyes one material
    fl, face = pick_lod(fsk, WANT[0]['face'])
    fto = adopt(fsk, fover, face, fl)
    ids = mat_ids(face)
    delete_tris(face, [i for i, m in enumerate(ids) if any(w in fto[m][1] for w in ('teeth', 'saliva', 'lash', 'eyeshell', 'cartilage', 'eyeedge'))])
    skin = next((m for m, (x, n) in fto.items() if n.startswith('head')), None)
    if skin is None:
        raise Fail('the face has no head skin')
    skin_id = pal.add(fto[skin][0], 'face')
    eye = next((m for m, (x, n) in fto.items() if 'eyeleft' in n), None)
    eye_id = pal.add(fto[eye][0], 'eyes') if eye is not None else skin_id
    set_ids(face, {m: (eye_id if 'eye' in fto[m][1] else skin_id) for m in set(mat_ids(face))})
    # clothes
    worn_parts, full = [], [read_skel(sk, 0).get_triangle_count() for sk, _ in clothes]
    for (sk, over), n in zip(clothes, full):
        l, dm = pick_lod(sk, WANT[0]['clothes'] * n / float(sum(full)))
        cto = adopt(sk, over, dm, l)
        set_ids(dm, {m: pal.add(x[0], 'clothes') for m, x in cto.items()})
        worn_parts.append(dm)
    # the body: only the skin that is seen
    bl, body = pick_lod(bsk, WANT[0]['body'] * 1.5)
    bto = adopt(bsk, bover, body, bl)
    gone = drop_hidden(body, worn_parts)
    set_ids(body, {m: pal.add(x[0], 'body') for m, x in bto.items() if m in set(mat_ids(body))})
    # hair and beard
    hair = []
    for part, g, over in grooms:
        part = re.sub(r'\W', '', part)
        if part in ('Fuzz', 'Eyelashes'):
            continue
        hs = helmets(name, g)
        if not hs:
            notes.append('%s (%s) has no solid mesh: left out' % (part, g.get_name()))
            continue
        mi = next((m for m in over if m and m.get_name().endswith('_Helmet')), next((m for m in over if m), None))
        if not mi:
            raise Fail(part + ' has no material')
        m, why = hair_material(name, part, mi)
        notes.append('%s %s' % (part, why))
        hair.append((hs, pal.add(m, 'hair')))
    if len(pal.mats) > 8:
        raise Fail('%d sections' % len(pal.mats))

    lods, tris, sections = [], [], []
    total = float(sum(d.get_triangle_count() for d in worn_parts)) or 1.0
    share = [d.get_triangle_count() / total for d in worn_parts]
    for level in range(3):
        w = WANT[level]
        if level == 1 and eye_id != skin_id:
            MM.remap_material_i_ds(face, eye_id, skin_id)          # (from 60 m an eye is a pixel: one section less)
        parts = [(face, w['face']), (body, w['body'])] + [(d, w['clothes'] * s) for d, s in zip(worn_parts, share)]
        mesh = None
        for dm, want in parts:
            if dm.get_triangle_count() == 0:
                continue
            simplify(dm, want)
            rebone(dm, ref, index, level)
            if mesh is None:
                mesh = unreal.DynamicMesh()
                BW.copy_bones_from_mesh(ref, mesh, unreal.GeometryScriptCopyBonesFromMeshOptions())
                BW.mesh_create_bone_weights(mesh, False)
                MM.enable_material_i_ds(mesh)
            ED.append_mesh(mesh, dm, unreal.Transform())
        for hs, hid in hair:
            for sm in hs[level]:
                dm = simplify(read_static(sm), w['hair'])
                MM.enable_material_i_ds(dm)
                MM.clear_material_i_ds(dm, hid)
                BW.copy_bones_from_mesh(ref, dm, unreal.GeometryScriptCopyBonesFromMeshOptions())
                BW.mesh_create_bone_weights(dm, False)
                new = {}
                for v in vert_ids(dm):
                    z = out(MQ.get_vertex_position(dm, v))[0].z
                    k = min(1.0, max(0.0, (head_z - 8.0 - z) / 25.0))   # (long hair lies on the shoulders, not on the head)
                    new[v] = {index['head']: 1.0 - k, index[back]: k} if 0.0 < k < 1.0 else {index[back] if k >= 1.0 else index['head']: 1.0}
                put_weights(dm, new)
                ED.append_mesh(mesh, dm, unreal.Transform())
        # what was put together has to be whole
        ids = set(mat_ids(mesh))
        if not ids <= set(range(len(pal.mats))):
            raise Fail('level %d: wrong material ids %s' % (level, sorted(ids)))
        used = set()
        for v in vert_ids(mesh):
            ws = out(BW.get_vertex_bone_weights(mesh, v))[0]
            s = sum(x.get_editor_property('weight') for x in ws)
            if abs(s - 1.0) > 0.02:
                raise Fail('level %d: a corner with weights %.3f' % (level, s))
            used.update(x.get_editor_property('bone_index') for x in ws)
        lods.append(mesh)
        tris.append(mesh.get_triangle_count())
        sections.append((len(ids), len(used)))

    path = '%s/%s/SKM_%s_Layer' % (LAYERS, name, name)
    if eal.does_asset_exist(path) and not eal.delete_asset(path):
        raise Fail('the old layer cannot be deleted')
    o = unreal.GeometryScriptCreateNewSkeletalMeshAssetOptions()
    o.set_editor_property('materials', {'%d_%s' % (i, k): m for i, (k, m) in enumerate(zip(pal.kinds, pal.mats))})
    o.set_editor_property('use_mesh_bone_proportions', True)
    o.set_editor_property('enable_recompute_tangents', True)
    sk = out(NA.create_new_skeletal_mesh_asset_from_mesh_lods(lods, bsk.get_editor_property('skeleton'), path, o))[0]
    if not sk:
        raise Fail('the mesh asset cannot be made')
    before = [ses.get_num_verts(sk, i) for i in range(ses.get_lod_count(sk))]
    sk.set_editor_property('lod_settings', lod_settings())         # (how small on the screen each level starts)
    after = [ses.get_num_verts(sk, i) for i in range(ses.get_lod_count(sk))]
    if before != after:
        raise Fail('the level settings changed the mesh: %s -> %s' % (before, after))
    if ses.get_lod_count(sk) != 3:
        raise Fail('%d levels' % ses.get_lod_count(sk))
    if [s.get_editor_property('material_interface') for s in sk.get_editor_property('materials')] != pal.mats:
        raise Fail('materials in a wrong order')
    if sk.get_editor_property('skeleton') != bsk.get_editor_property('skeleton'):
        raise Fail('another skeleton')
    a, b = sk.get_bounds(), bsk.get_bounds()
    if abs(a.box_extent.z - b.box_extent.z) > 25 or abs(a.origin.z - b.origin.z) > 25:
        raise Fail('size %s is not the body\'s %s' % (a.box_extent, b.box_extent))
    if not eal.save_asset(path, False):
        raise Fail('cannot be saved')
    flags = sorted({base_of(m).get_name() for m in pal.mats if not base_of(m).get_editor_property('used_with_skeletal_mesh')})
    if flags:
        notes.append('no skinned-mesh flag: %s' % flags)
    f = os.path.join(DISK, 'Layers', name, 'SKM_%s_Layer.uasset' % name)
    return {'triangles': tris, 'sections': [s for s, _ in sections], 'bones': [b for _, b in sections], 'materials': [m.get_name() for m in pal.mats],
            'source': {'face_lod': fl, 'body_lod': bl, 'skin_dropped': gone}, 'size_mb': round(os.path.getsize(f) / 1048576.0, 2),
            'seconds': round(time.time() - t0, 1), 'notes': notes}


def newest(folder):
    return max([os.path.getmtime(f) for f in glob.glob(os.path.join(folder, '**', '*.uasset'), recursive=True)] or [0])


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    force = 'force' in args
    names = [a for a in args if a != 'force']
    everyone = sorted(d for d in os.listdir(os.path.join(DISK, 'Built')) if d.startswith('MH_') and os.path.isfile(os.path.join(DISK, 'Built', d, 'BP_%s.uasset' % d)))
    if not names:
        raise Fail('say which people: names or all')
    todo = everyone if 'all' in names else []
    for n in names:
        if n == 'all':
            continue
        hit = [e for e in everyone if e == n or e.startswith('MH_%s_' % n)]
        if not hit:
            raise Fail('no person %s' % n)
        todo += [h for h in hit if h not in todo]
    res = {}
    if os.path.isfile(JSON):
        res = json.load(open(JSON, encoding='utf-8'))
    failed, skipped = [], 0
    for i, n in enumerate(todo):
        f = os.path.join(DISK, 'Layers', n, 'SKM_%s_Layer.uasset' % n)
        if not force and os.path.isfile(f) and os.path.getmtime(f) > newest(os.path.join(DISK, 'Built', n)):
            skipped += 1
            log('%s skipped (newer than the person)' % n)
            continue
        try:
            r = bake(n)
            res[n] = r
            log('%s ok: triangles %s, sections %s, bones %s, %.2f MB, %.1f s %s' % (n, r['triangles'], r['sections'], r['bones'], r['size_mb'], r['seconds'], '; '.join(r['notes'])))
        except Exception as e:
            failed.append(n)
            res[n] = {'error': str(e)}
            unreal.log_error('ADP_LAYER: %s FAILED: %s\n%s' % (n, e, traceback.format_exc()))
        json.dump(res, open(JSON, 'w', encoding='utf-8'), indent=1)
        if i % 8 == 7:
            unreal.SystemLibrary.collect_garbage()
    log('DONE %d baked, %d skipped, %d failed %s' % (len(todo) - skipped - len(failed), skipped, len(failed), failed))
    if failed:
        raise Fail('failed: %s' % failed)


main()
