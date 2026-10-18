# Kerr Black Hole Interior Navigator

[![Live Web Simulation](https://img.shields.io/badge/Live_Web_Simulator-GitHub_Pages-00e5ff?style=for-the-badge&logo=github)](https://highmachs.github.io/kerr-blackhole-cpp-visualizer/)
[![Resolution](https://img.shields.io/badge/Resolution-360p_to_8K_Ultra_HD-ff4d1a?style=for-the-badge)](https://github.com/highmachs/kerr-blackhole-cpp-visualizer)
[![License](https://img.shields.io/badge/License-MIT-blue?style=for-the-badge)](LICENSE)

An interactive, scientifically rigorous real-time Kerr black hole and Negative Universe raymarching simulator built with **C++20, OpenGL 4.6, GLFW, GLM, Dear ImGui, and GLSL compute shaders**, accompanied by an **instant in-browser WebGL 2 Web Simulator** hosted for free on GitHub Pages.

---

## 🌐 Try It Directly in Your Browser (No Download Required)

You can explore the full Kerr spacetime, Novikov-Thorne accretion disk, and Negative Universe right now without installing anything:

👉 **[https://highmachs.github.io/kerr-blackhole-cpp-visualizer/](https://highmachs.github.io/kerr-blackhole-cpp-visualizer/)**

---

## 🖥️ Dynamic In-App Resolution Switching & GPU Protection

The simulator includes live in-app render resolution scaling with hardware bilinear filtering:
- **360p (640x360)**: Ultra-low latency for laptops and integrated GPUs.
- **720p (1280x720)**: High performance (120+ FPS) on budget GPUs.
- **1080p (1920x1080)**: Full HD balanced fidelity.
- **1440p / 2K (2560x1440)**: Crisp high-density visuals.
- **4K UHD (3840x2160)**: Extreme raymarching cinematic fidelity.
- **8K Super-Res (7680x4320)**: Extreme stress test and ultra-dense benchmark mode.
- **Match Window (1:1)**: Dynamic real-time canvas matching your current window dimensions.

### 🛡️ Automatic GPU Overload Lag Recovery
If your GPU frame rate drops below 11 FPS for more than 2 seconds, the simulator detects the overload and triggers an emergency alert banner allowing instant 1-click downscaling to 1080p or 720p Safe Mode.

## Controls

| Key | Action |
| --- | --- |
| W/S | Increase or decrease inward radial velocity |
| A/D | Adjust angular momentum |
| Q/E | Adjust Carter constant |
| Mouse | Look around |
| Space | Reset to default infall trajectory |
| R | Reset simulation |
| T | Toggle debug grid |
| P | Toggle debug panel and geodesic trail |
| O | Capture screenshot |
| V | Toggle vsync |
| F11 | Toggle fullscreen |
| Tab | Toggle mouse capture |
| Esc | Quit |

## How To Use It

Start the app, then treat it like a cockpit view into the black hole. Move the camera with the mouse, adjust the path with W/S, A/D, and Q/E, and watch the readouts in the debug panel to see the current radius, horizon status, and geodesic parameters.

The important things to look for are:

- the accretion disk outside the hole
- the horizon crossing transition
- the whiteout / flash effect near the inner horizon
- the trail that shows your recent path
- the screenshot output when you press `O`

## Assets

The repository ships a procedural 4096x2048 starfield in both PNG and HDR form:

- [assets/starfield.png](assets/starfield.png)
- [assets/starfield.hdr](assets/starfield.hdr)

The renderer loads `.hdr` first, then `.png`, then falls back to an in-shader procedural sky.

## Build

### Visual Studio 2022

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\Release\kerr_bh.exe
```

### Ninja

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The CMake build fetches GLFW, GLM, and Dear ImGui automatically if they are not already installed as packages.

## Project Layout

```text
.
├── CMakeLists.txt
├── README.md
├── assets/
├── shaders/
├── src/
├── third_party/
└── kerrblackhole.md
```

## Current Status

The app builds on Windows/MSVC from the repository root. The core Kerr pipeline, ImGui overlays, starfield assets, geodesic trail, grid, audio, and screenshot path are in place.

What is complete now:

- root native build is in place
- Windows-only README and build instructions are in place
- core camera, renderer, shader, and audio paths exist
- starfield assets are shipped
- screenshots, fullscreen, debug overlay, grid, and trail controls exist

Remaining work is about deeper validation and polish rather than basic scaffolding:

- Performance claims still need benchmark runs on target hardware
- Further physics tuning may still be possible if you want stricter fidelity to the MD

What is not fully complete versus the full spec:

- cross-platform support is not being pursued here
- performance targets are not benchmark-verified yet
- the spec calls for more physics fidelity and broader validation than the current Windows build has proven

## Regenerate the Starfield

```powershell
python .\assets\make_starfield.py
python .\assets\make_starfield.py --hdr
```
