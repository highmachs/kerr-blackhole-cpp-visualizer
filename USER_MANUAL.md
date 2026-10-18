# Kerr Black Hole Visualizer — Flight Manual & User Guide

**High-Performance Real-Time Kerr Spacetime Geodesic Raytracer**  
**Version 1.0.0 (Windows x64 Native C++20 / OpenGL 4.6)**

---

## 1. Overview & Architecture

The **Kerr Black Hole Visualizer** is a real-time, physically grounded general relativity simulator that numerically integrates null geodesics (photon trajectories) around a rotating (Kerr) black hole using Runge-Kutta numerical integration in GLSL compute/fragment shaders.

Unlike simplified 2D approximations or pre-baked animations, this visualizer renders the actual curved geometry of spacetime in real-time, faithfully capturing:
- **Gravitational Lensing & Dual Einstein Rings**: Infinite optical distortion and Einstein rings produced by severe photon deflection.
- **Relativistic Frame-Dragging (Lense-Thirring Effect)**: Spacetime itself is dragged into rotation by the black hole's angular momentum, producing an ergosphere where nothing can remain at rest relative to distant stars.
- **Relativistic Beaming & Doppler Boosting**: The accretion disk appears drastically brighter and blueshifted on the side rotating toward you, and dimmer and redshifted on the receding side.
- **Gravitational Redshift & Time Dilation**: Clocks tick slower closer to the horizon; light escaping from deep gravitational wells experiences extreme energy loss.
- **The Inner Cauchy Horizon & Blueshift Flash**: The Cauchy horizon marks an infinite blueshift surface where the entire history of the external universe piles up into an intense optical flash.
- **The Ring Singularity & Negative Universe ($r < 0$)**: The Kerr singularity is not a point, but a rotating ring. Traveling through the central aperture opens into an antigravity realm with negative radial coordinates ($r < 0$).

---

## 2. System Requirements

| Component | Minimum Specification | Recommended Specification |
| :--- | :--- | :--- |
| **Operating System** | Windows 10 64-bit (Build 1909+) | Windows 11 64-bit |
| **Graphics Card (GPU)** | Dedicated GPU with **OpenGL 4.6** support (NVIDIA GTX 1060 / AMD RX 580) | NVIDIA RTX 3060 / 4060 or AMD Radeon RX 6700 XT or higher |
| **VRAM** | 2 GB dedicated VRAM | 4 GB+ dedicated VRAM |
| **Processor (CPU)** | Intel Core i3 / AMD Ryzen 3 (64-bit) | Intel Core i5 / AMD Ryzen 5 or faster |
| **RAM** | 4 GB System Memory | 8 GB+ System Memory |
| **Storage** | 30 MB free disk space | 30 MB free disk space |

> **Note on Integrated Graphics**: Intel Iris Xe and AMD Radeon 680M/780M can run the simulator smoothly when set to the **720p** or **360p** preset in the *Display & GPU* settings tab.

---

## 3. Quick Start Guide

