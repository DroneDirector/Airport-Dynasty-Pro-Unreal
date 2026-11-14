# The game's content in chunks (one pak file each), so an update only replaces the buildings that changed.
# Run inside the editor (UnrealEditor-Cmd <project> -ExecutePythonScript=Tools/make_chunk_labels.py): it writes one
# Primary Asset Label per chunk into /Game/Chunks. Each building's meshes are found from the Babylon model's own node tree
# (Data/babylon-terminal.json: TERMINAL/SITE/APRON/J-1 is jetway 1, .../RENTAL the car rental, ...); everything inside a
# building (its furniture, desks, seats, signs) goes with it. A mesh used by more than one building goes in the shared
# chunk; every material and texture goes in the materials chunk (a material change then updates only that file).
#   chunk 0  the game itself (maps, menus, UI, the engine's content, everything not below)
#   chunk 1  materials and textures      chunk 2  the terminal (with the lounge and the gate seats)
#   chunk 3  the jetways                 chunk 4  the car rental
#   chunk 5  the site (roads, car park, parked cars, trees)
#   chunk 6  the aircraft (the parked ones and the airliner models)
#   chunk 7  meshes shared by several buildings
#   chunk 8  the apron light masts (their materials are in chunk 1)
#   chunk 9  the fire station                chunk 10 the fire trucks
#   chunk 11 the offices, the ops room, the academy and their screens, the office pets, the phone
#   chunk 12 the characters (the walking player)   chunk 13 the windsocks
# (2026-11-12, owner: "content packs") everything big now has its own chunk, so chunk 0 holds only the maps, the menus and
# the engine's own content: an update re-sends only the chunks whose content changed.
# The ATC tower is built by the game's code from shared kit parts (no meshes of its own): its changes are in the program.
import json, os
from collections import defaultdict
import unreal

ROOT = unreal.Paths.project_dir()
# (the true-Unreal terminal, 2026-11-14: its own layout and its native pieces, Data/terminal-layout.json + terminal-pieces.json)
J = json.load(open(os.path.join(ROOT, 'Data', 'terminal-layout.json')))
PIECES = json.load(open(os.path.join(ROOT, 'Data', 'terminal-pieces.json')))
N = J['nodes']
parent = {}
for i, n in enumerate(N):
    for c in n.get('children', []): parent[c] = i

def group(i):
    names = []
    while True:
        names.append(N[i].get('name', ''))
        if i not in parent: break
        i = parent[i]
    path = '/'.join(reversed(names))
    if '/AIRCRAFT' in path: return 'aircraft'
    if '/RENTAL' in path or path.startswith('RENTAL'): return 'rental'
    if '/APRON/J-' in path: return 'jetways'
    if path.startswith('TERMINAL/SITE') and '/KERB' not in path: return 'site'
    return 'terminal'

used = defaultdict(set)
for i, n in enumerate(N):
    if 'mesh' in n: used[PIECES.get('terminal_mesh_%d' % n['mesh'], ['terminal_mesh_%d' % n['mesh']])[0]].add(group(i))

CHUNK = {'terminal': 2, 'jetways': 3, 'rental': 4, 'site': 5, 'aircraft': 6, 'shared': 7}
lists = defaultdict(list)
for m, gs in used.items():
    g = next(iter(gs)) if len(gs) == 1 else 'shared'
    lists[g].append('/Game/Terminal/Meshes/%s.%s' % (m, m))

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

def label(name, chunk, priority, assets=(), dirs=()):
    path = '/Game/Chunks/' + name
    a = eal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(name, '/Game/Chunks', unreal.PrimaryAssetLabel, None)
    rules = a.get_editor_property('rules')
    rules.set_editor_property('chunk_id', chunk)
    rules.set_editor_property('priority', priority)
    rules.set_editor_property('cook_rule', unreal.PrimaryAssetCookRule.ALWAYS_COOK)
    a.set_editor_property('rules', rules)
    a.set_editor_property('label_assets_in_my_directory', False)
    a.set_editor_property('is_runtime_label', False)
    a.set_editor_property('explicit_assets', [unreal.load_asset(p) for p in assets if eal.does_asset_exist(p.split('.')[0])])
    # whole folders by their assets
    folder_assets = []
    for d in dirs:
        for p in eal.list_assets(d, recursive=True, include_folder=False):
            folder_assets.append(p)
    if folder_assets:
        a.set_editor_property('explicit_assets', list(a.get_editor_property('explicit_assets')) + [unreal.load_asset(p) for p in folder_assets])
    eal.save_loaded_asset(a)
    unreal.log('chunk %d %s: %d assets' % (chunk, name, len(a.get_editor_property('explicit_assets'))))

# the materials first and above the rest (priority): shared by every building, kept in one file
label('Chunk01_Materials', 1, 10, dirs=['/Game/Materials', '/Game/Babylon/Masters', '/Game/Terminal/Masters', '/Game/Terminal/Materials',
      '/Game/Terminal/Textures', '/Game/Terminal/Detail', '/Game/Babylon/Lounge/lounge/Materials', '/Game/Babylon/Lounge/lounge/Textures',
      '/Game/Babylon/Airliner/airliner/Materials', '/Game/Babylon/Airliner/airliner/Textures', '/Game/Babylon/GateSeat/gateseat/Materials'])
label('Chunk02_Terminal', 2, 5, assets=lists['terminal'], dirs=['/Game/Babylon/Lounge/lounge/StaticMeshes', '/Game/Babylon/GateSeat/gateseat/StaticMeshes'])
label('Chunk03_Jetways', 3, 5, assets=lists['jetways'])
label('Chunk04_CarRental', 4, 5, assets=lists['rental'])
label('Chunk05_Site', 5, 5, assets=lists['site'])
label('Chunk06_Aircraft', 6, 5, assets=lists['aircraft'], dirs=['/Game/Babylon/Airliner/airliner/StaticMeshes'])
label('Chunk07_Shared', 7, 5, assets=lists['shared'])
label('Chunk08_LightMasts', 8, 5, dirs=['/Game/LightMast'])
label('Chunk09_FireStation', 9, 5, dirs=['/Game/Babylon/Firestation'])
label('Chunk10_FireTrucks', 10, 5, dirs=['/Game/Babylon/Firetruck1', '/Game/Babylon/Firetruck2', '/Game/Babylon/WashHose'])
label('Chunk11_Offices', 11, 5, dirs=['/Game/Babylon/Office', '/Game/Babylon/Ops', '/Game/Babylon/Academy', '/Game/Babylon/Monitor169',
      '/Game/Babylon/Monitor219', '/Game/Babylon/Monitor329', '/Game/Babylon/Officedog', '/Game/Babylon/Officecat', '/Game/Babylon/Smartphone'])
label('Chunk12_Characters', 12, 5, dirs=['/Game/Characters'])
label('Chunk13_WindSocks', 13, 5, dirs=['/Game/WindSock'])
# the terminal's meshes the node tree doesn't name (merged and split parts): with the terminal, below the labels above
label('Chunk02b_TerminalRest', 2, 1, dirs=['/Game/Terminal/Meshes'])
unreal.log('chunk labels done: ' + ', '.join('%s %d' % (k, len(v)) for k, v in lists.items()))
