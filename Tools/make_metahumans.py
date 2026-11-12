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
#   ... "make_metahumans.py refit [count] [first]"
#       the people already made, put right without Epic's cloud (2026-11-12: one hairstyle instead of the preset's own
#       plus ours, the body's build through the Fat measure, hair colour and grey on the one hair, beards only where
#       meant) and built again at crowd quality; a person whose body is the wrong sex is reported and left for 'build'
#       (delete their asset and Built folder first)
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
    # (2026-11-12, read from the presets' own bodies, Saved/MetaHumanPresetBodies.json: Aoi is a man, Bo and Cameron too)
    women = ('Ada', 'Aera', 'Asha', 'Celeste', 'Etta', 'Grace', 'Jelena', 'Lani', 'Sook-ja', 'Sunita', 'Tuya', 'Vivian', 'Zuri')
    unsure = ()
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

# ---- the owner's own character (2026-11-12): a man, built at a closer quality than the crowd (strand hair comes later
# as an option); his face can be changed later by building him again from another preset (4 min)
def player_spec():
    rng = random.Random(777)
    return {
        'i': 900, 'role': 'player', 'female': False, 'age': 38, 'height': 183.0, 'build': 'average',
        # light European skin (owner, 2026-11-12: Caucasian, like him), short dark hair, no beard
        'skin_u': 0.08, 'skin_v': 0.4, 'freckles': 0.0, 'eyes': 'blue', 'hair_melanin': 0.75, 'grey': 0.05,
        'beard': False, 'colours': ((0.08, 0.09, 0.12), (0.9, 0.9, 0.92)), 'face_seed': rng.randrange(1 << 30), 'pick': rng.randrange(1 << 30),
        'preset': 'Cameron', 'quality': 'MEDIUM', 'hair': 'WI_Hair_S_SideSweptFringe',
    }


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


# ---- the body's build: the parametric body's own measures (Saved/MetaHumanPresetBodies.json lists them all):
# 'Fat' (-2.75 .. 2.75) is the one that makes a person slim or heavy; the waist and hips go with it for the very
# overweight; teenagers a little less muscular; the height as asked
FAT = {'slim': -1.0, 'average': 0.2, 'sturdy': 1.2, 'heavy': 2.2, 'obese': 2.75}   # (2026-11-12 test: 1.5 still looked slim, 2.6 clearly heavy)


def body_shape(ch, cons, spec):
    if 'height' in cons:
        cons['height'].is_active = True; cons['height'].target_measurement = spec['height']
    if 'fat' in cons:
        cons['fat'].is_active = True; cons['fat'].target_measurement = FAT[spec['build']]
    if spec['build'] == 'obese':
        for key, f in (('waist', 1.3), ('hip', 1.18), ('high_hip', 1.25), ('chest', 1.15), ('thigh', 1.2), ('neck', 1.1)):
            if key in cons:
                cons[key].is_active = True; cons[key].target_measurement = cons[key].target_measurement * f
    if spec.get('teen') and 'muscularity' in cons:
        cons['muscularity'].is_active = True; cons['muscularity'].target_measurement = -0.8
    mhs.set_body_constraints(ch, list(cons.values()))
    mhs.commit_body_state(ch)


def masc_fem(ch):
    """The body's own Masculine/Feminine measure (above 0 a woman's body)."""
    for c in mhs.get_body_constraints(ch):
        if str(c.name).lower().startswith('masculine'):
            return float(c.target_measurement)
    return 0.0


# ---- the wardrobe: one item in a slot (the preset's own replaced, never added to), or the slot emptied
def wear_one(ch, slot, item_path, rng=None):
    if item_path is None:
        try:
            ch.internal_collection.default_instance.set_single_slot_selection(slot_name=slot, item_key=unreal.MetaHumanPaletteItemKey())
        except Exception as e:
            log('%s: could not empty the %s slot: %s' % (ch.get_name(), slot, e))
        return None
    wi = unreal.load_asset(item_path)
    key = ch.internal_collection.try_add_item_from_wardrobe_item(slot, wi)
    ch.internal_collection.default_instance.set_single_slot_selection(slot_name=slot, item_key=key)
    return key


def dress(ch, spec, info, rng):
    """Hair, brows, beard (or none), the outfit: one of each, in this person's colours."""
    def pick(slot, keyword_ok=None):
        items = [p for p in info['wardrobe'].get(slot, []) if keyword_ok is None or keyword_ok(p.lower())]
        return items[rng.randrange(len(items))] if items else None
    long_ok = (lambda p: True) if spec['female'] else (lambda p: '_l_' not in p or rng.random() < 0.1)
    hair = wear_one(ch, 'Hair', next((p for p in info['wardrobe'].get('Hair', []) if p.endswith('/' + spec['hair'])), None) if spec.get('hair') else pick('Hair', long_ok))
    wear_one(ch, 'Eyebrows', pick('Eyebrows'))
    wear_one(ch, 'Beard', pick('Beard') if spec['beard'] else None)
    wear_one(ch, 'Mustache', None)
    outfit = wear_one(ch, 'Outfits', pick('Outfits'))
    mhs.assemble_for_preview(character=ch)
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
    sel = [str(d.selection.slot_name) for d in ch.internal_collection.default_instance.get_slot_selection_data()]
    log('%s wears: %s' % (ch.get_name(), ', '.join(sel)))


