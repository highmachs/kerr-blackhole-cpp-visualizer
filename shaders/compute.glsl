#version 460 core
// ============================================================================
// Kerr Black Hole Interior Navigator — Compute Shader (GLSL 4.60)
// High-Performance Runge-Kutta / Verlet Null Geodesic Raytracing Pipeline
//
// Optimized for RTX 4060 & Modern GPUs:
//   - Precomputed invariant uniforms (u_a2, u_M2, u_rOuter, u_rInner, u_rISCO, u_rPhoton)
//   - Direct scalar multiply chains (eliminated transcendental pow() in kerrAccel)
//   - Zero redundant kerrR evaluations in RK2 Verlet step
//   - Branch-divergence isolation for Negative Universe features
//   - Conditional acos/atan evaluation (omitted when grid/photon sphere is off)
//   - Squared-radius distance checks (eliminated per-step sqrt in length)
// ============================================================================

layout(local_size_x = 16, local_size_y = 16) in;
layout(rgba16f, binding = 0) uniform writeonly image2D u_outImage;

uniform int   u_width;
uniform int   u_height;
uniform int   u_maxSteps;
uniform int   u_mode;             // 0 NORMAL, 1 BETWEEN, 2 INSIDE, 3 NEGATIVE
uniform int   u_showPhotonSphere;
uniform int   u_showGrid;
uniform int   u_diskStyle;        // 0 Classic, 1 High-Energy Sapphire, 2 Naked Lens, 3 Doppler Heatmap
uniform int   u_showReticle;      // 0 off, 1 on
uniform float u_a;
uniform float u_a2;
uniform float u_M;
uniform float u_M2;
uniform float u_rOuter;
uniform float u_rInner;
uniform float u_rISCO;
uniform float u_rPhoton;
uniform float u_time;
uniform float u_diskBrightness;
uniform float u_starBrightness;
uniform float u_starfieldIsFile;

uniform float u_r;
uniform float u_theta;
uniform float u_phi;
uniform float u_dr;
uniform float u_dphi;
uniform mat4  u_camBasis;

uniform sampler2D u_starfield;

// --- Palette (spec §4) ------------------------------------------------------
const vec3 C_DEEP_VOID          = vec3(10.0,   8.0,   7.0) / 255.0; // #0a0807
const vec3 C_PLASMA_CORE        = vec3(255.0, 77.0,  26.0) / 255.0; // #ff4d1a
const vec3 C_MOLTEN_IRON        = vec3(232.0,122.0,  58.0) / 255.0; // #e87a3a
const vec3 C_COOLING_EMBER      = vec3(184.0, 92.0,  42.0) / 255.0; // #b85c2a
const vec3 C_DRIED_CLAY         = vec3(168.0,107.0,  74.0) / 255.0; // #a86b4a
const vec3 C_EVENT_HORIZON_GLOW = vec3(255.0,106.0,  42.0) / 255.0; // #ff6a2a
const vec3 C_DOPPLER_SAPPHIRE   = vec3( 46.0, 90.0, 125.0) / 255.0; // #2e5a7d
const vec3 C_DOPPLER_RUBY       = vec3(125.0, 46.0,  46.0) / 255.0; // #7d2e2e
const vec3 C_BURNT_SIENNA       = vec3(201.0,126.0,  90.0) / 255.0; // #c97e5a
const vec3 C_WEATHERED_BRONZE   = vec3(138.0,110.0,  74.0) / 255.0; // #8a6e4a
const vec3 C_INVERTED_VOID      = vec3( 26.0, 15.0,  10.0) / 255.0; // #1a0f0a
const vec3 C_WARM_ASH           = vec3(196.0,181.0, 165.0) / 255.0; // #c4b5a5

const float PI      = 3.14159265358979323846;
const float HALF_PI = 1.57079632679489661923;

float rOuter()  { return u_rOuter; }
float rInner()  { return u_rInner; }
float rISCO()   { return u_rISCO; }
float rPhoton() { return u_rPhoton; }

// Convert Cartesian position to Kerr Boyer-Lindquist radial coordinate r
float kerrR(vec3 p, bool isNegative) {
    float rho2 = dot(p, p);
    float D = rho2 - u_a2;
    float r2 = 0.5 * (D + sqrt(max(0.0, D * D + 4.0 * u_a2 * p.z * p.z)));
    float r = sqrt(max(0.0, r2));
    return isNegative ? -r : r;
}

