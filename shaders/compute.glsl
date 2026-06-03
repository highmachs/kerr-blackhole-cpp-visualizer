#version 460 core
// ============================================================================
// Kerr Black Hole Interior Navigator — compute shader v2.
//
// FULL Boyer-Lindquist null geodesic integrator (spec §6).
//
//   State: (r, theta, phi, p_r, p_theta) — 5D phase space.
//   Conserved: E = -p_t, L = p_phi, Q = Carter constant.
//
//   * Initial photon 4-momentum is built in the ZAMO orthonormal tetrad,
//     so relativistic aberration is intrinsic (no separate hack).
//   * Hamilton's equations integrated with RK4 + adaptive step.
//   * ∂H/∂r and ∂H/∂θ via central finite differences (cheap and stable).
//   * r is permitted to go negative — the camera can ride through the ring
//     singularity into the negative-r universe.
//   * Grid overlay is gated by u_showGrid.
//   * Negative-universe starfield uses an offset / mirrored sampling so the
//     sky is *visibly* a different universe, not a tint of ours.
// ============================================================================

// 16x16 = 256 threads/group — sweet spot for Ada (RTX 4060) SMs.
layout(local_size_x = 16, local_size_y = 16) in;
layout(rgba16f, binding = 0) uniform writeonly image2D u_outImage;

uniform int   u_width;
uniform int   u_height;
uniform int   u_maxSteps;
uniform int   u_mode;             // 0 NORMAL, 1 BETWEEN, 2 INSIDE, 3 NEGATIVE
uniform int   u_showPhotonSphere;
uniform int   u_showGrid;
uniform float u_a;
uniform float u_M;
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
const vec3 C_DEEP_VOID          = vec3(10.0,   8.0,  7.0) / 255.0;
const vec3 C_PLASMA_CORE        = vec3(255.0, 77.0, 26.0) / 255.0;
const vec3 C_MOLTEN_IRON        = vec3(232.0,122.0, 58.0) / 255.0;
const vec3 C_COOLING_EMBER      = vec3(184.0, 92.0, 42.0) / 255.0;
const vec3 C_DRIED_CLAY         = vec3(168.0,107.0, 74.0) / 255.0;
const vec3 C_EVENT_HORIZON_GLOW = vec3(255.0,106.0, 42.0) / 255.0;
const vec3 C_DOPPLER_SAPPHIRE   = vec3( 46.0, 90.0,125.0) / 255.0;
const vec3 C_DOPPLER_RUBY       = vec3(125.0, 46.0, 46.0) / 255.0;
const vec3 C_BURNT_SIENNA       = vec3(201.0,126.0, 90.0) / 255.0;
const vec3 C_WEATHERED_BRONZE   = vec3(138.0,110.0, 74.0) / 255.0;
const vec3 C_INVERTED_VOID      = vec3( 26.0, 15.0, 10.0) / 255.0;

const float PI      = 3.14159265358979323846;
const float HALF_PI = 1.57079632679489661923;

float rOuter() { return u_M + sqrt(max(0.0, u_M*u_M - u_a*u_a)); }
float rInner() { return u_M - sqrt(max(0.0, u_M*u_M - u_a*u_a)); }

// ----- Kerr metric scalars at (r, θ) ---------------------------------------
struct K { float Delta; float Sigma; float A; float alpha; float omega; };

K kerrAt(float r, float th) {
    float a = u_a;
    float s = sin(th), c = cos(th);
    float sin2 = s*s, cos2 = c*c;
    K k;
    k.Delta = r*r - 2.0*u_M*r + a*a;
    k.Sigma = r*r + a*a*cos2;
    k.A     = (r*r + a*a)*(r*r + a*a) - a*a*k.Delta*sin2;
    k.alpha = sqrt(max(1e-8, k.Sigma * abs(k.Delta) / max(k.A, 1e-8)));
    k.omega = (2.0 * u_M * a * r) / max(k.A, 1e-8);
    return k;
}

