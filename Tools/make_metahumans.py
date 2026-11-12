# The airport's people as MetaHumans (owner, 2026-11-11: bodies for the AI people - passengers, the crews, pilots -
# randomised in how they look, made in Unreal with MetaHumans; only the owner signs in, once, for Epic's cloud rig and
# textures; players never do). Each person starts from one of Epic's preset faces (MetaHuman Creator Core Data) and is
# made their own: the face's shape nudged at random, their own height and build, skin tone, freckles, eye colour, hair,
# brows, beard, and an outfit in their role's colours; then the cloud rig (joints only: crowds) and textures, and the
# optimized game build at a crowd quality. Everything from a seed, so the same list always gives the same people.
#
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs path>/Tools/make_metahumans.py inspect"
#       lists the presets, the wardrobe (hair, brows, beards, outfits) and their colour parameters -> Saved/MetaHumans.json
#   ... "make_metahumans.py build [count] [first]"
#       builds the people (count of them, from number first; already built ones are kept), in /Game/People
import json
import os
import random
import sys
import unreal

args = [a for a in sys.argv[1:] if not a.startswith('-')]
MODE = args[0] if args else 'inspect'
COUNT = int(args[1]) if len(args) > 1 else 40
FIRST = int(args[2]) if len(args) > 2 else 0   # (80 people: 0-79)
OUT = '/Game/People'
SAVED = os.path.join(unreal.Paths.project_saved_dir(), 'MetaHumans.json')
ar = unreal.AssetRegistryHelpers.get_asset_registry()
eal = unreal.EditorAssetLibrary
mhs = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)


def log(m):
    unreal.log('ADP_MH: ' + m)


def assets_under(path, cls=None):
    out = []
    for p in eal.list_assets(path, recursive=True, include_folder=False):
        a = unreal.load_asset(p)
        if a is not None and (cls is None or isinstance(a, cls)):
            out.append(a)
    return out


# ---- what the library holds
def inspect():
    info = {'presets': [], 'wardrobe': {}}
    # which of Epic's preset faces are women (the others men; those not sure of are left out of both)
    women = ('Ada', 'Aera', 'Aoi', 'Asha', 'Celeste', 'Etta', 'Grace', 'Jelena', 'Lani', 'Sook-ja', 'Sunita', 'Tuya', 'Vivian', 'Zuri')
    unsure = ('Bo', 'Cameron')
    info['female'] = []
    for c in assets_under('/MetaHumanCharacter/Optional/Presets', unreal.MetaHumanCharacter):
        if c.get_name() in unsure:
            continue
        info['presets'].append(c.get_path_name())
        if c.get_name() in women:
            info['female'].append(c.get_path_name())
    for slot, path in (('Hair', 'Grooms/Bindings/Hair'), ('Eyebrows', 'Grooms/Bindings/Eyebrows'), ('Beard', 'Grooms/Bindings/Beards'),
                       ('Mustache', 'Grooms/Bindings/Mustaches'), ('Eyelashes', 'Grooms/Bindings/Eyelashes'), ('Outfits', 'Clothing')):
        items = []
        for p in eal.list_assets('/MetaHumanCharacter/Optional/' + path, recursive=True, include_folder=False):
            if os.path.basename(p).split('.')[0].startswith('WI_'):
                items.append(p.split('.')[0])
        info['wardrobe'][slot] = items
    with open(SAVED, 'w') as f:
        json.dump(info, f, indent=1)
    log('%d presets; wardrobe %s -> %s' % (len(info['presets']), {k: len(v) for k, v in info['wardrobe'].items()}, SAVED))
    return info


# ---- who works and travels at the airport (owner: as in the Babylon edition: passengers, cabin crew, pilots, ground
# crew, security, the fire crew), how many of each in a list of 40, and the colours of what they wear
ROLES = [
    # role, share, outfit colours (primary, secondary)
    ('passenger', 18, None),
    ('cabin', 6, ((0.02, 0.05, 0.16), (0.85, 0.85, 0.88))),       # navy jacket, white blouse / shirt
    ('pilot', 5, ((0.92, 0.93, 0.95), (0.02, 0.02, 0.03))),       # white shirt, black trousers
    ('ground', 5, ((1.0, 0.42, 0.0), (0.05, 0.06, 0.08))),         # hi-vis orange, dark trousers
    ('security', 3, ((0.03, 0.03, 0.035), (0.6, 0.62, 0.66))),     # black, grey
    ('fire', 3, ((0.04, 0.05, 0.09), (0.9, 0.7, 0.05))),           # dark navy, yellow trim
]
PASSENGER_COLOURS = [(0.55, 0.08, 0.06), (0.05, 0.2, 0.45), (0.1, 0.3, 0.12), (0.7, 0.62, 0.45), (0.15, 0.15, 0.17), (0.8, 0.8, 0.78),
                     (0.45, 0.25, 0.1), (0.35, 0.05, 0.3), (0.6, 0.45, 0.05), (0.2, 0.35, 0.5)]


