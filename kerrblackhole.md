# 🕳️ KERR BLACK HOLE INTERIOR NAVIGATOR — FULL BUILD SPEC (C++ / OpenGL)

## 1. OVERVIEW & GOAL

Build a **real-time, first-person ray-traced simulation** of flying into a rotating (Kerr) black hole — **passing through the outer horizon, crossing the inner Cauchy horizon, and emerging into a different universe** (or our own past).

- **Primary user:** Physicists, computational astrophysics enthusiasts, shader nerds, people who think Interstellar was too conservative
- **Key emotions:** Awe, vertigo, "I can't believe this is real-time"
- **Technical differentiator:** Almost no one renders the **interior** of a Kerr black hole. Most stop at the event horizon. You won't.

---

## 2. PHYSICS BACKGROUND (What You're Simulating)

| Concept | Description |
|---------|-------------|
| **Kerr Metric** | Describes spacetime around a rotating black hole. Two key parameters: mass M and spin a (0 to 1, where a=1 is maximally rotating). You'll use a = 0.95 for dramatic effects. |
| **Outer Horizon (Event Horizon)** | r_outer = M + sqrt(M² - a²). Once crossed, you cannot escape. |
| **Inner Horizon (Cauchy Horizon)** | r_inner = M - sqrt(M² - a²). Inside this, causality breaks down. |
| **Ergosphere** | Region outside outer horizon where spacetime is dragged so strongly that nothing can remain stationary. |
| **Ring Singularity** | At r = 0, θ = π/2. Not a point — a ring. You can pass *through* it. |
| **Negative Universe** | Beyond the ring singularity, r becomes negative. Spacetime flips. You emerge into a different universe (or same universe, time-reversed). |

---

## 3. TECH STACK

| Layer | Choice | Reason |
|-------|--------|--------|
| Language | C++17 | Performance, precision |
| Graphics API | OpenGL 4.6 | Widely supported, compute shaders for ray marching |
| Windowing | GLFW | Simple, lightweight |
| Math | GLM | Vector/matrix math |
| Image Loading | stb_image | Texture loading for starfields |
| UI | Dear ImGui | Debug controls, real-time parameter tuning |
| Shaders | GLSL (compute + fragment) | Ray tracing on GPU |
| Build System | CMake | Cross-platform |

---

## 4. UNIQUE COLOR PALETTE (No Common AI Colors)

**Same "Obsidian & Ember" palette as before — but adapted for black hole physics:**

| Element | Color Name | Hex | RGB | Usage |
|---------|------------|-----|-----|-------|
| Vacuum/Empty Space | Deep Void | #0a0807 | 10,8,7 | Background space |
| Accretion Disk Core | Plasma Core | #ff4d1a | 255,77,26 | Inner disk (hottest) |
| Accretion Disk Mid | Molten Iron | #e87a3a | 232,122,58 | Mid disk |
| Accretion Disk Outer | Cooling Ember | #b85c2a | 184,92,42 | Outer disk |
| Photon Sphere | Event Horizon Glow | #ff6a2a | 255,106,42 | Thin glowing ring |
| Blue-shifted Stars | Doppler Sapphire | #2e5a7d | 46,90,125 | Stars ahead (contracted) |
| Red-shifted Stars | Doppler Ruby | #7d2e2e | 125,46,46 | Stars behind (stretched) |
| Grid Lines (r) | Burnt Sienna | #c97e5a | 201,126,90 | Radial coordinate grid |
| Grid Lines (θ) | Dried Clay | #a86b4a | 168,107,74 | Polar coordinate grid |
| Grid Lines (φ) | Weathered Bronze | #8a6e4a | 138,110,74 | Azimuthal grid |
| Cauchy Horizon Flash | Whiteout | #ffffff | 255,255,255 | Brief full-screen flash |
| Negative Universe Sky | Inverted Void | #1a0f0a | 26,15,10 | Post-ring universe sky |
| Tooltip/Debug Text | Warm Ash | #c4b5a5 | 196,181,165 | ImGui text |

**No blue except Doppler Sapphire (physical, not aesthetic). No purple. No teal.**

---

## 5. RENDERING PIPELINE ARCHITECTURE

