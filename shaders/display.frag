#version 460 core
// ============================================================================
// Tone-map + High-Performance Bloom + Cauchy-Horizon Whiteout
// Optimized with constant Gaussian spatial weights (zero runtime exp calls)
// ============================================================================

in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_image;
uniform float u_bloom;     // 0..1
uniform float u_whiteout;  // seconds remaining of flash
uniform vec2  u_res;

// ACES filmic tonemapping
vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Precomputed 1D Gaussian weights for sigma ~ 1.3 (exp(-d^2 * 0.3))
// Eliminates 25 exp() calls per fragment across the entire screen!
const float kW[5] = float[5](0.301194, 0.740818, 1.0, 0.740818, 0.301194);

void main() {
    vec3 col = texture(u_image, v_uv).rgb;

    // Fast constant-weight Gaussian bloom on bright HDR caustics
    if (u_bloom > 0.001) {
        vec2 px = 2.0 / u_res;
        vec3 sum = vec3(0.0);
        float wsum = 0.0;

        for (int dy = -2; dy <= 2; ++dy) {
            float wy = kW[dy + 2];
            for (int dx = -2; dx <= 2; ++dx) {
                float wx = kW[dx + 2];
                vec3 s = texture(u_image, v_uv + vec2(float(dx), float(dy)) * px).rgb;
                float b = max(max(s.r, s.g), s.b);
                if (b > 0.55) {
                    float w = (wx * wy) * smoothstep(0.55, 1.35, b);
                    sum  += s * w;
                    wsum += w;
                }
            }
        }
        if (wsum > 0.0) {
            col += (sum / wsum) * u_bloom;
        }
    }

    col = aces(col);

    // Whiteout flash from Cauchy horizon crossing with natural optical decay
    float flash = clamp(u_whiteout / 0.35, 0.0, 1.0);
    if (flash > 0.001) {
        float flashCurve = smoothstep(0.0, 1.0, flash);
        vec3 flashCol = mix(vec3(0.78, 0.92, 1.00), vec3(1.0), flash);
        col = mix(col, flashCol, flashCurve);
    }

    fragColor = vec4(col, 1.0);
}
