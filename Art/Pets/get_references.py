# Reference photos for the pets (owner, 2026-11-15): breed photos from Wikimedia Commons, ONLY licences that allow use
# in a sold game (public domain, CC0, CC BY - the latter credited in the game's credits); never CC BY-SA / NC / unknown.
# Writes Art/Pets/Reference/<pet>/<file> and Art/Pets/Reference/credits.json (title, author, licence, source page).
#   python Art/Pets/get_references.py
import json, os, re, subprocess, sys, time, urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.join(HERE, 'Reference')
UA = 'ADP-reference-fetch/1.0 (game art reference)'
OK = ('public domain', 'cc0', 'cc by 2.0', 'cc by 3.0', 'cc by 4.0', 'cc by 2.5', 'pdm')
PICK = {
    'dog_labrador': ['Labrador Retriever yellow portrait 0248.jpg', '3064-labrador-standing (20505778121).jpg',
                     'Perfect Side View Of Black Labrador North East England.JPG', 'Labrador Retriever Nessie.JPG',
                     'Awesome Black Lab - 7820522182.jpg', 'Labr.jpg', 'Retriever Du Labrador.jpg',
                     'Chocolate Labrador Boomer 2007.jpg', 'Labrador Retriever Hershey chocolate.jpg',
                     'Labrador Retriever black ball.jpg', 'USMC-051028-M-0245S-011.jpg', 'Black lab dog sephia.jpg',
                     # (front, three-quarter and sitting views, for the 3D shape all round)
                     '021 labrador retriever.JPG', 'Afra the Labrador Retriever (mugshot).jpg', 'Labrador Retriever - Yellow.JPG',
                     'Labrador Retriever snow.jpg', 'Yellow Lab.JPG', 'Yellow dog in grass.jpg',
                     '3062-labrador-sitting-dog-show (20499435755).jpg', 'Buster the black Labrador Retriever (2007).jpg',
                     'Labrador-retriever.jpg', 'Senior Labrador (51085818202).jpg', 'Labrador Retriever chocolate Hershey sit.jpg',
                     'Yellow labrador retriever sitting in the back of a car.jpg'],
    'cat_british_shorthair': ['British Shorthair Cat (4159254405).jpg', 'British Shorthair Cat (4160001954).jpg',
                              'British Shorthair Cat (4160004506).jpg', 'British Shorthair Cat (4160008702).jpg',
                              'British Shorthair Cat (4160021588).jpg', 'British shorthair cat-3113513.jpg',
                              'British Shorthair Cat 2.jpg', 'British shorthair house cat Mio 5 months old.jpg',
                              # (standing on white, faces, sleeping, and a seen-from-behind study for the back)
                              'British Shorthair Fond Blanc.jpg', 'Ansicht Britisch Kurzhaar 20220114 HOF08558 20230123000983.png',
                              'Ansicht Britisch Kurzhaar 20220114 HOF08559 20230123000984.png', 'Katzengesicht BKH 20201107 DSC5735.jpg',
                              'Schlafende Katze 20201024 DSC4879.jpg', 'Schlafende Katze 20201024 DSC4881.jpg',
                              'A Cat Resting on All Fours, Seen from Behind MET DP807592.jpg'],
    # the jaw, teeth and tongue for panting (owner, 2026-11-15: "see its teeth and its tongue"; "the jaw very accurate"):
    # panting dogs from the side and front, and the dog's skull and teeth for where the jaw hinges and the teeth stand
    'dog_mouth': ['Panting-dog-No.3.jpg', 'Dog Panting on a Hot Day 2013.JPG', 'Keeshond dog panting (8109968656).jpg',
                  'Panting (9131401632).jpg', 'Panting brown dog.jpg', 'Dog with visible tongue.jpg',
                  'DSC09611a Search and Rescue Dog, Austrian Red Cross Perchtoldsdorf, 2024-10.jpg',
                  'Skull of a dog.png', 'Dog anatomy head.jpg', 'Heubach dog skull.jpg', 'Anatomy and physiology of animals Dogs skull.jpg',
                  'EB1911 Mammalia - Milk and Permanent Dentitions of the Dog.jpg', 'Animaldentition canisfamiliaris.png',
                  'Skull of Japanese Dog - Keulemans.png'],
    # (rear views of a Labrador are not found under a usable licence: a foxhound's rear (same build of hind legs and
    # tail set) and study drawings stand in for the shape from behind)
    'dog_rear': ['Dog (A Foxhound Viewed from Behind) MET DP835554.jpg',
                 'Two Studies of a Dog- Lying Down and Rear View Standing (from Sketchbook) MET 246966.jpg'],
}


def api(params):
    q = urllib.parse.urlencode(dict(params, format='json'))
    # (curl: this PC's Python has an out-of-date certificate store)
    for wait in (2, 5, 15, 30):      # (Commons answers nothing when asked too fast: wait and ask again)
        out = subprocess.run(['curl', '-s', '-A', UA, 'https://commons.wikimedia.org/w/api.php?' + q], capture_output=True).stdout
        try: return json.loads(out)
        except ValueError: time.sleep(wait)
    raise RuntimeError('Commons did not answer')


def strip(h): return re.sub('<[^>]+>', '', h or '').strip()


credits = {}
cp = os.path.join(REF, 'credits.json')
if os.path.exists(cp): credits = json.load(open(cp, encoding='utf-8'))
for pet, titles in PICK.items():
    os.makedirs(os.path.join(REF, pet), exist_ok=True)
    for t in titles:
        d = api({'action': 'query', 'titles': 'File:' + t, 'prop': 'imageinfo', 'iiprop': 'url|size|extmetadata', 'iiurlwidth': 3000})
        p = next(iter(d['query']['pages'].values()))
        if 'imageinfo' not in p: print('MISSING', t); continue
        ii = p['imageinfo'][0]; m = ii['extmetadata']
        lic = strip(m.get('LicenseShortName', {}).get('value'))
        if not any(lic.lower().startswith(k) for k in OK):
            print('SKIP (licence %s)' % lic, t); continue
        out = os.path.join(REF, pet, t.replace(' ', '_'))
        def good(f): return os.path.exists(f) and open(f, 'rb').read(3) in (bytes([255, 216, 255]), bytes([137, 80, 78]))
        for wait in (5, 20, 60, 120):     # (Commons sends an error page instead of the picture when asked too fast)
            if good(out): break
            src = ii.get('thumburl') if ii['width'] > 3000 else ii['url']
            subprocess.run(['curl', '-s', '-L', '-A', UA, '-o', out, src], check=True)
            if not good(out): time.sleep(wait)
        if not good(out):
            if os.path.exists(out): os.remove(out)
            print('FAILED', t); continue
        time.sleep(3.0)
        credits[pet + '/' + os.path.basename(out)] = {'title': t, 'author': strip(m.get('Artist', {}).get('value')), 'licence': lic,
                                                     'page': ii.get('descriptionurl'), 'size': [ii['width'], ii['height']]}
        print('OK', lic, ii['width'], 'x', ii['height'], t)
json.dump(credits, open(cp, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