```
┌─────────────────────────────────────────────────────────────┐
│                      CPU (C++ Main Loop)                    │
│  - Camera position/orientation update (WASD + mouse look)   │
│  - Integrate geodesic equations (RK4) to get new position   │
│  - Check horizon crossings (r < r_outer, r < r_inner, r<0) │
│  - Update universe mode flag (normal / negative)            │
│  - Pass camera state + mode to GPU                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    GPU Compute Shader                       │
│  - For each pixel, cast ray from camera                     │
│  - Backward integrate null geodesic through Kerr metric     │
│  - Accumulate radiance from:                                │
│    • Accretion disk (intersection)                          │
│    • Starfield (celestial sphere) with relativistic effects │
│    • Photon sphere (if ray trapped)                         │
│  - Apply redshift/blueshift based on frequency factor       │
│  - Output color to texture                                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    Fragment Shader (Fullscreen Quad)        │
│  - Display compute shader output texture                    │
│  - Apply tone mapping + bloom (optional)                    │
│  - Overlay debug grid (if enabled)                          │
│  - Render ImGui overlays                                    │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. KERR METRIC — MATHEMATICAL DETAILS

### Boyer-Lindquist Coordinates (t, r, θ, φ)

**Metric coefficients (geometrized units G=c=1, M=1 for normalization):**

```
Δ = r² - 2r + a²
Σ = r² + a² cos²θ
ρ² = Σ
α = √(ΣΔ / ( (r² + a²)² - a²Δ sin²θ ))   (lapse)
ω = 2a r / ((r² + a²)² - a²Δ sin²θ)     (frame dragging angular velocity)
```

**Null geodesic equations (for light rays):**

```
dt/dλ = ( (r² + a²)² - a²Δ sin²θ ) / (Δ Σ) × E - (2a r)/(Δ Σ) × L
dr/dλ = ± √(R) / Σ
dθ/dλ = ± √(Θ) / Σ
dφ/dλ = (2a r)/(Δ Σ) × E + (Δ - a² sin²θ)/(Δ Σ sin²θ) × L
```

Where:
- R = ( (r² + a²)E - aL )² - Δ( (L - aE)² + Q )
- Θ = Q - cos²θ( a²(1 - E²) + L²/sin²θ )
- E = energy constant (set to 1 for photons from infinity)
- L = angular momentum constant
- Q = Carter constant

**For ray tracing simplification (real-time performance):**
- Assume E = 1, L = impact parameter, Q = 0 (equatorial plane approximation for first version)
- Full 4D integration for advanced version

---

## 7. CAMERA & NAVIGATION

### Camera Model

| Property | Value |
|----------|-------|
| Type | First-person, free-fall |
| Initial position | r = 10M, θ = π/2 (equatorial plane), φ = 0 |
| Initial velocity | Inward radial (dr/dλ negative) |
| Control scheme | WASD = adjust angular momentum (L) and Carter constant (Q) — not Newtonian thrust |

### Controls

| Key | Action |
|-----|--------|
| W/S | Increase/decrease inward radial velocity (dr/dλ) |
| A/D | Increase/decrease angular momentum (L) — changes orbital direction |
| Q/E | Adjust Carter constant (Q) — changes out-of-plane motion |
| Mouse | Look around (adjusts ray direction in camera frame) |
| Space | Reset to default infall trajectory |
| R | Reset simulation to initial position |
| T | Toggle debug grid |
| P | Toggle showing geodesic path lines |
| O | Capture screenshot |

### Horizon Crossing Detection

```cpp
enum UniverseMode {
    NORMAL,      // r > r_outer
    BETWEEN,     // r_inner < r < r_outer
    INSIDE,      // 0 < r < r_inner (between horizons)
    NEGATIVE     // r < 0 (emerged into negative universe)
};

