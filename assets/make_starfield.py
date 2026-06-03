#!/usr/bin/env python3
"""
Procedural Milky Way equirectangular starfield generator.

Generates a 4096x2048 PNG that the renderer loads as `starfield.hdr`-equivalent.
No external assets, no attribution required — purely synthesized.

Usage:
    python3 make_starfield.py            # writes ./starfield.png
    python3 make_starfield.py --hdr      # also writes starfield.hdr (Radiance)

Then in the renderer assets/ directory either keep starfield.png, or rename it
to starfield.hdr so the C++ loader (stb_image) picks it up by name.
"""
import argparse, math, os, struct, sys
import numpy as np
from PIL import Image

W, H = 4096, 2048

def fbm(x, y, octaves=5, seed=1234):
    rng = np.random.default_rng(seed)
    total = np.zeros_like(x, dtype=np.float32)
    amp = 1.0
    freq = 1.0
    for _ in range(octaves):
        # cheap value noise: hash on rounded coords
        xi = (x * freq).astype(np.int32)
        yi = (y * freq).astype(np.int32)
        h = (xi * 374761393 + yi * 668265263) ^ rng.integers(0, 1 << 31)
        h = (h ^ (h >> 13)) * 1274126177
        n = ((h & 0xFFFFFF) / float(0xFFFFFF)).astype(np.float32)
        total += n * amp
        amp *= 0.5
        freq *= 2.0
    return total / total.max()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hdr", action="store_true")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "starfield.png"))
    args = ap.parse_args()

    print(f"[starfield] generating {W}x{H} …")

    # Coord grid.
    u = np.linspace(0, 1, W, dtype=np.float32)
    v = np.linspace(0, 1, H, dtype=np.float32)
    U, V = np.meshgrid(u, v)
    # Spherical
    theta = V * math.pi
    phi   = U * 2.0 * math.pi
    z = np.sin(theta) * np.cos(phi)   # noqa
    y = np.cos(theta)
    # latitude band for the Milky Way
    lat = (V - 0.5) * 2.0  # -1..1

    # --- Milky Way band ----------------------------------------------------
    band = np.exp(-((lat * 4.5) ** 2)) * 0.55
    # Tilt the band ~25° by warping phase along U
    tilt = np.sin(U * 2 * math.pi + 0.4) * 0.06
    band = np.exp(-(((lat + tilt) * 4.5) ** 2)) * 0.55

    # Dust lanes (subtract noise inside the band).
    dust = fbm(U * 8, V * 4, octaves=6, seed=42)
    dust = (dust - 0.5) * 1.4
    band *= np.clip(0.55 - dust * 0.6, 0.0, 1.0)

    # Warm/cool tint per column for galactic core vs arms.
    core = np.exp(-((U - 0.62) ** 2) * 60.0) * np.exp(-(lat ** 2) * 18.0)

    # --- Stars -------------------------------------------------------------
    rng = np.random.default_rng(7)
    star_field = np.zeros((H, W), dtype=np.float32)
    n_stars = 90_000
    sx = rng.integers(0, W, n_stars)
    sy = rng.integers(0, H, n_stars)
    # Concentrate stars near the band.
    band_w = np.exp(-(((sy / H - 0.5) * 2 + np.sin(sx / W * 2 * math.pi + 0.4) * 0.06) * 4.5) ** 2)
    keep = rng.random(n_stars) < (0.3 + 0.7 * band_w)
    sx, sy = sx[keep], sy[keep]
    mags = rng.power(0.35, sx.shape[0]).astype(np.float32)  # mostly dim, a few bright
    star_field[sy, sx] = np.maximum(star_field[sy, sx], mags)

    # A handful of bright "named" stars with bloom.
    n_bright = 250
    bx = rng.integers(0, W, n_bright)
    by = rng.integers(0, H, n_bright)
    for x_, y_, m in zip(bx, by, rng.uniform(1.0, 2.5, n_bright)):
        y0, y1 = max(0, y_-3), min(H, y_+4)
        x0, x1 = max(0, x_-3), min(W, x_+4)
        yy, xx = np.mgrid[y0:y1, x0:x1]
        d2 = (yy - y_) ** 2 + (xx - x_) ** 2
        star_field[y0:y1, x0:x1] = np.maximum(star_field[y0:y1, x0:x1],
                                              m * np.exp(-d2 * 0.6))

    # --- Compose -----------------------------------------------------------
    base_r = band * 0.35 + core * 0.9
    base_g = band * 0.28 + core * 0.55
    base_b = band * 0.55 + core * 0.20

    # Star color: slight blue-white scatter.
    star_tint_r = 0.9 + 0.1 * rng.random((H, W)).astype(np.float32)
    star_tint_g = 0.9 + 0.1 * rng.random((H, W)).astype(np.float32)
    star_tint_b = 1.0 + 0.1 * rng.random((H, W)).astype(np.float32)

    r = base_r + star_field * star_tint_r
    g = base_g + star_field * star_tint_g
    b = base_b + star_field * star_tint_b

    # Faint nebular wash via low-freq noise.
    neb = fbm(U * 3, V * 1.5, octaves=4, seed=2025) ** 2 * 0.06
    r += neb * 0.9
    g += neb * 0.6
    b += neb * 1.2

    img = np.stack([r, g, b], axis=-1)
    img = np.clip(img, 0, 1)

    if args.hdr:
        # Radiance .hdr writer (RGBE).
        hdr_out = os.path.splitext(args.out)[0] + ".hdr"
        write_hdr(hdr_out, img)
        print(f"[starfield] wrote {hdr_out}")

    img8 = (img ** (1/2.2) * 255).astype(np.uint8)
    Image.fromarray(img8, "RGB").save(args.out, optimize=True)
    sz = os.path.getsize(args.out) / 1024
    print(f"[starfield] wrote {args.out} ({sz:.0f} KiB)")

def write_hdr(path, img):
    h, w, _ = img.shape
    with open(path, "wb") as f:
        f.write(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n")
        f.write(f"-Y {h} +X {w}\n".encode())
        # Per-pixel RGBE (no RLE — simple).
        rgbe = np.zeros((h, w, 4), dtype=np.uint8)
        m = img.max(axis=-1)
        nz = m > 1e-32
        e = np.zeros_like(m)
        e[nz] = np.ceil(np.log2(m[nz])) + 128
        scale = np.zeros_like(m)
        scale[nz] = 256.0 / (2 ** (e[nz] - 128))
        rgbe[..., 0] = np.clip(img[..., 0] * scale, 0, 255)
        rgbe[..., 1] = np.clip(img[..., 1] * scale, 0, 255)
        rgbe[..., 2] = np.clip(img[..., 2] * scale, 0, 255)
        rgbe[..., 3] = e
        f.write(rgbe.tobytes())

if __name__ == "__main__":
    main()
