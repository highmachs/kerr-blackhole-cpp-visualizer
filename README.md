# Kerr Black Hole Interior Navigator

Windows-only Kerr black hole interior renderer built in C++17 with OpenGL 4.6, GLFW, GLM, Dear ImGui, GLSL compute shaders, and a CPU/GPU geodesic pipeline.

The repository root is the app root. The executable is built from [CMakeLists.txt](CMakeLists.txt) and the source lives in [src](src), [shaders](shaders), [assets](assets), and [third_party](third_party).

## Quick Start

1. Build the app from the repository root with Visual Studio 2022 or Ninja.
2. Run `build\Release\kerr_bh.exe`.
3. Use the mouse to look around and W/S, A/D, Q/E to steer the infall path.
4. Press `R` to reset, `T` to toggle the grid, `P` to toggle the debug panel and trail, and `O` to save a screenshot.

If the mouse is stuck, press `Tab` to toggle capture. Press `F11` for fullscreen and `Esc` to quit.

## What It Does

- Render a rotating Kerr black hole with an accretion disk and relativistic starfield
- Cross the outer horizon, inner Cauchy horizon, and ring singularity into negative-r mode
- Show a debug grid, geodesic trail, photon sphere glow, and whiteout flash transitions
- Play procedural audio cues for horizon and singularity events
- Capture screenshots from inside the app

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