void updateMode(double r) {
    if (r > r_outer) mode = NORMAL;
    else if (r > r_inner) mode = BETWEEN;
    else if (r > 0) mode = INSIDE;
    else mode = NEGATIVE;
}
```

When crossing from INSIDE to NEGATIVE:
- Play whiteout flash (Cauchy horizon flash)
- Flip starfield orientation (time reversal)
- Change skybox to "negative universe" variant (slightly different star positions)

---

## 8. ACCRETION DISK RENDERING

### Disk Model

| Parameter | Value |
|-----------|-------|
| Inner radius | r_ISCO (innermost stable circular orbit, ~1.5M for a=0.95) |
| Outer radius | r = 15M |
| Thickness | h = 0.1r (thin disk approximation) |
| Temperature profile | T(r) ∝ r^(-3/4) (Novikov-Thorne model) |

### Ray-Disk Intersection

For each ray, solve for intersection with disk plane (θ = π/2) within [r_in, r_out]. Then compute:

1. **Radial coordinate at intersection** → determines temperature
2. **Relative velocity** between disk plasma and photon → Doppler shift
3. **Gravitational redshift** from emission point to camera
4. **Frame dragging** affects observed angle

### Disk Color Mapping

| Temperature | Color |
|-------------|-------|
| Hottest (inner) | Plasma Core (#ff4d1a) |
| Hot (mid-inner) | Molten Iron (#e87a3a) |
| Warm (mid-outer) | Cooling Ember (#b85c2a) |
| Cool (outer) | Dried Clay (#a86b4a) |

**Add emissive glow:** Inner disk pixels should bloom slightly (post-process).

---

## 9. STARFIELD & RELATIVISTIC EFFECTS

### Celestial Sphere

- 4096x2048 equirectangular HDRI star map (Milky Way panorama)
- Mapped to sphere at infinity (r → ∞)

### Relativistic Aberration & Doppler Shift

For each ray direction in camera frame:

```
// Transform to local inertial frame (ZAMO)
// Apply aberration based on camera velocity relative to ZAMO
// Compute frequency shift factor g = ν_observed / ν_emitted

