#version 460 core
// Warm Ash (older) -> Lava Core (newest), per spec §10.
in  float v_t;
out vec4 fragColor;

void main() {
    vec3 warmAsh  = vec3(196.0, 181.0, 165.0) / 255.0;
    vec3 lavaCore = vec3(255.0,  77.0,  26.0) / 255.0;
    vec3 col = mix(warmAsh, lavaCore, v_t);
    fragColor = vec4(col, 0.25 + 0.75 * v_t);
}
