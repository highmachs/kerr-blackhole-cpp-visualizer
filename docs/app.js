/**
 * Kerr Black Hole Interior Navigator - WebGL 2 Real-Time Engine
 * Full General Relativity Null Geodesic Raymarching in Browser
 */

// --- Global Spacetime & Camera State ---
const state = {
  // Spacetime parameters
  M: 1.0,
  a: 0.95,
  r: 14.0,
  theta: 1.35,
  phi: 0.0,
  dr: -0.15,
  dphi: 0.0,
  mode: 0, // 0: NORMAL, 1: BETWEEN, 2: INSIDE, 3: NEGATIVE
  whiteoutTimer: 0.0,

  // Camera orientation
  yaw: 0.0,
  pitch: 0.0,
  roll: 0.0,
  velocity: [0, 0, 0],
  thrustMultiplier: 1.0,
  isAutoOrbit: false,
  isTouring: false,
  tourTime: 0.0,
  simSpeed: 1.0,

  // Render settings
  maxSteps: 180,
  diskStyle: 0, // 0: Classic, 1: Sapphire, 2: Naked, 3: Doppler
  diskBrightness: 1.0,
  starBrightness: 0.85,
  bloomIntensity: 0.22,
  showReticle: true,
  showGrid: false,
  resPreset: '1080p',
  renderScale: 1.0,
  autoGpuRecovery: true,

  // Performance telemetry
  frameMsSmoothed: 16.6,
  lowFpsDuration: 0.0,
  gpuLagAlert: false,
  audioActive: false
};

// --- WebGL 2 Vertex Shader ---
const vsSource = `#version 300 es
in vec2 a_position;
out vec2 v_uv;
void main() {
    v_uv = a_position * 0.5 + 0.5;
    gl_Position = vec4(a_position, 0.0, 1.0);
}`;