### Running the Standalone Release Package
1. Download **`kerr-blackhole-v1.0.0-windows-x64.zip`** from the [GitHub Releases](https://github.com/highmachs/kerr-blackhole-cpp-visualizer/releases/tag/v1.0.0) page.
2. Extract the ZIP archive anywhere on your PC.
3. Double-click **`Run_Kerr_Navigator.bat`** (or execute **`kerr_bh.exe`** directly).
4. No installation, Python runtimes, or external frameworks required!

---

## 4. Flight Controls & Hotkey Reference

The cockpit flight system provides complete 6 Degrees-of-Freedom (6-DOF) spatial navigation.

### 4.1 Movement & Thrusters

| Key / Input | Action | Description |
| :--- | :--- | :--- |
| **`W`** | **Thrust Forward** | Accelerate forward along your camera's crosshair vector. |
| **`S`** | **Thrust Backward** | Reverse thrust away from the crosshair target. |
| **`A`** | **Strafe Left** | Lateral slide to the port (left) side. |
| **`D`** | **Strafe Right** | Lateral slide to the starboard (right) side. |
| **`Space`** or **`E`** | **Ascend (Up)** | Vertical climb relative to camera horizon. |
| **`Left-Ctrl`** or **`Q`** | **Descend (Down)** | Vertical drop relative to camera horizon. |
| **`Z`** | **Roll Left** | Counter-clockwise roll rotation around the sightline axis. |
| **`C`** | **Roll Right** | Clockwise roll rotation around the sightline axis. |
| **`Left Shift`** | **Turbo Boost** | Increases thruster speed by **3.0x** for rapid orbital transit. |

### 4.2 Camera & Cockpit Interaction

| Key / Input | Action | Description |
| :--- | :--- | :--- |
| **`TAB`** or **Right-Click Drag** | **Toggle Mouse Look** | Swaps between interactive HUD mouse cursor mode and free-look orbital view. |
| **`P`** or **`H`** | **Toggle Cockpit HUD** | Hides all UI panels for a pristine, cinematic screen view (ideal for video recording). |
| **`R`** | **Reset Orbit** | Instantly restores camera to the canonical safe orbit ($r = 14M$, $77^\circ$ inclination). |
| **`K`** | **Cycle Accretion Disk** | Cycles through the 4 accretion disk rendering styles (Interstellar, Cyan, Naked Lens, Doppler). |
| **`T`** | **Coordinate Grid** | Toggles the 3D Boyer-Lindquist spatial coordinate grid lines. |
| **`O`** | **Save Screenshot** | Captures a lossless 24-bit PNG screenshot (`screenshot.png`) of the active viewport. |
| **`ESC`** | **Exit Simulator** | Cleanly terminates the application. |

### 4.3 Quantum Phase Jumps (Keys 1 – 6)

| Key | Phase Name | Radial Distance | Core Phenomenon |
| :---: | :--- | :---: | :--- |
| **`1`** | **Approach** | $r = 14.0 M$ | Distant stable Keplerian orbit; dual Einstein lensing rings. |
| **`2`** | **Ergosphere & Outer Horizon** | $r = 2.0 M$ | Frame-dragging forces all motion into spin direction ($g_{tt} = 0$). |
| **`3`** | **Mid-Horizon Inward Fall** | $r = 1.0 M$ | Space and time swap roles; inevitable inward geodesic curvature. |
| **`4`** | **Cauchy Horizon Blueshift** | $r = 0.68 M$ | Infinite blueshift flash; outside universe collapses into a bright ring. |
| **`5`** | **Ring Singularity Throat** | $r = 0.05 M$ | Approaching the central ring aperture; finite curvature crossing. |
| **`6`** | **Negative Universe** | $r = -2.0 M$ | Antigravity realm ($r < 0$); repulsive mass pushes ship away. |

---

## 5. Control Deck Tabs & Dashboard Features

The right-hand Control Deck sidebar contains five specialized control stations:

```
+-------------------------------------------------------------------------------+
| Kerr Navigator - Control Deck                                                 |
+-------------------------------------------------------------------------------+
| [ Telemetry ]  [ Phases ]  [ Visuals ]  [ Display & GPU ]  [ Flight Manual ]  |
+-------------------------------------------------------------------------------+
```

### Tab 1: Telemetry (Live General Relativity Metrics)
- **Zone Classifier**: Displays whether you are in *Safe High Orbit*, *Ergosphere*, *Inside Horizon*, *Cauchy Flash*, *Ring Throat*, or the *Negative Universe*.
- **Vortex Drag Speed**: The instantaneous frame-dragging velocity of spacetime in fractions of lightspeed ($c$).
- **Time Dilation**: Earth clock equivalent for 1 second of ship time (e.g. $1\text{s ship} = 34.2\text{s Earth}$).
- **Tidal Shear Stress**: Gravitational tidal gradient measuring differential pull ($F_{\text{tidal}} \propto 2M/r^3$). Alerts if approaching lethal spaghettification thresholds.
- **Orbital Radii Telemetry**: Live readout of radial distance $r$, outer event horizon $r_+$, and inner Cauchy horizon $r_-$.
- **Simulation Speed Slider**: Accelerates or decelerates flight physics between $0.1\times$ and $5.0\times$.

### Tab 2: Phases (Instant Navigation & Spin Parameter)
- **Phase Buttons 1–6**: 1-click teleportation to the 6 primary General Relativity milestones.
- **Spin Parameter ($a$) Slider**: Adjust the dimensionless black hole spin from $a = 0.00$ (static Schwarzschild) to $a = 0.99$ (near-extremal Kerr).
- **Reset Orbit & Screenshot**: Rapid action buttons.

### Tab 3: Visuals (Shaders, Optics & Geodesics)
- **Accretion Disk Styles**:
  1. *Classic Interstellar*: Realistic glowing amber gas with secondary blue/violet accretion caustics.
  2. *High-Energy Sapphire Plasma*: Electric cyan, high-temperature magnetized relativistic plasma.
  3. *Naked Gravitational Lens*: Pure vacuum spacetime lensing with starry background distortion only.
  4. *Doppler Heatmap*: Scientific false-color visualization showing Doppler blueshift (blue) and redshift (red).
- **Cockpit Reticle & Flight HUD**: Enable or disable HUD crosshairs and indicators.
- **Bloom & Brightness Sliders**: Fine-tune disk glow, stellar background intensity, and bloom saturation.
- **Geodesic Trail & Photon Sphere**: Toggle visible geometric aids.
- **Raymarching Step Presets**:
  - *Fast (140 steps)*: Ideal for battery conservation or budget GPUs.
  - *Balanced (240 steps)*: Standard high-fidelity preset.
  - *Cinematic (320 steps)*: Extreme ray integration density for razor-sharp accretion caustics.
- **UI Scale**: Custom scaling for high-DPI displays (100% to 300%).

### Tab 4: Display & GPU (Dynamic Resolution & Performance)
- **Resolution Preset Selector**: Switch dynamically between:
  - **360p** ($640 \times 360$) — Ultralight / Low GPU / Laptop
  - **720p** ($1280 \times 720$) — Fast High-Framerate (120+ FPS)
  - **1080p** ($1920 \times 1080$) — Full HD Balanced (Default)
  - **1440p** ($2560 \times 1440$) — 2K Quad HD
  - **4K** ($3840 \times 2160$) — Ultra HD Native
  - **8K** ($7680 \times 4320$) — Extreme Stress
  - **Match Window** — 1:1 pixel match with current window dimensions
- **Render Scale Slider**: Freely scale internal canvas from $0.25\times$ to $2.0\times$.
- **Hardware Upscaling Pipeline**: Uses high-quality OpenGL linear hardware upscaling to fill the full monitor without blurry artifacts.
- **Auto-Protect on GPU Lag**: Automatically monitors frametime. If your GPU drops below 11 FPS (latency $> 90\text{ ms}$), an emergency prompt offers instant 1-click downscaling to 1080p or 720p.
- **VSync Toggle**: Synchronize rendering with monitor refresh rate to eliminate tearing.

### Tab 5: Flight Manual & Help
- Full in-game copy of flight keys, relativistic science explanations, and GPU tuning tips directly accessible without alt-tabbing.

---

## 6. General Relativity Physics Explained

### The Kerr Metric
In Boyer-Lindquist coordinates $(t, r, \theta, \phi)$, the spacetime metric of an uncharged rotating black hole of mass $M$ and angular momentum $J = aM$ is:

$$ds^2 = -\left(1 - \frac{2Mr}{\rho^2}\right) dt^2 - \frac{4Mar\sin^2\theta}{\rho^2} dt\, d\phi + \frac{\rho^2}{\Delta} dr^2 + \rho^2 d\theta^2 + \left(r^2 + a^2 + \frac{2Ma^2 r\sin^2\theta}{\rho^2}\right)\sin^2\theta\, d\phi^2$$

where:
$$\rho^2 = r^2 + a^2 \cos^2\theta, \quad \Delta = r^2 - 2Mr + a^2$$

### The Horizons
The coordinate singularities occur where $\Delta = 0$:
$$r_\pm = M \pm \sqrt{M^2 - a^2}$$
- **$r_+$ (Outer Event Horizon)**: The true event horizon. Once crossed, all future-directed timelike curves lead inward.
- **$r_-$ (Inner Cauchy Horizon)**: A boundary beyond which Einstein field equations cannot deterministically predict future states. Photons entering from the external universe undergo infinite blueshift.

### Ergosphere & Frame-Dragging
The static limit occurs where $g_{tt} = 0$:
$$r_{\text{ergo}}(\theta) = M + \sqrt{M^2 - a^2\cos^2\theta}$$
In the region between $r_{\text{ergo}}$ and $r_+$, frame dragging is so powerful that an observer would have to travel faster than light relative to the local coordinate grid simply to appear stationary to an observer at infinity.

### The Ring Singularity & Negative Universe
Unlike the point singularity of a Schwarzschild black hole ($r=0$), the curvature singularity of a Kerr black hole occurs when $\rho^2 = 0$, which requires:
$$r = 0 \quad \text{and} \quad \theta = \frac{\pi}{2}$$
In 3D Cartesian coordinates, this corresponds to an open ring in the equatorial plane with radius $x^2 + y^2 = a^2$.
Because the singularity is a 1-dimensional ring, paths passing through the center of the ring do not hit the singularity! Instead, continuing through the aperture opens into a second asymptotic realm where $r < 0$. In this negative universe, the gravitational term $-2Mr/\rho^2$ switches sign to $+2M|r|/\rho^2$, causing gravity to become strictly **repulsive** (antigravity).

---

## 7. Command-Line Options Reference

You can pass command-line arguments to `kerr_bh.exe` directly from PowerShell, Command Prompt, or terminal shortcuts:

```powershell
.\kerr_bh.exe [options]
```

| Flag | Parameter | Description | Example |
| :--- | :--- | :--- | :--- |
| `--width` | `<pixels>` | Set initial window display width | `.\kerr_bh.exe --width 1920` |
| `--height` | `<pixels>` | Set initial window display height | `.\kerr_bh.exe --height 1080` |
| `--preset` | `<0-6>` | Initial resolution preset index (0=360p, 1=720p, 2=1080p, 3=1440p, 4=4K, 5=8K, 6=Window) | `.\kerr_bh.exe --preset 2` |
| `--scale` | `<float>` | Internal render buffer scale multiplier ($0.25$ to $2.0$) | `.\kerr_bh.exe --scale 1.0` |
| `--r` | `<float>` | Initial orbital radial distance in units of $M$ | `.\kerr_bh.exe --r 14.0` |
| `--theta` | `<radians>`| Initial camera inclination angle in radians | `.\kerr_bh.exe --theta 1.35` |
| `--screenshot`| `<filename>`| Render frames, capture screenshot, and exit | `.\kerr_bh.exe --screenshot out.png` |
| `--frames` | `<count>` | Number of frames to render before saving screenshot | `.\kerr_bh.exe --frames 10` |
| `--headless` | None | Run off-screen without opening a desktop window (for automated benchmarking) | `.\kerr_bh.exe --headless` |

---

## 8. Troubleshooting & FAQ

### Q: The screen is black on startup, or the window closes immediately.
- **Cause**: Your graphics driver does not support OpenGL 4.6 or is defaulting to an integrated GPU that lacks driver updates.
- **Solution**:
  1. Update your graphics drivers from [NVIDIA](https://www.nvidia.com/Download/index.aspx), [AMD](https://www.amd.com/en/support), or [Intel](https://www.intel.com/content/www/us/en/download-center/home.html).
  2. For laptops with dual GPUs (NVIDIA + Intel/AMD), open *NVIDIA Control Panel* > *Manage 3D Settings* > *Program Settings*, select `kerr_bh.exe`, and set *Preferred graphics processor* to **High-performance NVIDIA processor**.

### Q: The framerate is low or lagging on my PC.
- **Solution**:
  1. Open the **Display & GPU** tab in the sidebar and click **Quick Light 720p** or set the preset to **1080p**.
  2. In the **Visuals** tab, click the **Fast (140)** quality button to reduce ray steps.
  3. Ensure your laptop is plugged into wall power (battery saver modes often cap GPU power to 30W).

### Q: How do I hide all HUD menus for clean recording or desktop wallpaper shots?
- **Solution**:
  - Press **`P`** or **`H`** on your keyboard to instantly hide all HUD windows and crosshairs.
  - Press **`O`** to save a full-resolution PNG screenshot (`screenshot.png`).
  - Press **`P`** or **`H`** again anytime to bring the HUD back.

### Q: How do I steer with the mouse?
- **Solution**:
  - Press **`TAB`** or hold **Right-Click** and drag to capture the mouse for intuitive 3D flight look.
  - Press **`TAB`** again to release the mouse cursor and interact with the Control Deck buttons.

---

## 9. License & Credits

- **Physics & Geodesic Raytracing Engine**: Kerr metric Boyer-Lindquist numerical geodesic raymarching in GLSL.
- **Graphics Pipeline**: OpenGL 4.6 Core Profile with GLFW 3.4 and GLAD.
- **Interface**: Dear ImGui (v1.91+ docking branch) customized with high-DPI scaling and space-mission styling.
- **Open Source Repository**: [https://github.com/highmachs/kerr-blackhole-cpp-visualizer](https://github.com/highmachs/kerr-blackhole-cpp-visualizer)
