# Kerr Black Hole Interior Navigator

[![Download Standalone .EXE](https://img.shields.io/badge/Download-Standalone_kerrblackhole.exe_(3.1MB)-00e5ff?style=for-the-badge&logo=windows)](https://github.com/highmachs/kerr-blackhole-cpp-visualizer/releases/download/v1.0.0/kerrblackhole.exe)
[![Resolution](https://img.shields.io/badge/Resolution-360p_to_8K_Ultra_HD-ff4d1a?style=for-the-badge)](https://github.com/highmachs/kerr-blackhole-cpp-visualizer)
[![License](https://img.shields.io/badge/License-MIT-blue?style=for-the-badge)](LICENSE)

An interactive, scientifically rigorous real-time Kerr black hole and Negative Universe raymarching simulator built with **C++20, OpenGL 4.6, GLFW, GLM, Dear ImGui, and GLSL compute shaders**.

Runs **100% natively on your GPU** with multi-threaded performance, zero browser overhead, and sustained 60–400+ FPS.

---

## ⚡ Quick Start: Download & Run (Zero Install)

No compiler, Visual Studio, or dependencies required. Everything is pre-built and self-contained in a single portable `.exe`:

1. **[Download kerrblackhole.exe (Single Portable Executable, ~3.1 MB)](https://github.com/highmachs/kerr-blackhole-cpp-visualizer/releases/download/v1.0.0/kerrblackhole.exe)**
2. Double-click **`kerrblackhole.exe`** to run anywhere (Desktop, Downloads, USB).
3. No extraction, no installers, no external folders needed! All shaders and assets are embedded.
4. Enjoy smooth 60–400+ FPS raymarching!

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

| Key / Action | Function |
| --- | --- |
| **W / A / S / D** | 3D Flight Thrusters (Forward / Left / Backward / Right) |
| **Space or E** | Vertical Lift Thruster (Up) |
| **Left-Ctrl or Q** | Vertical Descent Thruster (Down) |
| **Left-Shift** | Turbo Booster (3.0x thrust speed) |
| **Mouse Click** | Lock mouse cursor to 3D Viewport for free-look |
| **Z / C** | 6-DOF Camera Roll (Roll Left / Roll Right) |
| **1 to 6** | Instant Quantum Teleportation across all 6 spacetime phases |
| **G** | Cinematic Guided Tour (Cruising through horizons into Negative Universe) |
| **X** | Lock Keplerian Circular Orbit |
| **R** | Reset Orbit to 14M Approach |
| **K** | Cycle Accretion Disk Styles (Classic, Sapphire, Naked Lens, Doppler Heatmap) |
| **O** | Capture Lossless Screenshot (`screenshot.png`) |
| **V** | Toggle VSync |
| **F11** | Toggle Borderless Fullscreen |
| **Tab or Esc** | Release / Recapture Mouse Cursor |
| **P or H** | Hide / Show HUD Interface |

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