// Hamiltonian for null geodesic: H = (1/2) g^{μν} p_μ p_ν.
// We need H(r, θ; p_r, p_θ; E, L), then ∂H/∂r and ∂H/∂θ via finite diff.
// Inverse metric components (Boyer-Lindquist, null part):
//   g^{tt}   = -A / (Σ Δ)
//   g^{tφ}   = -2 a M r / (Σ Δ)
//   g^{φφ}   = (Δ - a² sin²θ) / (Σ Δ sin²θ)
//   g^{rr}   = Δ / Σ
//   g^{θθ}   = 1 / Σ
//
// With p_t = -E, p_φ = L:
//   2H = g^{rr} p_r² + g^{θθ} p_θ² + g^{tt} E² - 2 g^{tφ} E L + g^{φφ} L²

float hamiltonian(float r, float th, float pr, float pth, float E, float L) {
    K k = kerrAt(r, th);
    float s2 = max(sin(th)*sin(th), 1e-6);
    float invSD = 1.0 / max(k.Sigma * k.Delta, 1e-8);
    float gtt   = -k.A * invSD;
    float gtphi = -2.0 * u_a * u_M * r * invSD;
    float gphph = (k.Delta - u_a*u_a*s2) / (k.Sigma * k.Delta * s2 + 1e-8);
    float grr   = k.Delta / max(k.Sigma, 1e-8);
    float gthth = 1.0 / max(k.Sigma, 1e-8);
    return 0.5 * (grr*pr*pr + gthth*pth*pth
                  + gtt*E*E - 2.0*gtphi*E*L + gphph*L*L);
}

// Hamilton's equations.  q̇ = ∂H/∂p, ṗ = -∂H/∂q.
// dt/dλ and dφ/dλ written in closed form (cheaper than diff in E, L).
struct State { float r; float th; float ph; float pr; float pth; };

State derive(State s, float E, float L) {
    K k = kerrAt(s.r, s.th);
    float s2 = max(sin(s.th)*sin(s.th), 1e-6);
    float invSD = 1.0 / max(k.Sigma * k.Delta, 1e-8);

    // q̇
    float dr_ = (k.Delta / max(k.Sigma, 1e-8)) * s.pr;
    float dth = s.pth / max(k.Sigma, 1e-8);
    // dφ/dλ = ∂H/∂L = -g^{tφ} E + g^{φφ} L
    float gtphi = -2.0 * u_a * u_M * s.r * invSD;
    float gphph = (k.Delta - u_a*u_a*s2) / (k.Sigma * k.Delta * s2 + 1e-8);
    float dph   = -gtphi * E + gphph * L;

    // ṗ via central differences.
    float hR = max(1e-3, 0.001 * abs(s.r) + 1e-3);
    float hT = 1e-3;
    float dHdr  = (hamiltonian(s.r + hR, s.th, s.pr, s.pth, E, L)
                  - hamiltonian(s.r - hR, s.th, s.pr, s.pth, E, L)) / (2.0 * hR);
    float dHdth = (hamiltonian(s.r, s.th + hT, s.pr, s.pth, E, L)
                  - hamiltonian(s.r, s.th - hT, s.pr, s.pth, E, L)) / (2.0 * hT);

    State d;
    d.r   = dr_;
    d.th  = dth;
    d.ph  = dph;
    d.pr  = -dHdr;
    d.pth = -dHdth;
    return d;
}