def role_of(i):
    n = i % 40
    for role, share, cols in ROLES:
        if n < share:
            return role, cols
        n -= share
    return 'passenger', None


BALANCE = {13: {'age': 68}, 14: {'age': 66}, 15: {'age': 71}, 16: {'age': 79}, 17: {'age': 81},
           26: {'female': True}, 38: {'female': True},
           # the second forty (owner: double everything, for hiring lots of crew): again 20 women and 20 men, old of both
           50: {'female': False}, 56: {'female': False}, 61: {'female': False}, 70: {'female': False}, 73: {'female': False},
           79: {'female': False}, 51: {'age': 78}, 52: {'age': 84}, 53: {'age': 67}}


def _legacy_person(i):
    """The first 14 (built 2026-11-11/12 before the plan below), as they were made."""
    rng = random.Random(20261111 + i * 7919)
    role, cols = role_of(i)
    female = rng.random() < (0.55 if role == 'cabin' else 0.35 if role in ('pilot', 'ground', 'fire', 'security') else 0.5)
    age = rng.choice([22, 28, 34, 41, 48, 55, 63, 72]) if role == 'passenger' else rng.choice([24, 29, 35, 42, 50, 57])
    # (owner, 2026-11-12: as many women as men, 20 and 20, and old people of both: these set it, the rest stays random)
    o = BALANCE.get(i, {})
    female = o.get('female', female); age = o.get('age', age)
    height = min(181.0, max(152.0, rng.gauss(166, 6.5))) if female else min(196.0, max(162.0, rng.gauss(179, 7)))
    build = rng.choice(['slim', 'average', 'average', 'sturdy', 'heavy'] if role == 'passenger' else ['slim', 'average', 'average', 'sturdy'])
    if not cols:
        cols = (rng.choice(PASSENGER_COLOURS), rng.choice(PASSENGER_COLOURS))
    return {
        'i': i, 'role': role, 'female': female, 'age': age, 'height': round(height, 1), 'build': build,
        'skin_u': round(rng.uniform(0.05, 0.95), 3), 'skin_v': round(rng.uniform(0.15, 0.85), 3),
        'freckles': round(max(0.0, rng.gauss(0.08, 0.12)), 3), 'eyes': rng.choice(['brown', 'brown', 'brown', 'hazel', 'blue', 'green', 'grey']),
        'hair_melanin': round(min(1.0, max(0.0, rng.gauss(0.55, 0.3))), 3), 'grey': round(max(0.0, (age - 40) / 40.0) * rng.uniform(0.4, 1.0), 3),
        'beard': (not female) and rng.random() < 0.35, 'colours': cols, 'face_seed': rng.randrange(1 << 30), 'pick': rng.randrange(1 << 30),
    }


# ---- the 120 (owner, 2026-11-12): crowd quality, 60 women and 60 men; passengers with old people of both sexes; each
# new person the game makes takes one of these and mixes hair colour, grey, skin details, clothes and accessories
LEGACY = 14
ROLE_RANGES = [('passenger', 0, 40), ('cabin', 40, 60), ('pilot', 60, 76), ('ground', 76, 92), ('security', 92, 106), ('fire', 106, 120)]
ROLE_COLS = {r: c for r, _, c in ROLES}
COUNT_ALL = 120
# ---- the extras (owner, 2026-11-12): 7 very overweight passengers (120-126, women among them: today's 'heavy' is only
# a little fuller at the waist) and 10 teenagers (127-136, 14 to 17, women among them: most shorter than adults, two of
# adult size, slim, smooth young skin, no beard, no grey, from the youngest-looking preset faces)
OBESE = (120, 127)
TEENS = (127, 137)
COUNT_ALL = 137
YOUNG_FACES = ('Aera', 'Aoi', 'Etta', 'Zuri', 'Tuya', 'Trey', 'Mateo', 'Orlando', 'Victor', 'Mikel')
# how much fuller each part of a very overweight body is (matched against the body constraints' names)
OBESE_PARTS = (('waist', 1.5), ('belly', 1.5), ('stomach', 1.5), ('hip', 1.35), ('chest', 1.3), ('bust', 1.3), ('thigh', 1.32),
               ('upper_arm', 1.3), ('forearm', 1.15), ('arm', 1.2), ('neck', 1.15), ('calf', 1.18), ('shoulder', 1.08))