// --- WebGL 2 Fragment Shader (Kerr Geodesic Raymarcher) ---
const fsSource = `#version 300 es
precision highp float;
precision highp int;

in vec2 v_uv;
out vec4 fragColor;

uniform vec2  u_resolution;
uniform float u_time;
uniform float u_a;
uniform float u_M;
uniform float u_r;
uniform float u_theta;
uniform float u_phi;
uniform float u_dr;
uniform mat4  u_camBasis;
uniform int   u_maxSteps;
uniform int   u_diskStyle;
uniform int   u_mode;
uniform float u_diskBrightness;
uniform float u_starBrightness;
uniform float u_whiteout;
uniform float u_bloom;
uniform int   u_showGrid;

const float PI = 3.141592653589793;
const float HALF_PI = 1.5707963267948966;

// Palette constants
const vec3 C_PLASMA_CORE        = vec3(1.00, 0.30, 0.10);
const vec3 C_MOLTEN_IRON        = vec3(0.91, 0.48, 0.23);
const vec3 C_COOLING_EMBER      = vec3(0.72, 0.36, 0.16);
const vec3 C_DRIED_CLAY         = vec3(0.66, 0.42, 0.29);
const vec3 C_DOPPLER_SAPPHIRE   = vec3(0.18, 0.45, 0.75);
const vec3 C_DOPPLER_RUBY       = vec3(0.75, 0.18, 0.18);
const vec3 C_EVENT_HORIZON_GLOW = vec3(1.00, 0.42, 0.16);
const vec3 C_BURNT_SIENNA       = vec3(0.79, 0.49, 0.35);

float rOuter() { return u_M + sqrt(max(0.0, u_M*u_M - u_a*u_a)); }
float rInner() { return u_M - sqrt(max(0.0, u_M*u_M - u_a*u_a)); }

float rISCO() {
    float a = clamp(u_a, 0.0, 0.998);
    float Z1 = 1.0 + pow(max(1e-5, 1.0 - a*a), 1.0/3.0) * (pow(1.0 + a, 1.0/3.0) + pow(max(1e-5, 1.0 - a), 1.0/3.0));
    float Z2 = sqrt(3.0*a*a + Z1*Z1);
    return u_M * (3.0 + Z2 - sqrt(max(0.0, (3.0 - Z1)*(3.0 + Z1 + 2.0*Z2))));
}

float kerrR(vec3 p, bool isNegative) {
    float rho2 = dot(p, p);
    float a2 = u_a * u_a;
    float D = rho2 - a2;
    float r2 = 0.5 * (D + sqrt(max(0.0, D * D + 4.0 * a2 * p.z * p.z)));
    float r = sqrt(max(0.0, r2));
    return isNegative ? -r : r;
}

vec3 kerrAccel(vec3 p, vec3 v, float r, bool isNegative) {
    float ar = max(0.08, abs(r));
    float rho2 = dot(p, p);
    float a2 = u_a * u_a;

    vec3 Lvec = cross(p, v);
    float L2 = dot(Lvec, Lvec);

    float gFactor = isNegative ? -1.0 : 1.0;
    float aMag = gFactor * (3.0 * u_M * L2 / (pow(ar, 5.0) + 0.04));
    vec3 a_grav = -aMag * p;

    float omega = (2.0 * u_M * u_a * ar) / (pow(ar * ar + a2, 2.0) + 0.08);
    vec3 wVec = vec3(0.0, 0.0, omega * gFactor);
    vec3 a_drag = 2.0 * cross(v, wVec);

    return a_grav + a_drag;
}

vec3 diskColor(float r) {
    float rIn = rISCO();
    float rOut = 15.0 * u_M;
    float t = clamp((r - rIn) / (rOut - rIn + 1e-4), 0.0, 1.0);
    vec3 c;
    if      (t < 0.18) c = mix(C_PLASMA_CORE,   C_MOLTEN_IRON,   t / 0.18);
    else if (t < 0.50) c = mix(C_MOLTEN_IRON,   C_COOLING_EMBER, (t - 0.18) / 0.32);
    else               c = mix(C_COOLING_EMBER, C_DRIED_CLAY,    (t - 0.50) / 0.50);

    float intensity = pow(rIn / max(r, rIn), 0.75) * pow(max(0.0, 1.0 - sqrt(rIn / max(r, rIn))), 0.25) * 2.2 + 0.40;
    return c * intensity;
}

vec3 applyDoppler(vec3 baseCol, float g) {
    vec3 tinted;
    if (g > 1.0) tinted = mix(baseCol, baseCol * C_DOPPLER_SAPPHIRE * 1.8, clamp(g - 1.0, 0.0, 1.0));
    else         tinted = mix(baseCol, baseCol * C_DOPPLER_RUBY     * 1.8, clamp(1.0 - g, 0.0, 1.0));
    return tinted * clamp(pow(g, 3.2), 0.08, 6.0);
}

// Procedural background starfield with galactic ash band
vec3 sampleStars(vec3 dir, int mode) {
    float u = 0.5 + 0.5 * atan(dir.y, dir.x) / PI;
    float v = 0.5 - asin(clamp(dir.z, -1.0, 1.0)) / PI;
    if (mode == 3) {
        u = fract(u + 0.37);
        v = 1.0 - v;
    }

    // Galactic nebula band
    float band = exp(-pow(v - 0.5, 2.0) * 45.0);
    float dust = band * (0.05 + 0.06 * sin(u * 12.0 + v * 8.0));
    vec3 nebCol = vec3(dust * 0.9, dust * 0.75, dust * 0.65);

    // Stars pseudo-random noise
    vec3 p = dir * 180.0;
    vec3 fl = floor(p);
    float n = fract(sin(dot(fl, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
    float star = 0.0;
    if (n > 0.975) {
        float d = length(fract(p) - 0.5);
        star = smoothstep(0.25, 0.0, d) * (0.6 + 0.4 * fract(n * 100.0));
    }

    return (nebCol + vec3(star)) * u_starBrightness;
}

void main() {
    vec2 uv = (gl_FragCoord.xy / u_resolution) * 2.0 - 1.0;
    uv.x *= (u_resolution.x / u_resolution.y);

    float sinTh0 = sin(u_theta);
    float cosTh0 = cos(u_theta);
    vec3 camPos = vec3(
        u_r * sinTh0 * cos(u_phi),
        u_r * sinTh0 * sin(u_phi),
        u_r * cosTh0
    );

    vec3 camRay = normalize(vec3(uv.x, uv.y, -1.0));
    vec3 rayDir = normalize((u_camBasis * vec4(camRay, 0.0)).xyz);

    vec3 pos = camPos;
    vec3 vel = rayDir;
    bool isNegative = (u_r < 0.0);

    vec3 accum = vec3(0.0);
    float transmission = 1.0;
    int steps = clamp(u_maxSteps, 30, 400);

    float ro = rOuter();
    float ri = rInner();

    for (int i = 0; i < 400; ++i) {
        if (i >= steps) break;

        float r = kerrR(pos, isNegative);
        float ar = abs(r);

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
        vec3 a1 = kerrAccel(pos, vel, r, isNegative);
        vec3 nextPos = pos + vel * h + 0.5 * a1 * h * h;
        float nextR = kerrR(nextPos, isNegative);
        vec3 a2 = kerrAccel(nextPos, vel + a1 * h, nextR, isNegative);
        vel = normalize(vel + 0.5 * (a1 + a2) * h);
        pos = nextPos;

        // Ring singularity passage: crossing z=0 inside ring radius a
        if (prevPos.z * pos.z <= 0.0 && (pos.x * pos.x + pos.y * pos.y) < (u_a * u_a)) {
            isNegative = !isNegative;
        }

        // Accretion disk analytic plane crossing (z = 0, r in [rISCO, 15M])
        if (prevPos.z * pos.z <= 0.0 && !isNegative) {
            float tCross = clamp(-prevPos.z / (pos.z - prevPos.z + 1e-7), 0.0, 1.0);
            vec3 pDisk = mix(prevPos, pos, tCross);
            float rDisk = length(pDisk.xy);

            if (rDisk >= rISCO() && rDisk <= 15.0 * u_M) {
                float Om = sqrt(u_M) / (pow(rDisk, 1.5) + u_a * sqrt(u_M));
                vec3 vPlasmaDir = normalize(vec3(-pDisk.y, pDisk.x, 0.0));
                vec3 vPhotDir = -normalize(pos - prevPos);
                float beta = dot(vPhotDir, vPlasmaDir);
                float vOrb = clamp(Om * rDisk, 0.0, 0.85);

                float gamma = 1.0 / sqrt(max(0.01, 1.0 - vOrb * vOrb));
                float alphaGrav = sqrt(max(0.02, 1.0 - 2.0 * u_M * rDisk / (rDisk * rDisk + u_a * u_a)));
                float g = clamp(alphaGrav / (gamma * max(0.08, 1.0 - beta * vOrb)), 0.20, 2.80);

                float ang = atan(pDisk.y, pDisk.x);
                float shearAngle = ang - Om * 6.0 * u_time;
                float turb = 0.84 + 0.10 * sin(24.0 * log(rDisk)) + 0.06 * sin(8.0 * shearAngle + 3.0 * log(rDisk));

                if (u_diskStyle != 2) {
                    vec3 radiated;
                    if (u_diskStyle == 1) {
                        vec3 plasmaCol = mix(vec3(0.15, 0.65, 1.00), vec3(0.58, 0.22, 0.95), clamp((rDisk - rISCO()) / (10.0 * u_M), 0.0, 1.0));
                        radiated = applyDoppler(plasmaCol, g) * turb * u_diskBrightness;
                    } else if (u_diskStyle == 3) {
                        vec3 heatCol;
                        if (g > 1.35)      heatCol = mix(vec3(0.10, 0.45, 1.00), vec3(0.20, 0.90, 1.00), clamp((g - 1.35) / 0.8, 0.0, 1.0));
                        else if (g > 1.08) heatCol = mix(vec3(0.15, 0.92, 0.88), vec3(0.20, 0.92, 0.25), clamp((g - 1.08) / 0.27, 0.0, 1.0));
                        else if (g > 0.82) heatCol = mix(vec3(0.20, 0.92, 0.25), vec3(1.00, 0.72, 0.15), clamp((1.08 - g) / 0.26, 0.0, 1.0));
                        else              heatCol = mix(vec3(1.00, 0.72, 0.15), vec3(0.85, 0.10, 0.10), clamp((0.82 - g) / 0.50, 0.0, 1.0));
                        radiated = heatCol * turb * u_diskBrightness;
                    } else {
                        radiated = applyDoppler(diskColor(rDisk), g) * turb * u_diskBrightness;
                    }

                    float tau = 2.0 * exp(-0.15 * (rDisk - rISCO()));
                    float alpha = clamp(1.0 - exp(-tau), 0.50, 0.88);
                    accum += radiated * alpha * transmission;
                    transmission *= (1.0 - alpha);
                    if (transmission < 0.03) break;
                }
            }
        }

        // Escaped to infinity (normal universe)
        if (length(pos) > 24.0 * u_M && !isNegative && dot(pos, vel) > 0.0) {
            vec3 dir = normalize(vel);
            accum += sampleStars(dir, u_mode) * transmission;
            transmission = 0.0;
            break;
        }

        // Escaped to negative universe celestial sphere
        if (length(pos) > 20.0 * u_M && isNegative && dot(pos, vel) > 0.0) {
            vec3 dir = normalize(vel);
            accum += sampleStars(dir, 3) * transmission;
            transmission = 0.0;
            break;
        }

        // Negative Universe White Hole Expulsion Halo & Jets
        float rho = length(pos.xy);
        if (isNegative && rho < 0.60 * u_a && abs(pos.z) < 6.0) {
            float jetCore = exp(-14.0 * rho / u_a);
            float jetZFalloff = exp(-0.6 * abs(pos.z)) * smoothstep(6.0, 2.5, abs(pos.z));
            vec3 jetCol = mix(vec3(0.20, 0.85, 1.00), vec3(1.0, 1.0, 1.0), jetCore);
            accum += jetCol * (jetCore * 2.4 * jetZFalloff) * 0.10 * transmission;
        }

        if (abs(pos.z) < 0.50 && rho < u_a * 1.35) {
            float uThroat = clamp(rho / max(0.01, u_a), 0.0, 1.30);
            float throatCore = exp(-8.0 * abs(pos.z));
            vec3 coreCol = mix(vec3(1.0, 1.0, 1.0), vec3(0.20, 0.90, 1.00), uThroat);
            accum += coreCol * (throatCore * 1.6) * 0.12 * transmission;
        }
    }

    // Cauchy infinite blueshift flash
    if (u_whiteout > 0.001) {
        accum = mix(accum, vec3(1.0, 0.98, 0.95), clamp(u_whiteout * 3.5, 0.0, 1.0));
    }

    // Bloom & Tonemapping
    vec3 col = accum + accum * u_bloom;
    col = col / (col + vec3(1.0)); // Reinhard tonemap
    col = pow(col, vec3(1.0 / 2.2)); // Gamma

    fragColor = vec4(col, 1.0);
}`;

