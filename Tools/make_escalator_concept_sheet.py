# The new escalator's concept sheet for the owner (2026-11-18): the artist renderings (Art/Escalator/make_escalator_concept.py)
# with the notes, black background (his rule). Out: docs/Blueprints/EscalatorNew/Escalator-New-Concept.png
import os
from PIL import Image, ImageDraw, ImageFont
D = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'docs', 'Blueprints', 'EscalatorNew')
W, PW, PH, M = 3400, 1600, 900, 60
def font(n, bold=False):
    for f in (('segoeuib.ttf' if bold else 'segoeui.ttf'), 'arial.ttf'):
        try: return ImageFont.truetype(f, n)
        except OSError: pass
    return ImageFont.load_default()
views = [('1-front', 'From the front (as your photo)'), ('2-side', 'From the side - same 6.0 m height, flush with both floors'),
         ('3-newel-bottom', 'Ground floor end: the round newels, handrail into its inlet'), ('4-newel-top', 'Upper floor end: the same round newels, on top')]
notes = ['THE NEW ESCALATOR - concept (artist rendering), for your OK before the detailed model',
         '1  A pair side by side with a flat stainless centre deck and its anti-slide studs (your top-down photo).',
         '2  Clear glass balustrades in a slim stainless clamp strip along the decking; the glass sits inside the handrail all the way round.',
         '3  The black handrail runs as a real loop: along the top, round a smooth round newel at BOTH ends (ground floor and upper floor),',
         '    back underneath and into a black rubber inlet on the decking - at the top exactly as at the bottom.',
         '4  Steps: grooved aluminium treads and cleated risers with yellow edges (front and sides), textured from your step photo; max 5 % reflective.',
         '5  Metal (skirts, decking, clamps, cladding, floor plates): brushed stainless, 25 % reflective; ribbed floor plates, yellow comb plates.',
         '6  Small parts in the detailed model: bolts and screws on plates and clamps, emergency stop and key switch at each end, skirt brushes,',
         '    the inlets\' rubber lips, LED lines under the combs, direction lights; other textures from free (CC0) online sources.',
         '7  Same height and slope as today (6.0 m, 30 degrees, 1 m steps), sitting flush in the ground floor and in the Floor 2 floor.',
         '8  The steps keep moving as now; the handrails run with them.',
         'Your OK and I build it in full detail, then 8K PNG renders of it inside the terminal.']
H = M * 2 + 70 + 2 * (PH + 70) + 60 + len(notes) * 52
im = Image.new('RGB', (W, H), (0, 0, 0)); d = ImageDraw.Draw(im)
d.text((M, M), 'Airport Dynasty Pro - the new escalator (concept)', font=font(52, True), fill=(120, 230, 170))
for i, (f, cap) in enumerate(views):
    x = M + (i % 2) * (PW + 80); y = M + 90 + (i // 2) * (PH + 80)
    im.paste(Image.open(os.path.join(D, f + '.png')).convert('RGB').resize((PW, PH), Image.LANCZOS), (x, y))
    d.text((x, y + PH + 10), cap, font=font(34), fill=(220, 222, 228))
y = M + 90 + 2 * (PH + 80) + 20
for i, t in enumerate(notes):
    d.text((M, y + i * 52), t, font=font(36, i == 0), fill=(120, 230, 170) if i in (0, len(notes) - 1) else (228, 230, 236))
out = os.path.join(D, 'Escalator-New-Concept.png'); im.save(out, optimize=True); print(out, im.size)
