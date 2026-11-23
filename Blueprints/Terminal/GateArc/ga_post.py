# Gate arc: the finished renders (F:/ADP-Export/ga_raw) into docs/Blueprints/Gate-Arc, each with one caption line; the top
# view with the owner's red arc over it, the before / after pairs.
from ga_common import *
from PIL import Image, ImageDraw, ImageFont
RAW = os.environ.get('GA_OUT') or 'F:/ADP-Export/ga_raw'
FT = lambda s, b=False: ImageFont.truetype('C:/Windows/Fonts/%s.ttf' % ('segoeuib' if b else 'segoeui'), s)


def cap(im, title, sub):
    d = ImageDraw.Draw(im, 'RGBA'); W_, H_ = im.size; d.rectangle((0, H_ - 104, W_, H_), fill=(5, 8, 12, 225))
    d.text((48, H_ - 88), title, font=FT(48, True), fill=(255, 209, 102)); x = 48 + d.textlength(title, font=FT(48, True)) + 46
    d.text((x, H_ - 79), sub, font=FT(38), fill=(240, 245, 250)); return im


def one(src, dst, title, sub):
    p = os.path.join(RAW, src + '.png')
    if os.path.exists(p): cap(Image.open(p).convert('RGB'), title, sub).save(os.path.join(OUT, dst)); print(dst)


def pair(a, b, dst, y0, title):
    pa, pb = os.path.join(RAW, a + '.png'), os.path.join(RAW, b + '.png')
    if not (os.path.exists(pa) and os.path.exists(pb)): return
    S = Image.new('RGB', (3840, 2160), (5, 8, 12))
    for k, (p, t, c) in enumerate(((pa, 'BEFORE  (today, flat)', (240, 245, 250)), (pb, 'AFTER  (curve, 3.0 m)', (255, 209, 102)))):
        S.paste(Image.open(p).convert('RGB').crop((0, y0, 3840, y0 + 1074)), (0, k * 1086)); d = ImageDraw.Draw(S, 'RGBA')
        w = d.textlength(t, font=FT(54, True)); d.rectangle((36, k * 1086 + 30, 36 + w + 48, k * 1086 + 118), fill=(5, 8, 12, 215)); d.text((60, k * 1086 + 36), t, font=FT(54, True), fill=c)
    S.save(os.path.join(OUT, dst)); print(dst)


one('apron', 'GA-3-Render-Apron.png', 'Gate 9 from the apron', 'standing eye height, 1.65 m · the curve 3.0 m deep · jetway J-9 docked to the A320, not moved')
one('close', 'GA-3b-Render-Apron-Close.png', 'Gate 9, closer', 'standing eye height · the floor and roof edges follow the same circle, 5.40 m clear below')
one('raised', 'GA-3c-Render-Apron-Raised.png', 'Gate 9, raised view', '15 m up · the neighbouring view bay stays flat; gate 11 behind is drawn as it is today')
one('front', 'GA-3d-Render-Apron-High.png', 'Gate 9, from above the stand', '20 m up · the corridor passes through the curved glass')
one('inside', 'GA-4-Render-Inside.png', 'Inside gate 9', 'two sections of 12 seats, a clear way to the glass between them, 1.55 m free in front of the knees')
one('inside2', 'GA-4b-Render-Inside-Entrance.png', 'Inside gate 9, the jetway entrance', 'the door exactly where it is today; the curved glass runs past it into a steel collar')
one('aisle', 'GA-4c-Render-Inside-Gap.png', 'In the gap', 'standing behind it: 2.42 m clear all the way to the glass, nothing in it')
one('through', 'GA-4d-Render-Inside-Option.png', 'OPTION: the aisle through the rows too', 'from the walkway door straight to the glass; every row keeps its 37 seats')
pair('apron_before', 'apron', 'GA-6-Before-After-Apron.png', 330, '')
pair('close_before', 'close', 'GA-6b-Before-After-Close.png', 190, '')
pair('front_before', 'front', 'GA-6c-Before-After-High.png', 420, '')
p = os.path.join(RAW, 'top.png')
if os.path.exists(p):                                                # ortho 135 m wide, centre (-215, 98)
    im = np.array(Image.open(p).convert('RGB')); fig = plt.figure(figsize=(W, H), dpi=DPI); fig.patch.set_facecolor(BG); ax = fig.add_axes([0, 0, 1, 1]); hw, hh = 67.5, 67.5 * 9 / 16
    ax.imshow(im, extent=(-215 - hw, -215 + hw, 98 - hh, 98 + hh), interpolation='none'); ax.axis('off'); ax.set_xlim(-215 - hw, -215 + hw); ax.set_ylim(98 - hh, 98 + hh)
    SK = np.load(os.path.join(HERE, 'sketch_arc.npy')); ax.plot(SK[:, 0], SK[:, 1], color='#ff2233', lw=13, alpha=0.38, solid_capstyle='round', zorder=5)
    arcline(ax, A, '#ffd166', 3.5, off=0.09, zorder=8); ax.plot([X0, XR], [82, 82], color='#1d2a36', lw=2, ls=(0, (6, 4)), zorder=7)
    kw = dict(fontsize=20, zorder=20, ha='center', va='center', bbox=dict(fc='#05080c', ec='none', pad=5, alpha=0.82))
    ax.annotate('your sketch (about 4.3 m)', (-226.6, 85.5), (-238.5, 104), color='#ff6b78', arrowprops=dict(arrowstyle='-|>', color='#ff6b78', lw=2), **kw)
    ax.annotate('the true arc, 3.0 m deep', (-219, 85.0), (-222, 75.5), color='#ffd166', arrowprops=dict(arrowstyle='-|>', color='#ffd166', lw=2), **kw)
    ax.annotate("today's flat front", (-203.2, 82.0), (-196, 92), color='#f0f5fa', arrowprops=dict(arrowstyle='-|>', color='#f0f5fa', lw=2), **kw)
    for x, z, s in ((-222, 70.5, 'GATE 9'), (-182.7, 74, 'VIEW BAY 9'), (-238.4, 85.2, 'tug box'), (-205.6, 100, 'J-9')): ax.text(x, z, s, color='#f0f5fa', **kw)
    ax.text(-215 - hw + 1, 98 + hh - 1.4, 'Gate 9 from above  ·  like your sketch, its red arc over it', color='#ffd166', fontsize=24, weight='bold', va='top', zorder=20, bbox=dict(fc='#05080c', ec='none', pad=6, alpha=0.85))
    check(fig, 'GA-5'); fig.savefig(os.path.join(OUT, 'GA-5-Render-Top.png'), dpi=DPI, facecolor=BG); plt.close(fig); print('GA-5')
