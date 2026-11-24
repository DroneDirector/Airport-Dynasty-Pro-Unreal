# The ambulift's sheet.  blender -b --python Art/Ambulift/make_ambulift.py -- --nochk --views   (poses drawn with the model's own pose())   then   python Tools/make_ambulift_blueprint.py
import os, json, textwrap
R = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); TMP = os.path.join(os.environ['TEMP'], 'ambulift_bp'); OUT = os.path.join(R, 'docs', 'Blueprints', 'Apron')
from PIL import Image, ImageDraw, ImageFont; Image.init()
W, H = 3400, 2700; S = Image.new('RGB', (W, H), (8, 10, 14)); d = ImageDraw.Draw(S)
def F(n, b=False): return ImageFont.truetype('C:/Windows/Fonts/' + ('arialbd.ttf' if b else 'arial.ttf'), n)
FG, DIM, LIN = (228, 234, 240), (150, 165, 180), (90, 190, 255); P = json.load(open(os.path.join(R, 'Art', 'Ambulift', 'ambulift-parts.json'))); sz = P['size_m']
d.text((60, 40), 'AMBULIFT  -  lift vehicle for passengers with reduced mobility', font=F(64, True), fill=FG); d.text((60, 122), 'To-do 212, part A. Our own design; sizes of the class. Metres. %.2f long x %.2f over the mirrors x %.2f high (stowed). %d parts, %d triangles.' % (*sz, len(P['parts']), P['tris']), font=F(32), fill=DIM)
def view(n, x, y, w, h, t):
    im = Image.open(os.path.join(TMP, n + '.png')).convert('RGB').resize((w, h)); S.paste(im, (x, y)); d.rectangle((x, y, x + w, y + h), outline=(60, 70, 84), width=2); d.text((x + 14, y + 10), t, font=F(30, True), fill=FG)
# 100 px = 1 m on the first row: side 11 x 4.6 m, front and rear 4 x 4.6 m
view('side', 60, 190, 1650, 690, 'Left side, stowed'); view('front', 1740, 190, 600, 690, 'Front'); view('rear', 2370, 190, 600, 690, 'Rear'); view('top', 60, 910, 1650, 600, 'Top')
view('up', 60, 1540, 1650, 1000, 'Sill 3.49 m (A320 L2): legs down, platform, bridge and gates out'); view('tail', 1740, 910, 1230, 600, 'Tail lift at the ground, ramp flap down')
PX = 150; x0 = 60 + 825; zg = 190 + 690 // 2 + int(2.1 * PX)
def X(x): return int(x0 - x * PX)
def dim(xa, xb, y, t): d.line((xa, y, xb, y), fill=LIN, width=3); [d.line((q, y - 10, q, y + 10), fill=LIN, width=3) for q in (xa, xb)]; d.text(((xa + xb) // 2 - d.textlength(t, F(26)) // 2, y - 34), t, font=F(26), fill=LIN)
lo, hi = P['x_m'][0], P['x_m'][0] + sz[0]
dim(X(hi), X(lo), 866, '%.2f' % sz[0]); dim(X(2.30), X(-2.20), 800, 'wheelbase 4.50'); dim(X(2.56), X(-4.0), 262, 'cabin 6.56'); dim(X(hi), X(2.82), 310, 'cab %.2f' % (hi - 2.82))
def vdim(x, za, zb, t): ya, yb = zg - int(za * PX), zg - int(zb * PX); d.line((x, ya, x, yb), fill=LIN, width=3); [d.line((x - 10, q, x + 10, q), fill=LIN, width=3) for q in (ya, yb)]; d.text((x + 12, (ya + yb) // 2 - 14), t, font=F(26), fill=LIN)
vdim(90, 0, 3.42, '3.42'); vdim(1620, 0, 1.28, 'floor 1.28'); vdim(250, 0, 2.135, 'cab 2.14')
N = [('Lift', 'One scissor X a side, L 4.60 m, two rams. Floor 1.28 down, 4.20 up (stroke 2.92 m); t = asin((0.12 + q) / 4.60). Four legs, 0.30 m. A320: 3.49 (L2) and 3.87 (L1), real 3.4: all inside the range.'),
     ('Front platform', '2.00 x 1.30 m, hinged at the sill, stands against the front wall for driving; folds down only from q 1.02 m (sill 2.30), over the 2.14 m cab. Rails fold (left up first), sliding bridge 0.50, sliding gates 0.40, rubber bumper. No hood (the types seen have none).'),
     ('Tail lift', 'Two arms 0.90 m, platform 1.42 x 1.44 m, side rails, ramp flap (stop up 90 deg, ramp -12 deg); floor to ground 1.18 m.'),
     ('Cabin', '6.46 x 2.30 x 2.05 m inside. 5 belted seats (left), stretcher on its own trolley (right), 4 wheelchair bays 1.20 x 0.80: two rails, four straps, shoulder belt each. Handrails, heater, first aid, extinguisher, hammers, ten ceiling lamps, sliding doors front and rear 1.10 x 1.96 m.'),
     ('Lights', 'Blue LED bars front and rear (2 x 8 reflector units each), two blue units in the face, amber beacon; low, high, indicators in real reflectors behind a cover; tail, brake, indicator, reverse; markers; four work lamps.'),
     ('Controls', 'Each a part with its pivot: cab switches, horn, gear lever, parking brake; legs panel outside left; lift / platform / bridge levers and buttons in front left in the cabin; tail buttons inside rear right and outside right; door buttons.'),
     ('Checked', 'Every pose by mesh overlap: lift 0 - 2.92, platform 0 - 90, rails, gates, bridge, doors, tail lift, flap, legs, cab doors, steering 38 deg. No part through another; flicker check 0.'),
     ('Photos looked at', 'Wikimedia Commons: Ambulift Arlanda; Ambulift Heathrow; PRM Highloader Stockholm; Ambulift-prm-boarding; Ambulift Lisbon; Ambulift with lowered cabin; Medical Highloader at LIS airport. Shapes only; nothing copied.'),
     ('Wheelchair', 'Art/Wheelchair: 1.00 x 0.65 x 0.93 m, own parts for wheels, castors, forks, footrests, plates, brakes, belt.')]
x, y = 1740, 1560
for h, t in N:
    d.text((x, y), h, font=F(30, True), fill=LIN); y += 36
    for l in textwrap.wrap(t, 116): d.text((x, y), l, font=F(27), fill=FG); y += 32
    y += 8
assert y < H, y
S.save(os.path.join(OUT, 'Ambulift.png')); S.save(os.path.join(OUT, 'Ambulift.pdf'), resolution=200); print('sheet', y)