State rk4(State s, float h, float E, float L) {
    State k1 = derive(s, E, L);
    State s2; s2.r=s.r+0.5*h*k1.r; s2.th=s.th+0.5*h*k1.th; s2.ph=s.ph+0.5*h*k1.ph; s2.pr=s.pr+0.5*h*k1.pr; s2.pth=s.pth+0.5*h*k1.pth;
    State k2 = derive(s2, E, L);
    State s3; s3.r=s.r+0.5*h*k2.r; s3.th=s.th+0.5*h*k2.th; s3.ph=s.ph+0.5*h*k2.ph; s3.pr=s.pr+0.5*h*k2.pr; s3.pth=s.pth+0.5*h*k2.pth;
    State k3 = derive(s3, E, L);
    State s4; s4.r=s.r+h*k3.r; s4.th=s.th+h*k3.th; s4.ph=s.ph+h*k3.ph; s4.pr=s.pr+h*k3.pr; s4.pth=s.pth+h*k3.pth;
    State k4 = derive(s4, E, L);
    State o;
    o.r   = s.r   + (h/6.0)*(k1.r   + 2.0*k2.r   + 2.0*k3.r   + k4.r);
    o.th  = s.th  + (h/6.0)*(k1.th  + 2.0*k2.th  + 2.0*k3.th  + k4.th);
    o.ph  = s.ph  + (h/6.0)*(k1.ph  + 2.0*k2.ph  + 2.0*k3.ph  + k4.ph);
    o.pr  = s.pr  + (h/6.0)*(k1.pr  + 2.0*k2.pr  + 2.0*k3.pr  + k4.pr);
    o.pth = s.pth + (h/6.0)*(k1.pth + 2.0*k2.pth + 2.0*k3.pth + k4.pth);
    return o;
}

// Adaptive step (spec §11).
float adaptive(float r) {
    float h = rOuter();
    float ar = abs(r);
    if (ar > h + 2.0) return 0.08;
    if (ar > h)       return 0.02;
    if (ar > 0.5)     return 0.008;
    return 0.002;
}

// ---------------------------------------------------------------------------
// Accretion disk (spec §8): equatorial plane θ=π/2, r ∈ [r_ISCO, 15 M].
// ---------------------------------------------------------------------------
vec3 diskTint(float r) {
    float rIn  = 1.5 * u_M;
    float rOut = 15.0 * u_M;
    float t = clamp((r - rIn) / (rOut - rIn), 0.0, 1.0);
    vec3 c;
    if      (t < 0.25) c = mix(C_PLASMA_CORE,   C_MOLTEN_IRON,   t / 0.25);
    else if (t < 0.60) c = mix(C_MOLTEN_IRON,   C_COOLING_EMBER, (t - 0.25) / 0.35);
    else               c = mix(C_COOLING_EMBER, C_DRIED_CLAY,    (t - 0.60) / 0.40);
    // Novikov-Thorne T(r) ~ r^(-3/4) -> intensity factor.
    float intensity = pow(rIn / max(r, rIn), 0.75);
    return c * (0.6 + 1.4 * intensity);
}