// --- WebGL Engine Initialization ---
class KerrEngine {
  constructor() {
    this.canvas = document.getElementById('glcanvas');
    this.gl = this.canvas.getContext('webgl2', {
      powerPreference: 'high-performance',
      antialias: false,
      alpha: false,
      preserveDrawingBuffer: true
    });

    if (!this.gl) {
      alert('WebGL 2 is required to run the Kerr Raymarcher. Please use a modern browser.');
      return;
    }

    this.initShaders();
    this.initBuffers();
    this.initUniforms();
    this.initAudio();
    this.initControls();
    this.updateResolution();

    this.lastTime = performance.now();
    this.animate = this.animate.bind(this);
    requestAnimationFrame(this.animate);
  }

  initShaders() {
    const gl = this.gl;
    const vs = this.compileShader(gl.VERTEX_SHADER, vsSource);
    const fs = this.compileShader(gl.FRAGMENT_SHADER, fsSource);
    this.prog = gl.createProgram();
    gl.attachShader(this.prog, vs);
    gl.attachShader(this.prog, fs);
    gl.linkProgram(this.prog);

    if (!gl.getProgramParameter(this.prog, gl.LINK_STATUS)) {
      console.error('Program link error:', gl.getProgramInfoLog(this.prog));
    }
  }

