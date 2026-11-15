# The outline of a pet in a reference photo (owner, 2026-11-15: "look at the photos in very, very high looks ... each
# shape, length, width and form of the entire body"): OpenCV's GrabCut separates the animal from the background
# (colour + edges, not a brightness cut - a shiny black coat stays whole), the edge is then smoothed only by the width of
# loose fur so the real shape stays, and drawn over the photo for checking (Art/Pets/<Pet>/<name>_outline_check.png).
#   python Art/Pets/trace_outline.py <photo> <out_name> x0 y0 x1 y1 [iterations | plain] [fg boxes x0,y0,x1,y1;...]
# fg boxes: spots that are surely the animal (the light top of a nose against a white background)
# 'bg': a plain studio background of any colour (the owner's German Shepherd sheet, cream): the animal is what differs
#   from the background colour (taken from the picture's border) - a grey or black coat stays whole
# 'plain': the photo is a cut-out on plain white - the animal is everything not white (GrabCut keeps a white halo there)
# '<iterations>:<hints>': hints name background colours that are never the animal (blue, white, grey, green), e.g. 8:blue,white
# (a brown dog in front of a blue sign: without the hint the cut-out took the sign as part of the head)
import os, sys
import numpy as np
import cv2
from scipy.ndimage import gaussian_filter1d

photo, name = sys.argv[1], sys.argv[2]
x0, y0, x1, y1 = (int(v) for v in sys.argv[3:7])
mode = sys.argv[7] if len(sys.argv) > 7 else '8'
img = cv2.imread(photo)
H, W = img.shape[:2]
if mode.startswith('bg'):
    border = np.r_[img[0], img[-1], img[:, 0], img[:, -1]].astype(float)
    bgc = np.median(border, 0); T = float(mode[2:] or 34)
    m = (np.linalg.norm(img.astype(float) - bgc, axis=2) > T).astype(np.uint8)
    # floor shadows (pale, colourless) are not the animal - only near the floor, so a grey saddle in the coat stays
    mx = img.max(2).astype(float); sat = (mx - img.min(2)) / np.maximum(mx, 1)
    floor = np.zeros_like(m, bool); floor[int(H * 0.86):] = True
    m[floor & (sat < 0.16) & (mx > 120)] = 0
    m[:y0], m[y1:], m[:, :x0], m[:, x1:] = 0, 0, 0, 0
    m = cv2.morphologyEx(m, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
elif mode == 'plain':
    m = (img.min(2) < 215).astype(np.uint8)
    m[:y0], m[y1:], m[:, :x0], m[:, x1:] = 0, 0, 0, 0
    m = cv2.morphologyEx(m, cv2.MORPH_OPEN, np.ones((3, 3), np.uint8))
else:
    its, _, hints = mode.partition(':')
    mask = np.zeros((H, W), np.uint8)
    bg, fg = np.zeros((1, 65), np.float64), np.zeros((1, 65), np.float64)
    if hints:
        mask[:] = cv2.GC_BGD; mask[y0:y1, x0:x1] = cv2.GC_PR_FGD
        b, g, r = (img[..., i].astype(int) for i in range(3))
        never = np.zeros((H, W), bool)
        if 'blue' in hints: never |= (b > r + 25) & (b > g + 5)
        if 'white' in hints:   # (pure white only: the pink-white top of a nose is not background)
            mx = np.maximum(img.max(2).astype(float), 1); never |= (img.min(2) > 205) & ((mx - img.min(2)) / mx < 0.08)
        if 'grey' in hints:   # (colourless for its brightness: a dark grey chair 48/43/34, never a brown coat even in shadow 31/16/9)
            mx = img.max(2).astype(float); never |= ((mx - img.min(2)) / np.maximum(mx, 1) < 0.35) & (mx > 25)
        # (only whole patches count as background, never single pixels inside the coat)
        n_, lab_, st_, _ = cv2.connectedComponentsWithStats(never.astype(np.uint8))
        never = np.isin(lab_, 1 + np.nonzero(st_[1:, cv2.CC_STAT_AREA] > H * W * 0.002)[0])
        if 'green' in hints: never |= (g > r + 15) & (g > b + 5)
        mask[never] = cv2.GC_BGD
        for bx in (sys.argv[8].split(';') if len(sys.argv) > 8 else []):
            a0, b0, a1, b1 = (int(v) for v in bx.split(',')); mask[b0:b1, a0:a1] = cv2.GC_FGD
        cv2.grabCut(img, mask, None, bg, fg, int(its), cv2.GC_INIT_WITH_MASK)
    else:
        cv2.grabCut(img, mask, (x0, y0, x1 - x0, y1 - y0), bg, fg, int(its), cv2.GC_INIT_WITH_RECT)
    m = np.where((mask == cv2.GC_FGD) | (mask == cv2.GC_PR_FGD), 1, 0).astype(np.uint8)
n, lab, st, _ = cv2.connectedComponentsWithStats(m)
m = (lab == 1 + np.argmax(st[1:, cv2.CC_STAT_AREA])).astype(np.uint8)
k = max(3, int(W / 120)) | 1                  # (small dents from shine on a wet nose or coat are closed; real shape stays)
m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (k, k)))
cs, _ = cv2.findContours(m, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
c = max(cs, key=len)[:, 0, :].astype(float)
fur = max(1.2, W / 500.0)                      # (smoothing: about one loose-fur width, scaled to the photo)
c = np.c_[gaussian_filter1d(c[:, 0], fur, mode='wrap'), gaussian_filter1d(c[:, 1], fur, mode='wrap')]
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), name.split('_')[0].capitalize())
os.makedirs(out, exist_ok=True)
np.save(os.path.join(out, name + '_outline.npy'), c)
sm = np.zeros((H, W), np.uint8); cv2.fillPoly(sm, [np.round(c).astype(np.int32)], 1)
np.save(os.path.join(out, name + '_mask.npy'), sm.astype(bool))
s = max(1, int(round(1600 / W)))
chk = cv2.resize(img, (W * s, H * s), interpolation=cv2.INTER_CUBIC)
cv2.polylines(chk, [np.round(c * s).astype(np.int32)], True, (0, 0, 255), max(2, s, W // 600), cv2.LINE_AA)
cv2.imwrite(os.path.join(out, name + '_outline_check.png'), chk)
print('outline %s: %d points, box %s' % (name, len(c), (c.min(0).round(), c.max(0).round())))
