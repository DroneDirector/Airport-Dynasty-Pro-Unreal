# Paint gloss maps for the house livery (owner 2026-11-28: "give it some reflection like a real fuselage").
# For each livery picture (ADP-Fuse-Main.png, ADP-engine-CFM56.png) a glTF metal-roughness map of the same size:
#   R = occlusion 1, G = roughness, B = metallic   (linear; Tools/make_a320_livery.py imports it as a mask, no sRGB)
# Real airliner paint: base roughness 0.30 (with a fine +-0.015 grain), non-metal for the white and the navy; the gold
# is a metallic paint, metallic 0.40. The clear coat over all of it (factor 1, roughness 0.08) is set in the material.
# The gold is found from the livery's own colours (hue 28-50 deg, saturation 0.30-0.80) and only where the livery
# differs from the plain white base (Art/A320/Textures/FG), so the base's own yellow/orange placards stay non-metal.
#   python Art/A320/Livery/make_livery_mr.py
import os
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = os.path.join(HERE, '..', 'Textures', 'FG')
PAIRS = (('ADP-Fuse-Main.png', 'Fuse-Main.png'), ('ADP-engine-CFM56.png', 'engine-CFM56.png'))
ROUGH, GOLD_ROUGH, GOLD_METAL, GRAIN = 0.30, 0.28, 0.40, 0.015


def gold_mask(liv, base):
    mx = liv.max(-1); mn = liv.min(-1); c = mx - mn
    r, g, b = liv[..., 0], liv[..., 1], liv[..., 2]
    hue = np.where(c > 1e-4, 60.0 * (g - b) / np.maximum(c, 1e-4), 0.0)   # valid where r is the max
    sat = np.where(mx > 1e-4, c / np.maximum(mx, 1e-4), 0.0)
    m = (r >= g) & (g >= b) & (hue > 28) & (hue < 50) & (sat > 0.30) & (sat < 0.80) & (mx > 0.15)
    m &= np.abs(liv - base).max(-1) > 0.04
    # soften the edge a pixel (box 3x3) so the metal does not alias at the gold line
    f = m.astype(np.float32); p = np.pad(f, 1, mode='edge')
    return sum(p[i:i + f.shape[0], j:j + f.shape[1]] for i in range(3) for j in range(3)) / 9.0


def main():
    rng = np.random.default_rng(320)
    for liv_name, base_name in PAIRS:
        liv = np.asarray(Image.open(os.path.join(HERE, liv_name)).convert('RGB'), dtype=np.float32) / 255.0
        base = Image.open(os.path.join(BASE, base_name)).convert('RGB')
        if base.size != liv.shape[1::-1]:
            base = base.resize(liv.shape[1::-1], Image.BILINEAR)
        base = np.asarray(base, dtype=np.float32) / 255.0
        g = gold_mask(liv, base)
        rough = ROUGH + (GOLD_ROUGH - ROUGH) * g + rng.uniform(-GRAIN, GRAIN, g.shape).astype(np.float32)
        metal = GOLD_METAL * g
        out = np.stack([np.ones_like(g), np.clip(rough, 0, 1), np.clip(metal, 0, 1)], -1)
        name = liv_name.replace('.png', '-MR.png')
        Image.fromarray((out * 255 + 0.5).astype(np.uint8)).save(os.path.join(HERE, name))
        print('%s: %dx%d, gold %.2f%% of the texels' % (name, g.shape[1], g.shape[0], 100.0 * (g > 0.5).mean()))


if __name__ == '__main__':
    main()