  compileShader(type, src) {
    const gl = this.gl;
    const shader = gl.createShader(type);
    gl.shaderSource(shader, src);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
      console.error('Shader compile error:', gl.getShaderInfoLog(shader));
    }
    return shader;
  }

  initBuffers() {
    const gl = this.gl;
    this.vao = gl.createVertexArray();
    gl.bindVertexArray(this.vao);

    const quad = new Float32Array([-1, -1,  1, -1, -1,  1, -1,  1,  1, -1,  1,  1]);
    const vbo = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
    gl.bufferData(gl.ARRAY_BUFFER, quad, gl.STATIC_DRAW);

    const aPos = gl.getAttribLocation(this.prog, 'a_position');
    gl.enableVertexAttribArray(aPos);
    gl.vertexAttribPointer(aPos, 2, gl.FLOAT, false, 0, 0);
  }

  initUniforms() {
    const gl = this.gl;
    gl.useProgram(this.prog);
    this.u = {
      resolution: gl.getUniformLocation(this.prog, 'u_resolution'),
      time: gl.getUniformLocation(this.prog, 'u_time'),
      a: gl.getUniformLocation(this.prog, 'u_a'),
      M: gl.getUniformLocation(this.prog, 'u_M'),
      r: gl.getUniformLocation(this.prog, 'u_r'),
      theta: gl.getUniformLocation(this.prog, 'u_theta'),
      phi: gl.getUniformLocation(this.prog, 'u_phi'),
      dr: gl.getUniformLocation(this.prog, 'u_dr'),
      camBasis: gl.getUniformLocation(this.prog, 'u_camBasis'),
      maxSteps: gl.getUniformLocation(this.prog, 'u_maxSteps'),
      diskStyle: gl.getUniformLocation(this.prog, 'u_diskStyle'),
      mode: gl.getUniformLocation(this.prog, 'u_mode'),
      diskBrightness: gl.getUniformLocation(this.prog, 'u_diskBrightness'),
      starBrightness: gl.getUniformLocation(this.prog, 'u_starBrightness'),
      whiteout: gl.getUniformLocation(this.prog, 'u_whiteout'),
      bloom: gl.getUniformLocation(this.prog, 'u_bloom'),
      showGrid: gl.getUniformLocation(this.prog, 'u_showGrid')
    };
  }

  updateResolution() {
    const presets = {
      '360p': [640, 360],
      '720p': [1280, 720],
      '1080p': [1920, 1080],
      '1440p': [2560, 1440],
      '4K': [3840, 2160]
    };

    let w, h;
    if (state.resPreset === 'match') {
      w = window.innerWidth * window.devicePixelRatio;
      h = window.innerHeight * window.devicePixelRatio;
    } else {
      [w, h] = presets[state.resPreset] || [1920, 1080];
    }

    w = Math.round(w * state.renderScale);
    h = Math.round(h * state.renderScale);

    this.canvas.width = w;
    this.canvas.height = h;
    this.gl.viewport(0, 0, w, h);

    document.getElementById('stat-res').textContent = state.resPreset.toUpperCase();
    document.getElementById('telem-render-res').textContent = `${w} x ${h}`;
    document.getElementById('telem-window-res').textContent = `${window.innerWidth} x ${window.innerHeight}`;
    document.getElementById('telem-mp').textContent = `${(w * h / 1000000).toFixed(2)} MP`;
  }

  getCamBasis() {
    const curR = Math.abs(state.r);
    const sTh = Math.sin(state.theta);
    const cTh = Math.cos(state.theta);
    const pos = [
      curR * sTh * Math.cos(state.phi),
      curR * sTh * Math.sin(state.phi),
      curR * cTh
    ];

    const len = Math.hypot(pos[0], pos[1], pos[2]);
    const centerFwd = len > 1e-4 ? [-pos[0]/len, -pos[1]/len, -pos[2]/len] : [0, 0, -1];
    
    // centerRight = cross(centerFwd, (0,0,1))
    let centerRight = [centerFwd[1], -centerFwd[0], 0];
    let rLen = Math.hypot(centerRight[0], centerRight[1]);
    if (rLen < 1e-4) centerRight = [1, 0, 0];
    else centerRight = [centerRight[0]/rLen, centerRight[1]/rLen, 0];

    // centerUp = cross(centerRight, centerFwd)
    const centerUp = [
      centerRight[1] * centerFwd[2] - centerRight[2] * centerFwd[1],
      centerRight[2] * centerFwd[0] - centerRight[0] * centerFwd[2],
      centerRight[0] * centerFwd[1] - centerRight[1] * centerFwd[0]
    ];

    const cp = Math.cos(state.pitch);
    const lookX = Math.sin(state.yaw) * cp;
    const lookY = Math.sin(state.pitch);
    const lookZ = -Math.cos(state.yaw) * cp;

    // worldFwd = normalize(lookX * centerRight + lookY * centerUp - lookZ * centerFwd)
    const wfX = lookX * centerRight[0] + lookY * centerUp[0] - lookZ * centerFwd[0];
    const wfY = lookX * centerRight[1] + lookY * centerUp[1] - lookZ * centerFwd[1];
    const wfZ = lookX * centerRight[2] + lookY * centerUp[2] - lookZ * centerFwd[2];
    const wfLen = Math.hypot(wfX, wfY, wfZ);
    const worldFwd = [wfX/wfLen, wfY/wfLen, wfZ/wfLen];

    // worldRight = normalize(cross(worldFwd, (0,0,1)))
    let wrX = worldFwd[1], wrY = -worldFwd[0];
    let wrLen = Math.hypot(wrX, wrY);
    let worldRight = wrLen < 1e-4 ? centerRight : [wrX/wrLen, wrY/wrLen, 0];

    // worldUp = cross(worldRight, worldFwd)
    let worldUp = [
      worldRight[1] * worldFwd[2] - worldRight[2] * worldFwd[1],
      worldRight[2] * worldFwd[0] - worldRight[0] * worldFwd[2],
      worldRight[0] * worldFwd[1] - worldRight[1] * worldFwd[0]
    ];

    // 4x4 column-major matrix
    return [
      worldRight[0], worldRight[1], worldRight[2], 0,
      worldUp[0],    worldUp[1],    worldUp[2],    0,
      -worldFwd[0],  -worldFwd[1],  -worldFwd[2],  0,
      0, 0, 0, 1
    ];
  }

  stepSimulation(dt) {
    if (state.isTouring) {
      state.tourTime += dt;
      if (state.tourTime >= 30.0) {
        state.isTouring = false;
        state.tourTime = 0.0;
        state.r = -2.5;
        state.theta = 1.45;
        state.mode = 3;
      } else {
        const u = state.tourTime / 30.0;
        state.r = 14.0 * (1.0 - u) + (-2.5) * u;
        state.phi += 0.25 * dt;
        if (state.tourTime >= 19.2 && state.tourTime <= 19.6 && state.whiteoutTimer <= 0.05) {
          state.whiteoutTimer = 0.40;
        }
      }
    } else {
      // Natural Kerr geodesic infall
      const rOut = state.M + Math.sqrt(Math.max(0.0, state.M*state.M - state.a*state.a));
      if (state.r > 0.0 && state.r <= rOut) {
        state.dr = Math.min(state.dr, -0.15); // Unidirectional event horizon
      }
      state.r += state.dr * dt * state.simSpeed;
      state.phi += 0.15 * dt * state.simSpeed;
    }

    // Whiteout timer decay
    if (state.whiteoutTimer > 0.0) {
      state.whiteoutTimer = Math.max(0.0, state.whiteoutTimer - dt * 1.5);
    }

    // Spacetime domain classification
    const rOut = state.M + Math.sqrt(Math.max(0.0, state.M*state.M - state.a*state.a));
    const rIn = state.M - Math.sqrt(Math.max(0.0, state.M*state.M - state.a*state.a));
    const prevMode = state.mode;

    if (state.r < 0.0) state.mode = 3; // NEGATIVE
    else if (state.r <= rIn) state.mode = 2; // INSIDE
    else if (state.r <= rOut) state.mode = 1; // BETWEEN
    else state.mode = 0; // NORMAL

    // Whiteout trigger on Cauchy horizon crossing
    if (prevMode === 1 && state.mode === 2) state.whiteoutTimer = 0.35;
    if (prevMode === 2 && state.mode === 3) state.whiteoutTimer = 0.35;
  }

  animate(now) {
    const dt = Math.min(0.05, Math.max(0.001, (now - this.lastTime) / 1000.0));
    this.lastTime = now;

    // Smooth frame latency tracking
    const frameMs = dt * 1000.0;
    state.frameMsSmoothed = state.frameMsSmoothed * 0.92 + frameMs * 0.08;

    // GPU lag overload detection (<15 FPS)
    if (state.frameMsSmoothed > 66.0 && state.autoGpuRecovery) {
      state.lowFpsDuration += dt;
      if (state.lowFpsDuration > 2.5) {
        document.getElementById('gpu-lag-banner').classList.remove('hidden');
      }
    } else {
      state.lowFpsDuration = Math.max(0.0, state.lowFpsDuration - dt * 0.5);
    }

    this.stepSimulation(dt);
    this.render(now / 1000.0);
    this.updateHUD();

    requestAnimationFrame(this.animate);
  }

  render(timeSec) {
    const gl = this.gl;
    gl.useProgram(this.prog);

    gl.uniform2f(this.u.resolution, this.canvas.width, this.canvas.height);
    gl.uniform1f(this.u.time, timeSec);
    gl.uniform1f(this.u.a, state.a);
    gl.uniform1f(this.u.M, state.M);
    gl.uniform1f(this.u.r, state.r);
    gl.uniform1f(this.u.theta, state.theta);
    gl.uniform1f(this.u.phi, state.phi);
    gl.uniform1f(this.u.dr, state.dr);
    gl.uniformMatrix4fv(this.u.camBasis, false, this.getCamBasis());
    gl.uniform1i(this.u.maxSteps, state.maxSteps);
    gl.uniform1i(this.u.diskStyle, state.diskStyle);
    gl.uniform1i(this.u.mode, state.mode);
    gl.uniform1f(this.u.diskBrightness, state.diskBrightness);
    gl.uniform1f(this.u.starBrightness, state.starBrightness);
    gl.uniform1f(this.u.whiteout, state.whiteoutTimer);
    gl.uniform1f(this.u.bloom, state.bloomIntensity);
    gl.uniform1i(this.u.showGrid, state.showGrid ? 1 : 0);

    gl.bindVertexArray(this.vao);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
  }

  updateHUD() {
    document.getElementById('stat-r').textContent = `${state.r.toFixed(2)} M`;
    document.getElementById('stat-spin').textContent = state.a.toFixed(2);
    document.getElementById('stat-fps').textContent = Math.round(1000.0 / Math.max(1.0, state.frameMsSmoothed));
    document.getElementById('telem-frametime').textContent = `${state.frameMsSmoothed.toFixed(1)} ms`;

    document.getElementById('telem-detail-r').textContent = `${state.r.toFixed(2)} M`;
    document.getElementById('telem-detail-dr').textContent = state.dr.toFixed(3);

    // Frame-dragging velocity
    const r2pa2 = state.r * state.r + state.a * state.a;
    const omega = (2.0 * state.M * state.a * Math.abs(state.r)) / (r2pa2 * r2pa2 + 0.1);
    const vDrag = Math.min(0.99, omega * Math.sqrt(r2pa2) * Math.sin(state.theta));
    document.getElementById('telem-detail-drag').textContent = `${vDrag.toFixed(2)} c`;

    const tidal = (2.0 * state.M / Math.pow(Math.abs(state.r) + 0.1, 3.0)).toFixed(4);
    document.getElementById('telem-detail-tidal').textContent = `${tidal} g/m`;

    // Status badge update
    const badge = document.getElementById('badge-mode');
    const titles = ['APPROACH (NORMAL)', 'BETWEEN HORIZONS', 'INSIDE HORIZON', 'NEGATIVE UNIVERSE'];
    const classes = ['mode-normal', 'mode-between', 'mode-inside', 'mode-negative'];
    badge.textContent = titles[state.mode];
    badge.className = `status-badge ${classes[state.mode]}`;

    // Zone banner
    const zTitle = document.getElementById('zone-title');
    const zLore = document.getElementById('zone-lore');
    if (state.mode === 3) {
      zTitle.textContent = 'NEGATIVE UNIVERSE (r < 0)';
      zLore.textContent = 'Antigravity domain. Gravitational mass inverts; the central singularity pushes you away!';
    } else if (state.mode === 2) {
      zTitle.textContent = 'INNER HORIZON (CAUCHY BARRIER)';
      zLore.textContent = 'Infinite blueshift optical boundary. Starlight from the external universe piles up into a flash.';
    } else if (state.mode === 1) {
      zTitle.textContent = 'INSIDE EVENT HORIZON (NO RETURN)';
      zLore.textContent = 'Space and time swap roles. All future-directed paths lead inexorably inward.';
    } else {
      zTitle.textContent = 'SAFE HIGH ORBIT';
      zLore.textContent = 'Stable Keplerian orbit outside the gravitational horizon. Spacetime curvature is mild.';
    }
  }

  jumpToPhase(p) {
    state.isTouring = false;
    state.isAutoOrbit = false;
    switch (p) {
      case 1: state.r = 14.0; state.theta = 1.35; state.dr = -0.15; break;
      case 2: state.r = 2.0;  state.theta = 1.42; state.dr = -0.15; break;
      case 3: state.r = 1.0;  state.theta = 1.48; state.dr = -0.15; break;
      case 4: state.r = 0.68; state.theta = 1.50; state.dr = -0.15; state.whiteoutTimer = 0.5; break;
      case 5: state.r = 0.05; state.theta = 1.57; state.dr = -0.05; break;
      case 6: state.r = -2.5; state.theta = 1.45; state.dr = -0.05; break;
    }
  }

  initControls() {
    window.addEventListener('resize', () => this.updateResolution());

    // Mouse look with pointer lock
    this.canvas.addEventListener('click', () => {
      if (document.pointerLockElement !== this.canvas) {
        this.canvas.requestPointerLock();
      }
    });

    document.addEventListener('mousemove', (e) => {
      if (document.pointerLockElement === this.canvas) {
        state.yaw += e.movementX * 0.0025;
        state.pitch += -e.movementY * 0.0025;
        state.pitch = Math.max(-1.55, Math.min(1.55, state.pitch));
      }
    });

    // Keyboard controls
    const keys = {};
    window.addEventListener('keydown', (e) => {
      keys[e.key.toLowerCase()] = true;
      if (e.key === 'Tab') {
        e.preventDefault();
        document.getElementById('sidebar').classList.toggle('active');
      }
      if (e.key === 'g' || e.key === 'G') state.isTouring = !state.isTouring;
      if (e.key === 'r' || e.key === 'R') this.jumpToPhase(1);
      if (e.key >= '1' && e.key <= '6') this.jumpToPhase(parseInt(e.key));
    });
    window.addEventListener('keyup', (e) => {
      keys[e.key.toLowerCase()] = false;
    });

    // WASD continuous poll
    setInterval(() => {
      if (document.pointerLockElement !== this.canvas) return;
      const fwd = (keys['w'] ? 1 : 0) - (keys['s'] ? 1 : 0);
      const side = (keys['d'] ? 1 : 0) - (keys['a'] ? 1 : 0);
      const up = (keys[' '] || keys['e'] ? 1 : 0) - (keys['control'] || keys['q'] ? 1 : 0);
      const turbo = keys['shift'] ? 3.0 : 1.0;

      if (fwd !== 0 || side !== 0 || up !== 0) {
        state.isTouring = false;
        state.r += (-fwd * 0.25 + up * 0.15) * turbo * 0.05;
        state.phi += (side * 0.02) * turbo;
      }
    }, 16);

    // UI Drawer Toggle
    document.getElementById('btn-menu').addEventListener('click', () => {
      document.getElementById('sidebar').classList.toggle('active');
    });
    document.getElementById('btn-close-sidebar').addEventListener('click', () => {
      document.getElementById('sidebar').classList.remove('active');
    });

    // Tab Navigation
    document.querySelectorAll('.tab-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
        btn.classList.add('active');
        document.getElementById(btn.dataset.tab).classList.add('active');
      });
    });

    // Resolution change
    document.getElementById('select-res').addEventListener('change', (e) => {
      state.resPreset = e.target.value;
      this.updateResolution();
    });

    document.getElementById('slider-scale').addEventListener('input', (e) => {
      state.renderScale = parseFloat(e.target.value);
      document.getElementById('val-scale').textContent = `${state.renderScale.toFixed(2)}x`;
      this.updateResolution();
    });

    document.getElementById('slider-steps').addEventListener('input', (e) => {
      state.maxSteps = parseInt(e.target.value);
      document.getElementById('val-steps').textContent = state.maxSteps;
    });

    document.getElementById('btn-steps-fast').addEventListener('click', () => {
      state.maxSteps = 100;
      document.getElementById('slider-steps').value = 100;
      document.getElementById('val-steps').textContent = 100;
    });
    document.getElementById('btn-steps-med').addEventListener('click', () => {
      state.maxSteps = 180;
      document.getElementById('slider-steps').value = 180;
      document.getElementById('val-steps').textContent = 180;
    });
    document.getElementById('btn-steps-max').addEventListener('click', () => {
      state.maxSteps = 300;
      document.getElementById('slider-steps').value = 300;
      document.getElementById('val-steps').textContent = 300;
    });

    document.getElementById('check-auto-gpu').addEventListener('change', (e) => {
      state.autoGpuRecovery = e.target.checked;
    });

    // Phase jump buttons
    document.querySelectorAll('[data-phase]').forEach(btn => {
      btn.addEventListener('click', () => this.jumpToPhase(parseInt(btn.dataset.phase)));
    });

    document.getElementById('btn-tour').addEventListener('click', () => {
      state.isTouring = !state.isTouring;
      state.tourTime = 0.0;
    });
    document.getElementById('btn-tour-drawer').addEventListener('click', () => {
      state.isTouring = true;
      state.tourTime = 0.0;
    });
    document.getElementById('btn-reset').addEventListener('click', () => this.jumpToPhase(1));

    // Lag recovery buttons
    document.getElementById('btn-lag-1080').addEventListener('click', () => {
      state.resPreset = '1080p';
      state.renderScale = 1.0;
      document.getElementById('select-res').value = '1080p';
      document.getElementById('slider-scale').value = 1.0;
      this.updateResolution();
      document.getElementById('gpu-lag-banner').classList.add('hidden');
    });
    document.getElementById('btn-lag-720').addEventListener('click', () => {
      state.resPreset = '720p';
      state.renderScale = 1.0;
      document.getElementById('select-res').value = '720p';
      document.getElementById('slider-scale').value = 1.0;
      this.updateResolution();
      document.getElementById('gpu-lag-banner').classList.add('hidden');
    });
    document.getElementById('btn-lag-dismiss').addEventListener('click', () => {
      document.getElementById('gpu-lag-banner').classList.add('hidden');
    });

    // Visuals settings
    document.getElementById('select-disk').addEventListener('change', (e) => {
      state.diskStyle = parseInt(e.target.value);
    });
    document.getElementById('slider-disk-bright').addEventListener('input', (e) => {
      state.diskBrightness = parseFloat(e.target.value);
      document.getElementById('val-disk-bright').textContent = state.diskBrightness.toFixed(2);
    });
    document.getElementById('slider-star-bright').addEventListener('input', (e) => {
      state.starBrightness = parseFloat(e.target.value);
      document.getElementById('val-star-bright').textContent = state.starBrightness.toFixed(2);
    });
    document.getElementById('slider-bloom').addEventListener('input', (e) => {
      state.bloomIntensity = parseFloat(e.target.value);
      document.getElementById('val-bloom').textContent = state.bloomIntensity.toFixed(2);
    });
    document.getElementById('check-reticle').addEventListener('change', (e) => {
      document.getElementById('reticle').style.display = e.target.checked ? 'block' : 'none';
    });
    document.getElementById('check-grid').addEventListener('change', (e) => {
      state.showGrid = e.target.checked;
    });

    // Fullscreen toggle
    document.getElementById('btn-fullscreen').addEventListener('click', () => {
      if (!document.fullscreenElement) document.documentElement.requestFullscreen();
      else document.exitFullscreen();
    });

    // Screenshot
    document.getElementById('btn-screenshot').addEventListener('click', () => {
      const a = document.createElement('a');
      a.download = `kerr-blackhole-${Date.now()}.png`;
      a.href = this.canvas.toDataURL('image/png');
      a.click();
    });
  }

  initAudio() {
    const btn = document.getElementById('btn-audio');
    btn.addEventListener('click', () => {
      if (!this.audioCtx) {
        this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        this.osc1 = this.audioCtx.createOscillator();
        this.osc2 = this.audioCtx.createOscillator();
        this.gain = this.audioCtx.createGain();

        this.osc1.type = 'sine';
        this.osc1.frequency.setValueAtTime(48, this.audioCtx.currentTime);
        this.osc2.type = 'triangle';
        this.osc2.frequency.setValueAtTime(96, this.audioCtx.currentTime);

        this.gain.gain.setValueAtTime(0.08, this.audioCtx.currentTime);
        this.osc1.connect(this.gain);
        this.osc2.connect(this.gain);
        this.gain.connect(this.audioCtx.destination);

        this.osc1.start();
        this.osc2.start();
        state.audioActive = true;
        btn.style.color = '#ff4d1a';
      } else {
        if (this.audioCtx.state === 'suspended') {
          this.audioCtx.resume();
          btn.style.color = '#ff4d1a';
        } else {
          this.audioCtx.suspend();
          btn.style.color = '';
        }
      }
    });
  }
}

// Bootstrap
window.addEventListener('DOMContentLoaded', () => {
  new KerrEngine();
});