def _extra_person(i):
    rng = random.Random(20261112 + i * 7919)
    teen = TEENS[0] <= i < TEENS[1]
    k = i - (TEENS[0] if teen else OBESE[0])
    female = k % 2 == 0   # women and men turn about: 4 of the 7 overweight, 5 of the 10 teenagers
    if teen:
        age = rng.randint(14, 17)
        if k in (7, 9): height = rng.uniform(168, 174) if female else rng.uniform(176, 183)   # the two of adult size
        else: height = rng.uniform(150, 166) if female else rng.uniform(152, 172)
        build = rng.choice(['slim', 'slim', 'average'])
    else:
        age = rng.choice([24, 31, 38, 45, 52, 58, 64])
        height = rng.uniform(156, 172) if female else rng.uniform(166, 184)
        build = 'obese'
    return {
        'i': i, 'role': 'passenger', 'female': female, 'age': age, 'height': round(height, 1), 'build': build, 'teen': teen,
        'skin_u': round(rng.uniform(0.05, 0.95), 3), 'skin_v': round(rng.uniform(0.15, 0.85), 3),
        'freckles': round(max(0.0, rng.gauss(0.15 if teen else 0.08, 0.12)), 3), 'eyes': rng.choice(['brown', 'brown', 'brown', 'hazel', 'blue', 'green', 'grey']),
        'hair_melanin': round(min(1.0, max(0.0, rng.gauss(0.55, 0.3))), 3), 'grey': 0.0 if teen else round(min(1.0, max(0.0, (age - 40) / 35.0) * rng.uniform(0.5, 1.0)), 3),
        'beard': (not female) and (not teen) and rng.random() < 0.35, 'colours': (rng.choice(PASSENGER_COLOURS), rng.choice(PASSENGER_COLOURS)),
        'face_seed': rng.randrange(1 << 30), 'pick': rng.randrange(1 << 30),
    }