g = 1 / (γ (1 - v·n/c))
```

Where:
- γ = Lorentz factor
- v = camera velocity relative to local ZAMO
- n = ray direction

**Apply g to star color:**
- If g > 1 (blueshift) → tint toward Doppler Sapphire (#2e5a7d)
- If g < 1 (redshift) → tint toward Doppler Ruby (#7d2e2e)
- Intensity scales with g⁴ (for blackbody spectrum approximation)

---

## 10. DEBUG VISUALIZATION (For Physics Nerds)

### Spacetime Grid

Render 3D grid lines in Boyer-Lindquist coordinates:

| Grid Line | Color | Spacing |
|-----------|-------|---------|
| r = constant surfaces | Burnt Sienna (#c97e5a) | r = 3M, 5M, 7M, 10M, 15M |
| θ = constant cones | Dried Clay (#a86b4a) | θ = 30°, 60°, 90°, 120°, 150° |
| φ = constant planes | Weathered Bronze (#8a6e4a) | φ = 0°, 45°, 90°, 135°, 180°, 225°, 270°, 315° |

**Grid lines only visible when camera is OUTSIDE the event horizon** (r > r_outer). Inside, they become wildly distorted and are hidden.

### Geodesic Path Lines

- Render trails of last 1000 camera positions
- Color gradient: Warm Ash (older) → Lava Core (newest)
- Shows the spiral path into the black hole

### Photon Sphere Visualization

- Render thin glowing ring at r = photon_sphere_radius (~2.5M for a=0.95)
- Color: Event Horizon Glow (#ff6a2a)
- Visible from outside only

---

## 11. COMPUTE SHADER IMPLEMENTATION (GLSL)

### Ray Integration Loop Pseudocode

```glsl
vec3 traceRay(vec3 origin, vec3 direction) {
    vec4 pos = vec4(origin, 0.0);  // (r, θ, φ, λ)
    vec3 color = vec3(0.0);
    float steps = 0.0;
    
    for (int i = 0; i < MAX_STEPS; i++) {
        // Get metric at current position
        KerrMetric metric = getKerrMetric(pos.xyz);
        
        // Integrate geodesic (Runge-Kutta 4)
        pos = rk4Step(pos, direction, metric);
        
        // Check for intersections
        if (hitAccretionDisk(pos)) {
            color += diskColor(pos);
            break;
        }
        
        // Check if ray escaped to infinity
        if (pos.x > 50.0) {
            color += starfieldColor(direction, pos.w);
            break;
        }
        
        // Check if ray hit singularity (r < 0.01 and θ near π/2)
        if (pos.x < 0.01 && abs(pos.y - HALF_PI) < 0.1) {
            color = singularityGlow();
            break;
        }
        
        steps++;
    }
    
    return color;
}
```

### RK4 Integration Step

```glsl
vec4 rk4Step(vec4 pos, vec3 dir, KerrMetric metric) {
    float h = adaptiveStepSize(pos.xyz);
    
    vec4 k1 = geodesicDerivative(pos, dir, metric);
    vec4 k2 = geodesicDerivative(pos + 0.5*h*k1, dir, metric);
    vec4 k3 = geodesicDerivative(pos + 0.5*h*k2, dir, metric);
    vec4 k4 = geodesicDerivative(pos + h*k3, dir, metric);
    
    return pos + (h/6.0)*(k1 + 2.0*k2 + 2.0*k3 + k4);
}
```

### Adaptive Step Size

```glsl
float adaptiveStepSize(vec3 coord) {
    float r = coord.x;
    float horizon = 2.0 * (1.0 + sqrt(1.0 - a*a));
    
    if (r > horizon + 2.0) return 0.05;  // Far from BH: large steps
    if (r > horizon) return 0.01;        // Near horizon: smaller steps
    if (r > 0.5) return 0.005;            // Inside: tiny steps
    return 0.001;                         // Near singularity: microscopic
}
```

---

## 12. PERFORMANCE OPTIMIZATIONS

| Technique | Implementation |
|-----------|----------------|
| Early ray termination | Stop after MAX_STEPS (100-200) or when r > 50M |
| Acceleration structure | None (ray marching in curved space doesn't lend itself to BVH) |
| Step size adaptation | Larger steps far from BH, tiny steps near horizons/singularity |
| Precomputed metric terms | Cache Δ, Σ, α, ω at each step |
| Thread groups | 8x8 or 16x16 workgroups in compute shader |
| Shared memory | Not useful here (each ray follows unique path) |

**Target framerate:** 30-60 FPS at 1080p on a mid-range GPU (RTX 3060 or better).

---

## 13. UI OVERLAYS (Dear ImGui)

### Debug Panel (Toggle with 'P')

| Control | Purpose |
|---------|---------|
| Spin parameter a | Slider 0.0 to 0.99 (live update, resets sim) |
| Camera r position | Readout |
| Camera θ position | Readout |
| Universe mode | NORMAL / BETWEEN / INSIDE / NEGATIVE |
| Current geodesic constants | E, L, Q |
| Step count | How many RK4 steps this frame |
| Frame time | ms and FPS |
| Screenshot button | Capture PNG |

### Render Settings Panel

| Control | Purpose |
|---------|---------|
| Max steps per ray | 50-500 |
| Accretion disk brightness | 0.0-2.0 |
| Starfield brightness | 0.0-1.5 |
| Bloom intensity | 0.0-1.0 |
| Show grid | Toggle |
| Show geodesic trail | Toggle |
| Show photon sphere | Toggle |

---

## 14. AUDIO (Optional But Cool)

| Event | Sound |
|-------|-------|
| Crossing outer horizon | Low-frequency rumble + Doppler-shifted white noise |
| Crossing inner horizon | Brief high-pitched sweep + whiteout flash |
| Passing through ring singularity | Reverse cymbal (whoosh in reverse) |
| Emergence in negative universe | Distorted, reversed ambience |

**Implementation:** OpenAL or miniaudio.h for simplicity.

---

## 15. BUILD INSTRUCTIONS

### Dependencies

```bash
# Ubuntu/Debian
sudo apt install libglfw3-dev libglm-dev libopengl-dev libimgui-dev

# Windows (vcpkg)
vcpkg install glfw3 glm imgui
```

### CMakeLists.txt (Minimal)

```cmake
cmake_minimum_required(VERSION 3.20)
project(KerrBlackHole)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(imgui REQUIRED)

add_executable(kerr_bh 
    src/main.cpp 
    src/shader.cpp 
    src/camera.cpp 
    src/metric.cpp
    src/renderer.cpp
)

target_link_libraries(kerr_bh 
    OpenGL::GL 
    glfw 
    glm::glm 
    imgui::imgui
)

