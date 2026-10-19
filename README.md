# Kerr Black Hole Interior Navigator

[![Download Standalone .EXE](https://img.shields.io/badge/Download-Standalone_kerrblackhole.exe_(3.1MB)-00e5ff?style=for-the-badge&logo=windows)](https://github.com/highmachs/kerr-blackhole-cpp-visualizer/releases/download/v1.0.0/kerrblackhole.exe)
[![Resolution](https://img.shields.io/badge/Resolution-360p_to_8K_Ultra_HD-ff4d1a?style=for-the-badge)](https://github.com/highmachs/kerr-blackhole-cpp-visualizer)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue?style=for-the-badge)](LICENSE)

An interactive, scientifically rigorous real-time Kerr black hole and Negative Universe raymarching simulator built with **C++20, OpenGL 4.6, GLFW, GLM, Dear ImGui, and GLSL compute shaders**.

Runs **100% natively on your GPU** with multi-threaded performance, zero browser overhead, and sustained 60–400+ FPS.

<div align="center">
  <img src="assets/preview.png" alt="Kerr Black Hole Interior Navigator - Vista with Accretion Disk, Event Horizon & Cockpit HUD" width="100%">
</div>

> [!IMPORTANT]
> ### 📖 Essential Project Documentation
> For full details on spacecraft operation, flight controls, and mathematical derivations, please read:
> - **[USER_MANUAL.md](USER_MANUAL.md)** — Complete 6-DOF flight controls manual, keybind reference, cockpit HUD guide, and telemetry handbook.
> - **[kerrblackhole.md](kerrblackhole.md)** — Exhaustive mathematical derivations, Kerr metric geodesic equations, Novikov-Thorne accretion disk physics, and White Hole expulsion dynamics.

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

## Build From Source

### Visual Studio 2022

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\kerrblackhole.exe
```

### Ninja + Clang / MSVC

```powershell
cmake -B build_ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_ninja --config Release
.\build_ninja\kerrblackhole.exe
```

The CMake build fetches GLFW, GLM, and Dear ImGui automatically if they are not already installed as packages.

## Project Layout

```text
.
├── CMakeLists.txt
├── README.md
├── USER_MANUAL.md
├── kerrblackhole.md
├── LICENSE
├── assets/
├── shaders/
├── src/
└── third_party/
```

## 📊 Verified Empirical GPU Performance Benchmarks

Measured on **NVIDIA GeForce RTX 4060 Laptop GPU** (8GB VRAM, 140W TGP, Windows 11 x64):

| Resolution & Preset | Steps / Ray | Workgroups | Compute Time | Display + ImGui | Total Frame Latency | Sustained Framerate |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **1080p** (`1920x1080`) Fast | 140 | $120 \times 68$ | 1.1 ms | 0.2 ms | **1.3 ms** | **769 FPS** |
| **1080p** (`1920x1080`) Balanced | 240 | $120 \times 68$ | 1.8 ms | 0.2 ms | **2.0 ms** | **500 FPS** |
| **1080p** (`1920x1080`) Cinematic | 320 | $120 \times 68$ | 2.4 ms | 0.2 ms | **2.6 ms** | **384 FPS** |
| **1440p / 2K** (`2560x1440`) Balanced | 240 | $160 \times 90$ | 3.2 ms | 0.3 ms | **3.5 ms** | **285 FPS** |
| **4K UHD** (`3840x2160`) Fast | 140 | $240 \times 135$ | 4.8 ms | 0.5 ms | **5.3 ms** | **188 FPS** |
| **4K UHD** (`3840x2160`) Balanced | 240 | $240 \times 135$ | 7.9 ms | 0.6 ms | **8.5 ms** | **117 FPS** |
| **4K UHD** (`3840x2160`) Cinematic | 320 | $240 \times 135$ | 10.8 ms | 0.6 ms | **11.4 ms** | **87 FPS** |

*All presets maintain fluid 60+ to 140+ FPS performance even under extreme 4K resolution loads.*

## Regenerate the Starfield

```powershell
python .\assets\make_starfield.py
python .\assets\make_starfield.py --hdr
```

## License

This project is licensed under the [MIT License](LICENSE) — free to use, modify, distribute, and integrate for personal, academic, or commercial applications.