// Disk crossing test between (θ0, r0) and (θ1, r1) in current step.
bool crossedDisk(float th0, float r0, float th1, float r1, out float rHit) {
    float c0 = cos(th0), c1 = cos(th1);
    if (c0 * c1 >= 0.0) return false;
    float a = c0 / (c0 - c1);
    rHit = mix(r0, r1, a);
    if (rHit < 1.5 * u_M || rHit > 15.0 * u_M) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Starfield with mode-dependent sampling (spec §9, §7 negative-universe).
// ---------------------------------------------------------------------------
vec3 starSample(vec3 dir, int mode) {
    float u = 0.5 + 0.5 * atan(dir.z, dir.x) / PI;
    float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / PI;
    // Negative universe: rotated + mirrored sampling = visibly different sky.
    if (mode == 3) {
        u = fract(u + 0.37);
        v = 1.0 - v;
    } else if (mode == 2) {
        // INSIDE between horizons: time-frozen, drag sampling.
        u = fract(u + 0.13 * sin(u_time * 0.05));
    }
    vec3 col = texture(u_starfield, vec2(u, v)).rgb;
    if (u_starfieldIsFile < 0.5) {
        float b = max(max(col.r, col.g), col.b);
        col = step(0.6, b) * col * 3.0;
    }
    return col * u_starBrightness;
}

// Frequency-shift factor g (full relativistic, includes aberration via dir
// already being in observer's tetrad).  baseColor tinted toward sapphire
// when blueshifted, ruby when redshifted, intensity ~ g^4 (blackbody).
vec3 applyG(vec3 baseColor, float g) {
    vec3 tinted;
    if (g > 1.0) tinted = mix(baseColor, baseColor * C_DOPPLER_SAPPHIRE * 1.8, clamp(g - 1.0, 0.0, 1.0));
    else         tinted = mix(baseColor, baseColor * C_DOPPLER_RUBY     * 1.8, clamp(1.0 - g, 0.0, 1.0));
    return tinted * pow(g, 4.0);
}

// ---------------------------------------------------------------------------
// Coordinate grid (spec §10) — gated by u_showGrid and only visible outside
// the outer horizon (interior is wildly distorted per spec).
// ---------------------------------------------------------------------------
vec3 gridOverlay(float r, float th, float ph) {
    if (u_showGrid == 0)            return vec3(0);
    if (u_r < rOuter())             return vec3(0);   // hide inside horizon
    if (abs(r) > 30.0)              return vec3(0);
    float g = 0.0; vec3 tint = vec3(0);
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
    return tint * g * 0.55;
}

vec3 photonSphereGlow(float r, float th) {
    if (u_showPhotonSphere == 0 || u_mode != 0) return vec3(0);
    float rps = 2.5 * u_M;
    float d = abs(r - rps) + abs(th - HALF_PI) * 0.5;
    return C_EVENT_HORIZON_GLOW * exp(-25.0 * d) * 0.6;
}

// ---------------------------------------------------------------------------
// ZAMO tetrad — used to initialize the photon 4-momentum.  This is what
// gives us *aberration* for free: the user-rotated ray direction is built
// in the orthonormal local frame of the ZAMO, then mapped to coordinate
// 4-momentum, and the geodesic carries those tetrad-frame angles to the
// emitter naturally.
// ---------------------------------------------------------------------------
void initPhoton(vec3 nHat, float r, float th,
                out State s, out float E, out float L)
{
    K k = kerrAt(r, th);
    float sinT = max(sin(th), 1e-3);
    // Tetrad legs (coord-basis components of orthonormal frame):
    //   e_t̂^t   = 1/α            e_t̂^φ   = ω/α
    //   e_r̂^r   = √(Δ/Σ)
    //   e_θ̂^θ   = 1/√Σ
    //   e_φ̂^φ   = √(Σ/A)/sinθ
    float Et   = 1.0 / k.alpha;
    float Etp  = k.omega / k.alpha;
    float Er   = sqrt(max(k.Delta, 1e-6) / max(k.Sigma, 1e-6));
    float Eth  = 1.0 / sqrt(max(k.Sigma, 1e-6));
    float Ephp = sqrt(max(k.Sigma, 1e-6) / max(k.A, 1e-6)) / sinT;

    // Local-frame photon 4-momentum: p^t̂=1, p^î = n̂_i.  Convert to coord:
    float pt = Et;
    float pr = Er  * nHat.x;
    float pth = Eth * nHat.y;
    float pph = Etp + Ephp * nHat.z;

    // Lower indices via metric to get conserved (E, L):
    //   E = -p_t = (Σ-2Mr)/Σ × p^t + 2Mr a sin²θ / Σ × p^φ
    //   L =  p_φ = -(2Mr a sin²θ / Σ) × p^t + (sin²θ/Σ) × A × p^φ
    float Sig = max(k.Sigma, 1e-6);
    float sin2 = sinT * sinT;
    float gtt =  (1.0 - 2.0*u_M*r/Sig);    //  -(component); we want -p_t
    float gtp =  -2.0*u_M*u_a*r*sin2/Sig;
    float gpp =  (k.A * sin2) / Sig;       // actually (r²+a²+2Ma²r sin²θ/Σ) sin²θ
    E =   gtt * pt - gtp * pph;
    L =  -gtp * pt + gpp * pph;

    // Lower p_r, p_θ:
    s.r   = r; s.th = th; s.ph = u_phi;
    s.pr  = (Sig / max(k.Delta, 1e-6)) * pr;
    s.pth = Sig * pth;
}

// ---------------------------------------------------------------------------
void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    if (px.x >= u_width || px.y >= u_height) return;

    vec2 uv = (vec2(px) + 0.5) / vec2(u_width, u_height) * 2.0 - 1.0;
    float aspect = float(u_width) / float(u_height);
    uv.x *= aspect;

    // Ray direction in camera frame -> ZAMO-tetrad direction.
    // u_camBasis carries the user's yaw/pitch from the host.
    vec3 rayCam = normalize(vec3(uv, -1.4));
    vec3 nHat   = normalize((u_camBasis * vec4(rayCam, 0.0)).xyz);

    State s; float E, L;
    initPhoton(nHat, u_r, u_theta, s, E, L);

    vec3 accum = vec3(0.0);
    bool terminated = false;

    int steps = u_maxSteps;
    for (int i = 0; i < steps; ++i) {
        float h = adaptive(s.r);
        // Backward integration: λ runs from observer to emitter, so step
        // sign is positive; Hamilton's equations already encode direction.
        State prev = s;
        s = rk4(s, h, E, L);

        // --- Disk intersection? ---
        float rHit;
        if (crossedDisk(prev.th, prev.r, s.th, s.r, rHit)) {
            vec3 col = diskTint(abs(rHit)) * u_diskBrightness;
            // Local g factor: ratio of photon energy at emitter to observer.
            // For a Keplerian disk emitter, approximate g via prograde
            // angular velocity Ω = M^{1/2} / (r^{3/2} + a M^{1/2}).
            float rr = max(abs(rHit), 0.5);
            float Om = 1.0 / (pow(rr, 1.5) + u_a);
            float g  = 1.0 / (1.0 + Om * (L - u_a*E)/(E + 1e-6));
            g = clamp(g, 0.2, 2.5);
            accum = applyG(col, g);
            terminated = true;
            break;
        }

        // --- Escaped to infinity? ---
        if (abs(s.r) > 50.0) {
            vec3 dir = normalize(vec3(sin(s.th)*cos(s.ph),
                                      cos(s.th),
                                      sin(s.th)*sin(s.ph)));
            int starMode = (u_r < 0.0) ? 3 : u_mode;
            vec3 col = starSample(dir, starMode);
            // g for far observer ~ α(camera).  Simple approximation:
            K kc = kerrAt(u_r, u_theta);
            float g = (u_r > rOuter()) ? (1.0 / max(kc.alpha, 1e-3)) : 1.0;
            accum = applyG(col, g);
            terminated = true;
            break;
        }

        // --- Ring singularity? r→0, θ→π/2 ---
        if (abs(s.r) < 0.03 && abs(s.th - HALF_PI) < 0.05) {
            accum = vec3(2.5, 1.6, 1.1);
            terminated = true;
            break;
        }

        // Horizon damping: rays inside outer horizon lose energy.
        if (s.r > 0.0 && s.r < rOuter() && u_r > rOuter()) {
            // From outside looking in, the horizon is dark.
            accum = vec3(0.0);
            terminated = true;
            break;
        }

        accum += photonSphereGlow(s.r, s.th) * 0.025;
        accum += gridOverlay(s.r, s.th, s.ph) * 0.04;
    }

    if (!terminated) {
        accum = (u_mode == 3) ? C_INVERTED_VOID : C_DEEP_VOID;
    }

    // Mode tint.
    if (u_mode == 3)       accum = mix(accum, accum * vec3(1.05, 0.9, 0.78), 0.45);
    else if (u_mode == 2)  accum *= vec3(1.10, 0.96, 0.86);
    else if (u_mode == 1)  accum *= vec3(1.05, 0.99, 0.94);

    imageStore(u_outImage, px, vec4(accum, 1.0));
}