// Kerr geodesic acceleration (lensing + frame-dragging)
// Optimized: uses scalar products instead of pow()
vec3 kerrAccel(vec3 p, vec3 v, float r, bool isNegative) {
    float ar = max(0.08, abs(r));
    float ar2 = ar * ar;
    float ar5 = ar2 * ar2 * ar;
    float rho2 = dot(p, p);

    vec3 Lvec = cross(p, v);
    float L2 = dot(Lvec, Lvec);

    // Relativistic photon deflection
    float gFactor = isNegative ? -1.0 : 1.0;
    float aMag = gFactor * (3.0 * u_M * L2 / (ar5 + 0.04));
    vec3 a_grav = -aMag * p;

    // Frame-dragging Coriolis acceleration around z-axis
    float denom = ar2 + u_a2;
    float omega = (2.0 * u_M * u_a * ar) / (denom * denom + 0.08);
    vec3 wVec = vec3(0.0, 0.0, omega * gFactor);
    vec3 a_drag = 2.0 * cross(v, wVec);

    return a_grav + a_drag;
}

// Novikov-Thorne accretion disk color profile (spec §8)
vec3 diskColor(float r) {
    float rIn = u_rISCO;
    float rOut = 15.0 * u_M;
    float t = clamp((r - rIn) / (rOut - rIn + 1e-4), 0.0, 1.0);
    vec3 c;
    if      (t < 0.18) c = mix(C_PLASMA_CORE,   C_MOLTEN_IRON,   t / 0.18);
    else if (t < 0.50) c = mix(C_MOLTEN_IRON,   C_COOLING_EMBER, (t - 0.18) / 0.32);
    else               c = mix(C_COOLING_EMBER, C_DRIED_CLAY,    (t - 0.50) / 0.50);

    float rRatio = rIn / max(r, rIn);
    float sqrtRRatio = sqrt(rRatio);
    float intensity = pow(rRatio, 0.75) * pow(max(0.0, 1.0 - sqrtRRatio), 0.25) * 2.2 + 0.40;
    return c * intensity;
}

// Relativistic Doppler boosting and frequency color shifting
vec3 applyDoppler(vec3 baseCol, float g) {
    vec3 tinted;
    if (g > 1.0) tinted = mix(baseCol, baseCol * C_DOPPLER_SAPPHIRE * 1.8, clamp(g - 1.0, 0.0, 1.0));
    else         tinted = mix(baseCol, baseCol * C_DOPPLER_RUBY     * 1.8, clamp(1.0 - g, 0.0, 1.0));
    return tinted * clamp(pow(g, 3.2), 0.08, 6.0);
}

// Celestial sphere starfield sampling with relativistic modes
vec3 sampleStars(vec3 dir, int mode) {
    float u = 0.5 + 0.5 * atan(dir.y, dir.x) / PI;
    float v = 0.5 - asin(clamp(dir.z, -1.0, 1.0)) / PI;
    if (mode == 3) {
        // Negative universe: mirrored & rotated sky (spec §7)
        u = fract(u + 0.37);
        v = 1.0 - v;
    } else if (mode == 2) {
        u = fract(u + 0.10 * sin(u_time * 0.05));
    }
    vec3 col = texture(u_starfield, vec2(u, v)).rgb;
    return col * u_starBrightness;
}

// Coordinate grid lines in Boyer-Lindquist coordinates (spec §10)
vec3 coordinateGrid(float r, float th, float ph) {
    if (u_showGrid == 0) return vec3(0.0);
    if (u_r <= u_rOuter) return vec3(0.0);
    if (abs(r) > 28.0) return vec3(0.0);

    float g = 0.0;
    vec3 tint = vec3(0.0);

    float rings[5] = float[5](3.0, 5.0, 7.0, 10.0, 15.0);
    for (int i = 0; i < 5; ++i) {
        float w = exp(-40.0 * abs(r - rings[i] * u_M));
        if (w > g) { g = w; tint = C_BURNT_SIENNA; }
    }
    float thetas[5] = float[5](PI/6.0, PI/3.0, HALF_PI, 2.0*PI/3.0, 5.0*PI/6.0);
    for (int i = 0; i < 5; ++i) {
        float w = 0.7 * exp(-60.0 * abs(th - thetas[i]));
        if (w > g) { g = w; tint = C_DRIED_CLAY; }
    }
    for (int i = 0; i < 8; ++i) {
        float p_i = -PI + (float(i) / 8.0) * 2.0 * PI;
        float w = 0.6 * exp(-80.0 * abs(ph - p_i));
        if (w > g) { g = w; tint = C_WEATHERED_BRONZE; }
    }
    return tint * g * 0.6;
}