def _plan():
    sex, age = {}, {}
    for role, a, b in ROLE_RANGES:
        old = [_legacy_person(i) for i in range(a, min(b, LEGACY))]
        new = list(range(max(a, LEGACY), b))
        rng = random.Random('plan-' + role)
        rng.shuffle(new)
        need_f = max(0, (b - a) // 2 - sum(1 for o in old if o['female']))
        for k, i in enumerate(new):
            sex[i] = k < need_f
        if role == 'passenger':
            old_f = max(0, 8 - sum(1 for o in old if o['female'] and o['age'] >= 65))
            old_m = max(0, 8 - sum(1 for o in old if not o['female'] and o['age'] >= 65))
            for i in new:
                if sex[i] and old_f > 0: age[i] = rng.randint(66, 79); old_f -= 1
                elif not sex[i] and old_m > 0: age[i] = rng.randint(67, 84); old_m -= 1
                else: age[i] = rng.choice([19, 23, 27, 31, 36, 42, 47, 53, 58, 62])
        else:
            lo, hi = (26, 60) if role == 'pilot' else (21, 58)
            for i in new: age[i] = rng.randint(lo, hi)
    return sex, age


_PLAN = None


def person(i):
    """Everything about person i, decided from the seed."""
    global _PLAN
    if i < LEGACY:
        return _legacy_person(i)
    if i >= OBESE[0]:
        return _extra_person(i)
    if _PLAN is None:
        _PLAN = _plan()
    rng = random.Random(20261112 + i * 7919)
    role = next(r for r, a, b in ROLE_RANGES if a <= i < b)
    cols = ROLE_COLS[role]
    female, age = _PLAN[0][i], _PLAN[1][i]
    height = min(181.0, max(152.0, rng.gauss(166 - max(0, age - 65) * 0.15, 6.5))) if female else min(196.0, max(162.0, rng.gauss(179 - max(0, age - 65) * 0.15, 7)))
    build = rng.choice(['slim', 'average', 'average', 'sturdy', 'heavy'] if role == 'passenger' else ['slim', 'average', 'average', 'sturdy'])
    if not cols:
        cols = (rng.choice(PASSENGER_COLOURS), rng.choice(PASSENGER_COLOURS))
    return {
        'i': i, 'role': role, 'female': female, 'age': age, 'height': round(height, 1), 'build': build,
        'skin_u': round(rng.uniform(0.05, 0.95), 3), 'skin_v': round(rng.uniform(0.15, 0.85), 3),
        'freckles': round(max(0.0, rng.gauss(0.08, 0.12)), 3), 'eyes': rng.choice(['brown', 'brown', 'brown', 'hazel', 'blue', 'green', 'grey']),
        'hair_melanin': round(min(1.0, max(0.0, rng.gauss(0.55, 0.3))), 3), 'grey': round(min(1.0, max(0.0, (age - 40) / 35.0) * rng.uniform(0.5, 1.0)), 3),
        'beard': (not female) and rng.random() < 0.35, 'colours': cols, 'face_seed': rng.randrange(1 << 30), 'pick': rng.randrange(1 << 30),
    }


# ---- making one
def make(spec, info):
    name = 'MH_%03d_%s' % (spec['i'], spec['role'])
    path = OUT + '/' + name
    if eal.does_asset_exist(path):
        log(name + ' already made')
        return
    rng = random.Random(spec['pick'])
    presets = info['presets']
    if not presets:
        unreal.log_error('ADP_MH: no presets: is MetaHuman Creator Core Data installed?')
        return
    # the starting face: a preset of this person's sex (Saved/MetaHumans.json's 'female' list, filled once from the
    # presets' looks), then made this person's own
    fem = set(info.get('female', []))
    pool = [p for p in presets if (p in fem) == spec['female']] if fem else presets
    if spec.get('teen'):
        pool = [p for p in pool if os.path.basename(p).split('.')[0] in YOUNG_FACES] or pool
    src = (pool or presets)[rng.randrange(len(pool or presets))]
    ch = eal.duplicate_asset(src, path)
    if ch is None:
        unreal.log_error('ADP_MH: could not copy ' + src)
        return
    if not mhs.try_add_object_to_edit(ch):
        unreal.log_error('ADP_MH: could not edit ' + name)
        return
    try:
        # the face: every landmark moved a little, smoothly (neighbours together), so no two faces are the same
        frng = random.Random(spec['face_seed'])
        lm = mhs.get_face_landmarks(ch)
        if lm:
            # a few smooth waves across the face (each a direction it pushes, a spatial frequency and a phase): nearby
            # landmarks move together, so the face changes shape without going lumpy; a few millimetres at most
            import math
            waves = []
            for _ in range(5):
                waves.append(((frng.uniform(-1, 1), frng.uniform(-1, 1), frng.uniform(-1, 1)),
                              (frng.uniform(-0.25, 0.25), frng.uniform(-0.25, 0.25), frng.uniform(-0.25, 0.25)),
                              frng.uniform(0, 6.283), frng.uniform(0.12, 0.35)))
            deltas = []
            for v in lm:
                dx = dy = dz = 0.0
                for (push, freq, ph, amp) in waves:
                    s = amp * math.sin(v.x * freq[0] + v.y * freq[1] + v.z * freq[2] + ph)
                    dx += push[0] * s; dy += push[1] * s; dz += push[2] * s
                deltas.append(unreal.Vector(dx, dy, dz))
            mhs.translate_face_landmarks(ch, list(range(len(lm))), deltas)
            mhs.commit_face_state(ch)
        # the body: their height and build
        cons = {str(c.name).lower().replace(' ', '_'): c for c in mhs.get_body_constraints(ch)}
        if 'height' in cons:
            cons['height'].is_active = True; cons['height'].target_measurement = spec['height']
        if spec['build'] == 'obese':
            # every part of the body fuller, the middle most (the first part name found in each constraint's name)
            log('body constraints: ' + ', '.join(sorted(cons)))
            for key, c in cons.items():
                if key == 'height':
                    continue
                f = next((f for part, f in OBESE_PARTS if part in key), None)
                if f: c.is_active = True; c.target_measurement = c.target_measurement * f
        else:
            girth = {'slim': 0.9, 'average': 1.0, 'sturdy': 1.08, 'heavy': 1.2}[spec['build']]
            for key in ('waist', 'waist_circumference', 'belly'):
                if key in cons:
                    c = cons[key]; c.is_active = True; c.target_measurement = c.target_measurement * girth
        mhs.set_body_constraints(ch, list(cons.values()))
        mhs.commit_body_state(ch)
        # the skin
        sk = unreal.MetaHumanCharacterSkinSettings()
        sp = unreal.MetaHumanCharacterSkinProperties(); sp.u = spec['skin_u']; sp.v = spec['skin_v']; sp.roughness = (0.68 if spec.get('teen') else 0.8) + 0.1 * rng.random()
        sk.skin = sp
        fr = unreal.MetaHumanCharacterFrecklesProperties(); fr.density = spec['freckles']; fr.strength = min(1.0, spec['freckles'] * 3)
        sk.freckles = fr
        mhs.commit_skin_settings(ch, sk)
        # hair, brows, beard, outfit from the library (owner can add more later; this picks among what is there)
        def wear(slot, keyword_ok=None):
            items = [p for p in info['wardrobe'].get(slot, []) if keyword_ok is None or keyword_ok(p.lower())]
            if not items:
                return None
            wi = unreal.load_asset(items[rng.randrange(len(items))])
            key = ch.internal_collection.try_add_item_from_wardrobe_item(slot if slot != 'Beard' else 'Beard', wi)
            ch.internal_collection.default_instance.try_add_slot_selection(unreal.MetaHumanPipelineSlotSelection(slot_name=slot, selected_item=key))
            return key
        long_ok = (lambda p: True) if spec['female'] else (lambda p: '_l_' not in p or rng.random() < 0.1)
        hair = wear('Hair', long_ok)
        wear('Eyebrows')
        if spec['beard']:
            wear('Beard')
        outfit = wear('Outfits')
        mhs.assemble_for_preview(character=ch)
        # colours: the hair's darkness and grey, the outfit in the role's colours
        def params(key):
            return ch.internal_collection.default_instance.get_instance_parameters(item_path=unreal.MetaHumanPaletteItemPath(item_key=key)) if key else []
        for p in params(hair):
            if p.name == 'Melanin': p.set_float(value=spec['hair_melanin'])
            if p.name in ('Greyness', 'Grayness', 'GreyAmount'): p.set_float(value=spec['grey'])
        prim, sec = spec['colours']
        for p in params(outfit):
            n = str(p.name).lower()
            if 'primarycolor' in n: p.set_color(value=unreal.LinearColor(prim[0], prim[1], prim[2], 1))
            elif 'secondarycolor' in n: p.set_color(value=unreal.LinearColor(sec[0], sec[1], sec[2], 1))
        # Epic's cloud: the face rig (joints only: lighter, for crowds) and the skin textures
        rq = unreal.MetaHumanCharacterAutoRiggingRequestParams(); rq.blocking = True; rq.report_progress = False
        rq.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        mhs.request_auto_rigging(ch, rq)
        tq = unreal.MetaHumanCharacterTextureRequestParams(); tq.blocking = True; tq.report_progress = False
        mhs.request_texture_sources(ch, tq)
        # the game build: optimized, crowd quality
        bp = unreal.MetaHumanCharacterEditorBuildParameters()
        bp.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        bp.pipeline_quality = unreal.MetaHumanQualityLevel.LOW   # crowd quality (owner, 2026-11-12): card hair, lighter textures
        bp.absolute_build_path = OUT + '/Built'
        bp.common_folder_path = OUT + '/Common'
        bp.enable_wardrobe_item_validation = False
        mhs.build_meta_human(character=ch, params=bp)
        eal.save_loaded_asset(ch, False)
        # the built person (their blueprint, meshes, textures) and the shared parts, saved to disk
        eal.save_directory(OUT + '/Built/' + name, only_if_is_dirty=False, recursive=True)
        eal.save_directory(OUT + '/Common', only_if_is_dirty=False, recursive=True)
        log('%s made: %s, %s, %d, %.0f cm, %s, from %s' % (name, 'teenager' if spec.get('teen') else spec['role'], ('girl' if spec.get('teen') else 'woman') if spec['female'] else ('boy' if spec.get('teen') else 'man'), spec['age'], spec['height'], spec['build'], os.path.basename(src)))
    except Exception as e:
        unreal.log_error('ADP_MH: %s failed: %s' % (name, e))
    finally:
        if mhs.is_object_added_for_editing(ch):
            mhs.remove_object_to_edit(ch)


try:
    info = inspect()
    if MODE == 'build':
        for i in range(FIRST, FIRST + COUNT):
            make(person(i), info)
        with open(os.path.join(unreal.Paths.project_saved_dir(), 'MetaHumanPeople.json'), 'w') as f:
            json.dump([person(i) for i in range(FIRST, FIRST + COUNT)], f, indent=1)
except Exception as e:
    unreal.log_error('ADP_MH: ' + str(e))
unreal.SystemLibrary.quit_editor()
