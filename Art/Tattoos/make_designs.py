# Tattoo designs as the game lays them (owner, 2026-11-15: any design on any place, chosen in the creator or at the
# tattooist, removable): each design is one picture in its place's own frame - u round the limb (0.5 = the outer side)
# or across a patch, v down the place (Art/Tattoos/make_spots.py: arms shoulder 0 -> knuckles 1 with the elbow at 0.30
# and the wrist at 0.81; legs hip 0 -> top of the foot 1, knee 0.29, ankle 0.78; neck patches 8 cm across, collar to
# under the jaw; fingers: four quarters, knuckle to the last joint - never the nail). Writes Designs/T_TatDesign_<id>.png
# and Data/tattoos.json (the list the game reads: id, name, place kind, colour or black and grey).
#   python Art/Tattoos/make_designs.py
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'Designs'); os.makedirs(OUT, exist_ok=True)
DATA = os.path.join(HERE, '..', '..', 'Data', 'tattoos.json')
spots = json.load(open(os.path.join(HERE, 'Placed', 'Spots', 'spots.json')))
WRIST = spots['arm_l_marks']['wrist']; ARM_CM = spots['arm_l_marks']['length_cm']
designs = []


def canvas(w, h): return Image.new('RGBA', (w, h), (0, 0, 0, 0))


def save(img, id_, name, kind, colour):
    img.save(os.path.join(OUT, 'T_TatDesign_%s.png' % id_))
    designs.append({'id': id_, 'name': name, 'kind': kind, 'colour': colour, 'picture': '/Game/People/Tattoos/Designs/T_TatDesign_%s' % id_})


# --- the sleeves (place_tattoos.py's unrolled sleeve: shoulder 0 -> wrist 1): onto the arm frame, shoulder to wrist
for f, id_, name, colour in (('tattoo_sleeve_waves_mountains.png', 'sleeve_waves', 'Waves and mountains sleeve', False),
                             ('tattoo_sleeve_waves_mountains_colour.png', 'sleeve_waves_colour', 'Waves and mountains sleeve, colour', True)):
    src = Image.open(os.path.join(HERE, f)).convert('RGBA')
    c = canvas(1024, 2048)
    c.paste(src.resize((1024, int(2048 * WRIST)), Image.LANCZOS), (0, 0))
    save(c, id_, name, 'arm', colour)

# --- the compass on the outer forearm, halfway between the elbow and the wrist, 10 cm across
comp = Image.open(os.path.join(HERE, 'tattoo_compass_rose.png')).convert('RGBA')
c = canvas(1024, 2048)
fore_v = (spots['arm_l_marks']['elbow'] + WRIST) / 2
circ_cm = 25.0                                                    # (round the forearm)
w = int(1024 * 10.0 / circ_cm); h = int(2048 * 10.0 / ARM_CM)
c.paste(comp.resize((w, h), Image.LANCZOS), (512 - w // 2, int(2048 * fore_v) - h // 2))
save(c, 'compass_forearm', 'Compass rose (forearm)', 'arm', False)

# --- the aurora over mountains as a neck patch
aur = Image.open(os.path.join(HERE, 'tattoo_aurora_mountains.png')).convert('RGBA')
c = canvas(1024, 1024); s = 860
c.paste(aur.resize((s, s), Image.LANCZOS), ((1024 - s) // 2, (1024 - s) // 2))
a = np.asarray(c).astype(float) / 255
a[..., 3] = 1 - (1 - np.clip(a[..., 3], 0, 1)) ** 3.0              # (colour ink reads on skin only when it is full)
g = a[..., :3].mean(-1, keepdims=True); a[..., :3] = np.clip((g + (a[..., :3] - g) * 1.5) * 0.72, 0, 1)
save(Image.fromarray((a * 255).astype(np.uint8), 'RGBA'), 'aurora_neck', 'Aurora over mountains', 'neck', True)

# --- the first timed batch (designs_batch1.py)
import designs_batch1
for id_, name, kind, colour, fn in designs_batch1.BATCH:
    save(fn(), id_, name, kind, colour)
import designs_batch2                                               # (the second batch, OK'd by the owner 2026-11-15)
for id_, name, kind, colour, fn in designs_batch2.BATCH:
    save(fn(), id_, name, kind, colour)
import designs_batch3                                               # (the third batch, OK'd 2026-11-15)
for id_, name, kind, colour, fn in designs_batch3.BATCH:
    save(fn(), id_, name, kind, colour)

Image.new('RGBA', (4, 4), (0, 0, 0, 0)).save(os.path.join(OUT, 'T_TatDesign_None.png'))
json.dump({'_': 'Tattoo designs (Art/Tattoos/make_designs.py). kind = the places it fits: arm (left or right), leg, neck (left, right, back, throat), fingers. One design can be on several places.',
           'places': {'arm': ['arm_l', 'arm_r'], 'leg': ['leg_l', 'leg_r'], 'neck': ['neck_l', 'neck_r', 'neck_back', 'neck_throat'], 'fingers': ['fingers_l', 'fingers_r']},
           'head_places': ['neck_l', 'neck_r', 'neck_back', 'neck_throat'],
           'designs': designs}, open(DATA, 'w'), indent=1)
print('designs:', [d['id'] for d in designs])