QUALITY = 'LOW'


def build_and_save(ch, name):
    bp = unreal.MetaHumanCharacterEditorBuildParameters()
    bp.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    bp.pipeline_quality = getattr(unreal.MetaHumanQualityLevel, QUALITY)   # crowd quality LOW (owner, 2026-11-12): card hair, lighter textures; the owner's own character closer
    bp.absolute_build_path = OUT + '/Built'
    bp.common_folder_path = OUT + '/Common'
    bp.enable_wardrobe_item_validation = False
    mhs.build_meta_human(character=ch, params=bp)
    eal.save_loaded_asset(ch, False)
    eal.save_directory(OUT + '/Built/' + name, only_if_is_dirty=False, recursive=True)
    eal.save_directory(OUT + '/Common', only_if_is_dirty=False, recursive=True)


def refit(spec, info):
    name = 'MH_%03d_%s' % (spec['i'], spec['role'])
    path = OUT + '/' + name
    if not eal.does_asset_exist(path):
        log(name + ' not made yet (build it)')
        return
    ch = unreal.load_asset(path)
    if not mhs.try_add_object_to_edit(ch):
        unreal.log_error('ADP_MH: could not edit ' + name)
        return
    try:
        mf = masc_fem(ch)
        if spec['female'] != (mf > -0.3):
            log('%s SEX MISMATCH: planned %s, the body says %.1f (build it again from a right preset)' % (name, 'woman' if spec['female'] else 'man', mf))
            return
        cons = {str(c.name).lower().replace(' ', '_').replace('/', '_'): c for c in mhs.get_body_constraints(ch)}
        body_shape(ch, cons, spec)
        dress(ch, spec, info, random.Random(spec['pick'] ^ 0x5A5A))
        # the old build's files go (the preset's hair and beard would stay on disk otherwise)
        if eal.does_directory_exist(OUT + '/Built/' + name): eal.delete_directory(OUT + '/Built/' + name)
        build_and_save(ch, name)
        log('%s refitted: %s, %s, %d, %.0f cm, %s' % (name, spec['role'], 'woman' if spec['female'] else 'man', spec['age'], spec['height'], spec['build']))
    except Exception as e:
        unreal.log_error('ADP_MH: %s refit failed: %s' % (name, e))
    finally:
        if mhs.is_object_added_for_editing(ch):
            mhs.remove_object_to_edit(ch)


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
    if spec.get('preset'): src = next((p for p in presets if os.path.basename(p).split('.')[0] == spec['preset']), src)
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
        body_shape(ch, cons, spec)
        # the skin
        sk = unreal.MetaHumanCharacterSkinSettings()
        sp = unreal.MetaHumanCharacterSkinProperties(); sp.u = spec['skin_u']; sp.v = spec['skin_v']; sp.roughness = (0.68 if spec.get('teen') else 0.8) + 0.1 * rng.random()
        sk.skin = sp
        fr = unreal.MetaHumanCharacterFrecklesProperties(); fr.density = spec['freckles']; fr.strength = min(1.0, spec['freckles'] * 3)
        sk.freckles = fr
        mhs.commit_skin_settings(ch, sk)
        # hair, brows, beard (or none), the outfit: one of each in place of the preset's own, in this person's colours
        dress(ch, spec, info, rng)
        # Epic's cloud: the face rig (joints only: lighter, for crowds) and the skin textures
        rq = unreal.MetaHumanCharacterAutoRiggingRequestParams(); rq.blocking = True; rq.report_progress = False
        rq.rig_type = unreal.MetaHumanRigType.JOINTS_ONLY
        mhs.request_auto_rigging(ch, rq)
        tq = unreal.MetaHumanCharacterTextureRequestParams(); tq.blocking = True; tq.report_progress = False
        mhs.request_texture_sources(ch, tq)
        # the game build: optimized, crowd quality; the person and the shared parts saved to disk
        build_and_save(ch, name)
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
    if MODE == 'player':
        QUALITY = 'MEDIUM'
        make(player_spec(), info)
    if MODE == 'refit':
        for i in range(FIRST, FIRST + COUNT):
            refit(person(i), info)
        with open(os.path.join(unreal.Paths.project_saved_dir(), 'MetaHumanPeople.json'), 'w') as f:
            json.dump([person(i) for i in range(FIRST, FIRST + COUNT)], f, indent=1)
except Exception as e:
    unreal.log_error('ADP_MH: ' + str(e))
unreal.SystemLibrary.quit_editor()
