# Checks the dog model against the photo: its side silhouette (blue) over the photo with the traced outline (red),
# Art/Pets/Dog/Views/<tag>_side_vs_photo.png, plus how far apart they are (percent of the outline area that differs).
#   python Art/Pets/Dog/check_dog.py <tag>
import os, sys
import numpy as np, cv2
HERE = os.path.dirname(os.path.abspath(__file__)); tag = sys.argv[1] if len(sys.argv) > 1 else 'v'
K = 57.0 / (466 - 148)
b = open(os.path.join(HERE, 'dog_body.ply'), 'rb').read(); h = b[:b.index(b'end_header\n') + 11].decode()
nv = int(h.split('element vertex ')[1].split()[0]); nf = int(h.split('element face ')[1].split()[0]); off = len(h)
v = np.frombuffer(b, '<f4', nv * 3, off).reshape(-1, 3)
f = np.frombuffer(b, dtype=[('n', 'u1'), ('i', '<i4', (3,))], count=nf, offset=off + nv * 12)['i']
img = cv2.imread(os.path.join(HERE, '..', 'Reference', 'dog_labrador', 'Retriever_Du_Labrador.jpg')); s = 3
img = cv2.resize(img, None, fx=s, fy=s)
px = np.c_[500 - v[:, 0] / K, 466 - v[:, 2] / K] * s
sil = np.zeros(img.shape[:2], np.uint8); cv2.fillPoly(sil, list(np.round(px[f]).astype(np.int32)), 255)
ph = np.zeros_like(sil); o = np.load(os.path.join(HERE, 'dog_side2_outline.npy'))
cv2.fillPoly(ph, [np.round(o * s).astype(np.int32)], 255)
# (the tail is posed differently in the photo - left out of the number)
ph[:, int(490 * s):] = 0; sil2 = sil.copy(); sil2[:, int(490 * s):] = 0
diff = np.logical_xor(sil2 > 0, ph > 0).sum() / max(1, (ph > 0).sum())
cs, _ = cv2.findContours(sil, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
out = img.copy(); cv2.drawContours(out, cs, -1, (255, 120, 0), 2)
cv2.polylines(out, [np.round(o * s).astype(np.int32)], True, (0, 0, 255), 2)
os.makedirs(os.path.join(HERE, 'Views'), exist_ok=True)
cv2.imwrite(os.path.join(HERE, 'Views', tag + '_side_vs_photo.png'), out)
print('side outline differs by %.1f%% of the photo dog (tail left out)' % (diff * 100))
