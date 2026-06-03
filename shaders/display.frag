#version 460 core
// Tone-map + cheap bloom + Cauchy-horizon whiteout overlay.

in  vec2 v_uv;
out vec4 fragColor;

uniform sampler2D u_image;
uniform float u_bloom;     // 0..1
uniform float u_whiteout;  // seconds remaining of flash
uniform vec2  u_res;

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 col = texture(u_image, v_uv).rgb;

    // Cheap 9-tap bloom on bright pixels.
    if (u_bloom > 0.001) {
        vec2 px = 1.0 / u_res;
        vec3 sum = vec3(0.0);
        float wsum = 0.0;
        for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            vec3 s = texture(u_image, v_uv + vec2(dx, dy) * px * 2.0).rgb;
            float b = max(max(s.r, s.g), s.b);
            float w = exp(-float(dx*dx + dy*dy) * 0.3) * smoothstep(0.6, 1.4, b);
            sum  += s * w;
            wsum += w;
        }
        if (wsum > 0.0) col += (sum / wsum) * u_bloom;
    }

    col = aces(col);

    // Whiteout flash from Cauchy horizon crossing.
    float flash = clamp(u_whiteout / 0.2, 0.0, 1.0);
    col = mix(col, vec3(1.0), flash);

    fragColor = vec4(col, 1.0);
}