// Photon sphere glowing ring
vec3 photonSphereGlow(float r, float th) {
    if (u_showPhotonSphere == 0 || u_mode != 0) return vec3(0.0);
    float d = abs(r - u_rPhoton) + abs(th - HALF_PI) * 0.4;
    return C_EVENT_HORIZON_GLOW * exp(-30.0 * d) * 0.7;
}

// ---------------------------------------------------------------------------
void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    if (px.x >= u_width || px.y >= u_height) return;

    vec2 uv = (vec2(px) + 0.5) / vec2(u_width, u_height) * 2.0 - 1.0;
    float aspect = float(u_width) / float(u_height);
    uv.x *= aspect;

    // Observer camera position in Cartesian coordinates
    float sinTh0 = sin(u_theta);
    float cosTh0 = cos(u_theta);
    vec3 camPos = vec3(
        u_r * sinTh0 * cos(u_phi),
        u_r * sinTh0 * sin(u_phi),
        u_r * cosTh0
    );

    // Ray direction in local camera space with wide-angle FOV
    vec3 camRay = normalize(vec3(uv.x, uv.y, -1.0));
    // Transform directly to world coordinates using camera basis
    vec3 rayDir = normalize((u_camBasis * vec4(camRay, 0.0)).xyz);

    vec3 pos = camPos;
    vec3 vel = rayDir;
    bool isNegative = (u_r < 0.0);

    vec3 accum = vec3(0.0);
    float transmission = 1.0;
    int steps = clamp(u_maxSteps, 30, 800);

    float ro = u_rOuter;
    float ri = u_rInner;
    float r = u_r;
    float ar = abs(u_r);

    for (int i = 0; i < steps; ++i) {
        // Adaptive step size based on radial proximity
        float h = 0.05;
        if (ar > 18.0 && dot(pos, vel) > 0.0) h = 0.60;
        else if (ar > 14.0) h = 0.35;
        else if (ar > 7.0)  h = 0.18;
        else if (ar > 3.5)  h = 0.08;
        else if (ar > ro + 0.4) h = 0.035;
        else if (ar > ro)       h = 0.018;
        else if (ar > ri)       h = 0.012;
        else if (ar > 0.25)     h = 0.008;
        else                    h = 0.002;

        vec3 prevPos = pos;
        vec3 prevVel = vel;

        // Velocity Verlet / RK2 integration
        vec3 a1 = kerrAccel(pos, vel, r, isNegative);
        vec3 nextPos = pos + vel * h + 0.5 * a1 * h * h;
        float nextR = kerrR(nextPos, isNegative);
        vec3 a2 = kerrAccel(nextPos, vel + a1 * h, nextR, isNegative);
        vel = normalize(vel + 0.5 * (a1 + a2) * h);
        pos = nextPos;
        r = nextR; // Reused directly without redundant kerrR evaluation

        // Ring singularity passage: crossing z=0 inside ring radius a (spec §2, §7)
        if (prevPos.z * pos.z <= 0.0 && (pos.x * pos.x + pos.y * pos.y) < u_a2) {
            isNegative = !isNegative;
            r = -r;
        }
        ar = abs(r);

        // --- 1. Accretion disk analytic plane crossing (z = 0, r in [rISCO, 15M]) ---
        if (prevPos.z * pos.z <= 0.0 && !isNegative) {
            float tCross = clamp(-prevPos.z / (pos.z - prevPos.z + 1e-7), 0.0, 1.0);
            vec3 pDisk = mix(prevPos, pos, tCross);
            float rDisk = length(pDisk.xy);

            if (rDisk >= u_rISCO && rDisk <= 15.0 * u_M) {
                // Keplerian prograde plasma orbital frequency & velocity
                float Om = sqrt(u_M) / (pow(rDisk, 1.5) + u_a * sqrt(u_M));
                vec3 vPlasmaDir = normalize(vec3(-pDisk.y, pDisk.x, 0.0));
                // Photon traveled towards camera (opposite of backward ray velocity)
                vec3 vPhotDir = -normalize(pos - prevPos);
                float beta = dot(vPhotDir, vPlasmaDir);
                float vOrb = clamp(Om * rDisk, 0.0, 0.85);

                float gamma = 1.0 / sqrt(max(0.01, 1.0 - vOrb * vOrb));
                float alphaGrav = sqrt(max(0.02, 1.0 - 2.0 * u_M * rDisk / (rDisk * rDisk + u_a2)));
                float g = alphaGrav / (gamma * max(0.08, 1.0 - beta * vOrb));
                g = clamp(g, 0.20, 2.80);

                // Gas density & turbulent spiral structure with Keplerian shear
                float ang = atan(pDisk.y, pDisk.x);
                float shearAngle = ang - Om * 6.0 * u_time;
                float logR = log(rDisk);
                float spiralPattern = sin(8.0 * shearAngle + 3.0 * logR);
                float turb = 0.84 + 0.10 * sin(24.0 * logR) + 0.06 * spiralPattern;

                // Accretion disk style options:
                // 2 = Naked Lens mode (skip disk completely to view pure gravitational lens)
                if (u_diskStyle != 2) {
                    vec3 radiated;
                    if (u_diskStyle == 1) {
                        // High-Energy Electric Sapphire / Violet Plasma
                        vec3 plasmaCol = mix(vec3(0.15, 0.65, 1.00), vec3(0.58, 0.22, 0.95), clamp((rDisk - u_rISCO) / (10.0 * u_M), 0.0, 1.0));
                        radiated = applyDoppler(plasmaCol, g) * turb * u_diskBrightness;
                    } else if (u_diskStyle == 3) {
                        // Doppler Energy Heatmap:
                        // Blueshift (approaching): Deep Sapphire -> Cyan
                        // Neutral (g ~ 1.0): Emerald
                        // Redshift (receding / gravitational): Amber -> Crimson Ruby
                        vec3 heatCol;
                        if (g > 1.35)      heatCol = mix(vec3(0.10, 0.45, 1.00), vec3(0.20, 0.90, 1.00), clamp((g - 1.35) / 0.8, 0.0, 1.0));
                        else if (g > 1.08) heatCol = mix(vec3(0.15, 0.92, 0.88), vec3(0.20, 0.92, 0.25), clamp((g - 1.08) / 0.27, 0.0, 1.0));
                        else if (g > 0.82) heatCol = mix(vec3(0.20, 0.92, 0.25), vec3(1.00, 0.72, 0.15), clamp((1.08 - g) / 0.26, 0.0, 1.0));
                        else              heatCol = mix(vec3(1.00, 0.72, 0.15), vec3(0.85, 0.10, 0.10), clamp((0.82 - g) / 0.50, 0.0, 1.0));
                        radiated = heatCol * turb * u_diskBrightness;
                    } else {
                        // 0 = Classic Interstellar Novikov-Thorne disk with Doppler beaming
                        radiated = applyDoppler(diskColor(rDisk), g) * turb * u_diskBrightness;
                    }

                    // Semi-transparent gas layer creates iconic warped double arcs
                    float tau = 2.0 * exp(-0.15 * (rDisk - u_rISCO));
                    float alpha = clamp(1.0 - exp(-tau), 0.50, 0.88);

                    accum += radiated * alpha * transmission;
                    transmission *= (1.0 - alpha);

                    if (transmission < 0.03) {
                        break;
                    }
                }
            }
        }

        float posSq = dot(pos, pos);

        // --- 2. Escaped to infinity (normal universe) ---
        if (posSq > 576.0 * u_M2 && !isNegative && dot(pos, vel) > 0.0) {
            vec3 dir = normalize(vel);
            vec3 col = sampleStars(dir, u_mode);
            float vcam = clamp(abs(u_dr) * 0.4, 0.0, 0.7);
            float gStar = 1.0 / (sqrt(1.0 - vcam * vcam) * (1.0 + vcam * dot(vel, normalize(camPos))));
            accum += applyDoppler(col, gStar) * transmission;
            transmission = 0.0;
            break;
        }

        // --- 3. Escaped to negative universe celestial sphere ---
        if (posSq > 400.0 * u_M2 && isNegative && dot(pos, vel) > 0.0) {
            vec3 dir = normalize(vel);
            vec3 col = sampleStars(dir, 3);
            accum += col * transmission;
            transmission = 0.0;
            break;
        }

        // --- 4. Relativistic Structures & Singularity ---
        float rho = length(pos.xy);

        if (isNegative) {
            // A. Relativistic Polar Expulsion Jets (Negative Universe only)
            if (rho < 0.60 * u_a && abs(pos.z) < 6.0) {
                float jetCore = exp(-14.0 * rho / u_a);
                float jetZFalloff = exp(-0.6 * abs(pos.z)) * smoothstep(6.0, 2.5, abs(pos.z));
                float jetPulse = 0.85 + 0.15 * sin(12.0 * abs(pos.z) - 6.0 * u_time);
                vec3 jetCol = mix(vec3(0.20, 0.85, 1.00), vec3(1.0, 1.0, 1.0), jetCore);
                accum += jetCol * (jetCore * 2.4 * jetZFalloff * jetPulse) * 0.10 * transmission;
            }

            // B. Central Throat Aperture in Negative Universe
            if (abs(pos.z) < 0.50 && rho < u_a * 1.35) {
                float uThroat = clamp(rho / max(0.01, u_a), 0.0, 1.30);
                vec3 whiteHoleCol = mix(vec3(1.0, 1.0, 1.0), vec3(0.25, 0.90, 1.00), smoothstep(0.0, 0.55, uThroat));
                whiteHoleCol = mix(whiteHoleCol, vec3(0.75, 0.35, 1.00), smoothstep(0.55, 1.05, uThroat));
                float zFalloff = exp(-8.0 * abs(pos.z));
                float shimmer = 0.82 + 0.18 * sin(18.0 * rho - 4.5 * u_time);
                float haloGain = 5.8 * zFalloff * shimmer;
                accum += whiteHoleCol * haloGain * 0.22 * transmission;
            }

            // C. Radiant Expulsion Disk / Relativistic Photon Ring in Negative Universe
            if (abs(pos.z) < 0.50 && rho >= u_a * 0.7 && rho <= u_a * 4.5) {
                float zSmooth = smoothstep(0.50, 0.0, abs(pos.z));
                float rhoSmooth = smoothstep(0.7 * u_a, 1.1 * u_a, rho) * smoothstep(4.5 * u_a, 2.0 * u_a, rho);
                float rRel = (rho - u_a) / (3.5 * u_a);
                float diskFalloff = exp(-8.0 * abs(pos.z)) * exp(-1.5 * rRel) * zSmooth * rhoSmooth;
                float phNeg = atan(pos.y, pos.x);
                float ripples = 0.82 + 0.18 * sin(14.0 * rho - 3.0 * phNeg + 3.5 * u_time);
                vec3 expCol = mix(vec3(0.30, 0.88, 1.00), vec3(0.12, 0.40, 0.95), clamp(rRel, 0.0, 1.0));
                accum += expCol * (diskFalloff * ripples * 2.8) * 0.12 * transmission;
            }
        } else {
            // Central Throat Aperture in Positive Universe
            if (abs(pos.z) < 0.50 && rho < u_a * 1.35) {
                float uThroat = clamp(rho / max(0.01, u_a), 0.0, 1.30);
                vec3 whiteHoleCol = mix(vec3(1.0, 1.0, 1.0), vec3(0.25, 0.90, 1.00), smoothstep(0.0, 0.55, uThroat));
                whiteHoleCol = mix(whiteHoleCol, vec3(0.75, 0.35, 1.00), smoothstep(0.55, 1.05, uThroat));
                float zFalloff = exp(-8.0 * abs(pos.z));
                float shimmer = 0.82 + 0.18 * sin(18.0 * rho - 4.5 * u_time);
                float haloGain = 2.5 * zFalloff * shimmer;
                accum += whiteHoleCol * haloGain * 0.22 * transmission;
            }

            // --- 5. Horizons and interior glowing structures ---
            float dOuter = abs(ar - ro);
            accum += C_EVENT_HORIZON_GLOW * exp(-30.0 * dOuter) * 0.08 * transmission;

            // Viewed from outside: rays captured by outer horizon fall into black hole shadow
            if (u_r > ro && ar < ro) {
                accum += C_DEEP_VOID * transmission;
                transmission = 0.0;
                break;
            }

            // Viewed from between horizons: inner Cauchy horizon glows ahead
            if (u_mode == 1 || u_mode == 2) {
                float dInner = abs(ar - ri);
                accum += (C_EVENT_HORIZON_GLOW * 1.6 + C_PLASMA_CORE * 0.8) * exp(-18.0 * dInner) * 0.15 * transmission;
            }
        }

        // D. Ring Singularity Filament at rho = a
        float dRing = abs(rho - u_a) + abs(pos.z) * 1.5;
        if (dRing < 0.25) {
            vec3 ringCol = isNegative ? (vec3(1.0, 1.0, 1.0) * 4.5 + vec3(0.35, 0.90, 1.00) * 3.0)
                                      : (C_PLASMA_CORE * 2.8 + C_EVENT_HORIZON_GLOW * 1.8);
            accum += ringCol * exp(-15.0 * dRing) * transmission;
            if (dRing < 0.02) {
                transmission = 0.0;
                break;
            }
        }

        // --- 6. Coordinate grid & photon sphere overlays (evaluated only when enabled) ---
        if (u_showGrid != 0 || u_showPhotonSphere != 0) {
            float th = acos(clamp(pos.z / max(0.01, sqrt(posSq)), -1.0, 1.0));
            float ph = atan(pos.y, pos.x);
            accum += photonSphereGlow(ar, th) * 0.035 * transmission;
            accum += coordinateGrid(ar, th, ph) * 0.045 * transmission;
        }
    }

    if (transmission > 0.0) {
        if (isNegative) {
            accum += C_INVERTED_VOID * transmission;
        } else if (u_mode == 3) {
            // Looking from Negative Universe through the throat into positive universe interior
            vec3 throatGlow = vec3(0.06, 0.03, 0.12) + vec3(0.04, 0.22, 0.35) * exp(-2.0 * max(0.0, ar));
            accum += throatGlow * transmission;
        } else {
            accum += C_DEEP_VOID * transmission;
        }
    }

    // Atmospheric/universe mode tint (spec §4)
    if (u_mode == 3) {
        accum = mix(accum, C_INVERTED_VOID, 0.15);
    } else if (u_mode == 2) {
        accum *= vec3(1.10, 0.95, 0.85);
    } else if (u_mode == 1) {
        accum *= vec3(1.05, 0.98, 0.92);
    }

    // Cockpit sci-fi flight reticle overlay (perfect isotropic pixel alignment)
    if (u_showReticle == 1 && u_mode != 3) {
        vec2 p = (vec2(px) + 0.5 - 0.5 * vec2(u_width, u_height)) / float(u_height);
        float d = length(p);
        float ring = smoothstep(0.0035, 0.0, abs(d - 0.045));
        float ticks = 0.0;
        if (abs(p.x) < 0.0015 && abs(p.y) > 0.025 && abs(p.y) < 0.065) ticks = 1.0;
        if (abs(p.y) < 0.0015 && abs(p.x) > 0.025 && abs(p.x) < 0.065) ticks = 1.0;
        float pip = smoothstep(0.003, 0.0, d);
        float ladder = 0.0;
        if (abs(p.x) > 0.08 && abs(p.x) < 0.16 && abs(abs(p.y) - 0.08) < 0.0015) ladder = 0.7;

        vec3 reticleCol = vec3(1.0, 0.72, 0.20); // Warm amber reticle
        accum += reticleCol * (ring * 0.35 + ticks * 0.45 + pip * 0.55 + ladder * 0.25);
    }

    imageStore(u_outImage, px, vec4(accum, 1.0));
}
