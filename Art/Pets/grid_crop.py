# Enlarged, gridded crop of a reference picture for hand-tracing parts (ears) - labels are the picture's own pixels.
#   python Art/Pets/grid_crop.py <picture> <out.jpg> x0 y0 x1 y1 scale step label_every [brighten]
import sys, cv2
p, out = sys.argv[1], sys.argv[2]; x0, y0, x1, y1, s, step, lab = (int(v) for v in sys.argv[3:10])
br = float(sys.argv[10]) if len(sys.argv) > 10 else 1.0
img = cv2.imread(p)
c = cv2.resize(img[y0:y1, x0:x1], None, fx=s, fy=s, interpolation=cv2.INTER_CUBIC)
if br != 1.0: c = cv2.convertScaleAbs(c, alpha=br, beta=10)
for x in range(x0 - x0 % step + step, x1, step):
    cv2.line(c, ((x - x0) * s, 0), ((x - x0) * s, c.shape[0]), (0, 200, 255) if x % lab == 0 else (120, 120, 120), 1)
    if x % lab == 0: cv2.putText(c, str(x), ((x - x0) * s + 2, 18), 0, 0.55, (0, 0, 255), 2)
for y in range(y0 - y0 % step + step, y1, step):
    cv2.line(c, (0, (y - y0) * s), (c.shape[1], (y - y0) * s), (0, 200, 255) if y % lab == 0 else (120, 120, 120), 1)
    if y % lab == 0: cv2.putText(c, str(y), (2, (y - y0) * s - 3), 0, 0.55, (0, 0, 255), 2)
cv2.imwrite(out, c)
