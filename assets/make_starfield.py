#!/usr/bin/env python3
"""
Procedural Milky Way equirectangular starfield generator.
Generates an artifact-free 4096x2048 equirectangular starfield complying with
the Obsidian & Ember palette (spec §4).
"""
import argparse, math, os, sys
import numpy as np
from PIL import Image

W, H = 4096, 2048

def smooth_noise(W, H, scale, seed=1234):
    rng = np.random.default_rng(seed)
    sw, sh = max(4, W // scale), max(4, H // scale)
    grid = rng.uniform(0.0, 1.0, (sh, sw)).astype(np.float32)
    # Resize with smooth bicubic interpolation to eliminate block artifacts
    img = Image.fromarray((grid * 255.0).astype(np.uint8))
    smooth = img.resize((W, H), Image.BICUBIC)
    return np.array(smooth, dtype=np.float32) / 255.0

def fbm(W, H, octaves=5, seed=1234):
    total = np.zeros((H, W), dtype=np.float32)
    amp = 1.0
    scale = 256
    for i in range(octaves):
        n = smooth_noise(W, H, scale, seed + i * 17)
        total += n * amp
        amp *= 0.5
        scale = max(4, scale // 2)
    return total / total.max()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "starfield.png"))
    args = ap.parse_args()

    print(f"[starfield] generating smooth {W}x{H} panorama with Obsidian & Ember palette...")

    v = np.linspace(0, 1, H, dtype=np.float32)[:, None]
    u = np.linspace(0, 1, W, dtype=np.float32)[None, :]
    lat = (v - 0.5) * 2.0  # -1..1

    # Milky Way band with sinusoidal galactic tilt
    tilt = np.sin(u * 2.0 * math.pi + 0.4) * 0.08
    band = np.exp(-(((lat + tilt) * 3.8) ** 2)) * 0.65

    # Smooth dust lanes (FBM noise)
    dust = fbm(W, H, octaves=4, seed=42)
    band *= np.clip(0.70 - dust * 0.45, 0.0, 1.0)

    # Galactic core glow
    core = np.exp(-((u - 0.62) ** 2) * 45.0) * np.exp(-(lat ** 2) * 15.0)

    # Palette tokens (spec §4):
    # Deep Void: #0a0807 (10, 8, 7)
    # Warm Ash:  #c4b5a5 (196, 181, 165)
    # Dried Clay:#a86b4a (168, 107, 74)
    # Molten Iron core: #e87a3a (232, 122, 58)
    r = (10.0 / 255.0) + band * (168.0 / 255.0) * 0.35 + core * (232.0 / 255.0) * 0.70
    g = ( 8.0 / 255.0) + band * (107.0 / 255.0) * 0.35 + core * (122.0 / 255.0) * 0.70
    b = ( 7.0 / 255.0) + band * ( 74.0 / 255.0) * 0.35 + core * ( 58.0 / 255.0) * 0.70

    # Stars
    rng = np.random.default_rng(2026)
    star_field = np.zeros((H, W), dtype=np.float32)
    n_stars = 70_000
    sx = rng.integers(0, W, n_stars)
    sy = rng.integers(0, H, n_stars)
    band_w = np.exp(-(((sy / H - 0.5) * 2 + np.sin(sx / W * 2 * math.pi + 0.4) * 0.08) * 3.8) ** 2)
    keep = rng.random(n_stars) < (0.35 + 0.65 * band_w)
    sx, sy = sx[keep], sy[keep]
    mags = rng.power(0.35, sx.shape[0]).astype(np.float32)
    star_field[sy, sx] = np.maximum(star_field[sy, sx], mags)

    # 400 prominent major stars with smooth anti-aliased Gaussian footprints
    n_bright = 400
    bx = rng.integers(0, W, n_bright)
    by = rng.integers(0, H, n_bright)
    for x_, y_, m in zip(bx, by, rng.uniform(1.2, 2.8, n_bright)):
        y0, y1 = max(0, y_-3), min(H, y_+4)
        x0, x1 = max(0, x_-3), min(W, x_+4)
        yy, xx = np.mgrid[y0:y1, x0:x1]
        d2 = (yy - y_) ** 2 + (xx - x_) ** 2
        star_field[y0:y1, x0:x1] = np.maximum(star_field[y0:y1, x0:x1], m * np.exp(-d2 * 0.75))

    # Add stars with Warm Ash / subtle stellar tint
    r += star_field * (196.0 / 255.0)
    g += star_field * (181.0 / 255.0)
    b += star_field * (165.0 / 255.0)

    img = np.stack([r, g, b], axis=-1)
    img = np.clip(img, 0.0, 1.0)
    out_img = Image.fromarray((img * 255.0).astype(np.uint8))
    out_img.save(args.out, quality=95)
    print(f"[starfield] saved smooth starfield to {args.out}")

if __name__ == "__main__":
    main()