target_compile_definitions(kerr_bh PRIVATE GLM_ENABLE_EXPERIMENTAL)
```

### Shader Files

```
shaders/
├── compute.glsl      # Ray tracing compute shader
├── fullscreen.vert   # Fullscreen quad vertex shader
├── display.frag      # Tone mapping + bloom fragment shader
└── grid.geom         # Optional grid geometry shader
```

---

## 16. TIMELINE ESTIMATE

| Phase | Weeks | Deliverable |
|-------|-------|-------------|
| 1 | 1 | OpenGL window + GLFW + ImGui setup, basic camera controls |
| 2 | 1.5 | Kerr metric implementation (CPU verification, then GPU) |
| 3 | 1 | Null geodesic integrator (RK4, adaptive steps) |
| 4 | 1 | Accretion disk rendering (intersection + color mapping) |
| 5 | 0.5 | Starfield + relativistic aberration/Doppler |
| 6 | 1 | Horizon crossing detection + universe mode switching + whiteout |
| 7 | 0.5 | Debug grid + geodesic trails + UI panels |
| 8 | 0.5 | Polish, performance tuning, bug fixes |
| **Total** | **7 weeks** | Fully functional Kerr interior navigator |

---

## 17. FINAL CHECKLIST

| Component | Done? |
|-----------|-------|
| OpenGL 4.6 + GLFW window | ☐ |
| Dear ImGui debug panels | ☐ |
| Kerr metric (a=0.95) | ☐ |
| Null geodesic RK4 integrator (GPU compute shader) | ☐ |
| Adaptive step sizing | ☐ |
| Accretion disk (thin, Novikov-Thorne temp profile) | ☐ |
| Starfield (equirectangular HDRI) | ☐ |
| Relativistic aberration + Doppler shift | ☐ |
| Outer horizon crossing detection | ☐ |
| Inner horizon (Cauchy) crossing detection | ☐ |
| Whiteout flash on Cauchy crossing | ☐ |
| Ring singularity passage (r < 0) | ☐ |
| Negative universe mode (flipped starfield + sky) | ☐ |
| Debug grid (Boyer-Lindquist coordinates) | ☐ |
| Geodesic trail rendering | ☐ |
| Photon sphere visualization | ☐ |
| Unique color palette (no blue/purple except Doppler) | ☐ |
| 30+ FPS at 1080p on mid-range GPU | ☐ |
| Cross-platform (Windows/macOS/Linux tested) | ☐ |

---

## 18. WHAT PHYSICISTS WILL NOTICE

✅ "They used the full Kerr metric — not Schwarzschild with rotation faked"  
✅ "The Cauchy horizon flash is physically accurate (infinite blueshift)"  
✅ "They included frame dragging — stars warp around the black hole correctly"  
✅ "Adaptive step sizing near horizons — they understand the numerical challenges"  
✅ "The negative universe isn't just a gimmick — it's predicted by the math"  
✅ "They rendered the ring singularity without crashing the integrator"

---

## 19. SAMPLE OUTPUT DESCRIPTIONS (What user sees)

### Phase 1: Approach (r = 10M → 3M)
- Accretion disk is a warped double arc above and below due to gravitational lensing
- Stars near the black hole are smeared into Einstein rings
- Frame dragging is visible as stars rotate around the black hole as you approach

### Phase 2: Crossing Outer Horizon (r = ~2.0M)
- The outside universe shrinks to a bright blue-shifted circle behind you
- The accretion disk becomes a blinding ring around your entire field of view
- Space begins to twist violently — grid lines (if enabled) become helices

### Phase 3: Between Horizons (2M > r > ~1.2M)
- Light echoes: you see multiple copies of the same star from different orbital paths
- Time dilation is extreme — outside universe appears frozen or moving very slowly
- The inner horizon glows faintly ahead (Eruption Red)

### Phase 4: Crossing Inner Horizon (r = ~1.2M)
- BLINDING WHITE FLASH (entire screen white for 0.2 seconds)
- Sound: high-pitched sweep upward
- The flash fades, and you're in a different region of spacetime

### Phase 5: Inside the Ring (r near 0)
- The sky is no longer black — it's a deep inverted void (#1a0f0a)
- The accretion disk is gone (you passed inside it)
- A thin glowing ring (the singularity) is visible ahead — you pass through it

### Phase 6: Negative Universe (r < 0)
- You emerge into a starfield that looks almost like our own — but subtly wrong
- Stars are in slightly different positions (different universe)
- The accretion disk is now behind you, visible through a white hole horizon
- Gravity now repels you from r = -∞ (the "other side")

---

# 🚀 READY TO BUILD

This is the most ambitious of the three projects. Start with a working OpenGL + ImGui scaffold, then add the Kerr metric integrator, then the accretion disk, then the horizon crossing logic.

**First step:** Set up GLFW + OpenGL 4.6 + ImGui and get a rotating cube on screen (to prove rendering works). Then replace the cube with the compute shader ray tracer.
